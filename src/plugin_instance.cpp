#include "clasp/instance.h"
#include "clasp/runtime.h"
#include <algorithm>
#include <cstring>

namespace clasp {

PluginInstance::PluginInstance(const PluginManifest &manifest)
    : manifest_(manifest), paramCount_(manifest.parameters.size()) {
  // Initialize parameter storage using unique_ptr array (atomics aren't
  // copyable)
  if (paramCount_ > 0) {
    paramValues_ = std::make_unique<std::atomic<float>[]>(paramCount_);
    for (size_t i = 0; i < paramCount_; ++i) {
      paramValues_[i].store(manifest.parameters[i].defaultValue);
    }
  }
}

PluginInstance::~PluginInstance() { destroy(); }

bool PluginInstance::init() {
  // Load the WASM module
  wasm_ = Runtime::instance().loadModule(manifest_.wasmPath);
  if (!wasm_) {
    return false;
  }
  return true;
}

void PluginInstance::destroy() { wasm_.reset(); }

bool PluginInstance::activate(double sampleRate, uint32_t minFrames,
                              uint32_t maxFrames) {
  if (!wasm_)
    return false;

  sampleRate_ = sampleRate;
  maxBlockSize_ = maxFrames;

  // Initialize the DSP
  if (!wasm_->init(static_cast<float>(sampleRate),
                   static_cast<int>(maxFrames))) {
    return false;
  }

  // Set initial parameter values
  for (size_t i = 0; i < paramCount_; ++i) {
    wasm_->setParam(manifest_.parameters[i].id, paramValues_[i].load());
  }

  activated_ = true;
  return true;
}

void PluginInstance::deactivate() { activated_ = false; }

bool PluginInstance::startProcessing() {
  processing_ = true;
  return true;
}

void PluginInstance::stopProcessing() { processing_ = false; }

void PluginInstance::reset() {
  if (wasm_) {
    wasm_->init(static_cast<float>(sampleRate_),
                static_cast<int>(maxBlockSize_));
  }
  pendingNoteOns_.clear();
  pendingNoteOffs_.clear();
}

void PluginInstance::setParameterValue(clap_id paramId, double value) {
  if (!paramValues_)
    return;
  // Find parameter index
  for (size_t i = 0; i < paramCount_; ++i) {
    if (manifest_.parameters[i].id == static_cast<int32_t>(paramId)) {
      paramValues_[i].store(static_cast<float>(value));
      return;
    }
  }
}

double PluginInstance::getParameterValue(clap_id paramId) const {
  if (!paramValues_)
    return 0.0;
  for (size_t i = 0; i < paramCount_; ++i) {
    if (manifest_.parameters[i].id == static_cast<int32_t>(paramId)) {
      return paramValues_[i].load();
    }
  }
  return 0.0;
}

void PluginInstance::processInputEvents(const clap_input_events_t *events) {
  if (!events)
    return;

  pendingNoteOns_.clear();
  pendingNoteOffs_.clear();

  for (uint32_t i = 0; i < events->size(events); ++i) {
    auto event = events->get(events, i);

    switch (event->type) {
    case CLAP_EVENT_PARAM_VALUE: {
      auto pv = reinterpret_cast<const clap_event_param_value_t *>(event);
      setParameterValue(pv->param_id, pv->value);
      break;
    }

    case CLAP_EVENT_NOTE_ON: {
      if (manifest_.isInstrument && wasm_ && wasm_->hasInstrumentSupport()) {
        auto note = reinterpret_cast<const clap_event_note_t *>(event);
        NoteEvent ne;
        ne.sampleOffset = event->time;
        ne.noteId = note->note_id;
        ne.channel = note->channel;
        ne.key = note->key;
        ne.velocity = static_cast<float>(note->velocity);
        pendingNoteOns_.push_back(ne);
      }
      break;
    }

    case CLAP_EVENT_NOTE_OFF: {
      if (manifest_.isInstrument && wasm_ && wasm_->hasInstrumentSupport()) {
        auto note = reinterpret_cast<const clap_event_note_t *>(event);
        NoteEvent ne;
        ne.sampleOffset = event->time;
        ne.noteId = note->note_id;
        ne.channel = note->channel;
        ne.key = note->key;
        ne.velocity = static_cast<float>(note->velocity);
        pendingNoteOffs_.push_back(ne);
      }
      break;
    }

    default:
      break;
    }
  }
}

void PluginInstance::applyParameterChanges() {
  if (!paramValues_)
    return;
  // Apply all parameter values to WASM
  for (size_t i = 0; i < paramCount_; ++i) {
    wasm_->setParam(manifest_.parameters[i].id, paramValues_[i].load());
  }
}

void PluginInstance::copyInputBuffers(const clap_process_t *process) {
  if (!process->audio_inputs || process->audio_inputs_count == 0)
    return;

  const auto &input = process->audio_inputs[0];
  uint32_t channels = std::min(static_cast<uint32_t>(manifest_.audio.inputs),
                               input.channel_count);

  for (uint32_t ch = 0; ch < channels; ++ch) {
    float *wasmBuf = wasm_->getInputBuffer(ch);
    if (wasmBuf && input.data32 && input.data32[ch]) {
      std::memcpy(wasmBuf, input.data32[ch],
                  process->frames_count * sizeof(float));
    }
  }
}

void PluginInstance::copyOutputBuffers(const clap_process_t *process) {
  if (!process->audio_outputs || process->audio_outputs_count == 0)
    return;

  auto &output = process->audio_outputs[0];
  uint32_t channels = std::min(static_cast<uint32_t>(manifest_.audio.outputs),
                               output.channel_count);

  for (uint32_t ch = 0; ch < channels; ++ch) {
    float *wasmBuf = wasm_->getOutputBuffer(ch);
    if (wasmBuf && output.data32 && output.data32[ch]) {
      std::memcpy(output.data32[ch], wasmBuf,
                  process->frames_count * sizeof(float));
    }
  }
}

clap_process_status PluginInstance::process(const clap_process_t *process) {
  if (!wasm_ || !activated_ || !processing_) {
    return CLAP_PROCESS_ERROR;
  }

  // Process input events (parameters, notes)
  processInputEvents(process->in_events);

  // Apply parameter changes
  applyParameterChanges();

  // Send note events to WASM (sorted by sample offset)
  // Note: For sample-accurate processing, we'd need to split the buffer
  // For now, we process all notes at the start of the block
  for (const auto &note : pendingNoteOns_) {
    wasm_->noteOn(note.sampleOffset, note.noteId, note.channel, note.key,
                  note.velocity);
  }
  for (const auto &note : pendingNoteOffs_) {
    wasm_->noteOff(note.sampleOffset, note.noteId, note.channel, note.key,
                   note.velocity);
  }

  // Copy input audio to WASM
  copyInputBuffers(process);

  // Process audio
  wasm_->process(static_cast<int>(process->frames_count));

  // Copy output audio from WASM
  copyOutputBuffers(process);

  return CLAP_PROCESS_CONTINUE;
}

bool PluginInstance::saveState(const clap_ostream_t *stream) {
  if (!wasm_)
    return false;

  // Get state from WASM
  int stateSize = wasm_->getStateSize();
  if (stateSize <= 0) {
    // No state - write empty marker
    uint32_t marker = 0;
    stream->write(stream, &marker, sizeof(marker));
    return true;
  }

  // Allocate buffer and get state
  std::vector<uint8_t> state(stateSize);
  wasm_->getState(state.data());

  // Write size and data
  uint32_t size = static_cast<uint32_t>(stateSize);
  if (stream->write(stream, &size, sizeof(size)) != sizeof(size)) {
    return false;
  }
  if (stream->write(stream, state.data(), size) != size) {
    return false;
  }

  return true;
}

bool PluginInstance::loadState(const clap_istream_t *stream) {
  if (!wasm_)
    return false;

  // Read size
  uint32_t size;
  if (stream->read(stream, &size, sizeof(size)) != sizeof(size)) {
    return false;
  }

  if (size == 0) {
    // Empty state
    return true;
  }

  // Read data
  std::vector<uint8_t> state(size);
  if (stream->read(stream, state.data(), size) != size) {
    return false;
  }

  // Apply state to WASM
  wasm_->setState(state.data(), size);

  // Update parameter values from WASM state
  if (paramValues_) {
    for (size_t i = 0; i < paramCount_; ++i) {
      float value = wasm_->getParam(manifest_.parameters[i].id);
      paramValues_[i].store(value);
    }
  }

  return true;
}

} // namespace clasp
