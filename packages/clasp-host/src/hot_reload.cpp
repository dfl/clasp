#include "clasp-host/hot_reload.h"
#include <chrono>
#include <map>

namespace fs = std::filesystem;

namespace clasp_host {

HotReload::HotReload() = default;

HotReload::~HotReload() { stop(); }

void HotReload::watch(const fs::path &bundlePath, ReloadCallback callback) {
  stop();

  watchPath_ = bundlePath;
  callback_ = std::move(callback);
  watching_ = true;

  watchThread_ = std::thread([this]() { watchLoop(); });
}

void HotReload::stop() {
  watching_ = false;
  if (watchThread_.joinable()) {
    watchThread_.join();
  }
}

void HotReload::watchLoop() {
  // Track last modification times
  std::map<std::string, fs::file_time_type> lastMod;

  auto updateTimes = [&]() {
    if (!fs::exists(watchPath_))
      return;

    for (const auto &entry : fs::recursive_directory_iterator(watchPath_)) {
      if (entry.is_regular_file()) {
        auto path = entry.path().string();
        auto ext = entry.path().extension().string();

        // Watch .wasm, .js, .html, .css files
        if (ext == ".wasm" || ext == ".js" || ext == ".html" || ext == ".css" ||
            ext == ".json") {
          lastMod[path] = fs::last_write_time(entry);
        }
      }
    }
  };

  // Initial scan
  updateTimes();

  while (watching_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (!fs::exists(watchPath_))
      continue;

    for (const auto &entry : fs::recursive_directory_iterator(watchPath_)) {
      if (!entry.is_regular_file())
        continue;

      auto path = entry.path().string();
      auto ext = entry.path().extension().string();

      if (ext != ".wasm" && ext != ".js" && ext != ".html" && ext != ".css" &&
          ext != ".json")
        continue;

      auto currentTime = fs::last_write_time(entry);
      auto it = lastMod.find(path);

      if (it == lastMod.end()) {
        // New file
        lastMod[path] = currentTime;
        if (callback_)
          callback_(path);
      } else if (it->second != currentTime) {
        // Modified file
        it->second = currentTime;
        if (callback_)
          callback_(path);
      }
    }
  }
}

} // namespace clasp_host
