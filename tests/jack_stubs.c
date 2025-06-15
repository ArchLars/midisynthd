#include <stdlib.h>
#include <string.h>
#include "midi_jack.h"

#ifdef HAVE_JACK
/* Minimal JACK API stubs for unit testing without a JACK server */

typedef struct {
    int (*process)(jack_nframes_t, void*);
    void *arg;
} dummy_client;

jack_client_t *jack_client_open(const char *name, jack_options_t options, jack_status_t *status, ...) {
    (void)name; (void)options; if (status) *status = 0;
    return (jack_client_t*)calloc(1, sizeof(dummy_client));
}

int jack_client_close(jack_client_t *client) {
    free(client); return 0;
}

jack_port_t *jack_port_register(jack_client_t *client, const char *name, const char *type, unsigned long flags, unsigned long buffer_size) {
    (void)client; (void)name; (void)type; (void)flags; (void)buffer_size;
    return (jack_port_t*)calloc(1,1);
}

int jack_set_process_callback(jack_client_t *client, JackProcessCallback cb, void *arg) {
    dummy_client *c = (dummy_client*)client; c->process=cb; c->arg=arg; return 0;
}

int jack_activate(jack_client_t *client) { (void)client; return 0; }

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t nframes) { (void)port; (void)nframes; return NULL; }

uint32_t jack_midi_get_event_count(void *buf) { (void)buf; return 0; }

int jack_midi_event_get(jack_midi_event_t *ev, void *buf, uint32_t index) { (void)ev; (void)buf; (void)index; return -1; }

const char **jack_get_ports(jack_client_t *client, const char *port_name_pattern, const char *type_name_pattern, unsigned long flags) { (void)client;(void)port_name_pattern;(void)type_name_pattern;(void)flags; return NULL; }

int jack_connect(jack_client_t *client, const char *source_port, const char *dest_port) { (void)client;(void)source_port;(void)dest_port; return 0; }

const char *jack_port_name(const jack_port_t *port) { (void)port; return "port"; }

#endif
