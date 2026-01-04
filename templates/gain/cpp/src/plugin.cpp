// {{PLUGIN_NAME}} - WCLAP Gain Plugin
// Minimal CLAP plugin compiled to WebAssembly

#include <clap/clap.h>
#include <cstring>
#include <cmath>

// Plugin state
static float g_gain = 0.5f;
static float g_sample_rate = 44100.0f;

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
    .description = "A simple gain plugin",
    .features = (const char*[]){CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_UTILITY, nullptr}
};

// Parameter info
enum ParamId { PARAM_GAIN = 0 };

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
    g_gain = 0.5f;
}

static clap_process_status plugin_process(const clap_plugin_t* plugin,
                                          const clap_process_t* process) {
    const uint32_t num_frames = process->frames_count;
    const uint32_t num_channels = process->audio_inputs[0].channel_count;

    // Process parameter events
    const clap_input_events_t* in_events = process->in_events;
    for (uint32_t i = 0; i < in_events->size(in_events); ++i) {
        const clap_event_header_t* event = in_events->get(in_events, i);
        if (event->type == CLAP_EVENT_PARAM_VALUE) {
            const clap_event_param_value_t* pv = (const clap_event_param_value_t*)event;
            if (pv->param_id == PARAM_GAIN) {
                g_gain = (float)pv->value;
            }
        }
    }

    // Process audio
    for (uint32_t ch = 0; ch < num_channels; ++ch) {
        const float* in = process->audio_inputs[0].data32[ch];
        float* out = process->audio_outputs[0].data32[ch];

        for (uint32_t i = 0; i < num_frames; ++i) {
            out[i] = in[i] * g_gain;
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

static const void* plugin_get_extension(const clap_plugin_t* plugin, const char* id) {
    if (strcmp(id, CLAP_EXT_PARAMS) == 0) {
        static const clap_plugin_params_t params_ext = {
            .count = [](const clap_plugin_t*) -> uint32_t { return 1; },
            .get_info = [](const clap_plugin_t*, uint32_t index, clap_param_info_t* info) -> bool {
                if (index != 0) return false;
                info->id = PARAM_GAIN;
                strncpy(info->name, "Gain", CLAP_NAME_SIZE);
                info->module[0] = '\0';
                info->min_value = 0.0;
                info->max_value = 2.0;
                info->default_value = 0.5;
                info->flags = CLAP_PARAM_IS_AUTOMATABLE;
                info->cookie = nullptr;
                return true;
            },
            .get_value = [](const clap_plugin_t*, clap_id id, double* value) -> bool {
                if (id == PARAM_GAIN) { *value = g_gain; return true; }
                return false;
            },
            .value_to_text = [](const clap_plugin_t*, clap_id, double value, char* buf, uint32_t size) -> bool {
                snprintf(buf, size, "%.2f", value);
                return true;
            },
            .text_to_value = [](const clap_plugin_t*, clap_id, const char* text, double* value) -> bool {
                *value = atof(text);
                return true;
            },
            .flush = [](const clap_plugin_t*, const clap_input_events_t* in, const clap_output_events_t*) {
                for (uint32_t i = 0; i < in->size(in); ++i) {
                    const clap_event_header_t* event = in->get(in, i);
                    if (event->type == CLAP_EVENT_PARAM_VALUE) {
                        const clap_event_param_value_t* pv = (const clap_event_param_value_t*)event;
                        if (pv->param_id == PARAM_GAIN) g_gain = (float)pv->value;
                    }
                }
            }
        };
        return &params_ext;
    }

    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        static const clap_plugin_audio_ports_t audio_ports_ext = {
            .count = [](const clap_plugin_t*, bool is_input) -> uint32_t { return 1; },
            .get = [](const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t* info) -> bool {
                if (index != 0) return false;
                info->id = is_input ? 0 : 1;
                strncpy(info->name, is_input ? "Input" : "Output", CLAP_NAME_SIZE);
                info->channel_count = 2;
                info->flags = CLAP_AUDIO_PORT_IS_MAIN;
                info->port_type = CLAP_PORT_STEREO;
                info->in_place_pair = CLAP_INVALID_ID;
                return true;
            }
        };
        return &audio_ports_ext;
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
// Factory
//-----------------------------------------------------------------------------

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* factory) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t* factory, uint32_t index) {
    return index == 0 ? &s_descriptor : nullptr;
}

static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t* factory,
    const clap_host_t* host,
    const char* plugin_id) {
    if (strcmp(plugin_id, s_descriptor.id) == 0) {
        return &s_plugin;
    }
    return nullptr;
}

static const clap_plugin_factory_t s_factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin
};

//-----------------------------------------------------------------------------
// Entry point
//-----------------------------------------------------------------------------

static bool entry_init(const char* path) {
    return true;
}

static void entry_deinit(void) {
}

static const void* entry_get_factory(const char* factory_id) {
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &s_factory;
    }
    return nullptr;
}

extern "C" {
    CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
        .clap_version = CLAP_VERSION,
        .init = entry_init,
        .deinit = entry_deinit,
        .get_factory = entry_get_factory
    };
}
