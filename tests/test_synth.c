#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "config.h"
#include "synth.h"

static void test_synth_basic(void **state) {
    (void)state;
    midisynthd_config_t cfg;
    config_init_defaults(&cfg);
    /* Use ALSA driver; may fall back automatically */
    cfg.audio_driver = AUDIO_DRIVER_ALSA;

    synth_t *s = synth_init(&cfg, NULL);
    assert_non_null(s);

    assert_int_equal(synth_note_on(s, 0, 60, 100), 0);
    assert_int_equal(synth_note_off(s, 0, 60, 0), 0);

    synth_cleanup(s);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_synth_basic),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
