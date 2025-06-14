#ifndef MIDI_ALSA_H
#define MIDI_ALSA_H

#include "config.h"
#include "synth.h"

typedef struct midi_alsa_s midi_alsa_t;

midi_alsa_t *midi_alsa_init(const midisynthd_config_t *config, synth_t *synth);
void midi_alsa_cleanup(midi_alsa_t *midi);
int midi_alsa_process_events(midi_alsa_t *midi, int timeout_ms);
int midi_alsa_disconnect_all(midi_alsa_t *midi);

#endif /* MIDI_ALSA_H */
