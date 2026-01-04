#include "clasp/runtime.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("Runtime Initialization", "[runtime]") {
  auto &runtime = clasp::Runtime::instance();
  CHECK(runtime.engine() != nullptr);
  CHECK(runtime.store() != nullptr);
  CHECK(runtime.context() != nullptr);
}

TEST_CASE("Runtime Load Invalid Module", "[runtime]") {
  auto &runtime = clasp::Runtime::instance();
  auto instance = runtime.loadModule("non_existent.wasm");
  CHECK(instance == nullptr);
  CHECK(!runtime.lastError().empty());
}

TEST_CASE("Runtime Cache Directory", "[runtime]") {
  auto &runtime = clasp::Runtime::instance();
  CHECK(!runtime.cacheDir().empty());

  std::string oldCache = runtime.cacheDir();
  runtime.setCacheDir("/tmp/clasp_test_cache");
  CHECK(runtime.cacheDir() == "/tmp/clasp_test_cache");
  runtime.setCacheDir(oldCache);
}
