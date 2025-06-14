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

#ifndef MIDI_ALSA_H
#define MIDI_ALSA_H

#include <stdint.h>
#include <stdbool.h>
#include <alsa/asoundlib.h>

#include "config.h"

/* Forward declarations */
typedef struct midi_alsa_s midi_alsa_t;
typedef struct synth_s synth_t;

/* MIDI event processing constants */
#define MIDI_ALSA_MAX_EVENTS_PER_POLL    64
#define MIDI_ALSA_POLL_TIMEOUT_MS        100
#define MIDI_ALSA_BUFFER_SIZE            1024

/* ALSA sequencer client capabilities */
#define MIDI_ALSA_CLIENT_CAPS    (SND_SEQ_CLIENT_CAP_MIDI_GENERIC | \
                                  SND_SEQ_CLIENT_CAP_MIDI_GM)

/* ALSA sequencer port capabilities */
#define MIDI_ALSA_PORT_CAPS      (SND_SEQ_PORT_CAP_WRITE | \
                                  SND_SEQ_PORT_CAP_SUBS_WRITE | \
                                  SND_SEQ_PORT_CAP_MIDI_GENERIC | \
                                  SND_SEQ_PORT_CAP_MIDI_GM)

/* ALSA sequencer port type */
#define MIDI_ALSA_PORT_TYPE      (SND_SEQ_PORT_TYPE_MIDI_GENERIC | \
                                  SND_SEQ_PORT_TYPE_MIDI_GM | \
                                  SND_SEQ_PORT_TYPE_SYNTHESIZER)

/* MIDI channel and controller constants */
#define MIDI_CHANNELS            16
#define MIDI_PERCUSSION_CHANNEL  9   /* Channel 10 in 1-based numbering */
#define MIDI_MAX_VELOCITY        127
#define MIDI_MAX_CONTROLLER      127
#define MIDI_MAX_PROGRAM         127
#define MIDI_MAX_PITCH_BEND      16383

/* Common MIDI controller numbers */
#define MIDI_CTRL_BANK_SELECT_MSB    0
#define MIDI_CTRL_MODULATION         1
#define MIDI_CTRL_BREATH             2
#define MIDI_CTRL_FOOT               4
#define MIDI_CTRL_PORTAMENTO_TIME    5
#define MIDI_CTRL_DATA_ENTRY_MSB     6
#define MIDI_CTRL_VOLUME             7
#define MIDI_CTRL_BALANCE            8
#define MIDI_CTRL_PAN                10
#define MIDI_CTRL_EXPRESSION         11
#define MIDI_CTRL_BANK_SELECT_LSB    32
#define MIDI_CTRL_DATA_ENTRY_LSB     38
#define MIDI_CTRL_SUSTAIN            64
#define MIDI_CTRL_PORTAMENTO         65
#define MIDI_CTRL_SOSTENUTO          66
#define MIDI_CTRL_SOFT_PEDAL         67
#define MIDI_CTRL_LEGATO             68
#define MIDI_CTRL_HOLD2              69
#define MIDI_CTRL_REVERB_SEND        91
#define MIDI_CTRL_CHORUS_SEND        93
#define MIDI_CTRL_NRPN_LSB           98
#define MIDI_CTRL_NRPN_MSB           99
#define MIDI_CTRL_RPN_LSB            100
#define MIDI_CTRL_RPN_MSB            101
#define MIDI_CTRL_ALL_SOUND_OFF      120
#define MIDI_CTRL_RESET_CONTROLLERS  121
#define MIDI_CTRL_LOCAL_CONTROL      122
#define MIDI_CTRL_ALL_NOTES_OFF      123

/* MIDI status byte masks */
#define MIDI_STATUS_MASK             0xF0
#define MIDI_CHANNEL_MASK            0x0F

/* MIDI message types */
typedef enum {
    MIDI_NOTE_OFF         = 0x80,
    MIDI_NOTE_ON          = 0x90,
    MIDI_POLY_PRESSURE    = 0xA0,
    MIDI_CONTROL_CHANGE   = 0xB0,
    MIDI_PROGRAM_CHANGE   = 0xC0,
    MIDI_CHANNEL_PRESSURE = 0xD0,
    MIDI_PITCH_BEND       = 0xE0,
    MIDI_SYSTEM_EXCLUSIVE = 0xF0
} midi_message_type_t;

/* MIDI connection info structure */
typedef struct {
    int client_id;
    int port_id;
    char client_name[64];
    char port_name[64];
    bool is_hardware;
    bool is_connected;
} midi_connection_info_t;

/* MIDI statistics structure */
typedef struct {
    uint64_t events_processed;
    uint64_t notes_on;
    uint64_t notes_off;
    uint64_t control_changes;
    uint64_t program_changes;
    uint64_t pitch_bends;
    uint64_t system_messages;
    uint64_t dropped_events;
    uint64_t invalid_events;
    uint32_t active_connections;
    double cpu_usage_percent;
} midi_alsa_stats_t;

/* Error codes */
typedef enum {
    MIDI_ALSA_SUCCESS           = 0,
    MIDI_ALSA_ERROR_INIT        = -1,
    MIDI_ALSA_ERROR_MEMORY      = -2,
    MIDI_ALSA_ERROR_ALSA        = -3,
    MIDI_ALSA_ERROR_CONFIG      = -4,
    MIDI_ALSA_ERROR_CLIENT      = -5,
    MIDI_ALSA_ERROR_PORT        = -6,
    MIDI_ALSA_ERROR_CONNECT     = -7,
    MIDI_ALSA_ERROR_POLL        = -8,
    MIDI_ALSA_ERROR_EVENT       = -9,
    MIDI_ALSA_ERROR_INVALID     = -10
} midi_alsa_error_t;

/* Function declarations */

/**
 * Initialize ALSA MIDI input subsystem
 * @param config Configuration structure
 * @param synth Synthesizer instance to route MIDI events to
 * @return MIDI ALSA instance on success, NULL on failure
 */
midi_alsa_t *midi_alsa_init(const midisynthd_config_t *config, synth_t *synth);

/**
 * Clean up and destroy MIDI ALSA instance
 * @param midi MIDI ALSA instance to clean up
 */
void midi_alsa_cleanup(midi_alsa_t *midi);

/**
 * Process pending MIDI events
 * @param midi MIDI ALSA instance
 * @param timeout_ms Timeout in milliseconds for polling (0 = non-blocking)
 * @return Number of events processed, or negative value on error
 */
int midi_alsa_process_events(midi_alsa_t *midi, int timeout_ms);

/**
 * Get ALSA sequencer client ID
 * @param midi MIDI ALSA instance
 * @return Client ID, or negative value on error
 */
int midi_alsa_get_client_id(const midi_alsa_t *midi);

/**
 * Get ALSA sequencer port ID
 * @param midi MIDI ALSA instance
 * @return Port ID, or negative value on error
 */
int midi_alsa_get_port_id(const midi_alsa_t *midi);

/**
 * Get client and port name
 * @param midi MIDI ALSA instance
 * @param client_name Buffer for client name (can be NULL)
 * @param client_name_len Size of client name buffer
 * @param port_name Buffer for port name (can be NULL)
 * @param port_name_len Size of port name buffer
 * @return 0 on success, negative value on error
 */
int midi_alsa_get_names(const midi_alsa_t *midi, 
                        char *client_name, size_t client_name_len,
                        char *port_name, size_t port_name_len);

/**
 * Get list of currently connected MIDI sources
 * @param midi MIDI ALSA instance
 * @param connections Array to store connection info
 * @param max_connections Maximum number of connections to return
 * @return Number of connections found, or negative value on error
 */
int midi_alsa_get_connections(const midi_alsa_t *midi,
                              midi_connection_info_t *connections,
                              int max_connections);

/**
 * Manually connect to a MIDI source
 * @param midi MIDI ALSA instance
 * @param src_client Source client ID
 * @param src_port Source port ID
 * @return 0 on success, negative value on error
 */
int midi_alsa_connect_source(midi_alsa_t *midi, int src_client, int src_port);

/**
 * Manually disconnect from a MIDI source
 * @param midi MIDI ALSA instance
 * @param src_client Source client ID
 * @param src_port Source port ID
 * @return 0 on success, negative value on error
 */
int midi_alsa_disconnect_source(midi_alsa_t *midi, int src_client, int src_port);

/**
 * Get MIDI processing statistics
 * @param midi MIDI ALSA instance
 * @param stats Structure to fill with statistics
 * @return 0 on success, negative value on error
 */
int midi_alsa_get_stats(const midi_alsa_t *midi, midi_alsa_stats_t *stats);

/**
 * Reset MIDI processing statistics
 * @param midi MIDI ALSA instance
 */
void midi_alsa_reset_stats(midi_alsa_t *midi);

/**
 * Enable or disable autoconnection to hardware MIDI devices
 * @param midi MIDI ALSA instance
 * @param enabled True to enable autoconnection, false to disable
 * @return 0 on success, negative value on error
 */
int midi_alsa_set_autoconnect(midi_alsa_t *midi, bool enabled);

/**
 * Get current autoconnection status
 * @param midi MIDI ALSA instance
 * @return True if autoconnection is enabled, false otherwise
 */
bool midi_alsa_get_autoconnect(const midi_alsa_t *midi);

/**
 * Send MIDI panic (all notes off, reset controllers) to synthesizer
 * @param midi MIDI ALSA instance
 * @return 0 on success, negative value on error
 */
int midi_alsa_panic(midi_alsa_t *midi);

/**
 * Convert MIDI ALSA error code to human-readable string
 * @param error Error code
 * @return Error description string
 */
const char *midi_alsa_strerror(midi_alsa_error_t error);

/**
 * Check if ALSA sequencer subsystem is available
 * @return True if ALSA sequencer is available, false otherwise
 */
bool midi_alsa_is_available(void);

/**
 * Get ALSA library version information
 * @param version_string Buffer to store version string
 * @param buffer_size Size of version string buffer
 * @return 0 on success, negative value on error
 */
int midi_alsa_get_version(char *version_string, size_t buffer_size);

/* Utility macros */

/**
 * Extract MIDI channel from status byte (0-15)
 */
#define MIDI_GET_CHANNEL(status) ((status) & MIDI_CHANNEL_MASK)

/**
 * Extract MIDI message type from status byte
 */
#define MIDI_GET_MESSAGE_TYPE(status) ((status) & MIDI_STATUS_MASK)

/**
 * Check if MIDI message is channel message
 */
#define MIDI_IS_CHANNEL_MESSAGE(status) (((status) & 0x80) && ((status) < 0xF0))

/**
 * Check if MIDI message is system message
 */
#define MIDI_IS_SYSTEM_MESSAGE(status) ((status) >= 0xF0)

/**
 * Check if MIDI note number is valid (0-127)
 */
#define MIDI_IS_VALID_NOTE(note) ((note) >= 0 && (note) <= 127)

/**
 * Check if MIDI velocity is valid (0-127)
 */
#define MIDI_IS_VALID_VELOCITY(vel) ((vel) >= 0 && (vel) <= 127)

/**
 * Check if MIDI controller number is valid (0-127)
 */
#define MIDI_IS_VALID_CONTROLLER(ctrl) ((ctrl) >= 0 && (ctrl) <= 127)

/**
 * Check if MIDI program number is valid (0-127)
 */
#define MIDI_IS_VALID_PROGRAM(prog) ((prog) >= 0 && (prog) <= 127)

/**
 * Convert 14-bit pitch bend value to signed integer (-8192 to +8191)
 */
#define MIDI_PITCH_BEND_TO_SIGNED(lsb, msb) \
    ((int)(((msb) << 7) | (lsb)) - 8192)

/**
 * Combine MSB and LSB into 14-bit value
 */
#define MIDI_COMBINE_14BIT(lsb, msb) (((msb) << 7) | (lsb))

#endif /* MIDI_ALSA_H */
