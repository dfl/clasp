#include "clasp/gui.h"
#include "clasp/instance.h"
#include "clasp/logger.h"
#include "clasp/scanner.h"
#include <algorithm>
#include <clap/clap.h>
#include <clap/ext/draft/webview.h>
#include <clap/ext/log.h>
#include <clap/helpers/host-proxy.hh>
#include <clap/helpers/plugin.hh>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace clasp {

// Global scanner and discovered plugins
static Scanner g_scanner;
static std::vector<PluginManifest> g_manifests;
static std::vector<clap_plugin_descriptor_t> g_descriptors;
static bool g_scanned = false;

// Expose scanner for adding paths from entry point
Scanner &getGlobalScanner() { return g_scanner; }

// Helper to append suffix to plugin name
static std::string formatPluginName(const std::string &name) {
  return name + " (thunder)";
}

// Helper to generate stable plugin ID
static std::string formatPluginId(const std::string &id) {
  return "thunder:" + id;
}

// Scan for plugins if not already done
static void ensureScanned() {
  if (g_scanned)
    return;

  g_manifests = g_scanner.scan();
  g_descriptors.clear();
  g_descriptors.reserve(g_manifests.size());

  for (auto &manifest : g_manifests) {
    clap_plugin_descriptor_t desc = {};
    desc.clap_version = CLAP_VERSION;

    // Store formatted strings (they need to persist)
    static std::vector<std::string> persistentStrings;
    persistentStrings.push_back(formatPluginId(manifest.id));
    persistentStrings.push_back(formatPluginName(manifest.name));
    persistentStrings.push_back(manifest.vendor);
    persistentStrings.push_back(manifest.version);

    desc.id = persistentStrings[persistentStrings.size() - 4].c_str();
    desc.name = persistentStrings[persistentStrings.size() - 3].c_str();
    desc.vendor = persistentStrings[persistentStrings.size() - 2].c_str();
    desc.version = persistentStrings[persistentStrings.size() - 1].c_str();

    desc.description = "WCLAP plugin hosted by thunder";
    desc.url = "";
    desc.manual_url = "";
    desc.support_url = "";

    // Features based on plugin type
    static const char *effectFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
                                           nullptr};
    static const char *instrumentFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT,
                                               CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                               nullptr};

    desc.features = manifest.isInstrument ? instrumentFeatures : effectFeatures;

    g_descriptors.push_back(desc);
  }

  g_scanned = true;
}

// Factory functions
static uint32_t factory_get_plugin_count(const clap_plugin_factory_t *factory) {
  ensureScanned();
  return static_cast<uint32_t>(g_descriptors.size());
}

static const clap_plugin_descriptor_t *
factory_get_plugin_descriptor(const clap_plugin_factory_t *factory,
                              uint32_t index) {
  ensureScanned();
  if (index >= g_descriptors.size()) {
    return nullptr;
  }
  return &g_descriptors[index];
}

// Forward declare the plugin wrapper class
class ThunderPlugin;

// Plugin wrapper that implements CLAP interface
class ThunderPlugin {
public:
  ThunderPlugin(const clap_host_t *host, const PluginManifest &manifest)
      : host_(host), manifest_(manifest) {
    instance_ = createPluginInstance(manifest);
    // Cache host params extension for notifying parameter changes
    if (host_) {
      hostParams_ = static_cast<const clap_host_params_t *>(
          host_->get_extension(host_, CLAP_EXT_PARAMS));
    }
  }

  // CLAP plugin interface
  bool init() { return instance_ && instance_->init(); }
  void destroy() {
    if (instance_)
      instance_->destroy();
  }

  bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
    return instance_ && instance_->activate(sampleRate, minFrames, maxFrames);
  }

  void deactivate() {
    if (instance_)
      instance_->deactivate();
  }

  bool startProcessing() { return instance_ && instance_->startProcessing(); }
  void stopProcessing() {
    if (instance_)
      instance_->stopProcessing();
  }
  void reset() {
    if (instance_)
      instance_->reset();
  }

  clap_process_status process(const clap_process_t *process) {
    bool guiUpdateQueued = false;

    // Scan events for GUI notification
    if (gui_ && process->in_events) {
      uint32_t size = process->in_events->size(process->in_events);
      for (uint32_t i = 0; i < size; ++i) {
        auto event = process->in_events->get(process->in_events, i);
        if (event->type == CLAP_EVENT_NOTE_ON) {
          auto note = reinterpret_cast<const clap_event_note_t *>(event);
          gui_->queueNoteOn(note->channel, note->key,
                            static_cast<float>(note->velocity));
          guiUpdateQueued = true;
        } else if (event->type == CLAP_EVENT_NOTE_OFF) {
          auto note = reinterpret_cast<const clap_event_note_t *>(event);
          gui_->queueNoteOff(note->channel, note->key);
          guiUpdateQueued = true;
        } else if (event->type == CLAP_EVENT_MIDI) {
          auto midi = reinterpret_cast<const clap_event_midi_t *>(event);
          if ((midi->data[0] & 0xF0) == 0xB0) { // CC
            gui_->queueMidiCC(midi->data[0] & 0x0F, midi->data[1],
                              midi->data[2]);
            guiUpdateQueued = true;
          }
        } else if (event->type == CLAP_EVENT_PARAM_VALUE) {
          auto pv = reinterpret_cast<const clap_event_param_value_t *>(event);
          gui_->queueParameterUpdate(static_cast<int>(pv->param_id),
                                     static_cast<float>(pv->value));
          guiUpdateQueued = true;
        }
      }
    }

    if (!instance_)
      return CLAP_PROCESS_ERROR;
    auto status = instance_->process(process);

    if (guiUpdateQueued && host_) {
      host_->request_callback(host_);
    }

    return status;
  }

  // Parameters extension
  uint32_t paramsCount() const {
    return static_cast<uint32_t>(manifest_.parameters.size());
  }

  bool paramsGetInfo(uint32_t index, clap_param_info_t *info) const {
    if (index >= manifest_.parameters.size())
      return false;

    const auto &param = manifest_.parameters[index];
    info->id = param.id;
    strncpy(info->name, param.name.c_str(), CLAP_NAME_SIZE - 1);
    info->name[CLAP_NAME_SIZE - 1] = '\0';
    info->module[0] = '\0';
    info->min_value = param.min;
    info->max_value = param.max;
    info->default_value = param.defaultValue;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    info->cookie = nullptr;

    return true;
  }

  bool paramsGetValue(clap_id paramId, double *value) const {
    if (!instance_)
      return false;
    *value = instance_->getParameterValue(paramId);
    return true;
  }

  bool paramsValueToText(clap_id paramId, double value, char *display,
                         uint32_t size) {
    snprintf(display, size, "%.2f", value);
    return true;
  }

  bool paramsTextToValue(clap_id paramId, const char *text, double *value) {
    *value = std::stod(text);
    return true;
  }

  void paramsFlush(const clap_input_events_t *in,
                   const clap_output_events_t *out) {
    // Process parameter events
    for (uint32_t i = 0; i < in->size(in); ++i) {
      auto event = in->get(in, i);
      if (event->type == CLAP_EVENT_PARAM_VALUE) {
        auto pv = reinterpret_cast<const clap_event_param_value_t *>(event);
        if (instance_)
          instance_->setParameterValue(pv->param_id, pv->value);

        // Queue GUI update (thread-safe, will be processed on main thread)
        if (gui_) {
          gui_->queueParameterUpdate(static_cast<int>(pv->param_id),
                                     static_cast<float>(pv->value));
          if (host_) {
            host_->request_callback(host_);
          }
        }
      }
    }
  }

  // State extension
  bool stateSave(const clap_ostream_t *stream) {
    return instance_ && instance_->saveState(stream);
  }

  bool stateLoad(const clap_istream_t *stream) {
    if (!instance_)
      return false;
    bool result = instance_->loadState(stream);

    // Notify GUI of all parameter values after loading state
    if (result && gui_) {
      for (const auto &param : manifest_.parameters) {
        float value =
            static_cast<float>(instance_->getParameterValue(param.id));
        gui_->notifyParameterChanged(param.id, value);
      }
    }

    return result;
  }

  // Audio ports extension
  uint32_t audioPortsCount(bool isInput) const {
    if (isInput) {
      return manifest_.audio.inputs > 0 ? 1 : 0;
    }
    return manifest_.audio.outputs > 0 ? 1 : 0;
  }

  bool audioPortsGet(uint32_t index, bool isInput,
                     clap_audio_port_info_t *info) const {
    if (index != 0)
      return false;

    info->id = isInput ? 0 : 1;
    strncpy(info->name, isInput ? "Input" : "Output", CLAP_NAME_SIZE);
    info->channel_count =
        isInput ? manifest_.audio.inputs : manifest_.audio.outputs;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type =
        info->channel_count == 2 ? CLAP_PORT_STEREO : CLAP_PORT_MONO;
    info->in_place_pair = CLAP_INVALID_ID;

    return true;
  }

  // Note ports extension (for instruments)
  uint32_t notePortsCount(bool isInput) const {
    if (!manifest_.isInstrument)
      return 0;
    return isInput ? 1 : 0;
  }

  bool notePortsGet(uint32_t index, bool isInput,
                    clap_note_port_info_t *info) const {
    if (!manifest_.isInstrument || index != 0 || !isInput)
      return false;

    info->id = 0;
    strncpy(info->name, "Note Input", CLAP_NAME_SIZE);
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;

    return true;
  }

  uint32_t latency() const { return manifest_.audio.latency; }
  uint32_t tail() const { return manifest_.audio.tailSize; }

  // GUI extension
  bool guiIsApiSupported(const char *api, bool isFloating) {
    if (!manifest_.ui.hasUi)
      return false;
    if (!gui_) {
      gui_ = std::make_unique<Gui>(instance_.get(), manifest_);
    }
    return gui_->isApiSupported(api, isFloating);
  }

  bool guiCreate(const char *api, bool isFloating) {
    if (!gui_)
      return false;

    // Wire up callback for parameter changes from UI
    gui_->setParamChangeCallback([this](int paramId, float value) {
      // Update the internal parameter value
      if (instance_)
        instance_->setParameterValue(static_cast<clap_id>(paramId), value);

      // Notify the host that a parameter changed (if host supports it)
      if (hostParams_ && host_) {
        hostParams_->rescan(host_, CLAP_PARAM_RESCAN_VALUES);
      }
    });

    return gui_->create(api, isFloating);
  }

  void guiDestroy() {
    if (gui_)
      gui_->destroy();
  }

  bool guiSetScale(double scale) {
    if (!gui_)
      return false;
    return gui_->setScale(scale);
  }

  bool guiGetSize(uint32_t *width, uint32_t *height) {
    if (!gui_)
      return false;
    return gui_->getSize(width, height);
  }

  bool guiCanResize() { return gui_ && gui_->canResize(); }

  bool guiGetResizeHints(clap_gui_resize_hints_t *hints) {
    if (!gui_)
      return false;
    return gui_->getResizeHints(hints);
  }

  bool guiAdjustSize(uint32_t *width, uint32_t *height) {
    if (!gui_)
      return false;
    return gui_->adjustSize(width, height);
  }

  // Webview extension (draft)
  int32_t webviewGetUri(char *uri, uint32_t uriCapacity) {
    std::string fullUri;
    if (manifest_.ui.entry.find("://") != std::string::npos) {
      fullUri = manifest_.ui.entry;
    } else {
      // Relative path to bundle-defined resource root
      // Returning a path starting with / tells the host to use get_resource()
      fullUri = "/" + manifest_.ui.entry;
    }

    if (uriCapacity == 0)
      return static_cast<int32_t>(fullUri.length() + 1);

    strncpy(uri, fullUri.c_str(), uriCapacity - 1);
    uri[uriCapacity - 1] = '\0';
    return static_cast<int32_t>(
        std::min<size_t>(fullUri.length() + 1, uriCapacity));
  }

  bool webviewGetResource(const char *path, char *mime, uint32_t mimeCapacity,
                          const clap_ostream_t *stream) {
    // Basic resource loading from bundle ui/ directory
    std::string fullPath = manifest_.bundlePath + "/ui" + path;
    std::ifstream file(fullPath, std::ios::binary);
    if (!file)
      return false;

    // Detect MIME type (very basic)
    std::string sPath(path);
    std::string type = "application/octet-stream";
    if (sPath.ends_with(".html"))
      type = "text/html";
    else if (sPath.ends_with(".js"))
      type = "text/javascript";
    else if (sPath.ends_with(".css"))
      type = "text/css";
    else if (sPath.ends_with(".png"))
      type = "image/png";
    else if (sPath.ends_with(".wasm"))
      type = "application/wasm";

    strncpy(mime, type.c_str(), mimeCapacity - 1);
    mime[mimeCapacity - 1] = '\0';

    // Stream the data
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
      int64_t written = stream->write(stream, buffer, file.gcount());
      if (written != file.gcount())
        return false;
    }

    return true;
  }

  bool webviewReceive(const void *buffer, uint32_t size) {
    // TODO: Implement binary message routing to WASM instance
    return true;
  }

  bool guiSetSize(uint32_t width, uint32_t height) {
    if (!gui_)
      return false;
    return gui_->setSize(width, height);
  }

  bool guiSetParent(const clap_window_t *window) {
    if (!gui_)
      return false;
    return gui_->setParent(window);
  }

  bool guiShow() {
    if (!gui_)
      return false;
    return gui_->show();
  }

  bool guiHide() {
    if (!gui_)
      return false;
    return gui_->hide();
  }

  bool guiSetTransient(const clap_window_t *window) {
    if (!gui_)
      return false;
    return gui_->setTransient(window);
  }

  void guiSuggestTitle(const char *title) {
    if (gui_)
      gui_->suggestTitle(title);
  }

  // Process queued GUI updates (called from main thread)
  void processGuiUpdates() {
    if (gui_) {
      gui_->processQueuedUpdates();
    }
  }

private:
  const clap_host_t *host_;
  const clap_host_params_t *hostParams_ = nullptr;
  PluginManifest manifest_;
  std::unique_ptr<IPluginInstance> instance_;
  std::unique_ptr<Gui> gui_;
};

// C wrapper for clap_plugin
struct PluginWrapper {
  clap_plugin_t plugin;
  ThunderPlugin impl;

  PluginWrapper(const clap_host_t *host, const clap_plugin_descriptor_t *desc,
                const PluginManifest &manifest)
      : impl(host, manifest) {
    plugin.desc = desc;
    plugin.plugin_data = this;

    // Plugin methods
    plugin.init = [](const clap_plugin_t *p) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.init();
    };
    plugin.destroy = [](const clap_plugin_t *p) {
      auto wrapper = static_cast<PluginWrapper *>(p->plugin_data);
      wrapper->impl.destroy();
      delete wrapper;
    };
    plugin.activate = [](const clap_plugin_t *p, double sr, uint32_t minF,
                         uint32_t maxF) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.activate(sr, minF, maxF);
    };
    plugin.deactivate = [](const clap_plugin_t *p) {
      static_cast<PluginWrapper *>(p->plugin_data)->impl.deactivate();
    };
    plugin.start_processing = [](const clap_plugin_t *p) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.startProcessing();
    };
    plugin.stop_processing = [](const clap_plugin_t *p) {
      static_cast<PluginWrapper *>(p->plugin_data)->impl.stopProcessing();
    };
    plugin.reset = [](const clap_plugin_t *p) {
      static_cast<PluginWrapper *>(p->plugin_data)->impl.reset();
    };
    plugin.process = [](const clap_plugin_t *p,
                        const clap_process_t *proc) -> clap_process_status {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.process(proc);
    };
    plugin.get_extension = getExtension;
    plugin.on_main_thread = [](const clap_plugin_t *p) {
      // Process any queued GUI updates on the main thread
      static_cast<PluginWrapper *>(p->plugin_data)->impl.processGuiUpdates();
    };
  }

  static const void *getExtension(const clap_plugin_t *p, const char *id);
};

// Extension implementations
static const clap_plugin_params_t paramsExtension = {
    .count = [](const clap_plugin_t *p) -> uint32_t {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.paramsCount();
    },
    .get_info = [](const clap_plugin_t *p, uint32_t idx,
                   clap_param_info_t *info) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.paramsGetInfo(idx, info);
    },
    .get_value = [](const clap_plugin_t *p, clap_id id, double *val) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.paramsGetValue(id, val);
    },
    .value_to_text = [](const clap_plugin_t *p, clap_id id, double val,
                        char *txt, uint32_t sz) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.paramsValueToText(id, val, txt, sz);
    },
    .text_to_value = [](const clap_plugin_t *p, clap_id id, const char *txt,
                        double *val) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.paramsTextToValue(id, txt, val);
    },
    .flush =
        [](const clap_plugin_t *p, const clap_input_events_t *in,
           const clap_output_events_t *out) {
          static_cast<PluginWrapper *>(p->plugin_data)
              ->impl.paramsFlush(in, out);
        },
};

static const clap_plugin_state_t stateExtension = {
    .save = [](const clap_plugin_t *p, const clap_ostream_t *s) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.stateSave(s);
    },
    .load = [](const clap_plugin_t *p, const clap_istream_t *s) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.stateLoad(s);
    },
};

static const clap_plugin_audio_ports_t audioPortsExtension = {
    .count = [](const clap_plugin_t *p, bool isInput) -> uint32_t {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.audioPortsCount(isInput);
    },
    .get = [](const clap_plugin_t *p, uint32_t idx, bool isInput,
              clap_audio_port_info_t *info) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.audioPortsGet(idx, isInput, info);
    },
};

static const clap_plugin_note_ports_t notePortsExtension = {
    .count = [](const clap_plugin_t *p, bool isInput) -> uint32_t {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.notePortsCount(isInput);
    },
    .get = [](const clap_plugin_t *p, uint32_t idx, bool isInput,
              clap_note_port_info_t *info) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.notePortsGet(idx, isInput, info);
    },
};

static const clap_plugin_gui_t guiExtension = {
    .is_api_supported = [](const clap_plugin_t *p, const char *api,
                           bool floating) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiIsApiSupported(api, floating);
    },
    .get_preferred_api = [](const clap_plugin_t *p, const char **api,
                            bool *floating) -> bool {
#if defined(__APPLE__)
      *api = CLAP_WINDOW_API_COCOA;
#elif defined(_WIN32)
      *api = CLAP_WINDOW_API_WIN32;
#else
      *api = CLAP_WINDOW_API_X11;
#endif
      *floating = false;
      return true;
    },
    .create = [](const clap_plugin_t *p, const char *api,
                 bool floating) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiCreate(api, floating);
    },
    .destroy =
        [](const clap_plugin_t *p) {
          static_cast<PluginWrapper *>(p->plugin_data)->impl.guiDestroy();
        },
    .set_scale = [](const clap_plugin_t *p, double scale) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiSetScale(scale);
    },
    .get_size = [](const clap_plugin_t *p, uint32_t *w, uint32_t *h) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiGetSize(w, h);
    },
    .can_resize = [](const clap_plugin_t *p) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.guiCanResize();
    },
    .get_resize_hints = [](const clap_plugin_t *p,
                           clap_gui_resize_hints_t *hints) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiGetResizeHints(hints);
    },
    .adjust_size = [](const clap_plugin_t *p, uint32_t *w,
                      uint32_t *h) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiAdjustSize(w, h);
    },
    .set_size = [](const clap_plugin_t *p, uint32_t w, uint32_t h) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiSetSize(w, h);
    },
    .set_parent = [](const clap_plugin_t *p, const clap_window_t *win) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiSetParent(win);
    },
    .set_transient = [](const clap_plugin_t *p,
                        const clap_window_t *win) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.guiSetTransient(win);
    },
    .suggest_title =
        [](const clap_plugin_t *p, const char *title) {
          static_cast<PluginWrapper *>(p->plugin_data)
              ->impl.guiSuggestTitle(title);
        },
    .show = [](const clap_plugin_t *p) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.guiShow();
    },
    .hide = [](const clap_plugin_t *p) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.guiHide();
    },
};

static const clap_plugin_webview_t webviewExtension = {
    .get_uri = [](const clap_plugin_t *p, char *uri,
                  uint32_t uri_capacity) -> int32_t {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.webviewGetUri(uri, uri_capacity);
    },
    .get_resource = [](const clap_plugin_t *p, const char *path, char *mime,
                       uint32_t mime_capacity,
                       const clap_ostream_t *stream) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.webviewGetResource(path, mime, mime_capacity, stream);
    },
    .receive = [](const clap_plugin_t *p, const void *buffer,
                  uint32_t size) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.webviewReceive(buffer, size);
    },
};

const void *PluginWrapper::getExtension(const clap_plugin_t *p,
                                        const char *id) {
  if (strcmp(id, CLAP_EXT_PARAMS) == 0)
    return &paramsExtension;
  if (strcmp(id, CLAP_EXT_STATE) == 0)
    return &stateExtension;
  if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0)
    return &audioPortsExtension;
  if (strcmp(id, CLAP_EXT_NOTE_PORTS) == 0)
    return &notePortsExtension;
  if (strcmp(id, CLAP_EXT_GUI) == 0)
    return &guiExtension;
  if (strcmp(id, CLAP_EXT_WEBVIEW) == 0)
    return &webviewExtension;
  if (strcmp(id, CLAP_EXT_LATENCY) == 0) {
    static const clap_plugin_latency_t latencyExt = {
        .get = [](const clap_plugin_t *p) -> uint32_t {
          return static_cast<PluginWrapper *>(p->plugin_data)->impl.latency();
        },
    };
    return &latencyExt;
  }
  if (strcmp(id, CLAP_EXT_TAIL) == 0) {
    static const clap_plugin_tail_t tailExt = {
        .get = [](const clap_plugin_t *p) -> uint32_t {
          return static_cast<PluginWrapper *>(p->plugin_data)->impl.tail();
        },
    };
    return &tailExt;
  }
  if (strcmp(id, CLAP_EXT_LOG) == 0) {
    static const clap_host_log_t logExt = {
        .log = [](const clap_host_t *host, clap_log_severity severity,
                  const char *msg) { Logger::instance().log(severity, msg); },
    };
    return &logExt;
  }
  return nullptr;
}

// Factory create plugin
static const clap_plugin_t *
factory_create_plugin(const clap_plugin_factory_t *factory,
                      const clap_host_t *host, const char *pluginId) {

  ensureScanned();

  // Find matching descriptor and manifest
  for (size_t i = 0; i < g_descriptors.size(); ++i) {
    if (strcmp(g_descriptors[i].id, pluginId) == 0) {
      auto wrapper = new PluginWrapper(host, &g_descriptors[i], g_manifests[i]);
      return &wrapper->plugin;
    }
  }

  return nullptr;
}

// The factory
const clap_plugin_factory_t pluginFactory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin,
};

} // namespace clasp

const clap_plugin_factory_t *getPluginFactory() {
  return &clasp::pluginFactory;
}
