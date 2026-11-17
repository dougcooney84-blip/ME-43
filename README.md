# ME - 43

ME - 43 is an original JUCE instrument plugin for house and techno sound design, targeting macOS Ventura (13.0+) and Ableton Live. It has eight MIDI voices, an original four-shape wavetable oscillator, a per-voice amplitude ADSR and resonant low-pass filter, smoothed master gain, ±2-semitone pitch bend, and parameter state save/restore. It aims for broad sound-design flexibility without cloning Serum.

The wavetable shapes are generated in code from additive sine harmonics; no third-party audio tables or sample assets are used. Wavetable Position interpolates through sine, soft triangle, saw, and hollow square shapes, with linear interpolation between oscillator samples. Thirty-two pitch-specific harmonic bands are prepared when the processor is constructed. Playback selects and smoothly crossfades adjacent bands so the highest partial stays at or below 90% of Nyquist.

## Build on macOS

Install the Apple command-line tools and CMake (using Homebrew if it is already installed):

```sh
xcode-select --install
brew install cmake
```

From the repository root, configure and build:

```sh
cmake -S plugins/stage-a-synth -B plugins/stage-a-synth/build -DCMAKE_BUILD_TYPE=Release
cmake --build plugins/stage-a-synth/build --config Release --target StageASynth_VST3 StageASynth_Standalone
```

CMake downloads JUCE 9.0.5 on the first configure. The VST3 bundle is produced under the build directory. Copy `ME - 43.vst3` to:

```text
~/Library/Audio/Plug-Ins/VST3/
```

Then rescan plug-ins in Ableton Live and load **ME - 43** on a MIDI track.

## Parameter IDs and ranges

These IDs are stable host automation/state identifiers. Parameter version is `1` for each ID.

| Parameter ID | Control | Range | Default | Description |
| --- | --- | ---: | ---: | --- |
| `masterGain` | Master | -60 to +6 dB | 0 dB | Output level, smoothed over 20 ms |
| `wavePosition` | WT Position | 0 to 1 | 0 | Morphs across the four wavetable shapes |
| `attack` | Attack | 0.001 to 5 s | 0.01 s | Amplitude rise time |
| `decay` | Decay | 0.001 to 5 s | 0.18 s | Time from peak to sustain level |
| `sustain` | Sustain | 0 to 1 | 0.75 | Held-note amplitude |
| `release` | Release | 0.005 to 10 s | 0.25 s | Note-off fade time |
| `filterCutoff` | Cutoff | 20 to 20,000 Hz | 12,000 Hz | Resonant low-pass cutoff; limited below Nyquist |
| `filterResonance` | Resonance | Q 0.1 to 10 | Q 0.707 | Low-pass resonance |

## Toolchain and target

- JUCE 9.0.5, fetched at the exact release tag by CMake.
- CMake 3.22 or newer and C++17.
- macOS Ventura (13.0+) with Xcode Command Line Tools and Apple Clang for the target build.
- VST3 for Ableton Live; a Standalone target is included for quick audio/MIDI checks.

Build on the Mac that will run the plugin. This Linux workspace cannot produce or validate a macOS VST3 binary, and Ableton Live host scanning and session restore require the target Mac/DAW.

## Test checklist

### Automated checks (Linux and macOS)

The JUCE regression executable renders the actual processor without requiring an audio device or DAW. Configure and run it from the repository root:

```sh
cmake -S plugins/stage-a-synth -B plugins/stage-a-synth/build -DCMAKE_BUILD_TYPE=Release
cmake --build plugins/stage-a-synth/build --config Release --target StageASynthTests
ctest --test-dir plugins/stage-a-synth/build --output-on-failure
```

It checks note-on/note-off and release completion, ±2-semitone pitch bend, eight-voice allocation and stealing, master-gain smoothing, valid save/restore for all eight parameters, rejection of truncated or unrelated session state without changing defaults, and a deterministic stereo reference render. These checks can run on Linux; CMake may need the platform's normal JUCE build dependencies. They do not replace DAW/host verification.

### macOS and Ableton Live checks

The following require macOS Ventura or newer and the target Ableton Live version:

1. Build both plug-in targets with the commands above; record macOS, Xcode/Clang, and JUCE versions.
2. Load the VST3 in Ableton Live and confirm it scans, opens, and unloads cleanly.
3. Confirm host MIDI input, automation, mono/stereo routing, and session save/reopen in Live.
4. Try the four Wavetable Position landmarks (0, about 0.33, about 0.67, and 1), then sweep between them. Confirm shape changes are smooth.
5. Test short and long envelope settings, overlapping notes, sustain pedal if provided by the host, and more than eight held notes. Confirm no stuck notes or abrupt release.
6. Sweep cutoff and resonance, including low cutoff and high resonance. Check stable output, audible resonance, and no unexpected DC or runaway level.
7. Repeat at 44.1 and 48 kHz and with different audio block sizes.

Linux can run the automated processor/audio assertions and build the test executable when JUCE dependencies are installed. It cannot produce or validate the macOS VST3 binary, plug-in loading, host automation, or Ableton Live session restore.

## Current limits

This is a focused first wavetable voice: there are no presets, modulation matrix, LFOs, effects, or filter envelope yet. The oscillator is limited to 32 harmonics, uses 2048-sample tables with linear sample interpolation, and retains a small guard band below Nyquist. Pitch-aware bands reduce harmonic foldback, but do not eliminate every source of aliasing, particularly in the extreme upper register or at unusual sample rates; evaluate the sound in the target DAW. JUCE is dual-licensed; confirm the applicable JUCE license before distributing binaries or source.
