#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <vector>

class PolivoksAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer {
public:
    explicit PolivoksAudioProcessorEditor (PolivoksAudioProcessor&);
    ~PolivoksAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showParameter(int index);
private:
    class PolivoksLook;
    class PanelKnob;
    class PanelSwitch;
    class KeyboardOverlay;
    PolivoksAudioProcessor& processor;
    juce::Image panelImage;
    std::unique_ptr<PolivoksLook> theme;
    std::vector<std::unique_ptr<PanelKnob>> knobs;
    std::vector<std::unique_ptr<PanelSwitch>> switches;
    std::unique_ptr<KeyboardOverlay> keyboard;
    juce::Label status;
    juce::TextButton initialise {"INIT"};
    juce::TextButton allNotesOff {"PANIC"};
    juce::TextButton clearMidi {"CLEAR MIDI CC"};
    int highlighted=-1;
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PolivoksAudioProcessorEditor)
};
