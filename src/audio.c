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
#include <syslog.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pulse/pulseaudio.h>
#include <pulse/error.h>
#include <fluidsynth.h>

#include "audio.h"
#include "config.h"

/* Audio driver name strings for FluidSynth */
const char *audio_driver_names[] = {
    "auto",       /* AUDIO_DRIVER_AUTO */
    "jack",       /* AUDIO_DRIVER_JACK */
    "pipewire",   /* AUDIO_DRIVER_PIPEWIRE */
    "pulseaudio", /* AUDIO_DRIVER_PULSEAUDIO */
    "alsa"        /* AUDIO_DRIVER_ALSA */
};

/* Audio subsystem structure */
struct audio_t {
    audio_driver_t detected_driver;
    fluid_audio_driver_t *fluid_driver;
    fluid_settings_t *fluid_settings;
    const midisynthd_config_t *config;
};

/**
 * Check if JACK server is running by attempting to connect
 */
static bool detect_jack(void) {
    /* Try to detect JACK by checking for the JACK daemon socket */
    const char *jack_server = getenv("JACK_DEFAULT_SERVER");
    if (!jack_server) {
        jack_server = "default";
    }
    
    /* Try to connect to JACK server socket */
    char socket_path[256];
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    
    snprintf(socket_path, sizeof(socket_path), "%s/jack-%d/%s", 
             tmpdir, getuid(), jack_server);
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    bool jack_available = (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    close(sock);
    
    if (jack_available) {
        syslog(LOG_DEBUG, "JACK server detected at %s", socket_path);
    }
    
    return jack_available;
}

/**
 * Check if PipeWire is available and running
 */
static bool detect_pipewire(void) {
    /* Check if PipeWire runtime directory exists */
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/tmp";
    }
    
    char pipewire_path[256];
    snprintf(pipewire_path, sizeof(pipewire_path), "%s/pipewire-0", runtime_dir);
    
    if (access(pipewire_path, F_OK) == 0) {
        syslog(LOG_DEBUG, "PipeWire runtime directory detected at %s", pipewire_path);
        return true;
    }
    
    /* Also check for PipeWire process */
    FILE *proc = popen("pidof pipewire 2>/dev/null", "r");
    if (proc) {
        char buffer[32];
        bool found = (fgets(buffer, sizeof(buffer), proc) != NULL);
        pclose(proc);
        
        if (found) {
            syslog(LOG_DEBUG, "PipeWire process detected");
            return true;
        }
    }
    
    return false;
}

/**
 * Check if PulseAudio is available and running
 */
static bool detect_pulseaudio(void) {
    /* Try to connect to PulseAudio server */
    pa_mainloop *mainloop = pa_mainloop_new();
    if (!mainloop) {
        return false;
    }
    
    pa_mainloop_api *api = pa_mainloop_get_api(mainloop);
    pa_context *context = pa_context_new(api, "midisynthd-detect");
    if (!context) {
        pa_mainloop_free(mainloop);
        return false;
    }
    
    /* Try to connect with a short timeout */
    pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL);
    
    bool pulse_available = false;
    int retries = 10; /* ~100ms timeout */
    
    while (retries > 0 && pa_context_get_state(context) == PA_CONTEXT_CONNECTING) {
        pa_mainloop_iterate(mainloop, 0, NULL);
        usleep(10000); /* 10ms */
        retries--;
    }
    
    if (pa_context_get_state(context) == PA_CONTEXT_READY) {
        pulse_available = true;
        syslog(LOG_DEBUG, "PulseAudio server detected and ready");
    }
    
    pa_context_disconnect(context);
    pa_context_unref(context);
    pa_mainloop_free(mainloop);
    
    return pulse_available;
}

/**
 * Detect the best available audio driver based on priority
 * Priority: JACK > PipeWire > PulseAudio > ALSA
 */
static audio_driver_t detect_audio_driver(audio_driver_t requested_driver) {
    if (requested_driver != AUDIO_DRIVER_AUTO) {
        /* User explicitly requested a specific driver */
        syslog(LOG_INFO, "User requested audio driver: %s", 
               audio_driver_names[requested_driver]);
        return requested_driver;
    }
    
    syslog(LOG_DEBUG, "Auto-detecting audio subsystem");
    
    /* Check in priority order */
    if (detect_jack()) {
        syslog(LOG_INFO, "Auto-detected audio driver: JACK");
        return AUDIO_DRIVER_JACK;
    }
    
    if (detect_pipewire()) {
        syslog(LOG_INFO, "Auto-detected audio driver: PipeWire");
        return AUDIO_DRIVER_PIPEWIRE;
    }
    
    if (detect_pulseaudio()) {
        syslog(LOG_INFO, "Auto-detected audio driver: PulseAudio");
        return AUDIO_DRIVER_PULSEAUDIO;
    }
    
    /* Fall back to ALSA */
    syslog(LOG_INFO, "Auto-detected audio driver: ALSA (fallback)");
    return AUDIO_DRIVER_ALSA;
}

/**
 * Configure FluidSynth audio settings based on configuration
 */
static int configure_audio_settings(fluid_settings_t *settings, 
                                   const midisynthd_config_t *config,
                                   audio_driver_t driver) {
    /* Set the audio driver */
    if (fluid_settings_setstr(settings, "audio.driver", 
                             audio_driver_names[driver]) != FLUID_OK) {
        syslog(LOG_ERR, "Failed to set FluidSynth audio driver to %s", 
               audio_driver_names[driver]);
        return -1;
    }
    
    /* Set sample rate */
    if (fluid_settings_setnum(settings, "synth.sample-rate", 
                             config->sample_rate) != FLUID_OK) {
        syslog(LOG_ERR, "Failed to set FluidSynth sample rate to %d", 
               config->sample_rate);
        return -1;
    }
    
    /* Set audio device if specified */
    if (strlen(config->audio_device) > 0) {
        const char *device_setting = NULL;
        
        switch (driver) {
            case AUDIO_DRIVER_JACK:
                device_setting = "audio.jack.id";
                break;
            case AUDIO_DRIVER_PIPEWIRE:
                device_setting = "audio.pipewire.device";
                break;
            case AUDIO_DRIVER_PULSEAUDIO:
                device_setting = "audio.pulseaudio.device";
                break;
            case AUDIO_DRIVER_ALSA:
                device_setting = "audio.alsa.device";
                break;
            default:
                break;
        }
        
        if (device_setting) {
            if (fluid_settings_setstr(settings, device_setting, 
                                     config->audio_device) != FLUID_OK) {
                syslog(LOG_WARNING, "Failed to set audio device to %s", 
                       config->audio_device);
            } else {
                syslog(LOG_INFO, "Set audio device to %s", config->audio_device);
            }
        }
    }
    
    /* Set audio buffer settings */
    if (config->audio_periods > 0) {
        if (fluid_settings_setint(settings, "audio.periods", 
                                 config->audio_periods) != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to set audio periods to %d", 
                   config->audio_periods);
        }
    }
    
    if (config->audio_period_size > 0) {
        if (fluid_settings_setint(settings, "audio.period-size", 
                                 config->audio_period_size) != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to set audio period size to %d", 
                   config->audio_period_size);
        }
    }
    
    /* Set realtime priority if enabled */
    if (config->realtime_priority) {
        if (fluid_settings_setint(settings, "audio.realtime-prio", 1) != FLUID_OK) {
            syslog(LOG_WARNING, "Failed to enable audio realtime priority");
        } else {
            syslog(LOG_DEBUG, "Enabled audio realtime priority");
        }
    }
    
    /* Additional driver-specific settings */
    switch (driver) {
        case AUDIO_DRIVER_JACK:
            /* JACK-specific settings */
            fluid_settings_setint(settings, "audio.jack.autoconnect", 1);
            syslog(LOG_DEBUG, "Configured JACK driver with autoconnect");
            break;
            
        case AUDIO_DRIVER_PIPEWIRE:
            /* PipeWire-specific settings */
            fluid_settings_setstr(settings, "audio.pipewire.media-role", "Music");
            syslog(LOG_DEBUG, "Configured PipeWire driver with Music media role");
            break;
            
        case AUDIO_DRIVER_PULSEAUDIO:
            /* PulseAudio-specific settings */
            fluid_settings_setstr(settings, "audio.pulseaudio.media-role", "music");
            fluid_settings_setstr(settings, "audio.pulseaudio.adjust-latency", "yes");
            syslog(LOG_DEBUG, "Configured PulseAudio driver with music role");
            break;
            
        case AUDIO_DRIVER_ALSA:
            /* ALSA-specific settings */
            if (strlen(config->audio_device) == 0) {
                /* Use default ALSA device if none specified */
                fluid_settings_setstr(settings, "audio.alsa.device", "default");
            }
            syslog(LOG_DEBUG, "Configured ALSA driver");
            break;
            
        default:
            break;
    }
    
    return 0;
}

/**
 * Initialize audio subsystem
 */
audio_t *audio_init(const midisynthd_config_t *config) {
    if (!config) {
        syslog(LOG_ERR, "Invalid configuration passed to audio_init");
        return NULL;
    }
    
    audio_t *audio = calloc(1, sizeof(audio_t));
    if (!audio) {
        syslog(LOG_ERR, "Failed to allocate memory for audio subsystem");
        return NULL;
    }
    
    audio->config = config;
    
    /* Detect the best available audio driver */
    audio->detected_driver = detect_audio_driver(config->audio_driver);
    
    /* Create FluidSynth settings */
    audio->fluid_settings = new_fluid_settings();
    if (!audio->fluid_settings) {
        syslog(LOG_ERR, "Failed to create FluidSynth settings");
        free(audio);
        return NULL;
    }
    
    /* Configure audio settings */
    if (configure_audio_settings(audio->fluid_settings, config, 
                                audio->detected_driver) < 0) {
        delete_fluid_settings(audio->fluid_settings);
        free(audio);
        return NULL;
    }
    
    /* Create FluidSynth audio driver */
    audio->fluid_driver = new_fluid_audio_driver(audio->fluid_settings, NULL);
    if (!audio->fluid_driver) {
        syslog(LOG_ERR, "Failed to create FluidSynth audio driver for %s", 
               audio_driver_names[audio->detected_driver]);
        
        /* Try fallback to ALSA if we weren't already using it */
        if (audio->detected_driver != AUDIO_DRIVER_ALSA) {
            syslog(LOG_WARNING, "Attempting fallback to ALSA audio driver");
            
            audio->detected_driver = AUDIO_DRIVER_ALSA;
            if (configure_audio_settings(audio->fluid_settings, config, 
                                        AUDIO_DRIVER_ALSA) == 0) {
                audio->fluid_driver = new_fluid_audio_driver(audio->fluid_settings, NULL);
            }
        }
        
        if (!audio->fluid_driver) {
            syslog(LOG_ERR, "Failed to create any FluidSynth audio driver");
            delete_fluid_settings(audio->fluid_settings);
            free(audio);
            return NULL;
        }
    }
    
    syslog(LOG_INFO, "Audio subsystem initialized successfully with %s driver", 
           audio_driver_names[audio->detected_driver]);
    
    return audio;
}

/**
 * Clean up audio subsystem
 */
void audio_cleanup(audio_t *audio) {
    if (!audio) {
        return;
    }
    
    syslog(LOG_DEBUG, "Cleaning up audio subsystem");
    
    if (audio->fluid_driver) {
        delete_fluid_audio_driver(audio->fluid_driver);
        audio->fluid_driver = NULL;
    }
    
    if (audio->fluid_settings) {
        delete_fluid_settings(audio->fluid_settings);
        audio->fluid_settings = NULL;
    }
    
    free(audio);
}

/**
 * Get the detected audio driver
 */
audio_driver_t audio_get_driver(const audio_t *audio) {
    if (!audio) {
        return AUDIO_DRIVER_AUTO;
    }
    
    return audio->detected_driver;
}

/**
 * Get FluidSynth settings for use by other modules
 */
fluid_settings_t *audio_get_fluid_settings(const audio_t *audio) {
    if (!audio) {
        return NULL;
    }
    
    return audio->fluid_settings;
}

/**
 * Get FluidSynth audio driver for use by other modules
 */
fluid_audio_driver_t *audio_get_fluid_driver(const audio_t *audio) {
    if (!audio) {
        return NULL;
    }
    
    return audio->fluid_driver;
}

/**
 * Check if audio subsystem is properly initialized
 */
bool audio_is_initialized(const audio_t *audio) {
    return (audio != NULL && 
            audio->fluid_settings != NULL && 
            audio->fluid_driver != NULL);
}
