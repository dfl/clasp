#include "clasp/scanner.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

// Using CHOC for JSON parsing
#include "choc/text/choc_JSON.h"

namespace fs = std::filesystem;

namespace clasp {

std::string expandPath(const std::string &path) {
  if (path.empty() || path[0] != '~') {
    return path;
  }

  const char *home = std::getenv("HOME");
  if (!home) {
    return path;
  }

  return std::string(home) + path.substr(1);
}

std::string getPluginBinaryPath() {
  // Platform-specific way to get the path of the running plugin
#if defined(__APPLE__)
  // On macOS, we need to find the bundle path
  // This will be filled in by the plugin entry point
  return ""; // Will be set externally
#elif defined(_WIN32)
  char path[MAX_PATH];
  GetModuleFileNameA(nullptr, path, MAX_PATH);
  return std::string(path);
#else
  char path[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
  if (count != -1) {
    return std::string(path, count);
  }
  return "";
#endif
}

Scanner::Scanner() { initDefaultPaths(); }

void Scanner::initDefaultPaths() {
  // Default search paths:
  // 1. User directory: ~/.clasp/plugins/
  searchPaths_.push_back(expandPath("~/.clasp/plugins"));

  // 2. Check CLASP_PLUGIN_PATH environment variable
  const char *envPath = std::getenv("CLASP_PLUGIN_PATH");
  if (envPath) {
    std::string pathStr(envPath);
    size_t start = 0;
    size_t end;

    // Split by colon (Unix) or semicolon (Windows)
#ifdef _WIN32
    char delimiter = ';';
#else
    char delimiter = ':';
#endif
    while ((end = pathStr.find(delimiter, start)) != std::string::npos) {
      std::string p = pathStr.substr(start, end - start);
      if (!p.empty()) {
        searchPaths_.push_back(expandPath(p));
      }
      start = end + 1;
    }
    std::string lastPath = pathStr.substr(start);
    if (!lastPath.empty()) {
      searchPaths_.push_back(expandPath(lastPath));
    }
  }

  // 3. Plugin binary sibling directory will be added by plugin entry
}

void Scanner::addSearchPath(const std::string &path) {
  searchPaths_.push_back(expandPath(path));
}

void Scanner::clearSearchPaths() { searchPaths_.clear(); }

std::vector<PluginManifest> Scanner::scan() {
  std::vector<PluginManifest> result;

  for (const auto &path : searchPaths_) {
    auto plugins = scanDirectory(path);
    result.insert(result.end(), plugins.begin(), plugins.end());
  }

  return result;
}

std::vector<PluginManifest> Scanner::scanDirectory(const std::string &path) {
  std::vector<PluginManifest> result;

  if (!fs::exists(path) || !fs::is_directory(path)) {
    return result;
  }

  for (const auto &entry : fs::directory_iterator(path)) {
    if (entry.is_directory()) {
      std::string dirName = entry.path().filename().string();

      // Check if it's a .clasp bundle
      if (dirName.size() > 6 &&
          dirName.substr(dirName.size() - 6) == ".clasp") {
        auto manifest = loadManifest(entry.path().string());
        if (manifest) {
          result.push_back(std::move(*manifest));
        }
      }
    }
  }

  return result;
}

std::optional<PluginManifest>
Scanner::loadManifest(const std::string &bundlePath) {
  std::string jsonPath = bundlePath + "/plugin.json";
  return parseManifest(jsonPath, bundlePath);
}

std::optional<PluginManifest>
Scanner::parseManifest(const std::string &jsonPath,
                       const std::string &bundlePath) {
  // Read JSON file
  std::ifstream file(jsonPath);
  if (!file) {
    std::cerr << "[clasp] Failed to open: " << jsonPath << std::endl;
    return std::nullopt;
  }

  std::string jsonStr((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  // Parse JSON using CHOC
  try {
    auto json = choc::json::parse(jsonStr);

    PluginManifest manifest;
    manifest.bundlePath = bundlePath;
    manifest.wasmPath = bundlePath + "/dsp.wasm";

    // Check if WASM file exists
    if (!fs::exists(manifest.wasmPath)) {
      std::cerr << "[clasp] Missing dsp.wasm in: " << bundlePath << std::endl;
      return std::nullopt;
    }

    // Required fields
    if (!json.hasObjectMember("id") || !json.hasObjectMember("name") ||
        !json.hasObjectMember("vendor") || !json.hasObjectMember("version")) {
      std::cerr << "[clasp] Missing required fields in: " << jsonPath
                << std::endl;
      return std::nullopt;
    }

    manifest.id = json["id"].toString();
    manifest.name = json["name"].toString();
    manifest.vendor = json["vendor"].toString();
    manifest.version = json["version"].toString();

    // Plugin type
    if (json.hasObjectMember("type")) {
      std::string type = json["type"].toString();
      manifest.isInstrument = (type == "instrument" || type == "synth");
    }

    // Audio configuration
    if (json.hasObjectMember("audio")) {
      auto audio = json["audio"];
      if (audio.hasObjectMember("inputs")) {
        manifest.audio.inputs = static_cast<int>(audio["inputs"].getInt64());
      }
      if (audio.hasObjectMember("outputs")) {
        manifest.audio.outputs = static_cast<int>(audio["outputs"].getInt64());
      }
      if (audio.hasObjectMember("latency")) {
        manifest.audio.latency =
            static_cast<uint32_t>(audio["latency"].getInt64());
      }
      if (audio.hasObjectMember("tail")) {
        manifest.audio.tailSize =
            static_cast<uint32_t>(audio["tail"].getInt64());
      }
    }

    // Parameters
    if (json.hasObjectMember("parameters") && json["parameters"].isArray()) {
      auto params = json["parameters"];
      for (uint32_t i = 0; i < params.size(); ++i) {
        auto p = params[i];
        ParamInfo info;
        info.id = static_cast<int32_t>(p["id"].getInt64());
        info.name = p["name"].toString();
        info.min = static_cast<float>(p["min"].getFloat64());
        info.max = static_cast<float>(p["max"].getFloat64());
        info.defaultValue = static_cast<float>(p["default"].getFloat64());
        manifest.parameters.push_back(info);
      }
    }

    // UI configuration
    if (json.hasObjectMember("ui")) {
      auto ui = json["ui"];
      manifest.ui.hasUi = true;

      if (ui.hasObjectMember("entry")) {
        manifest.ui.entry = bundlePath + "/" + ui["entry"].toString();
      } else {
        manifest.ui.entry = bundlePath + "/ui/index.html";
      }

      if (ui.hasObjectMember("width")) {
        manifest.ui.width = static_cast<int>(ui["width"].getInt64());
      }
      if (ui.hasObjectMember("height")) {
        manifest.ui.height = static_cast<int>(ui["height"].getInt64());
      }
    }

    std::cerr << "[clasp] Loaded plugin: " << manifest.name << " ("
              << manifest.id << ")" << std::endl;

    return manifest;

  } catch (const std::exception &e) {
    std::cerr << "[clasp] JSON parse error in " << jsonPath << ": " << e.what()
              << std::endl;
    return std::nullopt;
  }
}

} // namespace clasp
