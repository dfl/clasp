// clasp-host: Dev host plugin instance
// Loads a WCLAP bundle and provides hot-reload during development

#include "clasp-host/descriptor.h"
#include "clasp-host/hot_reload.h"
#include <clap/clap.h>
#include <memory>
#include <string>

namespace {

class DevHostPlugin {
public:
  DevHostPlugin(const clap_host_t *host) : host_(host) {}

  bool init() {
    // TODO: Initialize file browser to select WCLAP bundle
    // TODO: Set up hot-reload watcher
    return true;
  }

  void destroy() { hotReload_.stop(); }

  bool activate(double sampleRate, uint32_t minFrames, uint32_t maxFrames) {
    sampleRate_ = sampleRate;
    return true;
  }

  void deactivate() {}

  bool startProcessing() { return true; }

  void stopProcessing() {}

  clap_process_status process(const clap_process_t *process) {
    // TODO: Forward to loaded WCLAP plugin
    // For now, pass through audio
    for (uint32_t i = 0; i < process->audio_outputs_count; ++i) {
      auto &out = process->audio_outputs[i];
      auto &in = process->audio_inputs[i];

      for (uint32_t ch = 0; ch < out.channel_count; ++ch) {
        if (out.data32[ch] && in.data32[ch]) {
          memcpy(out.data32[ch], in.data32[ch],
                 process->frames_count * sizeof(float));
        }
      }
    }
    return CLAP_PROCESS_CONTINUE;
  }

  void onMainThread() {
    // Process hot-reload events
    // TODO: Reload WCLAP if files changed
  }

  void loadBundle(const std::string &path) {
    bundlePath_ = path;

    // Start watching for changes
    hotReload_.watch(path, [this](const std::string &changedFile) {
      pendingReload_ = true;
      // Request main thread callback to reload
      if (host_->request_callback) {
        host_->request_callback(host_);
      }
    });
  }

private:
  const clap_host_t *host_;
  double sampleRate_ = 44100.0;
  std::string bundlePath_;
  clasp_host::HotReload hotReload_;
  bool pendingReload_ = false;
};

// CLAP plugin wrapper
struct PluginWrapper {
  clap_plugin_t plugin;
  DevHostPlugin impl;

  PluginWrapper(const clap_host_t *host, const clap_plugin_descriptor_t *desc)
      : impl(host) {
    plugin.desc = desc;
    plugin.plugin_data = this;
    plugin.init = [](const clap_plugin_t *p) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.init();
    };
    plugin.destroy = [](const clap_plugin_t *p) {
      auto *w = static_cast<PluginWrapper *>(p->plugin_data);
      w->impl.destroy();
      delete w;
    };
    plugin.activate = [](const clap_plugin_t *p, double sr, uint32_t min,
                         uint32_t max) -> bool {
      return static_cast<PluginWrapper *>(p->plugin_data)
          ->impl.activate(sr, min, max);
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
    plugin.reset = [](const clap_plugin_t *) {};
    plugin.process = [](const clap_plugin_t *p,
                        const clap_process_t *proc) -> clap_process_status {
      return static_cast<PluginWrapper *>(p->plugin_data)->impl.process(proc);
    };
    plugin.get_extension = [](const clap_plugin_t *, const char *) -> const void * {
      return nullptr;
    };
    plugin.on_main_thread = [](const clap_plugin_t *p) {
      static_cast<PluginWrapper *>(p->plugin_data)->impl.onMainThread();
    };
  }
};

} // namespace

const clap_plugin_t *clasp_host_create_plugin(const clap_host_t *host,
                                               const char *plugin_id) {
  auto *wrapper = new PluginWrapper(host, clasp_host::getDescriptor());
  return &wrapper->plugin;
}
