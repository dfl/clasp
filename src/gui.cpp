#include "clasp/gui.h"
#include "clasp/instance.h"
#include <iostream>

// CHOC WebView - single header library
#if __has_include("choc/gui/choc_WebView.h")
#include "choc/gui/choc_WebView.h"
#define CLASP_HAS_WEBVIEW 1
#else
#define CLASP_HAS_WEBVIEW 0
#endif

// CHOC Value for JSON
#include "choc/containers/choc_Value.h"
#include "choc/text/choc_JSON.h"

namespace clasp {

#if CLASP_HAS_WEBVIEW

struct Gui::Impl {
    std::unique_ptr<choc::ui::WebView> webview;
    void* parentWindow = nullptr;

    Impl() = default;
    ~Impl() = default;
};

#else

// Stub implementation when WebView is not available
struct Gui::Impl {
    void* parentWindow = nullptr;
};

#endif

Gui::Gui(PluginInstance* plugin, const PluginManifest& manifest)
    : plugin_(plugin)
    , manifest_(manifest)
    , impl_(std::make_unique<Impl>())
    , width_(manifest.ui.width)
    , height_(manifest.ui.height) {
}

Gui::~Gui() {
    destroy();
}

bool Gui::isApiSupported(const char* api, bool isFloating) {
    if (!manifest_.ui.hasUi) return false;

#if CLASP_HAS_WEBVIEW
    // We don't support floating windows for now
    if (isFloating) return false;

    if (api == nullptr) return true;  // Just checking if we have GUI at all

#if defined(__APPLE__)
    return strcmp(api, CLAP_WINDOW_API_COCOA) == 0;
#elif defined(_WIN32)
    return strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
#else
    return strcmp(api, CLAP_WINDOW_API_X11) == 0;
#endif

#else
    return false;
#endif
}

bool Gui::getPreferredApi(const char** api, bool* isFloating) {
#if CLASP_HAS_WEBVIEW
#if defined(__APPLE__)
    *api = CLAP_WINDOW_API_COCOA;
#elif defined(_WIN32)
    *api = CLAP_WINDOW_API_WIN32;
#else
    *api = CLAP_WINDOW_API_X11;
#endif
    *isFloating = false;
    return true;
#else
    return false;
#endif
}

bool Gui::create(const char* api, bool isFloating) {
#if CLASP_HAS_WEBVIEW
    if (!isApiSupported(api, isFloating)) return false;

    choc::ui::WebView::Options options;
    options.enableDebugMode = true;  // Allow dev tools for debugging

    impl_->webview = std::make_unique<choc::ui::WebView>(options);

    // Bind JavaScript functions for parameter control
    impl_->webview->bind("clasp_setParam", [this](const choc::value::ValueView& args) -> choc::value::Value {
        if (args.size() >= 2) {
            int paramId = args[0].getInt32();
            float value = static_cast<float>(args[1].getFloat64());
            if (paramChangeCallback_) {
                paramChangeCallback_(paramId, value);
            }
        }
        return {};
    });

    impl_->webview->bind("clasp_getParam", [this](const choc::value::ValueView& args) -> choc::value::Value {
        if (args.size() >= 1) {
            int paramId = args[0].getInt32();
            double value = plugin_->getParameterValue(paramId);
            return choc::value::createFloat64(value);
        }
        return choc::value::createFloat64(0);
    });

    impl_->webview->bind("clasp_getPluginInfo", [this](const choc::value::ValueView&) -> choc::value::Value {
        auto info = choc::value::createObject("PluginInfo");
        info.addMember("id", manifest_.id);
        info.addMember("name", manifest_.name);
        info.addMember("vendor", manifest_.vendor);
        info.addMember("version", manifest_.version);
        info.addMember("isInstrument", manifest_.isInstrument);

        auto params = choc::value::createEmptyArray();
        for (const auto& p : manifest_.parameters) {
            auto param = choc::value::createObject("Param");
            param.addMember("id", static_cast<int32_t>(p.id));
            param.addMember("name", p.name);
            param.addMember("min", static_cast<double>(p.min));
            param.addMember("max", static_cast<double>(p.max));
            param.addMember("default", static_cast<double>(p.defaultValue));
            params.addArrayElement(param);
        }
        info.addMember("parameters", params);

        return info;
    });

    // Inject the clasp JavaScript API
    std::string initScript = R"(
        window.clasp = {
            setParam: function(id, value) {
                return clasp_setParam(id, value);
            },
            getParam: function(id) {
                return clasp_getParam(id);
            },
            getPluginInfo: function() {
                return clasp_getPluginInfo();
            },
            onParamChange: null  // Set by UI to receive updates
        };

        // Notify UI when loaded
        if (window.onClaspReady) {
            window.onClaspReady();
        }
    )";
    impl_->webview->addInitScript(initScript);

    return true;
#else
    return false;
#endif
}

void Gui::destroy() {
#if CLASP_HAS_WEBVIEW
    impl_->webview.reset();
    impl_->parentWindow = nullptr;
#endif
    visible_ = false;
}

bool Gui::setScale(double scale) {
    scale_ = scale;
    return true;
}

bool Gui::getSize(uint32_t* width, uint32_t* height) {
    *width = width_;
    *height = height_;
    return true;
}

bool Gui::canResize() {
    return true;  // Allow resizing
}

bool Gui::getResizeHints(clap_gui_resize_hints_t* hints) {
    hints->can_resize_horizontally = true;
    hints->can_resize_vertically = true;
    hints->preserve_aspect_ratio = false;
    hints->aspect_ratio_width = 1;
    hints->aspect_ratio_height = 1;
    return true;
}

bool Gui::adjustSize(uint32_t* width, uint32_t* height) {
    // Accept any size
    return true;
}

bool Gui::setSize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    // WebView size is managed by the parent window
    return true;
}

bool Gui::setParent(const clap_window_t* window) {
#if CLASP_HAS_WEBVIEW
    if (!impl_->webview || !window) return false;

    void* handle = nullptr;

#if defined(__APPLE__)
    if (window->api && strcmp(window->api, CLAP_WINDOW_API_COCOA) == 0) {
        handle = window->cocoa;
    }
#elif defined(_WIN32)
    if (window->api && strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0) {
        handle = window->win32;
    }
#else
    if (window->api && strcmp(window->api, CLAP_WINDOW_API_X11) == 0) {
        handle = reinterpret_cast<void*>(window->x11);
    }
#endif

    if (!handle) return false;

    impl_->parentWindow = handle;

    // Get the native webview handle and embed it
    auto webviewHandle = impl_->webview->getViewHandle();

    // Platform-specific embedding
#if defined(__APPLE__)
    // On macOS, add the WebView as a subview
    // This requires Objective-C, handled by CHOC internally
    // For now, we rely on CHOC's setSize to work after navigation
#elif defined(_WIN32)
    // On Windows, set parent window
    SetParent((HWND)webviewHandle, (HWND)handle);
#else
    // On Linux, reparent the X11 window
    // This is complex and platform-specific
#endif

    // Navigate to the UI HTML
    if (!manifest_.ui.entry.empty()) {
        std::string url = "file://" + manifest_.ui.entry;
        impl_->webview->navigate(url);
    }

    return true;
#else
    return false;
#endif
}

bool Gui::setTransient(const clap_window_t* window) {
    return false;  // Not supported
}

void Gui::suggestTitle(const char* title) {
    // Not applicable for embedded view
}

bool Gui::show() {
#if CLASP_HAS_WEBVIEW
    visible_ = true;
    return true;
#else
    return false;
#endif
}

bool Gui::hide() {
#if CLASP_HAS_WEBVIEW
    visible_ = false;
    return true;
#else
    return false;
#endif
}

void Gui::notifyParameterChanged(int paramId, float value) {
#if CLASP_HAS_WEBVIEW
    if (impl_->webview && visible_) {
        // Call JavaScript to update UI
        std::string js = "if (window.clasp && window.clasp.onParamChange) { "
                        "window.clasp.onParamChange(" +
                        std::to_string(paramId) + ", " +
                        std::to_string(value) + "); }";
        impl_->webview->evaluateJavascript(js);
    }
#endif
}

void Gui::setParamChangeCallback(ParamChangeCallback callback) {
    paramChangeCallback_ = std::move(callback);
}

} // namespace clasp
