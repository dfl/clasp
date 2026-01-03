#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace clasp {

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
};

// Scanner for discovering .clasp bundles
class Scanner {
public:
    Scanner();

    // Scan all configured paths and return discovered plugins
    std::vector<PluginManifest> scan();

    // Scan a specific directory
    std::vector<PluginManifest> scanDirectory(const std::string& path);

    // Load manifest from a specific bundle
    std::optional<PluginManifest> loadManifest(const std::string& bundlePath);

    // Configure search paths
    void addSearchPath(const std::string& path);
    void clearSearchPaths();

    // Get current search paths
    const std::vector<std::string>& searchPaths() const { return searchPaths_; }

private:
    std::vector<std::string> searchPaths_;

    // Initialize default search paths
    void initDefaultPaths();

    // Parse plugin.json
    std::optional<PluginManifest> parseManifest(const std::string& jsonPath,
                                                 const std::string& bundlePath);
};

// Get the path to the running plugin binary
std::string getPluginBinaryPath();

// Expand ~ to home directory
std::string expandPath(const std::string& path);

} // namespace clasp
