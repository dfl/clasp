// clasp-warm: Pre-compile WCLAP bundles to warm the AOT cache
//
// Usage:
//   clasp-warm              # Warm cache for all discovered plugins
//   clasp-warm ./foo.wclap  # Warm specific bundle

#include "clasp/scanner.h"
#include "clasp/wclap_runtime.h"
#include <chrono>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

void warmBundle(const fs::path &bundlePath) {
  fs::path wasmPath = bundlePath / "dsp.wasm";
  if (!fs::exists(wasmPath)) {
    wasmPath = bundlePath / "module.wasm";
  }

  if (!fs::exists(wasmPath)) {
    std::cerr << "  No .wasm found in " << bundlePath << "\n";
    return;
  }

  auto start = std::chrono::steady_clock::now();

  auto &runtime = clasp::WclapRuntime::instance();
  auto instance = runtime.loadModule(wasmPath.string());

  auto end = std::chrono::steady_clock::now();
  auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

  if (instance) {
    std::cout << "  " << bundlePath.filename().string() << " (" << ms << "ms)\n";
  } else {
    std::cerr << "  FAILED: " << bundlePath.filename().string() << "\n";
  }
}

void warmAll() {
  std::cout << "Warming AOT cache for all discovered plugins...\n";

  clasp::Scanner scanner;
  auto manifests = scanner.scan();

  if (manifests.empty()) {
    std::cout << "No plugins found.\n";
    return;
  }

  for (const auto &m : manifests) {
    warmBundle(m.bundlePath);
  }

  std::cout << "Done. Warmed " << manifests.size() << " plugin(s).\n";
}

int main(int argc, char **argv) {
  if (argc > 1) {
    std::string arg = argv[1];

    if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: clasp-warm [path.wclap]\n";
      std::cout << "  Without arguments: warm cache for all discovered plugins\n";
      std::cout << "  With path: warm cache for specific bundle\n";
      return 0;
    }

    fs::path bundlePath = arg;
    if (!fs::exists(bundlePath)) {
      std::cerr << "Error: " << bundlePath << " not found\n";
      return 1;
    }

    std::cout << "Warming AOT cache...\n";
    warmBundle(bundlePath);
  } else {
    warmAll();
  }

  return 0;
}
