#pragma once

#include "clasp/scanner.h"
#include <clap/clap.h>
#include <memory>
#include <functional>
#include <string>

namespace clasp {

// Forward declare the plugin instance
class PluginInstance;

// GUI wrapper using CHOC WebView
class Gui {
public:
    Gui(PluginInstance* plugin, const PluginManifest& manifest);
    ~Gui();

    // CLAP GUI extension interface
    bool isApiSupported(const char* api, bool isFloating);
    bool getPreferredApi(const char** api, bool* isFloating);
    bool create(const char* api, bool isFloating);
    void destroy();
    bool setScale(double scale);
    bool getSize(uint32_t* width, uint32_t* height);
    bool canResize();
    bool getResizeHints(clap_gui_resize_hints_t* hints);
    bool adjustSize(uint32_t* width, uint32_t* height);
    bool setSize(uint32_t width, uint32_t height);
    bool setParent(const clap_window_t* window);
    bool setTransient(const clap_window_t* window);
    void suggestTitle(const char* title);
    bool show();
    bool hide();

    // Parameter updates from plugin to UI
    void notifyParameterChanged(int paramId, float value);

    // Callback for parameter changes from UI
    using ParamChangeCallback = std::function<void(int paramId, float value)>;
    void setParamChangeCallback(ParamChangeCallback callback);

private:
    PluginInstance* plugin_;
    PluginManifest manifest_;

    // Platform-specific implementation (using CHOC)
    struct Impl;
    std::unique_ptr<Impl> impl_;

    ParamChangeCallback paramChangeCallback_;

    uint32_t width_;
    uint32_t height_;
    double scale_ = 1.0;
    bool visible_ = false;
};

} // namespace clasp
