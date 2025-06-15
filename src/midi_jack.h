#ifndef MIDI_JACK_H
#define MIDI_JACK_H

#include "config.h"
#include "synth.h"

#ifdef HAVE_JACK
#include <jack/jack.h>
#include <jack/midiport.h>
#endif

typedef struct midi_jack_s midi_jack_t;

midi_jack_t *midi_jack_init(const midisynthd_config_t *config, synth_t *synth);
void midi_jack_cleanup(midi_jack_t *midi);
int midi_jack_process_events(midi_jack_t *midi, int timeout_ms);
int midi_jack_disconnect_all(midi_jack_t *midi);

#endif /* MIDI_JACK_H */
