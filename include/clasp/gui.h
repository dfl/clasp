#pragma once

#include "clasp/scanner.h"
#include <array>
#include <atomic>
#include <chrono>
#include <clap/clap.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace clasp {

// Forward declare the plugin instance
class PluginInstance;

// GUI wrapper using CHOC WebView
class Gui {
public:
  Gui(PluginInstance *plugin, const PluginManifest &manifest);
  ~Gui();

  // CLAP GUI extension interface
  bool isApiSupported(const char *api, bool isFloating);
  bool getPreferredApi(const char **api, bool *isFloating);
  bool create(const char *api, bool isFloating);
  void destroy();
  bool setScale(double scale);
  bool getSize(uint32_t *width, uint32_t *height);
  bool canResize();
  bool getResizeHints(clap_gui_resize_hints_t *hints);
  bool adjustSize(uint32_t *width, uint32_t *height);
  bool setSize(uint32_t width, uint32_t height);
  bool setParent(const clap_window_t *window);
  bool setTransient(const clap_window_t *window);
  void suggestTitle(const char *title);
  bool show();
  bool hide();

  // Thread-safe parameter updates (can be called from audio thread)
  void queueParameterUpdate(int paramId, float value);

  // MIDI note notifications (can be called from audio thread)
  void queueNoteOn(int channel, int key, float velocity);
  void queueNoteOff(int channel, int key);

  // Process queued updates on main thread (call from on_main_thread)
  void processQueuedUpdates();

  // Legacy direct notification (use queueParameterUpdate instead)
  void notifyParameterChanged(int paramId, float value);

  // Callback for parameter changes from UI
  using ParamChangeCallback = std::function<void(int paramId, float value)>;
  void setParamChangeCallback(ParamChangeCallback callback);

private:
  PluginInstance *plugin_;
  PluginManifest manifest_;

  // Platform-specific implementation (using CHOC)
  struct Impl;
  std::unique_ptr<Impl> impl_;

  ParamChangeCallback paramChangeCallback_;

  uint32_t width_;
  uint32_t height_;
  double scale_ = 1.0;
  bool visible_ = false;

  // Thread-safe update queues
  struct ParamUpdate {
    int id;
    float value;
  };
  struct NoteEvent {
    int channel;
    int key;
    float velocity;
    bool isNoteOn;
  };

  std::mutex updateMutex_;
  std::vector<ParamUpdate> pendingParams_;
  std::vector<NoteEvent> pendingNotes_;

  // Throttling (max 60 updates per second per parameter)
  static constexpr int MAX_PARAMS = 256;
  std::array<std::chrono::steady_clock::time_point, MAX_PARAMS>
      lastParamUpdate_;
  static constexpr auto UPDATE_INTERVAL =
      std::chrono::milliseconds(16); // ~60Hz
};

} // namespace clasp
