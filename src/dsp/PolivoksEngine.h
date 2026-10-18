// Volks82 — emulazione del sintetizzatore Formanta "Polivoks" (1982)
// Motore DSP indipendente da JUCE: lavora a frequenza sovracampionata.
//
// Mappa dei blocchi originali (schemi ЭМС "Поливокс"):
//   У1  blocco tastiera  -> KeyAssigner: duofonia (nota più bassa = Gen I, più alta = Gen II),
//                           glissando solo su Gen I, impulso di trigger a ogni nuovo tasto,
//                           tensione tenuta (FET KП303) dopo il rilascio
//   У2/У3 generatori     -> Generator: nucleo a dente di sega esponenziale, triangolo ottenuto
//                           raddrizzando la sega (diodi КД521), impulsi da comparatore (3 larghezze)
//   У7  modulatore       -> Modulator: triangolo, quadra, impulsi caotici (comparatore su rumore
//                           filtrato 82k/10n), gradini casuali (sample & hold FET KП303 + 0.22 µF)
//   У6  mixer            -> sommatore invertente 22k/22k, condensatore d'uscita 0.22 µF
//   У4  filtro           -> SVF a 2 poli con amplificatori programmabili K140UD12 usati come
//                           integratori: la corrente di polarizzazione fissa la frequenza e limita
//                           lo slew-rate (tanh), da cui il carattere aggressivo; ФНЧ / ПФ
//   У5  VCA              -> coppia differenziale K118UD1 (tanh), inviluppo a 4 fasi con modo ripetuto
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace vks
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct EngineParams
{
    // Модулятор
    float lfoRate = 0.45f;      // 0..1
    int   lfoMode = 0;          // 0 triangolo, 1 quadra, 2 impulsi caotici, 3 gradini casuali
    // Sezione tastiera / uscita
    float phones = 0.6f;        // Т.ЛФ
    float tune = 0.0f;          // НАСТРОЙКА  -1..+1  (±1 tono = ±2 semitoni)
    float glide = 0.0f;         // ГЛИССАНДО
    bool  duo = false;          // 1-ГОЛ / 2-ГОЛ
    float volume = 0.7f;        // ГРОМКОСТЬ
    bool  lineOut = true;       // ВЫХОД
    // Генератор I
    int   g1Range = 2;          // 0=32' 1=16' 2=8' 3=4' 4=2'
    int   g1Shape = 1;          // 0 triangolo 1 sega 2 quadra 3 impulso 4 impulso stretto
    float g1Mod = 0.0f;
    float g1Fm = 0.0f;          // ГЕНЕРАТОР II
    // Генератор II
    int   g2Range = 2;
    int   g2Shape = 1;
    float g2Mod = 0.0f;
    float g2Detune = 0.0f;      // РАССТРОЙКА -1..+1 (±2 toni = ±4 semitoni)
    // Микшер
    float mixG1 = 0.8f, mixG2 = 0.0f, mixNoise = 0.0f, mixExt = 0.0f;
    // Фильтр
    float fAtt = 0.0f, fDec = 0.4f, fSus = 0.0f, fRel = 0.3f;
    bool  fLoop = false;
    float fCut = 0.55f, fRes = 0.2f;
    bool  fLowpass = true;      // ПФ/ФНЧ: premuto = ФНЧ (passa-basso), rilasciato = ПФ (passa-banda)
    float fMod = 0.0f, fEnv = 0.5f;
    // Усилитель
    float aAtt = 0.0f, aDec = 0.5f, aSus = 0.8f, aRel = 0.25f;
    bool  aLoop = false;
    float aMod = 0.0f;
    bool  aOn = false;          // ВКЛ: VCA sempre aperto sull'ultima nota (drone)
    // Pedale e alimentazione
    float pedal = 0.0f;
    bool  power = true;         // СЕТЬ
    // Non originali (comodità MIDI)
    float bendSemis = 0.0f;
    float analog = 1.0f;        // quantità di deriva/tolleranze analogiche
};

//==============================================================================
class Rng
{
public:
    explicit Rng (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float bipolar() { return (float) ((int32_t) next()) * (1.0f / 2147483648.0f); }
    inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
private:
    uint32_t s;
};

inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt;             return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt;  return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

//==============================================================================
// Inviluppo dei blocchi У4/У5: il condensatore si carica verso ~+12 V e il comparatore
// commuta in decadimento a +6 V (attacco quasi lineare); decadimento/rilascio esponenziali.
class Envelope
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double fs) { sr = fs; }
    void setTimes (float attKnob, float decKnob, float susKnob, float relKnob)
    {
        const double ta = 0.0012 * std::pow (2.0, attKnob * 12.2);  // ~1.2 ms .. ~5.6 s
        const double td = 0.004  * std::pow (2.0, decKnob * 11.3);  // ~4 ms .. ~10 s
        const double tr = 0.004  * std::pow (2.0, relKnob * 11.3);
        aCoef = coef (ta / std::log (2.0));
        dCoef = coef (td / 3.0);
        rCoef = coef (tr / 3.0);
        sustain = std::clamp (susKnob, 0.0f, 1.0f);
    }
    void gateOn()  { stage = Attack; }
    void retrigger() { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void reset() { stage = Idle; y = 0.0; }

    inline float tick (bool loop, bool gate)
    {
        switch (stage)
        {
            case Attack:
                y += (2.0 - y) * aCoef;
                if (y >= 1.0) { y = 1.0; stage = Decay; }
                break;
            case Decay:
                y += (sustain - y) * dCoef;
                if (y - sustain < 0.004)
                {
                    // ПЬЕДЕСТАЛ al massimo: il decadimento non ha effetto (come da manuale)
                    if (loop && gate && sustain < 0.99f) stage = Attack;
                    else stage = Sustain;
                }
                break;
            case Sustain:
                y = sustain;
                if (loop && gate && sustain < 0.99f) stage = Attack;
                break;
            case Release:
                y += (0.0 - y) * rCoef;
                if (y < 1.0e-5) { y = 0.0; stage = Idle; }
                break;
            case Idle: default: y = 0.0; break;
        }
        return (float) y;
    }
    float value() const { return (float) y; }
    Stage getStage() const { return stage; }

private:
    double coef (double tau) const { return 1.0 - std::exp (-1.0 / std::max (1.0e-6, tau * sr)); }
    double sr = 48000.0, y = 0.0, aCoef = 0.01, dCoef = 0.001, rCoef = 0.001;
    float sustain = 0.5f;
    Stage stage = Idle;
};

//==============================================================================
// Generatore (У2/У3): fase -> sega discendente "|\", triangolo = |sega| (raddrizzatore),
// impulsi = comparatore A10 sul triangolo con tre soglie (forma d'onda ФОРМА).
class Generator
{
public:
    void setSampleRate (double fs) { sr = fs; }
    void resetPhase (double p) { phase = p; }

    inline float tick (double freq, int shape)
    {
        const double dt = std::clamp (freq / sr, 1.0e-7, 0.45);
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        const double t = phase;
        float out = 0.0f;

        switch (shape)
        {
            case 0: // triangolo dal raddrizzamento della sega: piccola asimmetria del ponte a diodi
            {
                float saw = (float) (1.0 - 2.0 * t);
                float tri = std::fabs (saw);
                tri += 0.012f * (1.0f - tri) * (saw > 0 ? 1.0f : -1.0f); // soglia diodi non ideale
                out = 2.0f * tri - 1.0f;
                break;
            }
            case 1: // sega discendente con breve tempo di ritorno (scarica tramite V5/V6/V7)
                out = (float) (1.0 - 2.0 * t);
                out += 2.0f * polyBlep (t, dt);
                out *= 0.95f;
                break;
            default: // impulsi: 50 %, ~25 %, ~10 %
            {
                const double w = shape == 2 ? 0.5 : (shape == 3 ? 0.25 : 0.10);
                out = t < w ? 1.0f : -1.0f;
                out += polyBlep (t, dt);
                double t2 = t - w; if (t2 < 0.0) t2 += 1.0;
                out -= polyBlep (t2, dt);
                out *= 0.8f;
                break;
            }
        }
        return out;
    }
    double getPhase() const { return phase; }

private:
    double sr = 48000.0, phase = 0.0;
};

//==============================================================================
// Modulatore (У7)
class Modulator
{
public:
    void setSampleRate (double fs)
    {
        sr = fs;
        nzCoef = 1.0 - std::exp (-2.0 * kPi * 194.0 / fs); // R7 82k / C5 10 nF
        smCoef = 1.0 - std::exp (-2.0 * kPi * 900.0 / fs); // fronti della quadra non istantanei
    }

    // ritorna l'uscita bipolare ±1 (±1.5 V nell'originale)
    inline float tick (float rateKnob, int mode, float noiseSample)
    {
        const double f = 0.1 * std::pow (2.0, rateKnob * 9.0); // 0.1 .. ~51 Hz
        phase += f / sr;
        bool wrapped = false;
        if (phase >= 1.0) { phase -= 1.0; wrapped = true; }

        nz += (noiseSample - nz) * nzCoef;
        float target = 0.0f;
        switch (mode)
        {
            case 0: target = (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase); break;
            case 1: target = phase < 0.5 ? 1.0f : -1.0f; break;
            case 2: target = nz * 6.0 > 0.0 ? 1.0f : -1.0f; break;                 // impulsi caotici
            default:
                if (wrapped) held = (float) std::clamp (nz * 5.5, -1.0, 1.0);      // S&H a ogni periodo
                target = held;
                break;
        }
        out += (target - out) * smCoef;
        led = mode == 0 ? 0.5f * (out + 1.0f) : (out > 0.0f ? 1.0f : 0.0f);
        return out;
    }
    float ledLevel() const { return led; }

private:
    double sr = 48000.0, phase = 0.0, nz = 0.0, nzCoef = 0.01, smCoef = 0.1;
    float held = 0.0f, out = 0.0f, led = 0.0f;
};

//==============================================================================
// Filtro У4: SVF a 2 poli con integratori K140UD12 (OTA-like: corrente d'uscita limitata ->
// tanh sull'ingresso dell'integratore), retroazione di smorzamento regolata da РЕЗОНАНС,
// clipping ai rail degli operazionali.
class PolivoksFilter
{
public:
    void setSampleRate (double fs) { sr = fs; }
    void reset() { s1 = s2 = 0.0; }

    inline float tick (float x, double cutoffHz, float resKnob, bool lowpass)
    {
        const double fc = std::clamp (cutoffHz, 5.0, sr * 0.42);
        const double g = std::tan (kPi * fc / sr);
        // smorzamento: 1 -> Q 0.5; leggermente negativo al massimo -> autooscillazione limitata dalle tanh
        const double r = 1.0 - 1.2 * std::pow ((double) resKnob, 0.85);

        // soluzione lineare TPT, poi le non linearità sugli ingressi degli integratori
        const double hp = (x - (2.0 * r + g) * s1 - s2) / (1.0 + 2.0 * r * g + g * g);
        const double v1 = g * sat (hp);
        const double bp = v1 + s1;
        s1 = bp + v1;
        const double v2 = g * sat (bp);
        const double lp = v2 + s2;
        s2 = lp + v2;
        // rail degli operazionali (±12.5 V, uscita nominale 0.5 V)
        s1 = rail (s1); s2 = rail (s2);
        const double y = lowpass ? lp : bp * 1.15;
        return (float) rail (y);
    }

private:
    static inline double sat (double v) { constexpr double vs = 1.0; return vs * std::tanh (v / vs); }
    static inline double rail (double v) { constexpr double vr = 4.0; return vr * std::tanh (v / vr); }
    double sr = 48000.0, s1 = 0.0, s2 = 0.0;
};

//==============================================================================
// Assegnazione dei tasti (У1)
class KeyAssigner
{
public:
    KeyAssigner() { held.reserve (128); }
    void noteOn (int n)
    {
        removeNote (n);
        held.push_back (n);
        pressed[(size_t) (n & 127)] = true;
        changed = true;
    }
    void noteOff (int n, bool sustainPedal)
    {
        pressed[(size_t) (n & 127)] = false;
        if (sustainPedal) return;
        removeNote (n);
        changed = true;
    }
    void releaseSustained()
    {
        const auto before = held.size();
        held.erase (std::remove_if (held.begin(), held.end(),
                                    [this] (int n) { return ! pressed[(size_t) n]; }), held.end());
        if (held.size() != before) changed = true;
    }
    void allOff() { held.clear(); pressed.fill (false); changed = true; }

    // aggiorna le tensioni di tastiera; ritorna true se c'è un nuovo impulso di trigger
    bool update (bool duo)
    {
        if (! changed) return false;
        changed = false;
        if (held.empty()) { gate = false; return false; }
        const int lo = *std::min_element (held.begin(), held.end());
        const int hi = *std::max_element (held.begin(), held.end());
        const int n1 = lo;
        const int n2 = duo ? hi : lo;
        // l'impulso di avvio nasce dal cambiamento della tensione di tastiera (o dal primo tasto)
        const bool newKey = ! gate || n1 != note1 || n2 != note2;
        note1 = n1; note2 = n2;
        gate = true;
        return newKey;
    }

    bool gate = false;
    int note1 = 53, note2 = 53; // tensione tenuta (FET KП303) dopo il rilascio

private:
    void removeNote (int n) { held.erase (std::remove (held.begin(), held.end(), n), held.end()); }
    std::vector<int> held;
    std::array<bool, 128> pressed {};
    bool changed = false;
};

//==============================================================================
class PolivoksEngine
{
public:
    PolivoksEngine() = default;

    void prepare (double internalRate)
    {
        sr = internalRate;
        g1.setSampleRate (sr); g2.setSampleRate (sr);
        lfo.setSampleRate (sr);
        filt.setSampleRate (sr); filt.reset();
        envF.setSampleRate (sr); envA.setSampleRate (sr);
        envF.reset(); envA.reset();
        g2.resetPhase (0.37);
        dcCoef = 1.0 - std::exp (-2.0 * kPi * 8.0 / sr);
        driftCoef = 1.0 - std::exp (-2.0 * kPi * 0.07 / sr);
        smoothCoef = 1.0 - std::exp (-1.0 / (0.004 * sr));
        powerCoef = 1.0 - std::exp (-1.0 / (0.08 * sr));
        glideLevel1 = (double) keys.note1;
    }

    void setParams (const EngineParams& p) { prm = p; }
    const EngineParams& params() const { return prm; }

    void noteOn (int note)  { keys.noteOn (note); }
    void noteOff (int note) { keys.noteOff (note, sustainPedal); }
    void setSustainPedal (bool on) { sustainPedal = on; if (! on) keys.releaseSustained(); }
    void allNotesOff() { keys.allOff(); }

    float lfoLed() const { return prm.power ? lfo.ledLevel() : 0.0f; }
    bool  keyGate() const { return keys.gate; }

    // Rende n campioni alla frequenza interna. ext può essere nullptr.
    void process (float* out, const float* ext, int n)
    {
        const bool trig = keys.update (prm.duo);
        const bool gate = keys.gate || prm.aOn;
        if (trig) { envF.retrigger(); envA.retrigger(); }
        else if (gate && ! lastGate) { envF.gateOn(); envA.gateOn(); }
        else if (! gate && lastGate) { envF.gateOff(); envA.gateOff(); }
        lastGate = gate;

        envF.setTimes (prm.fAtt, prm.fDec, prm.fSus, prm.fRel);
        envA.setTimes (prm.aAtt, prm.aDec, prm.aSus, prm.aRel);

        // glissando (solo Generatore I): RC sulla tensione di tastiera
        const double glideTau = 0.0005 + 2.2 * std::pow ((double) prm.glide, 2.2);
        const double glideCoef = 1.0 - std::exp (-1.0 / (glideTau * sr));

        const double oct1 = (double) (prm.g1Range - 2);
        const double oct2 = (double) (prm.g2Range - 2);
        const double tuneSemis = prm.tune * 2.0 + prm.bendSemis;
        const double detuneSemis = prm.g2Detune * 3.86; // ±25 %
        const double vib1 = prm.g1Mod * prm.g1Mod * 1.0;   // ottave per unità di modulatore
        const double vib2 = prm.g2Mod * prm.g2Mod * 1.0;
        const double fmDepth = prm.g1Fm * prm.g1Fm * 3.0;
        const double fMod = prm.fMod * prm.fMod * 4.0;
        const double fEnvOct = prm.fEnv * 9.0;
        const double aMod = prm.aMod * prm.aMod * 1.2;
        const double an = prm.analog;

        const float mG1 = taper (prm.mixG1), mG2 = taper (prm.mixG2);
        const float mNz = taper (prm.mixNoise) * 0.75f, mExt = taper (prm.mixExt) * 2.0f;
        const double cutBaseOct = std::log2 (8.0) + prm.fCut * 10.8 + prm.pedal * 5.0;

        const float vol = std::pow (prm.volume, 2.5f) * 1.1f;
        const float ph  = std::pow (prm.phones, 2.5f) * 0.7f;
        const float powerTarget = prm.power ? 1.0f : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float white = rng.bipolar();
            const float m = lfo.tick (prm.lfoRate, prm.lfoMode, white);

            // deriva lenta dei generatori (±0.5 % in 4 ore nell'originale) + piccolo jitter
            drift1 += (rng.bipolar() * 0.9 - drift1) * driftCoef;
            drift2 += (rng.bipolar() * 0.9 - drift2) * driftCoef;

            glideLevel1 += ((double) keys.note1 - glideLevel1) * glideCoef;

            const double semis1 = glideLevel1 + tuneSemis + oct1 * 12.0 - 69.0
                                  + an * drift1 * 0.06;
            const double semis2 = (double) keys.note2 + tuneSemis + detuneSemis + oct2 * 12.0 - 69.0
                                  + an * (drift2 * 0.06 + 0.035);

            const double f2 = 440.0 * std::exp2 (semis2 / 12.0 + vib2 * m);
            const float o2 = g2.tick (f2, prm.g2Shape);
            const double f1 = 440.0 * std::exp2 (semis1 / 12.0 + vib1 * m + fmDepth * o2);
            const float o1 = g1.tick (f1, prm.g1Shape);

            // Mixer У6 (sommatore invertente) + condensatore d'uscita
            float mix = o1 * mG1 + o2 * mG2 + white * mNz;
            if (ext != nullptr) mix += ext[i] * mExt;
            mix = 3.0f * std::tanh (mix / 3.0f);
            dc += (mix - dc) * dcCoef;
            const float mixOut = mix - (float) dc;

            // Filtro
            const float ef = envF.tick (prm.fLoop, gate);
            const double cutOct = cutBaseOct + fEnvOct * ef + fMod * m + an * drift1 * 0.01;
            // rumore termico dei K140UD12 (innesca l'autooscillazione anche senza segnale)
            const float fo = filt.tick (mixOut * 0.85f + white * 2.0e-5f, std::exp2 (cutOct), prm.fRes, prm.fLowpass);

            // VCA У5 (coppia differenziale K118UD1)
            const float ea = envA.tick (prm.aLoop, gate);
            double gain = ea * (1.0 + aMod * m);
            gain = std::clamp (gain, 0.0, 1.6);
            gainSm += (gain - gainSm) * smoothCoef;
            const float vca = (float) (gainSm * 1.8 * std::tanh (fo / 1.8));

            // Uscite: ВЫХОД (linea, ГРОМКОСТЬ) oppure cuffia (Т.ЛФ, finale KT815/KT814)
            float y;
            if (prm.lineOut) y = vca * vol;
            else
            {
                float h = vca * ph * 1.4f;
                const float xo = 0.004f; // piccola zona morta del push-pull senza polarizzazione
                h = h > xo ? h - xo * 0.5f : (h < -xo ? h + xo * 0.5f : h * 0.5f);
                y = std::tanh (h * 1.2f) / 1.2f;
            }
            // stadio d'uscita: lineare fino a ~-3 dBFS, poi saturazione morbida (rail dell'operazionale)
            const float ay = std::fabs (y);
            if (ay > 0.7f) y = std::copysign (0.7f + 0.3f * std::tanh ((ay - 0.7f) / 0.3f), y);
            powerLvl += (powerTarget - powerLvl) * powerCoef;
            out[i] = y * (float) powerLvl;
        }
    }

private:
    static float taper (float k) { return k * k * (0.35f + 0.65f * k) * 1.0f + 0.0f; }

    EngineParams prm;
    double sr = 96000.0;
    Generator g1, g2;
    Modulator lfo;
    PolivoksFilter filt;
    Envelope envF, envA;
    KeyAssigner keys;
    Rng rng { 0xBADC0DEu };
    bool sustainPedal = false, lastGate = false;
    double dc = 0.0, dcCoef = 0.0, drift1 = 0.0, drift2 = 0.0, driftCoef = 0.0;
    double glideLevel1 = 53.0, gainSm = 0.0, smoothCoef = 0.01, powerLvl = 1.0, powerCoef = 0.001;
};

} // namespace vks
