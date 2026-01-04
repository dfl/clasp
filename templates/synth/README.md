# {{PLUGIN_NAME}}

A WCLAP polyphonic synthesizer plugin.

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

## Features

- 16-voice polyphony with voice stealing
- ADSR envelope (Attack, Decay, Sustain, Release)
- Simple lowpass filter
- Virtual keyboard UI with computer keyboard support
- MIDI input via CLAP note ports

## Structure

```
{{PLUGIN_NAME_SNAKE}}/
├── CMakeLists.txt      # Build configuration
├── cpp/
│   └── src/
│       └── plugin.cpp  # CLAP synthesizer implementation
└── ui/
    ├── index.html      # Plugin UI with ADSR & keyboard
    └── clasp.js        # Host communication library
```
