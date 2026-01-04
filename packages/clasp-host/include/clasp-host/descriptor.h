#pragma once

#include <clap/clap.h>

namespace clasp_host {

namespace detail {
inline const char *const features[] = {CLAP_PLUGIN_FEATURE_UTILITY,
                                        CLAP_PLUGIN_FEATURE_ANALYZER, nullptr};
}

inline const clap_plugin_descriptor_t *getDescriptor() {
  static const clap_plugin_descriptor_t desc = {
      .clap_version = CLAP_VERSION,
      .id = "com.clasp.dev-host",
      .name = "CLASP Dev Host",
      .vendor = "CLASP",
      .url = "https://github.com/user/clasp",
      .manual_url = "",
      .support_url = "",
      .version = "1.0.0",
      .description =
          "Development host for WCLAP plugins with hot-reload support",
      .features = detail::features,
  };
  return &desc;
}

} // namespace clasp_host
