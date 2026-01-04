// {{PLUGIN_NAME}} - WCLAP Synthesizer
// Minimal polyphonic CLAP synth compiled to WebAssembly

#include <clap/clap.h>
#include <cstring>
#include <cmath>

static constexpr int MAX_VOICES = 16;
static constexpr float TWO_PI = 6.283185307179586f;

// Voice state
struct Voice {
    bool active = false;
    int32_t note_id = -1;
    int16_t channel = 0;
    int16_t key = 0;
    float velocity = 0.0f;
    float phase = 0.0f;
    float env = 0.0f;
    float env_stage = 0; // 0=off, 1=attack, 2=decay, 3=sustain, 4=release
    float release_level = 0.0f;
};

// Plugin state
static Voice g_voices[MAX_VOICES];
static float g_sample_rate = 44100.0f;

// Parameters
static float g_attack = 0.01f;   // seconds
static float g_decay = 0.1f;    // seconds
static float g_sustain = 0.7f;  // level
static float g_release = 0.3f;  // seconds
static float g_cutoff = 1.0f;   // filter (0-1)

enum ParamId {
    PARAM_ATTACK = 0,
    PARAM_DECAY = 1,
    PARAM_SUSTAIN = 2,
    PARAM_RELEASE = 3,
    PARAM_CUTOFF = 4
};

// Plugin descriptor
static const clap_plugin_descriptor_t s_descriptor = {
    .clap_version = CLAP_VERSION,
    .id = "{{PLUGIN_ID}}",
    .name = "{{PLUGIN_NAME}}",
    .vendor = "{{PLUGIN_VENDOR}}",
    .url = "",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "A simple polyphonic synthesizer",
    .features = (const char*[]){CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, nullptr}
};

//-----------------------------------------------------------------------------
// DSP Helpers
//-----------------------------------------------------------------------------

static float noteToFreq(int key) {
    return 440.0f * powf(2.0f, (key - 69) / 12.0f);
}

static Voice* findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!g_voices[i].active) return &g_voices[i];
    }
    // Steal oldest voice
    return &g_voices[0];
}

static Voice* findVoice(int32_t note_id, int16_t channel, int16_t key) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (g_voices[i].active &&
            (note_id == -1 || g_voices[i].note_id == note_id) &&
            g_voices[i].channel == channel &&
            g_voices[i].key == key) {
            return &g_voices[i];
        }
    }
    return nullptr;
}

static void processEnvelope(Voice& v, float dt) {
    float attack_rate = g_attack > 0.001f ? 1.0f / (g_attack * g_sample_rate) : 1.0f;
    float decay_rate = g_decay > 0.001f ? 1.0f / (g_decay * g_sample_rate) : 1.0f;
    float release_rate = g_release > 0.001f ? 1.0f / (g_release * g_sample_rate) : 1.0f;

    switch ((int)v.env_stage) {
        case 1: // Attack
            v.env += attack_rate;
            if (v.env >= 1.0f) {
                v.env = 1.0f;
                v.env_stage = 2;
            }
            break;
        case 2: // Decay
            v.env -= decay_rate * (1.0f - g_sustain);
            if (v.env <= g_sustain) {
                v.env = g_sustain;
                v.env_stage = 3;
            }
            break;
        case 3: // Sustain
            v.env = g_sustain;
            break;
        case 4: // Release
            v.env -= release_rate * v.release_level;
            if (v.env <= 0.0f) {
                v.env = 0.0f;
                v.active = false;
                v.env_stage = 0;
            }
            break;
    }
}

//-----------------------------------------------------------------------------
// Plugin implementation
//-----------------------------------------------------------------------------

static bool plugin_init(const clap_plugin_t* plugin) {
    return true;
}

static void plugin_destroy(const clap_plugin_t* plugin) {
}

static bool plugin_activate(const clap_plugin_t* plugin, double sample_rate,
                           uint32_t min_frames, uint32_t max_frames) {
    g_sample_rate = (float)sample_rate;
    return true;
}

static void plugin_deactivate(const clap_plugin_t* plugin) {
}

static bool plugin_start_processing(const clap_plugin_t* plugin) {
    return true;
}

static void plugin_stop_processing(const clap_plugin_t* plugin) {
}

static void plugin_reset(const clap_plugin_t* plugin) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        g_voices[i] = Voice{};
    }
}

static void handleNoteOn(int32_t note_id, int16_t channel, int16_t key, float velocity) {
    Voice* v = findFreeVoice();
    v->active = true;
    v->note_id = note_id;
    v->channel = channel;
    v->key = key;
    v->velocity = velocity;
    v->phase = 0.0f;
    v->env = 0.0f;
    v->env_stage = 1; // Attack
}

static void handleNoteOff(int32_t note_id, int16_t channel, int16_t key) {
    Voice* v = findVoice(note_id, channel, key);
    if (v && v->active) {
        v->release_level = v->env;
        v->env_stage = 4; // Release
    }
}

static clap_process_status plugin_process(const clap_plugin_t* plugin,
                                          const clap_process_t* process) {
    const uint32_t num_frames = process->frames_count;

    // Process events
    const clap_input_events_t* in_events = process->in_events;
    uint32_t event_idx = 0;
    uint32_t next_event_frame = 0;

    if (in_events->size(in_events) > 0) {
        next_event_frame = in_events->get(in_events, 0)->time;
    } else {
        next_event_frame = num_frames;
    }

    for (uint32_t frame = 0; frame < num_frames; ) {
        // Process events at this frame
        while (event_idx < in_events->size(in_events) && next_event_frame == frame) {
            const clap_event_header_t* event = in_events->get(in_events, event_idx);

            if (event->type == CLAP_EVENT_NOTE_ON) {
                const clap_event_note_t* note = (const clap_event_note_t*)event;
                handleNoteOn(note->note_id, note->channel, note->key, (float)note->velocity);
            } else if (event->type == CLAP_EVENT_NOTE_OFF) {
                const clap_event_note_t* note = (const clap_event_note_t*)event;
                handleNoteOff(note->note_id, note->channel, note->key);
            } else if (event->type == CLAP_EVENT_PARAM_VALUE) {
                const clap_event_param_value_t* pv = (const clap_event_param_value_t*)event;
                switch (pv->param_id) {
                    case PARAM_ATTACK: g_attack = (float)pv->value; break;
                    case PARAM_DECAY: g_decay = (float)pv->value; break;
                    case PARAM_SUSTAIN: g_sustain = (float)pv->value; break;
                    case PARAM_RELEASE: g_release = (float)pv->value; break;
                    case PARAM_CUTOFF: g_cutoff = (float)pv->value; break;
                }
            }

            ++event_idx;
            if (event_idx < in_events->size(in_events)) {
                next_event_frame = in_events->get(in_events, event_idx)->time;
            } else {
                next_event_frame = num_frames;
            }
        }

        // Process audio until next event
        uint32_t frames_to_process = next_event_frame - frame;

        float* out_l = process->audio_outputs[0].data32[0] + frame;
        float* out_r = process->audio_outputs[0].data32[1] + frame;

        for (uint32_t i = 0; i < frames_to_process; ++i) {
            float sample = 0.0f;

            for (int v = 0; v < MAX_VOICES; ++v) {
                Voice& voice = g_voices[v];
                if (!voice.active) continue;

                float freq = noteToFreq(voice.key);
                float phase_inc = freq / g_sample_rate;

                // Simple saw oscillator
                float osc = voice.phase * 2.0f - 1.0f;

                // Apply envelope and velocity
                processEnvelope(voice, 1.0f);
                sample += osc * voice.env * voice.velocity * 0.3f;

                // Advance phase
                voice.phase += phase_inc;
                if (voice.phase >= 1.0f) voice.phase -= 1.0f;
            }

            // Simple lowpass filter
            static float filter_state = 0.0f;
            float alpha = g_cutoff;
            filter_state += alpha * (sample - filter_state);
            sample = filter_state;

            out_l[i] = sample;
            out_r[i] = sample;
        }

        frame = next_event_frame;
    }

    return CLAP_PROCESS_CONTINUE;
}

static const void* plugin_get_extension(const clap_plugin_t* plugin, const char* id) {
    if (strcmp(id, CLAP_EXT_PARAMS) == 0) {
        static const clap_plugin_params_t params_ext = {
            .count = [](const clap_plugin_t*) -> uint32_t { return 5; },
            .get_info = [](const clap_plugin_t*, uint32_t index, clap_param_info_t* info) -> bool {
                static const struct { clap_id id; const char* name; float min, max, def; } params[] = {
                    {PARAM_ATTACK, "Attack", 0.001f, 2.0f, 0.01f},
                    {PARAM_DECAY, "Decay", 0.001f, 2.0f, 0.1f},
                    {PARAM_SUSTAIN, "Sustain", 0.0f, 1.0f, 0.7f},
                    {PARAM_RELEASE, "Release", 0.001f, 2.0f, 0.3f},
                    {PARAM_CUTOFF, "Cutoff", 0.01f, 1.0f, 1.0f},
                };
                if (index >= 5) return false;
                info->id = params[index].id;
                strncpy(info->name, params[index].name, CLAP_NAME_SIZE);
                info->module[0] = '\0';
                info->min_value = params[index].min;
                info->max_value = params[index].max;
                info->default_value = params[index].def;
                info->flags = CLAP_PARAM_IS_AUTOMATABLE;
                info->cookie = nullptr;
                return true;
            },
            .get_value = [](const clap_plugin_t*, clap_id id, double* value) -> bool {
                switch (id) {
                    case PARAM_ATTACK: *value = g_attack; return true;
                    case PARAM_DECAY: *value = g_decay; return true;
                    case PARAM_SUSTAIN: *value = g_sustain; return true;
                    case PARAM_RELEASE: *value = g_release; return true;
                    case PARAM_CUTOFF: *value = g_cutoff; return true;
                }
                return false;
            },
            .value_to_text = [](const clap_plugin_t*, clap_id id, double value, char* buf, uint32_t size) -> bool {
                if (id == PARAM_SUSTAIN || id == PARAM_CUTOFF) {
                    snprintf(buf, size, "%.0f%%", value * 100);
                } else {
                    snprintf(buf, size, "%.0f ms", value * 1000);
                }
                return true;
            },
            .text_to_value = [](const clap_plugin_t*, clap_id, const char* text, double* value) -> bool {
                *value = atof(text);
                return true;
            },
            .flush = [](const clap_plugin_t*, const clap_input_events_t* in, const clap_output_events_t*) {}
        };
        return &params_ext;
    }

    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        static const clap_plugin_audio_ports_t audio_ports_ext = {
            .count = [](const clap_plugin_t*, bool is_input) -> uint32_t {
                return is_input ? 0 : 1;  // No inputs, one stereo output
            },
            .get = [](const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t* info) -> bool {
                if (is_input || index != 0) return false;
                info->id = 0;
                strncpy(info->name, "Output", CLAP_NAME_SIZE);
                info->channel_count = 2;
                info->flags = CLAP_AUDIO_PORT_IS_MAIN;
                info->port_type = CLAP_PORT_STEREO;
                info->in_place_pair = CLAP_INVALID_ID;
                return true;
            }
        };
        return &audio_ports_ext;
    }

    if (strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) {
        static const clap_plugin_note_ports_t note_ports_ext = {
            .count = [](const clap_plugin_t*, bool is_input) -> uint32_t {
                return is_input ? 1 : 0;  // One note input
            },
            .get = [](const clap_plugin_t*, uint32_t index, bool is_input, clap_note_port_info_t* info) -> bool {
                if (!is_input || index != 0) return false;
                info->id = 0;
                strncpy(info->name, "Note Input", CLAP_NAME_SIZE);
                info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
                info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
                return true;
            }
        };
        return &note_ports_ext;
    }

    return nullptr;
}

static void plugin_on_main_thread(const clap_plugin_t* plugin) {
}

//-----------------------------------------------------------------------------
// Plugin instance
//-----------------------------------------------------------------------------

static clap_plugin_t s_plugin = {
    .desc = &s_descriptor,
    .plugin_data = nullptr,
    .init = plugin_init,
    .destroy = plugin_destroy,
    .activate = plugin_activate,
    .deactivate = plugin_deactivate,
    .start_processing = plugin_start_processing,
    .stop_processing = plugin_stop_processing,
    .reset = plugin_reset,
    .process = plugin_process,
    .get_extension = plugin_get_extension,
    .on_main_thread = plugin_on_main_thread
};

//-----------------------------------------------------------------------------
// Factory & Entry
//-----------------------------------------------------------------------------

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t*) { return 1; }

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t*, uint32_t index) {
    return index == 0 ? &s_descriptor : nullptr;
}

static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t*, const clap_host_t*, const char* plugin_id) {
    return strcmp(plugin_id, s_descriptor.id) == 0 ? &s_plugin : nullptr;
}

static const clap_plugin_factory_t s_factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin
};

static bool entry_init(const char*) { return true; }
static void entry_deinit(void) {}
static const void* entry_get_factory(const char* factory_id) {
    return strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0 ? &s_factory : nullptr;
}

extern "C" {
    CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
        .clap_version = CLAP_VERSION,
        .init = entry_init,
        .deinit = entry_deinit,
        .get_factory = entry_get_factory
    };
}
