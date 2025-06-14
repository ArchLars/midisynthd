#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <alsa/asoundlib.h>

#include "config.h"
#include "synth.h"

static void test_midi_event_dispatch(void **state) {
    (void)state;
    midisynthd_config_t cfg;
    config_init_defaults(&cfg);
    cfg.audio_driver = AUDIO_DRIVER_ALSA;

    synth_t *s = synth_init(&cfg, NULL);
    assert_non_null(s);

    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.type = SND_SEQ_EVENT_NOTEON;
    ev.data.note.channel = 0;
    ev.data.note.note = 60;
    ev.data.note.velocity = 100;
    assert_int_equal(synth_handle_midi_event(s, &ev), 0);

    ev.type = SND_SEQ_EVENT_NOTEOFF;
    ev.data.note.velocity = 0;
    assert_int_equal(synth_handle_midi_event(s, &ev), 0);

    synth_cleanup(s);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_midi_event_dispatch),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
