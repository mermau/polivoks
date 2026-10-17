# POLIVOKS STUDY v0.1 — C++ / JUCE 8 synthesizer

**Status: prototype source project; the DSP core has been built and tested on Linux. macOS AU and VST3 binaries have not been compiled or host-validated in the supplied environment.** This code is **not** yet a rigorously identical circuit emulation.

## What is implemented

- C++17 engine (`Source/PolivoksDSP.h`) that is independent of JUCE and makes no heap allocations while processing audio samples.
- Two oscillators with five shapes: triangle, anti-aliased saw, square, two pulse widths. Oscillator ranges, tuning, detune, cross-modulation, and LFO modulation.
- Mono and shared-filter **duophonic** playing modes (one VCO assigned to lower held note, the other to higher held note); keyboard 48 visible keys, MIDI notes, sustain pedal, pitch wheel and mod wheel.
- Noise generator, osc/noise/external-input levels; optional audio input bus is exposed but support depends on the instrument host.
- Circuit-inspired nonlinear filter with 12 dB/oct nominal lowpass and bandpass outputs, cutoff, resonance, filter envelope and LFO modulation.
- Independent filter and VCA ADSR envelopes, repeatable AD stages, LFO tremolo, volume and output switching.
- Scanned-original-inspired photo front panel (user-supplied) with 37 interactive rotary/toggle controls, including right-click **MIDI CC Learn**, persistent CC mappings and plugin state saved with DAW projects.
- CMake configurations for **VST3**, macOS **Audio Unit**, and a **standalone** test instrument. `tests/dsp_tests.cpp` covers silent idle, note/envelope rendering, duophony, LP/BP selection and resonance numerical stability.

## Build on macOS (VST3 + AU)

Prerequisites: recent Xcode command line tools; CMake 3.22+; git; internet to obtain pinned JUCE release **8.0.15**. JUCE itself is not redistributed inside this archive.

```bash
cd PolivoksEmulation
./build-macos.sh
```

Or with a preinstalled local JUCE checkout:

```bash
cmake -S . -B build-macos -G Xcode \
  -DJUCE_SOURCE_DIR=/path/to/JUCE \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-macos --config Release --parallel 4
```

After build, the expected output is under `build-macos/PolivoksEmulation_artefacts/Release/` (use `find build-macos -name '*.vst3' -o -name '*.component'` if your JUCE version arranges it differently).

Copy `.vst3` into `~/Library/Audio/Plug-Ins/VST3/` and `.component` into `~/Library/Audio/Plug-Ins/Components/`, then rescan plugins in your host. Third-party AU/VST3 validation, signing and notarization are separate deployment steps.

**Compatibility note:** Ableton Live can host VST3 and AU on macOS, whereas **Pro Tools cannot load these formats directly**; it needs an AAX build or an additional wrapper/host. AAX is not supplied by this project.

## DSP-only tests (no JUCE, no audio device)

```bash
cmake -S . -B build-tests -DPOLIVOKS_BUILD_PLUGIN=OFF
cmake --build build-tests --parallel 4
ctest --test-dir build-tests --output-on-failure
```

## Arturia KeyLab / other MIDI controller

1. Create a MIDI/instrument track in your DAW, load **Polivoks Study** as an instrument and select your Arturia keyboard as the track input. Enable monitoring/record arm as appropriate.
2. Play notes from the KeyLab. By default the virtual panel keyboard spans **MIDI notes C2–B5 (36–83)**.
3. Right-click any rotary knob or on-screen hardware switch and choose **Learn MIDI CC**.
4. Move the desired KeyLab encoder, knob or fader once. The next incoming CC maps to that parameter. Repeat as needed for all 37 controls. The assignment is saved/restored with the plugin state.
5. MIDI pitch bend covers +/-2 semitones; CC1 contributes vibrato, CC64 is sustain. Those messages retain their conventional behaviour even when you also use MIDI Learn on them. To release a mapping, right-click the control again.
6. If MIDI Learn does not receive the CC, check that the DAW passes raw MIDI CC to the instrument rather than capturing it for a DAW-specific remote-control assignment. You can alternatively map the plugin's exposed host parameters using the DAW's automation/MIDI-remote features.
7. `INIT` restores synthesis controls; `PANIC` releases stuck notes; `CLEAR MIDI CC` clears all CC mapping assignments.

### Limitations of the original hardware translation

The original hardware was not a modern velocity/MIDI synthesizer. MIDI velocity and 48 on-screen keys are practical adaptations. A *real* faithful clone would need measured oscillator/VCF/VCA response curves and circuit solver work; see `docs/CIRCUIT_ANALYSIS.md`.

The supplied reference photograph belongs to its original rightsholder and is included for this user's local reference/project; do not assume it may be redistributed in third-party commercial releases. The same caveat applies to the accompanying scanned circuit drawings.

## Dependencies / licensing

This project links against JUCE 8.0.15, which is **dual-licensed AGPLv3 or commercial**. Depending on how you distribute the resulting binary or license your code, you may need a **commercial JUCE licence**. See `https://raw.githubusercontent.com/juce-framework/JUCE/8.0.15/LICENSE.md` and `https://juce.com/legal/juce-8-licence/`. Do not interpret a successful CMake build as authorisation for redistribution.

Engineering background reference: `https://github.com/clarionut/Polivoks_filter` describes the programmable op-amp filter behavior. The factual historical feature summary is independently cross-checked against `https://en.wikipedia.org/wiki/Polivoks`.

## GitHub Actions Apple Silicon build

`.github/workflows/macos-apple-silicon.yml` runs on GitHub's `macos-15` **arm64** runner. It fetches JUCE 8.0.15, builds the VST3 and AU (plus standalone), runs DSP tests, checks bundle binaries with `lipo`, then publishes an unsigned/unnotarized zip as the `Polivoks-Study-macOS-arm64` Actions artifact. Use **Actions → Build macOS Apple Silicon (VST3 + AU) → Run workflow** on `main`, or push to `main`. This CI recipe has not yet been executed on GitHub.

The scanned original schematics PDF is not needed for compilation and is intentionally excluded from the upload-ready source zip. Check redistributable rights for the reference panel photograph before publishing it in a public repository.