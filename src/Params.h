#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace vks::pid
{
// Модулятор
inline constexpr const char* lfoRate = "lfo_rate";
inline constexpr const char* lfoMode = "lfo_mode";
// Tastiera / uscita
inline constexpr const char* phones  = "phones";
inline constexpr const char* tune    = "tune";
inline constexpr const char* glide   = "glide";
inline constexpr const char* duo     = "duo";
inline constexpr const char* volume  = "volume";
inline constexpr const char* lineOut = "line_out";
// Генератор I / II
inline constexpr const char* g1Range = "g1_range";
inline constexpr const char* g1Shape = "g1_shape";
inline constexpr const char* g1Mod   = "g1_mod";
inline constexpr const char* g1Fm    = "g1_fm";
inline constexpr const char* g2Range = "g2_range";
inline constexpr const char* g2Shape = "g2_shape";
inline constexpr const char* g2Mod   = "g2_mod";
inline constexpr const char* g2Detune= "g2_detune";
// Микшер
inline constexpr const char* mixG1   = "mix_g1";
inline constexpr const char* mixG2   = "mix_g2";
inline constexpr const char* mixNoise= "mix_noise";
inline constexpr const char* mixExt  = "mix_ext";
// Фильтр
inline constexpr const char* fAtt    = "f_att";
inline constexpr const char* fDec    = "f_dec";
inline constexpr const char* fSus    = "f_sus";
inline constexpr const char* fRel    = "f_rel";
inline constexpr const char* fLoop   = "f_loop";
inline constexpr const char* fCut    = "f_cut";
inline constexpr const char* fRes    = "f_res";
inline constexpr const char* fLp     = "f_lp";
inline constexpr const char* fMod    = "f_mod";
inline constexpr const char* fEnv    = "f_env";
// Усилитель
inline constexpr const char* aAtt    = "a_att";
inline constexpr const char* aDec    = "a_dec";
inline constexpr const char* aSus    = "a_sus";
inline constexpr const char* aRel    = "a_rel";
inline constexpr const char* aLoop   = "a_loop";
inline constexpr const char* aMod    = "a_mod";
inline constexpr const char* aOn     = "a_on";
// Pedale, alimentazione, impostazioni
inline constexpr const char* pedal   = "pedal";
inline constexpr const char* power   = "power";
inline constexpr const char* bendRange = "bend_range";
inline constexpr const char* analog  = "analog";
inline constexpr const char* quality = "quality";
}

namespace vks
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    int v = 1;
    auto knob = [&] (const char* id, const char* name, float def, float lo = 0.0f, float hi = 1.0f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, v }, name, NormalisableRange<float> (lo, hi, 0.0f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([lo] (float x, int)
            {
                // scala del pannello: 0 … 6 (manopole bipolari: −2 … +2)
                return lo < 0.0f ? String (x * 2.0f, 2) : String (x * 6.0f, 2);
            })));
    };
    auto choice = [&] (const char* id, const char* name, StringArray items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, v }, name, items, def)); };
    auto toggle = [&] (const char* id, const char* name, bool def)
    { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, v }, name, def)); };

    const StringArray ranges { "32'", "16'", "8'", "4'", "2'" };
    const StringArray shapes { "Triangolo", "Dente di sega", "Quadra", "Impulso", "Impulso stretto" };

    knob   (pid::lfoRate, "Modulatore - Frequenza", 0.45f);
    choice (pid::lfoMode, "Modulatore - Modo", { "Triangolo", "Quadra", "Impulsi caotici", "Gradini casuali" }, 0);

    knob   (pid::phones, "Cuffia (T.LF)", 0.6f);
    knob   (pid::tune,   "Accordatura", 0.0f, -1.0f, 1.0f);
    knob   (pid::glide,  "Glissando", 0.0f);
    toggle (pid::duo,    "2 voci", false);
    knob   (pid::volume, "Volume", 0.7f);
    toggle (pid::lineOut,"Uscita", true);

    choice (pid::g1Range, "Gen I - Gamma", ranges, 2);
    choice (pid::g1Shape, "Gen I - Forma", shapes, 1);
    knob   (pid::g1Mod,   "Gen I - Modulazione", 0.0f);
    knob   (pid::g1Fm,    "Gen I - FM da Gen II", 0.0f);
    choice (pid::g2Range, "Gen II - Gamma", ranges, 2);
    choice (pid::g2Shape, "Gen II - Forma", shapes, 1);
    knob   (pid::g2Mod,   "Gen II - Modulazione", 0.0f);
    knob   (pid::g2Detune,"Gen II - Scordatura", 0.0f, -1.0f, 1.0f);

    knob   (pid::mixG1,    "Mixer - Gen I", 0.8f);
    knob   (pid::mixG2,    "Mixer - Gen II", 0.0f);
    knob   (pid::mixNoise, "Mixer - Rumore", 0.0f);
    knob   (pid::mixExt,   "Mixer - Esterno", 0.0f);

    knob   (pid::fAtt, "Filtro - Attacco", 0.0f);
    knob   (pid::fDec, "Filtro - Decadimento", 0.4f);
    knob   (pid::fSus, "Filtro - Piedistallo", 0.0f);
    knob   (pid::fRel, "Filtro - Rilascio", 0.3f);
    toggle (pid::fLoop,"Filtro - Inviluppo ripetuto", false);
    knob   (pid::fCut, "Filtro - Frequenza di taglio", 0.35f);
    knob   (pid::fRes, "Filtro - Risonanza", 0.2f);
    toggle (pid::fLp,  "Filtro - Passa-basso (FNCh)", true);
    knob   (pid::fMod, "Filtro - Modulazione", 0.0f);
    knob   (pid::fEnv, "Filtro - Inviluppo", 0.6f);

    knob   (pid::aAtt, "Amplificatore - Attacco", 0.0f);
    knob   (pid::aDec, "Amplificatore - Decadimento", 0.5f);
    knob   (pid::aSus, "Amplificatore - Piedistallo", 0.8f);
    knob   (pid::aRel, "Amplificatore - Rilascio", 0.25f);
    toggle (pid::aLoop,"Amplificatore - Inviluppo ripetuto", false);
    knob   (pid::aMod, "Amplificatore - Modulazione", 0.0f);
    toggle (pid::aOn,  "Amplificatore - VKL (sempre aperto)", false);

    knob   (pid::pedal, "Pedale", 0.0f);
    toggle (pid::power, "Rete (SET')", true);
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid::bendRange, v }, "Pitch bend (semitoni)", 0, 12, 2));
    knob   (pid::analog, "Deriva analogica", 1.0f);
    choice (pid::quality, "Qualita", { "Normale (2x)", "Alta (4x)" }, 0);

    return { p.begin(), p.end() };
}
}
