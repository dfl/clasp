#pragma once

#include "clasp/scanner.h"
#include <clasp-gui/webview.h>
#include <clap/clap.h>
#include <functional>
#include <memory>
#include <string>

namespace clasp {

// Forward declare the plugin instance
class PluginInstance;

// GUI wrapper using clasp-gui WebView
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

  // Bulk parameter sync (for preset loads)
  void syncAllParameters();

  // MIDI note notifications (can be called from audio thread)
  void queueNoteOn(int channel, int key, float velocity);
  void queueNoteOff(int channel, int key);

  // Thread-safe MIDI CC notifications
  void queueMidiCC(int channel, int cc, int value);

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

  // clasp-gui WebView
  std::unique_ptr<clasp_gui::WebView> webview_;

  ParamChangeCallback paramChangeCallback_;

  uint32_t width_;
  uint32_t height_;
  double scale_ = 1.0;
  bool visible_ = false;

  // Setup JS bindings for plugin control
  void setupBindings();
};

} // namespace clasp
