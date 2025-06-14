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
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
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
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "config.h"
#include "synth.h"
#include "midi_alsa.h"
#include "audio.h"
#include "daemonize.h"

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "midisynthd"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0.0"
#endif

/* Global state */
static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_reload_config = 0;
static midisynthd_config_t g_config;
static synth_t *g_synth = NULL;
static midi_alsa_t *g_midi = NULL;
static audio_t *g_audio = NULL;

/* Command line options */
static struct option long_options[] = {
    {"help",        no_argument,       0, 'h'},
    {"version",     no_argument,       0, 'v'},
    {"config",      required_argument, 0, 'c'},
    {"daemonize",   no_argument,       0, 'd'},
    {"verbose",     no_argument,       0, 'V'},
    {"quiet",       no_argument,       0, 'q'},
    {"test-config", no_argument,       0, 't'},
    {"soundfont",   required_argument, 0, 's'},
    {"no-realtime", no_argument,       0, 'n'},
    {"user",        required_argument, 0, 'u'},
    {"group",       required_argument, 0, 'g'},
    {0, 0, 0, 0}
};

/**
 * Print usage information
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("System-level MIDI synthesizer daemon for Linux\n");
    printf("Provides General MIDI synthesis via FluidSynth with ALSA sequencer integration\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help          Show this help message and exit\n");
    printf("  -v, --version       Show version information and exit\n");
    printf("  -c, --config FILE   Use specified configuration file\n");
    printf("  -d, --daemonize     Run as daemon (default: foreground for systemd)\n");
    printf("  -V, --verbose       Enable verbose logging (debug level)\n");
    printf("  -q, --quiet         Reduce logging output (warnings and errors only)\n");
    printf("  -t, --test-config   Test configuration and exit\n");
    printf("  -s, --soundfont SF2 Override default soundfont file path\n");
    printf("  -n, --no-realtime   Disable real-time priority scheduling\n");
    printf("  -u, --user USER     Run as specified user (if started as root)\n");
    printf("  -g, --group GROUP   Run as specified group (if started as root)\n");
    printf("\n");
    printf("Configuration files (in order of precedence):\n");
    printf("  User config:        ~/.config/midisynthd.conf\n");
    printf("  System config:      %s\n", CONFIG_SYSTEM_PATH);
    printf("  Built-in defaults\n");
    printf("\n");
    printf("Default soundfont locations searched:\n");
    printf("  /usr/share/soundfonts/\n");
    printf("  /usr/share/sounds/sf2/\n");
    printf("  " CONFIG_DEFAULT_SOUNDFONT_PATH "\n");
    printf("\n");
    printf("ALSA MIDI integration:\n");
    printf("  The daemon creates an ALSA sequencer client named '%s'\n", CONFIG_DEFAULT_CLIENT_NAME);
    printf("  Connect MIDI devices with: aconnect <source> '%s'\n", CONFIG_DEFAULT_CLIENT_NAME);
    printf("  List available MIDI ports: aconnect -l\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                           # Run in foreground\n", program_name);
    printf("  %s --daemonize               # Run as daemon\n", program_name);
    printf("  %s --test-config             # Test configuration\n", program_name);
    printf("  %s --verbose --config custom.conf  # Debug with custom config\n", program_name);
    printf("\n");
    printf("Report bugs to: https://github.com/ArchLars/midisynthd/issues\n");
}

/**
 * Print version and license information
 */
static void print_version(void) {
    printf("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
    printf("Copyright (C) 2025 ArchLars\n");
    printf("License LGPLv2.1: GNU LGPL version 2.1 <http://gnu.org/licenses/lgpl-2.1.html>\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
    printf("\n");
    printf("Built with:\n");
    printf("  FluidSynth:  General MIDI synthesis engine\n");
    printf("  ALSA:        Linux audio and MIDI subsystem\n");
#ifdef HAVE_SYSTEMD
    printf("  systemd:     Service management and logging\n");
#endif
    printf("\n");
    printf("Audio drivers supported: JACK, PipeWire, PulseAudio, ALSA\n");
    printf("MIDI drivers supported:  ALSA Sequencer, Raw ALSA MIDI\n");
}

/**
 * Signal handler for graceful shutdown and configuration reload
 */
static void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            if (g_config.log_level >= LOG_LEVEL_INFO) {
                syslog(LOG_INFO, "Received signal %d (%s), initiating graceful shutdown", 
                       sig, sig == SIGTERM ? "SIGTERM" : "SIGINT");
            }
            g_running = 0;
            break;
        case SIGHUP:
            if (g_config.log_level >= LOG_LEVEL_INFO) {
                syslog(LOG_INFO, "Received SIGHUP, scheduling configuration reload");
            }
            g_reload_config = 1;
            break;
        case SIGUSR1:
            if (g_config.log_level >= LOG_LEVEL_INFO) {
                syslog(LOG_INFO, "Received SIGUSR1, printing status information");
            }
            /* TODO: Print status to log */
            break;
        default:
            syslog(LOG_WARNING, "Received unexpected signal %d", sig);
            break;
    }
}

/**
 * Setup signal handlers for daemon operation
 */
static int setup_signals(void) {
    struct sigaction sa;
    
    /* Setup signal handler structure */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    /* Install signal handlers */
    if (sigaction(SIGTERM, &sa, NULL) < 0 ||
        sigaction(SIGINT, &sa, NULL) < 0 ||
        sigaction(SIGHUP, &sa, NULL) < 0 ||
        sigaction(SIGUSR1, &sa, NULL) < 0) {
        syslog(LOG_ERR, "Failed to setup signal handlers: %s", strerror(errno));
        return -1;
    }
    
    /* Ignore SIGPIPE to prevent termination on broken pipes */
    signal(SIGPIPE, SIG_IGN);
    
    return 0;
}

/**
 * Get expanded path for user configuration file
 */
static char* get_user_config_path(void) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    
    if (!home) {
        return NULL;
    }
    
    size_t path_len = strlen(home) + strlen(CONFIG_USER_PATH) + 1;
    char *path = malloc(path_len);
    if (!path) {
        return NULL;
    }
    
    snprintf(path, path_len, "%s%s", home, CONFIG_USER_PATH);
    return path;
}

/**
 * Load configuration from files and apply command line overrides
 */
static int load_configuration(const char *config_file, const char *soundfont_override,
                             const char *user_override, const char *group_override,
                             int verbose, int quiet, int no_realtime) {
    char *user_config_path = NULL;
    int ret = 0;
    
    /* Initialize with default values */
    config_init_defaults(&g_config);
    
    /* Load configuration files */
    if (config_file) {
        /* Use specified config file */
        if (config_load_file(&g_config, config_file) < 0) {
            syslog(LOG_ERR, "Failed to load configuration file: %s", config_file);
            return -1;
        }
    } else {
        /* Load configuration with standard precedence */
        if (config_load(&g_config) < 0) {
            syslog(LOG_WARNING, "Failed to load configuration files, using defaults");
        }
    }
    
    /* Apply command line overrides */
    if (verbose) {
        g_config.log_level = LOG_LEVEL_DEBUG;
    } else if (quiet) {
        g_config.log_level = LOG_LEVEL_WARN;
    }
    
    if (soundfont_override) {
        if (strlen(soundfont_override) < CONFIG_MAX_PATH_LEN) {
            strncpy(g_config.soundfonts[0].path, soundfont_override, CONFIG_MAX_PATH_LEN - 1);
            g_config.soundfonts[0].path[CONFIG_MAX_PATH_LEN - 1] = '\0';
            g_config.soundfonts[0].enabled = true;
            g_config.soundfont_count = 1;
        } else {
            syslog(LOG_ERR, "Soundfont path too long: %s", soundfont_override);
            ret = -1;
        }
    }
    
    if (user_override) {
        strncpy(g_config.user, user_override, CONFIG_MAX_STRING_LEN - 1);
        g_config.user[CONFIG_MAX_STRING_LEN - 1] = '\0';
    }
    
    if (group_override) {
        strncpy(g_config.group, group_override, CONFIG_MAX_STRING_LEN - 1);
        g_config.group[CONFIG_MAX_STRING_LEN - 1] = '\0';
    }
    
    if (no_realtime) {
        g_config.realtime_priority = false;
    }
    
    /* Validate configuration */
    int validation_result = config_validate(&g_config);
    if (validation_result < 0) {
        syslog(LOG_ERR, "Configuration validation failed with critical errors");
        ret = -1;
    } else if (validation_result > 0) {
        syslog(LOG_WARNING, "Configuration validation fixed %d invalid values", validation_result);
    }
    
    return ret;
}

/**
 * Drop privileges if running as root
 */
static int drop_privileges(void) {
    if (getuid() != 0) {
        return 0; /* Not running as root */
    }
    
    if (strlen(g_config.user) == 0 && strlen(g_config.group) == 0) {
        syslog(LOG_WARNING, "Running as root without user/group configuration - consider security implications");
        return 0;
    }
    
    /* Drop group privileges first */
    if (strlen(g_config.group) > 0) {
        struct group *gr = getgrnam(g_config.group);
        if (!gr) {
            syslog(LOG_ERR, "Group '%s' not found", g_config.group);
            return -1;
        }
        
        if (setgid(gr->gr_gid) < 0) {
            syslog(LOG_ERR, "Failed to set group to '%s': %s", g_config.group, strerror(errno));
            return -1;
        }
        
        syslog(LOG_INFO, "Changed group to '%s' (gid %d)", g_config.group, gr->gr_gid);
    }
    
    /* Drop user privileges */
    if (strlen(g_config.user) > 0) {
        struct passwd *pw = getpwnam(g_config.user);
        if (!pw) {
            syslog(LOG_ERR, "User '%s' not found", g_config.user);
            return -1;
        }
        
        if (setuid(pw->pw_uid) < 0) {
            syslog(LOG_ERR, "Failed to set user to '%s': %s", g_config.user, strerror(errno));
            return -1;
        }
        
        syslog(LOG_INFO, "Changed user to '%s' (uid %d)", g_config.user, pw->pw_uid);
    }
    
    return 0;
}

/**
 * Initialize all subsystem modules
 */
static int initialize_modules(void) {
    syslog(LOG_INFO, "Initializing audio subsystem");
    g_audio = audio_init(&g_config);
    if (!g_audio) {
        syslog(LOG_ERR, "Failed to initialize audio subsystem");
        return -1;
    }
    
    syslog(LOG_INFO, "Initializing FluidSynth synthesis engine");
    g_synth = synth_init(&g_config, g_audio);
    if (!g_synth) {
        syslog(LOG_ERR, "Failed to initialize synthesizer engine");
        return -1;
    }
    
    syslog(LOG_INFO, "Initializing ALSA MIDI input system");
    g_midi = midi_alsa_init(&g_config, g_synth);
    if (!g_midi) {
        syslog(LOG_ERR, "Failed to initialize MIDI input system");
        return -1;
    }
    
    syslog(LOG_INFO, "All modules initialized successfully");
    return 0;
}

/**
 * Clean up all allocated resources
 */
static void cleanup_modules(void) {
    if (g_config.log_level >= LOG_LEVEL_INFO) {
        syslog(LOG_INFO, "Cleaning up modules and shutting down");
    }
    
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
    
    config_cleanup(&g_config);
}

/**
 * Reload configuration during runtime
 */
static int reload_configuration(void) {
    syslog(LOG_INFO, "Reloading configuration");
    
    /* Save current log level for comparison */
    log_level_t old_log_level = g_config.log_level;
    
    /* Load new configuration */
    midisynthd_config_t new_config;
    config_init_defaults(&new_config);
    
    if (config_load(&new_config) < 0) {
        syslog(LOG_ERR, "Failed to reload configuration, keeping current settings");
        config_cleanup(&new_config);
        return -1;
    }
    
    if (config_validate(&new_config) < 0) {
        syslog(LOG_ERR, "New configuration is invalid, keeping current settings");
        config_cleanup(&new_config);
        return -1;
    }
    
    /* Apply new configuration that can be changed at runtime */
    g_config.log_level = new_config.log_level;
    g_config.gain = new_config.gain;
    g_config.chorus_enabled = new_config.chorus_enabled;
    g_config.chorus_level = new_config.chorus_level;
    g_config.reverb_enabled = new_config.reverb_enabled;
    g_config.reverb_level = new_config.reverb_level;
    
    /* Update log mask if log level changed */
    if (old_log_level != g_config.log_level) {
        int log_mask = LOG_UPTO(LOG_ERR);
        switch (g_config.log_level) {
            case LOG_LEVEL_DEBUG: log_mask = LOG_UPTO(LOG_DEBUG); break;
            case LOG_LEVEL_INFO:  log_mask = LOG_UPTO(LOG_INFO); break;
            case LOG_LEVEL_WARN:  log_mask = LOG_UPTO(LOG_WARNING); break;
            case LOG_LEVEL_ERROR: log_mask = LOG_UPTO(LOG_ERR); break;
            default: break;
        }
        setlogmask(log_mask);
    }
    
    /* TODO: Apply runtime-changeable settings to modules */
    
    syslog(LOG_INFO, "Configuration reloaded successfully");
    config_cleanup(&new_config);
    return 0;
}

/**
 * Main daemon event loop
 */
static int main_loop(void) {
    syslog(LOG_INFO, "%s %s started successfully", PACKAGE_NAME, PACKAGE_VERSION);
    syslog(LOG_INFO, "ALSA client: '%s', Audio driver: %s, MIDI autoconnect: %s",
           g_config.client_name,
           audio_driver_names[g_config.audio_driver],
           g_config.midi_autoconnect ? "enabled" : "disabled");
    
#ifdef HAVE_SYSTEMD
    /* Notify systemd that the service is ready */
    daemon_notify_ready();
    daemon_notify_status("Processing MIDI events");
#endif
    
    /* Main event loop */
    while (g_running) {
        /* Handle configuration reload request */
        if (g_reload_config) {
            g_reload_config = 0;
            reload_configuration();
        }
        
        /* Process MIDI events */
        if (midi_alsa_process_events(g_midi, 100) < 0) {
            syslog(LOG_ERR, "Critical error processing MIDI events");
            break;
        }
        
        /* Brief sleep to prevent busy waiting */
        usleep(1000); /* 1ms */
    }
    
#ifdef HAVE_SYSTEMD
    /* Notify systemd that we're stopping */
    daemon_notify_status("Shutting down gracefully");
#endif
    
    syslog(LOG_INFO, "%s shutting down", PACKAGE_NAME);
    return 0;
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    int opt;
    int option_index = 0;
    int daemonize = 0;
    int verbose = 0;
    int quiet = 0;
    int test_config = 0;
    int no_realtime = 0;
    char *config_file = NULL;
    char *soundfont_override = NULL;
    char *user_override = NULL;
    char *group_override = NULL;
    int ret = EXIT_SUCCESS;
    
    /* Parse command line arguments */
    while ((opt = getopt_long(argc, argv, "hvc:dVqts:nu:g:", long_options, &option_index)) != -1) {
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
            case 'n':
                no_realtime = 1;
                break;
            case 'u':
                user_override = optarg;
                break;
            case 'g':
                group_override = optarg;
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
    
    if (!daemonize && !test_config) {
        log_option |= LOG_PERROR; /* Also log to stderr when in foreground */
    }
    
    openlog(PACKAGE_NAME, log_option, LOG_DAEMON);
    setlogmask(LOG_UPTO(log_level));
    
    syslog(LOG_INFO, "Starting %s %s (PID %d)", PACKAGE_NAME, PACKAGE_VERSION, getpid());
    
    /* Load and validate configuration */
    if (load_configuration(config_file, soundfont_override, user_override, 
                          group_override, verbose, quiet, no_realtime) < 0) {
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    /* Test configuration and exit if requested */
    if (test_config) {
        printf("Configuration test successful\n\n");
        config_print(&g_config);
        goto cleanup;
    }
    
    /* Validate that we can access required files */
    if (g_config.soundfont_count > 0 && g_config.soundfonts[0].enabled) {
        if (access(g_config.soundfonts[0].path, R_OK) != 0) {
            syslog(LOG_ERR, "Cannot access soundfont file: %s (%s)", 
                   g_config.soundfonts[0].path, strerror(errno));
            ret = EXIT_FAILURE;
            goto cleanup;
        }
    } else {
        syslog(LOG_ERR, "No valid soundfont configured");
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    /* Daemonize if requested (before dropping privileges) */
    if (daemonize) {
        if (daemon_init() < 0) {
            syslog(LOG_ERR, "Failed to daemonize process");
            ret = EXIT_FAILURE;
            goto cleanup;
        }
    }
    
    /* Drop privileges if running as root */
    if (drop_privileges() < 0) {
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    /* Setup signal handlers */
    if (setup_signals() < 0) {
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    /* Initialize all subsystem modules */
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
