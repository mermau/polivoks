#include "../Source/PolivoksDSP.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

static void u16(std::ofstream& f,uint16_t n) { char v[2] {(char)(n&255),(char)(n>>8)}; f.write(v,2); }
static void u32(std::ofstream& f,uint32_t n) { char v[4] {(char)(n&255),(char)((n>>8)&255),(char)((n>>16)&255),(char)(n>>24)}; f.write(v,4); }
int main(int argc,char** argv) {
    if(argc != 2) { std::cerr<<"usage: render_demo output.wav\n"; return 2; }
    constexpr uint32_t rate=48000, seconds=7, frames=rate*seconds;
    std::ofstream f(argv[1],std::ios::binary);
    if(!f) return 3;
    f.write("RIFF",4);u32(f,36+frames*2);f.write("WAVEfmt ",8);
    u32(f,16);u16(f,1);u16(f,1);u32(f,rate);u32(f,rate*2);u16(f,2);u16(f,16);
    f.write("data",4);u32(f,frames*2);
    pv::Engine e;e.prepare(rate);
    e.set(pv::Osc1Wave,.25f);e.set(pv::Osc2Wave,.50f);
    e.set(pv::Osc2Detune,.52f);e.set(pv::FCutoff,.55f);
    e.set(pv::FResonance,.57f);
    for(uint32_t n=0;n<frames;++n) {
        if(n==rate/4) e.noteOn(48,1.0f);
        if(n==rate*2) e.noteOff(48);
        if(n==rate*2+rate/3) {
            e.set(pv::FMode,1); e.set(pv::FCutoff,.52f);e.set(pv::FResonance,.80f);
            e.noteOn(43,.9f);
        }
        if(n==rate*4) e.noteOff(43);
        if(n==rate*4+rate/3) {
            e.set(pv::DuoMode,1);e.set(pv::FMode,0);
            e.set(pv::FCutoff,.4f);e.set(pv::FEnv,.75f);e.set(pv::MixNoise,.1f);
            e.noteOn(48,1.0f);e.noteOn(55,.9f);
        }
        if(n==rate*6) { e.noteOff(48); e.noteOff(55); }
        const int sample=static_cast<int>(std::clamp(e.render(),-1.f,1.f)*32760.0f);
        u16(f,static_cast<uint16_t>(static_cast<int16_t>(sample)));
    }
    std::cout<<"Wrote WAV demo "<<argv[1]<<" (three patches; algorithmic render, not hardware reference)\n";
}
