#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/PolivoksEngine.h"
#include "Params.h"

class VksProcessor : public juce::AudioProcessor
{
public:
    VksProcessor();
    ~VksProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // --- MIDI Learn -----------------------------------------------------------
    // indice del parametro (in getParameters()) assegnato a ogni CC, -1 = nessuno
    void startLearn (int paramIndex) { learnIndex.store (paramIndex); }
    void cancelLearn() { learnIndex.store (-1); }
    int  learningIndex() const { return learnIndex.load(); }
    int  ccForParam (int paramIndex) const;
    void clearMapping (int paramIndex);
    void resetDefaultMapping();
    int  paramIndexOf (const juce::String& id) const;

    float lfoLed() const { return ledLevel.load(); }

private:
    void readParams();
    void handleMidi (const juce::MidiMessage&);

    vks::PolivoksEngine engine;
    vks::EngineParams ep;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> mono;
    std::vector<float> extHi;
    int osFactor = 4;

    std::array<std::atomic<int>, 128> ccMap;
    std::atomic<int> learnIndex { -1 };
    std::atomic<float> ledLevel { 0.0f };

    std::unordered_map<std::string, std::atomic<float>*> raw;
    float rawv (const char* id) const { return raw.at (id)->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VksProcessor)
};
