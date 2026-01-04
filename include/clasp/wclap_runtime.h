#pragma once

// WCLAP Runtime - Wasmtime-based implementation for wclap::Instance
//
// This provides the bridge between wclap-cpp's Instance interface and Wasmtime.
// It implements all the required methods for loading and running WCLAP modules.

// Include cstddef before wclap headers (they use size_t without including it)
#include <cstddef>

#include <wclap/instance.hpp>
#include <wclap/wclap.hpp>

#include <wasmtime.h>

#include <atomic>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace clasp {

// Forward declarations
class WclapRuntime;

// Implementation class for wclap::Instance<WasmtimeInstance>
// This provides all the Wasmtime-specific functionality
class WasmtimeInstance {
public:
  WasmtimeInstance(wclap::Instance<WasmtimeInstance> *wrapper,
                   WclapRuntime &runtime, const std::string &wasmPath);
  ~WasmtimeInstance();

  // Error handling
  std::optional<std::string> error() const { return error_; }

  // Virtual filesystem path for this plugin
  const char *path() const { return path_.c_str(); }

  // Whether this is a 64-bit WASM module (wasm64)
  bool is64() const { return is64_; }

  // Initialize the module, return entry point pointer
  uint32_t init32();
  uint64_t init64();

  // Thread management
  int32_t nextThreadId();
  void runThread(int32_t threadId, uint64_t threadContext);

  // Memory allocation in WASM
  wclap32::Pointer<void> malloc32(uint32_t size);
  wclap64::Pointer<void> malloc64(uint64_t size);

  // Read/write arrays from WASM memory
  template <class V>
  bool getArray(wclap32::Pointer<V> ptr, std::remove_cv_t<V> *result,
                size_t count) {
    return getArrayImpl(ptr.wasmPointer, result, sizeof(V) * count);
  }

  template <class V>
  bool setArray(wclap32::Pointer<V> ptr, const V *value, size_t count) {
    return setArrayImpl(ptr.wasmPointer, value, sizeof(V) * count);
  }

  template <class V>
  bool getArray(wclap64::Pointer<V> ptr, std::remove_cv_t<V> *result,
                size_t count) {
    return getArrayImpl(ptr.wasmPointer, result, sizeof(V) * count);
  }

  template <class V>
  bool setArray(wclap64::Pointer<V> ptr, const V *value, size_t count) {
    return setArrayImpl(ptr.wasmPointer, value, sizeof(V) * count);
  }

  // Call WASM functions - 32-bit
  template <class Return, class... Args, class... CArgs>
  Return call(wclap32::Function<Return, Args...> fnPtr, CArgs... args) {
    return callImpl<Return>(fnPtr.wasmPointer, args...);
  }

  template <class Return, class... Args, class... CArgs>
  Return callAt(wclap32::Pointer<wclap32::Function<Return, Args...>> fnPtrPtr,
                CArgs... args) {
    // Read the function pointer from memory, then call it
    wclap32::Function<Return, Args...> fn;
    getArrayImpl(fnPtrPtr.wasmPointer, &fn.wasmPointer, sizeof(fn.wasmPointer));
    return callImpl<Return>(fn.wasmPointer, args...);
  }

  // Call WASM functions - 64-bit
  template <class Return, class... Args, class... CArgs>
  Return call(wclap64::Function<Return, Args...> fnPtr, CArgs... args) {
    return callImpl<Return>(fnPtr.wasmPointer, args...);
  }

  template <class Return, class... Args, class... CArgs>
  Return callAt(wclap64::Pointer<wclap64::Function<Return, Args...>> fnPtrPtr,
                CArgs... args) {
    wclap64::Function<Return, Args...> fn;
    getArrayImpl(fnPtrPtr.wasmPointer, &fn.wasmPointer, sizeof(fn.wasmPointer));
    return callImpl<Return>(fn.wasmPointer, args...);
  }

  // Register host callbacks - these allow WASM to call back into native code
  template <class Return, class... Args>
  wclap32::Function<Return, Args...> registerHost32(void *context,
                                                    Return (*fn)(void *,
                                                                 Args...)) {
    return {registerHostCallback(context, reinterpret_cast<void *>(fn),
                                 sizeof...(Args))};
  }

  template <class Return, class... Args>
  wclap64::Function<Return, Args...> registerHost64(void *context,
                                                    Return (*fn)(void *,
                                                                 Args...)) {
    return {registerHostCallback(context, reinterpret_cast<void *>(fn),
                                 sizeof...(Args))};
  }

private:
  wclap::Instance<WasmtimeInstance> *wrapper_;
  WclapRuntime &runtime_;

  std::string path_;
  std::optional<std::string> error_;
  bool is64_ = false;

  // Wasmtime handles
  wasmtime_store_t *store_ = nullptr;
  wasmtime_context_t *context_ = nullptr;
  wasmtime_instance_t instance_{};
  wasmtime_memory_t memory_{};
  wasmtime_module_t *module_ = nullptr;

  // Function table for indirect calls
  wasmtime_table_t funcTable_{};
  bool hasFuncTable_ = false;

  // Thread ID counter
  std::atomic<int32_t> nextThreadId_{1};

  // Host callback registry
  struct HostCallback {
    void *context;
    void *fn;
    size_t argCount;
  };
  std::vector<HostCallback> hostCallbacks_;
  std::mutex callbackMutex_;

  // Memory access helpers
  uint8_t *memoryBase();
  size_t memorySize();
  bool getArrayImpl(uint64_t ptr, void *result, size_t bytes);
  bool setArrayImpl(uint64_t ptr, const void *value, size_t bytes);

  // Function calling helpers
  template <class Return, class... Args>
  Return callImpl(uint64_t funcIndex, Args... args);

  uint32_t registerHostCallback(void *context, void *fn, size_t argCount);

  // Find and cache required exports
  bool resolveExports();
  bool findExport(const char *name, wasmtime_extern_t *out);

  // Entry point export
  wasmtime_func_t entryFunc_{};
  uint32_t entryPtr32_ = 0;
  uint64_t entryPtr64_ = 0;
};

// Global WCLAP runtime - manages Wasmtime engine and module loading
class WclapRuntime {
public:
  static WclapRuntime &instance();

  // Load a WCLAP module and create an instance
  std::unique_ptr<wclap::Instance<WasmtimeInstance>>
  loadModule(const std::string &wasmPath);

  // Access to the Wasmtime engine
  wasm_engine_t *engine() { return engine_; }

  // AOT cache management
  void setCacheDir(const std::string &dir) { cacheDir_ = dir; }
  const std::string &cacheDir() const { return cacheDir_; }
  bool aotCacheEnabled() const { return aotCacheEnabled_; }
  void setAotCacheEnabled(bool enabled) { aotCacheEnabled_ = enabled; }

  // Compile or load from cache
  wasmtime_module_t *compileOrLoadCached(const std::string &wasmPath,
                                         const std::vector<uint8_t> &wasmBytes);

  const std::string &lastError() const { return lastError_; }

private:
  WclapRuntime();
  ~WclapRuntime();

  wasm_engine_t *engine_ = nullptr;
  std::string cacheDir_;
  std::string lastError_;
  bool aotCacheEnabled_ = true;

  std::string getCachePath(const std::string &wasmPath);
  bool isCacheValid(const std::string &wasmPath, const std::string &cachePath);
  bool saveToCache(wasmtime_module_t *module, const std::string &cachePath);
  wasmtime_module_t *loadFromCache(const std::string &cachePath);
};

} // namespace clasp
