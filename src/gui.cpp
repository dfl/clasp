#include "clasp/gui.h"
#include "clasp/instance.h"
#include <cstring>
#include <sstream>

// For JSON parsing of binding arguments
#if __has_include("choc/containers/choc_Value.h")
#include "choc/containers/choc_Value.h"
#include "choc/text/choc_JSON.h"
#define CLASP_HAS_CHOC_VALUE 1
#else
#define CLASP_HAS_CHOC_VALUE 0
#endif

namespace clasp {

Gui::Gui(PluginInstance *plugin, const PluginManifest &manifest)
    : plugin_(plugin), manifest_(manifest),
      width_(manifest.ui.width), height_(manifest.ui.height) {}

Gui::~Gui() { destroy(); }

bool Gui::isApiSupported(const char *api, bool isFloating) {
  if (!manifest_.ui.hasUi)
    return false;

  if (isFloating)
    return false;

  if (api == nullptr)
    return true;

  return clasp_gui::WebView::isApiSupported(
    api == nullptr ? clasp_gui::WindowApi::Unknown :
#if defined(__APPLE__)
    (strcmp(api, CLAP_WINDOW_API_COCOA) == 0 ? clasp_gui::WindowApi::Cocoa : clasp_gui::WindowApi::Unknown)
#elif defined(_WIN32)
    (strcmp(api, CLAP_WINDOW_API_WIN32) == 0 ? clasp_gui::WindowApi::Win32 : clasp_gui::WindowApi::Unknown)
#else
    (strcmp(api, CLAP_WINDOW_API_X11) == 0 ? clasp_gui::WindowApi::X11 : clasp_gui::WindowApi::Unknown)
#endif
  );
}

bool Gui::getPreferredApi(const char **api, bool *isFloating) {
  if (!clasp_gui::WebView::isAvailable())
    return false;

#if defined(__APPLE__)
  *api = CLAP_WINDOW_API_COCOA;
#elif defined(_WIN32)
  *api = CLAP_WINDOW_API_WIN32;
#else
  *api = CLAP_WINDOW_API_X11;
#endif
  *isFloating = false;
  return true;
}

bool Gui::create(const char *api, bool isFloating) {
  if (!isApiSupported(api, isFloating))
    return false;

  clasp_gui::WebViewOptions options;
  options.enableDebugMode = true;
  options.disableContextMenu = true;
  options.enablePointerCaptureFix = true;

  webview_ = std::make_unique<clasp_gui::WebView>(options);
  if (!webview_->create())
    return false;

  setupBindings();
  return true;
}

void Gui::setupBindings() {
  if (!webview_)
    return;

#if CLASP_HAS_CHOC_VALUE
  // Bind clasp_setParam(paramId, value)
  webview_->bind("clasp_setParam", [this](const std::string &argsJson) -> std::string {
    try {
      auto args = choc::json::parse(argsJson);
      if (args.size() >= 2) {
        int paramId = 0;
        float value = 0.0f;

        if (args[0].isInt32()) {
          paramId = args[0].get<int32_t>();
        } else if (args[0].isInt64()) {
          paramId = static_cast<int>(args[0].get<int64_t>());
        } else if (args[0].isFloat64()) {
          paramId = static_cast<int>(args[0].get<double>());
        }

        if (args[1].isFloat64()) {
          value = static_cast<float>(args[1].get<double>());
        } else if (args[1].isFloat32()) {
          value = args[1].get<float>();
        } else if (args[1].isInt32()) {
          value = static_cast<float>(args[1].get<int32_t>());
        } else if (args[1].isInt64()) {
          value = static_cast<float>(args[1].get<int64_t>());
        }

        if (paramChangeCallback_) {
          paramChangeCallback_(paramId, value);
        }
      }
    } catch (...) {
      // Silently ignore malformed arguments
    }
    return "";
  });

  // Bind clasp_getParam(paramId) -> value
  webview_->bind("clasp_getParam", [this](const std::string &argsJson) -> std::string {
    try {
      auto args = choc::json::parse(argsJson);
      if (args.size() >= 1) {
        int paramId = args[0].getInt32();
        double value = plugin_->getParameterValue(paramId);
        return std::to_string(value);
      }
    } catch (...) {}
    return "0";
  });

  // Bind clasp_getPluginInfo() -> {id, name, vendor, version, isInstrument, parameters}
  webview_->bind("clasp_getPluginInfo", [this](const std::string &) -> std::string {
    auto info = choc::value::createObject("PluginInfo");
    info.addMember("id", manifest_.id);
    info.addMember("name", manifest_.name);
    info.addMember("vendor", manifest_.vendor);
    info.addMember("version", manifest_.version);
    info.addMember("isInstrument", manifest_.isInstrument);

    auto params = choc::value::createEmptyArray();
    for (const auto &p : manifest_.parameters) {
      auto param = choc::value::createObject("Param");
      param.addMember("id", static_cast<int32_t>(p.id));
      param.addMember("name", p.name);
      param.addMember("min", static_cast<double>(p.min));
      param.addMember("max", static_cast<double>(p.max));
      param.addMember("default", static_cast<double>(p.defaultValue));
      params.addArrayElement(param);
    }
    info.addMember("parameters", params);

    return choc::json::toString(info);
  });
#endif

  // Inject additional clasp-specific API
  std::string initScript = R"(
    // Extend clasp object with plugin-specific helpers
    window.clasp.setParam = function(id, value) {
      return clasp_setParam(id, value);
    };
    window.clasp.getParam = function(id) {
      return clasp_getParam(id);
    };
    window.clasp.getPluginInfo = function() {
      return JSON.parse(clasp_getPluginInfo());
    };

    // MIDI Learn API (client-side state, plugin handles persistence)
    window.clasp.midiLearn = {
      active: false,
      target: null,
      start: function(paramId) {
        this.active = true;
        this.target = { paramId: paramId };
        console.log('MIDI Learn started for param ' + paramId);
      },
      stop: function() {
        this.active = false;
        this.target = null;
        console.log('MIDI Learn stopped');
      }
    };

    // Auto-learn on CC if learning
    var origOnMidiCC = window.clasp.onMidiCC;
    window.clasp.onMidiCC = function(channel, cc, value) {
      if (window.clasp.midiLearn.active && window.clasp.midiLearn.target) {
        // Would notify native to store mapping
        console.log('Learned CC ' + cc + ' ch ' + channel + ' -> param ' + window.clasp.midiLearn.target.paramId);
        window.clasp.midiLearn.stop();
      }
      if (origOnMidiCC) origOnMidiCC(channel, cc, value);
    };

    // Ready notification
    if (window.onClaspReady) {
      window.onClaspReady();
    }
  )";
  webview_->evaluateScript(initScript);
}

void Gui::destroy() {
  if (webview_) {
    webview_->destroy();
    webview_.reset();
  }
  visible_ = false;
}

bool Gui::setScale(double scale) {
  scale_ = scale;
  return true;
}

bool Gui::getSize(uint32_t *width, uint32_t *height) {
  *width = width_;
  *height = height_;
  return true;
}

bool Gui::canResize() {
  return true;
}

bool Gui::getResizeHints(clap_gui_resize_hints_t *hints) {
  hints->can_resize_horizontally = true;
  hints->can_resize_vertically = true;
  hints->preserve_aspect_ratio = false;
  hints->aspect_ratio_width = 1;
  hints->aspect_ratio_height = 1;
  return true;
}

bool Gui::adjustSize(uint32_t *width, uint32_t *height) {
  return true;
}

bool Gui::setSize(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;

  if (webview_) {
    webview_->setSize(width, height);
  }

  return true;
}

bool Gui::setParent(const clap_window_t *window) {
  if (!webview_ || !window)
    return false;

  clasp_gui::NativeWindow native;

#if defined(__APPLE__)
  if (window->api && strcmp(window->api, CLAP_WINDOW_API_COCOA) == 0) {
    native.api = clasp_gui::WindowApi::Cocoa;
    native.handle = window->cocoa;
  }
#elif defined(_WIN32)
  if (window->api && strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0) {
    native.api = clasp_gui::WindowApi::Win32;
    native.handle = window->win32;
  }
#else
  if (window->api && strcmp(window->api, CLAP_WINDOW_API_X11) == 0) {
    native.api = clasp_gui::WindowApi::X11;
    native.handle = reinterpret_cast<void *>(window->x11);
  }
#endif

  if (!native.handle)
    return false;

  webview_->setParent(native);
  webview_->setSize(width_, height_);

  // Navigate to the UI HTML
  if (!manifest_.ui.entry.empty()) {
    std::string url = "file://" + manifest_.ui.entry;
    webview_->navigate(url);
  }

  return true;
}

bool Gui::setTransient(const clap_window_t *window) {
  return false;
}

void Gui::suggestTitle(const char *title) {
  // Not applicable for embedded view
}

bool Gui::show() {
  visible_ = true;
  if (webview_) {
    return webview_->show();
  }
  return false;
}

bool Gui::hide() {
  visible_ = false;
  if (webview_) {
    return webview_->hide();
  }
  return false;
}

void Gui::notifyParameterChanged(int paramId, float value) {
  if (webview_ && visible_) {
    std::ostringstream js;
    js << "if (window.clasp && window.clasp.onParamChange) { "
       << "window.clasp.onParamChange(" << paramId << ", " << value << "); }";
    webview_->evaluateScript(js.str());
  }
}

void Gui::queueParameterUpdate(int paramId, float value) {
  if (webview_) {
    webview_->queueParamUpdate(paramId, value);
  }
}

void Gui::syncAllParameters() {
  if (!webview_ || !plugin_)
    return;

  std::vector<std::pair<int, float>> params;
  for (const auto &p : manifest_.parameters) {
    float value = static_cast<float>(plugin_->getParameterValue(p.id));
    params.emplace_back(static_cast<int>(p.id), value);
  }
  webview_->queueBulkParamUpdate(params);
}

void Gui::queueNoteOn(int channel, int key, float velocity) {
  if (webview_) {
    webview_->queueNoteOn(channel, key, velocity);
  }
}

void Gui::queueNoteOff(int channel, int key) {
  if (webview_) {
    webview_->queueNoteOff(channel, key);
  }
}

void Gui::queueMidiCC(int channel, int cc, int value) {
  if (webview_) {
    webview_->queueMidiCC(channel, cc, value);
  }
}

void Gui::processQueuedUpdates() {
  if (webview_ && visible_) {
    webview_->processQueuedUpdates();
  }
}

void Gui::setParamChangeCallback(ParamChangeCallback callback) {
  paramChangeCallback_ = std::move(callback);
}

} // namespace clasp
