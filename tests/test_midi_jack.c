#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "config.h"
#include "synth.h"
#include "midi_jack.h"

static void test_jack_init(void **state) {
    (void)state;
#ifdef HAVE_JACK
    midisynthd_config_t cfg;
    config_init_defaults(&cfg);
    synth_t *s = synth_init(&cfg, NULL);
    assert_non_null(s);
    midi_jack_t *m = midi_jack_init(&cfg, s);
    assert_non_null(m);
    midi_jack_cleanup(m);
    synth_cleanup(s);
#else
    (void)state;
#endif
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_jack_init),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
