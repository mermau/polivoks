// Rende una nota con parametri passati come chiave=valore (strumento di taratura).
// uso: render_patch <out.wav> <nota> <durata_s> <gate_s> [chiave=valore ...]
#include "../src/dsp/PolivoksEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using namespace vks;

int main (int argc, char** argv)
{
    if (argc < 5) return 1;
    const int sr = 44100, os = 2;
    const int note = std::atoi (argv[2]);
    const double dur = std::atof (argv[3]), gate = std::atof (argv[4]);
    EngineParams p; p.analog = 0.0f;
    for (int i = 5; i < argc; ++i)
    {
        std::string a (argv[i]); auto eq = a.find ('='); if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq); const float v = (float) std::atof (a.c_str() + eq + 1);
        #define F(name) if (k == #name) p.name = v;
        #define I(name) if (k == #name) p.name = (int) v;
        #define B(name) if (k == #name) p.name = v > 0.5f;
        F(lfoRate) I(lfoMode) F(glide) B(duo) F(volume) I(g1Range) I(g1Shape) F(g1Mod) F(g1Fm) I(g2Range) I(g2Shape)
        F(g2Mod) F(g2Detune) F(mixG1) F(mixG2) F(mixNoise) F(fAtt) F(fDec) F(fSus) F(fRel) B(fLoop) F(fCut) F(fRes)
        B(fLowpass) F(fMod) F(fEnv) F(aAtt) F(aDec) F(aSus) F(aRel) B(aLoop) F(aMod) F(tune)
    }
    PolivoksEngine e; e.prepare ((double) sr * os); e.setParams (p); e.noteOn (note);
    const int total = (int) (dur * sr * os), block = 32 * os, gateAt = (int) (gate * sr * os);
    std::vector<float> hi ((size_t) total);
    bool off = false;
    for (int pos = 0; pos < total; pos += block)
    {
        if (! off && pos >= gateAt) { e.noteOff (note); off = true; }
        e.process (hi.data() + pos, nullptr, std::min (block, total - pos));
    }
    std::vector<float> out ((size_t) (total / os));
    for (size_t i = 1; i < out.size(); ++i) out[i] = 0.25f * hi[2 * i - 1] + 0.5f * hi[2 * i] + 0.25f * hi[2 * i + 1 < hi.size() ? 2 * i + 1 : 2 * i];
    FILE* f = std::fopen (argv[1], "wb");
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t bytes = (uint32_t) out.size() * 4;
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (3); w16 (1); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (bytes); std::fwrite (out.data(), 4, out.size(), f); std::fclose (f);
    return 0;
}
