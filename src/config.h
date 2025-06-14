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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef MIDISYNTHD_CONFIG_H
#define MIDISYNTHD_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* Configuration file paths */
#define CONFIG_SYSTEM_PATH          "/etc/midisynthd.conf"
#define CONFIG_USER_PATH            "/.config/midisynthd.conf"
#define CONFIG_USER_DIR             "/.config"

/* Maximum path and string lengths */
#define CONFIG_MAX_PATH_LEN         4096
#define CONFIG_MAX_STRING_LEN       512
#define CONFIG_MAX_SOUNDFONTS       16

/* Audio driver types */
typedef enum {
    AUDIO_DRIVER_AUTO = 0,      /* Auto-detect best available driver */
    AUDIO_DRIVER_JACK,          /* JACK Audio Connection Kit */
    AUDIO_DRIVER_PIPEWIRE,      /* PipeWire */
    AUDIO_DRIVER_PULSEAUDIO,    /* PulseAudio */
    AUDIO_DRIVER_ALSA,          /* Raw ALSA */
    AUDIO_DRIVER_COUNT
} audio_driver_t;

/* MIDI driver types */
typedef enum {
    MIDI_DRIVER_AUTO = 0,       /* Auto-detect best available driver */
    MIDI_DRIVER_ALSA_SEQ,       /* ALSA Sequencer */
    MIDI_DRIVER_ALSA_RAW,       /* Raw ALSA MIDI */
    MIDI_DRIVER_COUNT
} midi_driver_t;

/* Log levels */
typedef enum {
    LOG_LEVEL_ERROR = 0,        /* Error messages only */
    LOG_LEVEL_WARN,             /* Warnings and errors */
    LOG_LEVEL_INFO,             /* Informational messages */
    LOG_LEVEL_DEBUG,            /* Debug messages */
    LOG_LEVEL_COUNT
} log_level_t;

/* SoundFont configuration entry */
typedef struct {
    char path[CONFIG_MAX_PATH_LEN];     /* Path to soundfont file */
    int bank_offset;                    /* Bank offset for this soundfont */
    bool enabled;                       /* Whether this soundfont is active */
} soundfont_config_t;

/* Main configuration structure */
typedef struct {
    /* Audio configuration */
    audio_driver_t audio_driver;        /* Preferred audio driver */
    char audio_device[CONFIG_MAX_STRING_LEN];  /* Audio device name */
    int sample_rate;                    /* Audio sample rate (Hz) */
    int audio_periods;                  /* Audio buffer periods */
    int audio_period_size;              /* Audio period size (frames) */
    double gain;                        /* Master gain (0.0 - 1.0) */
    
    /* MIDI configuration */
    midi_driver_t midi_driver;          /* Preferred MIDI driver */
    char midi_device[CONFIG_MAX_STRING_LEN];   /* MIDI device name */
    char client_name[CONFIG_MAX_STRING_LEN];   /* ALSA client name */
    bool midi_autoconnect;              /* Auto-connect to MIDI inputs */
    int midi_channels;                  /* Number of MIDI channels */
    
    /* Synthesis configuration */
    int polyphony;                      /* Maximum polyphony */
    soundfont_config_t soundfonts[CONFIG_MAX_SOUNDFONTS];  /* SoundFont list */
    int soundfont_count;                /* Number of configured soundfonts */
    bool chorus_enabled;                /* Enable chorus effect */
    double chorus_nr;                   /* Chorus voice count */
    double chorus_level;                /* Chorus level */
    double chorus_speed;                /* Chorus speed */
    double chorus_depth;                /* Chorus depth */
    bool reverb_enabled;                /* Enable reverb effect */
    double reverb_roomsize;             /* Reverb room size */
    double reverb_damping;              /* Reverb damping */
    double reverb_width;                /* Reverb width */
    double reverb_level;                /* Reverb level */
    
    /* Daemon configuration */
    bool daemonize;                     /* Run as daemon */
    log_level_t log_level;              /* Logging verbosity */
    char log_file[CONFIG_MAX_PATH_LEN]; /* Log file path (empty for syslog) */
    char pid_file[CONFIG_MAX_PATH_LEN]; /* PID file path */
    
    /* Real-time configuration */
    bool realtime_priority;             /* Enable real-time priority */
    int realtime_prio;                  /* Real-time priority level */
    
    /* Security configuration */
    char user[CONFIG_MAX_STRING_LEN];   /* Run as user (if started as root) */
    char group[CONFIG_MAX_STRING_LEN];  /* Run as group (if started as root) */
} midisynthd_config_t;

/* Default configuration values */
#define CONFIG_DEFAULT_AUDIO_DRIVER         AUDIO_DRIVER_AUTO
#define CONFIG_DEFAULT_AUDIO_DEVICE         ""
#define CONFIG_DEFAULT_SAMPLE_RATE          48000
#define CONFIG_DEFAULT_AUDIO_PERIODS        3
#define CONFIG_DEFAULT_AUDIO_PERIOD_SIZE    256
#define CONFIG_DEFAULT_GAIN                 0.8

#define CONFIG_DEFAULT_MIDI_DRIVER          MIDI_DRIVER_AUTO
#define CONFIG_DEFAULT_MIDI_DEVICE          ""
#define CONFIG_DEFAULT_CLIENT_NAME          "MidiSynth Daemon"
#define CONFIG_DEFAULT_MIDI_AUTOCONNECT     true
#define CONFIG_DEFAULT_MIDI_CHANNELS        16

#define CONFIG_DEFAULT_POLYPHONY            256
#define CONFIG_DEFAULT_SOUNDFONT_PATH       "/usr/share/soundfonts/FluidR3_GM.sf2"
#define CONFIG_DEFAULT_CHORUS_ENABLED       true
#define CONFIG_DEFAULT_CHORUS_NR            3.0
#define CONFIG_DEFAULT_CHORUS_LEVEL         2.0
#define CONFIG_DEFAULT_CHORUS_SPEED         0.3
#define CONFIG_DEFAULT_CHORUS_DEPTH         8.0
#define CONFIG_DEFAULT_REVERB_ENABLED       true
#define CONFIG_DEFAULT_REVERB_ROOMSIZE      0.2
#define CONFIG_DEFAULT_REVERB_DAMPING       0.0
#define CONFIG_DEFAULT_REVERB_WIDTH         0.5
#define CONFIG_DEFAULT_REVERB_LEVEL         0.9

#define CONFIG_DEFAULT_DAEMONIZE            false
#define CONFIG_DEFAULT_LOG_LEVEL            LOG_LEVEL_INFO
#define CONFIG_DEFAULT_LOG_FILE             ""
#define CONFIG_DEFAULT_PID_FILE             "/var/run/midisynthd.pid"

#define CONFIG_DEFAULT_REALTIME_PRIORITY    true
#define CONFIG_DEFAULT_REALTIME_PRIO        50

#define CONFIG_DEFAULT_USER                 ""
#define CONFIG_DEFAULT_GROUP                ""

/* Audio driver names for configuration parsing */
extern const char *audio_driver_names[AUDIO_DRIVER_COUNT];

/* MIDI driver names for configuration parsing */
extern const char *midi_driver_names[MIDI_DRIVER_COUNT];

/* Log level names for configuration parsing */
extern const char *log_level_names[LOG_LEVEL_COUNT];

/* Function declarations */

/**
 * Initialize configuration structure with default values
 * @param config Pointer to configuration structure to initialize
 */
void config_init_defaults(midisynthd_config_t *config);

/**
 * Load configuration from files (system-wide, then user-specific)
 * @param config Pointer to configuration structure to populate
 * @return 0 on success, negative error code on failure
 */
int config_load(midisynthd_config_t *config);

/**
 * Load configuration from a specific file
 * @param config Pointer to configuration structure to populate
 * @param filepath Path to configuration file
 * @return 0 on success, negative error code on failure
 */
int config_load_file(midisynthd_config_t *config, const char *filepath);

/**
 * Validate configuration values and fix invalid entries
 * @param config Pointer to configuration structure to validate
 * @return 0 if valid, positive count of fixed values, negative on critical errors
 */
int config_validate(midisynthd_config_t *config);

/**
 * Print configuration to stdout (for debugging)
 * @param config Pointer to configuration structure to print
 */
void config_print(const midisynthd_config_t *config);

/**
 * Free any dynamically allocated memory in configuration
 * @param config Pointer to configuration structure to cleanup
 */
void config_cleanup(midisynthd_config_t *config);

/**
 * Parse audio driver name to enum value
 * @param name Driver name string
 * @return Driver enum value, AUDIO_DRIVER_AUTO if unknown
 */
audio_driver_t config_parse_audio_driver(const char *name);

/**
 * Parse MIDI driver name to enum value
 * @param name Driver name string
 * @return Driver enum value, MIDI_DRIVER_AUTO if unknown
 */
midi_driver_t config_parse_midi_driver(const char *name);

/**
 * Parse log level name to enum value
 * @param name Log level string
 * @return Log level enum value, LOG_LEVEL_INFO if unknown
 */
log_level_t config_parse_log_level(const char *name);

/**
 * Expand user home directory in path
 * @param path Input path (may contain ~ prefix)
 * @param expanded Buffer to store expanded path
 * @param max_len Maximum length of expanded buffer
 * @return 0 on success, negative error code on failure
 */
int config_expand_path(const char *path, char *expanded, size_t max_len);

#endif /* MIDISYNTHD_CONFIG_H */
