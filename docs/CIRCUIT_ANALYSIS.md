# Polivoks engineering study — schematic mapping and fidelity limits

## Evidence from the user-supplied 18-page Russian scan

| PDF page | Visible Russian heading | Physical subsystem | Status of DSP representation |
|---|---|---|---|
| 11 | Блок генератора (У2, У3) | Two generator boards | Two digital oscillators; PolyBLEP saw / pulse, integrated triangle; octave selection, tuning and modulation. **Not** a transcription of transistor or op-amp equations. |
| 13 | Блок управляемых усилителей (У5) | Controlled amplifier stages | One shared amplitude ADSR and softly saturating VCA approximation. |
| 14 | Блок фильтра (У4) | Filter board | Two-integrator TPT state-variable filter with tunable nonlinear feedback and slew limiting. Lowpass or bandpass. **Not** a solved К140УД12 transistor-level model. |
| 15 | Блок модулятора (У7) | Modulator | 4 LFO waveforms, sample/hold and noise. |
| 16 | Блок клавиатуры (У1) | Keyboard and scanning | 48 keys; mono or two independently pitched VCOs feeding the same filter/VCA. MIDI adapts non-MIDI original hardware. |
| 7 | Блок питания (У8) | PSU | Deliberately not emulated as a separate electrical circuit; power supply imperfections can be modeled later if measured. |
| 9 | Педаль управления (У9) | Foot/remote control | MIDI CC mapping replaces physical control connections. |

Pages 1–5, 10, 12, 17–18 are mostly board/printed-track diagrams. Page 6 is another combined circuit sheet; page 8 shows an additional external/electrical block. The scans are not accompanied by calibrated transfer curves, measured tolerances, or verified operational audio recordings.

## Filter technology

Literature on hardware replicas describes the use of **two К140УД12 programmable op-amps** in the Polivoks filter. The voltage/current-dependent slew rate of these chips, feedback network, overload, and op-amp behaviour are important. A linear biquad alone cannot reproduce the instrument's nonlinearities. The code's `SlewFilter` is an explicit, documented approximation of this topology: lowpass / bandpass responses, tunable resonance, internal soft limits, and a cutoff-dependent rate limit.

For research background see `https://github.com/clarionut/Polivoks_filter`.

## Areas not yet faithful enough for a hardware-accurate release

1. Read and transcribe each component value/net into a circuit simulation (e.g. SPICE) at sufficient scan resolution. Confirm chip revisions and transistor parameters from the actual production unit(s).
2. Measure the amplitude-frequency response and resonance decay/self-oscillation, slew recovery, large-signal clipping, filter control calibration, and oscillator sync/tracking with a real original unit.
3. Quantify envelope attack/decay calibration and looping contours, oscillator waveform duty cycle and nonlinear mixer interactions.
4. Replace placeholder filter heuristics with a discretised physical model (WDF, nonlinear state-space, or a validated black-box approximation) and properly band-limited oversampling/decimation stages.
5. Capture and compare test audio for matched notes/patches using objective metrics (spectra, spectral convergence, transient error, THD, aliasing, parameter response) plus double-blind listening.
6. Perform DAW integration validation on actual macOS arm64 and x86_64 machines, including AU validation (`auval`), VST3 testing, plugin sandboxing, CI, load/save automation tests, host MIDI CC routing and memory/CPU profiling.

### Distinction between functionality and fidelity

All panel positions are mapped to interactive controls. However several aspects of the historical control labeling/function have been assigned a **useful digital interpretation rather than hardware-verified semantics**, notably Tone/Drive, cross-mod routing, voltage-controlled amp switches and ext input bus behavior. These must be reassessed against original operating manuals and circuit tracing before claiming a strict clone.
