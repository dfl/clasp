/**
 * Simple Gain Plugin - CLASP DSP Example (C++)
 *
 * This demonstrates the minimal CLASP DSP ABI for an audio effect.
 * Compile to WASM using wasi-sdk:
 *
 *   clang++ --target=wasm32-wasi -O3 -nostdlib -Wl,--no-entry \
 *           -Wl,--export-all -o dsp.wasm gain.cpp
 */

#include <cstdint>

// WASM export macro
#define EXPORT extern "C" __attribute__((visibility("default")))

// Audio configuration
static constexpr int NUM_CHANNELS = 2;
static constexpr int MAX_BLOCK_SIZE = 4096;

// State
static float g_sampleRate = 44100.0f;
static int g_maxBlockSize = 512;
static float g_gain = 1.0f;

// Audio buffers (in WASM linear memory)
static float g_inputBuffers[NUM_CHANNELS][MAX_BLOCK_SIZE];
static float g_outputBuffers[NUM_CHANNELS][MAX_BLOCK_SIZE];

// State for save/restore
struct PluginState {
    float gain;
};
static PluginState g_state;

//-----------------------------------------------------------------------------
// CLASP DSP ABI Implementation
//-----------------------------------------------------------------------------

EXPORT void dsp_init(float sampleRate, int maxBlockSize) {
    g_sampleRate = sampleRate;
    g_maxBlockSize = maxBlockSize > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : maxBlockSize;
    g_gain = 1.0f;
}

EXPORT void dsp_reset() {
    // Reset any internal state (none for this simple plugin)
}

EXPORT void dsp_process(int blockSize) {
    // Apply gain to each channel
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        for (int i = 0; i < blockSize; ++i) {
            g_outputBuffers[ch][i] = g_inputBuffers[ch][i] * g_gain;
        }
    }
}

EXPORT void dsp_set_param(int paramId, float value) {
    switch (paramId) {
        case 0: g_gain = value; break;
    }
}

EXPORT float dsp_get_param(int paramId) {
    switch (paramId) {
        case 0: return g_gain;
        default: return 0.0f;
    }
}

EXPORT float* dsp_get_input_buffer(int channel) {
    if (channel < 0 || channel >= NUM_CHANNELS) return nullptr;
    return g_inputBuffers[channel];
}

EXPORT float* dsp_get_output_buffer(int channel) {
    if (channel < 0 || channel >= NUM_CHANNELS) return nullptr;
    return g_outputBuffers[channel];
}

EXPORT int dsp_get_state_size() {
    return sizeof(PluginState);
}

EXPORT void dsp_get_state(uint8_t* out) {
    g_state.gain = g_gain;
    auto* state = reinterpret_cast<PluginState*>(out);
    *state = g_state;
}

EXPORT void dsp_set_state(const uint8_t* in) {
    auto* state = reinterpret_cast<const PluginState*>(in);
    g_gain = state->gain;
}
