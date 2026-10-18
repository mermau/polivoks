// Test del motore DSP (senza JUCE): rende alcune patch in WAV e verifica intonazione,
// livelli, stabilità, autooscillazione del filtro e costo di CPU.
#include "../src/dsp/PolivoksEngine.h"
#include <chrono>
#include <cstdio>
#include <string>

using namespace vks;

static void writeWav (const std::string& path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t bytes = (uint32_t) x.size() * 2;
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (1); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 2); w16 (2); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (float v : x) { int16_t s = (int16_t) std::lround (std::clamp (v, -1.0f, 1.0f) * 32767.0f); std::fwrite (&s, 2, 1, f); }
    std::fclose (f);
}

struct Ev { double t; int type; int note; }; // 1 on, 0 off

static std::vector<float> render (EngineParams p, std::vector<Ev> evs, double seconds, int os = 4, int sr = 48000)
{
    PolivoksEngine e; e.prepare ((double) sr * os); e.setParams (p);
    const int block = 64 * os;
    const int total = (int) (seconds * sr * os);
    std::vector<float> hi ((size_t) total);
    size_t ei = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        while (ei < evs.size() && evs[ei].t * sr * os <= pos)
        {
            if (evs[ei].type == 1) e.noteOn (evs[ei].note); else e.noteOff (evs[ei].note);
            ++ei;
        }
        e.process (hi.data() + pos, nullptr, std::min (block, total - pos));
    }
    std::vector<float> out ((size_t) (total / os));
    for (size_t i = 0; i < out.size(); ++i) { float s = 0; for (int k = 0; k < os; ++k) s += hi[i * (size_t) os + (size_t) k]; out[i] = s / (float) os; }
    return out;
}

static double estimateFreq (const std::vector<float>& x, int sr, double t0, double t1)
{
    // conteggio degli attraversamenti dello zero (discendenti) con interpolazione
    int a = (int) (t0 * sr), b = (int) (t1 * sr);
    double first = -1, last = -1; int count = 0;
    for (int i = a + 1; i < b; ++i)
        if (x[(size_t) i - 1] > 0 && x[(size_t) i] <= 0)
        {
            double fr = (double) x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]);
            double t = (i - 1) + fr;
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? count * (double) sr / (last - first) : 0.0;
}

static float peak (const std::vector<float>& x) { float p = 0; for (float v : x) p = std::max (p, std::fabs (v)); return p; }
static bool finite (const std::vector<float>& x) { for (float v : x) if (! std::isfinite (v)) return false; return true; }

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "OK  " : "FAIL", what); if (! ok) ++fails; };

    // 1) intonazione: A4 sul piede 8', filtro aperto, analogico spento
    {
        EngineParams p; p.analog = 0.0f; p.fCut = 1.0f; p.fEnv = 0.0f; p.fRes = 0.0f; p.g1Shape = 2; p.aSus = 1.0f;
        auto x = render (p, { { 0.0, 1, 69 } }, 1.0);
        double f = estimateFreq (x, 48000, 0.2, 0.9);
        std::printf ("      A4 8' = %.2f Hz\n", f);
        check (std::fabs (f - 440.0) < 1.0, "A4 = 440 Hz");
        p.g1Range = 0; auto y = render (p, { { 0.0, 1, 41 } }, 2.0); // F più basso a 32'
        double f0 = estimateFreq (y, 48000, 0.3, 1.9);
        std::printf ("      F tasto più basso a 32' = %.2f Hz (atteso 21.83)\n", f0);
        check (std::fabs (f0 - 21.83) < 0.3, "estensione bassa 21.8 Hz (fa subcontra)");
        p.g1Range = 4; auto z = render (p, { { 0.0, 1, 88 } }, 1.0); // E più alto a 2'
        double f1 = estimateFreq (z, 48000, 0.2, 0.9);
        std::printf ("      E tasto più alto a 2' = %.1f Hz (atteso 5274)\n", f1);
        check (std::fabs (f1 - 5274.0) < 15.0, "estensione alta 5274 Hz (mi della quinta ottava)");
        writeWav (dir + "/01_intonazione.wav", x, 48000);
    }
    // 2) basso classico: sega, filtro con inviluppo, risonanza media
    {
        EngineParams p; p.g1Range = 1; p.g1Shape = 1; p.fCut = 0.35f; p.fRes = 0.6f; p.fEnv = 0.55f; p.fDec = 0.35f;
        p.aSus = 0.7f; p.aDec = 0.4f;
        std::vector<Ev> ev; double t = 0;
        int seq[] = { 41, 41, 53, 41, 48, 41, 51, 46 };
        for (int k = 0; k < 16; ++k) { ev.push_back ({ t, 1, seq[k % 8] }); ev.push_back ({ t + 0.12, 0, seq[k % 8] }); t += 0.18; }
        auto x = render (p, ev, 3.5);
        check (finite (x), "basso: nessun NaN");
        std::printf ("      picco basso = %.3f\n", peak (x));
        check (peak (x) > 0.05 && peak (x) < 1.0, "basso: livello sensato");
        writeWav (dir + "/02_basso.wav", x, 48000);
    }
    // 3) autooscillazione del filtro (risonanza al massimo, nessuna sorgente)
    {
        EngineParams p; p.mixG1 = 0.0f; p.fRes = 1.0f; p.fCut = 0.6f; p.fEnv = 0.0f; p.aSus = 1.0f;
        auto x = render (p, { { 0.0, 1, 60 } }, 2.0);
        double f = estimateFreq (x, 48000, 0.8, 1.9);
        std::printf ("      autooscillazione: picco %.3f, f = %.0f Hz\n", peak (x), f);
        check (finite (x) && peak (x) > 0.05, "filtro autooscilla con РЕЗОНАНС al massimo");
        writeWav (dir + "/03_autooscillazione.wav", x, 48000);
    }
    // 4) passa-banda risonante + FM dal Generatore II
    {
        EngineParams p; p.fLowpass = false; p.fRes = 0.85f; p.fCut = 0.45f; p.fEnv = 0.4f; p.g1Fm = 0.55f; p.g2Range = 1;
        p.mixG2 = 0.0f; p.aSus = 0.9f; p.fLoop = true; p.fDec = 0.3f; p.fAtt = 0.25f;
        auto x = render (p, { { 0.0, 1, 57 }, { 3.0, 0, 57 } }, 3.6);
        check (finite (x), "PF + FM + inviluppo ripetuto: nessun NaN");
        writeWav (dir + "/04_pf_fm_ripetuto.wav", x, 48000);
    }
    // 5) duofonia con glissando
    {
        EngineParams p; p.duo = true; p.mixG2 = 0.8f; p.glide = 0.5f; p.fCut = 0.65f; p.fEnv = 0.2f; p.aSus = 0.9f;
        p.g1Shape = 2; p.g2Shape = 3;
        auto x = render (p, { { 0.0, 1, 48 }, { 0.5, 1, 64 }, { 1.0, 0, 48 }, { 1.0, 1, 55 }, { 2.0, 0, 55 }, { 2.0, 0, 64 } }, 2.8);
        check (finite (x), "duofonia + glissando: nessun NaN");
        writeWav (dir + "/05_duofonia_glissando.wav", x, 48000);
    }
    // 6) modulatore a gradini casuali sul filtro + rumore
    {
        EngineParams p; p.lfoMode = 3; p.lfoRate = 0.55f; p.fMod = 0.7f; p.fRes = 0.75f; p.mixNoise = 0.5f; p.aSus = 1.0f;
        auto x = render (p, { { 0.0, 1, 50 }, { 3.0, 0, 50 } }, 3.5);
        check (finite (x), "S&H + rumore: nessun NaN");
        writeWav (dir + "/06_gradini_casuali.wav", x, 48000);
    }
    // 7) caso peggiore: tutto al massimo
    {
        EngineParams p; p.mixG1 = p.mixG2 = p.mixNoise = 1.0f; p.fRes = 1.0f; p.fCut = 1.0f; p.fEnv = 1.0f; p.g1Fm = 1.0f;
        p.g1Mod = p.g2Mod = p.fMod = p.aMod = 1.0f; p.lfoRate = 1.0f; p.volume = 1.0f; p.aSus = 1.0f; p.g1Range = 4; p.g2Range = 0;
        auto x = render (p, { { 0.0, 1, 88 } }, 2.0);
        std::printf ("      caso peggiore picco = %.3f\n", peak (x));
        check (finite (x) && peak (x) < 1.5, "caso peggiore: stabile e limitato");
    }
    // 8) CPU
    {
        EngineParams p; p.mixG2 = 0.6f; p.mixNoise = 0.2f;
        PolivoksEngine e; e.prepare (192000.0); e.setParams (p); e.noteOn (60);
        std::vector<float> buf (256);
        auto t0 = std::chrono::steady_clock::now();
        const int blocks = 192000 * 10 / 256;
        for (int b = 0; b < blocks; ++b) e.process (buf.data(), nullptr, 256);
        double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("      10 s di audio a 4x (192 kHz) in %.3f s -> %.1f %% di un core\n", s, s * 10.0);
        check (s < 2.0, "CPU accettabile");
    }
    std::printf ("\n%d errori\n", fails);
    return fails == 0 ? 0 : 1;
}
