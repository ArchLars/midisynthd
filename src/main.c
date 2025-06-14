/*
 * midisynthd - System-level MIDI Synthesizer Daemon for Linux
 * Copyright (C) 2025 ArchLars
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "config.h"
#include "synth.h"
#include "midi_alsa.h"
#include "audio.h"
#include "daemonize.h"

#define PROGRAM_NAME "midisynthd"
#define PROGRAM_VERSION "1.0.0"
#define DEFAULT_CONFIG_PATH "/etc/midisynthd.conf"
#define USER_CONFIG_PATH "/.config/midisynthd.conf"

/* Global state */
static volatile int g_running = 1;
static config_t *g_config = NULL;
static synth_t *g_synth = NULL;
static midi_alsa_t *g_midi = NULL;
static audio_t *g_audio = NULL;

/* Command line options */
static struct option long_options[] = {
    {"help",       no_argument,       0, 'h'},
    {"version",    no_argument,       0, 'v'},
    {"config",     required_argument, 0, 'c'},
    {"daemonize",  no_argument,       0, 'd'},
    {"verbose",    no_argument,       0, 'V'},
    {"quiet",      no_argument,       0, 'q'},
    {"test-config", no_argument,      0, 't'},
    {"soundfont",  required_argument, 0, 's'},
    {0, 0, 0, 0}
};

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("System-level MIDI synthesizer daemon for Linux\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help          Show this help message and exit\n");
    printf("  -v, --version       Show version information and exit\n");
    printf("  -c, --config FILE   Use specified configuration file\n");
    printf("  -d, --daemonize     Run as daemon (default: foreground for systemd)\n");
    printf("  -V, --verbose       Enable verbose logging\n");
    printf("  -q, --quiet         Reduce logging output\n");
    printf("  -t, --test-config   Test configuration and exit\n");
    printf("  -s, --soundfont SF2 Override soundfont file path\n");
    printf("\n");
    printf("Files:\n");
    printf("  System config:      %s\n", DEFAULT_CONFIG_PATH);
    printf("  User config:        ~%s\n", USER_CONFIG_PATH);
    printf("\n");
    printf("Report bugs to: https://github.com/ArchLars/midisynthd/issues\n");
}

static void print_version(void) {
    printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("Copyright (C) 2025 ArchLars\n");
    printf("License LGPLv2.1: GNU LGPL version 2.1 <http://gnu.org/licenses/lgpl-2.1.html>\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

static void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            syslog(LOG_INFO, "Received signal %d, shutting down gracefully", sig);
            g_running = 0;
            break;
        case SIGHUP:
            syslog(LOG_INFO, "Received SIGHUP, reloading configuration");
            // TODO: Implement config reload
            break;
        default:
            syslog(LOG_WARNING, "Received unexpected signal %d", sig);
            break;
    }
}

static int setup_signals(void) {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGTERM, &sa, NULL) < 0 ||
        sigaction(SIGINT, &sa, NULL) < 0 ||
        sigaction(SIGHUP, &sa, NULL) < 0) {
        syslog(LOG_ERR, "Failed to setup signal handlers: %s", strerror(errno));
        return -1;
    }
    
    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);
    
    return 0;
}

static char* get_user_config_path(void) {
    const char *home = getenv("HOME");
    if (!home) {
        return NULL;
    }
    
    char *path = malloc(strlen(home) + strlen(USER_CONFIG_PATH) + 1);
    if (!path) {
        return NULL;
    }
    
    strcpy(path, home);
    strcat(path, USER_CONFIG_PATH);
    return path;
}

static int load_configuration(const char *config_file, const char *soundfont_override) {
    char *user_config_path = NULL;
    int ret = 0;
    
    /* Load system-wide configuration first */
    g_config = config_load(config_file ? config_file : DEFAULT_CONFIG_PATH);
    if (!g_config) {
        syslog(LOG_WARNING, "Failed to load system config, using defaults");
        g_config = config_create_default();
        if (!g_config) {
            syslog(LOG_ERR, "Failed to create default configuration");
            return -1;
        }
    }
    
    /* Load user configuration if available (overrides system config) */
    user_config_path = get_user_config_path();
    if (user_config_path) {
        if (access(user_config_path, R_OK) == 0) {
            if (config_merge_user(g_config, user_config_path) < 0) {
                syslog(LOG_WARNING, "Failed to merge user configuration");
            } else {
                syslog(LOG_INFO, "Loaded user configuration from %s", user_config_path);
            }
        }
        free(user_config_path);
    }
    
    /* Apply command line overrides */
    if (soundfont_override) {
        if (config_set_soundfont_path(g_config, soundfont_override) < 0) {
            syslog(LOG_ERR, "Failed to set soundfont override");
            ret = -1;
        }
    }
    
    return ret;
}

static int initialize_modules(void) {
    int ret = 0;
    
    syslog(LOG_INFO, "Initializing audio subsystem");
    g_audio = audio_init(g_config);
    if (!g_audio) {
        syslog(LOG_ERR, "Failed to initialize audio subsystem");
        return -1;
    }
    
    syslog(LOG_INFO, "Initializing FluidSynth engine");
    g_synth = synth_init(g_config, g_audio);
    if (!g_synth) {
        syslog(LOG_ERR, "Failed to initialize synthesizer engine");
        return -1;
    }
    
    syslog(LOG_INFO, "Initializing ALSA MIDI input");
    g_midi = midi_alsa_init(g_config, g_synth);
    if (!g_midi) {
        syslog(LOG_ERR, "Failed to initialize MIDI input");
        return -1;
    }
    
    syslog(LOG_INFO, "All modules initialized successfully");
    return 0;
}

static void cleanup_modules(void) {
    syslog(LOG_INFO, "Cleaning up modules");
    
    if (g_midi) {
        midi_alsa_cleanup(g_midi);
        g_midi = NULL;
    }
    
    if (g_synth) {
        synth_cleanup(g_synth);
        g_synth = NULL;
    }
    
    if (g_audio) {
        audio_cleanup(g_audio);
        g_audio = NULL;
    }
    
    if (g_config) {
        config_cleanup(g_config);
        g_config = NULL;
    }
}

static int main_loop(void) {
    syslog(LOG_INFO, "MidiSynth daemon started successfully");
    
#ifdef HAVE_SYSTEMD
    /* Notify systemd that we're ready */
    sd_notify(0, "READY=1");
#endif
    
    /* Main event loop */
    while (g_running) {
        /* Process MIDI events */
        if (midi_alsa_process_events(g_midi, 100) < 0) {
            syslog(LOG_ERR, "Error processing MIDI events");
            break;
        }
        
        /* Brief sleep to prevent busy waiting */
        usleep(1000); /* 1ms */
    }
    
#ifdef HAVE_SYSTEMD
    /* Notify systemd that we're stopping */
    sd_notify(0, "STOPPING=1");
#endif
    
    syslog(LOG_INFO, "MidiSynth daemon shutting down");
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;
    int option_index = 0;
    int daemonize = 0;
    int verbose = 0;
    int quiet = 0;
    int test_config = 0;
    char *config_file = NULL;
    char *soundfont_override = NULL;
    int ret = EXIT_SUCCESS;
    
    /* Parse command line arguments */
    while ((opt = getopt_long(argc, argv, "hvc:dVqts:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'd':
                daemonize = 1;
                break;
            case 'V':
                verbose = 1;
                break;
            case 'q':
                quiet = 1;
                break;
            case 't':
                test_config = 1;
                break;
            case 's':
                soundfont_override = optarg;
                break;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    /* Check for conflicting options */
    if (verbose && quiet) {
        fprintf(stderr, "Error: --verbose and --quiet options are mutually exclusive\n");
        exit(EXIT_FAILURE);
    }
    
    /* Initialize logging */
    int log_option = LOG_PID;
    int log_level = quiet ? LOG_WARNING : (verbose ? LOG_DEBUG : LOG_INFO);
    
    if (!daemonize) {
        log_option |= LOG_PERROR; /* Also log to stderr */
    }
    
    openlog(PROGRAM_NAME, log_option, LOG_DAEMON);
    setlogmask(LOG_UPTO(log_level));
    
    syslog(LOG_INFO, "Starting %s %s", PROGRAM_NAME, PROGRAM_VERSION);
    
    /* Load configuration */
    if (load_configuration(config_file, soundfont_override) < 0) {
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    /* Test configuration and exit if requested */
    if (test_config) {
        printf("Configuration test successful\n");
        config_print_summary(g_config);
        goto cleanup;
    }
    
    /* Daemonize if requested */
    if (daemonize) {
        if (daemon_init() < 0) {
            syslog(LOG_ERR, "Failed to daemonize");
            ret = EXIT_FAILURE;
            goto cleanup;
        }
    }
    
    /* Setup signal handlers */
    if (setup_signals() < 0) {
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    /* Initialize all modules */
    if (initialize_modules() < 0) {
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    /* Run main event loop */
    if (main_loop() < 0) {
        ret = EXIT_FAILURE;
    }
    
cleanup:
    cleanup_modules();
    closelog();
    
    return ret;
}
