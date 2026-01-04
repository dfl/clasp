// clasp-host: Plugin factory
// Lists the dev host as a single "shell" plugin that loads WCLAPs

#include "clasp-host/descriptor.h"
#include <clap/clap.h>
#include <cstring>

// Forward declaration
const clap_plugin_t *clasp_host_create_plugin(const clap_host_t *host,
                                               const char *plugin_id);

static uint32_t get_plugin_count(const clap_plugin_factory_t *factory) {
  return 1; // Single dev host plugin
}

static const clap_plugin_descriptor_t *
get_plugin_descriptor(const clap_plugin_factory_t *factory, uint32_t index) {
  if (index == 0) {
    return clasp_host::getDescriptor();
  }
  return nullptr;
}

static const clap_plugin_t *create_plugin(const clap_plugin_factory_t *factory,
                                          const clap_host_t *host,
                                          const char *plugin_id) {
  auto *desc = clasp_host::getDescriptor();
  if (strcmp(plugin_id, desc->id) == 0) {
    return clasp_host_create_plugin(host, plugin_id);
  }
  return nullptr;
}

static const clap_plugin_factory_t factory = {
    .get_plugin_count = get_plugin_count,
    .get_plugin_descriptor = get_plugin_descriptor,
    .create_plugin = create_plugin,
};

const clap_plugin_factory_t *clasp_host_get_factory() { return &factory; }
