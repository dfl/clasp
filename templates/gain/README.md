# {{PLUGIN_NAME}}

A WCLAP audio effect plugin.

## Building

Requires [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases).

```bash
export WASI_SDK_PREFIX=/path/to/wasi-sdk

mkdir build && cd build
cmake ..
make
make install
```

This creates `{{PLUGIN_NAME_SNAKE}}.wclap/` bundle ready for use.

## Development

1. Copy the `.wclap` bundle to `~/.wclap/plugins/`
2. Load in your DAW via thunder.clap
3. Edit `ui/index.html` for UI changes (hot-reload supported)
4. Rebuild for DSP changes

## Structure

```
{{PLUGIN_NAME_SNAKE}}/
├── CMakeLists.txt      # Build configuration
├── cpp/
│   └── src/
│       └── plugin.cpp  # CLAP plugin implementation
└── ui/
    ├── index.html      # Plugin UI
    └── clasp.js        # Host communication library
```
