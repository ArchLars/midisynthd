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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <syslog.h>
#include <limits.h>
#include <pwd.h>

/* Configuration file paths */
#define CONFIG_SYSTEM_PATH "/etc/midisynthd.conf"
#define CONFIG_USER_PATH "/.config/midisynthd.conf"

/* Default configuration values */
#define CONFIG_DEFAULT_SOUNDFONT_PATH "/usr/share/soundfonts/FluidR3_GM_GS.sf2"
#define CONFIG_DEFAULT_CLIENT_NAME "MidiSynth Daemon"
#define CONFIG_DEFAULT_GAIN 0.5f
#define CONFIG_DEFAULT_CHORUS_LEVEL 1.2f
#define CONFIG_DEFAULT_REVERB_LEVEL 0.9f
#define CONFIG_DEFAULT_SAMPLE_RATE 48000
#define CONFIG_DEFAULT_POLYPHONY 256
#define CONFIG_DEFAULT_BUFFER_SIZE 512
#define CONFIG_DEFAULT_AUDIO_PERIODS 2

/* Configuration limits */
#define CONFIG_MAX_PATH_LEN 512
#define CONFIG_MAX_STRING_LEN 256
#define CONFIG_MAX_SOUNDFONTS 8
#define CONFIG_MAX_LINE_LEN 1024

/* Audio driver names array (referenced in main.c) */
const char *audio_driver_names[] = {
    "auto",
    "jack",
    "pipewire", 
    "pulseaudio",
    "alsa",
    NULL
};

/**
 * Trim whitespace from the beginning and end of a string
 */
static char* trim_whitespace(char *str) {
    char *end;
    
    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == 0) return str; /* All spaces? */
    
    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    
    /* Write new null terminator */
    end[1] = '\0';
    
    return str;
}

/**
 * Parse log level from string
 */
static log_level_t parse_log_level(const char *str) {
    if (!str) return LOG_LEVEL_INFO;
    
    if (strcasecmp(str, "debug") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "info") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(str, "warn") == 0 || strcasecmp(str, "warning") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(str, "error") == 0) return LOG_LEVEL_ERROR;
    
    return LOG_LEVEL_INFO; /* Default */
}

/**
 * Parse audio driver from string
 */
static audio_driver_t parse_audio_driver(const char *str) {
    if (!str) return AUDIO_DRIVER_AUTO;
    
    if (strcasecmp(str, "auto") == 0) return AUDIO_DRIVER_AUTO;
    if (strcasecmp(str, "jack") == 0) return AUDIO_DRIVER_JACK;
    if (strcasecmp(str, "pipewire") == 0) return AUDIO_DRIVER_PIPEWIRE;
    if (strcasecmp(str, "pulseaudio") == 0 || strcasecmp(str, "pulse") == 0) return AUDIO_DRIVER_PULSEAUDIO;
    if (strcasecmp(str, "alsa") == 0) return AUDIO_DRIVER_ALSA;
    
    return AUDIO_DRIVER_AUTO; /* Default */
}

/**
 * Parse boolean value from string
 */
static bool parse_bool(const char *str) {
    if (!str) return false;
    
    if (strcasecmp(str, "true") == 0 || strcasecmp(str, "yes") == 0 || 
        strcasecmp(str, "on") == 0 || strcasecmp(str, "1") == 0) {
        return true;
    }
    
    return false;
}

/**
 * Parse float value from string with bounds checking
 */
static float parse_float(const char *str, float min_val, float max_val, float default_val) {
    if (!str) return default_val;
    
    char *endptr;
    float val = strtof(str, &endptr);
    
    if (*endptr != '\0' || val < min_val || val > max_val) {
        return default_val;
    }
    
    return val;
}

/**
 * Parse integer value from string with bounds checking
 */
static int parse_int(const char *str, int min_val, int max_val, int default_val) {
    if (!str) return default_val;
    
    char *endptr;
    long val = strtol(str, &endptr, 10);
    
    if (*endptr != '\0' || val < min_val || val > max_val) {
        return default_val;
    }
    
    return (int)val;
}

/**
 * Initialize configuration with default values
 */
void config_init_defaults(midisynthd_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(midisynthd_config_t));
    
    /* Logging */
    config->log_level = LOG_LEVEL_INFO;
    
    /* Audio settings */
    config->audio_driver = AUDIO_DRIVER_AUTO;
    config->sample_rate = CONFIG_DEFAULT_SAMPLE_RATE;
    config->buffer_size = CONFIG_DEFAULT_BUFFER_SIZE;
    config->audio_periods = CONFIG_DEFAULT_AUDIO_PERIODS;
    config->gain = CONFIG_DEFAULT_GAIN;
    
    /* MIDI settings */
    strncpy(config->client_name, CONFIG_DEFAULT_CLIENT_NAME, CONFIG_MAX_STRING_LEN - 1);
    config->client_name[CONFIG_MAX_STRING_LEN - 1] = '\0';
    config->midi_autoconnect = true;
    
    /* Synthesis settings */
    config->polyphony = CONFIG_DEFAULT_POLYPHONY;
    config->chorus_enabled = true;
    config->chorus_level = CONFIG_DEFAULT_CHORUS_LEVEL;
    config->reverb_enabled = true;
    config->reverb_level = CONFIG_DEFAULT_REVERB_LEVEL;
    
    /* Soundfonts - try to find a default */
    config->soundfont_count = 0;
    const char *default_soundfonts[] = {
        CONFIG_DEFAULT_SOUNDFONT_PATH,
        "/usr/share/soundfonts/FluidR3_GM_GS.sf2",
        "/usr/share/soundfonts/GeneralUser_GS.sf2",
        "/usr/share/sounds/sf2/FluidR3_GM_GS.sf2",
        "/usr/share/sounds/sf2/GeneralUser_GS.sf2",
        NULL
    };
    
    for (int i = 0; default_soundfonts[i] && config->soundfont_count < CONFIG_MAX_SOUNDFONTS; i++) {
        if (access(default_soundfonts[i], R_OK) == 0) {
            strncpy(config->soundfonts[config->soundfont_count].path, 
                   default_soundfonts[i], CONFIG_MAX_PATH_LEN - 1);
            config->soundfonts[config->soundfont_count].path[CONFIG_MAX_PATH_LEN - 1] = '\0';
            config->soundfonts[config->soundfont_count].enabled = true;
            config->soundfont_count++;
            break; /* Only use first found soundfont as default */
        }
    }
    
    /* Daemon settings */
    config->realtime_priority = true;
    config->user[0] = '\0';
    config->group[0] = '\0';
}

/**
 * Parse a single configuration line
 */
static void parse_config_line(midisynthd_config_t *config, const char *line) {
    if (!config || !line) return;
    
    /* Skip comments and empty lines */
    if (line[0] == '#' || line[0] == ';' || line[0] == '\0') return;
    
    /* Find the '=' separator */
    char *equals = strchr(line, '=');
    if (!equals) return;
    
    /* Split into key and value */
    char key[CONFIG_MAX_STRING_LEN];
    char value[CONFIG_MAX_STRING_LEN];
    
    size_t key_len = equals - line;
    if (key_len >= sizeof(key)) return;
    
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    
    strncpy(value, equals + 1, sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    
    /* Trim whitespace */
    char *trimmed_key = trim_whitespace(key);
    char *trimmed_value = trim_whitespace(value);
    
    /* Parse configuration options */
    if (strcasecmp(trimmed_key, "log_level") == 0) {
        config->log_level = parse_log_level(trimmed_value);
    }
    else if (strcasecmp(trimmed_key, "audio_driver") == 0) {
        config->audio_driver = parse_audio_driver(trimmed_value);
    }
    else if (strcasecmp(trimmed_key, "sample_rate") == 0) {
        config->sample_rate = parse_int(trimmed_value, 8000, 192000, CONFIG_DEFAULT_SAMPLE_RATE);
    }
    else if (strcasecmp(trimmed_key, "buffer_size") == 0) {
        config->buffer_size = parse_int(trimmed_value, 64, 8192, CONFIG_DEFAULT_BUFFER_SIZE);
    }
    else if (strcasecmp(trimmed_key, "audio_periods") == 0) {
        config->audio_periods = parse_int(trimmed_value, 2, 8, CONFIG_DEFAULT_AUDIO_PERIODS);
    }
    else if (strcasecmp(trimmed_key, "gain") == 0) {
        config->gain = parse_float(trimmed_value, 0.0f, 2.0f, CONFIG_DEFAULT_GAIN);
    }
    else if (strcasecmp(trimmed_key, "client_name") == 0) {
        strncpy(config->client_name, trimmed_value, CONFIG_MAX_STRING_LEN - 1);
        config->client_name[CONFIG_MAX_STRING_LEN - 1] = '\0';
    }
    else if (strcasecmp(trimmed_key, "midi_autoconnect") == 0) {
        config->midi_autoconnect = parse_bool(trimmed_value);
    }
    else if (strcasecmp(trimmed_key, "polyphony") == 0) {
        config->polyphony = parse_int(trimmed_value, 16, 4096, CONFIG_DEFAULT_POLYPHONY);
    }
    else if (strcasecmp(trimmed_key, "chorus_enabled") == 0) {
        config->chorus_enabled = parse_bool(trimmed_value);
    }
    else if (strcasecmp(trimmed_key, "chorus_level") == 0) {
        config->chorus_level = parse_float(trimmed_value, 0.0f, 10.0f, CONFIG_DEFAULT_CHORUS_LEVEL);
    }
    else if (strcasecmp(trimmed_key, "reverb_enabled") == 0) {
        config->reverb_enabled = parse_bool(trimmed_value);
    }
    else if (strcasecmp(trimmed_key, "reverb_level") == 0) {
        config->reverb_level = parse_float(trimmed_value, 0.0f, 10.0f, CONFIG_DEFAULT_REVERB_LEVEL);
    }
    else if (strcasecmp(trimmed_key, "soundfont") == 0 || strcasecmp(trimmed_key, "soundfont_path") == 0) {
        if (config->soundfont_count < CONFIG_MAX_SOUNDFONTS) {
            strncpy(config->soundfonts[config->soundfont_count].path, 
                   trimmed_value, CONFIG_MAX_PATH_LEN - 1);
            config->soundfonts[config->soundfont_count].path[CONFIG_MAX_PATH_LEN - 1] = '\0';
            config->soundfonts[config->soundfont_count].enabled = true;
            config->soundfont_count++;
        }
    }
    else if (strcasecmp(trimmed_key, "realtime_priority") == 0) {
        config->realtime_priority = parse_bool(trimmed_value);
    }
    else if (strcasecmp(trimmed_key, "user") == 0) {
        strncpy(config->user, trimmed_value, CONFIG_MAX_STRING_LEN - 1);
        config->user[CONFIG_MAX_STRING_LEN - 1] = '\0';
    }
    else if (strcasecmp(trimmed_key, "group") == 0) {
        strncpy(config->group, trimmed_value, CONFIG_MAX_STRING_LEN - 1);
        config->group[CONFIG_MAX_STRING_LEN - 1] = '\0';
    }
}

/**
 * Load configuration from a single file
 */
int config_load_file(midisynthd_config_t *config, const char *filename) {
    if (!config || !filename) return -1;
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        if (errno != ENOENT) {
            syslog(LOG_WARNING, "Failed to open config file %s: %s", filename, strerror(errno));
        }
        return -1;
    }
    
    char line[CONFIG_MAX_LINE_LEN];
    int line_number = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        parse_config_line(config, line);
    }
    
    fclose(file);
    syslog(LOG_DEBUG, "Loaded configuration from %s (%d lines)", filename, line_number);
    return 0;
}

/**
 * Get path to user configuration file
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
 * Load configuration with standard precedence (system -> user)
 */
int config_load(midisynthd_config_t *config) {
    if (!config) return -1;
    
    int loaded_files = 0;
    
    /* Load system-wide configuration */
    if (config_load_file(config, CONFIG_SYSTEM_PATH) == 0) {
        loaded_files++;
    }
    
    /* Load user configuration (overrides system config) */
    char *user_config_path = get_user_config_path();
    if (user_config_path) {
        if (config_load_file(config, user_config_path) == 0) {
            loaded_files++;
        }
        free(user_config_path);
    }
    
    if (loaded_files == 0) {
        syslog(LOG_DEBUG, "No configuration files found, using defaults");
        return -1;
    }
    
    syslog(LOG_DEBUG, "Loaded %d configuration file(s)", loaded_files);
    return 0;
}

/**
 * Validate configuration and fix invalid values
 */
int config_validate(midisynthd_config_t *config) {
    if (!config) return -1;
    
    int fixes = 0;
    
    /* Validate sample rate */
    if (config->sample_rate < 8000 || config->sample_rate > 192000) {
        syslog(LOG_WARNING, "Invalid sample rate %d, using default %d", 
               config->sample_rate, CONFIG_DEFAULT_SAMPLE_RATE);
        config->sample_rate = CONFIG_DEFAULT_SAMPLE_RATE;
        fixes++;
    }
    
    /* Validate buffer size (must be power of 2) */
    if (config->buffer_size < 64 || config->buffer_size > 8192) {
        syslog(LOG_WARNING, "Invalid buffer size %d, using default %d", 
               config->buffer_size, CONFIG_DEFAULT_BUFFER_SIZE);
        config->buffer_size = CONFIG_DEFAULT_BUFFER_SIZE;
        fixes++;
    }
    
    /* Validate audio periods */
    if (config->audio_periods < 2 || config->audio_periods > 8) {
        syslog(LOG_WARNING, "Invalid audio periods %d, using default %d", 
               config->audio_periods, CONFIG_DEFAULT_AUDIO_PERIODS);
        config->audio_periods = CONFIG_DEFAULT_AUDIO_PERIODS;
        fixes++;
    }
    
    /* Validate gain */
    if (config->gain < 0.0f || config->gain > 2.0f) {
        syslog(LOG_WARNING, "Invalid gain %.2f, using default %.2f", 
               config->gain, CONFIG_DEFAULT_GAIN);
        config->gain = CONFIG_DEFAULT_GAIN;
        fixes++;
    }
    
    /* Validate polyphony */
    if (config->polyphony < 16 || config->polyphony > 4096) {
        syslog(LOG_WARNING, "Invalid polyphony %d, using default %d", 
               config->polyphony, CONFIG_DEFAULT_POLYPHONY);
        config->polyphony = CONFIG_DEFAULT_POLYPHONY;
        fixes++;
    }
    
    /* Validate chorus level */
    if (config->chorus_level < 0.0f || config->chorus_level > 10.0f) {
        syslog(LOG_WARNING, "Invalid chorus level %.2f, using default %.2f", 
               config->chorus_level, CONFIG_DEFAULT_CHORUS_LEVEL);
        config->chorus_level = CONFIG_DEFAULT_CHORUS_LEVEL;
        fixes++;
    }
    
    /* Validate reverb level */
    if (config->reverb_level < 0.0f || config->reverb_level > 10.0f) {
        syslog(LOG_WARNING, "Invalid reverb level %.2f, using default %.2f", 
               config->reverb_level, CONFIG_DEFAULT_REVERB_LEVEL);
        config->reverb_level = CONFIG_DEFAULT_REVERB_LEVEL;
        fixes++;
    }
    
    /* Validate client name */
    if (strlen(config->client_name) == 0) {
        syslog(LOG_WARNING, "Empty client name, using default");
        strncpy(config->client_name, CONFIG_DEFAULT_CLIENT_NAME, CONFIG_MAX_STRING_LEN - 1);
        config->client_name[CONFIG_MAX_STRING_LEN - 1] = '\0';
        fixes++;
    }
    
    /* Validate soundfonts */
    int valid_soundfonts = 0;
    for (int i = 0; i < config->soundfont_count; i++) {
        if (config->soundfonts[i].enabled && strlen(config->soundfonts[i].path) > 0) {
            if (access(config->soundfonts[i].path, R_OK) != 0) {
                syslog(LOG_WARNING, "Soundfont not accessible: %s (%s)", 
                       config->soundfonts[i].path, strerror(errno));
                config->soundfonts[i].enabled = false;
                fixes++;
            } else {
                valid_soundfonts++;
            }
        }
    }
    
    if (valid_soundfonts == 0) {
        syslog(LOG_ERR, "No valid soundfonts configured");
        return -1; /* This is a critical error */
    }
    
    return fixes;
}

/**
 * Print configuration to stdout
 */
void config_print(const midisynthd_config_t *config) {
    if (!config) return;
    
    printf("Midisynthd Configuration:\n");
    printf("========================\n\n");
    
    printf("Logging:\n");
    printf("  Log Level:          %s\n", 
           config->log_level == LOG_LEVEL_DEBUG ? "debug" :
           config->log_level == LOG_LEVEL_INFO ? "info" :
           config->log_level == LOG_LEVEL_WARN ? "warn" : "error");
    
    printf("\nAudio:\n");
    printf("  Driver:             %s\n", audio_driver_names[config->audio_driver]);
    printf("  Sample Rate:        %d Hz\n", config->sample_rate);
    printf("  Buffer Size:        %d samples\n", config->buffer_size);
    printf("  Audio Periods:      %d\n", config->audio_periods);
    printf("  Gain:               %.2f\n", config->gain);
    
    printf("\nMIDI:\n");
    printf("  Client Name:        %s\n", config->client_name);
    printf("  Auto-connect:       %s\n", config->midi_autoconnect ? "yes" : "no");
    
    printf("\nSynthesis:\n");
    printf("  Polyphony:          %d voices\n", config->polyphony);
    printf("  Chorus:             %s", config->chorus_enabled ? "enabled" : "disabled");
    if (config->chorus_enabled) {
        printf(" (level %.2f)", config->chorus_level);
    }
    printf("\n");
    printf("  Reverb:             %s", config->reverb_enabled ? "enabled" : "disabled");
    if (config->reverb_enabled) {
        printf(" (level %.2f)", config->reverb_level);
    }
    printf("\n");
    
    printf("\nSoundfonts:\n");
    if (config->soundfont_count == 0) {
        printf("  (none configured)\n");
    } else {
        for (int i = 0; i < config->soundfont_count; i++) {
            printf("  [%d] %s: %s\n", i + 1,
                   config->soundfonts[i].enabled ? "enabled" : "disabled",
                   config->soundfonts[i].path);
        }
    }
    
    printf("\nDaemon:\n");
    printf("  Realtime Priority:  %s\n", config->realtime_priority ? "yes" : "no");
    if (strlen(config->user) > 0) {
        printf("  Run as User:        %s\n", config->user);
    }
    if (strlen(config->group) > 0) {
        printf("  Run as Group:       %s\n", config->group);
    }
    
    printf("\n");
}

/**
 * Clean up configuration resources
 */
void config_cleanup(midisynthd_config_t *config) {
    if (!config) return;
    
    /* Currently no dynamic memory to free, but this function
     * is provided for future extensibility */
    memset(config, 0, sizeof(midisynthd_config_t));
}
