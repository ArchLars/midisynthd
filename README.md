# System-level MIDI Synthesizer Daemon for Linux

This is a a system-level MIDI synthesizer daemon for Linux – an equivalent to the Microsoft GS Wavetable SW Synth on Windows. The daemon uses FluidSynth as the core engine to provide General MIDI playback, exposing itself as an ALSA sequencer device so any MIDI-capable application can use it transparently. It will intelligently integrate with the host audio system (JACK, PipeWire, PulseAudio) without user intervention, support flexible configuration (both system-wide and per-user), and run as a background service (with systemd auto-start by default). The following sections present the planned repository layout and a step-by-step implementation plan.

## Filesystem Layout

Below is the proposed GitHub repository structure for the project (tentatively named `midisynthd`), with directories and key files. Each major file is annotated with a one-line description of its purpose:

```
midisynthd/                     // Project root directory
├── README.md                  // Project overview, build instructions, and usage
├── LICENSE                    // License information (e.g. MIT or GPL)
├── PLAN.md                    // Plan on how to develop the daemon.
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
├── config/
│   ├── midisynthd.conf        // System-wide default config (installed to /etc/midisynthd.conf)
│   └── midisynthd.conf.example// Example user configuration with documentation of options
├── systemd/
│   ├── midisynthd.service     // Systemd service unit (system-wide service, disabled by default)
│   └── midisynthd-user.service// Systemd user service unit (for per-user auto-start at login)
├── tests/
│   ├── test_config.c          // Unit tests for configuration parsing and precedence rules
│   ├── test_synth.c           // Unit tests for synth engine (e.g. loading soundfont, note playback)
│   ├── test_midi.c            // Unit tests for MIDI event handling (simulate ALSA events to synth)
│   └── CMakeLists.txt         // Build configuration for tests (if using CMake and CTest)
└── .github/workflows/
    └── ci-build.yml           // CI pipeline (build, run unit tests, packaging checks on each push)
```

(Each source file above represents a logical module of the daemon: `main.c` initializes the service, `synth.c` wraps FluidSynth functionality, `midi_alsa.c` handles ALSA sequencer I/O, etc. The configuration and systemd directories provide integration with the OS environment, and the tests directory ensures each component works correctly.)
