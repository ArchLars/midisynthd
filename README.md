<table>
  <tr align="center">
    <td>
      <img src="logo.png" alt="midisynthd logo" width="250" />
    </td>
    <td>
      <h1 style="margin:0;">midisynthd</h1>
      <p style="margin:8px 0 0;">
        <a href="https://github.com/ArchLars/midisynthd/actions">
          <img src="https://github.com/ArchLars/midisynthd/workflows/CI/badge.svg"
               alt="CI Build Status" />
        </a>
        <a href="https://github.com/ArchLars/midisynthd/releases">
          <img src="https://img.shields.io/github/v/release/ArchLars/midisynthd"
               alt="Latest Release" />
        </a>
        <a href="https://www.gnu.org/licenses/lgpl-2.1.html">
          <img src="https://img.shields.io/badge/License-LGPL%20v2.1-blue.svg"
               alt="License: LGPL v2.1" />
        </a>
        <a href="https://www.kernel.org/">
          <img src="https://img.shields.io/badge/Platform-Linux-orange.svg"
               alt="Platform: Linux" />
        </a>
      </p>
    </td>
  </tr>
</table>

---

## 🎵 Overview

`midisynthd` is a lightweight, system-level MIDI synthesizer daemon that provides General MIDI playback on Linux systems. It runs as a background service and exposes itself as a sequencer device, allowing any application that can send MIDI to an sequencer to be heard through a global synthesizer.

### Why midisynthd?

- **Seamless Integration**: Appears as a standard MIDI device that any application can use
- **Automatic Audio Routing**: Intelligently detects and uses JACK, PipeWire, PulseAudio, or ALSA
- **Zero Configuration**: Works out-of-the-box with sensible defaults
- **System Service**: Runs automatically at startup via systemd
- **Low Latency**: Optimized for real-time MIDI playback
- **General MIDI Compatible**: Full GM instrument set with high-quality SoundFonts
- **Supports JACK and ALSA-Seq**: Support for JACK and ALSA right out of the box.


## 📦 Installation

### Building from Source

#### Dependencies

- **Build Tools**: CMake (≥ 3.12), GCC/Clang, pkg-config
- **Libraries**:
  - FluidSynth (≥ 2.0)
  - ALSA (libasound2)
  - systemd (optional, for service integration)
- **Runtime**:
  - General MIDI SoundFont (e.g., FluidR3_GM.sf2)

For Debian/Ubuntu systems, install the build requirements with:

```bash
sudo apt-get install build-essential cmake pkg-config libasound2-dev \
    libfluidsynth-dev libsystemd-dev libcmocka-dev
```


#### Build Steps

```bash
# Clone the repository
git clone https://github.com/ArchLars/midisynthd.git
cd midisynthd

# Create build directory
mkdir build && cd build

# Configure for local installation
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local \
         -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Run tests (optional)
make test

# Install into ~/.local (no sudo needed)
make install

# Install user service file
mkdir -p ~/.config/systemd/user
cp ../systemd/midisynthd-user.service ~/.config/systemd/user/
```

## 🚀 Quick Start

### Enable and Start the Service

For most desktop users, enable the **user service**:

```bash
# Enable auto-start at login
systemctl --user enable midisynthd.service

# Start immediately
systemctl --user start midisynthd.service

# Check status
systemctl --user status midisynthd.service
```

### Verify MIDI Port

Check that the MIDI port is available:

```bash
# List MIDI ports
aconnect -l

# You should see something like:
# client 128: 'FLUID Synth (MidiSynth Daemon)' [type=user,pid=12345]
#     0 'Synth input port (MidiSynth Daemon:0)'
```

### Play a MIDI File

```bash
# Using aplaymidi (replace 128:0 with the port from `aconnect -l`)
aplaymidi -p 128:0 song.mid

# Using pmidi
pmidi -p 128:0 song.mid

# Using timidity in ALSA mode
timidity -Os song.mid
```

### Connect a MIDI Keyboard

```bash
# List available MIDI inputs
aconnect -i

# Connect your keyboard (example: port 24:0) to midisynthd (replace 128:0 with your synth port)
aconnect 24:0 128:0
```

## ⚙️ Configuration

### Configuration Files

midisynthd uses a cascading configuration system:

1. **System-wide**: `/etc/midisynthd.conf`
2. **Per-user**: `~/.config/midisynthd.conf` (overrides system settings)
3. **Built-in defaults**: Used when no config files exist

The default configuration expects the FluidR3 GM SoundFont at
`/usr/share/sounds/sf2/FluidR3_GM.sf2`. Install it from your distribution's
package repositories or change the `soundfont` path:

- **Debian/Ubuntu**: `fluid-soundfont-gm`
- **Fedora/RHEL**: `fluid-soundfont`
- **Arch Linux**: `soundfont-fluid`

### Multiple SoundFonts

Load multiple SoundFonts for layered sounds:

```ini
# Primary General MIDI SoundFont
soundfont = /usr/share/soundfonts/FluidR3_GM.sf2

# Additional SoundFonts (stacked)
soundfont = /home/user/soundfonts/strings.sf2
soundfont = /home/user/soundfonts/piano.sf2
```

### Audio Driver Selection

The `auto` driver detects in this order:
1. **JACK** - If JACK server is running
2. **PipeWire** - If PipeWire is available  
3. **PulseAudio** - If PulseAudio is running
4. **ALSA** - Direct hardware access (fallback)

Force a specific driver:
```ini
audio_driver = pipewire
```

### MIDI Driver Selection

Two MIDI input backends are available: the ALSA sequencer (`alsa_seq`) and JACK
(`jack`). The default remains `alsa_seq`.

```ini
midi_driver = alsa_seq
;midi_driver = jack
```

### Audio Effects

midisynthd exposes simple controls for its built‑in effects.

```ini
# Enable or disable chorus (default: yes)
chorus_enabled = yes

# Strength of the chorus effect (default: 1.2)
chorus_level = 1.2

# Enable or disable reverb (default: yes)
reverb_enabled = yes

# Strength of the reverb effect (default: 0.9)
reverb_level = 0.9
```

## 📖 Advanced Usage

### Integration with Applications

#### LMMS
1. Go to Edit → Settings → MIDI
2. Select "ALSA-Sequencer" as MIDI interface
3. The port "FLUID Synth (MidiSynth Daemon)" will appear in available outputs

#### Rosegarden
1. Go to Studio → Manage MIDI Devices
2. The port "FLUID Synth (MidiSynth Daemon)" appears as an available MIDI output
3. Assign tracks to use it

#### REAPER
1. Options → Preferences → MIDI Devices
2. Enable "FLUID Synth (MidiSynth Daemon)" in MIDI outputs
3. Route tracks to the synthesizer

#### Command Line Tools
```bash
# MIDI file player
wildmidi -A song.mid

# Real-time MIDI routing
aconnect "USB MIDI Keyboard" 128:0

# MIDI monitoring
aseqdump -p 128:0
```

### Troubleshooting

#### No Sound
```bash
# Check if service is running
systemctl --user status midisynthd

# Check audio system
pactl info  # For PulseAudio/PipeWire

# Verify MIDI connection
aconnect -l
```

#### High CPU Usage
- Reduce polyphony in config: `polyphony = 128`
- Increase buffer size: `buffer_size = 1024`
- Disable effects: `reverb_enabled = no`

#### Permission Issues
For real-time priority, add your user to the `audio` group:
```bash
sudo usermod -a -G audio $USER
```

#### Stuck Notes After Abort
If a MIDI player stops unexpectedly (for example when killed with `SIGKILL`),
lingering notes may continue to sound. The daemon reacts to `SIGUSR2` by
sending an *All Notes Off* message. Silence any stuck tones with:

```bash
killall -USR2 midisynthd
```

## 🛠️ Development

### Building for Development

```bash
# Debug build with sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DENABLE_ASAN=ON \
         -DENABLE_TESTS=ON

# Run with verbose logging
./midisynthd --verbose --config ../config/midisynthd.conf

# Run test suite
ctest --output-on-failure
```

## 📄 License

This project is licensed under the GNU Lesser General Public License v2.1 in harmony with ALSA and FluidSynth. 

See the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **FluidSynth** developers for the excellent synthesis engine
- **ALSA** project for Linux audio/MIDI infrastructure  
- **systemd** for modern service management
- General MIDI SoundFont creators

## Notes

I wrote this for myself and cleaned it up for publication with AI. That's partly why I made a decision on the soundfont side of things. You can always compile it without that if you'd like. 

I decided to release this publicly because maybe someone would get some use out of it? I cannot guarantee it will ALWAYS work. It should, but mileage may wary. ALSA is the most stable default. 

## 🔗 Links

- [Homepage](https://github.com/ArchLars/midisynthd)
- [Issue Tracker](https://github.com/ArchLars/midisynthd/issues)
- [Releases](https://github.com/ArchLars/midisynthd/releases)

---

<p align="center">
Made with ♪ for the Linux audio community
</p>
