# Minimal WCLAP Template

A simple gain plugin template for WCLAP development.

## Structure

```
my-plugin.wclap/
├── manifest.json      # Plugin metadata
├── dsp.wasm          # Compiled DSP module
└── ui/
    ├── index.html    # Plugin UI
    └── clasp.js      # Communication library
```

## Building the DSP

### Rust (recommended)
```bash
cargo build --target wasm32-wasi --release
cp target/wasm32-wasi/release/*.wasm ./dsp.wasm
```

### C++ (with wasi-sdk)
```bash
$WASI_SDK/bin/clang++ -O3 -o dsp.wasm src/dsp.cpp
```

## Testing

1. Copy the `.wclap` bundle to your CLAP plugin directory
2. Load `clasp.clap` in your DAW
3. The plugin should appear in the instrument/effect list

## Hot Reload (Development)

Set `CLASP_HOT_RELOAD=1` to enable automatic reloading when files change.
