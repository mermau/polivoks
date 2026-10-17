#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "PolivoksDSP.h"
#include <array>
#include <atomic>

class PolivoksAudioProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    PolivoksAudioProcessor();
    ~PolivoksAudioProcessor() override { stopTimer(); }

    const juce::String getName() const override { return "Polivoks Study"; }
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Panel"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();

    void beginMidiLearn(int param) noexcept;
    void cancelMidiLearn() noexcept { learning.store(-1); }
    int  getLearningParam() const noexcept { return learning.load(); }
    int  getCcForParam(int param) const noexcept;
    void clearCc(int param) noexcept;
    void clearAllCc() noexcept;
    void transferMidiChangesToParameters(); // GUI MESSAGE THREAD ONLY
    void panic() noexcept { panicRequested.store(true); }
    void initPanel(); // GUI MESSAGE THREAD ONLY

private:
    pv::Engine engine;
    std::array<juce::RangedAudioParameter*, pv::Count> parameterRefs{};
    std::array<std::atomic<int>, pv::Count> mapping;
    std::array<std::atomic<float>, pv::Count> pendingCc;
    std::atomic<int> learning {-1};
    std::atomic<bool> panicRequested {false};

    void handleMidi(const juce::MidiMessage& message) noexcept;
    void timerCallback() override { transferMidiChangesToParameters(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolivoksAudioProcessor)
};
