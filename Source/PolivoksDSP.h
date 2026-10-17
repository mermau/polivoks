#pragma once

// Real-time-safe DSP core: standard C++17, no JUCE, no heap allocations in render().
// Circuit-INSPIRED model, NOT a validated component-by-component circuit solver.
// The nonlinear filter is a two-integrator ZDF/SVF with tunable feedback and
// band-limiting/slew approximation inspired by К140УД12 programmable op-amps.
#include "Parameters.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace pv {

inline constexpr double pi = 3.141592653589793238462643383279502884;

struct Envelope {
    enum Stage { Off, Attack, Decay, Sustain, Release } stage = Off;
    double level = 0.0;

    void trigger() noexcept { stage = Attack; }
    void release() noexcept { if (stage != Off) stage = Release; }
    void reset() noexcept { stage = Off; level = 0.0; }
    double next (double dt, float atk, float dec, float sus, float rel, bool loop, bool gate) noexcept {
        const auto approach = [dt] (double v, double target, double seconds) noexcept {
            // Preserve a finite time constant; no instantaneous discontinuities.
            return target + (v - target) * std::exp (-dt * 7.0 / std::max (seconds, 0.001));
        };
        switch (stage) {
            case Off: level = 0.0; break;
            case Attack:
                level = approach (level, 1.0, envSeconds (atk));
                if (level >= 0.999) { level = 1.0; stage = Decay; }
                break;
            case Decay: {
                // When AD-repeat is on, decay smoothly to zero before retriggering.
                // This avoids a discontinuity at the loop point.
                const double destination = (loop && gate) ? 0.0 : sus;
                level = approach (level, destination, envSeconds (dec));
                if (std::abs (level - destination) < 0.001) {
                    level = destination;
                    stage = (loop && gate) ? Attack : Sustain;
                }
                break;
            }
            case Sustain:
                level = sus;
                if (loop && gate) { stage = Attack; level = 0.0; }
                break;
            case Release:
                level = approach (level, 0.0, envSeconds (rel));
                if (level < 0.0001) reset();
                break;
        }
        return level;
    }
};

struct Oscillator {
    double phase = 0.0;
    double triangle = -1.0;

    static double polyBlep (double p, double dt) noexcept {
        if (p < dt) { const double t = p / dt; return t + t - t * t - 1.0; }
        if (p > 1.0 - dt) {
            const double t = (p - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }
    double next (double frequency, double sampleRate, int wave) noexcept {
        const double dt = std::clamp (frequency / sampleRate, 0.0, 0.45);
        const double p = phase;
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        switch (wave) {
            case 1: return (2.0 * p - 1.0) - polyBlep (p, dt);      // saw
            case 2: case 3: case 4: {
                const double width = (wave == 2) ? 0.5 : (wave == 3 ? 0.25 : 0.10);
                double v = p < width ? 1.0 : -1.0;
                v += polyBlep (p, dt);
                double p2 = p - width;
                if (p2 < 0.0) p2 += 1.0;
                v -= polyBlep (p2, dt);
                return v - (2.0 * width - 1.0); // DC-reduced pulse
            }
            default: {
                // Triangle from integration of band-limited square.
                double s = p < 0.5 ? 1.0 : -1.0;
                s += polyBlep (p, dt);
                double p2 = p - 0.5;
                if (p2 < 0.0) p2 += 1.0;
                s -= polyBlep (p2, dt);
                triangle = 0.99998 * triangle + 4.0 * dt * s;
                return std::clamp (triangle, -1.25, 1.25);
            }
        }
    }
};

struct SlewFilter {
    double s1 = 0.0, s2 = 0.0;
    double lastBand = 0.0, lastLow = 0.0;

    void reset() noexcept { s1 = s2 = lastBand = lastLow = 0.0; }
    double next (double x, double cutoff, double resonance, double fs, bool bandpass) noexcept {
        // Two-integrator TPT filter with deliberately nonlinear feedback and
        // soft limits on op-amp-like internal stages.
        const double g = std::tan (pi * std::clamp (cutoff / fs, 0.00001, 0.43));
        const double k = 2.0 - 1.94 * std::clamp (resonance, 0.0, 1.0);
        const double norm = 1.0 / (1.0 + g * (g + k));
        const double nonlinearDrive = 1.0 + 0.6 * resonance;
        const double residual = std::tanh (x * nonlinearDrive) / nonlinearDrive - s2;
        double band = (s1 + g * residual) * norm;
        double low = s2 + g * band;
        // Smooth op-amp saturation and programmable slew approximation.
        const double maxSlew = 0.012 + 6.0 * cutoff / fs;
        band = lastBand + std::clamp (band - lastBand, -maxSlew, maxSlew);
        low  = lastLow  + std::clamp (low - lastLow, -maxSlew, maxSlew);
        band = 3.0 * std::tanh (band / 3.0);
        low  = 3.0 * std::tanh (low / 3.0);
        s1 = std::clamp (2.0 * band - s1, -12.0, 12.0);
        s2 = std::clamp (2.0 * low - s2, -12.0, 12.0);
        lastBand = band;
        lastLow = low;
        return bandpass ? band * 1.35 : low;
    }
};

class Engine {
public:
    Engine() noexcept {
        for (int i = 0; i < Count; ++i) params[static_cast<size_t>(i)] = definitions[static_cast<size_t>(i)].initial;
    }
    void prepare (double newSampleRate) noexcept {
        fs = std::clamp (newSampleRate, 8000.0, 384000.0);
        reset();
    }
    void reset() noexcept {
        osc1 = {}; osc2 = {}; filter.reset(); ampEnv.reset(); filterEnv.reset();
        held.fill (false); physical.fill(false); sustainOnly.fill(false); velocity.fill (0.0f);
        gate = false; currentPitch1 = currentPitch2 = 60.0;
        targetPitch1 = targetPitch2 = 60.0;
        modulationPhase = 0.0; shValue = 0.0;
        filtHz = 800.0; modWheel = 0.0;
        sustaining = false; pitchBend = 0.0;
    }
    void set (int index, float n) noexcept {
        if (index >= 0 && index < Count) params[static_cast<size_t>(index)] = clip01 (n);
    }
    float get (Param p) const noexcept { return params[static_cast<size_t>(p)]; }
    void setPitchBend (float n) noexcept { pitchBend = std::clamp (n, -1.0f, 1.0f); }
    void setModWheel (float n) noexcept { modWheel = clip01(n); }
    void setSustain (bool down) noexcept {
        sustaining = down;
        if (!down) for (int n = 0; n < 128; ++n)
            if (sustainOnly[static_cast<size_t>(n)]) {
                held[static_cast<size_t>(n)] = false;
                sustainOnly[static_cast<size_t>(n)] = false;
            }
        updateNotes(false);
    }
    void noteOn (int note, float vel) noexcept {
        if (note < 0 || note > 127 || vel <= 0.0f) return;
        held[static_cast<size_t>(note)] = true;
        physical[static_cast<size_t>(note)] = true;
        sustainOnly[static_cast<size_t>(note)] = false;
        velocity[static_cast<size_t>(note)] = clip01(vel);
        updateNotes(true);
    }
    void noteOff (int note) noexcept {
        if (note < 0 || note > 127) return;
        physical[static_cast<size_t>(note)] = false;
        if (sustaining) sustainOnly[static_cast<size_t>(note)] = true;
        else held[static_cast<size_t>(note)] = false;
        updateNotes(false);
    }
    void allNotesOff() noexcept {
        held.fill(false); physical.fill(false); sustainOnly.fill(false);
        gate = false; ampEnv.release(); filterEnv.release();
    }
    float render (float externalSample = 0.0f) noexcept {
        constexpr int oversampling = 4;
        const double dt = 1.0 / fs;
        const double lfoDelta = get(ModRate) < 0.001f ? 0.05 : 0.05 * std::pow (500.0, get(ModRate));
        const double prior = modulationPhase;
        modulationPhase += lfoDelta * dt;
        if (modulationPhase >= 1.0) modulationPhase -= std::floor(modulationPhase);
        if (modulationPhase < prior) shValue = randomBipolar();
        double lfo = 0.0;
        switch (selected (get(ModShape), 4)) {
            case 0: lfo = 1.0 - 4.0 * std::abs(modulationPhase - 0.5); break;
            case 1: lfo = modulationPhase < 0.5 ? 1.0 : -1.0; break;
            case 2: lfo = shValue; break;
            default: lfo = randomBipolar(); break;
        }
        const bool fLoop = selected (get(FLoop), 2) != 0;
        const bool aLoop = selected (get(ALoop), 2) != 0;
        const double fe = filterEnv.next(dt,get(FAttack),get(FDecay),get(FSustain),get(FRelease),fLoop,gate);
        const double ae = ampEnv.next(dt,get(AAttack),get(ADecay),get(ASustain),get(ARelease),aLoop,gate);
        const double glide = get(GlideTime) * get(GlideTime) * 2.5;
        const double pitchAlpha = glide < 0.001 ? 0.0 : std::exp (-dt * 6.9 / glide);
        currentPitch1 = pitchAlpha * currentPitch1 + (1.0 - pitchAlpha) * targetPitch1;
        currentPitch2 = pitchAlpha * currentPitch2 + (1.0 - pitchAlpha) * targetPitch2;
        const double common = (get(MasterTune) - 0.5) * 24.0 + pitchBend * 2.0;
        const double p1 = currentPitch1 + common + (selected(get(Osc1Range),5) - 2) * 12.0
                          + lfo * (get(Osc1Lfo) + modWheel * 0.3) * 4.0;
        const double p2 = currentPitch2 + common + (get(Osc2Detune) - 0.5) * 2.0
                          + (selected(get(Osc2Range),5) - 2) * 12.0
                          + lfo * (get(Osc2Lfo) + modWheel * 0.3) * 4.0;
        const double f1 = midiToHz(p1);
        const double f2 = midiToHz(p2);
        const double octaveCutoff = std::log2(24.0) + get(FCutoff) * 9.5
                                    + (get(Tone) - 0.5) * 0.85
                                    + get(FEnv) * 5.0 * fe
                                    + get(FLfo) * 3.0 * lfo;
        const double desiredHz = std::clamp (std::exp2(octaveCutoff), 20.0, std::min(19000.0, fs * 0.44));
        filtHz = filtHz * 0.993 + desiredHz * 0.007;
        const double drive = 1.0 + 7.0 * get(Tone) * get(Tone);
        const double res = get(FResonance);
        const bool bp = selected(get(FMode),2) != 0;
        const int w1 = selected(get(Osc1Wave),5);
        const int w2 = selected(get(Osc2Wave),5);
        const double osr = fs * oversampling;
        double out = 0.0;
        for (int sub=0; sub<oversampling; ++sub) {
            const double x2 = osc2.next(f2, osr, w2);
            const double deviation = 1.0 + (get(CrossMod) * 0.45) * x2;
            const double x1 = osc1.next (f1 * std::max(0.1,deviation), osr, w1);
            const double raw = (x1 * get(MixOsc1) + x2 * get(MixOsc2)) * 0.48
                + randomBipolar() * 0.28 * get(MixNoise)
                + externalSample * get(MixExternal);
            const double driven = std::tanh (raw * drive) / std::tanh (drive);
            out += filter.next(driven, filtHz, res, osr, bp);
        }
        out /= oversampling;
        const double tremolo = 1.0 - 0.95 * get(ALfo) * (0.5 + 0.5 * lfo);
        const double enabled = (selected(get(AmpOn),2) && selected(get(OutputOn),2)) ? 1.0 : 0.0;
        const double velAmp = (vel1 + vel2) * 0.5;
        const double y = out * ae * tremolo * enabled * velAmp * get(MasterGain) * 0.85;
        return static_cast<float>(std::tanh (y * 1.5));
    }

private:
    double fs = 44100.0;
    std::array<float, Count> params {};
    std::array<bool,128> held{}, physical{}, sustainOnly{};
    std::array<float,128> velocity{};
    Oscillator osc1, osc2;
    Envelope ampEnv, filterEnv;
    SlewFilter filter;
    bool gate = false, sustaining = false;
    double targetPitch1=60.0, targetPitch2=60.0, currentPitch1=60.0, currentPitch2=60.0;
    double vel1=1.0, vel2=1.0, modulationPhase=0.0, shValue=0.0, filtHz=800.0;
    float pitchBend=0.0f, modWheel=0.0f;
    uint32_t rng=0x5EAED123u;
    double randomBipolar() noexcept {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return 2.0 * (static_cast<double>(rng) / 4294967295.0) - 1.0;
    }
    static double midiToHz (double note) noexcept {
        return std::clamp (440.0 * std::exp2((note - 69.0) / 12.0), 1.0, 18000.0);
    }
    void updateNotes (bool retrigger) noexcept {
        int low=-1, high=-1, count=0;
        for (int n=0; n<128; ++n)
            if (held[static_cast<size_t>(n)]) {
                if (low == -1) low = n;
                high = n;
                ++count;
            }
        if (count == 0) {
            if (gate) { ampEnv.release(); filterEnv.release(); }
            gate = false;
            return;
        }
        const bool wasGated = gate;
        gate = true;
        const bool duo = selected(get(DuoMode),2) != 0;
        targetPitch1 = duo ? low : high;
        targetPitch2 = high;
        vel1 = velocity[static_cast<size_t>(duo ? low : high)];
        vel2 = velocity[static_cast<size_t>(high)];
        if (!wasGated) {
            currentPitch1 = targetPitch1; currentPitch2 = targetPitch2;
        }
        if (retrigger || !wasGated) { ampEnv.trigger(); filterEnv.trigger(); }
    }
};
} // namespace pv