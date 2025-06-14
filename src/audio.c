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

#include "audio.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pulse/pulseaudio.h>
#include <fluidsynth.h>

/* Audio subsystem structure */
struct audio_s {
    audio_driver_t driver_type;
    fluid_settings_t *settings;
    fluid_audio_driver_t *driver;
    bool initialized;
};

/**
 * Check if JACK server is running by attempting to connect to it
 */
static bool is_jack_available(void) {
    /* Try to connect to JACK server socket */
    const char *jack_server_dir = getenv("JACK_SERVER_DIR");
    if (!jack_server_dir) {
        jack_server_dir = "/dev/shm";
    }
    
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "%s/jack-%d/default", jack_server_dir, getuid());
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    bool available = (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    close(sock);
    
    /* Also check for jackd process as fallback */
    if (!available) {
        FILE *fp = popen("pgrep -x jackd >/dev/null 2>&1", "r");
        if (fp) {
            available = (pclose(fp) == 0);
        }
    }
    
    return available;
}

/**
 * Check if PipeWire is available by looking for PipeWire runtime directory
 */
static bool is_pipewire_available(void) {
    /* Check for PipeWire runtime directory */
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        char runtime_path[256];
        snprintf(runtime_path, sizeof(runtime_path), "/run/user/%d", getuid());
        runtime_dir = runtime_path;
    }
    
    char pipewire_path[512];
    snprintf(pipewire_path, sizeof(pipewire_path), "%s/pipewire-0", runtime_dir);
    
    if (access(pipewire_path, F_OK) == 0) {
        return true;
    }
    
    /* Check for running pipewire process */
    FILE *fp = popen("pgrep -x pipewire >/dev/null 2>&1", "r");
    if (fp) {
        return (pclose(fp) == 0);
    }
    
    return false;
}

/**
 * Check if PulseAudio is available
 */
static bool is_pulseaudio_available(void) {
    /* Check for PulseAudio socket */
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir) {
        char pulse_path[512];
        snprintf(pulse_path, sizeof(pulse_path), "%s/pulse/native", runtime_dir);
        if (access(pulse_path, F_OK) == 0) {
            return true;
        }
    }
    
    /* Check for running pulseaudio process */
    FILE *fp = popen("pgrep -x pulseaudio >/dev/null 2>&1", "r");
    if (fp) {
        return (pclose(fp) == 0);
    }
    
    return false;
}

/**
 * Detect the best available audio driver based on running services
 */
static audio_driver_t detect_audio_driver(void) {
    /* Priority order: JACK -> PipeWire -> PulseAudio -> ALSA */
    if (is_jack_available()) {
        syslog(LOG_INFO, "Detected JACK audio server");
        return AUDIO_DRIVER_JACK;
    }
    
    if (is_pipewire_available()) {
        syslog(LOG_INFO, "Detected PipeWire audio server");
        return AUDIO_DRIVER_PIPEWIRE;
    }
    
    if (is_pulseaudio_available()) {
        syslog(LOG_INFO, "Detected PulseAudio server");
        return AUDIO_DRIVER_PULSEAUDIO;
    }
    
    syslog(LOG_INFO, "No audio server detected, falling back to ALSA");
    return AUDIO_DRIVER_ALSA;
}

/**
 * Configure FluidSynth settings for the specified audio driver
 */
static int configure_audio_settings(fluid_settings_t *settings, audio_driver_t driver,
                                   const midisynthd_config_t *config) {
    /* Set audio driver */
    if (fluid_settings_setstr(settings, "audio.driver", audio_driver_names[driver]) != FLUID_OK) {
        syslog(LOG_ERR, "Failed to set audio driver to %s", audio_driver_names[driver]);
        return -1;
    }
    
    /* Set sample rate */
    if (fluid_settings_setnum(settings, "synth.sample-rate", config->sample_rate) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set sample rate to %d", config->sample_rate);
    }
    
    /* Set audio buffer size */
    if (fluid_settings_setint(settings, "audio.period-size", config->buffer_size) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set buffer size to %d", config->buffer_size);
    }
    
    /* Set number of audio buffers */
    if (fluid_settings_setint(settings, "audio.periods", config->audio_periods) != FLUID_OK) {
        syslog(LOG_WARNING, "Failed to set buffer count to %d", config->audio_periods);
    }
    
    /* Set real-time priority if enabled */
    if (config->realtime_priority) {
        if (fluid_settings_setint(settings, "audio.realtime-prio", 1) != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to enable real-time priority for audio");
        }
    }
    
    /* Driver-specific settings */
    switch (driver) {
        case AUDIO_DRIVER_JACK:
            /* JACK-specific settings */
            if (fluid_settings_setint(settings, "audio.jack.autoconnect", 1) != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to enable JACK autoconnect");
            }
            /* Set JACK client name */
            if (fluid_settings_setstr(settings, "audio.jack.id", "midisynthd") != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to set JACK client name");
            }
            break;
            
        case AUDIO_DRIVER_PIPEWIRE:
            /* PipeWire-specific settings (uses JACK compatibility) */
            if (fluid_settings_setint(settings, "audio.jack.autoconnect", 1) != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to enable PipeWire autoconnect");
            }
            if (fluid_settings_setstr(settings, "audio.jack.id", "midisynthd") != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to set PipeWire client name");
            }
            break;
            
        case AUDIO_DRIVER_PULSEAUDIO:
            /* PulseAudio-specific settings */
            if (fluid_settings_setstr(settings, "audio.pulseaudio.server", "default") != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to set PulseAudio server");
            }
            if (fluid_settings_setstr(settings, "audio.pulseaudio.device", "midisynthd") != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to set PulseAudio device name");
            }
            break;
            
        case AUDIO_DRIVER_ALSA:
            /* ALSA-specific settings - use default device */
            if (fluid_settings_setstr(settings, "audio.alsa.device", "default") != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to set ALSA device to default");
            }
            break;
            
        default:
            syslog(LOG_WARNING, "Unknown audio driver type: %d", driver);
            break;
    }
    
    return 0;
}

/**
 * Initialize audio subsystem
 */
audio_t *audio_init(const midisynthd_config_t *config) {
    if (!config) {
        syslog(LOG_ERR, "Invalid configuration provided to audio_init");
        return NULL;
    }
    
    audio_t *audio = calloc(1, sizeof(audio_t));
    if (!audio) {
        syslog(LOG_ERR, "Failed to allocate memory for audio subsystem");
        return NULL;
    }
    
    /* Determine which audio driver to use */
    if (config->audio_driver == AUDIO_DRIVER_AUTO) {
        audio->driver_type = detect_audio_driver();
    } else {
        audio->driver_type = config->audio_driver;
        syslog(LOG_INFO, "Using configured audio driver: %s", 
               audio_driver_names[audio->driver_type]);
    }
    
    /* Create FluidSynth settings */
    audio->settings = new_fluid_settings();
    if (!audio->settings) {
        syslog(LOG_ERR, "Failed to create FluidSynth settings");
        goto error;
    }
    
    /* Configure audio settings */
    if (configure_audio_settings(audio->settings, audio->driver_type, config) < 0) {
        syslog(LOG_ERR, "Failed to configure audio settings");
        goto error;
    }
    
    /* Create audio driver */
    audio->driver = new_fluid_audio_driver(audio->settings, NULL);
    if (!audio->driver) {
        syslog(LOG_ERR, "Failed to create %s audio driver", 
               audio_driver_names[audio->driver_type]);
        
        /* Try fallback to ALSA if another driver failed */
        if (audio->driver_type != AUDIO_DRIVER_ALSA) {
            syslog(LOG_WARNING, "Falling back to ALSA audio driver");
            audio->driver_type = AUDIO_DRIVER_ALSA;
            
            if (configure_audio_settings(audio->settings, AUDIO_DRIVER_ALSA, config) == 0) {
                audio->driver = new_fluid_audio_driver(audio->settings, NULL);
            }
        }
        
        if (!audio->driver) {
            syslog(LOG_ERR, "Failed to create any audio driver");
            goto error;
        }
    }
    
    audio->initialized = true;
    
    syslog(LOG_INFO, "Audio subsystem initialized successfully using %s driver", 
           audio_driver_names[audio->driver_type]);
    syslog(LOG_INFO, "Audio settings: %d Hz, %d-frame buffer, %d buffers", 
           config->sample_rate, config->buffer_size, config->audio_periods);
    
    return audio;
    
error:
    audio_cleanup(audio);
    return NULL;
}

/**
 * Get the FluidSynth settings for use by other modules
 */
fluid_settings_t *audio_get_settings(audio_t *audio) {
    if (!audio || !audio->initialized) {
        return NULL;
    }
    return audio->settings;
}

/**
 * Get the audio driver type that is currently in use
 */
audio_driver_t audio_get_driver_type(audio_t *audio) {
    if (!audio || !audio->initialized) {
        return AUDIO_DRIVER_ALSA; /* Default fallback */
    }
    return audio->driver_type;
}

/**
 * Get the audio driver name for logging/display
 */
const char *audio_get_driver_name(audio_t *audio) {
    if (!audio || !audio->initialized) {
        return "unknown";
    }
    return audio_driver_names[audio->driver_type];
}

/**
 * Check if audio subsystem is properly initialized
 */
bool audio_is_initialized(audio_t *audio) {
    return audio && audio->initialized && audio->driver && audio->settings;
}

/**
 * Cleanup audio subsystem
 */
void audio_cleanup(audio_t *audio) {
    if (!audio) {
        return;
    }
    
    if (audio->driver) {
        syslog(LOG_INFO, "Shutting down %s audio driver", 
               audio_driver_names[audio->driver_type]);
        delete_fluid_audio_driver(audio->driver);
        audio->driver = NULL;
    }
    
    if (audio->settings) {
        delete_fluid_settings(audio->settings);
        audio->settings = NULL;
    }
    
    audio->initialized = false;
    free(audio);
}

audio_driver_t audio_detect_best_driver(void) {
    return detect_audio_driver();
}

int audio_detect_drivers(audio_driver_info_t drivers[AUDIO_DRIVER_COUNT]) {
    if (!drivers) return 0;
    memset(drivers, 0, sizeof(audio_driver_info_t) * AUDIO_DRIVER_COUNT);
    drivers[AUDIO_DRIVER_JACK].available = is_jack_available();
    drivers[AUDIO_DRIVER_PIPEWIRE].available = is_pipewire_available();
    drivers[AUDIO_DRIVER_PULSEAUDIO].available = is_pulseaudio_available();
    drivers[AUDIO_DRIVER_ALSA].available = true;
    int count = 0;
    for (int i = 0; i < AUDIO_DRIVER_COUNT; i++)
        if (drivers[i].available) count++;
    return count;
}

int audio_get_stats(const audio_t *audio, audio_stats_t *stats) {
    if (!audio || !stats || !audio->settings) return -1;
    memset(stats, 0, sizeof(*stats));
    double sr;
    if (fluid_settings_getnum(audio->settings, "synth.sample-rate", &sr) == FLUID_OK)
        stats->sample_rate = (uint32_t)sr;
    fluid_settings_getint(audio->settings, "audio.period-size", (int *)&stats->buffer_size);
    stats->channels = 2;
    stats->format_bits = 16;
    return 0;
}

int audio_set_gain(audio_t *audio, float gain) {
    (void)audio; (void)gain; return -1;
}

float audio_get_gain(const audio_t *audio) {
    (void)audio; return -1.0f;
}
