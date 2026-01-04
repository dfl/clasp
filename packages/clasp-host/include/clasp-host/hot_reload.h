#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

namespace clasp_host {

// File watcher for hot-reload functionality
class HotReload {
public:
  using ReloadCallback = std::function<void(const std::string &path)>;

  HotReload();
  ~HotReload();

  // Start watching a WCLAP bundle for changes
  void watch(const std::filesystem::path &bundlePath, ReloadCallback callback);

  // Stop watching
  void stop();

  // Check if currently watching
  bool isWatching() const { return watching_.load(); }

private:
  std::atomic<bool> watching_{false};
  std::thread watchThread_;
  ReloadCallback callback_;
  std::filesystem::path watchPath_;

  void watchLoop();
};

} // namespace clasp_host
