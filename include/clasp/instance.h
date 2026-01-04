#pragma once

#include "clasp/runtime.h"
#include "clasp/scanner.h"
#include <atomic>
#include <clap/clap.h>
#include <memory>
#include <string>
#include <vector>

namespace clasp {

// Note event for instruments
struct NoteEvent {
  int32_t sampleOffset;
  int16_t noteId;
  int16_t channel;
  int16_t key;
  float velocity;
};

// Plugin instance wrapping a WASM DSP module
class PluginInstance {
public:
  PluginInstance(const PluginManifest &manifest);
  ~PluginInstance();

  // CLAP lifecycle
  bool init();
  void destroy();
  bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames);
  void deactivate();
  bool startProcessing();
  void stopProcessing();
  void reset();

  // Audio processing
  clap_process_status process(const clap_process_t *process);

  // Parameters (thread-safe)
  void setParameterValue(clap_id paramId, double value);
  double getParameterValue(clap_id paramId) const;

  // State
  bool saveState(const clap_ostream_t *stream);
  bool loadState(const clap_istream_t *stream);

  // Info
  const PluginManifest &manifest() const { return manifest_; }
  bool isInstrument() const { return manifest_.isInstrument; }
  bool hasUi() const { return manifest_.ui.hasUi; }

private:
  PluginManifest manifest_;
  std::unique_ptr<WasmInstance> wasm_;

  // Processing state
  bool activated_ = false;
  bool processing_ = false;
  double sampleRate_ = 44100.0;
  uint32_t maxBlockSize_ = 512;

  // Lock-free parameter storage (unique_ptr array since atomics aren't
  // copyable)
  std::unique_ptr<std::atomic<float>[]> paramValues_;
  size_t paramCount_ = 0;

  // Note events queue (for instruments)
  std::vector<NoteEvent> pendingNoteOns_;
  std::vector<NoteEvent> pendingNoteOffs_;

  // Process input events from host
  void processInputEvents(const clap_input_events_t *events);

  // Copy audio buffers
  void copyInputBuffers(const clap_process_t *process);
  void copyOutputBuffers(const clap_process_t *process);

  // Apply parameter changes to WASM
  void applyParameterChanges();
};

} // namespace clasp
