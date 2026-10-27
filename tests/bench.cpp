// Banco di prova realistico: host a 48 kHz, blocchi da 512, qualità normale (2x),
// due generatori, rumore, vibrato, FM e un flusso di CC (manopola girata: un evento ogni 32 campioni).
#include "../src/dsp/PolivoksEngine.h"
#include <chrono>
#include <cstdio>
using namespace vks;

static double run (bool fm, bool ccStream, int os)
{
    EngineParams p; p.mixG2 = 0.6f; p.mixNoise = 0.2f; p.g1Mod = 0.2f; p.fRes = 0.6f; p.fMod = 0.3f;
    if (fm) p.g1Fm = 0.4f;
    PolivoksEngine e; e.prepare (48000.0 * os); e.setParams (p); e.noteOn (48); e.noteOn (60);
    std::vector<float> buf (512 * (size_t) os);
    const int blocks = 48000 * 20 / 512;   // 20 s di audio
    auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        if (! ccStream) { e.process (buf.data(), nullptr, 512 * os); continue; }
        for (int pos = 0; pos < 512; pos += 32)
        {
            p.fCut = 0.3f + 0.4f * (float) ((b * 16 + pos / 32) % 127) / 127.0f;   // CC 74 che si muove
            e.setParams (p);
            e.process (buf.data() + pos * os, nullptr, 32 * os);
        }
    }
    const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return s / 20.0 * 100.0;   // % di un core
}

int main()
{
    for (int os : { 2, 4 })
        std::printf ("%dx  base %.2f %%   +FM %.2f %%   +CC ogni 32 campioni %.2f %%   FM+CC %.2f %%\n",
                     os, run (false, false, os), run (true, false, os), run (false, true, os), run (true, true, os));
}
