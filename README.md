# System-level MIDI Synthesizer Daemon for Linux

This design outlines a system-level MIDI synthesizer daemon for Linux – an equivalent to the Microsoft GS Wavetable SW Synth on Windows. The daemon uses FluidSynth as the core engine to provide General MIDI playback, exposing itself as an ALSA sequencer device so any MIDI-capable application can use it transparently. It will intelligently integrate with the host audio system (JACK, PipeWire, PulseAudio) without user intervention, support flexible configuration (both system-wide and per-user), and run as a background service (with systemd auto-start by default). The following sections present the planned repository layout and a step-by-step implementation plan.

## Filesystem Layout

Below is the proposed GitHub repository structure for the project (tentatively named `midisynthd`), with directories and key files. Each major file is annotated with a one-line description of its purpose:

```
midisynthd/                     // Project root directory
├── README.md                  // Project overview, build instructions, and usage
├── LICENSE                    // License information (e.g. MIT or GPL)
├── src/
│   ├── main.c                 // Entry point: argument parsing, daemon initialization
│   ├── synth.c                // FluidSynth engine setup (soundfont loading, synth control)
│   ├── synth.h                // Interface for synthesizer engine (e.g. play notes, program change)
│   ├── midi_alsa.c            // ALSA sequencer integration (creates virtual MIDI input port)
│   ├── midi_alsa.h            // MIDI input handling definitions and utilities
│   ├── audio.c                // Audio backend management (JACK/PipeWire/PulseAudio detection)
│   ├── audio.h                // Audio output configuration interface and constants
│   ├── config.c               // Configuration file parsing (global and per-user settings)
│   ├── config.h               // Configuration structure and default values
│   ├── daemonize.c            // Daemon lifecycle (systemd integration, signal handling)
│   └── daemonize.h            // Helper functions for daemon mode and auto-start
├── include/                   // Public headers (if exposing any library API, optional)
│   └── midisynthd.h           // Example public API header (not needed for basic daemon usage)
├── soundfonts/                // (Optional) SoundFont files or placeholders
│   └── FluidR3_GM_GS.sf2         // (Optional) Default General MIDI SoundFont (if distribution permits)
├── config/
│   ├── midisynthd.conf        // System-wide default config (installed to /etc/midisynthd.conf)
│   └── midisynthd.conf.example// Example user configuration with documentation of options
├── systemd/
│   ├── midisynthd.service     // Systemd service unit (system-wide service, disabled by default)
│   └── midisynthd-user.service// Systemd user service unit (for per-user auto-start at login)
├── packaging/
│   ├── midisynthd.spec        // RPM spec file for building packages (Fedora, SUSE, etc.)
│   ├── debian/                // Debian packaging files (control, postinst, systemd links)
│   └── AppArmor.profile       // (Optional) AppArmor profile for security confinement
├── tests/
│   ├── test_config.c          // Unit tests for configuration parsing and precedence rules
│   ├── test_synth.c           // Unit tests for synth engine (e.g. loading soundfont, note playback)
│   ├── test_midi.c            // Unit tests for MIDI event handling (simulate ALSA events to synth)
│   └── CMakeLists.txt         // Build configuration for tests (if using CMake and CTest)
└── .github/workflows/
    └── ci-build.yml           // CI pipeline (build, run unit tests, packaging checks on each push)
```

(Each source file above represents a logical module of the daemon: `main.c` initializes the service, `synth.c` wraps FluidSynth functionality, `midi_alsa.c` handles ALSA sequencer I/O, etc. The configuration and systemd directories provide integration with the OS environment, and the tests directory ensures each component works correctly.)

## Implementation Plan

The development will proceed in sequential steps, addressing core functionality first and then integration and configuration. Each step lists the relevant technologies/libraries, design considerations, and testing/packaging strategies:

### 1. Project Setup and Dependencies

**Technologies & Libraries:**  
Initialize a C/C++ project (e.g. using C99 or C++17) with a build system like CMake or Meson. Add ALSA sequencer library (`libasound`) for MIDI input, FluidSynth (`libfluidsynth`) for synthesis, and a configuration parsing library such as libconfig or json-c for reading config files. Include systemd development files if using `sd_notify` or other systemd-specific APIs. Set up a GitHub Actions CI workflow to build the project on Linux and run tests on each commit.

**Design Considerations:**  
Organize code into modules (as reflected in the directory layout) for clarity and maintainability. Determine the daemon name (`midisynthd`) and ALSA client name. Plan for a modular design: the main program will initialize config, audio, MIDI, and synth modules in order. Consider the threading model early: FluidSynth spawns its own audio thread (and possibly MIDI thread) for real-time processing, so the main thread can be used for control and signal handling. Ensure the project links with the necessary libraries and consider using static analysis and coding standards (e.g. clang-tidy, cppcheck) as part of CI.

**Testing & Packaging:**  
Write initial unit tests (possibly using a lightweight framework like CMocka or GoogleTest) for any utility functions (e.g., config parser test with a sample config string). Verify in CI that the project builds in different environments (for example, with JACK present vs not present, PulseAudio vs PipeWire available, etc., to ensure the code handles optional dependencies). No actual packaging is done in this step, but lay the groundwork for packaging: e.g., ensure the build system can produce a library or binary and consider how it will be installed (prefix, etc.). This step results in a skeletal daemon that does nothing yet, but confirms the development environment is set up correctly.

---

### 2. FluidSynth Engine Integration (Audio Output Setup)

**Technologies & Libraries:**  
Utilize the FluidSynth API to create the synthesizer instance. Call `new_fluid_settings()` to configure synthesis parameters and `new_fluid_synth()` to instantiate the synth. Use FluidSynth to load a General MIDI SoundFont (e.g. FluidR3_GM_GS.sf2 or another free GM sound set). Create an audio driver via FluidSynth (using `new_fluid_audio_driver()`) with an appropriate audio backend. Integrate with JACK, PipeWire, or PulseAudio using FluidSynth's support for those audio systems.

**Design Considerations:**  
Ensure the synthesizer is configured for General MIDI (16 channels, channel 10 as percussion). Set a default sample rate (e.g. 48000 Hz) and polyphony (e.g. 256 voices). Implement audio subsystem detection:  
1. If a JACK server is running, use the JACK driver.  
2. Else if PipeWire is available, use the PipeWire driver.  
3. Otherwise, fall back to PulseAudio, and only use raw ALSA if no sound server is detected.  

Allow buffer tuning via config to avoid underruns.

**Testing & Packaging:**  
Test synthesizing audio from manually fed notes or FluidSynth’s file rendering capabilities. Verify chosen audio driver matches environment. Ensure packaging declares FluidSynth and ALSA dependencies, and decide on bundling or depending on a GM soundfont (e.g., fluid-soundfont-gm).

---

### 3. ALSA Sequencer MIDI Input Integration

**Technologies & Libraries:**  
Use ALSA sequencer interface (`libasound`) or FluidSynth’s built-in MIDI driver (`midi.driver = alsa_seq`) and `new_fluid_midi_driver(settings, handler, data)` to create the MIDI port.

**Design Considerations:**  
Register the daemon as a persistent MIDI synth device with a fixed client name (`MidiSynth Daemon`). Enable `midi.autoconnect` for auto-connecting hardware MIDI controllers. Ensure real-time performance via `midi.realtime-prio` and document audio-group permissions.

**Testing & Packaging:**  
Write an integration test using ALSA sequencer loopback to send Note On events to the daemon and verify synthesis. Test auto-connect feature. Consider granting `CAP_SYS_NICE` via packaging for real-time priority if needed.

---

### 4. Configuration Management (Global and Per-User Settings)

**Technologies & Libraries:**  
Implement config parsing using libconfig or json-c. Provide `/etc/midisynthd.conf` for system-wide defaults and `~/.config/midisynthd.conf` for per-user overrides. Key settings: soundfont path, audio/midi driver preferences, gain, polyphony, autoconnect, log level.

**Design Considerations:**  
Load global config first, then user config to override. Handle missing files gracefully with defaults. Optionally support SIGHUP for reload. Document config precedence and example files.

**Testing & Packaging:**  
Unit-test config parser, merging behavior, and fallback defaults. Install config files properly in packages and preserve user edits.

---

### 5. Daemonization and Service Auto-start Integration

**Technologies & Libraries:**  
Create systemd unit files:
- User service: `/usr/lib/systemd/user/midisynthd.service` (`WantedBy=default.target`).
- System service: `/usr/lib/systemd/system/midisynthd.service` (`WantedBy=multi-user.target`, disabled by default).

Use `Type=simple` or `Type=notify` with `sd_notify()` for readiness.

**Design Considerations:**  
Run in foreground under systemd; support `--daemonize` for manual use. Handle `SIGTERM`, `SIGINT`, and optionally `SIGHUP`. In service units, add `After=pipewire.service pulseaudio.service`.

**Testing & Packaging:**  
Test user service start/enable, check ALSA port listing with `aconnect -l` or `pmidi -l`, play MIDI files, auto-start on login. Test system service on headless vs desktop scenarios. Verify logs via `journalctl`. Use `dh_installinit`/`dh_installsystemd` in Debian packaging and inform users how to enable the service.

---

### 6. Quality Assurance and Future Enhancements

**Technologies & Libraries:**  
Incorporate Valgrind or sanitizers, use systemd logging macros.

**Design Considerations:**  
Stress-test with complex MIDI streams, monitor resource usage, validate MIDI inputs to avoid buffer overflows, drop root privileges if started as root, consider upcoming PipeWire MIDI API support.

**Testing & Packaging:**  
Perform long-term stability tests, ensure correct permissions and lint checks in packages, verify DAW integration via volume controls and MIDI port listings. Document usage and guidance for seamless experience. Prepare `.deb` and `.rpm` artifacts with binaries, configs, service files, and soundfont recommendations. Future work: GUI/CLI controls, multiple ports, dynamic soundfont swapping, enhanced PipeWire MIDI support.
