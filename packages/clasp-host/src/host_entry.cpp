// clasp-host: Development host plugin entry point
// This is the dev-focused variant of the clasp plugin with hot-reload support

#include <clap/clap.h>
#include <cstring>

extern const clap_plugin_factory_t *clasp_host_get_factory();

static const void *get_factory(const char *factory_id) {
  if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
    return clasp_host_get_factory();
  }
  return nullptr;
}

static bool init(const char *plugin_path) {
  // Initialize runtime
  return true;
}

static void deinit() {
  // Cleanup
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = init,
    .deinit = deinit,
    .get_factory = get_factory,
};
