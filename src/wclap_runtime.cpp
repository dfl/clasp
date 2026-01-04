#include "clasp/wclap_runtime.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace clasp {

//------------------------------------------------------------------------------
// WclapRuntime - Singleton managing Wasmtime engine
//------------------------------------------------------------------------------

WclapRuntime &WclapRuntime::instance() {
  static WclapRuntime runtime;
  return runtime;
}

WclapRuntime::WclapRuntime() {
// Set default cache directory
#ifdef _WIN32
  const char *appData = getenv("LOCALAPPDATA");
  if (appData) {
    cacheDir_ = std::string(appData) + "\\clasp\\cache";
  }
#else
  const char *home = getenv("HOME");
  if (home) {
    cacheDir_ = std::string(home) + "/.clasp/cache";
  }
#endif

  // Create engine with optimized configuration for DSP
  wasm_config_t *config = wasm_config_new();
  if (!config) {
    lastError_ = "Failed to create Wasmtime config";
    return;
  }

  // Enable SIMD for DSP vector operations
  wasmtime_config_wasm_simd_set(config, true);
  wasmtime_config_wasm_relaxed_simd_set(config, true);
  wasmtime_config_wasm_bulk_memory_set(config, true);
  wasmtime_config_wasm_multi_memory_set(config, true);

  // Enable reference types for function tables
  wasmtime_config_wasm_reference_types_set(config, true);

  // Optimization settings
  wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_SPEED);
  wasmtime_config_parallel_compilation_set(config, true);

  engine_ = wasm_engine_new_with_config(config);
  if (!engine_) {
    lastError_ = "Failed to create Wasmtime engine";
    return;
  }
}

WclapRuntime::~WclapRuntime() {
  if (engine_) {
    wasm_engine_delete(engine_);
  }
}

std::string WclapRuntime::getCachePath(const std::string &wasmPath) {
  fs::path wasmFile(wasmPath);
  std::string basename = wasmFile.stem().string();
  std::hash<std::string> hasher;
  size_t hash = hasher(wasmPath);
  return cacheDir_ + "/" + basename + "_" + std::to_string(hash) + ".cwasm";
}

bool WclapRuntime::isCacheValid(const std::string &wasmPath,
                                const std::string &cachePath) {
  if (!fs::exists(cachePath))
    return false;
  if (!fs::exists(wasmPath))
    return false;

  auto wasmTime = fs::last_write_time(wasmPath);
  auto cacheTime = fs::last_write_time(cachePath);
  return cacheTime > wasmTime;
}

bool WclapRuntime::saveToCache(wasmtime_module_t *module,
                               const std::string &cachePath) {
  fs::create_directories(fs::path(cachePath).parent_path());

  wasm_byte_vec_t serialized;
  wasmtime_error_t *error = wasmtime_module_serialize(module, &serialized);
  if (error) {
    wasmtime_error_delete(error);
    return false;
  }

  std::ofstream file(cachePath, std::ios::binary);
  if (!file) {
    wasm_byte_vec_delete(&serialized);
    return false;
  }

  file.write(reinterpret_cast<const char *>(serialized.data), serialized.size);
  wasm_byte_vec_delete(&serialized);
  return file.good();
}

wasmtime_module_t *WclapRuntime::loadFromCache(const std::string &cachePath) {
  std::ifstream file(cachePath, std::ios::binary | std::ios::ate);
  if (!file)
    return nullptr;

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> bytes(size);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), size)) {
    return nullptr;
  }

  wasmtime_module_t *module = nullptr;
  wasmtime_error_t *error =
      wasmtime_module_deserialize(engine_, bytes.data(), bytes.size(), &module);
  if (error) {
    wasmtime_error_delete(error);
    return nullptr;
  }

  return module;
}

wasmtime_module_t *
WclapRuntime::compileOrLoadCached(const std::string &wasmPath,
                                  const std::vector<uint8_t> &wasmBytes) {
  wasmtime_module_t *module = nullptr;

  // Try AOT cache first
  if (aotCacheEnabled_ && !cacheDir_.empty()) {
    std::string cachePath = getCachePath(wasmPath);
    if (isCacheValid(wasmPath, cachePath)) {
      module = loadFromCache(cachePath);
      if (module) {
        std::cerr << "[clasp] Loaded AOT-cached module: " << wasmPath
                  << std::endl;
        return module;
      }
    }
  }

  // JIT compile
  std::cerr << "[clasp] JIT compiling: " << wasmPath << std::endl;
  wasmtime_error_t *error =
      wasmtime_module_new(engine_, wasmBytes.data(), wasmBytes.size(), &module);
  if (error) {
    wasm_message_t message;
    wasmtime_error_message(error, &message);
    lastError_ = std::string("Failed to compile WASM: ") +
                 std::string(message.data, message.size);
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    return nullptr;
  }

  // Save to cache
  if (aotCacheEnabled_ && !cacheDir_.empty()) {
    std::string cachePath = getCachePath(wasmPath);
    if (saveToCache(module, cachePath)) {
      std::cerr << "[clasp] Saved AOT cache: " << cachePath << std::endl;
    }
  }

  return module;
}

std::unique_ptr<wclap::Instance<WasmtimeInstance>>
WclapRuntime::loadModule(const std::string &wasmPath) {
  // Create the instance - it will load and compile the module
  auto instance =
      std::make_unique<wclap::Instance<WasmtimeInstance>>(*this, wasmPath);

  if (instance->error()) {
    lastError_ = *instance->error();
    return nullptr;
  }

  return instance;
}

//------------------------------------------------------------------------------
// WasmtimeInstance - Per-module instance wrapping Wasmtime
//------------------------------------------------------------------------------

WasmtimeInstance::WasmtimeInstance(
    wclap::Instance<WasmtimeInstance> *wrapper, WclapRuntime &runtime,
    const std::string &wasmPath)
    : wrapper_(wrapper), runtime_(runtime), path_(wasmPath) {

  // Read WASM file
  std::ifstream file(wasmPath, std::ios::binary | std::ios::ate);
  if (!file) {
    error_ = "Failed to open WASM file: " + wasmPath;
    return;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> wasmBytes(size);
  if (!file.read(reinterpret_cast<char *>(wasmBytes.data()), size)) {
    error_ = "Failed to read WASM file: " + wasmPath;
    return;
  }

  // Compile or load from cache
  module_ = runtime_.compileOrLoadCached(wasmPath, wasmBytes);
  if (!module_) {
    error_ = runtime_.lastError();
    return;
  }

  // Create store for this instance
  store_ = wasmtime_store_new(runtime_.engine(), nullptr, nullptr);
  if (!store_) {
    error_ = "Failed to create Wasmtime store";
    wasmtime_module_delete(module_);
    module_ = nullptr;
    return;
  }
  context_ = wasmtime_store_context(store_);

  // Create linker and add WASI (WCLAP modules typically need WASI)
  wasmtime_linker_t *linker = wasmtime_linker_new(runtime_.engine());

  // Add WASI to linker
  wasmtime_error_t *error = wasmtime_linker_define_wasi(linker);
  if (error) {
    wasm_message_t message;
    wasmtime_error_message(error, &message);
    error_ = std::string("Failed to add WASI: ") +
             std::string(message.data, message.size);
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    wasmtime_linker_delete(linker);
    return;
  }

  // Configure WASI
  wasi_config_t *wasi_config = wasi_config_new();
  wasi_config_inherit_stdout(wasi_config);
  wasi_config_inherit_stderr(wasi_config);

  // Set up virtual filesystem paths
  // /plugin/ -> the plugin's bundle directory (read-only access)
  fs::path bundlePath = fs::path(wasmPath).parent_path();
  wasi_config_preopen_dir(wasi_config, bundlePath.string().c_str(), "/plugin",
                          WASMTIME_WASI_DIR_PERMS_READ,
                          WASMTIME_WASI_FILE_PERMS_READ);

  error = wasmtime_context_set_wasi(context_, wasi_config);
  if (error) {
    wasm_message_t message;
    wasmtime_error_message(error, &message);
    error_ = std::string("Failed to configure WASI: ") +
             std::string(message.data, message.size);
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    wasmtime_linker_delete(linker);
    return;
  }

  // Instantiate the module
  wasm_trap_t *trap = nullptr;
  error =
      wasmtime_linker_instantiate(linker, context_, module_, &instance_, &trap);
  wasmtime_linker_delete(linker);

  if (error || trap) {
    if (error) {
      wasm_message_t message;
      wasmtime_error_message(error, &message);
      error_ = std::string("Failed to instantiate: ") +
               std::string(message.data, message.size);
      wasm_byte_vec_delete(&message);
      wasmtime_error_delete(error);
    }
    if (trap) {
      wasm_message_t message;
      wasm_trap_message(trap, &message);
      error_ = std::string("Trap during instantiation: ") +
               std::string(message.data, message.size);
      wasm_byte_vec_delete(&message);
      wasm_trap_delete(trap);
    }
    return;
  }

  // Resolve required exports
  if (!resolveExports()) {
    return; // error_ already set
  }
}

WasmtimeInstance::~WasmtimeInstance() {
  if (store_) {
    wasmtime_store_delete(store_);
  }
  if (module_) {
    wasmtime_module_delete(module_);
  }
}

bool WasmtimeInstance::resolveExports() {
  // Get memory export
  wasmtime_extern_t memoryExtern;
  if (!findExport("memory", &memoryExtern) ||
      memoryExtern.kind != WASMTIME_EXTERN_MEMORY) {
    error_ = "WASM module must export 'memory'";
    return false;
  }
  memory_ = memoryExtern.of.memory;

  // Get function table (for indirect calls)
  wasmtime_extern_t tableExtern;
  if (findExport("__indirect_function_table", &tableExtern) &&
      tableExtern.kind == WASMTIME_EXTERN_TABLE) {
    funcTable_ = tableExtern.of.table;
    hasFuncTable_ = true;
  }

  // Look for wclap_plugin_entry or clap_entry
  wasmtime_extern_t entryExtern;
  if (findExport("wclap_plugin_entry", &entryExtern)) {
    if (entryExtern.kind == WASMTIME_EXTERN_GLOBAL) {
      // It's a global - read the pointer value
      wasmtime_val_t val;
      wasmtime_global_get(context_, &entryExtern.of.global, &val);
      if (val.kind == WASMTIME_I32) {
        entryPtr32_ = val.of.i32;
        is64_ = false;
      } else if (val.kind == WASMTIME_I64) {
        entryPtr64_ = val.of.i64;
        is64_ = true;
      }
    }
  } else if (findExport("clap_entry", &entryExtern)) {
    if (entryExtern.kind == WASMTIME_EXTERN_GLOBAL) {
      wasmtime_val_t val;
      wasmtime_global_get(context_, &entryExtern.of.global, &val);
      if (val.kind == WASMTIME_I32) {
        entryPtr32_ = val.of.i32;
        is64_ = false;
      } else if (val.kind == WASMTIME_I64) {
        entryPtr64_ = val.of.i64;
        is64_ = true;
      }
    }
  }

  // Entry point is required but we'll check in init()
  return true;
}

bool WasmtimeInstance::findExport(const char *name, wasmtime_extern_t *out) {
  return wasmtime_instance_export_get(context_, &instance_, name, strlen(name),
                                      out);
}

uint8_t *WasmtimeInstance::memoryBase() {
  return wasmtime_memory_data(context_, &memory_);
}

size_t WasmtimeInstance::memorySize() {
  return wasmtime_memory_data_size(context_, &memory_);
}

bool WasmtimeInstance::getArrayImpl(uint64_t ptr, void *result, size_t bytes) {
  if (ptr + bytes > memorySize()) {
    return false;
  }
  std::memcpy(result, memoryBase() + ptr, bytes);
  return true;
}

bool WasmtimeInstance::setArrayImpl(uint64_t ptr, const void *value,
                                    size_t bytes) {
  if (ptr + bytes > memorySize()) {
    return false;
  }
  std::memcpy(memoryBase() + ptr, value, bytes);
  return true;
}

uint32_t WasmtimeInstance::init32() {
  if (entryPtr32_ == 0) {
    error_ = "No wclap_plugin_entry or clap_entry export found";
    return 0;
  }

  // Call _initialize if present (WASI initialization)
  wasmtime_extern_t initExtern;
  if (findExport("_initialize", &initExtern) &&
      initExtern.kind == WASMTIME_EXTERN_FUNC) {
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *err = wasmtime_func_call(context_, &initExtern.of.func,
                                               nullptr, 0, nullptr, 0, &trap);
    if (err) {
      wasmtime_error_delete(err);
    }
    if (trap) {
      wasm_trap_delete(trap);
    }
  }

  return entryPtr32_;
}

uint64_t WasmtimeInstance::init64() {
  if (entryPtr64_ == 0) {
    error_ = "No wclap_plugin_entry or clap_entry export found";
    return 0;
  }

  // Call _initialize if present
  wasmtime_extern_t initExtern;
  if (findExport("_initialize", &initExtern) &&
      initExtern.kind == WASMTIME_EXTERN_FUNC) {
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *err = wasmtime_func_call(context_, &initExtern.of.func,
                                               nullptr, 0, nullptr, 0, &trap);
    if (err) {
      wasmtime_error_delete(err);
    }
    if (trap) {
      wasm_trap_delete(trap);
    }
  }

  return entryPtr64_;
}

int32_t WasmtimeInstance::nextThreadId() { return nextThreadId_.fetch_add(1); }

void WasmtimeInstance::runThread(int32_t threadId, uint64_t threadContext) {
  // WASI threads support - find wasi_thread_start and call it
  wasmtime_extern_t startExtern;
  if (findExport("wasi_thread_start", &startExtern) &&
      startExtern.kind == WASMTIME_EXTERN_FUNC) {
    wasmtime_val_t args[2] = {{.kind = WASMTIME_I32, .of = {.i32 = threadId}},
                              {.kind = WASMTIME_I64, .of = {.i64 = (int64_t)threadContext}}};
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *err = wasmtime_func_call(context_, &startExtern.of.func,
                                               args, 2, nullptr, 0, &trap);
    if (err)
      wasmtime_error_delete(err);
    if (trap)
      wasm_trap_delete(trap);
  }
}

wclap32::Pointer<void> WasmtimeInstance::malloc32(uint32_t size) {
  // Try to find malloc export
  wasmtime_extern_t mallocExtern;
  if (findExport("malloc", &mallocExtern) &&
      mallocExtern.kind == WASMTIME_EXTERN_FUNC) {
    wasmtime_val_t args[1] = {{.kind = WASMTIME_I32, .of = {.i32 = (int32_t)size}}};
    wasmtime_val_t result[1];
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *err = wasmtime_func_call(
        context_, &mallocExtern.of.func, args, 1, result, 1, &trap);
    if (!err && !trap) {
      return {static_cast<uint32_t>(result[0].of.i32)};
    }
    if (err)
      wasmtime_error_delete(err);
    if (trap)
      wasm_trap_delete(trap);
  }

  // Try cabi_realloc (component model)
  if (findExport("cabi_realloc", &mallocExtern) &&
      mallocExtern.kind == WASMTIME_EXTERN_FUNC) {
    // cabi_realloc(old_ptr, old_size, align, new_size) -> ptr
    wasmtime_val_t args[4] = {{.kind = WASMTIME_I32, .of = {.i32 = 0}},
                              {.kind = WASMTIME_I32, .of = {.i32 = 0}},
                              {.kind = WASMTIME_I32, .of = {.i32 = 8}},
                              {.kind = WASMTIME_I32, .of = {.i32 = (int32_t)size}}};
    wasmtime_val_t result[1];
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *err = wasmtime_func_call(
        context_, &mallocExtern.of.func, args, 4, result, 1, &trap);
    if (!err && !trap) {
      return {static_cast<uint32_t>(result[0].of.i32)};
    }
    if (err)
      wasmtime_error_delete(err);
    if (trap)
      wasm_trap_delete(trap);
  }

  return {0};
}

wclap64::Pointer<void> WasmtimeInstance::malloc64(uint64_t size) {
  // 64-bit allocation - similar to malloc32 but with i64 types
  wasmtime_extern_t mallocExtern;
  if (findExport("malloc", &mallocExtern) &&
      mallocExtern.kind == WASMTIME_EXTERN_FUNC) {
    wasmtime_val_t args[1] = {{.kind = WASMTIME_I64, .of = {.i64 = (int64_t)size}}};
    wasmtime_val_t result[1];
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *err = wasmtime_func_call(
        context_, &mallocExtern.of.func, args, 1, result, 1, &trap);
    if (!err && !trap) {
      return {static_cast<uint64_t>(result[0].of.i64)};
    }
    if (err)
      wasmtime_error_delete(err);
    if (trap)
      wasm_trap_delete(trap);
  }
  return {0};
}

uint32_t WasmtimeInstance::registerHostCallback(void *context, void *fn,
                                                size_t argCount) {
  std::lock_guard<std::mutex> lock(callbackMutex_);

  // Store the callback info
  uint32_t index = static_cast<uint32_t>(hostCallbacks_.size());
  hostCallbacks_.push_back({context, fn, argCount});

  // Note: Actually registering the callback with Wasmtime's function table
  // requires more complex setup with wasmtime_func_new. For now we just
  // track them. The WASM module typically uses these indices to call back.

  return index;
}

// Template specialization for calling WASM functions
// This is a simplified version - full implementation would need
// to handle all WASM value types properly
template <>
bool WasmtimeInstance::callImpl<bool>(uint64_t funcIndex) {
  if (!hasFuncTable_ || funcIndex == 0) {
    return false;
  }

  wasmtime_val_t funcRef;
  if (!wasmtime_table_get(context_, &funcTable_, static_cast<uint32_t>(funcIndex), &funcRef)) {
    return false;
  }

  if (funcRef.kind != WASMTIME_FUNCREF || funcRef.of.funcref.store_id == 0) {
    return false;
  }

  wasmtime_val_t result[1];
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *err = wasmtime_func_call(
      context_, &funcRef.of.funcref, nullptr, 0, result, 1, &trap);

  if (err) {
    wasmtime_error_delete(err);
    return false;
  }
  if (trap) {
    wasm_trap_delete(trap);
    return false;
  }

  return result[0].of.i32 != 0;
}

template <>
void WasmtimeInstance::callImpl<void>(uint64_t funcIndex) {
  if (!hasFuncTable_ || funcIndex == 0) {
    return;
  }

  wasmtime_val_t funcRef;
  if (!wasmtime_table_get(context_, &funcTable_, static_cast<uint32_t>(funcIndex), &funcRef)) {
    return;
  }

  if (funcRef.kind != WASMTIME_FUNCREF || funcRef.of.funcref.store_id == 0) {
    return;
  }

  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *err = wasmtime_func_call(context_, &funcRef.of.funcref,
                                             nullptr, 0, nullptr, 0, &trap);

  if (err)
    wasmtime_error_delete(err);
  if (trap)
    wasm_trap_delete(trap);
}

template <>
int32_t WasmtimeInstance::callImpl<int32_t>(uint64_t funcIndex) {
  if (!hasFuncTable_ || funcIndex == 0) {
    return 0;
  }

  wasmtime_val_t funcRef;
  if (!wasmtime_table_get(context_, &funcTable_, static_cast<uint32_t>(funcIndex), &funcRef)) {
    return 0;
  }

  if (funcRef.kind != WASMTIME_FUNCREF || funcRef.of.funcref.store_id == 0) {
    return 0;
  }

  wasmtime_val_t result[1];
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *err = wasmtime_func_call(
      context_, &funcRef.of.funcref, nullptr, 0, result, 1, &trap);

  if (err) {
    wasmtime_error_delete(err);
    return 0;
  }
  if (trap) {
    wasm_trap_delete(trap);
    return 0;
  }

  return result[0].of.i32;
}

template <>
uint32_t WasmtimeInstance::callImpl<uint32_t>(uint64_t funcIndex) {
  return static_cast<uint32_t>(callImpl<int32_t>(funcIndex));
}

} // namespace clasp
