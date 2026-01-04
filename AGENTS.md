# AGENTS.md - AI Agent Context for CLASP

This file provides context for AI coding agents working on this codebase.

## Project Overview

CLASP is a native CLAP audio plugin that hosts WebAssembly modules. It's a cross-platform implementation compatible with the [WCLAP](https://github.com/WebCLAP) ecosystem - both host WASM plugins as native CLAP in DAWs. CLASP adds Windows/Linux support, AOT caching, and WebView GUI.

**Tagline:** Native CLAP wrapper for WASM — compile once, run everywhere!

## Architecture

```
DAW (CLAP Host) → clasp.clap (Native) → YourPlugin.clasp (WASM bundle)
```

- **clasp.clap**: Single native plugin binary that scans for and loads `.clasp` bundles
- **.clasp bundles**: Directories containing `plugin.json` + `dsp.wasm` + optional `ui/`
- **Wasmtime**: JIT/AOT WASM runtime embedded in clasp.clap
- **CHOC WebView**: HTML/JS UI rendering (WKWebView/WebView2/WebKitGTK)

## Key Directories

```
src/                    # Native wrapper source
├── plugin_entry.cpp    # CLAP entry point
├── plugin_factory.cpp  # Plugin factory, descriptors, extensions
├── plugin_instance.cpp # Per-plugin WASM instance wrapper
├── runtime.cpp         # Wasmtime engine singleton, AOT caching
├── scanner.cpp         # .clasp bundle discovery, JSON parsing
└── gui.cpp             # CHOC WebView integration

include/clasp/          # Headers
examples/               # Example WASM plugins (C++, Rust)
cmake/                  # CMake modules (FetchWasmtime, FetchWasiSdk)
extern/                 # Git submodules (clap, clap-helpers, choc)
```

## DSP ABI

> **Migration in progress**: CLASP is adopting the [WCLAP ABI](https://github.com/WebCLAP/wclap-cpp) for full CLAP compatibility. The legacy ABI below is being replaced.

### WCLAP ABI (new)
WASM modules export `clap_entry` and implement the full CLAP plugin interface. See [WCLAP.md](WCLAP.md) for details and upstream proposals.

### Legacy ABI (deprecated)
The following custom ABI is being phased out:

```c
// Required
void dsp_init(float sample_rate, int max_block_size);
void dsp_process(int block_size);
void dsp_set_param(int param_id, float value);
float dsp_get_param(int param_id);
float* dsp_get_input_buffer(int channel);
float* dsp_get_output_buffer(int channel);

// Optional
void dsp_reset();
int dsp_get_state_size();
void dsp_get_state(uint8_t* out);
void dsp_set_state(const uint8_t* in);

// Instruments only
void dsp_note_on(int32_t offset, int16_t note_id, int16_t channel, int16_t key, float velocity);
void dsp_note_off(int32_t offset, int16_t note_id, int16_t channel, int16_t key, float velocity);
void dsp_note_expression(int32_t offset, int16_t note_id, int16_t channel, int16_t key, int32_t expression_id, float value);
```

## Build Commands

```bash
make install          # Build + sign + install to ~/Library/Audio/Plug-Ins/CLAP/
make clean && make    # Full rebuild

# Example plugins
cd examples/gain-cpp && cmake -B build && cmake --build build
```

## Key Dependencies

| Dependency | Purpose | Location |
|------------|---------|----------|
| Wasmtime | WASM runtime | Auto-fetched to build/_deps/ |
| wasi-sdk | WASM compiler | Auto-fetched for examples |
| CLAP | Plugin API | extern/clap (submodule) |
| CHOC | WebView + JSON | extern/choc (submodule) |

## Plugin Discovery Paths

1. `~/.clasp/plugins/`
2. Same directory as `clasp.clap`
3. `CLASP_PLUGIN_PATH` environment variable

## AOT Cache

Compiled WASM cached at `~/.clasp/cache/<name>_<hash>.cwasm`

## Common Issues

- **std::atomic in vector**: Atomics aren't copyable; use `std::unique_ptr<std::atomic<T>[]>` instead
- **CHOC WebView has no setSize()**: Size managed by parent window via gui_cocoa.mm
- **macOS quarantine**: `make install` handles xattr removal and ad-hoc signing  
- **CHOC value types**: JavaScript numbers may arrive as int32, int64, or float64. Use `isInt32()`, `isInt64()`, `isFloat64()` checks with `get<T>()` template

## WebView/GUI Development

### JavaScript API

The WebView exposes these bindings:

```javascript
clasp.setParam(id, value)     // Set parameter (flows to DSP)
clasp.getParam(id)            // Get parameter value
clasp.getPluginInfo()         // Get plugin metadata
clasp.onParamChange = fn      // Callback for host automation
```

### Developer Tools

**Right-click in the WebView** to open browser Developer Tools (console, network, DOM inspector). This is enabled via `options.enableDebugMode = true` in gui.cpp.

### UI Best Practices

```css
html, body {
    user-select: none;           /* Disable text selection */
    -webkit-user-select: none;
    overflow: hidden;            /* Disable scrolling */
    overscroll-behavior: none;
}
```

## CLI Tools

### clasp-tool
The unified CLI for CLASP development:

```bash
./build/clasp-tool create "My Synth"   # Scaffolds a new plugin project
./build/clasp-tool warm                 # Warms up the AOT cache
```

The `create` command generates a modern C++/WASM template with a responsive HTML/JS UI and MPE-ready DSP boilerplate.

## Version

Current: `0.1.0-alpha` (see VERSION file)

## Testing

No test suite yet. Manual testing:
1. Build and install clasp.clap
2. Build example plugin, copy to ~/.clasp/plugins/
3. Run `clasp-precompile` to warm cache
4. Open DAW, scan plugins, look for "Plugin Name (clasp)"
5. Open GUI, move controls, verify parameter changes affect audio

## Future Work

### High Priority
- **Background pre-compilation during scan**: While the CLI tool exists, move this to a background thread inside `factory_init()` for an even smoother experience.
- **Improved MIDI Mapping**: Persistent storage for MIDI learned mappings.

### Medium Priority  
- **Wayland Native Support**: While discovery is implemented, raw surface management for Wayland subsurfaces needs a native Linux testbed.
- **Preset management in GUI**: JavaScript API for saving/loading presets.
- **Parameter groups**: Support for organizing parameters in the UI.

### Low Priority
- **WebView hot reload**: Watch ui/ folder and reload WebView on file changes during development.
- **WebView size constraints**: Properly handle `canResize()` and `adjustSize()` for resizable UIs.

