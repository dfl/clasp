// clasp-precompile: Pre-compile all WASM modules in the plugin directory
// This warms up the AOT cache so plugin instantiation is fast.

#include "clasp/runtime.h"
#include "clasp/scanner.h"
#include <chrono>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
  std::cout << "CLASP Pre-compiler\n";
  std::cout << "==================\n\n";

  // Initialize runtime
  auto &runtime = clasp::Runtime::instance();
  if (!runtime.engine()) {
    std::cerr << "Error: Failed to initialize Wasmtime runtime\n";
    std::cerr << runtime.lastError() << "\n";
    return 1;
  }

  // Scan for plugins
  clasp::Scanner scanner;
  auto manifests = scanner.scan();

  if (manifests.empty()) {
    std::cout << "No plugins found in:\n";
    std::cout << "  - ~/.clasp/plugins/\n";
    std::cout << "  - CLASP_PLUGIN_PATH\n";
    return 0;
  }

  std::cout << "Found " << manifests.size() << " plugin(s)\n\n";

  int compiled = 0;
  int cached = 0;
  int failed = 0;

  for (const auto &manifest : manifests) {
    std::cout << "  " << manifest.name << " (" << manifest.id << ")\n";

    // Check if WASM exists
    fs::path wasmPath = fs::path(manifest.bundlePath) / "dsp.wasm";
    if (!fs::exists(wasmPath)) {
      std::cout << "    ⚠ No dsp.wasm found, skipping\n";
      continue;
    }

    auto startTime = std::chrono::steady_clock::now();

    // Load module - this will compile or load from cache
    auto instance = runtime.loadModule(wasmPath.string());

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    if (instance) {
      if (duration.count() < 50) {
        std::cout << "    ✓ Cached (" << duration.count() << "ms)\n";
        cached++;
      } else {
        std::cout << "    ✓ Compiled (" << duration.count() << "ms)\n";
        compiled++;
      }
    } else {
      std::cout << "    ✗ Failed: " << runtime.lastError() << "\n";
      failed++;
    }
  }

  std::cout << "\nSummary:\n";
  std::cout << "  Compiled: " << compiled << "\n";
  std::cout << "  Cached:   " << cached << "\n";
  if (failed > 0) {
    std::cout << "  Failed:   " << failed << "\n";
  }
  std::cout << "\nCache location: ~/.clasp/cache/\n";

  return failed > 0 ? 1 : 0;
}
