#include "../Source/PolivoksDSP.h"
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>

// Always-on check: plain assert() is compiled out under NDEBUG (Release / CI builds).
#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "FAIL: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; std::exit(1); } } while (0)

int main() {
    pv::Engine synth;
    synth.prepare (48000.0);
    for (int n=0; n<5000; ++n) CHECK(std::abs(synth.render()) < 1e-6f);
    synth.noteOn(48,1.0f);
    float peak=0; double rms=0;
    for (int n=0; n<24000; ++n) {
        float v=synth.render();
        CHECK(std::isfinite(v));
        CHECK(std::abs(v)<=1.0001f);
        peak = std::max(peak,std::abs(v)); rms+=v*v;
    }
    CHECK(peak > .01f); CHECK(rms > .1);
    synth.noteOn(60,0.8f);
    synth.set(pv::DuoMode,1.0f);
    synth.set(pv::FMode,1.0f);
    for (int n=0;n<12000;++n) CHECK(std::isfinite(synth.render()));
    synth.noteOff(48); synth.noteOff(60);
    float tail=0;
    for(int n=0;n<48000*3;++n) tail=std::abs(synth.render());
    CHECK(tail < .001f);
    for(int mode=0; mode<2; ++mode) for(int q=0;q<=10;++q) {
        synth.reset();
        synth.set(pv::FMode,static_cast<float>(mode));
        synth.set(pv::FResonance,static_cast<float>(q)/10.0f);
        synth.noteOn(72,1.0f);
        for(int n=0;n<8192;++n) {
            auto v=synth.render();
            CHECK(std::isfinite(v)); CHECK(std::abs(v)<=1.0001f);
        }
    }
    std::cout << "PASS: idle silence, envelope, duophony, LP/BP, resonance sweep, no NaN/clipping\n";
}
