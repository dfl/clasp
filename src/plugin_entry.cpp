#include "clasp/logger.h"
#include "clasp/scanner.h"
#include "clasp/wclap_runtime.h"
#include <clap/clap.h>
#include <cstring>
#include <string>
#include <vector>

// Forward declarations from plugin_factory.cpp
extern const clap_plugin_factory_t *getPluginFactory();

namespace clasp {
Scanner &getGlobalScanner();
}

// Plugin path (set during init)
static std::string g_pluginPath;

static bool thunder_init(const char *pluginPath) {
  g_pluginPath = pluginPath ? pluginPath : "";

  // Initialize logging system
  clasp::Logger::instance().init();
  CLASP_LOG_INFO("thunder.clap initializing...");

  // Add the plugin's sibling directory to search paths
  if (!g_pluginPath.empty()) {
    // Get directory containing the plugin
    std::string dir = g_pluginPath;
    size_t lastSlash = dir.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
      dir = dir.substr(0, lastSlash);

      // Look for .wclap bundles in the same directory
      clasp::getGlobalScanner().addSearchPath(dir);
    }
  }

  // Initialize the WclapRuntime (Wasmtime engine)
  auto &runtime = clasp::WclapRuntime::instance();
  (void)runtime; // Initialization happens in constructor

  CLASP_LOG_INFO("thunder.clap initialized successfully");
  return true;
}

static void thunder_deinit() {
  // Runtime cleanup happens automatically via singleton destructor
}

static const void *thunder_get_factory(const char *factoryId) {
  if (strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0) {
    return getPluginFactory();
  }
  return nullptr;
}

// The plugin entry point
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = thunder_init,
    .deinit = thunder_deinit,
    .get_factory = thunder_get_factory,
};
