#include "clasp/scanner.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("Scanner Manifest Loading", "[scanner]") {
  // Setup a dummy bundle with absolute path
  fs::path bundlePath = fs::absolute("TestPlugin.clasp");
  fs::create_directories(bundlePath);

  std::ofstream f(bundlePath / "plugin.json");
  f << R"({
        "id": "com.test.plugin",
        "name": "Test Plugin",
        "vendor": "Test Vendor",
        "version": "1.0.0",
        "audio": { "inputs": 2, "outputs": 2 },
        "parameters": [
            { "id": 0, "name": "Gain", "min": 0.0, "max": 2.0, "default": 1.0 }
        ]
    })";
  f.close();

  // Scanner expects dsp.wasm to exist
  std::ofstream wasm(bundlePath / "dsp.wasm");
  wasm.close();

  clasp::Scanner scanner;
  auto manifest = scanner.loadManifest(bundlePath.string());

  REQUIRE(manifest.has_value());
  CHECK(manifest->id == "com.test.plugin");
  CHECK(manifest->name == "Test Plugin");
  CHECK(manifest->audio.inputs == 2);
  REQUIRE(manifest->parameters.size() == 1);
  CHECK(manifest->parameters[0].name == "Gain");

  // Cleanup
  fs::remove_all(bundlePath);
}

TEST_CASE("Path Expansion", "[scanner]") {
  // This is hard to test cross-platform with hardcoded ~ but we can check
  // relative expansion
  std::string path = "relative/path";
  auto expanded = clasp::expandPath(path);
  CHECK(expanded == path); // Relative paths shouldn't change
}
