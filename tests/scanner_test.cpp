#include "clasp/scanner.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("Scanner WCLAP Bundle Detection", "[scanner]") {
  // Setup a dummy .wclap bundle with module.wasm
  fs::path bundlePath = fs::absolute("TestPlugin.wclap");
  fs::create_directories(bundlePath);

  // Create a dummy module.wasm (just needs to exist)
  std::ofstream wasm(bundlePath / "module.wasm");
  wasm.close();

  clasp::Scanner scanner;
  auto manifest = scanner.loadManifest(bundlePath.string());

  // Without cached metadata, this should return nullopt
  // (WCLAP requires either cached metadata or runtime introspection)
  CHECK_FALSE(manifest.has_value());

  // Cleanup
  fs::remove_all(bundlePath);
}

TEST_CASE("Scanner Invalid Bundle", "[scanner]") {
  // Setup a bundle without module.wasm
  fs::path bundlePath = fs::absolute("Invalid.wclap");
  fs::create_directories(bundlePath);

  clasp::Scanner scanner;
  auto manifest = scanner.loadManifest(bundlePath.string());

  // Should fail - no module.wasm
  CHECK_FALSE(manifest.has_value());

  // Cleanup
  fs::remove_all(bundlePath);
}

TEST_CASE("Path Expansion", "[scanner]") {
  // Relative paths shouldn't change
  std::string path = "relative/path";
  auto expanded = clasp::expandPath(path);
  CHECK(expanded == path);
}
