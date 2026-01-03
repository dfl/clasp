/**
 * Simple Synth - CLASP DSP Instrument Example (C++)
 *
 * A basic polyphonic synthesizer demonstrating:
 * - Note events (note on/off)
 * - Multiple parameters
 * - Simple oscillator + filter
 *
 * Compile with wasi-sdk:
 *   clang++ --target=wasm32-wasi -O3 -nostdlib -Wl,--no-entry \
 *           -Wl,--export-all -msimd128 -o dsp.wasm synth.cpp
 */

#include <cstdint>
#include <cmath>

#define EXPORT extern "C" __attribute__((visibility("default")))

// Constants
constexpr int NUM_CHANNELS = 2;
constexpr int MAX_BLOCK_SIZE = 4096;
constexpr int MAX_VOICES = 16;
constexpr float PI = 3.14159265358979f;
constexpr float TWO_PI = 2.0f * PI;

// Voice state
struct Voice {
    bool active = false;
    int16_t noteId = -1;
    int16_t key = 0;
    float velocity = 0.0f;
    float phase = 0.0f;
    float phaseInc = 0.0f;
    float envLevel = 0.0f;
    float envStage = 0; // 0=off, 1=attack, 2=sustain, 3=release
    float releaseLevel = 0.0f;
};

// Plugin state
static float g_sampleRate = 44100.0f;
static int g_maxBlockSize = 512;

// Parameters
static float g_cutoff = 2000.0f;
static float g_resonance = 0.5f;
static float g_attack = 0.01f;
static float g_release = 0.3f;

// Voices
static Voice g_voices[MAX_VOICES];

// Filter state (per-voice simplified)
static float g_filterZ1[MAX_VOICES] = {0};
static float g_filterZ2[MAX_VOICES] = {0};

// Output buffers
static float g_outputBuffers[NUM_CHANNELS][MAX_BLOCK_SIZE];
static float g_inputBuffers[NUM_CHANNELS][MAX_BLOCK_SIZE]; // Not used for instruments

// State for save/load
struct PluginState {
    float cutoff;
    float resonance;
    float attack;
    float release;
};
static PluginState g_state;

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

static float midiToFreq(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

static int findFreeVoice() {
    // Find inactive voice
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!g_voices[i].active) return i;
    }
    // Steal oldest voice (simple strategy)
    return 0;
}

static int findVoice(int16_t noteId, int16_t key) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (g_voices[i].active &&
            (noteId == -1 ? g_voices[i].key == key : g_voices[i].noteId == noteId)) {
            return i;
        }
    }
    return -1;
}

//-----------------------------------------------------------------------------
// CLASP DSP ABI Implementation
//-----------------------------------------------------------------------------

EXPORT void dsp_init(float sampleRate, int maxBlockSize) {
    g_sampleRate = sampleRate;
    g_maxBlockSize = maxBlockSize > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : maxBlockSize;

    // Reset voices
    for (int i = 0; i < MAX_VOICES; ++i) {
        g_voices[i] = Voice{};
        g_filterZ1[i] = 0;
        g_filterZ2[i] = 0;
    }
}

EXPORT void dsp_reset() {
    for (int i = 0; i < MAX_VOICES; ++i) {
        g_voices[i] = Voice{};
        g_filterZ1[i] = 0;
        g_filterZ2[i] = 0;
    }
}

EXPORT void dsp_process(int blockSize) {
    // Clear output buffers
    for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
        for (int i = 0; i < blockSize; ++i) {
            g_outputBuffers[ch][i] = 0.0f;
        }
    }

    // Filter coefficients (simplified one-pole)
    float cutoffNorm = g_cutoff / g_sampleRate;
    if (cutoffNorm > 0.49f) cutoffNorm = 0.49f;
    float filterCoeff = 1.0f - expf(-TWO_PI * cutoffNorm);

    // Envelope timing
    float attackRate = 1.0f / (g_attack * g_sampleRate);
    float releaseRate = 1.0f / (g_release * g_sampleRate);

    // Process each voice
    for (int v = 0; v < MAX_VOICES; ++v) {
        Voice& voice = g_voices[v];
        if (!voice.active) continue;

        for (int i = 0; i < blockSize; ++i) {
            // Envelope
            if (voice.envStage == 1) { // Attack
                voice.envLevel += attackRate;
                if (voice.envLevel >= 1.0f) {
                    voice.envLevel = 1.0f;
                    voice.envStage = 2; // Sustain
                }
            } else if (voice.envStage == 3) { // Release
                voice.envLevel = voice.releaseLevel * (1.0f - (voice.envLevel * releaseRate));
                voice.envLevel -= releaseRate;
                if (voice.envLevel <= 0.0f) {
                    voice.envLevel = 0.0f;
                    voice.active = false;
                    continue;
                }
            }

            // Sawtooth oscillator
            float osc = 2.0f * voice.phase - 1.0f;
            voice.phase += voice.phaseInc;
            if (voice.phase >= 1.0f) voice.phase -= 1.0f;

            // Simple lowpass filter
            g_filterZ1[v] += filterCoeff * (osc - g_filterZ1[v]);
            float filtered = g_filterZ1[v];

            // Apply envelope and velocity
            float sample = filtered * voice.envLevel * voice.velocity * 0.3f;

            // Output to both channels
            g_outputBuffers[0][i] += sample;
            g_outputBuffers[1][i] += sample;
        }
    }
}

EXPORT void dsp_set_param(int paramId, float value) {
    switch (paramId) {
        case 0: g_cutoff = value; break;
        case 1: g_resonance = value; break;
        case 2: g_attack = value; break;
        case 3: g_release = value; break;
    }
}

EXPORT float dsp_get_param(int paramId) {
    switch (paramId) {
        case 0: return g_cutoff;
        case 1: return g_resonance;
        case 2: return g_attack;
        case 3: return g_release;
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
    g_state.cutoff = g_cutoff;
    g_state.resonance = g_resonance;
    g_state.attack = g_attack;
    g_state.release = g_release;
    auto* state = reinterpret_cast<PluginState*>(out);
    *state = g_state;
}

EXPORT void dsp_set_state(const uint8_t* in) {
    auto* state = reinterpret_cast<const PluginState*>(in);
    g_cutoff = state->cutoff;
    g_resonance = state->resonance;
    g_attack = state->attack;
    g_release = state->release;
}

//-----------------------------------------------------------------------------
// Instrument ABI (note events)
//-----------------------------------------------------------------------------

EXPORT void dsp_note_on(int32_t sampleOffset, int16_t noteId, int16_t channel,
                        int16_t key, float velocity) {
    int v = findFreeVoice();

    Voice& voice = g_voices[v];
    voice.active = true;
    voice.noteId = noteId;
    voice.key = key;
    voice.velocity = velocity;
    voice.phase = 0.0f;
    voice.phaseInc = midiToFreq(key) / g_sampleRate;
    voice.envLevel = 0.0f;
    voice.envStage = 1; // Attack

    // Reset filter for this voice
    g_filterZ1[v] = 0;
    g_filterZ2[v] = 0;
}

EXPORT void dsp_note_off(int32_t sampleOffset, int16_t noteId, int16_t channel,
                         int16_t key, float velocity) {
    int v = findVoice(noteId, key);
    if (v >= 0) {
        Voice& voice = g_voices[v];
        voice.releaseLevel = voice.envLevel;
        voice.envStage = 3; // Release
    }
}

EXPORT void dsp_note_expression(int32_t noteId, int32_t expressionId, float value) {
    // Future: handle per-note expression (pressure, slide, etc.)
}
