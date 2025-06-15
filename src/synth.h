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

#ifndef MIDISYNTHD_SYNTH_H
#define MIDISYNTHD_SYNTH_H

#include <stdint.h>
#include <stdbool.h>
#include <fluidsynth.h>
#include <alsa/asoundlib.h>

/* Forward declarations */
typedef struct synth_s synth_t;
struct midisynthd_config_t;
typedef struct midisynthd_config_t midisynthd_config_t;
struct audio_s;
typedef struct audio_s audio_t;

/**
 * MIDI message types for event handling
 */
typedef enum {
    MIDI_NOTE_OFF       = 0x80,
    MIDI_NOTE_ON        = 0x90,
    MIDI_KEY_PRESSURE   = 0xA0,
    MIDI_CONTROL_CHANGE = 0xB0,
    MIDI_PROGRAM_CHANGE = 0xC0,
    MIDI_CHANNEL_PRESSURE = 0xD0,
    MIDI_PITCH_BEND     = 0xE0,
    MIDI_SYSTEM_EXCLUSIVE = 0xF0
} midi_message_type_t;

/**
 * Standard MIDI control change messages
 */
typedef enum {
    MIDI_CC_BANK_SELECT_MSB    = 0,
    MIDI_CC_MODULATION_WHEEL   = 1,
    MIDI_CC_BREATH_CONTROLLER  = 2,
    MIDI_CC_FOOT_PEDAL         = 4,
    MIDI_CC_PORTAMENTO_TIME    = 5,
    MIDI_CC_DATA_ENTRY_MSB     = 6,
    MIDI_CC_VOLUME             = 7,
    MIDI_CC_BALANCE            = 8,
    MIDI_CC_PAN                = 10,
    MIDI_CC_EXPRESSION         = 11,
    MIDI_CC_BANK_SELECT_LSB    = 32,
    MIDI_CC_DATA_ENTRY_LSB     = 38,
    MIDI_CC_SUSTAIN_PEDAL      = 64,
    MIDI_CC_PORTAMENTO         = 65,
    MIDI_CC_SOSTENUTO_PEDAL    = 66,
    MIDI_CC_SOFT_PEDAL         = 67,
    MIDI_CC_LEGATO_PEDAL       = 68,
    MIDI_CC_HOLD_2_PEDAL       = 69,
    MIDI_CC_SOUND_VARIATION    = 70,
    MIDI_CC_RESONANCE          = 71,
    MIDI_CC_SOUND_RELEASE_TIME = 72,
    MIDI_CC_SOUND_ATTACK_TIME  = 73,
    MIDI_CC_SOUND_BRIGHTNESS   = 74,
    MIDI_CC_REVERB_LEVEL       = 91,
    MIDI_CC_CHORUS_LEVEL       = 93,
    MIDI_CC_ALL_SOUND_OFF      = 120,
    MIDI_CC_ALL_CONTROLLERS_OFF = 121,
    MIDI_CC_LOCAL_KEYBOARD     = 122,
    MIDI_CC_ALL_NOTES_OFF      = 123,
    MIDI_CC_OMNI_MODE_OFF      = 124,
    MIDI_CC_OMNI_MODE_ON       = 125,
    MIDI_CC_MONO_MODE_ON       = 126,
    MIDI_CC_POLY_MODE_ON       = 127
} midi_control_change_t;

/**
 * MIDI channel constants
 */
#define MIDI_CHANNELS           16
#define MIDI_PERCUSSION_CHANNEL 9   /* Channel 10 (0-indexed as 9) */
#define MIDI_MAX_NOTE           127
#define MIDI_MAX_VELOCITY       127
#define MIDI_MAX_CONTROL_VALUE  127
#define MIDI_MAX_PROGRAM        127
#define MIDI_MAX_PITCH_BEND     16383  /* 14-bit value */

/**
 * Synthesizer status and statistics
 */
typedef struct {
    bool initialized;           /* Whether synth is properly initialized */
    int active_voices;          /* Number of currently playing voices */
    int max_polyphony;          /* Maximum polyphony setting */
    float cpu_load;             /* CPU usage percentage (0.0-100.0) */
    int soundfonts_loaded;      /* Number of loaded soundfonts */
    char current_preset[64];    /* Name of current preset on channel 0 */
    double sample_rate;         /* Current audio sample rate */
    int buffer_size;            /* Audio buffer size in frames */
} synth_status_t;

/**
 * Initialize the FluidSynth synthesis engine
 * 
 * Creates a new FluidSynth synthesizer instance with the specified configuration
 * and audio backend. Loads soundfonts, sets up General MIDI defaults, and
 * prepares the synthesizer for real-time operation.
 * 
 * @param config Configuration structure containing synth settings
 * @param audio Audio backend instance for output routing
 * @return Pointer to initialized synth instance, or NULL on failure
 */
synth_t* synth_init(const midisynthd_config_t *config, audio_t *audio);

/**
 * Clean up and destroy the synthesizer instance
 * 
 * Stops all playing notes, unloads soundfonts, destroys the FluidSynth
 * instance, and frees all associated resources.
 * 
 * @param synth Synthesizer instance to clean up
 */
void synth_cleanup(synth_t *synth);

/**
 * Process a MIDI Note On event
 * 
 * Triggers a note to start playing on the specified channel with the given
 * velocity. If velocity is 0, this is treated as a Note Off event.
 * 
 * @param synth Synthesizer instance
 * @param channel MIDI channel (0-15)
 * @param note MIDI note number (0-127)
 * @param velocity Note velocity (0-127)
 * @return 0 on success, negative on error
 */
int synth_note_on(synth_t *synth, int channel, int note, int velocity);

/**
 * Process a MIDI Note Off event
 * 
 * Stops a playing note on the specified channel. The velocity parameter
 * affects the release characteristics on some synthesizers.
 * 
 * @param synth Synthesizer instance
 * @param channel MIDI channel (0-15)
 * @param note MIDI note number (0-127)
 * @param velocity Release velocity (0-127) - optional, can be 0
 * @return 0 on success, negative on error
 */
int synth_note_off(synth_t *synth, int channel, int note, int velocity);

/**
 * Process a MIDI Program Change event
 * 
 * Changes the instrument (program) on the specified channel. The program
 * number selects from the currently loaded soundfont presets.
 * 
 * @param synth Synthesizer instance
 * @param channel MIDI channel (0-15)
 * @param program Program number (0-127)
 * @return 0 on success, negative on error
 */
int synth_program_change(synth_t *synth, int channel, int program);

/**
 * Process a MIDI Control Change event
 * 
 * Modifies a control parameter on the specified channel. Common controls
 * include volume (CC 7), pan (CC 10), modulation (CC 1), and sustain (CC 64).
 * 
 * @param synth Synthesizer instance
 * @param channel MIDI channel (0-15)
 * @param control Control number (0-127)
 * @param value Control value (0-127)
 * @return 0 on success, negative on error
 */
int synth_control_change(synth_t *synth, int channel, int control, int value);

/**
 * Process a MIDI Pitch Bend event
 * 
 * Applies pitch bend to all notes on the specified channel. The bend value
 * is a 14-bit unsigned integer where 8192 represents no bend.
 * 
 * @param synth Synthesizer instance
 * @param channel MIDI channel (0-15)
 * @param value Pitch bend value (0-16383, center = 8192)
 * @return 0 on success, negative on error
 */
int synth_pitch_bend(synth_t *synth, int channel, int value);

/**
 * Process a MIDI Channel Pressure (Aftertouch) event
 * 
 * Applies pressure-sensitive modulation to all notes on the channel.
 * 
 * @param synth Synthesizer instance
 * @param channel MIDI channel (0-15)
 * @param pressure Pressure value (0-127)
 * @return 0 on success, negative on error
 */
int synth_channel_pressure(synth_t *synth, int channel, int pressure);

/**
 * Process a MIDI Key Pressure (Polyphonic Aftertouch) event
 * 
 * Applies pressure-sensitive modulation to a specific note on the channel.
 * 
 * @param synth Synthesizer instance
 * @param channel MIDI channel (0-15)
 * @param note MIDI note number (0-127)
 * @param pressure Pressure value (0-127)
 * @return 0 on success, negative on error
 */
int synth_key_pressure(synth_t *synth, int channel, int note, int pressure);

/**
 * Process raw MIDI data
 * 
 * Parses and processes a complete MIDI message from raw bytes. This is
 * useful for handling MIDI data from ALSA sequencer or other sources.
 * 
 * @param synth Synthesizer instance
 * @param data Pointer to MIDI message bytes
 * @param length Length of MIDI message in bytes
 * @return 0 on success, negative on error
 */
int synth_process_midi_data(synth_t *synth, const uint8_t *data, size_t length);

/**
 * Stop all playing notes immediately
 * 
 * Sends Note Off events to all channels and all notes, effectively
 * creating silence. This is useful for panic situations or initialization.
 * 
 * @param synth Synthesizer instance
 * @return 0 on success, negative on error
 */
int synth_all_notes_off(synth_t *synth);

/**
 * Reset all MIDI controllers to default values
 * 
 * Resets all control change values (volume, pan, modulation, etc.) to
 * their default states on all channels.
 * 
 * @param synth Synthesizer instance
 * @return 0 on success, negative on error
 */
int synth_reset_controllers(synth_t *synth);

/**
 * Set the master gain (volume) of the synthesizer
 * 
 * Adjusts the overall output level of the synthesizer. This affects
 * all channels and all notes uniformly.
 * 
 * @param synth Synthesizer instance
 * @param gain Gain value (0.0 = silence, 1.0 = normal, >1.0 = amplification)
 * @return 0 on success, negative on error
 */
int synth_set_gain(synth_t *synth, float gain);

/**
 * Get the current master gain setting
 * 
 * @param synth Synthesizer instance
 * @return Current gain value, or negative on error
 */
float synth_get_gain(synth_t *synth);


/**
 * Unload a previously loaded soundfont
 * 
 * @param synth Synthesizer instance
 * @param soundfont_id Identifier of the soundfont to unload
 * @return 0 on success, negative on error
 */
int synth_unload_soundfont(synth_t *synth, int soundfont_id);

/**
 * Get current synthesizer status and performance statistics
 * 
 * @param synth Synthesizer instance
 * @param status Pointer to status structure to fill
 * @return 0 on success, negative on error
 */
int synth_get_status(synth_t *synth, synth_status_t *status);

/**
 * Set polyphony limit (maximum number of simultaneous voices)
 * 
 * @param synth Synthesizer instance
 * @param polyphony Maximum number of voices (1-65535)
 * @return 0 on success, negative on error
 */
int synth_set_polyphony(synth_t *synth, int polyphony);

/**
 * Get current polyphony limit
 * 
 * @param synth Synthesizer instance
 * @return Current polyphony limit, or negative on error
 */
int synth_get_polyphony(synth_t *synth);

/**
 * Check if the synthesizer is properly initialized and ready
 * 
 * @param synth Synthesizer instance
 * @return True if ready for operation, false otherwise
 */
bool synth_is_ready(synth_t *synth);

/**
 * Get the FluidSynth settings object used by the synthesizer
 * 
 * @param synth Synthesizer instance
 * @return FluidSynth settings object, or NULL on error
 */
fluid_settings_t *synth_get_settings(synth_t *synth);

/**
 * Get the FluidSynth object for MIDI driver use
 * 
 * @param synth Synthesizer instance
 * @return FluidSynth object, or NULL on error
 */
fluid_synth_t *synth_get_fluidsynth(synth_t *synth);

/**
 * Handle an ALSA sequencer MIDI event
 * 
 * @param synth Synthesizer instance
 * @param ev ALSA sequencer event to process
 * @return 0 on success, negative on error
 */
int synth_handle_midi_event(synth_t *synth, snd_seq_event_t *ev);

#endif /* MIDISYNTHD_SYNTH_H */
