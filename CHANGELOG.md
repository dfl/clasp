# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0-beta] - 2026-01-04

### Added
- New `clasp-tool` CLI for plugin generation and cache management
- Scaffolding support for multiple languages (C++, Rust, AssemblyScript)
- Externalized project templates for better customization
- MIDI Polyphonic Expression (MPE) and Note Expression support
- Expanded DSP ABI with `dsp_note_expression`
- Added comprehensive `CONTRIBUTING.md` and upgraded documentation
- Improved build system integration for templates

## [0.1.0-alpha] - 2026-01-03

### Added
- Initial alpha release
- Native CLAP wrapper with Wasmtime runtime
- WASM DSP ABI for effects and instruments
  - `dsp_init`, `dsp_process`, `dsp_reset`
  - `dsp_set_param`, `dsp_get_param`
  - `dsp_get_input_buffer`, `dsp_get_output_buffer`
  - `dsp_note_on`, `dsp_note_off` (instruments)
  - `dsp_get_state`, `dsp_set_state`, `dsp_get_state_size`
- Plugin discovery from `~/.clasp/plugins/`, sibling directory, and `CLASP_PLUGIN_PATH`
- AOT caching for fast subsequent loads (`~/.clasp/cache/`)
- WASM SIMD support for DSP performance
- WebView GUI support via CHOC (HTML/CSS/JS interfaces)
- JavaScript bridge API (`clasp.setParam`, `clasp.getParam`, etc.)
- Cross-platform build system (macOS, Linux, Windows)
- Auto-fetch of Wasmtime and wasi-sdk dependencies
- Example plugins: gain-cpp, gain-rust, synth-cpp

### Known Issues
- WebView embedding not yet tested on Windows/Linux
- Floating window mode not supported
- No per-sample parameter smoothing

[Unreleased]: https://github.com/anthropics/clasp/compare/v0.1.0-alpha...HEAD
[0.1.0-alpha]: https://github.com/anthropics/clasp/releases/tag/v0.1.0-alpha
