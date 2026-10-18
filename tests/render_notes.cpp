// Rende singole note come le registrazioni di riferimento (filtro aperto, risonanza al minimo),
// per il confronto con i campioni dell'originale.
#include "../src/dsp/PolivoksEngine.h"
#include <cstdio>
#include <string>
using namespace vks;

static void writeWav (const std::string& path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t bytes = (uint32_t) x.size() * 4;
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (3); w16 (1); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (bytes); std::fwrite (x.data(), 4, x.size(), f); std::fclose (f);
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const int sr = 44100, os = 2;
    struct J { int shape; const char* name; std::vector<int> notes; };
    const J jobs[] = { { 1, "Sawtooth", { 48, 72, 96 } }, { 2, "Square", { 48, 72, 96 } }, { 3, "Pulse", { 24, 36, 48, 72, 96, 108 } } };
    for (auto& j : jobs)
        for (int n : j.notes)
        {
            EngineParams p; p.g1Shape = j.shape; p.mixG1 = 0.8f; p.fCut = 1.0f; p.fRes = 0.0f; p.fEnv = 0.0f;
            p.aAtt = 0.0f; p.aSus = 1.0f; p.aRel = 0.2f; p.analog = 0.0f; p.volume = 0.8f;
            // la nota MIDI suona al piede 8' (Do3 MIDI 48 = 130.8 Hz)
            PolivoksEngine e; e.prepare ((double) sr * os); e.setParams (p); e.noteOn (n);
            const int total = (int) (3.5 * sr * os), block = 64 * os;
            std::vector<float> hi ((size_t) total);
            for (int pos = 0; pos < total; pos += block)
            {
                if (pos >= (int) (3.25 * sr * os) && pos < (int) (3.25 * sr * os) + block) e.noteOff (n);
                e.process (hi.data() + pos, nullptr, std::min (block, total - pos));
            }
            // decimazione semplice con FIR a finestra (solo per l'analisi)
            std::vector<float> out ((size_t) (total / os));
            const int taps = 63; std::vector<float> h (taps);
            for (int k = 0; k < taps; ++k) { const double m = k - (taps - 1) / 2.0; const double sinc = m == 0 ? 0.5 : std::sin (kPi * 0.5 * m) / (kPi * m);
                h[(size_t) k] = (float) (sinc * (0.42 - 0.5 * std::cos (2 * kPi * k / (taps - 1)) + 0.08 * std::cos (4 * kPi * k / (taps - 1)))); }
            for (size_t i = 0; i < out.size(); ++i) { double acc = 0; for (int k = 0; k < taps; ++k) { long idx = (long) (i * os) - k; if (idx >= 0) acc += h[(size_t) k] * hi[(size_t) idx]; } out[i] = (float) acc; }
            writeWav (dir + "/" + std::to_string (n) + "_" + j.name + "_SYNTHOVOKS.wav", out, sr);
        }
    return 0;
}
