#include "clasp/instance.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace clasp {

// Factory function to create WCLAP instance
std::unique_ptr<IPluginInstance>
createPluginInstance(const PluginManifest &manifest) {
  if (manifest.bundleType != BundleType::Wclap) {
    std::cerr << "[thunder] Unsupported bundle type: " << manifest.bundlePath
              << std::endl;
    return nullptr;
  }
  return std::make_unique<WclapPluginInstance>(manifest);
}

// ============================================================================
// WclapPluginInstance - WCLAP plugin wrapper
// ============================================================================

WclapPluginInstance::WclapPluginInstance(const PluginManifest &manifest)
    : manifest_(manifest), paramCount_(manifest.parameters.size()) {
  if (paramCount_ > 0) {
    paramValues_ = std::make_unique<std::atomic<float>[]>(paramCount_);
    for (size_t i = 0; i < paramCount_; ++i) {
      paramValues_[i].store(manifest.parameters[i].defaultValue);
    }
  }
}

WclapPluginInstance::~WclapPluginInstance() { destroy(); }

bool WclapPluginInstance::init() {
  // Load the WASM module via WclapRuntime
  wasm_ = WclapRuntime::instance().loadModule(manifest_.wasmPath);
  if (!wasm_) {
    std::cerr << "[thunder] Failed to load WCLAP module: " << manifest_.wasmPath
              << std::endl;
    return false;
  }

  // Initialize the WASM module and get entry point
  if (!wasm_->init()) {
    std::cerr << "[thunder] Failed to initialize WCLAP module (no entry point)"
              << std::endl;
    return false;
  }

  // Entry point is now available via entry32 or entry64
  if (wasm_->is64()) {
    std::cerr << "[thunder] WCLAP module loaded (64-bit), entry at 0x" << std::hex
              << wasm_->entry64.wasmPointer << std::dec << std::endl;
  } else {
    std::cerr << "[thunder] WCLAP module loaded (32-bit), entry at 0x" << std::hex
              << wasm_->entry32.wasmPointer << std::dec << std::endl;
  }

  // TODO: Read clap_plugin_entry struct from WASM memory
  // TODO: Call entry->init(pluginPath)
  // TODO: Get factory and create plugin instance

  return true;
}

void WclapPluginInstance::destroy() {
  // TODO: Call plugin->destroy() in WASM
  // TODO: Call entry->deinit()
  pluginPtr_ = 0;
  wasm_.reset();
}

bool WclapPluginInstance::activate(double sampleRate, uint32_t minFrames,
                                    uint32_t maxFrames) {
  if (!wasm_)
    return false;

  sampleRate_ = sampleRate;
  maxBlockSize_ = maxFrames;

  // TODO: Call plugin->activate() in WASM

  activated_ = true;
  return true;
}

void WclapPluginInstance::deactivate() {
  // TODO: Call plugin->deactivate() in WASM
  activated_ = false;
}

bool WclapPluginInstance::startProcessing() {
  // TODO: Call plugin->start_processing() in WASM
  processing_ = true;
  return true;
}

void WclapPluginInstance::stopProcessing() {
  // TODO: Call plugin->stop_processing() in WASM
  processing_ = false;
}

void WclapPluginInstance::reset() {
  // TODO: Call plugin->reset() in WASM
}

clap_process_status
WclapPluginInstance::process(const clap_process_t *process) {
  if (!wasm_ || !activated_ || !processing_) {
    return CLAP_PROCESS_ERROR;
  }

  // TODO: Marshal clap_process_t to WASM memory
  // TODO: Call plugin->process() in WASM
  // TODO: Unmarshal results back

  // For now, pass through audio unchanged
  if (process->audio_inputs && process->audio_outputs &&
      process->audio_inputs_count > 0 && process->audio_outputs_count > 0) {
    const auto &in = process->audio_inputs[0];
    auto &out = process->audio_outputs[0];
    uint32_t channels = std::min(in.channel_count, out.channel_count);
    for (uint32_t ch = 0; ch < channels; ++ch) {
      if (in.data32[ch] && out.data32[ch]) {
        std::memcpy(out.data32[ch], in.data32[ch],
                    process->frames_count * sizeof(float));
      }
    }
  }

  return CLAP_PROCESS_CONTINUE;
}

void WclapPluginInstance::setParameterValue(clap_id paramId, double value) {
  if (!paramValues_)
    return;
  for (size_t i = 0; i < paramCount_; ++i) {
    if (manifest_.parameters[i].id == static_cast<int32_t>(paramId)) {
      paramValues_[i].store(static_cast<float>(value));
      // TODO: Forward to WASM plugin
      return;
    }
  }
}

double WclapPluginInstance::getParameterValue(clap_id paramId) const {
  if (!paramValues_)
    return 0.0;
  for (size_t i = 0; i < paramCount_; ++i) {
    if (manifest_.parameters[i].id == static_cast<int32_t>(paramId)) {
      return paramValues_[i].load();
    }
  }
  return 0.0;
}

bool WclapPluginInstance::saveState(const clap_ostream_t *stream) {
  // TODO: Call plugin state extension in WASM
  uint32_t marker = 0;
  stream->write(stream, &marker, sizeof(marker));
  return true;
}

bool WclapPluginInstance::loadState(const clap_istream_t *stream) {
  // TODO: Call plugin state extension in WASM
  uint32_t size;
  stream->read(stream, &size, sizeof(size));
  return true;
}

void WclapPluginInstance::onMessage(const void *buffer, uint32_t size) {
  // TODO: Forward to WASM plugin
}

} // namespace clasp
