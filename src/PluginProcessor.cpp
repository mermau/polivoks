#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vks;

VksProcessor::VksProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Ingresso esterno (ВХОД)", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Uscita", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SYNTHOVOKS", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        prm[(size_t) i] = apvts.getRawParameterValue (kParamIds[i]);
        jassert (prm[(size_t) i] != nullptr);
    }
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
    hostRate = sampleRate;
    for (size_t st = 1; st <= 2; ++st)
    {
        overs[st] = std::make_unique<juce::dsp::Oversampling<float>> (
            1, st, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        overs[st]->initProcessing ((size_t) samplesPerBlock);
    }
    selectFactor (factorFor (v (iQuality) > 0.5f));
    cancelPendingUpdate();
    pendingLatency.store (-1);
    setLatencySamples (oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0);
    mono.setSize (1, samplesPerBlock);
    extHi.assign ((size_t) (samplesPerBlock * 4), 0.0f);
    readParams();
    engine.setParams (ep);
    keyboardState.reset();
}

// Qualità normale: il motore gira a ~88–96 kHz; alta: ~176–192 kHz
int VksProcessor::factorFor (bool high) const
{
    if (hostRate <= 50000.0)  return high ? 4 : 2;
    if (hostRate <= 100000.0) return high ? 2 : 1;
    return 1;
}

void VksProcessor::selectFactor (int factor)
{
    osFactor = factor;
    oversampler = factor == 4 ? overs[2].get() : (factor == 2 ? overs[1].get() : nullptr);
    if (oversampler) oversampler->reset();
    engine.prepare (hostRate * factor);   // nessuna allocazione
}

void VksProcessor::handleAsyncUpdate()
{
    const int lat = pendingLatency.exchange (-1);
    if (lat >= 0) setLatencySamples (lat);
}

void VksProcessor::readParams()
{
    ep.lfoRate = v (iLfoRate);
    ep.lfoMode = (int) v (iLfoMode);
    ep.phones  = v (iPhones);
    ep.tune    = v (iTune);
    ep.glide   = v (iGlide);
    ep.duo     = v (iDuo) > 0.5f;
    ep.volume  = v (iVolume);
    ep.lineOut = v (iLineOut) > 0.5f;
    ep.g1Range = (int) v (iG1Range);
    ep.g1Shape = (int) v (iG1Shape);
    ep.g1Mod   = v (iG1Mod);
    ep.g1Fm    = v (iG1Fm);
    ep.g2Range = (int) v (iG2Range);
    ep.g2Shape = (int) v (iG2Shape);
    ep.g2Mod   = v (iG2Mod);
    ep.g2Detune= v (iG2Detune);
    ep.mixG1   = v (iMixG1);
    ep.mixG2   = v (iMixG2);
    ep.mixNoise= v (iMixNoise);
    ep.mixExt  = v (iMixExt);
    ep.fAtt = v (iFAtt); ep.fDec = v (iFDec); ep.fSus = v (iFSus); ep.fRel = v (iFRel);
    ep.fLoop = v (iFLoop) > 0.5f;
    ep.fCut = v (iFCut); ep.fRes = v (iFRes);
    ep.fLowpass = v (iFLp) > 0.5f;
    ep.fMod = v (iFMod); ep.fEnv = v (iFEnv);
    ep.aAtt = v (iAAtt); ep.aDec = v (iADec); ep.aSus = v (iASus); ep.aRel = v (iARel);
    ep.aLoop = v (iALoop) > 0.5f;
    ep.aMod = v (iAMod);
    ep.aOn  = v (iAOn) > 0.5f;
    ep.pedal = v (iPedal);
    ep.power = v (iPower) > 0.5f;
    ep.analog = v (iAnalog);
}

bool VksProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())            engine.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())      engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel())
    {
        const float b = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        ep.bendSemis = b * v (iBendRange);
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
            const auto& ps = getParameters();
            if (idx < ps.size())
            {
                ps.getUnchecked (idx)->setValueNotifyingHost ((float) val / 127.0f);
                return true;
            }
        }
        else if (cc == 64)
            engine.setSustainPedal (val >= 64);
    }
    return false;
}

void VksProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    // tasti premuti sull'interfaccia + note in arrivo mostrate sulla tastiera
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (const int want = factorFor (v (iQuality) > 0.5f); want != osFactor)
    {
        selectFactor (want);
        pendingLatency.store (oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0);
        triggerAsyncUpdate();
    }

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
    const bool useExt = numIn > 0 && v (iMixExt) > 0.0f;

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
        if (handleMidi (meta.getMessage()))   // un CC assegnato ha cambiato un parametro
            readParams();
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
