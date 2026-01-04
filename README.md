# clasp - Tooling for Fast WCLAP Plugin Development

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0--beta-green.svg)](https://github.com/dfl/clasp)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey.svg)](https://github.com/dfl/clasp)

> **Rapid iteration tools for WCLAP audio plugin development**

**clasp** provides tooling for building [WCLAP](https://github.com/user/wclap) audio plugins with fast iteration cycles:

- **thunder.clap** - A dev-oriented meta-host with hot-reload for rapid iteration
- **clasp-create** - CLI tool for scaffolding new WCLAP projects
- **clasp-gui** - WebView library for building plugin UIs with HTML/CSS/JS
- **Templates** - Batteries-included project templates with DSP + WebView UI

---

## Download

Pre-built binaries available on the [Releases](https://github.com/dfl/clasp/releases) page.

Install `thunder.clap` to your CLAP plugins folder:
- **macOS**: `~/Library/Audio/Plug-Ins/CLAP/`
- **Windows**: `%LOCALAPPDATA%\Programs\Common\CLAP\`
- **Linux**: `~/.clap/`

---

## Quick Start

### 1. Build thunder.clap (optional)

```bash
# Clone with submodules
git clone --recursive https://github.com/dfl/clasp.git
cd clasp

# Build
mkdir build && cd build
cmake ..
make

# Install to your CLAP plugins folder
make install
```

### 2. Create a New Plugin

```bash
# Install clasp-create
cd packages/clasp-create
npm install && npm run build
npm link

# Create a new WCLAP effect (C++)
clasp-create "My Gain" --type effect --lang cpp --vendor "My Company"

# Or create an instrument
clasp-create "My Synth" --type instrument --lang cpp

cd my_gain

# Build (requires wasi-sdk)
export WASI_SDK_PREFIX=/path/to/wasi-sdk
mkdir build && cd build
cmake .. && make && make install
```

### 3. Load in Your DAW

1. Copy your `.wclap` bundle to `~/.wclap/plugins/`
2. Open your DAW and scan for plugins
3. Look for your plugin with the "(thunder)" suffix

---

## WCLAP Bundle Structure

A `.wclap` bundle is a directory containing:

```
MyPlugin.wclap/
├── module.wasm      # CLAP plugin compiled to WASM (standard CLAP ABI)
└── ui/              # Optional web-based UI
    ├── index.html
    └── clasp.js     # Communication library
```

The `module.wasm` implements the standard CLAP ABI - the same interface as native CLAP plugins, but compiled to WebAssembly.

---

## Development Features

### Hot Reload

Set the environment variable to enable automatic reloading when files change:

```bash
export CLASP_HOT_RELOAD=1
```

### WebView UI

thunder.clap implements the [CLAP WebView Draft Extension](https://github.com/free-audio/clap/blob/main/include/clap/ext/draft/webview.h):

- **Host-Managed**: Modern hosts provide the WebView, enabling tighter integration
- **Self-Contained**: Falls back to built-in WebView (via clasp-gui) in other hosts

### clasp.js Communication

Include `clasp.js` in your UI to communicate with the plugin:

```javascript
// Subscribe to parameter changes
clasp.on('paramChange', (id, value) => {
    console.log(`Param ${id} = ${value}`);
});

// Change a parameter
clasp.call('setParam', paramId, value);

// Get plugin info
clasp.call('getPluginInfo').then(info => {
    console.log(info.name, info.parameters);
});

// Subscribe to MIDI
clasp.on('noteOn', (channel, key, velocity) => { ... });
clasp.on('noteOff', (channel, key) => { ... });
clasp.on('midiCC', (channel, cc, value) => { ... });
```

---

## Project Structure

```
clasp/
├── src/                    # thunder.clap source
│   ├── plugin_entry.cpp    # CLAP entry point
│   ├── plugin_factory.cpp  # Plugin factory & wrapper
│   ├── plugin_instance.cpp # WclapPluginInstance implementation
│   ├── scanner.cpp         # .wclap bundle discovery
│   ├── wclap_runtime.cpp   # Wasmtime integration
│   └── gui.cpp             # WebView GUI wrapper
├── include/clasp/          # Headers
├── extern/
│   ├── clap/               # CLAP SDK
│   ├── clap-helpers/       # CLAP helper utilities
│   ├── clasp-gui/          # WebView library (submodule)
│   ├── wclap-cpp/          # WASM boundary helpers (submodule)
│   └── choc/               # CHOC utilities
├── packages/
│   └── clasp-create/       # Node.js CLI for scaffolding
├── templates/
│   └── gain/               # Gain effect template (C++ + WebView UI)
└── tests/                  # Unit tests
```

---

## Related Projects

- [WCLAP](https://github.com/user/wclap) - WebAssembly CLAP specification
- [wclap-bridge](https://github.com/user/wclap-bridge) - Wasmtime runtime for hosting WCLAP plugins
- [wclap-cpp](https://github.com/user/wclap-cpp) - Header-only C++ library for WASM boundary marshalling
- [clasp-gui](https://github.com/dfl/clasp-gui) - Decoupled WebView + Protocol library

---

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Author

**[David Lowenfels](https://github.com/dfl)**

## License

MIT License. See [LICENSE](LICENSE) for details.
