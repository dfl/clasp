#include "clasp/runtime.h"
#include "clasp/scanner.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Helper to find the templates directory
fs::path findTemplatesDir(const char *argv0) {
  fs::path exePath = fs::absolute(fs::path(argv0).parent_path());

  // Check relative to executable (assuming built in build/ directory)
  std::vector<fs::path> searchPaths = {
      exePath / "templates", exePath / "../templates",
      exePath / "../../templates", "/usr/local/share/clasp/templates"};

  const char *envDir = std::getenv("CLASP_TEMPLATES_DIR");
  if (envDir)
    searchPaths.insert(searchPaths.begin(), fs::path(envDir));

  for (const auto &p : searchPaths) {
    if (fs::exists(p / "common/plugin.json.template")) {
      return p;
    }
  }
  return "";
}

std::string loadTemplate(const fs::path &p) {
  std::ifstream f(p);
  if (!f.is_open())
    return "";
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

void replaceAll(std::string &str, const std::string &from,
                const std::string &to) {
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
}

void precompile() {
  std::cout << "Warming up AOT cache...\n";
  auto &runtime = clasp::Runtime::instance();
  clasp::Scanner scanner;
  auto manifests = scanner.scan();
  for (const auto &m : manifests) {
    fs::path p = fs::path(m.bundlePath) / "dsp.wasm";
    if (fs::exists(p)) {
      auto start = std::chrono::steady_clock::now();
      auto inst = runtime.loadModule(p.string());
      auto end = std::chrono::steady_clock::now();
      auto ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
              .count();
      if (inst)
        std::cout << "  " << m.name << " (" << ms << "ms)\n";
      else
        std::cout << "  FAILED: " << m.name << "\n";
    }
  }
}

void createPlugin(const fs::path &templateDir, std::string name,
                  std::string lang) {
  std::string id = name;
  for (auto &c : id)
    c = std::tolower(c);
  replaceAll(id, " ", "-");

  std::cout << "Creating " << lang << " plugin: " << name << " (" << id
            << ")...\n";

  fs::create_directories(id + "/ui");

  auto write = [&](const std::string &dest, const fs::path &templatePath) {
    std::string content = loadTemplate(templatePath);
    if (content.empty()) {
      std::cerr << "Warning: Could not find template " << templatePath << "\n";
      return;
    }
    replaceAll(content, "PLUGIN_NAME", name);
    replaceAll(content, "PLUGIN_ID", id);

    fs::path destPath = fs::path(id) / dest;
    fs::create_directories(destPath.parent_path());
    std::ofstream f(destPath);
    f << content;
  };

  // Common UI and Manifest
  write("ui/index.html", templateDir / "common/ui/index.html.template");
  write("plugin.json", templateDir / "common/plugin.json.template");

  if (lang == "cpp") {
    write("src/dsp.cpp", templateDir / "cpp/src/dsp.cpp.template");
    write("CMakeLists.txt", templateDir / "cpp/CMakeLists.txt.template");
    std::cout << "Success! To build:\n  cd " << id
              << "\n  cmake -B build && cmake --build build\n";
  } else if (lang == "rust") {
    write("src/lib.rs", templateDir / "rust/src/lib.rs.template");
    write("Cargo.toml", templateDir / "rust/Cargo.toml.template");
    std::cout << "Success! To build:\n  cd " << id
              << "\n  cargo build --target wasm32-wasi --release\n";
  } else if (lang == "as") {
    write("assembly/index.ts", templateDir / "as/assembly/index.ts.template");
    write("package.json", templateDir / "as/package.json.template");
    write("asconfig.json", templateDir / "as/asconfig.json.template");
    std::cout << "Success! To build:\n  cd " << id
              << "\n  npm install && npm run asbuild\n";
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Usage: clasp-tool <command>\n";
    std::cout << "Commands:\n";
    std::cout << "  warm                                - Pre-compile "
                 "available plugins\n";
    std::cout << "  create [--lang cpp|rust|as] <name>  - Create a new plugin "
                 "template\n";
    return 1;
  }

  std::string cmd = argv[1];
  if (cmd == "warm") {
    precompile();
  } else if (cmd == "create" && argc > 2) {
    fs::path templateDir = findTemplatesDir(argv[0]);
    if (templateDir.empty()) {
      std::cerr << "Error: Could not find templates directory.\n";
      std::cerr
          << "Ensure the 'templates' folder exists in the project root.\n";
      return 1;
    }

    std::string lang = "cpp";
    std::string name = argv[2];
    if (argc > 3) {
      std::string arg = argv[2];
      if (arg == "--lang" && argc > 4) {
        lang = argv[3];
        name = argv[4];
      }
    }
    createPlugin(templateDir, name, lang);
  } else {
    std::cerr << "Invalid command.\n";
    return 1;
  }
  return 0;
}
