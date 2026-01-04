#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <wasmtime.h>

namespace clasp {

// Forward declarations
class WasmInstance;

// Singleton Wasmtime engine wrapper
class Runtime {
public:
  static Runtime &instance();

  // Delete copy/move
  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;
  Runtime(Runtime &&) = delete;
  Runtime &operator=(Runtime &&) = delete;

  // Load a WASM module from file (with AOT caching)
  // If a cached .cwasm file exists and is newer than the .wasm, use it
  // Otherwise, compile and cache
  std::unique_ptr<WasmInstance> loadModule(const std::string &wasmPath);

  // Get the raw engine (for advanced usage)
  wasm_engine_t *engine() const { return engine_; }
  wasmtime_store_t *store() const { return store_; }
  wasmtime_context_t *context() const { return context_; }

  // Error handling
  std::string lastError() const { return lastError_; }

  // AOT cache management
  void setCacheDir(const std::string &dir) { cacheDir_ = dir; }
  std::string cacheDir() const { return cacheDir_; }
  void enableAotCache(bool enable) { aotCacheEnabled_ = enable; }

private:
  Runtime();
  ~Runtime();

  // Compile or load from AOT cache
  wasmtime_module_t *compileOrLoadCached(const std::string &wasmPath,
                                         const std::vector<uint8_t> &wasmBytes);

  // AOT serialization
  std::string getCachePath(const std::string &wasmPath);
  bool saveToCache(wasmtime_module_t *module, const std::string &cachePath);
  wasmtime_module_t *loadFromCache(const std::string &cachePath);
  bool isCacheValid(const std::string &wasmPath, const std::string &cachePath);

  wasm_engine_t *engine_ = nullptr;
  wasmtime_store_t *store_ = nullptr;
  wasmtime_context_t *context_ = nullptr;
  std::string lastError_;
  std::string cacheDir_;
  bool aotCacheEnabled_ = true;
};

// DSP function pointers (resolved from WASM exports)
struct DspFunctions {
  wasmtime_func_t init;
  wasmtime_func_t reset;
  wasmtime_func_t process;
  wasmtime_func_t setParam;
  wasmtime_func_t getParam;
  wasmtime_func_t getInputBuffer;
  wasmtime_func_t getOutputBuffer;
  wasmtime_func_t getStateSize;
  wasmtime_func_t getState;
  wasmtime_func_t setState;

  // Instrument extensions (optional)
  wasmtime_func_t noteOn;
  wasmtime_func_t noteOff;
  wasmtime_func_t noteExpression;

  bool hasInstrumentSupport = false;
};

// Represents a loaded WASM DSP module instance
class WasmInstance {
public:
  WasmInstance(wasmtime_context_t *context, wasmtime_instance_t instance,
               wasmtime_memory_t memory, DspFunctions funcs);
  ~WasmInstance();

  // DSP lifecycle
  bool init(float sampleRate, int maxBlockSize);
  void reset();
  void process(int blockSize);

  // Parameters
  void setParam(int paramId, float value);
  float getParam(int paramId);

  // Audio buffers - returns pointer into WASM linear memory
  float *getInputBuffer(int channel);
  float *getOutputBuffer(int channel);

  // State
  int getStateSize();
  void getState(uint8_t *out);
  void setState(const uint8_t *in, int size);

  // Instrument support
  bool hasInstrumentSupport() const { return funcs_.hasInstrumentSupport; }
  void noteOn(int32_t sampleOffset, int16_t noteId, int16_t channel,
              int16_t key, float velocity);
  void noteOff(int32_t sampleOffset, int16_t noteId, int16_t channel,
               int16_t key, float velocity);
  void noteExpression(int32_t sampleOffset, int16_t noteId, int16_t channel,
                      int16_t key, int32_t expressionId, float value);

  // Memory access
  uint8_t *memoryBase();
  size_t memorySize();

private:
  wasmtime_context_t *context_;
  wasmtime_instance_t instance_;
  wasmtime_memory_t memory_;
  DspFunctions funcs_;

  // Helper to call WASM functions
  bool callVoid(wasmtime_func_t *func);
  bool callVoidFloat(wasmtime_func_t *func, float arg);
  bool callVoidInt(wasmtime_func_t *func, int32_t arg);
  bool callVoidFloatInt(wasmtime_func_t *func, float arg1, int32_t arg2);
  bool callVoidIntFloat(wasmtime_func_t *func, int32_t arg1, float arg2);
  int32_t callInt(wasmtime_func_t *func);
  int32_t callIntInt(wasmtime_func_t *func, int32_t arg);
  float callFloatInt(wasmtime_func_t *func, int32_t arg);
};

} // namespace clasp
