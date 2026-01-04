# CLASP - CLAP WebAssembly Plugin Host

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0--beta-green.svg)](https://github.com/dfl/clasp)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey.svg)](https://github.com/dfl/clasp)

> **Native CLAP wrapper for WASM — ✨ Compile once, run everywhere!**

**CLASP** (CLAP + WASM) is a performance-focused host for WebAssembly audio modules. As a developer with a deep love for both high-end DSP and web technologies, I built CLASP to bridge the gap between native performance and the ease of modern web development. 

The goal is simple: write your audio logic once in C++, Rust, Zig, or AssemblyScript, and have it "just work" in any DAW across macOS, Windows, and Linux.

---

### Development Status
We're currently in **1.0.0-beta**. I'm actively expanding support for more CLAP extensions and refining the cross-platform experience. If you find a bug or have a feature idea, I'd love to hear from you. Check out [CONTRIBUTING.md](CONTRIBUTING.md).

#### 🗺️ Roadmap
- **v1.0.0**: Release final stable build with existing WebView implementation.
- **v1.0.1**: Implement the draft `CLAP_EXT_WEBVIEW` extension for standardized, host-managed web UIs.

## 🚀 Download & Install

For most users, I recommend downloading the pre-built binaries for your platform. 

**[Get the latest CLASP release here](https://github.com/dfl/clasp/releases)**

1.  **Download** the `.zip` or `.tar.gz` for your operating system.
2.  **Extract** the `clasp.clap` bundle and `clasp-tool`.
3.  **Install** `clasp.clap` by copying it to your standard CLAP folder:
    *   **macOS**: `~/Library/Audio/Plug-Ins/CLAP`
    *   **Windows**: `%LOCALAPPDATA%\Programs\Common\CLAP`
    *   **Linux**: `~/.clap`
4.  **Verify**: Open your DAW and scan for new plugins. You should see "CLASP" as a loaded plugin.

```
┌─────────────────────────────────────────────────────────────┐
│                      Your DAW (CLAP Host)                   │
│                    (Bitwig, REAPER, etc.)                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   clasp.clap (Native Wrapper)               │
│                                                             │
│     Scanner        Wasmtime Engine        WebView GUI       │
│  (finds .clasp)    (JIT + AOT cache)        (CHOC)          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      YourPlugin.clasp                       │
│     plugin.json  +  dsp.wasm  +  ui/index.html (optional)   │
└─────────────────────────────────────────────────────────────┘
```

---

## What makes CLASP different?

I've always been frustrated by the friction of cross-platform audio development. CLASP solves this by bringing modern runtime technology to the audio world:

- **Unified Binary**: You install `clasp.clap` once. It acts as a dynamic host that finds and loads all your `.clasp` bundles on the fly.
- **Portability by Design**: Your DSP logic lives in WebAssembly. No more managing three different toolchains just to share a plugin with a friend on another OS.
- **Wasmtime JIT**: Performance is critical. CLASP uses Wasmtime to JIT-compile your code with SIMD support. It also caches the native result for near-instant startup on subsequent loads.
- **HTML5 UIs**: Skip the complicated C++ GUI frameworks. Build your interface with the tools you already know: HTML, CSS, and standard JavaScript.
- **Expressive Control**: Full support for MIDI, CCs, and MIDI Polyphonic Expression (MPE).

---

## Quick Start

### 1. Get the Native Wrapper

You need the `clasp.clap` host to run your plugins. You can either:

*   **Download pre-built**: Grab the latest from [Releases](https://github.com/dfl/clasp/releases).
*   **Build from source** (Requires CMake and a C++ compiler):

```bash
# Clone with submodules
git clone --recursive https://github.com/dfl/clasp.git
cd clasp

# Build and install to your CLAP plugins folder
make install
```

### 2. Scaffold a New Plugin (`clasp-tool`)

I've included a helper tool called `clasp-tool` to get you started immediately with a working template. From your build directory:

```bash
# Generate a new plugin (defaults to C++)
./build/clasp-tool create "My Gain Effect"

# Or try another language
./build/clasp-tool create --lang rust "Rust Synth"
./build/clasp-tool create --lang as "AssemblyScript FX"
```

This creates a project structure containing your DSP code, a responsive UI, the `plugin.json` manifest, and a build script to generate the final `.wasm`.

### 3. Manual Plugin Structure

If you prefer to build from scratch, a `.clasp` plugin is just a folder containing:

```
MyPlugin.clasp/
├── plugin.json     # Metadata and parameter definitions
├── dsp.wasm        # Your DSP logic (compiled WASM)
└── ui/             # (Optional) HTML UI directory
    └── index.html
```

#### Example `plugin.json`

```json
{
  "id": "com.example.gain",
  "name": "Simple Gain",
  "vendor": "Dev Name",
  "version": "1.0.0",
  "type": "effect",
  "audio": { "inputs": 2, "outputs": 2 },
  "parameters": [
    { "id": 0, "name": "Gain", "min": 0, "max": 2, "default": 1 }
  ]
}
```

#### Compile your logic (using wasi-sdk)

```bash
clang++ --target=wasm32-wasi -O3 -msimd128 \
        -nostdlib -Wl,--no-entry -Wl,--export-dynamic \
        -o dsp.wasm dsp.cpp
```

Once built, move your `.clasp` folder to `~/.clasp/plugins/` (or the folder where `clasp.clap` resides) and it will be discovered by your DAW.

---

## DSP ABI Reference

To talk to the host, your WASM module should export these functions:

### Required Exports
```c
// Lifecycle
void dsp_init(float sample_rate, int max_block_size);
void dsp_reset();

// Main processing loop
void dsp_process(int block_size);

// Parameters
void dsp_set_param(int param_id, float value);
float dsp_get_param(int param_id);

// Audio Buffer access (returning pointers into WASM linear memory)
float* dsp_get_input_buffer(int channel);
float* dsp_get_output_buffer(int channel);

// Optional State Persistence
int dsp_get_state_size();
void dsp_get_state(uint8_t* out);
void dsp_set_state(const uint8_t* in);
```

### Instruments (MPE Support)
```c
void dsp_note_on(int32_t sample_offset, int16_t note_id, int16_t channel, int16_t key, float velocity);
void dsp_note_off(int32_t sample_offset, int16_t note_id, int16_t channel, int16_t key, float velocity);
void dsp_note_expression(int32_t offset, int16_t note_id, int16_t channel, int16_t key, int32_t expr_id, float value);
```

---

## UI Development

The UI is a simple WebView. You can access the host via the `clasp` object in vanilla JavaScript:

```javascript
// Change a parameter from the UI
clasp.setParam(0, 0.5);

// Listen for updates from the host (automation, etc)
clasp.onParamChange = (id, value) => {
    console.log(`Param ${id} is now ${value}`);
};
```

I recommend using [Vite](https://vitejs.dev/) if you want a more modern TypeScript/React/Vue setup for your interface. Just set the build output to your `ui/` folder.

---

## Contributing

I'm a big fan of collaboration, pair programming, and rigorous testing. If you're interested in making CLASP better, please check out the [CONTRIBUTING.md](CONTRIBUTING.md) guide.

## Author(s)

**David Lowenfels**  
Creative developer, DSP enthusiast, and polymath based in the UK.

## License

MIT License. See [LICENSE](LICENSE) for details.
