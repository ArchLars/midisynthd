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

#ifndef MIDISYNTHD_CONFIG_H
#define MIDISYNTHD_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration file paths */
#define CONFIG_SYSTEM_PATH          "/etc/midisynthd.conf"
#define CONFIG_USER_PATH            "/.config/midisynthd.conf"
#define CONFIG_DEFAULT_SOUNDFONT_PATH "/usr/share/sounds/sf2/FluidR3_GM.sf2"

/* Default values */
#define CONFIG_DEFAULT_CLIENT_NAME   "MidiSynth Daemon"
#define CONFIG_DEFAULT_SAMPLE_RATE   48000
#define CONFIG_DEFAULT_POLYPHONY     256
#define CONFIG_DEFAULT_GAIN          0.5f
#define CONFIG_DEFAULT_CHORUS_LEVEL  1.2f
#define CONFIG_DEFAULT_REVERB_LEVEL  0.9f
#define CONFIG_DEFAULT_BUFFER_SIZE   512
#define CONFIG_DEFAULT_AUDIO_PERIODS 4

/* String and path length limits */
#define CONFIG_MAX_PATH_LEN         512
#define CONFIG_MAX_STRING_LEN       128
#define CONFIG_MAX_SOUNDFONTS       8
#define CONFIG_MAX_MIDI_CHANNELS    16

/* Logging levels */
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN  = 1,
    LOG_LEVEL_INFO  = 2,
    LOG_LEVEL_DEBUG = 3
} log_level_t;

/* Audio driver types */
typedef enum {
    AUDIO_DRIVER_AUTO = 0,
    AUDIO_DRIVER_JACK,
    AUDIO_DRIVER_PIPEWIRE,
    AUDIO_DRIVER_PULSEAUDIO,
    AUDIO_DRIVER_ALSA,
    AUDIO_DRIVER_COUNT
} audio_driver_t;

/* MIDI driver types */
typedef enum {
    MIDI_DRIVER_ALSA_SEQ = 0,
    MIDI_DRIVER_ALSA_RAW,
    MIDI_DRIVER_JACK,
    MIDI_DRIVER_COUNT
} midi_driver_t;

/* Audio driver names for display and configuration */
extern const char *audio_driver_names[];
extern const char *midi_driver_names[MIDI_DRIVER_COUNT];

/* SoundFont configuration */
typedef struct {
    char path[CONFIG_MAX_PATH_LEN];
    bool enabled;
    int bank_offset;
    float gain_offset;
} soundfont_config_t;

/* Audio configuration */
typedef struct {
    audio_driver_t driver;
    char device[CONFIG_MAX_STRING_LEN];
    int sample_rate;
    int buffer_size;
    int buffer_count;
    bool realtime_priority;
    int priority_level;
} audio_config_t;

/* MIDI configuration */
typedef struct {
    midi_driver_t driver;
    char device[CONFIG_MAX_STRING_LEN];
    bool autoconnect;
    bool announce;
    char client_name[CONFIG_MAX_STRING_LEN];
    int client_id;
} midi_config_t;

/* Synthesis engine configuration */
typedef struct {
    int polyphony;
    float gain;
    bool interpolation;
    
    /* Chorus settings */
    bool chorus_enabled;
    int chorus_voices;
    float chorus_level;
    float chorus_speed;
    float chorus_depth;
    int chorus_type;
    
    /* Reverb settings */
    bool reverb_enabled;
    float reverb_level;
    float reverb_roomsize;
    float reverb_damping;
    float reverb_width;
} synth_config_t;

/* Main configuration structure used by config.c */
typedef struct midisynthd_config_t {
    log_level_t log_level;
    audio_driver_t audio_driver;
    midi_driver_t midi_driver;
    int sample_rate;
    int buffer_size;
    int audio_periods;
    float gain;
    char client_name[CONFIG_MAX_STRING_LEN];
    bool midi_autoconnect;
    int polyphony;
    bool chorus_enabled;
    float chorus_level;
    bool reverb_enabled;
    float reverb_level;
    soundfont_config_t soundfonts[CONFIG_MAX_SOUNDFONTS];
    int soundfont_count;
    bool realtime_priority;
    char user[CONFIG_MAX_STRING_LEN];
    char group[CONFIG_MAX_STRING_LEN];
} midisynthd_config_t;

/* Configuration validation result codes */
#define CONFIG_VALID                 0
#define CONFIG_INVALID              -1
#define CONFIG_WARNINGS_FIXED        1

/* Function prototypes */

/**
 * Initialize configuration structure with default values
 * @param config Configuration structure to initialize
 */
void config_init_defaults(midisynthd_config_t *config);

/**
 * Load configuration from standard locations
 * Loads system config first, then user config for overrides
 * @param config Configuration structure to populate
 * @return 0 on success, -1 on error
 */
int config_load(midisynthd_config_t *config);

/**
 * Load configuration from a specific file
 * @param config Configuration structure to populate
 * @param filename Path to configuration file
 * @return 0 on success, -1 on error
 */
int config_load_file(midisynthd_config_t *config, const char *filename);

/**
 * Validate configuration values and fix invalid ones
 * @param config Configuration structure to validate
 * @return CONFIG_VALID if all values valid,
 *         CONFIG_WARNINGS_FIXED if some values were corrected,
 *         CONFIG_INVALID if critical errors found
 */
int config_validate(midisynthd_config_t *config);

/**
 * Save configuration to a file
 * @param config Configuration structure to save
 * @param filename Output file path
 * @return 0 on success, -1 on error
 */
int config_save(const midisynthd_config_t *config, const char *filename);

/**
 * Print configuration to stdout (for testing and debugging)
 * @param config Configuration structure to print
 */
void config_print(const midisynthd_config_t *config);

/**
 * Clean up any dynamically allocated configuration resources
 * @param config Configuration structure to clean up
 */
void config_cleanup(midisynthd_config_t *config);

/**
 * Get the default system configuration file path
 * @return Path to system configuration file
 */
const char* config_get_system_path(void);

/**
 * Get the user configuration file path
 * Allocates memory that must be freed by caller
 * @return Path to user configuration file, or NULL on error
 */
char* config_get_user_path(void);

/**
 * Check if a configuration file exists and is readable
 * @param filename Path to configuration file
 * @return true if file exists and is readable, false otherwise
 */
bool config_file_exists(const char *filename);

/**
 * Parse a log level string to enum value
 * @param level_str String representation of log level
 * @return Log level enum value, or LOG_LEVEL_INFO if invalid
 */
log_level_t config_parse_log_level(const char *level_str);

/**
 * Parse an audio driver string to enum value
 * @param driver_str String representation of audio driver
 * @return Audio driver enum value, or AUDIO_DRIVER_AUTO if invalid
 */
audio_driver_t config_parse_audio_driver(const char *driver_str);

/**
 * Parse a MIDI driver string to enum value
 * @param driver_str String representation of MIDI driver
 * @return MIDI driver enum value, or MIDI_DRIVER_ALSA_SEQ if invalid
 */
midi_driver_t config_parse_midi_driver(const char *driver_str);

/**
 * Convert log level enum to string
 * @param level Log level enum value
 * @return String representation of log level
 */
const char* config_log_level_to_string(log_level_t level);

/**
 * Convert audio driver enum to string
 * @param driver Audio driver enum value
 * @return String representation of audio driver
 */
const char* config_audio_driver_to_string(audio_driver_t driver);

/**
 * Convert MIDI driver enum to string
 * @param driver MIDI driver enum value
 * @return String representation of MIDI driver
 */
const char* config_midi_driver_to_string(midi_driver_t driver);

/**
 * Merge user configuration into system configuration
 * User settings override system settings where specified
 * @param system_config System configuration (modified in place)
 * @param user_config User configuration to merge
 */
void config_merge(midisynthd_config_t *system_config, const midisynthd_config_t *user_config);

/**
 * Check if soundfont file exists and is valid
 * @param path Path to soundfont file
 * @return true if soundfont is valid, false otherwise
 */
bool config_validate_soundfont(const char *path);

/**
 * Find the first available soundfont in standard locations
 * @param found_path Buffer to store found soundfont path
 * @param path_len Maximum length of found_path buffer
 * @return true if soundfont found, false otherwise
 */
bool config_find_default_soundfont(char *found_path, size_t path_len);

/**
 * Set configuration option from string key-value pair
 * Used for command line overrides and dynamic configuration
 * @param config Configuration structure to modify
 * @param key Configuration key (e.g., "audio.driver")
 * @param value Configuration value as string
 * @return 0 on success, -1 on error
 */
int config_set_option(midisynthd_config_t *config, const char *key, const char *value);

/**
 * Get configuration option as string
 * @param config Configuration structure to read from
 * @param key Configuration key
 * @param value Buffer to store value string
 * @param value_len Maximum length of value buffer
 * @return 0 on success, -1 on error
 */
int config_get_option(const midisynthd_config_t *config, const char *key, char *value, size_t value_len);

#ifdef __cplusplus
}
#endif

#endif /* MIDISYNTHD_CONFIG_H */
