// Synthovoks — emulazione del sintetizzatore Formanta "Polivoks" (1982)
// Motore DSP indipendente da JUCE: lavora a frequenza sovracampionata.
//
// Unità interne: 1.0 = 0.5 V, il livello nominale indicato sugli schemi all'uscita del mixer
// e del filtro ("~0,5 В").
//
// Mappa dei blocchi originali (schemi ЭМС "Поливокс"):
//   У1  tastiera   -> KeyAssigner: duofonia (nota più bassa = Gen I, più alta = Gen II),
//                     glissando solo sul Gen I, impulso di avvio a ogni cambio di tensione,
//                     tensione tenuta (FET KП303) dopo il rilascio
//   У2/У3 generatori -> Generator: sega 0…+10 V (A7, scarica V5–V7), triangolo dal
//                     raddrizzatore di precisione A9 (0…−5 V), impulsi dal comparatore A10
//                     (uscita ±12 V ridotta da R38/R36 a circa ±2 V)
//   У7  modulatore -> Modulator: trigger di Schmitt A5 (soglie ±1,5 V con R13/R14) che carica
//                     C7 = 10 µF; triangolo ±1,5 V (A6), quadra solo positiva 0…+1,5 V (A7),
//                     impulsi caotici (comparatore su rumore filtrato 82k/10n), gradini casuali
//                     (sample & hold FET + 0,22 µF comandato a ogni periodo)
//   У6  mixer      -> sommatore invertente 22k/22k con potenziometri da 100 kΩ caricati
//   У4  filtro     -> due K140UD12 come integratori a transconduttanza: A2 riceve l'errore
//                     attraverso il partitore 47k/1k (÷48), A3 riceve l'uscita di A2 da R6/R7
//                     (÷48). La corrente di polarizzazione viene da A1+V1 (tensione -> corrente),
//                     quindi la frequenza di taglio è LINEARE nella somma dei comandi.
//                     La risonanza è il partitore R89 + R54 sull'uscita passa-banda.
//   У5  VCA        -> due coppie differenziali K118UD1 in serie (modulazione e inviluppo),
//                     ingressi ridotti di 100 volte da R2/R4 e R13/R16: tanh sui picchi forti
//   Inviluppi      -> condensatore da 5 µF caricato verso +12,5 V e commutato a +6 V dal
//                     comparatore; potenziometri da 1 MΩ (curva logaritmica) più resistenze serie
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
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
    float g2Detune = 0.0f;      // РАССТРОЙКА -1..+1 (±2 toni = ±25 %)
    // Микшер
    float mixG1 = 0.8f, mixG2 = 0.0f, mixNoise = 0.0f, mixExt = 0.0f;
    // Фильтр
    float fAtt = 0.0f, fDec = 0.4f, fSus = 0.0f, fRel = 0.3f;
    bool  fLoop = false;
    float fCut = 0.35f, fRes = 0.2f;
    bool  fLowpass = true;      // ПФ/ФНЧ: premuto = ФНЧ (passa-basso), rilasciato = ПФ (passa-banda)
    float fMod = 0.0f, fEnv = 0.6f;
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
// Matematica veloce
// uguaglianza esatta, usata solo per sapere se una manopola è cambiata (evita -Wfloat-equal)
inline bool same (float a, float b) { return ! (a < b) && ! (a > b); }
inline bool same (double a, double b) { return ! (a < b) && ! (a > b); }

inline float fastTanh (float x)
{
    // Padé 7/6, errore < 1e-4 per |x| < 4.97, poi saturazione. Per i segnali piccoli (il caso
    // comune) basta lo sviluppo x − x³/3 + 2x⁵/15, con errore < 2e-8.
    const float x2 = x * x;
    if (x2 < 0.01f) return x * (1.0f - x2 * (0.33333333f - 0.13333333f * x2));
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)))
             / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2)));
}
// tanh(x)/x, guadagno equivalente per la linearizzazione istantanea (senza dividere per x)
inline float tanhGain (float x)
{
    const float x2 = x * x;
    if (x2 < 0.01f) return 1.0f - x2 * (0.33333333f - 0.13333333f * x2);
    if (x2 > 24.7f) return 1.0f / std::fabs (x);
    return (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)))
         / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2)));
}

// 2^x veloce (errore relativo < 2e-7, cioè meno di 0,001 cent) per l'FM a ogni campione:
// parte intera scritta direttamente nell'esponente IEEE 754, parte frazionaria con un polinomio
inline double fastExp2 (double x)
{
    x = std::clamp (x, -1000.0, 1000.0);
    int i = (int) x;
    if (x < (double) i) --i;
    const double f = x - (double) i;
    const double p = 1.0 + f * (0.6931471805599453 + f * (0.2402265069591007 + f * (0.0555041086648216
                     + f * (0.0096181291076285 + f * (0.0013333558146428 + f * 0.0001540353039338)))));
    uint64_t bits;
    std::memcpy (&bits, &p, sizeof bits);
    bits += (uint64_t) (int64_t) i << 52;
    double r;
    std::memcpy (&r, &bits, sizeof r);
    return r;
}

class Rng
{
public:
    explicit Rng (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float bipolar() { return (float) ((int32_t) next()) * (1.0f / 2147483648.0f); }
private:
    uint32_t s;
};

inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt;             return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt;  return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}
// polyBLAMP: correzione dei cambi di pendenza (spigoli del triangolo)
inline float polyBlamp (double t, double dt)
{
    if (t < dt)       { t = t / dt - 1.0;         return (float) (-1.0 / 3.0 * t * t * t); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt + 1.0; return (float) ( 1.0 / 3.0 * t * t * t); }
    return 0.0f;
}

// Potenziometro lineare usato come partitore e caricato da una resistenza verso massa virtuale:
// guadagno = a·Rl / (Rl + a(1−a)·Rp)
inline float loadedPot (float a, float rLoad, float rPot)
{
    a = std::clamp (a, 0.0f, 1.0f);
    return a * rLoad / (rLoad + a * (1.0f - a) * rPot);
}
// Potenziometro logaritmico (gruppo "Б"): ~10 % della resistenza a metà corsa
inline double logPot (float k) { return (std::pow (10.0, 2.0 * (double) std::clamp (k, 0.0f, 1.0f)) - 1.0) / 99.0; }

//==============================================================================
// Inviluppo dei blocchi У4/У5
class Envelope
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double fs) { sr = fs; lastA = -1.0f; }
    // rSerieX: resistenze in serie ai potenziometri (Ω); C = 5 µF, potenziometri 1 MΩ logaritmici
    void setSeries (double ra, double rd, double rr) { rsA = ra; rsD = rd; rsR = rr; }
    void setTimes (float attKnob, float decKnob, float susKnob, float relKnob)
    {
        if (same (attKnob, lastA) && same (decKnob, lastD) && same (susKnob, lastS) && same (relKnob, lastR)) return;
        lastA = attKnob; lastD = decKnob; lastS = susKnob; lastR = relKnob;
        constexpr double C = 5.0e-6, Rpot = 1.0e6;
        aCoef = coef (C * (rsA + Rpot * logPot (attKnob)));
        dCoef = coef (C * (rsD + Rpot * logPot (decKnob)));
        rCoef = coef (C * (rsR + Rpot * logPot (relKnob)));
        sustain = std::clamp (susKnob, 0.0f, 1.0f);   // ПЬЕДЕСТАЛ: partitore lineare 0…6 V
    }
    void trigger() { stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void reset() { stage = Idle; y = 0.0; }

    inline float tick (bool loop, bool gate)
    {
        switch (stage)
        {
            case Attack:
                // carica verso +12,5 V, il comparatore commuta a +6 V
                y += (kAttackTarget - y) * aCoef;
                if (y >= 1.0) { y = 1.0; stage = Decay; }
                break;
            case Decay:
                y += (sustain - y) * dCoef;
                // modo ripetuto (∧∧): il comparatore riparte quando la tensione torna quasi a zero,
                // quindi la ripetizione avviene con ПЬЕДЕСТАЛ basso, come descritto nel manuale
                if (loop && gate && y < kRepeatThreshold) stage = Attack;
                else if (std::fabs (y - sustain) < 1.0e-4) stage = Sustain;
                break;
            case Sustain:
                y = sustain;
                if (loop && gate && y < kRepeatThreshold) stage = Attack;
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
    bool isIdle() const { return stage == Idle; }

private:
    static constexpr double kAttackTarget = 12.5 / 6.0;
    static constexpr double kRepeatThreshold = 0.08;
    double coef (double tau) const { return 1.0 - std::exp (-1.0 / std::max (1.0e-6, tau * sr)); }
    double sr = 48000.0, y = 0.0, aCoef = 0.01, dCoef = 0.001, rCoef = 0.001;
    double rsA = 200.0, rsD = 200.0, rsR = 200.0;
    float sustain = 0.5f;
    float lastA = -1.0f, lastD = -1.0f, lastS = -1.0f, lastR = -1.0f;
    Stage stage = Idle;
};

//==============================================================================
// Generatore (У2/У3). Le registrazioni dell'originale mostrano la stessa ampiezza picco-picco
// per sega, quadra e impulso (= 2.0 in unità interne).
class Generator
{
public:
    void setSampleRate (double fs) { sr = fs; lastFreq = -1.0; }
    void resetPhase (double p) { phase = p; }

    inline float tick (double freq, int shape)
    {
        if (! same (freq, lastFreq))   // cambia solo a frequenza di controllo (o con l'FM)
        {
            lastFreq = freq;
            // tempo di scarica del nucleo a sega, quasi tutto compensato dal trimmer СМЕЩЕНИЕ:
            // nelle registrazioni dell'originale il Do7 cala di ~2 cent
            fEff = freq / (1.0 + freq * 0.2e-6);
            dt = std::clamp (fEff / sr, 1.0e-7, 0.45);
        }
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        const double t = phase;

        switch (shape)
        {
            case 0: // triangolo dal raddrizzatore di precisione A9, accoppiato in AC
            {
                const float saw = (float) (1.0 - 2.0 * t);
                float tri = std::fabs (saw);
                tri += 0.012f * (1.0f - tri) * (saw > 0 ? 1.0f : -1.0f); // soglia non ideale dei diodi
                // spigoli in t = 0 (pendenza da +4 a −4) e t = 0,5 (da −4 a +4)
                double th = t + 0.5; if (th >= 1.0) th -= 1.0;
                const float corr = (float) (4.0 * dt) * (polyBlamp (th, dt) - polyBlamp (t, dt));
                // il gradino dei diodi a metà periodo (−0,048) corretto con un polyBLEP
                return 2.0f * tri - 1.0f + corr - 0.024f * polyBlep (th, dt);
            }
            case 1: // sega: rampa che sale e ricaduta veloce, con la stessa polarità degli impulsi
                    // (verificata sulle registrazioni)
                return (float) (2.0 * t - 1.0) - polyBlep (t, dt);   // polyBLEP normalizzato su un salto di 2
            default: // comparatore A10 sulla sega. Misurati sulle registrazioni: quadra 46,6 %, impulso 19,8 %;
            {        // impulso stretto ~10 % (stima). Il comparatore allunga lo stato alto di ~13 µs, per cui
                     // in acuto il ciclo utile cresce (Do6: quadra 49,8 %, impulso 22,6 %, come misurato).
                const double w = std::min (0.9, (shape == 2 ? 0.466 : (shape == 3 ? 0.198 : 0.10)) + 13.0e-6 * fEff);
                float out = t < w ? 1.0f : -1.0f;
                out += polyBlep (t, dt);
                double t2 = t - w; if (t2 < 0.0) t2 += 1.0;
                out -= polyBlep (t2, dt);
                return 0.8f * out;   // picco-picco relativo alla sega misurato: ~0,8
            }
        }
    }

private:
    double sr = 48000.0, phase = 0.0, lastFreq = -1.0, fEff = 0.0, dt = 0.0;
};

//==============================================================================
// Modulatore (У7): trigger di Schmitt A5 + condensatore C7
class Modulator
{
public:
    void setSampleRate (double fs)
    {
        sr = fs;
        nzCoef = (float) (1.0 - std::exp (-2.0 * kPi * 194.0 / fs)); // R7 82k / C5 10 nF
        lastRate = -1.0f;
    }

    // C7 = 10 µF caricato dall'uscita ±12 V di A5 attraverso 4,7 kΩ + potenziometro da 1 MΩ:
    // il triangolo tra ±1,5 V è un tratto quasi lineare di una carica RC (0,2 … ~40 Hz)
    void setRate (float rateKnob)
    {
        if (same (rateKnob, lastRate)) return;
        lastRate = rateKnob;
        const double R = 4.7e3 + 1.0e6 * logPot (1.0f - rateKnob);
        k = (float) (1.0 / (R * 10.0e-6 * sr));
    }

    inline float tick (int mode, float noiseSample)
    {
        tri += ((sq ? 8.0f : -8.0f) - tri) * k;            // 12 V / 1,5 V = 8
        bool newPeriod = false;
        if (sq && tri >= 1.0f)        { tri = 1.0f;  sq = false; }
        else if (! sq && tri <= -1.0f) { tri = -1.0f; sq = true; newPeriod = true; }

        nz += (noiseSample - nz) * nzCoef;
        switch (mode)
        {
            case 0: out = tri; break;                                       // triangolo ±1,5 V
            case 1: out = sq ? 1.0f : 0.0f; break;                          // quadra 0…+1,5 V
            case 2: out = nz > 0.0f ? 1.0f : -1.0f; break;                  // impulsi caotici (comparatore A3)
            default:
                if (newPeriod) held = std::clamp (nz * 2.2f, -1.0f, 1.0f);  // S&H all'inizio del periodo
                out = held;
                break;
        }
        return out;
    }
    float ledLevel() const { return sq ? 1.0f : 0.0f; }   // V5 pilotato dalla quadra di A5

private:
    double sr = 48000.0;
    float tri = 0.0f, nz = 0.0f, nzCoef = 0.01f, held = 0.0f, out = 0.0f, k = 0.0f, lastRate = -1.0f;
    bool sq = true;
};

//==============================================================================
// Generatore di rumore (blocco У7): giunzione base-emettitore di V1 (KT315Б) in valanga, polarizzata
// da R1 100 k. C1 10 nF su R2 100 k = passa-alto a 159 Hz; C2 100 pF sullo stesso nodo = passa-basso
// a 15,9 kHz; A1 (×6,6) e A2 (×100, KR140UD8: banda di guadagno unitario ~1 MHz -> ~10 kHz).
// Lo spettro è quindi definito in Hz e non dipende più dalla frequenza di campionamento interna.
class NoiseSource
{
public:
    void prepare (double fs)
    {
        hp = (float) std::exp (-2.0 * kPi * 159.0 / fs);
        lp1 = (float) (1.0 - std::exp (-2.0 * kPi * 15900.0 / fs));
        lp2 = (float) (1.0 - std::exp (-2.0 * kPi * 10000.0 / fs));
        // normalizzazione: valore efficace fisso (indipendente dalla frequenza di campionamento)
        Rng r (0x5EEDu); reset(); double acc = 0.0; const int n = 1 << 16;
        gain = 1.0f;
        for (int i = 0; i < n; ++i) { const float v = tick (r.bipolar()); acc += (double) v * v; }
        gain = (float) (kRms / std::sqrt (acc / n));
        reset();
    }
    void reset() { x1 = y1 = a = b = 0.0f; }
    inline float tick (float white)
    {
        const float h = hp * (y1 + white - x1);   // passa-alto a un polo
        x1 = white; y1 = h;
        a += (h - a) * lp1;
        b += (a - b) * lp2;
        return b * gain;
    }
    static constexpr float kRms = 0.42f;   // ~±1,5 V di picco su A2, in unità interne relative alle onde

private:
    float hp = 0.99f, lp1 = 0.5f, lp2 = 0.5f, gain = 1.0f, x1 = 0.0f, y1 = 0.0f, a = 0.0f, b = 0.0f;
};

//==============================================================================
// Tolleranze di uno strumento reale: ogni registro (ДИАПАЗОН) passa per una rete di resistenze
// tarata a mano (trimmer "Регистры", vedi la procedura di accordatura), e il convertitore
// esponenziale di ogni generatore ha un piccolo errore di scala. Valori fissi, come su un singolo
// esemplare; proporzionali alla "deriva analogica" (a zero lo strumento è ideale).
struct Tolerances
{
    std::array<float, 5> range1 {}, range2 {};   // cent per registro
    float scale1 = 0.0f, scale2 = 0.0f;          // errore di scala, cent per ottava dal La 440
    Tolerances()
    {
        Rng r (0x0F0E1982u);
        for (auto& c : range1) c = 2.0f * r.bipolar();
        for (auto& c : range2) c = 2.0f * r.bipolar();
        range1[2] = 0.0f;                         // 8' è il registro di riferimento dell'accordatura
        scale1 = 0.6f * r.bipolar();
        scale2 = 0.6f * r.bipolar();
    }
};

//==============================================================================
// Filtro У4 (vedi intestazione). In forma normalizzata, con e = ingresso − LP − d·BP:
//   dBP/dt = ω·S·tanh(e/S)     (A2, errore ÷48 sulla coppia d'ingresso, S = 48·2Vt/0,5 V ≈ 5)
//   dLP/dt = ω·S·tanh(BP/S)    (A3, uscita di A2 ÷48 da R6/R7)
// d = smorzamento dal partitore R89 (100 k) + R54 (47 k), meno un piccolo contributo dovuto al
// secondo polo dei K140UD12 che permette l'autooscillazione a risonanza massima.
// Integrazione trapezoidale (TPT) con linearizzazione istantanea delle tanh.
class PolivoksFilter
{
public:
    void setSampleRate (double fs) { sr = fs; }
    void reset() { s1 = s2 = 0.0f; e1 = e2 = 0.0f; }
    static float gFor (double hz, double fs) { return (float) std::tan (kPi * std::clamp (hz, 0.2, fs * 0.42) / fs); }

    static float dampingFor (float resKnob)
    {
        const float a = 1.0f - std::clamp (resKnob, 0.0f, 1.0f);   // cursore verso massa = più risonanza
        const float d = a * 47.0f / (47.0f + a * (1.0f - a) * 100.0f);
        return d - 0.035f;
    }

    inline float tick (float x, float g, float d, bool lowpass)
    {
        constexpr float S = 5.0f, kRail = 22.0f;   // saturazione d'ingresso e rail (±11 V) in unità
        const float g1 = g * tanhGain (e1 / S);
        const float g2 = g * tanhGain (e2 / S);
        const float hp = (x - (g2 + d) * s1 - s2) / (1.0f + g1 * (g2 + d));
        const float v1 = g1 * hp;
        const float bp = v1 + s1;
        s1 = bp + v1;
        const float v2 = g2 * bp;
        const float lp = v2 + s2;
        s2 = lp + v2;
        e1 = hp; e2 = bp;
        // rail degli operazionali
        if (std::fabs (s1) > kRail) s1 = std::copysign (kRail, s1);
        if (std::fabs (s2) > kRail) s2 = std::copysign (kRail, s2);
        return lowpass ? lp : bp;
    }

private:
    double sr = 48000.0;
    float s1 = 0.0f, s2 = 0.0f, e1 = 0.0f, e2 = 0.0f;
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

    // aggiorna le tensioni di tastiera; ritorna true se c'è un nuovo impulso di avvio
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
    static constexpr int kControlInterval = 16;   // campioni (a frequenza interna) tra due aggiornamenti dei comandi

    void prepare (double internalRate)
    {
        sr = internalRate;
        g1.setSampleRate (sr); g2.setSampleRate (sr);
        g2.resetPhase (0.37);
        lfo.setSampleRate (sr);
        noise.prepare (sr);
        filt.setSampleRate (sr); filt.reset();
        envF.setSampleRate (sr); envA.setSampleRate (sr);
        envF.setSeries (200.0, 4300.0, 200.0);   // R9, R10, R12 del blocco У4
        envA.setSeries (200.0, 200.0, 200.0);    // R21, R22, R23 del blocco У5
        envF.reset(); envA.reset();
        dcCoef = (float) (1.0 - std::exp (-2.0 * kPi * 12.0 / sr));   // accoppiamento in AC misurato (~12 Hz)
        shelfCoef = (float) std::tan (kPi * 3100.0 / sr);
        ovCoef = (float) std::tan (kPi * (1.0 / (2.0 * kPi * 0.23e-3)) / sr);
        vcaDcCoef = (float) (1.0 - std::exp (-2.0 * kPi * 0.8 / sr)); // C1 10 µF / R2 20 k del VCA
        driftCoef = 1.0 - std::exp (-2.0 * kPi * 0.07 / sr);
        smoothCoef = (float) (1.0 - std::exp (-1.0 / (0.002 * sr)));
        powerCoef = (float) (1.0 - std::exp (-1.0 / (0.08 * sr)));
        // i potenziometri si muovono con continuità: i valori in arrivo (MIDI a 7 bit, automazioni a
        // blocchi) vengono raccordati con una costante di tempo di 5 ms, a frequenza di controllo
        potCoef = (float) (1.0 - std::exp (-(double) kControlInterval / (0.005 * sr)));
        glideLevel1 = (double) keys.note1;
        lastGlide = -1.0f;
        ctrlCount = 0; fresh = true; snapPots = true;
        gCur = gNext = PolivoksFilter::gFor (1000.0, sr);
        gStep = 0.0f;
    }

    void setParams (const EngineParams& p) { prm = p; }
    const EngineParams& params() const { return prm; }

    void noteOn (int note)  { keys.noteOn (note); }
    void noteOff (int note) { keys.noteOff (note, sustainPedal); }
    void setSustainPedal (bool on) { sustainPedal = on; if (! on) keys.releaseSustained(); }
    void allNotesOff() { keys.allOff(); }

    float lfoLed() const { return prm.power ? lfo.ledLevel() : 0.0f; }
    bool  keyGate() const { return keys.gate; }
    bool  isSilent() const { return envA.isIdle() && ! keys.gate && ! prm.aOn; }

    // Rende n campioni alla frequenza interna. ext può essere nullptr.
    void process (float* out, const float* ext, int n)
    {
        const bool trig = keys.update (prm.duo);
        const bool gate = keys.gate || prm.aOn;
        if (trig || (gate && ! lastGate)) { envF.trigger(); envA.trigger(); }
        else if (! gate && lastGate)       { envF.gateOff(); envA.gateOff(); }
        lastGate = gate;

        envF.setTimes (prm.fAtt, prm.fDec, prm.fSus, prm.fRel);
        envA.setTimes (prm.aAtt, prm.aDec, prm.aSus, prm.aRel);
        lfo.setRate (prm.lfoRate);

        // ---------------------------------------------------------- spento: niente da calcolare
        const float powerTarget = prm.power ? 1.0f : 0.0f;
        if (! prm.power && powerLvl < 1.0e-4f)
        {
            std::fill (out, out + n, 0.0f);
            powerLvl = 0.0f;
            idleReset();
            return;
        }
        // ---------------------------------------------------------- silenzio: solo il modulatore (LED)
        if (envA.isIdle() && ! gate && powerLvl > 0.999f && prm.power)
        {
            for (int i = 0; i < n; ++i) lfo.tick (prm.lfoMode, noise.tick (rng.bipolar()));
            std::fill (out, out + n, 0.0f);
            powerLvl = 1.0f;
            idleReset();
            return;
        }

        // ---------------------------------------------------------- bersagli dei comandi
        if (! same (prm.glide, lastGlide))
        {
            lastGlide = prm.glide;
            const double glideTau = 0.0005 + 2.2 * std::pow ((double) prm.glide, 2.2);
            glideCoef = 1.0 - std::exp (-1.0 / (glideTau * sr));
        }
        Pots t;
        t.tuneSemis   = prm.tune * 2.0f + prm.bendSemis;
        t.detuneSemis = prm.g2Detune * 3.86f;                         // ±25 %
        t.vib1   = loadedPot (prm.g1Mod, 100.0f, 100.0f);             // ottave per unità di modulatore
        t.vib2   = loadedPot (prm.g2Mod, 100.0f, 100.0f);
        t.fm     = loadedPot (prm.g1Fm, 47.0f, 100.0f) * 2.5f;
        // mixer: potenziometri da 100 kΩ caricati dagli ingressi da 22 kΩ del sommatore
        t.mG1    = loadedPot (prm.mixG1, 22.0f, 100.0f);
        t.mG2    = loadedPot (prm.mixG2, 22.0f, 100.0f);
        t.mNz    = loadedPot (prm.mixNoise, 22.0f, 100.0f);
        t.mExt   = loadedPot (prm.mixExt, 22.0f, 100.0f) * 2.0f;
        // filtro: corrente di polarizzazione = somma lineare dei comandi (A1 + V1).
        // ЧАСТОТА СРЕЗА: 12,5 V attraverso R87 47 k + la parte alta del potenziometro da 100 k
        // verso la massa virtuale, meno la corrente a manopola chiusa (filtro chiuso a zero).
        t.cutI   = (float) ((12.5 / (47.0 + (1.0 - prm.fCut) * 100.0) - 12.5 / 147.0) / 0.180952);
        t.envI   = loadedPot (prm.fEnv, 100.0f, 100.0f) * 0.40f;      // inviluppo 0…6 V, R88 100 k
        t.modI   = loadedPot (prm.fMod, 100.0f, 100.0f) * 0.11f;      // modulatore ±1,5 V, R86 100 k
        t.pedI   = prm.pedal * 0.7f;                                   // pedale attraverso 39 k
        t.damp   = PolivoksFilter::dampingFor (prm.fRes);
        t.aMod   = loadedPot (prm.aMod, 100.0f, 100.0f) * 0.8f;
        t.vol    = std::pow (prm.volume, 2.5f) * 0.45f;
        t.ph     = std::pow (prm.phones, 2.5f) * 0.55f;
        if (snapPots) { pot = t; potPrev = t; snapPots = false; }

        constexpr float fMax = 12000.0f;
        const float an = prm.analog;
        const float oct1 = (float) (prm.g1Range - 2), oct2 = (float) (prm.g2Range - 2);
        const float tol1 = an * (tol.range1[(size_t) prm.g1Range] / 100.0f);
        const float tol2 = an * (tol.range2[(size_t) prm.g2Range] / 100.0f);
        const float invCI = 1.0f / (float) kControlInterval;

        for (int i = 0; i < n; ++i)
        {
            const float white = rng.bipolar();
            const float nz = noise.tick (white);
            const float m = lfo.tick (prm.lfoMode, nz);
            const float ef = envF.tick (prm.fLoop, gate);
            const float ea = envA.tick (prm.aLoop, gate);
            glideLevel1 += ((double) keys.note1 - glideLevel1) * glideCoef;

            // ------------------------------------------- aggiornamenti a frequenza di controllo
            if (--ctrlCount <= 0)
            {
                ctrlCount = kControlInterval;
                potPrev = pot;
                potsMoving = pot.approach (t, potCoef);
                if (! potsMoving) potPrev = pot;
                drift1 += (rng.bipolar() * 0.9 - drift1) * driftCoef * kControlInterval;
                drift2 += (rng.bipolar() * 0.9 - drift2) * driftCoef * kControlInterval;

                const float iSum = pot.cutI + pot.envI * ef + pot.modI * m + pot.pedI + (float) (an * drift1 * 0.004);
                const float hz = std::max (0.0f, iSum) * fMax;
                gCur = gNext;
                gNext = PolivoksFilter::gFor (hz, sr);
                if (fresh) { gCur = gNext; fresh = false; }
                gStep = (gNext - gCur) * invCI;

                // intonazione in semitoni dal La 440, poi errori di scala dei convertitori
                const double s1 = glideLevel1 + pot.tuneSemis + oct1 * 12.0 - 69.0 + an * drift1 * 0.06;
                const double s2 = (double) keys.note2 + pot.tuneSemis + pot.detuneSemis + oct2 * 12.0 - 69.0
                                  + an * (drift2 * 0.06 + 0.035);
                const double o1 = s1 / 12.0, o2 = s2 / 12.0;
                f1Base = 440.0 * fastExp2 (o1 * (1.0 + an * tol.scale1 / 1200.0) + tol1 / 12.0 + pot.vib1 * m);
                f2Base = 440.0 * fastExp2 (o2 * (1.0 + an * tol.scale2 / 1200.0) + tol2 / 12.0 + pot.vib2 * m);
            }
            gCur += gStep;
            // posizione nell'intervallo di controllo: i livelli sono interpolati senza scalini
            const float fr = potsMoving ? (float) (kControlInterval - ctrlCount) * invCI : 1.0f;
            auto lerp = [fr] (float a, float b) { return a + (b - a) * fr; };

            // ------------------------------------------- generatori
            const float o2 = g2.tick (f2Base, prm.g2Shape);
            const double f1 = pot.fm > 1.0e-4f ? f1Base * fastExp2 ((double) pot.fm * o2) : f1Base;
            const float o1 = g1.tick (f1, prm.g1Shape);

            // ------------------------------------------- mixer
            float mix = o1 * lerp (potPrev.mG1, pot.mG1) + o2 * lerp (potPrev.mG2, pot.mG2)
                      + nz * lerp (potPrev.mNz, pot.mNz);
            if (ext != nullptr) mix += ext[i] * lerp (potPrev.mExt, pot.mExt);
            if (std::fabs (mix) > 14.0f) mix = 22.0f * fastTanh (mix / 22.0f);   // rail ±11 V
            dc += (mix - dc) * dcCoef;
            float mixOut = mix - dc;
            // risposta delle uscite dei generatori misurata sulle registrazioni:
            //  - dopo ogni fronte un piccolo superamento (~12 % del picco-picco, τ ≈ 0,23 ms)
            //  - sopra ~3 kHz una discesa a mensola fino a circa −11 dB (a = 0,29, fc ≈ 3,1 kHz)
            {
                const float v = (mixOut - ovS) * ovCoef / (1.0f + ovCoef);
                const float lpo = v + ovS; ovS = lpo + v;
                mixOut += 0.12f * (mixOut - lpo);
                const float w = (mixOut - shelfS) * shelfCoef / (1.0f + shelfCoef);
                const float lps = w + shelfS; shelfS = lps + w;
                mixOut = 0.29f * mixOut + 0.71f * lps;
            }

            // ------------------------------------------- filtro (rumore termico per l'autooscillazione)
            const float fo = filt.tick (mixOut + white * 2.0e-5f, gCur, lerp (potPrev.damp, pot.damp), prm.fLowpass);

            // ------------------------------------------- VCA: accoppiamento C1, poi due coppie differenziali
            vcaDc += (fo - vcaDc) * vcaDcCoef;
            const float vin = fo - vcaDc;
            // modulazione d'ampiezza (con la quadra, solo positiva, il volume sale soltanto)
            const float modGain = std::max (0.0f, 1.0f + lerp (potPrev.aMod, pot.aMod) * m);
            const float st1 = 10.5f * fastTanh (vin / 10.5f) * modGain;
            const float st2 = 4.0f * fastTanh (st1 / 4.0f);
            gainSm += (ea - gainSm) * smoothCoef;
            const float vca = st2 * gainSm;

            // ------------------------------------------- uscite
            float y;
            if (prm.lineOut) y = vca * lerp (potPrev.vol, pot.vol);
            else
            {
                float h = vca * lerp (potPrev.ph, pot.ph);
                const float xo = 0.004f; // piccola zona morta del push-pull KT815/KT814 senza polarizzazione
                h = h > xo ? h - xo * 0.5f : (h < -xo ? h + xo * 0.5f : h * 0.5f);
                y = fastTanh (h * 1.2f) / 1.2f;
            }
            const float ay = std::fabs (y);
            if (ay > 0.7f) y = std::copysign (0.7f + 0.3f * fastTanh ((ay - 0.7f) / 0.3f), y);
            powerLvl += (powerTarget - powerLvl) * powerCoef;
            out[i] = y * powerLvl;
        }
    }

private:
    // valori dei potenziometri già convertiti in grandezze elettriche, raccordati nel tempo
    struct Pots
    {
        float tuneSemis = 0, detuneSemis = 0, vib1 = 0, vib2 = 0, fm = 0;
        float mG1 = 0, mG2 = 0, mNz = 0, mExt = 0;
        float cutI = 0, envI = 0, modI = 0, pedI = 0, damp = 1, aMod = 0, vol = 0, ph = 0;
        // ritorna true se qualche valore si sta ancora muovendo
        bool approach (const Pots& t, float k)
        {
            bool moving = false;
            auto a = [k, &moving] (float& v, float target)
            {
                const float d = target - v;
                if (std::fabs (d) > 1.0e-6f) { v += d * k; moving = true; }
                else v = target;
            };
            a (tuneSemis, t.tuneSemis); a (detuneSemis, t.detuneSemis); a (vib1, t.vib1); a (vib2, t.vib2);
            a (fm, t.fm); a (mG1, t.mG1); a (mG2, t.mG2); a (mNz, t.mNz); a (mExt, t.mExt);
            a (cutI, t.cutI); a (envI, t.envI); a (modI, t.modI); a (pedI, t.pedI); a (damp, t.damp);
            a (aMod, t.aMod); a (vol, t.vol); a (ph, t.ph);
            return moving;
        }
    };

    void idleReset()
    {
        filt.reset(); gainSm = 0.0f; vcaDc = 0.0f; shelfS = 0.0f; ovS = 0.0f;
        ctrlCount = 0; fresh = true; snapPots = true;
    }

    EngineParams prm;
    double sr = 96000.0;
    Generator g1, g2;
    Modulator lfo;
    NoiseSource noise;
    Tolerances tol;
    PolivoksFilter filt;
    Envelope envF, envA;
    KeyAssigner keys;
    Rng rng { 0xBADC0DEu };
    Pots pot, potPrev;
    bool sustainPedal = false, lastGate = false;
    float dc = 0.0f, dcCoef = 0.0f, vcaDc = 0.0f, vcaDcCoef = 0.0f, potCoef = 0.1f, lastGlide = -1.0f;
    double drift1 = 0.0, drift2 = 0.0, driftCoef = 0.0, glideCoef = 1.0;
    double glideLevel1 = 53.0, f1Base = 440.0, f2Base = 440.0;
    float shelfS = 0.0f, shelfCoef = 0.1f, ovS = 0.0f, ovCoef = 0.01f;
    float gainSm = 0.0f, smoothCoef = 0.01f, powerLvl = 1.0f, powerCoef = 0.001f;
    float gCur = 0.0f, gNext = 0.0f, gStep = 0.0f;
    int ctrlCount = 0;
    bool fresh = true, snapPots = true, potsMoving = false;
};

} // namespace vks
