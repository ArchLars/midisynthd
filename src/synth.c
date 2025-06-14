/*
 * midisynthd - System-level MIDI Synthesizer Daemon for Linux
 * Copyright (C) 2025 ArchLars
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include "synth.h"
#include "config.h"
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <sys/stat.h>

#include <fluidsynth.h>
#include <fluidsynth/midi.h>

/**
 * Internal synthesizer structure
 */
struct synth_s {
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    fluid_audio_driver_t *audio_driver;
    const midisynthd_config_t *config;
    audio_t *audio;
    int soundfont_id;
    bool initialized;
};

/**
 * Audio driver names for FluidSynth
 */
static const char* fluidsynth_driver_names[] = {
    [AUDIO_DRIVER_AUTO]       = "auto",
    [AUDIO_DRIVER_JACK]       = "jack",
    [AUDIO_DRIVER_PIPEWIRE]   = "pipewire", 
    [AUDIO_DRIVER_PULSEAUDIO] = "pulseaudio",
    [AUDIO_DRIVER_ALSA]       = "alsa"
};

/**
 * Default soundfont search paths
 */
static const char* default_soundfont_paths[] = {
    "/usr/share/soundfonts/FluidR3_GM.sf2",
    "/usr/share/sounds/sf2/FluidR3_GM.sf2",
    "/usr/share/soundfonts/default.sf2",
    "/usr/share/sounds/sf2/default.sf2",
    CONFIG_DEFAULT_SOUNDFONT_PATH,
    NULL
};

/**
 * Check if a file exists and is readable
 */
static bool file_exists_and_readable(const char *path) {
    if (!path || strlen(path) == 0) {
        return false;
    }
    return access(path, R_OK) == 0;
}

/**
 * Find the first available soundfont file
 */
static const char* find_available_soundfont(void) {
    for (int i = 0; default_soundfont_paths[i] != NULL; i++) {
        if (file_exists_and_readable(default_soundfont_paths[i])) {
            return default_soundfont_paths[i];
        }
    }
    return NULL;
}

/**
 * Setup FluidSynth settings based on configuration
 */
static int setup_fluidsynth_settings(synth_t *synth) {
    const midisynthd_config_t *config = synth->config;
    
    /* Set audio driver */
    const char *driver_name = "auto";
    if (config->audio_driver < AUDIO_DRIVER_COUNT) {
        driver_name = fluidsynth_driver_names[config->audio_driver];
    }
    
    if (fluid_settings_setstr(synth->settings, "audio.driver", driver_name) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set audio driver to '%s', using auto", driver_name);
    } else {
        syslog(LOG_DEBUG, "Set FluidSynth audio driver to '%s'", driver_name);
    }
    
    /* Set sample rate */
    if (fluid_settings_setnum(synth->settings, "synth.sample-rate", config->sample_rate) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set sample rate to %d", config->sample_rate);
    } else {
        syslog(LOG_DEBUG, "Set sample rate to %d Hz", config->sample_rate);
    }
    
    /* Set polyphony */
    if (fluid_settings_setint(synth->settings, "synth.polyphony", config->polyphony) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set polyphony to %d", config->polyphony);
    } else {
        syslog(LOG_DEBUG, "Set polyphony to %d voices", config->polyphony);
    }
    
    /* Set gain */
    if (fluid_settings_setnum(synth->settings, "synth.gain", config->gain) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set gain to %.2f", config->gain);
    } else {
        syslog(LOG_DEBUG, "Set gain to %.2f", config->gain);
    }
    
    /* Set buffer size for low latency */
    if (fluid_settings_setint(synth->settings, "audio.period-size", config->buffer_size) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set buffer size to %d", config->buffer_size);
    } else {
        syslog(LOG_DEBUG, "Set buffer size to %d frames", config->buffer_size);
    }
    
    /* Set number of buffer periods */
    if (fluid_settings_setint(synth->settings, "audio.periods", config->audio_periods) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set buffer count to %d", config->audio_periods);
    } else {
        syslog(LOG_DEBUG, "Set buffer count to %d periods", config->audio_periods);
    }
    
    /* Enable real-time priority if configured */
    if (config->realtime_priority) {
        if (fluid_settings_setstr(synth->settings, "audio.realtime-prio", "yes") != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to enable real-time priority");
        } else {
            syslog(LOG_DEBUG, "Enabled real-time priority");
        }
    }
    
    /* Set JACK client name if using JACK */
    if (config->audio_driver == AUDIO_DRIVER_JACK || config->audio_driver == AUDIO_DRIVER_AUTO) {
        if (fluid_settings_setstr(synth->settings, "audio.jack.id", config->client_name) != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to set JACK client name");
        } else {
            syslog(LOG_DEBUG, "Set JACK client name to '%s'", config->client_name);
        }
        
        /* Enable JACK auto-connect by default */
        if (fluid_settings_setstr(synth->settings, "audio.jack.autoconnect", "yes") != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to enable JACK auto-connect");
        } else {
            syslog(LOG_DEBUG, "Enabled JACK auto-connect");
        }
    }
    
    /* Set PulseAudio client name */
    if (config->audio_driver == AUDIO_DRIVER_PULSEAUDIO || config->audio_driver == AUDIO_DRIVER_AUTO) {
        if (fluid_settings_setstr(synth->settings, "audio.pulseaudio.server", "default") != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to set PulseAudio server");
        }
        
        if (fluid_settings_setstr(synth->settings, "audio.pulseaudio.device", config->client_name) != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to set PulseAudio device name");
        } else {
            syslog(LOG_DEBUG, "Set PulseAudio device name to '%s'", config->client_name);
        }
    }
    
    return 0;
}

/**
 * Load soundfonts into the synthesizer
 */
static int load_soundfonts(synth_t *synth) {
    const midisynthd_config_t *config = synth->config;
    int loaded_count = 0;
    
    /* Try to load configured soundfonts first */
    for (int i = 0; i < config->soundfont_count && i < CONFIG_MAX_SOUNDFONTS; i++) {
        if (!config->soundfonts[i].enabled) {
            continue;
        }
        
        const char *sf_path = config->soundfonts[i].path;
        if (!file_exists_and_readable(sf_path)) {
            syslog(LOG_WARNING, "Soundfont file not accessible: %s", sf_path);
            continue;
        }
        
        syslog(LOG_INFO, "Loading soundfont: %s", sf_path);
        int sf_id = fluid_synth_sfload(synth->synth, sf_path, 1);
        if (sf_id == FLUID_FAILED) {
            syslog(LOG_ERR, "Failed to load soundfont: %s", sf_path);
            continue;
        }
        
        if (loaded_count == 0) {
            synth->soundfont_id = sf_id; /* Remember first loaded soundfont */
        }
        
        loaded_count++;
        syslog(LOG_INFO, "Successfully loaded soundfont: %s (ID: %d)", sf_path, sf_id);
        
        /* Set bank offset if specified */
        if (config->soundfonts[i].bank_offset != 0) {
            /* FluidSynth doesn't have a direct API for bank offset, 
             * but we can note this for future channel mapping */
            syslog(LOG_DEBUG, "Bank offset %d noted for soundfont %s", 
                   config->soundfonts[i].bank_offset, sf_path);
        }
    }
    
    /* If no configured soundfonts were loaded, try to find a default one */
    if (loaded_count == 0) {
        const char *default_sf = find_available_soundfont();
        if (default_sf) {
            syslog(LOG_INFO, "Loading default soundfont: %s", default_sf);
            int sf_id = fluid_synth_sfload(synth->synth, default_sf, 1);
            if (sf_id != FLUID_FAILED) {
                synth->soundfont_id = sf_id;
                loaded_count++;
                syslog(LOG_INFO, "Successfully loaded default soundfont: %s (ID: %d)", default_sf, sf_id);
            } else {
                syslog(LOG_ERR, "Failed to load default soundfont: %s", default_sf);
            }
        }
    }
    
    if (loaded_count == 0) {
        syslog(LOG_ERR, "No soundfonts could be loaded - synthesis will not work");
        return -1;
    }
    
    syslog(LOG_INFO, "Loaded %d soundfont(s) successfully", loaded_count);
    return 0;
}

/**
 * Setup synthesizer effects (chorus, reverb)
 */
static void setup_effects(synth_t *synth) {
    const midisynthd_config_t *config = synth->config;
    
    /* Configure chorus */
    if (config->chorus_enabled) {
        fluid_synth_set_chorus_on(synth->synth, 1);
        /* FluidSynth chorus parameters: N, level, speed, depth, type */
        fluid_synth_set_chorus(synth->synth, 3, config->chorus_level, 0.3, 8.0, FLUID_CHORUS_MOD_SINE);
        syslog(LOG_DEBUG, "Enabled chorus with level %.2f", config->chorus_level);
    } else {
        fluid_synth_set_chorus_on(synth->synth, 0);
        syslog(LOG_DEBUG, "Disabled chorus");
    }
    
    /* Configure reverb */
    if (config->reverb_enabled) {
        fluid_synth_set_reverb_on(synth->synth, 1);
        /* FluidSynth reverb parameters: roomsize, damping, width, level */
        fluid_synth_set_reverb(synth->synth, 0.2, 0.0, 0.5, config->reverb_level);
        syslog(LOG_DEBUG, "Enabled reverb with level %.2f", config->reverb_level);
    } else {
        fluid_synth_set_reverb_on(synth->synth, 0);
        syslog(LOG_DEBUG, "Disabled reverb");
    }
}

/**
 * Initialize the synthesizer engine
 */
synth_t* synth_init(const midisynthd_config_t *config, audio_t *audio) {
    if (!config) {
        syslog(LOG_ERR, "Invalid configuration passed to synth_init");
        return NULL;
    }
    
    synth_t *synth = calloc(1, sizeof(synth_t));
    if (!synth) {
        syslog(LOG_ERR, "Failed to allocate memory for synthesizer");
        return NULL;
    }
    
    synth->config = config;
    synth->audio = audio;
    synth->soundfont_id = FLUID_FAILED;
    synth->initialized = false;
    
    /* Create FluidSynth settings */
    synth->settings = new_fluid_settings();
    if (!synth->settings) {
        syslog(LOG_ERR, "Failed to create FluidSynth settings");
        goto error;
    }
    
    /* Configure FluidSynth settings */
    if (setup_fluidsynth_settings(synth) < 0) {
        syslog(LOG_ERR, "Failed to configure FluidSynth settings");
        goto error;
    }
    
    /* Create FluidSynth synthesizer */
    synth->synth = new_fluid_synth(synth->settings);
    if (!synth->synth) {
        syslog(LOG_ERR, "Failed to create FluidSynth synthesizer");
        goto error;
    }
    
    /* Load soundfonts */
    if (load_soundfonts(synth) < 0) {
        syslog(LOG_ERR, "Failed to load any soundfonts");
        goto error;
    }
    
    /* Setup effects */
    setup_effects(synth);
    
    /* Create audio driver */
    synth->audio_driver = new_fluid_audio_driver(synth->settings, synth->synth);
    if (!synth->audio_driver) {
        syslog(LOG_ERR, "Failed to create FluidSynth audio driver");
        goto error;
    }
    
    synth->initialized = true;
    syslog(LOG_INFO, "FluidSynth synthesizer initialized successfully");
    
    /* Log the actual driver being used */
    char *actual_driver = NULL;
    if (fluid_settings_dupstr(synth->settings, "audio.driver", &actual_driver) == FLUID_OK) {
        syslog(LOG_INFO, "Using audio driver: %s", actual_driver);
        if (actual_driver) {
            free(actual_driver);
        }
    }
    
    return synth;
    
error:
    synth_cleanup(synth);
    return NULL;
}

/**
 * Clean up synthesizer resources
 */
void synth_cleanup(synth_t *synth) {
    if (!synth) {
        return;
    }
    
    syslog(LOG_DEBUG, "Cleaning up FluidSynth synthesizer");
    
    if (synth->audio_driver) {
        delete_fluid_audio_driver(synth->audio_driver);
        synth->audio_driver = NULL;
    }
    
    if (synth->synth) {
        delete_fluid_synth(synth->synth);
        synth->synth = NULL;
    }
    
    if (synth->settings) {
        delete_fluid_settings(synth->settings);
        synth->settings = NULL;
    }
    
    synth->initialized = false;
    free(synth);
}

/**
 * Send a Note On MIDI event to the synthesizer
 */
int synth_note_on(synth_t *synth, int channel, int key, int velocity) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16 || key < 0 || key > 127 || velocity < 0 || velocity > 127) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d, key=%d, velocity=%d", channel, key, velocity);
        return -1;
    }
    
    int result = fluid_synth_noteon(synth->synth, channel, key, velocity);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth note on failed: channel=%d, key=%d, velocity=%d", channel, key, velocity);
        return -1;
    }
    
    return 0;
}

/**
 * Send a Note Off MIDI event to the synthesizer
 */
int synth_note_off(synth_t *synth, int channel, int key, int velocity) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16 || key < 0 || key > 127) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d, key=%d", channel, key);
        return -1;
    }
    
    int result = fluid_synth_noteoff(synth->synth, channel, key);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth note off failed: channel=%d, key=%d", channel, key);
        return -1;
    }
    
    return 0;
}

/**
 * Send a Program Change MIDI event to the synthesizer
 */
int synth_program_change(synth_t *synth, int channel, int program) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16 || program < 0 || program > 127) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d, program=%d", channel, program);
        return -1;
    }
    
    int result = fluid_synth_program_change(synth->synth, channel, program);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth program change failed: channel=%d, program=%d", channel, program);
        return -1;
    }
    
    return 0;
}

/**
 * Send a Control Change MIDI event to the synthesizer
 */
int synth_control_change(synth_t *synth, int channel, int control, int value) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16 || control < 0 || control > 127 || value < 0 || value > 127) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d, control=%d, value=%d", channel, control, value);
        return -1;
    }
    
    int result = fluid_synth_cc(synth->synth, channel, control, value);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth control change failed: channel=%d, control=%d, value=%d", channel, control, value);
        return -1;
    }
    
    return 0;
}

/**
 * Send a Pitch Bend MIDI event to the synthesizer
 */
int synth_pitch_bend(synth_t *synth, int channel, int value) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16 || value < 0 || value > 16383) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d, pitch_bend=%d", channel, value);
        return -1;
    }
    
    int result = fluid_synth_pitch_bend(synth->synth, channel, value);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth pitch bend failed: channel=%d, value=%d", channel, value);
        return -1;
    }
    
    return 0;
}

/**
 * Send a Channel Pressure (Aftertouch) MIDI event to the synthesizer
 */
int synth_channel_pressure(synth_t *synth, int channel, int pressure) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16 || pressure < 0 || pressure > 127) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d, pressure=%d", channel, pressure);
        return -1;
    }
    
    int result = fluid_synth_channel_pressure(synth->synth, channel, pressure);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth channel pressure failed: channel=%d, pressure=%d", channel, pressure);
        return -1;
    }
    
    return 0;
}

/**
 * Send a Key Pressure (Polyphonic Aftertouch) MIDI event to the synthesizer
 */
int synth_key_pressure(synth_t *synth, int channel, int key, int pressure) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16 || key < 0 || key > 127 || pressure < 0 || pressure > 127) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d, key=%d, pressure=%d", channel, key, pressure);
        return -1;
    }
    
    int result = fluid_synth_key_pressure(synth->synth, channel, key, pressure);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth key pressure failed: channel=%d, key=%d, pressure=%d", channel, key, pressure);
        return -1;
    }
    
    return 0;
}

/**
 * Send an All Sound Off MIDI event to the synthesizer
 */
int synth_all_sound_off(synth_t *synth, int channel) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (channel < 0 || channel >= 16) {
        syslog(LOG_DEBUG, "Invalid MIDI parameters: channel=%d", channel);
        return -1;
    }
    
    int result = fluid_synth_all_sounds_off(synth->synth, channel);
    if (result != FLUID_OK) {
        syslog(LOG_DEBUG, "FluidSynth all sound off failed: channel=%d", channel);
        return -1;
    }
    
    return 0;
}

/**
 * Send an All Notes Off MIDI event to the synthesizer
 */
int synth_all_notes_off(synth_t *synth) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    /* Send all notes off to all channels */
    for (int channel = 0; channel < 16; channel++) {
        int result = fluid_synth_all_notes_off(synth->synth, channel);
        if (result != FLUID_OK) {
            syslog(LOG_DEBUG, "FluidSynth all notes off failed: channel=%d", channel);
        }
    }
    
    return 0;
}

/**
 * Reset all MIDI channels
 */
int synth_reset_controllers(synth_t *synth) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    /* Reset all channels */
    for (int i = 0; i < 16; i++) {
        fluid_synth_all_sounds_off(synth->synth, i);
        fluid_synth_all_notes_off(synth->synth, i);
        
        /* Reset controllers to defaults */
        fluid_synth_cc(synth->synth, i, 7, 100);    /* Volume */
        fluid_synth_cc(synth->synth, i, 10, 64);    /* Pan */
        fluid_synth_cc(synth->synth, i, 11, 127);   /* Expression */
        fluid_synth_cc(synth->synth, i, 64, 0);     /* Sustain pedal off */
        fluid_synth_cc(synth->synth, i, 123, 0);    /* All notes off */
        fluid_synth_cc(synth->synth, i, 121, 0);    /* Reset all controllers */
        
        fluid_synth_pitch_bend(synth->synth, i, 8192); /* Center pitch bend */
        
        /* Reset to program 0 (piano) except for channel 9 (drums) */
        if (i != 9) {
            fluid_synth_program_change(synth->synth, i, 0);
        }
    }
    
    syslog(LOG_INFO, "Synthesizer reset completed");
    return 0;
}

/**
 * Set the master gain (volume) of the synthesizer
 */
int synth_set_gain(synth_t *synth, float gain) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1;
    }
    
    if (gain < 0.0f || gain > 2.0f) {
        syslog(LOG_DEBUG, "Invalid gain value: %.2f", gain);
        return -1;
    }
    
    fluid_synth_set_gain(synth->synth, gain);
    return 0;
}

/**
 * Get the current master gain setting
 */
float synth_get_gain(synth_t *synth) {
    if (!synth || !synth->initialized || !synth->synth) {
        return -1.0f;
    }
    
    return fluid_synth_get_gain(synth->synth);
}

/**
 * Get synthesizer status information
 */
int synth_get_status(synth_t *synth, synth_status_t *status) {
    if (!synth || !synth->initialized || !synth->synth || !status) {
        return -1;
    }
    
    memset(status, 0, sizeof(synth_status_t));
    
    status->initialized = synth->initialized;
    status->active_voices = fluid_synth_get_active_voice_count(synth->synth);
    status->max_polyphony = fluid_synth_get_polyphony(synth->synth);
    status->cpu_load = fluid_synth_get_cpu_load(synth->synth);
    status->soundfonts_loaded = (synth->soundfont_id != FLUID_FAILED) ? 1 : 0;
    
    /* Get sample rate and buffer size from settings */
    double sample_rate;
    if (fluid_settings_getnum(synth->settings, "synth.sample-rate", &sample_rate) == FLUID_OK) {
        status->sample_rate = sample_rate;
    }
    
    int buffer_size;
    if (fluid_settings_getint(synth->settings, "audio.period-size", &buffer_size) == FLUID_OK) {
        status->buffer_size = buffer_size;
    }
    
    return 0;
}

/**
 * Update runtime-changeable settings
 */
int synth_update_settings(synth_t *synth, const midisynthd_config_t *new_config) {
    if (!synth || !synth->initialized || !synth->synth || !new_config) {
        return -1;
    }
    
    /* Update gain */
    if (new_config->gain != synth->config->gain) {
        fluid_synth_set_gain(synth->synth, new_config->gain);
        syslog(LOG_INFO, "Updated synthesizer gain to %.2f", new_config->gain);
    }
    
    /* Update chorus settings */
    if (new_config->chorus_enabled != synth->config->chorus_enabled ||
        new_config->chorus_level != synth->config->chorus_level) {
        
        if (new_config->chorus_enabled) {
            fluid_synth_set_chorus_on(synth->synth, 1);
            fluid_synth_set_chorus(synth->synth, 3, new_config->chorus_level, 0.3, 8.0, FLUID_CHORUS_MOD_SINE);
            syslog(LOG_INFO, "Updated chorus: enabled, level %.2f", new_config->chorus_level);
        } else {
            fluid_synth_set_chorus_on(synth->synth, 0);
            syslog(LOG_INFO, "Updated chorus: disabled");
        }
    }
    
    /* Update reverb settings */
    if (new_config->reverb_enabled != synth->config->reverb_enabled ||
        new_config->reverb_level != synth->config->reverb_level) {
        
        if (new_config->reverb_enabled) {
            fluid_synth_set_reverb_on(synth->synth, 1);
            fluid_synth_set_reverb(synth->synth, 0.2, 0.0, 0.5, new_config->reverb_level);
            syslog(LOG_INFO, "Updated reverb: enabled, level %.2f", new_config->reverb_level);
        } else {
            fluid_synth_set_reverb_on(synth->synth, 0);
            syslog(LOG_INFO, "Updated reverb: disabled");
        }
    }
    
    /* Update config pointer */
    synth->config = new_config;
    
    return 0;
}

fluid_settings_t *synth_get_settings(synth_t *synth) {
    if (!synth) return NULL;
    return synth->settings;
}

/**
 * Get the FluidSynth object for MIDI driver use
 */
fluid_synth_t *synth_get_fluidsynth(synth_t *synth) {
    if (!synth || !synth->initialized) return NULL;
    return synth->synth;
}

int synth_handle_midi_event(synth_t *synth, snd_seq_event_t *ev) {
    if (!synth || !ev) return -1;

    switch (ev->type) {
        case SND_SEQ_EVENT_NOTEON:
            return synth_note_on(synth, ev->data.note.channel,
                                 ev->data.note.note,
                                 ev->data.note.velocity);
        case SND_SEQ_EVENT_NOTEOFF:
            return synth_note_off(synth, ev->data.note.channel,
                                  ev->data.note.note, 0);
        case SND_SEQ_EVENT_KEYPRESS:
            return synth_key_pressure(synth, ev->data.note.channel,
                                      ev->data.note.note,
                                      ev->data.note.velocity);
        case SND_SEQ_EVENT_CONTROLLER:
            return synth_control_change(synth,
                                        ev->data.control.channel,
                                        ev->data.control.param,
                                        ev->data.control.value);
        case SND_SEQ_EVENT_PGMCHANGE:
            return synth_program_change(synth,
                                        ev->data.control.channel,
                                        ev->data.control.value);
        case SND_SEQ_EVENT_CHANPRESS:
            return synth_channel_pressure(synth,
                                          ev->data.control.channel,
                                          ev->data.control.value);
        case SND_SEQ_EVENT_PITCHBEND:
            return synth_pitch_bend(synth,
                                   ev->data.control.channel,
                                   ev->data.control.value + 8192);
        default:
            break;
    }
    return 0;
}

/**
 * Check if the synthesizer is properly initialized and ready
 */
bool synth_is_ready(synth_t *synth) {
    return synth && synth->initialized && synth->synth && synth->audio_driver;
}

int synth_unload_soundfont(synth_t *synth, int soundfont_id) {
    if (!synth || !synth->synth) return -1;
    if (fluid_synth_sfunload(synth->synth, soundfont_id, 1) == FLUID_OK)
        return 0;
    return -1;
}

int synth_set_polyphony(synth_t *synth, int polyphony) {
    if (!synth || !synth->synth || polyphony <= 0) return -1;
    if (fluid_synth_set_polyphony(synth->synth, polyphony) != FLUID_OK)
        return -1;
    return 0;
}

int synth_get_polyphony(synth_t *synth) {
    if (!synth || !synth->synth) return -1;
    return fluid_synth_get_polyphony(synth->synth);
}
