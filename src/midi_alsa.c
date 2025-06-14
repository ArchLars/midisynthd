#include <stdlib.h>
#include <syslog.h>
#include <fluidsynth.h>
#include "midi_alsa.h"
#include "synth.h"

struct midi_alsa_s {
    fluid_midi_driver_t *driver;
    fluid_settings_t *settings;
};

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

    midi->settings = synth_get_settings(synth);
    if (!midi->settings) {
        syslog(LOG_ERR, "Synth settings unavailable");
        free(midi);
        return NULL;
    }

    fluid_settings_setstr(midi->settings, "midi.driver", "alsa_seq");
    fluid_settings_setstr(midi->settings, "midi.alsa_seq.id", config->client_name);
    fluid_settings_setint(midi->settings, "midi.autoconnect", config->midi_autoconnect ? 1 : 0);

    midi->driver = new_fluid_midi_driver(midi->settings,
                                         (fluid_midi_event_handler_t)fluid_synth_handle_midi_event,
                                         synth);
    if (!midi->driver) {
        syslog(LOG_ERR, "Failed to create FluidSynth MIDI driver");
        free(midi);
        return NULL;
    }

    syslog(LOG_INFO, "ALSA sequencer MIDI driver initialized");
    return midi;
}

int midi_alsa_process_events(midi_alsa_t *midi, int timeout_ms) {
    (void)midi;
    (void)timeout_ms;
    return 0; /* handled internally by FluidSynth */
}

void midi_alsa_cleanup(midi_alsa_t *midi) {
    if (!midi) return;
    if (midi->driver)
        delete_fluid_midi_driver(midi->driver);
    free(midi);
}
