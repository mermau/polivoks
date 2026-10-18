#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vks;

VksProcessor::VksProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Ingresso esterno (ВХОД)", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Uscita", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOLKS82", createLayout())
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            raw[rp->paramID.toStdString()] = apvts.getRawParameterValue (rp->paramID);
    resetDefaultMapping();
}

int VksProcessor::paramIndexOf (const juce::String& id) const
{
    const auto& ps = getParameters();
    for (int i = 0; i < ps.size(); ++i)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ps[i]))
            if (rp->paramID == id) return i;
    return -1;
}

void VksProcessor::resetDefaultMapping()
{
    for (auto& c : ccMap) c.store (-1);
    // Assegnazioni predefinite (tutto il resto con MIDI Learn: clic destro su un comando)
    ccMap[74].store (paramIndexOf (pid::fCut));    // brightness -> ЧАСТОТА СРЕЗА
    ccMap[71].store (paramIndexOf (pid::fRes));    // resonance  -> РЕЗОНАНС
    ccMap[11].store (paramIndexOf (pid::pedal));   // expression -> pedale del filtro
    ccMap[7].store  (paramIndexOf (pid::volume));  // volume     -> ГРОМКОСТЬ
    ccMap[5].store  (paramIndexOf (pid::glide));   // portamento -> ГЛИССАНДО
}

int VksProcessor::ccForParam (int paramIndex) const
{
    for (int cc = 0; cc < 128; ++cc) if (ccMap[(size_t) cc].load() == paramIndex) return cc;
    return -1;
}

void VksProcessor::clearMapping (int paramIndex)
{
    for (auto& c : ccMap) if (c.load() == paramIndex) c.store (-1);
}

bool VksProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void VksProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // l'emulazione lavora sempre a circa 176–192 kHz: alias e saturazioni restano puliti
    osFactor = sampleRate <= 50000.0 ? 4 : (sampleRate <= 100000.0 ? 2 : 1);
    const int stages = osFactor == 4 ? 2 : (osFactor == 2 ? 1 : 0);
    if (stages > 0)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, (size_t) stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) samplesPerBlock);
        setLatencySamples ((int) std::round (oversampler->getLatencyInSamples()));
    }
    else
    {
        oversampler.reset();
        setLatencySamples (0);
    }
    mono.setSize (1, samplesPerBlock);
    extHi.assign ((size_t) (samplesPerBlock * osFactor), 0.0f);
    engine.prepare (sampleRate * osFactor);
    readParams();
    engine.setParams (ep);
    keyboardState.reset();
}

void VksProcessor::readParams()
{
    ep.lfoRate = rawv (pid::lfoRate);
    ep.lfoMode = (int) rawv (pid::lfoMode);
    ep.phones  = rawv (pid::phones);
    ep.tune    = rawv (pid::tune);
    ep.glide   = rawv (pid::glide);
    ep.duo     = rawv (pid::duo) > 0.5f;
    ep.volume  = rawv (pid::volume);
    ep.lineOut = rawv (pid::lineOut) > 0.5f;
    ep.g1Range = (int) rawv (pid::g1Range);
    ep.g1Shape = (int) rawv (pid::g1Shape);
    ep.g1Mod   = rawv (pid::g1Mod);
    ep.g1Fm    = rawv (pid::g1Fm);
    ep.g2Range = (int) rawv (pid::g2Range);
    ep.g2Shape = (int) rawv (pid::g2Shape);
    ep.g2Mod   = rawv (pid::g2Mod);
    ep.g2Detune= rawv (pid::g2Detune);
    ep.mixG1   = rawv (pid::mixG1);
    ep.mixG2   = rawv (pid::mixG2);
    ep.mixNoise= rawv (pid::mixNoise);
    ep.mixExt  = rawv (pid::mixExt);
    ep.fAtt = rawv (pid::fAtt); ep.fDec = rawv (pid::fDec); ep.fSus = rawv (pid::fSus); ep.fRel = rawv (pid::fRel);
    ep.fLoop = rawv (pid::fLoop) > 0.5f;
    ep.fCut = rawv (pid::fCut); ep.fRes = rawv (pid::fRes);
    ep.fLowpass = rawv (pid::fLp) > 0.5f;
    ep.fMod = rawv (pid::fMod); ep.fEnv = rawv (pid::fEnv);
    ep.aAtt = rawv (pid::aAtt); ep.aDec = rawv (pid::aDec); ep.aSus = rawv (pid::aSus); ep.aRel = rawv (pid::aRel);
    ep.aLoop = rawv (pid::aLoop) > 0.5f;
    ep.aMod = rawv (pid::aMod);
    ep.aOn  = rawv (pid::aOn) > 0.5f;
    ep.pedal = rawv (pid::pedal);
    ep.power = rawv (pid::power) > 0.5f;
    ep.analog = rawv (pid::analog);
}

void VksProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())            engine.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())      engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel())
    {
        const float b = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        ep.bendSemis = b * rawv (pid::bendRange);
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        const int learn = learnIndex.load();
        if (learn >= 0 && cc != 64)
        {
            clearMapping (learn);
            ccMap[(size_t) cc].store (learn);
            learnIndex.store (-1);
        }
        const int idx = ccMap[(size_t) cc].load();
        if (idx >= 0)
        {
            if (auto* p = getParameters()[idx])
                p->setValueNotifyingHost ((float) val / 127.0f);
        }
        else if (cc == 64)
            engine.setSustainPedal (val >= 64);
    }
}

void VksProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    // tasti premuti sull'interfaccia + note in arrivo mostrate sulla tastiera
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (mono.getNumSamples() < n) mono.setSize (1, n, false, false, true);
    if ((int) extHi.size() < n * osFactor) extHi.resize ((size_t) (n * osFactor));

    // ingresso esterno (ВХОД) in mono
    mono.clear();
    for (int ch = 0; ch < numIn; ++ch)
        mono.addFrom (0, 0, buffer, ch, 0, n, 1.0f / (float) numIn);

    juce::dsp::AudioBlock<float> monoBlock (mono.getArrayOfWritePointers(), 1, (size_t) n);
    float* hi = nullptr;
    juce::dsp::AudioBlock<float> hiBlock;
    if (oversampler)
    {
        hiBlock = oversampler->processSamplesUp (monoBlock);
        hi = hiBlock.getChannelPointer (0);
    }
    else hi = mono.getWritePointer (0);
    std::copy (hi, hi + n * osFactor, extHi.begin());
    const bool useExt = numIn > 0 && rawv (pid::mixExt) > 0.0f;

    readParams();
    engine.setParams (ep);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            engine.process (hi + pos * osFactor, useExt ? extHi.data() + pos * osFactor : nullptr, (at - pos) * osFactor);
            pos = at;
        }
        handleMidi (meta.getMessage());
        readParams();          // un CC appreso può aver cambiato un parametro
        engine.setParams (ep);
    }
    if (pos < n)
        engine.process (hi + pos * osFactor, useExt ? extHi.data() + pos * osFactor : nullptr, (n - pos) * osFactor);

    if (oversampler) oversampler->processSamplesDown (monoBlock);

    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, mono, 0, 0, n);

    ledLevel.store (engine.lfoLed());
}

void VksProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto map = state.getOrCreateChildWithName ("MIDIMAP", nullptr);
    map.removeAllProperties (nullptr);
    for (int cc = 0; cc < 128; ++cc)
    {
        const int idx = ccMap[(size_t) cc].load();
        if (idx >= 0)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (getParameters()[idx]))
                map.setProperty ("cc" + juce::String (cc), rp->paramID, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VksProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid()) return;
        auto map = state.getChildWithName ("MIDIMAP");
        if (map.isValid())
        {
            for (auto& c : ccMap) c.store (-1);
            for (int i = 0; i < map.getNumProperties(); ++i)
            {
                const auto name = map.getPropertyName (i).toString();
                const int cc = name.substring (2).getIntValue();
                if (cc >= 0 && cc < 128)
                    ccMap[(size_t) cc].store (paramIndexOf (map.getProperty (name).toString()));
            }
            state.removeChild (map, nullptr);
        }
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* VksProcessor::createEditor() { return new VksEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VksProcessor(); }
