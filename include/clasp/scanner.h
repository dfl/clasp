#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace clasp {

// Bundle format types
enum class BundleType {
  Clasp, // Legacy .clasp bundles with plugin.json + dsp.wasm
  Wclap  // Standard .wclap bundles with module.wasm (CLAP compiled to WASM)
};

// Parameter definition from plugin.json
struct ParamInfo {
  int32_t id;
  std::string name;
  float min;
  float max;
  float defaultValue;
};

// Audio configuration from plugin.json
struct AudioConfig {
  int inputs = 2;
  int outputs = 2;
  uint32_t latency = 0;
  uint32_t tailSize = 0;
};

// UI configuration from plugin.json
struct UiConfig {
  bool hasUi = false;
  std::string entry;
  int width = 800;
  int height = 600;
};

// Complete plugin manifest
struct PluginManifest {
  std::string id;
  std::string name;
  std::string vendor;
  std::string version;
  bool isInstrument = false;
  AudioConfig audio;
  std::vector<ParamInfo> parameters;
  UiConfig ui;

  // Paths
  std::string bundlePath;
  std::string wasmPath;

  // Bundle type
  BundleType bundleType = BundleType::Clasp;
};

// Scanner for discovering .clasp and .wclap bundles
class Scanner {
public:
  Scanner();

  // Scan all configured paths and return discovered plugins
  std::vector<PluginManifest> scan();

  // Scan a specific directory
  std::vector<PluginManifest> scanDirectory(const std::string &path);

  // Load manifest from a specific bundle (auto-detects type)
  std::optional<PluginManifest> loadManifest(const std::string &bundlePath);

  // Configure search paths
  void addSearchPath(const std::string &path);
  void clearSearchPaths();

  // Get current search paths
  const std::vector<std::string> &searchPaths() const { return searchPaths_; }

  // Metadata cache management
  void setCacheDir(const std::string &dir) { cacheDir_ = dir; }
  const std::string &cacheDir() const { return cacheDir_; }

private:
  std::vector<std::string> searchPaths_;
  std::string cacheDir_;

  // Initialize default search paths
  void initDefaultPaths();

  // Parse plugin.json (for .clasp bundles)
  std::optional<PluginManifest> parseClaspManifest(const std::string &jsonPath,
                                                   const std::string &bundlePath);

  // Load/save WCLAP metadata from cache
  std::optional<PluginManifest> loadWclapCached(const std::string &bundlePath);
  bool saveWclapCache(const PluginManifest &manifest);
  std::string getCachePath(const std::string &bundlePath);
  bool isCacheValid(const std::string &bundlePath, const std::string &cachePath);

  // Query WCLAP module for metadata (requires instantiation)
  std::optional<PluginManifest> queryWclapMetadata(const std::string &bundlePath);
};

// Get the path to the running plugin binary
std::string getPluginBinaryPath();

// Expand ~ to home directory
std::string expandPath(const std::string &path);

} // namespace clasp
