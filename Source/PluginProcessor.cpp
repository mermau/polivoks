#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PolivoksAudioProcessor::makeParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& def : pv::definitions) {
        const float step = def.steps > 1 ? 1.0f / static_cast<float>(def.steps - 1) : 0.0f;
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID {def.id, 1},
            def.label,
            juce::NormalisableRange<float> {0.0f, 1.0f, step},
            def.initial));
    }
    return layout;
}

PolivoksAudioProcessor::PolivoksAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("External audio", juce::AudioChannelSet::stereo(), false)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "POLIVOKS_STUDY", makeParameterLayout()) {
    for (int i=0; i<pv::Count; ++i) {
        parameterRefs[static_cast<size_t>(i)] = state.getParameter (pv::definitions[static_cast<size_t>(i)].id);
        mapping[static_cast<size_t>(i)].store(-1);
        pendingCc[static_cast<size_t>(i)].store(-1.0f);
    }
    // Keep the host parameters synchronised even if the editor is closed.
    // Timer callback runs on the message thread, never in processBlock.
    startTimerHz(25);
}

void PolivoksAudioProcessor::prepareToPlay (double rate, int) {
    engine.prepare(rate);
}

bool PolivoksAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto input = layouts.getMainInputChannelSet();
    if (!input.isDisabled() && input != juce::AudioChannelSet::mono()
        && input != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void PolivoksAudioProcessor::handleMidi(const juce::MidiMessage& m) noexcept {
    if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(),m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
    else if (m.isPitchWheel()) engine.setPitchBend ((m.getPitchWheelValue()-8192)/8192.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isController()) {
        const int cc = m.getControllerNumber();
        const int value = m.getControllerValue();
        const float normalised = value / 127.0f;
        if (cc == 1) engine.setModWheel(normalised);
        if (cc == 64) engine.setSustain(value >= 64);
        if (cc == 120 || cc == 123) engine.allNotesOff();
        const int learn = learning.exchange(-1);
        if (learn >= 0 && learn < pv::Count) {
            mapping[static_cast<size_t>(learn)].store(cc);
        }
        for (int i=0; i<pv::Count; ++i) {
            if (mapping[static_cast<size_t>(i)].load() == cc) {
                // Realtime playback updates immediately. Host/UI notification is
                // deferred to the message thread to avoid host calls from audio.
                engine.set(i, normalised);
                pendingCc[static_cast<size_t>(i)].store(normalised);
            }
        }
    }
    else if (m.isChannelPressure()) engine.setModWheel (m.getChannelPressureValue()/127.0f);
}

void PolivoksAudioProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = audio.getNumSamples();
    const int outCh = getTotalNumOutputChannels();
    const int inCh = getTotalNumInputChannels();
    if (panicRequested.exchange(false)) engine.allNotesOff();
    for (int p=0; p<pv::Count; ++p)
        engine.set(p, pendingCc[static_cast<size_t>(p)].load() >= 0.0f
                      ? pendingCc[static_cast<size_t>(p)].load()
                      : parameterRefs[static_cast<size_t>(p)]->getValue());

    keyboard.processNextMidiBuffer(midi,0,n,true);
    int currentSample=0;
    auto writeRange = [this, &audio, outCh, inCh] (int from, int to) {
        for(int s=from; s<to; ++s) {
            float external=0.0f;
            if(inCh >= 1) external = audio.getSample(0,s);
            if(inCh >= 2) external = 0.5f * (external + audio.getSample(1,s));
            float val = engine.render(external);
            for(int channel=0; channel<outCh; ++channel)
                audio.setSample(channel,s,val);
        }
    };
    for (const auto event : midi) {
        const int pos = juce::jlimit (0,n,event.samplePosition);
        if(pos > currentSample) writeRange(currentSample,pos);
        handleMidi(event.getMessage());
        currentSample=pos;
    }
    writeRange(currentSample,n);
    midi.clear(); // Synth does not forward incoming MIDI events.
}

juce::AudioProcessorEditor* PolivoksAudioProcessor::createEditor() {
    return new PolivoksAudioProcessorEditor (*this);
}

void PolivoksAudioProcessor::beginMidiLearn (int p) noexcept {
    if (p >= 0 && p < pv::Count) learning.store(p);
}
int PolivoksAudioProcessor::getCcForParam(int p) const noexcept {
    return p >= 0 && p < pv::Count ? mapping[static_cast<size_t>(p)].load() : -1;
}
void PolivoksAudioProcessor::clearCc(int p) noexcept {
    if(p >= 0 && p < pv::Count) mapping[static_cast<size_t>(p)].store(-1);
}
void PolivoksAudioProcessor::clearAllCc() noexcept {
    for(auto& v : mapping) v.store(-1);
    learning.store(-1);
}
void PolivoksAudioProcessor::transferMidiChangesToParameters() {
    for(int i=0; i<pv::Count; ++i) {
        const float value = pendingCc[static_cast<size_t>(i)].exchange(-1.0f);
        if(value >= 0.0f) parameterRefs[static_cast<size_t>(i)]->setValueNotifyingHost(value);
    }
}
void PolivoksAudioProcessor::initPanel() {
    for(auto& pending : pendingCc) pending.store(-1.0f);
    for(int i=0; i<pv::Count; ++i) {
        auto* p = parameterRefs[static_cast<size_t>(i)];
        p->beginChangeGesture();
        p->setValueNotifyingHost(pv::definitions[static_cast<size_t>(i)].initial);
        p->endChangeGesture();
    }
    panic();
}
void PolivoksAudioProcessor::getStateInformation (juce::MemoryBlock& memory) {
    // CC assignments are embedded in the same preset state as synthesis parameters.
    auto tree = state.copyState();
    for(int i=0; i<pv::Count; ++i)
        tree.setProperty("cc_" + juce::String(i),mapping[static_cast<size_t>(i)].load(),nullptr);
    juce::MemoryOutputStream stream(memory,false);
    tree.writeToStream(stream);
}
void PolivoksAudioProcessor::setStateInformation (const void* data,int size) {
    auto tree = juce::ValueTree::readFromData(data,static_cast<size_t>(size));
    if(!tree.isValid() || !tree.hasType("POLIVOKS_STUDY")) return;
    for(int i=0; i<pv::Count; ++i) {
        const auto key = "cc_" + juce::String(i);
        int cc = static_cast<int>(tree.getProperty(key,-1));
        mapping[static_cast<size_t>(i)].store((cc>=0 && cc<=127) ? cc : -1);
        tree.removeProperty(key,nullptr);
    }
    for(auto& pending : pendingCc) pending.store(-1.0f);
    state.replaceState(tree);
    panic();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PolivoksAudioProcessor(); }
