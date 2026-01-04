#include "clasp/runtime.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace clasp {

// Singleton instance
Runtime &Runtime::instance() {
  static Runtime runtime;
  return runtime;
}

Runtime::Runtime() {
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

  // Enable SIMD (128-bit) for DSP vector operations
  wasmtime_config_wasm_simd_set(config, true);

  // Enable relaxed SIMD (faster but potentially non-deterministic)
  wasmtime_config_wasm_relaxed_simd_set(config, true);

  // Enable bulk memory operations (faster memcpy/memset)
  wasmtime_config_wasm_bulk_memory_set(config, true);

  // Enable multi-memory (useful for separating audio buffers)
  wasmtime_config_wasm_multi_memory_set(config, true);

  // Optimization: Cranelift compiler with speed optimizations
  wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_SPEED);

  // Enable parallel compilation for faster startup
  wasmtime_config_parallel_compilation_set(config, true);

  engine_ = wasm_engine_new_with_config(config);
  if (!engine_) {
    lastError_ = "Failed to create Wasmtime engine";
    return;
  }

  // Create store
  store_ = wasmtime_store_new(engine_, nullptr, nullptr);
  if (!store_) {
    lastError_ = "Failed to create Wasmtime store";
    wasm_engine_delete(engine_);
    engine_ = nullptr;
    return;
  }

  context_ = wasmtime_store_context(store_);
}

Runtime::~Runtime() {
  if (store_) {
    wasmtime_store_delete(store_);
  }
  if (engine_) {
    wasm_engine_delete(engine_);
  }
}

std::string Runtime::getCachePath(const std::string &wasmPath) {
  // Generate cache filename from the original filename + hash for uniqueness
  // Cache location: ~/.clasp/cache/<filename>_<hash>.cwasm
  fs::path wasmFile(wasmPath);
  std::string basename = wasmFile.stem().string();

  // Hash the full path to handle same-named files in different locations
  std::hash<std::string> hasher;
  size_t hash = hasher(wasmPath);

  return cacheDir_ + "/" + basename + "_" + std::to_string(hash) + ".cwasm";
}

bool Runtime::isCacheValid(const std::string &wasmPath,
                           const std::string &cachePath) {
  if (!fs::exists(cachePath))
    return false;
  if (!fs::exists(wasmPath))
    return false;

  auto wasmTime = fs::last_write_time(wasmPath);
  auto cacheTime = fs::last_write_time(cachePath);

  return cacheTime > wasmTime;
}

bool Runtime::saveToCache(wasmtime_module_t *module,
                          const std::string &cachePath) {
  // Ensure cache directory exists
  fs::create_directories(fs::path(cachePath).parent_path());

  // Serialize the compiled module
  wasm_byte_vec_t serialized;
  wasmtime_error_t *error = wasmtime_module_serialize(module, &serialized);
  if (error) {
    wasmtime_error_delete(error);
    return false;
  }

  // Write to file
  std::ofstream file(cachePath, std::ios::binary);
  if (!file) {
    wasm_byte_vec_delete(&serialized);
    return false;
  }

  file.write(reinterpret_cast<const char *>(serialized.data), serialized.size);
  wasm_byte_vec_delete(&serialized);

  return file.good();
}

wasmtime_module_t *Runtime::loadFromCache(const std::string &cachePath) {
  // Read cached compiled module
  std::ifstream file(cachePath, std::ios::binary | std::ios::ate);
  if (!file)
    return nullptr;

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> bytes(size);
  if (!file.read(reinterpret_cast<char *>(bytes.data()), size)) {
    return nullptr;
  }

  // Deserialize
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
Runtime::compileOrLoadCached(const std::string &wasmPath,
                             const std::vector<uint8_t> &wasmBytes) {
  wasmtime_module_t *module = nullptr;

  // Try to load from AOT cache first
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

  // JIT compile the module
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

  // Save to AOT cache for next time
  if (aotCacheEnabled_ && !cacheDir_.empty()) {
    std::string cachePath = getCachePath(wasmPath);
    if (saveToCache(module, cachePath)) {
      std::cerr << "[clasp] Saved AOT cache: " << cachePath << std::endl;
    }
  }

  return module;
}

std::unique_ptr<WasmInstance> Runtime::loadModule(const std::string &wasmPath) {
  if (!engine_ || !context_) {
    lastError_ = "Runtime not initialized";
    return nullptr;
  }

  // Read WASM file
  std::ifstream file(wasmPath, std::ios::binary | std::ios::ate);
  if (!file) {
    lastError_ = "Failed to open WASM file: " + wasmPath;
    return nullptr;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> wasmBytes(size);
  if (!file.read(reinterpret_cast<char *>(wasmBytes.data()), size)) {
    lastError_ = "Failed to read WASM file: " + wasmPath;
    return nullptr;
  }

  // Compile or load from cache (JIT happens here, before any DSP calls)
  wasmtime_module_t *module = compileOrLoadCached(wasmPath, wasmBytes);
  if (!module) {
    return nullptr; // lastError_ already set
  }

  // Create linker for imports (empty for now - DSP modules shouldn't need WASI)
  wasmtime_linker_t *linker = wasmtime_linker_new(engine_);

  // Instantiate
  wasmtime_instance_t instance;
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_linker_instantiate(linker, context_, module, &instance, &trap);
  wasmtime_linker_delete(linker);
  wasmtime_module_delete(module);

  if (error || trap) {
    if (error) {
      wasm_message_t message;
      wasmtime_error_message(error, &message);
      lastError_ = std::string("Failed to instantiate: ") +
                   std::string(message.data, message.size);
      wasm_byte_vec_delete(&message);
      wasmtime_error_delete(error);
    }
    if (trap) {
      wasm_message_t message;
      wasm_trap_message(trap, &message);
      lastError_ = std::string("Trap during instantiation: ") +
                   std::string(message.data, message.size);
      wasm_byte_vec_delete(&message);
      wasm_trap_delete(trap);
    }
    return nullptr;
  }

  // Get memory export
  wasmtime_extern_t memoryExtern;
  bool found = wasmtime_instance_export_get(context_, &instance, "memory", 6,
                                            &memoryExtern);
  if (!found || memoryExtern.kind != WASMTIME_EXTERN_MEMORY) {
    lastError_ = "WASM module must export 'memory'";
    return nullptr;
  }
  wasmtime_memory_t memory = memoryExtern.of.memory;

  // Helper to get function export
  auto getFunc = [&](const char *name, wasmtime_func_t *func) -> bool {
    wasmtime_extern_t ext;
    if (!wasmtime_instance_export_get(context_, &instance, name, strlen(name),
                                      &ext)) {
      return false;
    }
    if (ext.kind != WASMTIME_EXTERN_FUNC) {
      return false;
    }
    *func = ext.of.func;
    return true;
  };

  // Get required DSP functions
  DspFunctions funcs{};

  if (!getFunc("dsp_init", &funcs.init)) {
    lastError_ = "Missing export: dsp_init";
    return nullptr;
  }
  if (!getFunc("dsp_reset", &funcs.reset)) {
    lastError_ = "Missing export: dsp_reset";
    return nullptr;
  }
  if (!getFunc("dsp_process", &funcs.process)) {
    lastError_ = "Missing export: dsp_process";
    return nullptr;
  }
  if (!getFunc("dsp_set_param", &funcs.setParam)) {
    lastError_ = "Missing export: dsp_set_param";
    return nullptr;
  }
  if (!getFunc("dsp_get_param", &funcs.getParam)) {
    lastError_ = "Missing export: dsp_get_param";
    return nullptr;
  }
  if (!getFunc("dsp_get_input_buffer", &funcs.getInputBuffer)) {
    lastError_ = "Missing export: dsp_get_input_buffer";
    return nullptr;
  }
  if (!getFunc("dsp_get_output_buffer", &funcs.getOutputBuffer)) {
    lastError_ = "Missing export: dsp_get_output_buffer";
    return nullptr;
  }
  if (!getFunc("dsp_get_state_size", &funcs.getStateSize)) {
    lastError_ = "Missing export: dsp_get_state_size";
    return nullptr;
  }
  if (!getFunc("dsp_get_state", &funcs.getState)) {
    lastError_ = "Missing export: dsp_get_state";
    return nullptr;
  }
  if (!getFunc("dsp_set_state", &funcs.setState)) {
    lastError_ = "Missing export: dsp_set_state";
    return nullptr;
  }

  // Optional instrument functions
  funcs.hasInstrumentSupport = getFunc("dsp_note_on", &funcs.noteOn) &&
                               getFunc("dsp_note_off", &funcs.noteOff);
  getFunc("dsp_note_expression", &funcs.noteExpression);
  getFunc("dsp_on_message", &funcs.onMessage);
  getFunc("dsp_get_message_buffer", &funcs.getMessageBuffer);

  return std::make_unique<WasmInstance>(context_, instance, memory, funcs);
}

// WasmInstance implementation

WasmInstance::WasmInstance(wasmtime_context_t *context,
                           wasmtime_instance_t instance,
                           wasmtime_memory_t memory, DspFunctions funcs)
    : context_(context), instance_(instance), memory_(memory), funcs_(funcs) {}

WasmInstance::~WasmInstance() {
  // Instance is owned by the store, nothing to delete
}

uint8_t *WasmInstance::memoryBase() {
  return wasmtime_memory_data(context_, &memory_);
}

size_t WasmInstance::memorySize() {
  return wasmtime_memory_data_size(context_, &memory_);
}

bool WasmInstance::callVoid(wasmtime_func_t *func) {
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, nullptr, 0, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
  return !error && !trap;
}

bool WasmInstance::callVoidInt(wasmtime_func_t *func, int32_t arg) {
  wasmtime_val_t args[1] = {{.kind = WASMTIME_I32, .of = {.i32 = arg}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, args, 1, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
  return !error && !trap;
}

bool WasmInstance::callVoidFloat(wasmtime_func_t *func, float arg) {
  wasmtime_val_t args[1] = {{.kind = WASMTIME_F32, .of = {.f32 = arg}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, args, 1, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
  return !error && !trap;
}

bool WasmInstance::callVoidFloatInt(wasmtime_func_t *func, float arg1,
                                    int32_t arg2) {
  wasmtime_val_t args[2] = {{.kind = WASMTIME_F32, .of = {.f32 = arg1}},
                            {.kind = WASMTIME_I32, .of = {.i32 = arg2}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, args, 2, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
  return !error && !trap;
}

bool WasmInstance::callVoidIntFloat(wasmtime_func_t *func, int32_t arg1,
                                    float arg2) {
  wasmtime_val_t args[2] = {{.kind = WASMTIME_I32, .of = {.i32 = arg1}},
                            {.kind = WASMTIME_F32, .of = {.f32 = arg2}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, args, 2, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
  return !error && !trap;
}

int32_t WasmInstance::callInt(wasmtime_func_t *func) {
  wasmtime_val_t result[1];
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, nullptr, 0, result, 1, &trap);
  if (error) {
    wasmtime_error_delete(error);
    return 0;
  }
  if (trap) {
    wasm_trap_delete(trap);
    return 0;
  }
  return result[0].of.i32;
}

int32_t WasmInstance::callIntInt(wasmtime_func_t *func, int32_t arg) {
  wasmtime_val_t args[1] = {{.kind = WASMTIME_I32, .of = {.i32 = arg}}};
  wasmtime_val_t result[1];
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, args, 1, result, 1, &trap);
  if (error) {
    wasmtime_error_delete(error);
    return 0;
  }
  if (trap) {
    wasm_trap_delete(trap);
    return 0;
  }
  return result[0].of.i32;
}

float WasmInstance::callFloatInt(wasmtime_func_t *func, int32_t arg) {
  wasmtime_val_t args[1] = {{.kind = WASMTIME_I32, .of = {.i32 = arg}}};
  wasmtime_val_t result[1];
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, func, args, 1, result, 1, &trap);
  if (error) {
    wasmtime_error_delete(error);
    return 0.0f;
  }
  if (trap) {
    wasm_trap_delete(trap);
    return 0.0f;
  }
  return result[0].of.f32;
}

bool WasmInstance::init(float sampleRate, int maxBlockSize) {
  return callVoidFloatInt(&funcs_.init, sampleRate, maxBlockSize);
}

void WasmInstance::reset() { callVoid(&funcs_.reset); }

void WasmInstance::process(int blockSize) {
  callVoidInt(&funcs_.process, blockSize);
}

void WasmInstance::setParam(int paramId, float value) {
  callVoidIntFloat(&funcs_.setParam, paramId, value);
}

float WasmInstance::getParam(int paramId) {
  return callFloatInt(&funcs_.getParam, paramId);
}

float *WasmInstance::getInputBuffer(int channel) {
  int32_t ptr = callIntInt(&funcs_.getInputBuffer, channel);
  if (ptr == 0)
    return nullptr;
  return reinterpret_cast<float *>(memoryBase() + ptr);
}

float *WasmInstance::getOutputBuffer(int channel) {
  int32_t ptr = callIntInt(&funcs_.getOutputBuffer, channel);
  if (ptr == 0)
    return nullptr;
  return reinterpret_cast<float *>(memoryBase() + ptr);
}

int WasmInstance::getStateSize() { return callInt(&funcs_.getStateSize); }

void WasmInstance::getState(uint8_t *out) {
  // For state, we need to pass a pointer into WASM memory
  // The DSP module should have a way to write state to a buffer
  // This is a simplified version - real implementation would need
  // to allocate WASM memory and copy back
  int32_t size = getStateSize();
  if (size <= 0)
    return;

  // Call with a pointer argument (to WASM linear memory)
  // For now, assume the DSP module manages its own state buffer
  wasmtime_val_t args[1] = {{.kind = WASMTIME_I32, .of = {.i32 = 0}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error = wasmtime_func_call(context_, &funcs_.getState, args,
                                               1, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);

  // Copy from WASM memory - assumes state is at beginning of memory
  // Real implementation would use a proper allocation scheme
  std::memcpy(out, memoryBase(), size);
}

void WasmInstance::setState(const uint8_t *in, int size) {
  if (size <= 0)
    return;

  // Copy to WASM memory (simplified - assumes beginning of memory is free)
  std::memcpy(memoryBase(), in, size);

  // Call dsp_set_state with pointer to the data
  wasmtime_val_t args[1] = {{.kind = WASMTIME_I32, .of = {.i32 = 0}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error = wasmtime_func_call(context_, &funcs_.setState, args,
                                               1, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
}

void WasmInstance::noteOn(int32_t sampleOffset, int16_t noteId, int16_t channel,
                          int16_t key, float velocity) {
  if (!funcs_.hasInstrumentSupport)
    return;

  // Pack note event into WASM memory and call
  // Simplified - real implementation would use proper struct layout
  wasmtime_val_t args[5] = {{.kind = WASMTIME_I32, .of = {.i32 = sampleOffset}},
                            {.kind = WASMTIME_I32, .of = {.i32 = noteId}},
                            {.kind = WASMTIME_I32, .of = {.i32 = channel}},
                            {.kind = WASMTIME_I32, .of = {.i32 = key}},
                            {.kind = WASMTIME_F32, .of = {.f32 = velocity}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, &funcs_.noteOn, args, 5, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
}

void WasmInstance::noteOff(int32_t sampleOffset, int16_t noteId,
                           int16_t channel, int16_t key, float velocity) {
  if (!funcs_.hasInstrumentSupport)
    return;

  wasmtime_val_t args[5] = {{.kind = WASMTIME_I32, .of = {.i32 = sampleOffset}},
                            {.kind = WASMTIME_I32, .of = {.i32 = noteId}},
                            {.kind = WASMTIME_I32, .of = {.i32 = channel}},
                            {.kind = WASMTIME_I32, .of = {.i32 = key}},
                            {.kind = WASMTIME_F32, .of = {.f32 = velocity}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error =
      wasmtime_func_call(context_, &funcs_.noteOff, args, 5, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
}

void WasmInstance::noteExpression(int32_t sampleOffset, int16_t noteId,
                                  int16_t channel, int16_t key,
                                  int32_t expressionId, float value) {
  if (!funcs_.hasInstrumentSupport)
    return;

  wasmtime_val_t args[6] = {{.kind = WASMTIME_I32, .of = {.i32 = sampleOffset}},
                            {.kind = WASMTIME_I32, .of = {.i32 = noteId}},
                            {.kind = WASMTIME_I32, .of = {.i32 = channel}},
                            {.kind = WASMTIME_I32, .of = {.i32 = key}},
                            {.kind = WASMTIME_I32, .of = {.i32 = expressionId}},
                            {.kind = WASMTIME_F32, .of = {.f32 = value}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error = wasmtime_func_call(context_, &funcs_.noteExpression,
                                               args, 6, nullptr, 0, &trap);
  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
}

// Helper to check if a function was resolved
static bool isValid(const wasmtime_func_t &func) { return func.store_id != 0; }

void WasmInstance::onMessage(const void *buffer, uint32_t size) {
  if (!isValid(funcs_.onMessage) || !isValid(funcs_.getMessageBuffer))
    return;

  // Get offset from WASM
  int32_t offset =
      callIntInt(&funcs_.getMessageBuffer, static_cast<int32_t>(size));
  if (offset == 0)
    return;

  // Copy data
  uint8_t *base = memoryBase();
  size_t memSize = memorySize();
  if (static_cast<size_t>(offset) + size > memSize)
    return;

  std::memcpy(base + offset, buffer, size);

  // Call onMessage
  wasmtime_val_t args[2] = {
      {.kind = WASMTIME_I32, .of = {.i32 = offset}},
      {.kind = WASMTIME_I32, .of = {.i32 = (int32_t)size}}};
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error = wasmtime_func_call(context_, &funcs_.onMessage,
                                               args, 2, nullptr, 0, &trap);

  if (error)
    wasmtime_error_delete(error);
  if (trap)
    wasm_trap_delete(trap);
}

} // namespace clasp
