#include "config.h"
#include "synth.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct synth_s {
    int last_note;
};

void config_init_defaults(midisynthd_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->sample_rate = CONFIG_DEFAULT_SAMPLE_RATE;
    cfg->gain = CONFIG_DEFAULT_GAIN;
    cfg->buffer_size = CONFIG_DEFAULT_BUFFER_SIZE;
    cfg->audio_periods = CONFIG_DEFAULT_AUDIO_PERIODS;
    cfg->polyphony = CONFIG_DEFAULT_POLYPHONY;
    cfg->audio_driver = AUDIO_DRIVER_AUTO;
    cfg->log_level = LOG_LEVEL_INFO;
    cfg->midi_autoconnect = true;
    cfg->chorus_enabled = true;
    cfg->chorus_level = CONFIG_DEFAULT_CHORUS_LEVEL;
    cfg->reverb_enabled = true;
    cfg->reverb_level = CONFIG_DEFAULT_REVERB_LEVEL;
    cfg->realtime_priority = true;
    strncpy(cfg->client_name, CONFIG_DEFAULT_CLIENT_NAME, CONFIG_MAX_STRING_LEN - 1);
    cfg->client_name[CONFIG_MAX_STRING_LEN - 1] = '\0';
}

int config_load_file(midisynthd_config_t *cfg, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char key[64], val[64];
    while (fscanf(f, "%63[^=]=%63s\n", key, val) == 2) {
        if (strcmp(key, "gain") == 0) cfg->gain = atof(val);
        else if (strcmp(key, "sample_rate") == 0) cfg->sample_rate = atoi(val);
        else if (strcmp(key, "buffer_size") == 0) cfg->buffer_size = atoi(val);
        else if (strcmp(key, "polyphony") == 0) cfg->polyphony = atoi(val);
    }
    fclose(f);
    return 0;
}

int config_validate(midisynthd_config_t *cfg) { 
    (void)cfg; 
    return 0; 
}

synth_t* synth_init(const midisynthd_config_t *cfg, audio_t *audio) {
    (void)cfg; (void)audio;
    return calloc(1, sizeof(synth_t));
}

void synth_cleanup(synth_t *s) { 
    free(s); 
}

int synth_note_on(synth_t *s, int ch, int note, int vel) {
    (void)ch; (void)vel; 
    if (!s) return -1; 
    s->last_note = note; 
    return 0; 
}

int synth_note_off(synth_t *s, int ch, int note, int vel) {
    (void)ch; (void)vel; 
    if (!s || s->last_note != note) return -1; 
    return 0; 
}

fluid_settings_t *synth_get_settings(synth_t *s) {
    (void)s;
    return NULL;  /* Stub - return NULL for tests */
}

fluid_synth_t *synth_get_fluidsynth(synth_t *s) {
    (void)s;
    return NULL;  /* Stub - return NULL for tests */
}

int synth_handle_midi_event(synth_t *s, snd_seq_event_t *ev) {
    if (!s || !ev) return -1;
    switch (ev->type) {
        case SND_SEQ_EVENT_NOTEON:
            return synth_note_on(s, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
        case SND_SEQ_EVENT_NOTEOFF:
            return synth_note_off(s, ev->data.note.channel, ev->data.note.note, ev->data.note.velocity);
        default:
            return 0;
    }
}
