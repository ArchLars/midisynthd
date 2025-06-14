#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>

#include "config.h"

static void test_load_precedence(void **state) {
    (void)state;
    midisynthd_config_t cfg;
    config_init_defaults(&cfg);

    const char *sys_conf = "/tmp/midisynthd_sys.conf";
    FILE *f = fopen(sys_conf, "w");
    assert_non_null(f);
    fprintf(f, "gain=0.5\nsample_rate=44100\n");
    fclose(f);
    assert_int_equal(config_load_file(&cfg, sys_conf), 0);
    assert_int_equal(cfg.sample_rate, 44100);
    assert_true(cfg.gain - 0.5f < 0.01f);

    const char *user_conf = "/tmp/midisynthd_user.conf";
    f = fopen(user_conf, "w");
    assert_non_null(f);
    fprintf(f, "gain=1.2\n");
    fclose(f);
    assert_int_equal(config_load_file(&cfg, user_conf), 0);
    assert_true(cfg.gain - 1.2f < 0.01f);

    remove(sys_conf);
    remove(user_conf);
}

static void test_validation(void **state) {
    (void)state;
    midisynthd_config_t cfg;
    config_init_defaults(&cfg);
    cfg.sample_rate = 1; /* invalid */
    cfg.buffer_size = 10; /* invalid */
    int ret = config_validate(&cfg);
    assert_int_equal(ret, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_load_precedence),
        cmocka_unit_test(test_validation),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
