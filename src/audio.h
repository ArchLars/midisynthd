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

#ifndef MIDISYNTHD_AUDIO_H
#define MIDISYNTHD_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
struct midisynthd_config_t;
typedef struct midisynthd_config_t midisynthd_config_t;
struct audio_s;
typedef struct audio_s audio_t;

/**
 * Audio driver types supported by the daemon
 * These correspond to FluidSynth's audio driver capabilities
 * Order reflects detection/preference priority
 */
typedef enum {
    AUDIO_DRIVER_AUTO = 0,      /* Automatic detection based on system */
    AUDIO_DRIVER_JACK,          /* JACK Audio Connection Kit */
    AUDIO_DRIVER_PIPEWIRE,      /* PipeWire audio server */
    AUDIO_DRIVER_PULSEAUDIO,    /* PulseAudio sound server */
    AUDIO_DRIVER_ALSA,          /* Raw ALSA (fallback) */
    AUDIO_DRIVER_FILE,          /* File output (for testing/recording) */
    AUDIO_DRIVER_COUNT          /* Number of audio drivers */
} audio_driver_t;

/**
 * Audio driver names for display and configuration
 * Maps to audio_driver_t enum values
 */
extern const char* const audio_driver_names[AUDIO_DRIVER_COUNT];

/**
 * Audio buffer size presets for different use cases
 */
typedef enum {
    AUDIO_BUFFER_SIZE_ULTRA_LOW = 32,    /* Ultra-low latency (gaming, live performance) */
    AUDIO_BUFFER_SIZE_LOW = 64,          /* Low latency (interactive use) */
    AUDIO_BUFFER_SIZE_NORMAL = 128,      /* Normal latency (general use) */
    AUDIO_BUFFER_SIZE_HIGH = 256,        /* High latency (less CPU usage) */
    AUDIO_BUFFER_SIZE_ULTRA_HIGH = 512   /* Ultra-high latency (very low CPU) */
} audio_buffer_size_t;

/**
 * Audio sample rate constants
 */
typedef enum {
    AUDIO_SAMPLE_RATE_22050 = 22050,
    AUDIO_SAMPLE_RATE_44100 = 44100,
    AUDIO_SAMPLE_RATE_48000 = 48000,    /* Recommended default */
    AUDIO_SAMPLE_RATE_88200 = 88200,
    AUDIO_SAMPLE_RATE_96000 = 96000
} audio_sample_rate_t;

/**
 * Audio format constants
 */
typedef enum {
    AUDIO_FORMAT_16BIT = 16,
    AUDIO_FORMAT_24BIT = 24,
    AUDIO_FORMAT_32BIT = 32              /* Float format */
} audio_format_t;

/**
 * Audio driver capabilities and status
 */
typedef struct {
    bool available;                      /* Driver is available on system */
    bool active;                         /* Driver is currently running */
    bool realtime_capable;               /* Supports real-time operation */
    const char *description;             /* Human-readable description */
    const char *version;                 /* Driver version if available */
} audio_driver_info_t;

/**
 * Audio system runtime statistics
 */
typedef struct {
    uint32_t sample_rate;                /* Current sample rate */
    uint16_t buffer_size;                /* Current buffer size in frames */
    uint8_t channels;                    /* Number of audio channels */
    uint8_t format_bits;                 /* Audio format bit depth */
    float cpu_load;                      /* CPU load percentage (0.0-100.0) */
    uint64_t xruns;                      /* Number of audio dropouts */
    bool realtime_active;                /* Real-time scheduling active */
} audio_stats_t;

/**
 * Initialize audio subsystem
 * 
 * Detects available audio drivers and initializes the best available
 * driver based on configuration preferences and system capabilities.
 * 
 * @param config Configuration containing audio preferences
 * @return Initialized audio context or NULL on failure
 */
audio_t* audio_init(const midisynthd_config_t *config);

/**
 * Cleanup and shutdown audio subsystem
 * 
 * Properly shuts down the audio driver and releases all resources.
 * Safe to call with NULL pointer.
 * 
 * @param audio Audio context to cleanup
 */
void audio_cleanup(audio_t *audio);

/**
 * Detect available audio drivers on the system
 * 
 * Probes the system to determine which audio drivers are available
 * and their current status.
 * 
 * @param drivers Array to fill with driver info (size AUDIO_DRIVER_COUNT)
 * @return Number of available drivers found
 */
int audio_detect_drivers(audio_driver_info_t drivers[AUDIO_DRIVER_COUNT]);

/**
 * Get the automatically selected audio driver
 * 
 * Performs intelligent detection to choose the best available audio
 * driver based on system state:
 * 1. JACK if server is running
 * 2. PipeWire if available
 * 3. PulseAudio if available
 * 4. ALSA as fallback
 * 
 * @return Best available audio driver
 */
audio_driver_t audio_detect_best_driver(void);

/**
 * Get current audio system statistics
 * 
 * @param audio Audio context
 * @param stats Structure to fill with current statistics
 * @return 0 on success, -1 on error
 */
int audio_get_stats(const audio_t *audio, audio_stats_t *stats);

/**
 * Set audio gain/volume
 * 
 * @param audio Audio context
 * @param gain Gain value (0.0 to 2.0, where 1.0 is unity gain)
 * @return 0 on success, -1 on error
 */
int audio_set_gain(audio_t *audio, float gain);

/**
 * Get current audio gain/volume
 * 
 * @param audio Audio context
 * @return Current gain value or -1.0 on error
 */
float audio_get_gain(const audio_t *audio);

/**
 * Check if audio driver supports a specific feature
 * 
 * @param driver Audio driver type
 * @param feature Feature name (e.g., "realtime", "duplex", "exclusive")
 * @return true if feature is supported, false otherwise
 */
bool audio_driver_supports_feature(audio_driver_t driver, const char *feature);

/**
 * Get human-readable driver status message
 * 
 * @param audio Audio context
 * @return Status string (e.g., "JACK running, 48kHz, 128 frames")
 */
const char* audio_get_status_string(const audio_t *audio);

/**
 * Validate audio configuration parameters
 * 
 * Checks if the given audio parameters are valid and supported
 * by the specified driver.
 * 
 * @param driver Audio driver type
 * @param sample_rate Requested sample rate
 * @param buffer_size Requested buffer size
 * @param channels Number of channels
 * @return 0 if valid, negative error code otherwise
 */
int audio_validate_config(audio_driver_t driver, 
                         uint32_t sample_rate,
                         uint16_t buffer_size,
                         uint8_t channels);

/**
 * Error codes for audio subsystem
 */
#define AUDIO_ERROR_INVALID_DRIVER    -1
#define AUDIO_ERROR_DRIVER_INIT       -2
#define AUDIO_ERROR_NO_DEVICES        -3
#define AUDIO_ERROR_DEVICE_BUSY       -4
#define AUDIO_ERROR_INVALID_FORMAT    -5
#define AUDIO_ERROR_PERMISSION        -6
#define AUDIO_ERROR_REALTIME_FAIL     -7

#ifdef __cplusplus
}
#endif

#endif /* MIDISYNTHD_AUDIO_H */
