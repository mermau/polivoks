#pragma once

// Shared parameter registry. All values are host-normalised [0,1].
// Coordinates refer to the 1000 x 563 reference photograph.
#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace pv {

enum Param : int {
    ModRate, ModShape, Tone, MasterTune, GlideTime, DuoMode, MasterGain, OutputOn,
    Osc1Range, Osc1Wave, Osc1Lfo, CrossMod,
    Osc2Range, Osc2Wave, Osc2Lfo, Osc2Detune,
    MixOsc1, MixOsc2, MixNoise, MixExternal,
    FAttack, FDecay, FSustain, FRelease, FCutoff, FResonance, FLfo, FEnv,
    FLoop, FMode,
    AAttack, ADecay, ASustain, ARelease, ALfo, ALoop, AmpOn,
    Count
};

struct ParamDef {
    const char* id;
    const char* label;
    float initial;
    int x, y;
    int steps;         // 0 = continuous; otherwise discrete values including endpoints
    bool toggle;
    const char* detail;
};

inline constexpr std::array<ParamDef, Count> definitions {{
    {"modRate",   "Modulator frequency",         .24f, 160,  68, 0,false,"LFO rate 0.05 - 25 Hz"},
    {"modShape",  "Modulator waveform",          .00f, 226,  68, 4,false,"Triangle / square / sample-hold / noise"},
    {"tone",      "Tone / drive",                .40f, 155, 126, 0,false,"Pre-filter colour and drive (approximation)"},
    {"tune",      "Master tuning",               .50f, 226, 126, 0,false,"Master pitch +/- 12 semitones"},
    {"glide",     "Glissando",                   .00f, 153, 185, 0,false,"Portamento 0 - 2.5 s"},
    {"duo",       "Keyboard: two voices",        .00f, 215, 188, 2,true,"Two independently pitched oscillators into one filter"},
    {"gain",      "Output volume",               .66f, 152, 246, 0,false,"Master output amplitude"},
    {"outOn",     "Output switch",              1.00f, 215, 251, 2,true,"Enable / mute output"},
    {"o1Range",   "Generator I range",          .50f, 294, 108, 5,false,"Footage -2 / -1 / 0 / +1 / +2 octaves"},
    {"o1Wave",    "Generator I waveform",       .25f, 367, 108, 5,false,"Triangle / saw / square / 25% pulse / 10% pulse"},
    {"o1Lfo",     "Generator I modulation",     .00f, 293, 171, 0,false,"LFO -> oscillator I pitch"},
    {"crossFm",   "Generator II to I",          .00f, 367, 171, 0,false,"Oscillator II -> oscillator I FM (approximation)"},
    {"o2Range",   "Generator II range",         .50f, 429, 108, 5,false,"Footage -2 / -1 / 0 / +1 / +2 octaves"},
    {"o2Wave",    "Generator II waveform",      .25f, 508, 108, 5,false,"Triangle / saw / square / 25% pulse / 10% pulse"},
    {"o2Lfo",     "Generator II modulation",    .00f, 429, 171, 0,false,"LFO -> oscillator II pitch"},
    {"o2Detune",  "Generator II fine tuning",   .50f, 508, 171, 0,false,"Fine detune +/- 100 cents"},
    {"mix1",      "Mixer generator I",          .68f, 293, 246, 0,false,"Oscillator I input level"},
    {"mix2",      "Mixer generator II",         .68f, 367, 246, 0,false,"Oscillator II input level"},
    {"mixNoise",  "Mixer noise",                .00f, 435, 246, 0,false,"Analogue-style white noise"},
    {"mixExt",    "Mixer external input",       .00f, 508, 246, 0,false,"Optional incoming audio bus; host support varies"},
    {"fAttack",   "Filter envelope attack",     .05f, 575,  70, 0,false,"Filter ADSR attack"},
    {"fDecay",    "Filter envelope decay",      .23f, 575, 130, 0,false,"Filter ADSR decay"},
    {"fSustain",  "Filter envelope sustain",    .65f, 630, 130, 0,false,"Filter ADSR sustain"},
    {"fRelease",  "Filter envelope release",    .24f, 692, 130, 0,false,"Filter ADSR release"},
    {"fCutoff",   "Filter cutoff frequency",     .62f, 575, 189, 0,false,"Logarithmic nominal cutoff"},
    {"fRes",      "Filter resonance",           .52f, 631, 189, 0,false,"High-Q feedback; nonlinear slew limiting"},
    {"fLfo",      "Filter modulator amount",    .00f, 575, 246, 0,false,"LFO -> filter cutoff"},
    {"fEnv",      "Filter envelope amount",     .43f, 630, 246, 0,false,"Filter ADSR -> cutoff"},
    {"fLoop",     "Filter envelope auto-repeat",.00f, 692,  76, 2,true,"Repeat attack / decay while gate held"},
    {"fMode",     "Filter mode",                .00f, 692, 188, 2,true,"Lowpass / bandpass (12 dB/oct nominal)"},
    {"aAttack",   "Amplifier envelope attack",  .00f, 759,  70, 0,false,"Amplifier ADSR attack"},
    {"aDecay",    "Amplifier envelope decay",   .24f, 814,  70, 0,false,"Amplifier ADSR decay"},
    {"aSustain",  "Amplifier envelope sustain", .88f, 759, 130, 0,false,"Amplifier ADSR sustain"},
    {"aRelease",  "Amplifier envelope release", .18f, 814, 130, 0,false,"Amplifier ADSR release"},
    {"aLfo",      "Amplifier modulation amount", .00f, 759, 246, 0,false,"LFO -> amplifier (tremolo)"},
    {"aLoop",     "Amplifier envelope auto-repeat",.00f,814,188,2,true,"Repeat attack / decay while gate held"},
    {"ampOn",     "Amplifier switch",           1.00f, 814, 247, 2,true,"Enable / mute voltage-controlled amplifier"}
}};

inline float clip01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline int selected (float norm, int steps) noexcept {
    if (steps < 2) return 0;
    return static_cast<int> (std::round (clip01 (norm) * (steps - 1)));
}
inline float envSeconds (float n) noexcept {
    return 0.001f + 7.999f * clip01 (n) * clip01 (n) * clip01 (n);
}
} // namespace pv