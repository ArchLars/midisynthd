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

#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <poll.h>
#include <fluidsynth.h>
#include "midi_alsa.h"
#include "synth.h"

struct midi_alsa_s {
    fluid_midi_driver_t *driver;
    fluid_settings_t *settings;
    synth_t *synth;
    fluid_synth_t *fluid_synth;
    bool initialized;
};

/**
 * MIDI event handler callback
 * This function is called by FluidSynth's MIDI driver when MIDI events are received
 */
static int midi_event_handler(void *data, fluid_midi_event_t *event) {
    midi_alsa_t *midi = (midi_alsa_t *)data;
    
    if (!midi || !midi->fluid_synth || !event) {
        return FLUID_FAILED;
    }
    
    /* Let FluidSynth handle the MIDI event directly */
    return fluid_synth_handle_midi_event(midi->fluid_synth, event);
}

/**
 * Initialize ALSA MIDI input system
 */
midi_alsa_t *midi_alsa_init(const midisynthd_config_t *config, synth_t *synth) {
    if (!config || !synth) {
        syslog(LOG_ERR, "Invalid parameters for MIDI initialization");
        return NULL;
    }

    midi_alsa_t *midi = calloc(1, sizeof(*midi));
    if (!midi) {
        syslog(LOG_ERR, "Failed to allocate MIDI object");
        return NULL;
    }

    midi->synth = synth;
    midi->fluid_synth = synth_get_fluidsynth(synth);
    if (!midi->fluid_synth) {
        syslog(LOG_ERR, "Failed to get FluidSynth instance from synth");
        free(midi);
        return NULL;
    }

    /* Get FluidSynth settings from the synth module */
    midi->settings = synth_get_settings(synth);
    if (!midi->settings) {
        syslog(LOG_ERR, "Synth settings unavailable");
        free(midi);
        return NULL;
    }

    /* Configure MIDI driver settings */
    if (fluid_settings_setstr(midi->settings, "midi.driver", "alsa_seq") != FLUID_OK) {
        syslog(LOG_ERR, "Failed to set MIDI driver to alsa_seq");
        free(midi);
        return NULL;
    }
    
    /* Set ALSA sequencer client name */
    if (fluid_settings_setstr(midi->settings, "midi.alsa_seq.id", config->client_name) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set ALSA sequencer client name");
    }
    
    /* Configure auto-connect setting */
    if (fluid_settings_setint(midi->settings, "midi.autoconnect", 
                             config->midi_autoconnect ? 1 : 0) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set MIDI autoconnect setting");
    }

    /* Enable real-time priority for MIDI thread if configured */
    if (config->realtime_priority) {
        if (fluid_settings_setint(midi->settings, "midi.realtime-prio", 50) != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to set MIDI real-time priority");
        }
    }

    /* Create the MIDI driver with our event handler */
    midi->driver = new_fluid_midi_driver(midi->settings,
                                         midi_event_handler,
                                         midi);
    if (!midi->driver) {
        syslog(LOG_ERR, "Failed to create FluidSynth MIDI driver");
        free(midi);
        return NULL;
    }

    midi->initialized = true;
    
    syslog(LOG_INFO, "ALSA sequencer MIDI driver initialized successfully");
    syslog(LOG_INFO, "MIDI client name: '%s', autoconnect: %s", 
           config->client_name, config->midi_autoconnect ? "enabled" : "disabled");
    
    return midi;
}

/**
 * Process MIDI events (with proper timeout handling)
 * 
 * Note: Since we're using FluidSynth's built-in MIDI driver with a callback,
 * most event processing happens automatically in FluidSynth's own thread.
 * This function can be used for additional processing or monitoring.
 */
int midi_alsa_process_events(midi_alsa_t *midi, int timeout_ms) {
    if (!midi || !midi->initialized) {
        return -1;
    }
    
    /* Since FluidSynth handles MIDI events in its own thread via the callback,
     * we don't need to explicitly poll for events here. However, we can
     * use this function for other MIDI-related processing if needed.
     * 
     * For now, we just return success. In a more complex implementation,
     * this could be used for:
     * - Custom MIDI routing
     * - MIDI event logging
     * - Statistics collection
     * - Additional MIDI sources beyond ALSA sequencer
     */
    
    (void)timeout_ms; /* Unused for now */
    
    return 0; /* Success - events handled by FluidSynth callback */
}

/**
 * Get basic MIDI driver status
 */
bool midi_alsa_get_status(midi_alsa_t *midi) {
    return midi && midi->initialized && midi->driver;
}

/**
 * Check if MIDI system is properly initialized
 */
bool midi_alsa_is_ready(midi_alsa_t *midi) {
    return midi && midi->initialized && midi->driver && midi->fluid_synth;
}

/**
 * Get MIDI client name for display/logging
 */
const char* midi_alsa_get_client_name(midi_alsa_t *midi) {
    if (!midi || !midi->initialized) {
        return "unknown";
    }
    
    static char client_name[128] = {0};
    char *name = NULL;
    
    if (fluid_settings_dupstr(midi->settings, "midi.alsa_seq.id", &name) == FLUID_OK && name) {
        strncpy(client_name, name, sizeof(client_name) - 1);
        client_name[sizeof(client_name) - 1] = '\0';
        free(name);
        return client_name;
    }
    
    return "MidiSynth Daemon";
}

/**
 * Force disconnect all MIDI connections (emergency stop)
 */
int midi_alsa_disconnect_all(midi_alsa_t *midi) {
    if (!midi || !midi->initialized) {
        return -1;
    }
    
    /* FluidSynth manages connections internally, but we can request
     * all notes off on all channels as an emergency measure */
    if (midi->synth) {
        synth_all_notes_off(midi->synth);
        syslog(LOG_INFO, "MIDI emergency stop: all notes off");
    }
    
    return 0;
}

/**
 * Cleanup MIDI system
 */
void midi_alsa_cleanup(midi_alsa_t *midi) {
    if (!midi) {
        return;
    }
    
    syslog(LOG_DEBUG, "Cleaning up ALSA MIDI driver");
    
    if (midi->driver) {
        /* This will stop the MIDI thread and close ALSA sequencer */
        delete_fluid_midi_driver(midi->driver);
        midi->driver = NULL;
    }
    
    midi->initialized = false;
    midi->settings = NULL; /* Don't delete - owned by synth module */
    midi->fluid_synth = NULL; /* Don't delete - owned by synth module */
    midi->synth = NULL; /* Don't delete - just a reference */
    
    free(midi);
    
    syslog(LOG_INFO, "ALSA MIDI driver cleanup completed");
}
