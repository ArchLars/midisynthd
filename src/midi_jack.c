/*
 * MIDI input via JACK API for midisynthd
 */
#include "midi_jack.h"
#ifdef HAVE_JACK
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

struct midi_jack_s {
    jack_client_t *client;
    jack_port_t *in_port;
    synth_t *synth;
    bool initialized;
};

static void handle_event(midi_jack_t *midi, const jack_midi_event_t *ev) {
    if (!midi || !midi->synth || !ev || ev->size == 0) return;
    const uint8_t *d = ev->buffer;
    uint8_t status = d[0];
    int ch = status & 0x0F;
    snd_seq_event_t sev; 
    snd_seq_ev_clear(&sev);

    switch (status & 0xF0) {
        case 0x90: /* Note on */
            if (ev->size >= 3) {
                sev.type = SND_SEQ_EVENT_NOTEON;
                sev.data.note.channel = ch;
                sev.data.note.note = d[1];
                sev.data.note.velocity = d[2];
                synth_handle_midi_event(midi->synth, &sev);
            }
            break;
        case 0x80: /* Note off */
            if (ev->size >= 3) {
                sev.type = SND_SEQ_EVENT_NOTEOFF;
                sev.data.note.channel = ch;
                sev.data.note.note = d[1];
                sev.data.note.velocity = d[2];
                synth_handle_midi_event(midi->synth, &sev);
            }
            break;
        case 0xB0: /* Control change */
            if (ev->size >= 3) {
                sev.type = SND_SEQ_EVENT_CONTROLLER;
                sev.data.control.channel = ch;
                sev.data.control.param = d[1];
                sev.data.control.value = d[2];
                synth_handle_midi_event(midi->synth, &sev);
            }
            break;
        case 0xC0: /* Program change */
            if (ev->size >= 2) {
                sev.type = SND_SEQ_EVENT_PGMCHANGE;
                sev.data.control.channel = ch;
                sev.data.control.value = d[1];
                synth_handle_midi_event(midi->synth, &sev);
            }
            break;
        case 0xE0: /* Pitch bend */
            if (ev->size >= 3) {
                sev.type = SND_SEQ_EVENT_PITCHBEND;
                sev.data.control.channel = ch;
                sev.data.control.value = ((d[2] << 7) | d[1]) - 8192;
                synth_handle_midi_event(midi->synth, &sev);
            }
            break;
        default:
            break;
    }
}

static int process_callback(jack_nframes_t nframes, void *arg) {
    midi_jack_t *midi = arg;
    void *buf = jack_port_get_buffer(midi->in_port, nframes);
    uint32_t count = jack_midi_get_event_count(buf);
    for (uint32_t i = 0; i < count; i++) {
        jack_midi_event_t ev;
        if (jack_midi_event_get(&ev, buf, i) == 0) {
            handle_event(midi, &ev);
        }
    }
    return 0;
}

midi_jack_t *midi_jack_init(const midisynthd_config_t *config, synth_t *synth) {
    if (!config || !synth) {
        syslog(LOG_ERR, "Invalid parameters for JACK MIDI init");
        return NULL;
    }

    midi_jack_t *midi = calloc(1, sizeof(*midi));
    if (!midi) return NULL;
    midi->synth = synth;

    jack_status_t status = 0;
    midi->client = jack_client_open(config->client_name, JackNoStartServer, &status);
    if (!midi->client) {
        syslog(LOG_ERR, "Failed to open JACK client");
        free(midi);
        return NULL;
    }

    midi->in_port = jack_port_register(midi->client, "midi_in", JACK_DEFAULT_MIDI_TYPE,
                                       JackPortIsInput, 0);
    if (!midi->in_port) {
        syslog(LOG_ERR, "Failed to register JACK MIDI port");
        jack_client_close(midi->client);
        free(midi);
        return NULL;
    }

    jack_set_process_callback(midi->client, process_callback, midi);
    if (jack_activate(midi->client) != 0) {
        syslog(LOG_ERR, "Failed to activate JACK client");
        jack_client_close(midi->client);
        free(midi);
        return NULL;
    }

    if (config->midi_autoconnect) {
        const char **ports = jack_get_ports(midi->client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
        if (ports) {
            for (int i = 0; ports[i]; i++) {
                jack_connect(midi->client, ports[i], jack_port_name(midi->in_port));
            }
            free(ports);
        }
    }

    midi->initialized = true;
    syslog(LOG_INFO, "JACK MIDI driver initialized");
    return midi;
}

int midi_jack_process_events(midi_jack_t *midi, int timeout_ms) {
    if (!midi || !midi->initialized) return -1;
    if (timeout_ms > 0) poll(NULL, 0, timeout_ms);
    return 0;
}

int midi_jack_disconnect_all(midi_jack_t *midi) {
    if (!midi || !midi->initialized) return -1;
    synth_all_notes_off(midi->synth);
    return 0;
}

void midi_jack_cleanup(midi_jack_t *midi) {
    if (!midi) return;
    if (midi->client) {
        jack_client_close(midi->client);
        midi->client = NULL;
    }
    free(midi);
}

#endif /* HAVE_JACK */
