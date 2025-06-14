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
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "midi_alsa.h"
#include "config.h"
#include "synth.h"

/**
 * ALSA MIDI input handler structure
 */
struct midi_alsa_s {
    snd_seq_t *seq_handle;           /* ALSA sequencer handle */
    int client_id;                   /* Our sequencer client ID */
    int port_id;                     /* Our input port ID */
    synth_t *synth;                  /* Synthesizer instance to send events to */
    const midisynthd_config_t *config; /* Configuration reference */
    pthread_t thread;                /* MIDI processing thread */
    volatile int running;            /* Thread control flag */
    int poll_fd;                     /* Poll file descriptor for sequencer */
    struct pollfd *poll_fds;         /* Poll file descriptors array */
    int poll_fd_count;               /* Number of poll file descriptors */
};

/**
 * MIDI event processing thread function
 */
static void* midi_thread_func(void *arg) {
    midi_alsa_t *midi = (midi_alsa_t*)arg;
    snd_seq_event_t *ev;
    int ret;
    
    if (midi->config->log_level >= LOG_LEVEL_DEBUG) {
        syslog(LOG_DEBUG, "MIDI processing thread started");
    }
    
    while (midi->running) {
        /* Poll for events with timeout */
        ret = poll(midi->poll_fds, midi->poll_fd_count, 100); /* 100ms timeout */
        
        if (ret < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal, continue */
            }
            syslog(LOG_ERR, "MIDI poll error: %s", strerror(errno));
            break;
        }
        
        if (ret == 0) {
            continue; /* Timeout, check running flag and continue */
        }
        
        /* Process all available events */
        while (midi->running && snd_seq_event_input_pending(midi->seq_handle, 0) > 0) {
            ret = snd_seq_event_input(midi->seq_handle, &ev);
            if (ret < 0) {
                if (ret == -EAGAIN) {
                    break; /* No more events */
                }
                syslog(LOG_ERR, "Error reading MIDI event: %s", snd_strerror(ret));
                break;
            }
            
            /* Forward event to synthesizer */
            if (synth_handle_midi_event(midi->synth, ev) < 0) {
                if (midi->config->log_level >= LOG_LEVEL_DEBUG) {
                    syslog(LOG_DEBUG, "Failed to process MIDI event type %d", ev->type);
                }
            }
            
            /* Free the event if it was allocated */
            snd_seq_free_event(ev);
        }
    }
    
    if (midi->config->log_level >= LOG_LEVEL_DEBUG) {
        syslog(LOG_DEBUG, "MIDI processing thread stopped");
    }
    
    return NULL;
}

/**
 * Setup auto-connection to hardware MIDI devices
 */
static int setup_autoconnect(midi_alsa_t *midi) {
    snd_seq_client_info_t *client_info;
    snd_seq_port_info_t *port_info;
    int client_id, port_id;
    int connected_count = 0;
    
    if (!midi->config->midi_autoconnect) {
        return 0; /* Auto-connect disabled */
    }
    
    snd_seq_client_info_alloca(&client_info);
    snd_seq_port_info_alloca(&port_info);
    
    /* Iterate through all clients */
    snd_seq_client_info_set_client(client_info, -1);
    while (snd_seq_query_next_client(midi->seq_handle, client_info) >= 0) {
        client_id = snd_seq_client_info_get_client(client_info);
        
        /* Skip ourselves and system clients */
        if (client_id == midi->client_id || client_id == SND_SEQ_CLIENT_SYSTEM) {
            continue;
        }
        
        /* Iterate through ports of this client */
        snd_seq_port_info_set_client(port_info, client_id);
        snd_seq_port_info_set_port(port_info, -1);
        while (snd_seq_query_next_port(midi->seq_handle, port_info) >= 0) {
            unsigned int caps = snd_seq_port_info_get_capability(port_info);
            unsigned int type = snd_seq_port_info_get_type(port_info);
            
            /* Check if this is a hardware MIDI output port */
            if ((caps & SND_SEQ_PORT_CAP_READ) &&
                (caps & SND_SEQ_PORT_CAP_SUBS_READ) &&
                (type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) &&
                (type & SND_SEQ_PORT_TYPE_HARDWARE)) {
                
                port_id = snd_seq_port_info_get_port(port_info);
                
                /* Create subscription from hardware port to our port */
                snd_seq_port_subscribe_t *subs;
                snd_seq_port_subscribe_alloca(&subs);
                
                snd_seq_addr_t sender;
                snd_seq_addr_t dest;
                
                sender.client = client_id;
                sender.port = port_id;
                dest.client = midi->client_id;
                dest.port = midi->port_id;
                
                snd_seq_port_subscribe_set_sender(subs, &sender);
                snd_seq_port_subscribe_set_dest(subs, &dest);
                snd_seq_port_subscribe_set_time_update(subs, 1);
                snd_seq_port_subscribe_set_time_real(subs, 1);
                
                if (snd_seq_subscribe_port(midi->seq_handle, subs) >= 0) {
                    connected_count++;
                    if (midi->config->log_level >= LOG_LEVEL_INFO) {
                        syslog(LOG_INFO, "Auto-connected MIDI device: %s:%s (%d:%d)",
                               snd_seq_client_info_get_name(client_info),
                               snd_seq_port_info_get_name(port_info),
                               client_id, port_id);
                    }
                } else {
                    if (midi->config->log_level >= LOG_LEVEL_DEBUG) {
                        syslog(LOG_DEBUG, "Failed to auto-connect to %d:%d", client_id, port_id);
                    }
                }
            }
        }
    }
    
    if (connected_count > 0) {
        syslog(LOG_INFO, "Auto-connected to %d MIDI device(s)", connected_count);
    } else if (midi->config->log_level >= LOG_LEVEL_INFO) {
        syslog(LOG_INFO, "No hardware MIDI devices found for auto-connection");
    }
    
    return 0;
}

/**
 * Initialize ALSA MIDI input system
 */
midi_alsa_t* midi_alsa_init(const midisynthd_config_t *config, synth_t *synth) {
    midi_alsa_t *midi;
    int ret;
    
    if (!config || !synth) {
        syslog(LOG_ERR, "Invalid parameters for MIDI initialization");
        return NULL;
    }
    
    midi = calloc(1, sizeof(midi_alsa_t));
    if (!midi) {
        syslog(LOG_ERR, "Failed to allocate MIDI structure: %s", strerror(errno));
        return NULL;
    }
    
    midi->config = config;
    midi->synth = synth;
    midi->running = 1;
    midi->client_id = -1;
    midi->port_id = -1;
    
    /* Open ALSA sequencer */
    ret = snd_seq_open(&midi->seq_handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to open ALSA sequencer: %s", snd_strerror(ret));
        goto error;
    }
    
    /* Set client name */
    ret = snd_seq_set_client_name(midi->seq_handle, config->client_name);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to set client name: %s", snd_strerror(ret));
        goto error;
    }
    
    /* Get our client ID */
    midi->client_id = snd_seq_client_id(midi->seq_handle);
    if (midi->client_id < 0) {
        syslog(LOG_ERR, "Failed to get client ID: %s", snd_strerror(midi->client_id));
        goto error;
    }
    
    /* Create input port */
    midi->port_id = snd_seq_create_simple_port(midi->seq_handle, "MIDI Input",
                                              SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                              SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (midi->port_id < 0) {
        syslog(LOG_ERR, "Failed to create MIDI input port: %s", snd_strerror(midi->port_id));
        goto error;
    }
    
    /* Setup poll file descriptors for event processing */
    midi->poll_fd_count = snd_seq_poll_descriptors_count(midi->seq_handle, POLLIN);
    if (midi->poll_fd_count <= 0) {
        syslog(LOG_ERR, "No poll descriptors available for sequencer");
        goto error;
    }
    
    midi->poll_fds = calloc(midi->poll_fd_count, sizeof(struct pollfd));
    if (!midi->poll_fds) {
        syslog(LOG_ERR, "Failed to allocate poll descriptors: %s", strerror(errno));
        goto error;
    }
    
    ret = snd_seq_poll_descriptors(midi->seq_handle, midi->poll_fds, midi->poll_fd_count, POLLIN);
    if (ret < 0) {
        syslog(LOG_ERR, "Failed to get poll descriptors: %s", snd_strerror(ret));
        goto error;
    }
    
    /* Set up auto-connection to hardware devices */
    if (setup_autoconnect(midi) < 0) {
        syslog(LOG_WARNING, "Auto-connection setup failed, but continuing");
    }
    
    /* Start MIDI processing thread */
    ret = pthread_create(&midi->thread, NULL, midi_thread_func, midi);
    if (ret != 0) {
        syslog(LOG_ERR, "Failed to create MIDI thread: %s", strerror(ret));
        goto error;
    }
    
    /* Set real-time priority if enabled and possible */
    if (config->realtime_priority) {
        struct sched_param param;
        param.sched_priority = 10; /* Moderate real-time priority */
        
        if (pthread_setschedparam(midi->thread, SCHED_FIFO, &param) != 0) {
            if (config->log_level >= LOG_LEVEL_DEBUG) {
                syslog(LOG_DEBUG, "Failed to set real-time priority for MIDI thread (may need audio group membership)");
            }
        } else {
            if (config->log_level >= LOG_LEVEL_DEBUG) {
                syslog(LOG_DEBUG, "Set real-time priority for MIDI thread");
            }
        }
    }
    
    syslog(LOG_INFO, "ALSA MIDI client '%s' created successfully (client %d, port %d)",
           config->client_name, midi->client_id, midi->port_id);
    
    return midi;
    
error:
    midi_alsa_cleanup(midi);
    return NULL;
}

/**
 * Process MIDI events (non-threaded mode - used for timeout processing)
 */
int midi_alsa_process_events(midi_alsa_t *midi, int timeout_ms) {
    if (!midi || !midi->seq_handle) {
        return -1;
    }
    
    /* In threaded mode, this is mostly a no-op, but we can use it for */
    /* checking connection health or processing administrative events */
    
    /* Check if our thread is still running */
    if (!midi->running) {
        return -1;
    }
    
    /* TODO: Could add periodic health checks here */
    /* For now, just sleep briefly to prevent busy waiting in main loop */
    if (timeout_ms > 0) {
        usleep(timeout_ms * 1000);
    }
    
    return 0;
}

/**
 * Clean up ALSA MIDI resources
 */
void midi_alsa_cleanup(midi_alsa_t *midi) {
    if (!midi) {
        return;
    }
    
    /* Stop the processing thread */
    if (midi->running) {
        midi->running = 0;
        
        /* Wait for thread to finish */
        pthread_join(midi->thread, NULL);
    }
    
    /* Clean up ALSA resources */
    if (midi->seq_handle) {
        if (midi->port_id >= 0) {
            snd_seq_delete_port(midi->seq_handle, midi->port_id);
        }
        snd_seq_close(midi->seq_handle);
    }
    
    /* Free poll descriptors */
    if (midi->poll_fds) {
        free(midi->poll_fds);
    }
    
    if (midi->config && midi->config->log_level >= LOG_LEVEL_INFO) {
        syslog(LOG_INFO, "ALSA MIDI client cleaned up");
    }
    
    free(midi);
}

/**
 * Get the ALSA client and port information (for external tools)
 */
int midi_alsa_get_client_info(midi_alsa_t *midi, int *client_id, int *port_id) {
    if (!midi || !client_id || !port_id) {
        return -1;
    }
    
    *client_id = midi->client_id;
    *port_id = midi->port_id;
    
    return 0;
}

/**
 * Refresh auto-connections (useful for hotplug devices)
 */
int midi_alsa_refresh_connections(midi_alsa_t *midi) {
    if (!midi) {
        return -1;
    }
    
    return setup_autoconnect(midi);
}
