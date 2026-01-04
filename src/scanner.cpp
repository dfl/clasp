#include "clasp/scanner.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

// Using CHOC for JSON parsing
#include "choc/text/choc_JSON.h"

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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

namespace {
// Get user cache directory for metadata
std::string getDefaultCacheDir() {
#ifdef _WIN32
  char path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0,
                                  path))) {
    return std::string(path) + "\\clasp\\cache";
  }
  return "";
#elif defined(__APPLE__)
  const char *home = std::getenv("HOME");
  if (home) {
    return std::string(home) + "/Library/Caches/clasp";
  }
  return "";
#else
  // Linux: use XDG_CACHE_HOME or ~/.cache
  const char *xdgCache = std::getenv("XDG_CACHE_HOME");
  if (xdgCache) {
    return std::string(xdgCache) + "/clasp";
  }
  const char *home = std::getenv("HOME");
  if (home) {
    return std::string(home) + "/.cache/clasp";
  }
  return "";
#endif
}

// Check if a directory name has a specific extension
bool hasExtension(const std::string &name, const std::string &ext) {
  if (name.size() <= ext.size())
    return false;
  return name.substr(name.size() - ext.size()) == ext;
}
} // namespace

Scanner::Scanner() { initDefaultPaths(); }

void Scanner::initDefaultPaths() {
  // Default cache directory
  cacheDir_ = getDefaultCacheDir();

  // Default search paths:
  // 1. User directory: ~/.wclap/plugins/
  searchPaths_.push_back(expandPath("~/.wclap/plugins"));

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

  // 3. Check WCLAP_PLUGIN_PATH environment variable
  const char *wclapPath = std::getenv("WCLAP_PLUGIN_PATH");
  if (wclapPath) {
    std::string pathStr(wclapPath);
    size_t start = 0;
    size_t end;
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

  // 4. Plugin binary sibling directory will be added by plugin entry
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

      // Check if it's a .wclap bundle
      if (hasExtension(dirName, ".wclap")) {
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
  std::string moduleWasm = bundlePath + "/module.wasm";

  // .wclap bundle: has module.wasm (standard WCLAP format)
  if (fs::exists(moduleWasm)) {
    // Try cached metadata first, then query module
    auto cached = loadWclapCached(bundlePath);
    if (cached) {
      return cached;
    }
    // TODO: queryWclapMetadata requires runtime integration
    // For now, just log and skip
    std::cerr << "[thunder] WCLAP bundle needs metadata cache: " << bundlePath
              << std::endl;
    return std::nullopt;
  }

  std::cerr << "[thunder] Invalid bundle (missing module.wasm): " << bundlePath << std::endl;
  return std::nullopt;
}

// Metadata caching for WCLAP bundles
std::string Scanner::getCachePath(const std::string &bundlePath) {
  if (cacheDir_.empty()) {
    return "";
  }

  // Create a hash-based filename from the bundle path
  std::hash<std::string> hasher;
  size_t pathHash = hasher(bundlePath);

  // Get bundle name for readability
  fs::path p(bundlePath);
  std::string bundleName = p.filename().string();

  return cacheDir_ + "/metadata/" + bundleName + "." +
         std::to_string(pathHash) + ".json";
}

bool Scanner::isCacheValid(const std::string &bundlePath,
                           const std::string &cachePath) {
  if (!fs::exists(cachePath)) {
    return false;
  }

  // Check if module.wasm is newer than cache
  std::string wasmPath = bundlePath + "/module.wasm";
  if (!fs::exists(wasmPath)) {
    return false;
  }

  auto wasmTime = fs::last_write_time(wasmPath);
  auto cacheTime = fs::last_write_time(cachePath);

  return cacheTime >= wasmTime;
}

std::optional<PluginManifest>
Scanner::loadWclapCached(const std::string &bundlePath) {
  std::string cachePath = getCachePath(bundlePath);
  if (cachePath.empty() || !isCacheValid(bundlePath, cachePath)) {
    return std::nullopt;
  }

  // Load cached metadata
  std::ifstream file(cachePath);
  if (!file) {
    return std::nullopt;
  }

  std::string jsonStr((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  try {
    auto json = choc::json::parse(jsonStr);

    PluginManifest manifest;
    manifest.bundlePath = bundlePath;
    manifest.wasmPath = bundlePath + "/module.wasm";
    manifest.bundleType = BundleType::Wclap;

    manifest.id = json["id"].toString();
    manifest.name = json["name"].toString();
    manifest.vendor = json["vendor"].toString();
    manifest.version = json["version"].toString();
    manifest.isInstrument = json["isInstrument"].getBool();

    if (json.hasObjectMember("audio")) {
      auto audio = json["audio"];
      manifest.audio.inputs = static_cast<int>(audio["inputs"].getInt64());
      manifest.audio.outputs = static_cast<int>(audio["outputs"].getInt64());
    }

    std::cerr << "[thunder] Loaded WCLAP plugin (cached): " << manifest.name
              << " (" << manifest.id << ")" << std::endl;

    return manifest;

  } catch (const std::exception &e) {
    std::cerr << "[thunder] Cache parse error: " << e.what() << std::endl;
    return std::nullopt;
  }
}

bool Scanner::saveWclapCache(const PluginManifest &manifest) {
  std::string cachePath = getCachePath(manifest.bundlePath);
  if (cachePath.empty()) {
    return false;
  }

  // Ensure cache directory exists
  fs::path cacheDir = fs::path(cachePath).parent_path();
  std::error_code ec;
  fs::create_directories(cacheDir, ec);
  if (ec) {
    return false;
  }

  // Write JSON cache
  std::ofstream file(cachePath);
  if (!file) {
    return false;
  }

  // Simple JSON serialization
  file << "{\n";
  file << "  \"id\": \"" << manifest.id << "\",\n";
  file << "  \"name\": \"" << manifest.name << "\",\n";
  file << "  \"vendor\": \"" << manifest.vendor << "\",\n";
  file << "  \"version\": \"" << manifest.version << "\",\n";
  file << "  \"isInstrument\": " << (manifest.isInstrument ? "true" : "false")
       << ",\n";
  file << "  \"audio\": {\n";
  file << "    \"inputs\": " << manifest.audio.inputs << ",\n";
  file << "    \"outputs\": " << manifest.audio.outputs << "\n";
  file << "  }\n";
  file << "}\n";

  return true;
}

std::optional<PluginManifest>
Scanner::queryWclapMetadata(const std::string &bundlePath) {
  // TODO: Implement WCLAP metadata query
  // This requires:
  // 1. Loading the WASM module with WclapRuntime
  // 2. Calling clap_plugin_factory_create
  // 3. Querying plugin descriptor
  // 4. Caching the result
  //
  // For now, return empty - WCLAP bundles need pre-cached metadata
  // or manual metadata generation via clasp-tool
  return std::nullopt;
}

} // namespace clasp
