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
#include <fluidsynth.h>
#include "config.h"

/* Forward declarations */
struct midisynthd_config_t;
typedef struct midisynthd_config_t midisynthd_config_t;
struct audio_s;
typedef struct audio_s audio_t;

/* Audio driver type comes from config.h */


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
 * Retrieve the FluidSynth settings instance used by the audio system
 *
 * @param audio Audio context
 * @return FluidSynth settings pointer or NULL if not initialized
 */
fluid_settings_t *audio_get_settings(audio_t *audio);

/**
 * Get the currently active audio driver type
 *
 * @param audio Audio context
 * @return Driver type enum value
 */
audio_driver_t audio_get_driver_type(audio_t *audio);

/**
 * Get a human readable name for the active driver
 */
const char *audio_get_driver_name(audio_t *audio);

/**
 * Determine whether the audio subsystem has been initialized
 */
bool audio_is_initialized(audio_t *audio);



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
