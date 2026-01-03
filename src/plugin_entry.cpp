#include <clap/clap.h>
#include "clasp/scanner.h"
#include "clasp/runtime.h"
#include <string>
#include <vector>
#include <cstring>

// Forward declarations from plugin_factory.cpp
extern const clap_plugin_factory_t* getPluginFactory();

namespace clasp {
    Scanner& getGlobalScanner();
}

// Plugin path (set during init)
static std::string g_pluginPath;

static bool clasp_init(const char* pluginPath) {
    g_pluginPath = pluginPath ? pluginPath : "";

    // Add the plugin's sibling directory to search paths
    if (!g_pluginPath.empty()) {
        // Get directory containing the plugin
        std::string dir = g_pluginPath;
        size_t lastSlash = dir.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            dir = dir.substr(0, lastSlash);

            // Look for .clasp bundles in the same directory
            clasp::getGlobalScanner().addSearchPath(dir);
        }
    }

    // Initialize the Wasmtime runtime
    auto& runtime = clasp::Runtime::instance();
    if (runtime.engine() == nullptr) {
        return false;
    }

    return true;
}

static void clasp_deinit() {
    // Runtime cleanup happens automatically via singleton destructor
}

static const void* clasp_get_factory(const char* factoryId) {
    if (strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return getPluginFactory();
    }
    return nullptr;
}

// The plugin entry point
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = clasp_init,
    .deinit = clasp_deinit,
    .get_factory = clasp_get_factory,
};
