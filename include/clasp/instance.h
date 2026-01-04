#pragma once

#include "clasp/runtime.h"
#include "clasp/scanner.h"
#include "clasp/wclap_runtime.h"
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

struct ExpressionEvent {
  int32_t sampleOffset;
  int16_t noteId;
  int16_t channel;
  int16_t key;
  int32_t expressionId;
  float value;
};

// Abstract base for plugin instances (both .clasp and .wclap)
class IPluginInstance {
public:
  virtual ~IPluginInstance() = default;

  // CLAP lifecycle
  virtual bool init() = 0;
  virtual void destroy() = 0;
  virtual bool activate(double sampleRate, uint32_t minFrames,
                        uint32_t maxFrames) = 0;
  virtual void deactivate() = 0;
  virtual bool startProcessing() = 0;
  virtual void stopProcessing() = 0;
  virtual void reset() = 0;

  // Audio processing
  virtual clap_process_status process(const clap_process_t *process) = 0;

  // Parameters (thread-safe)
  virtual void setParameterValue(clap_id paramId, double value) = 0;
  virtual double getParameterValue(clap_id paramId) const = 0;

  // State
  virtual bool saveState(const clap_ostream_t *stream) = 0;
  virtual bool loadState(const clap_istream_t *stream) = 0;

  // UI messages
  virtual void onMessage(const void *buffer, uint32_t size) = 0;

  // Info
  virtual const PluginManifest &manifest() const = 0;
  virtual bool isInstrument() const = 0;
  virtual bool hasUi() const = 0;
  virtual uint32_t latency() const = 0;
  virtual uint32_t tail() const = 0;
};

// Factory to create appropriate instance type based on bundle format
std::unique_ptr<IPluginInstance> createPluginInstance(const PluginManifest &manifest);

// Plugin instance wrapping a WASM DSP module (.clasp bundles - legacy format)
class PluginInstance : public IPluginInstance {
public:
  PluginInstance(const PluginManifest &manifest);
  ~PluginInstance() override;

  // CLAP lifecycle
  bool init() override;
  void destroy() override;
  bool activate(double sampleRate, uint32_t minFrames,
                uint32_t maxFrames) override;
  void deactivate() override;
  bool startProcessing() override;
  void stopProcessing() override;
  void reset() override;

  // Audio processing
  clap_process_status process(const clap_process_t *process) override;

  // Parameters (thread-safe)
  void setParameterValue(clap_id paramId, double value) override;
  double getParameterValue(clap_id paramId) const override;

  // State
  bool saveState(const clap_ostream_t *stream) override;
  bool loadState(const clap_istream_t *stream) override;

  // UI messages
  void onMessage(const void *buffer, uint32_t size) override;

  // Info
  const PluginManifest &manifest() const override { return manifest_; }
  bool isInstrument() const override { return manifest_.isInstrument; }
  bool hasUi() const override { return manifest_.ui.hasUi; }
  uint32_t latency() const override { return manifest_.audio.latency; }
  uint32_t tail() const override { return manifest_.audio.tailSize; }

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
  std::vector<ExpressionEvent> pendingExpressions_;

  // Process input events from host
  void processInputEvents(const clap_input_events_t *events);

  // Copy audio buffers
  void copyInputBuffers(const clap_process_t *process);
  void copyOutputBuffers(const clap_process_t *process);

  // Apply parameter changes to WASM
  void applyParameterChanges();
};

// Plugin instance wrapping a full WCLAP plugin (.wclap bundles)
class WclapPluginInstance : public IPluginInstance {
public:
  WclapPluginInstance(const PluginManifest &manifest);
  ~WclapPluginInstance() override;

  // CLAP lifecycle
  bool init() override;
  void destroy() override;
  bool activate(double sampleRate, uint32_t minFrames,
                uint32_t maxFrames) override;
  void deactivate() override;
  bool startProcessing() override;
  void stopProcessing() override;
  void reset() override;

  // Audio processing
  clap_process_status process(const clap_process_t *process) override;

  // Parameters (thread-safe)
  void setParameterValue(clap_id paramId, double value) override;
  double getParameterValue(clap_id paramId) const override;

  // State
  bool saveState(const clap_ostream_t *stream) override;
  bool loadState(const clap_istream_t *stream) override;

  // UI messages
  void onMessage(const void *buffer, uint32_t size) override;

  // Info
  const PluginManifest &manifest() const override { return manifest_; }
  bool isInstrument() const override { return manifest_.isInstrument; }
  bool hasUi() const override { return manifest_.ui.hasUi; }
  uint32_t latency() const override { return 0; } // TODO: query from WASM
  uint32_t tail() const override { return 0; }    // TODO: query from WASM

private:
  PluginManifest manifest_;
  std::unique_ptr<wclap::Instance<WasmtimeInstance>> wasm_;

  // WASM pointers to CLAP plugin interface
  uint64_t pluginPtr_ = 0;      // clap_plugin_t* in WASM
  uint64_t processPtr_ = 0;     // clap_process_t* in WASM (reused)

  // Processing state
  bool activated_ = false;
  bool processing_ = false;
  double sampleRate_ = 44100.0;
  uint32_t maxBlockSize_ = 512;

  // Parameter cache
  std::unique_ptr<std::atomic<float>[]> paramValues_;
  size_t paramCount_ = 0;
};

} // namespace clasp
