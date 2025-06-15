#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>

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

static void test_merge_overwrite(void **state) {
    (void)state;
    midisynthd_config_t sys_cfg;
    midisynthd_config_t user_cfg;

    config_init_defaults(&sys_cfg);
    config_init_defaults(&user_cfg);

    sys_cfg.sample_rate = 22050;
    strcpy(sys_cfg.client_name, "SystemName");
    sys_cfg.gain = 0.3f;

    user_cfg.sample_rate = 48000;
    user_cfg.client_name[0] = '\0';
    user_cfg.gain = 1.0f;

    config_merge(&sys_cfg, &user_cfg);

    assert_int_equal(sys_cfg.sample_rate, user_cfg.sample_rate);
    assert_true(sys_cfg.gain - user_cfg.gain < 0.001f);
    assert_string_equal(sys_cfg.client_name, user_cfg.client_name);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_load_precedence),
        cmocka_unit_test(test_validation),
        cmocka_unit_test(test_merge_overwrite),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
