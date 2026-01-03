# CLASP - CLAP WebAssembly Plugin Host

> **Native CLAP wrapper for WASM — compile once, run everywhere!**

**CLASP** (CLAP + WASM = CLASP) is a native CLAP plugin that hosts WebAssembly DSP modules. Write your audio code once in C++, Rust, or any WASM-targeting language, and run it in any DAW on any platform.

```
┌─────────────────────────────────────────────────────────────┐
│                      Your DAW (CLAP Host)                    │
│                    (Bitwig, REAPER, etc.)                    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   clasp.clap (Native Wrapper)                │
│                                                              │
│     Scanner        Wasmtime Engine        WebView GUI        │
│  (finds .clasp)    (JIT + AOT cache)        (CHOC)           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      YourPlugin.clasp                        │
│     plugin.json  +  dsp.wasm  +  ui/index.html (optional)    │
└─────────────────────────────────────────────────────────────┘
```

## Features

- **One binary, many plugins**: Single `clasp.clap` loads all your WASM plugins
- **Cross-platform DSP**: Compile once to WASM, runs on macOS/Windows/Linux
- **Fast**: Wasmtime JIT with SIMD support, AOT caching for instant reload
- **Web UI**: Optional HTML/CSS/JS interface via embedded WebView
- **Effects & Instruments**: Full support for audio effects and synthesizers

## Quick Start

### Building the Native Wrapper

```bash
# Clone with submodules
git clone --recursive https://github.com/dflowenfels/clasp.git
cd clasp

# Build and install (macOS/Linux)
make install

# Or build for specific architecture
make release ARCH=universal   # macOS: fat binary (x86_64 + arm64)
make release ARCH=arm64       # macOS: Apple Silicon only
```

### Creating a WASM Plugin

1. Create a `.clasp` bundle:

```
MyPlugin.clasp/
├── plugin.json     # Plugin metadata
├── dsp.wasm        # Your DSP code compiled to WASM
└── ui/             # Optional HTML UI
    └── index.html
```

2. Write `plugin.json`:

```json
{
  "id": "com.yourname.myplugin",
  "name": "My Plugin",
  "vendor": "Your Name",
  "version": "1.0.0",
  "type": "effect",
  "audio": { "inputs": 2, "outputs": 2 },
  "parameters": [
    { "id": 0, "name": "Gain", "min": 0, "max": 2, "default": 1 }
  ]
}
```

3. Implement the DSP ABI:

```cpp
extern "C" {
    void dsp_init(float sampleRate, int maxBlockSize);
    void dsp_process(int blockSize);
    void dsp_set_param(int paramId, float value);
    float* dsp_get_input_buffer(int channel);
    float* dsp_get_output_buffer(int channel);
}
```

4. Compile to WASM (using wasi-sdk):

```bash
clang++ --target=wasm32-wasi -O3 -msimd128 \
        -nostdlib -Wl,--no-entry -Wl,--export-dynamic \
        -o dsp.wasm myplugin.cpp
```

5. Place your `.clasp` bundle in `~/.clasp/plugins/`

### Building the Example Plugin

```bash
cd examples/gain-cpp
cmake -B build
cmake --build build

# Install to plugins folder
cp -R build/SimpleGain.clasp ~/.clasp/plugins/
```

## DSP ABI Reference

### Required Exports (Effects & Instruments)

```c
// Lifecycle
void dsp_init(float sample_rate, int max_block_size);
void dsp_reset();

// Processing
void dsp_process(int block_size);

// Parameters
void dsp_set_param(int param_id, float value);
float dsp_get_param(int param_id);

// Audio Buffers (pointers into WASM linear memory)
float* dsp_get_input_buffer(int channel);
float* dsp_get_output_buffer(int channel);

// State Persistence (optional)
int dsp_get_state_size();
void dsp_get_state(uint8_t* out);
void dsp_set_state(const uint8_t* in);
```

### Additional Exports (Instruments Only)

```c
void dsp_note_on(int32_t sample_offset, int16_t note_id,
                 int16_t channel, int16_t key, float velocity);
void dsp_note_off(int32_t sample_offset, int16_t note_id,
                  int16_t channel, int16_t key, float velocity);
```

## Plugin Discovery

CLASP scans for `.clasp` bundles in:

1. `~/.clasp/plugins/`
2. Same directory as `clasp.clap`
3. Paths in `CLASP_PLUGIN_PATH` environment variable (colon-separated)

Discovered plugins appear in your DAW as "Plugin Name (clasp)".

## UI Development

Create `ui/index.html` in your bundle:

```html
<!DOCTYPE html>
<html>
<body>
    <input type="range" id="gain" min="0" max="2" step="0.01">
    <script>
        document.getElementById('gain').addEventListener('input', (e) => {
            clasp.setParam(0, parseFloat(e.target.value));
        });

        window.onClaspReady = () => {
            document.getElementById('gain').value = clasp.getParam(0);
            clasp.onParamChange = (id, value) => {
                if (id === 0) document.getElementById('gain').value = value;
            };
        };
    </script>
</body>
</html>
```

### JavaScript API

```javascript
clasp.setParam(id, value)     // Set parameter value
clasp.getParam(id)            // Get parameter value
clasp.getPluginInfo()         // Get plugin metadata
clasp.onParamChange = fn      // Callback for automation updates
```

## AOT Caching

First load compiles WASM to native code (JIT). Subsequent loads use cached `.cwasm` files from `~/.clasp/cache/` for instant startup.

## Examples

See `examples/` for complete implementations:

- `gain-cpp/` - Simple stereo gain effect (C++)
- `gain-rust/` - Same effect in Rust
- `synth-cpp/` - Polyphonic synthesizer (C++)

## Dependencies

| Dependency | Purpose | Source |
|------------|---------|--------|
| [Wasmtime](https://wasmtime.dev/) | WASM runtime (JIT/AOT) | Auto-fetched |
| [CLAP](https://cleveraudio.org/) | Plugin API | Git submodule |
| [clap-helpers](https://github.com/free-audio/clap-helpers) | C++ helpers | Git submodule |
| [CHOC](https://github.com/Tracktion/choc) | WebView + utilities | Git submodule |
| [wasi-sdk](https://github.com/WebAssembly/wasi-sdk) | WASM compiler | Auto-fetched (examples) |

## Building from Source

### Requirements

- CMake 3.16+
- C++17 compiler (Clang, GCC, MSVC)
- Git (for submodules)

### Build Commands

```bash
make                  # Build release for current platform
make debug            # Build with debug symbols
make install          # Build, sign, and install to CLAP folder
make package          # Create distributable archive
make clean            # Remove build artifacts
```

### Cross-Platform Builds

```bash
# macOS
make release ARCH=universal    # Fat binary (Intel + Apple Silicon)

# Linux
make release                   # Native build

# Windows (use Developer Command Prompt)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Author

David Lowenfels

## License

MIT License - see [LICENSE](LICENSE) for details.
