#pragma once
#include "PluginProcessor.h"

namespace vks::ui
{
enum class Scale { ZeroSix, MinMax, Bipolar };

struct Fonts
{
    static Fonts& get();
    juce::Font label (float h) const;   // PT Sans Narrow Bold (etichette del pannello)
    juce::Font bold (float h) const;    // PT Sans Bold
    juce::Font regular (float h) const;
    juce::Font logo() const;            // Russo One: lettere larghe e squadrate come la targa originale
    juce::Typeface::Ptr logoFace, narrowBold, sansBold, sansRegular;
private:
    Fonts();
};

class LearnHost
{
public:
    virtual ~LearnHost() = default;
    virtual void showControlMenu (juce::Component& c, const juce::String& paramId) = 0;
    virtual bool isLearning (const juce::String& paramId) const = 0;
};

//==============================================================================
class Knob : public juce::Slider
{
public:
    Knob (LearnHost& h, juce::String id, bool selector, int numPositions = 0)
        : host (h), paramId (std::move (id)), isSelector (selector), positions (numPositions)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity (selector ? 90 : 220);
        setVelocityBasedMode (false);
        setDoubleClickReturnValue (false, 0.0);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { host.showControlMenu (*this, paramId); return; }
        juce::Slider::mouseDown (e);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (isSelector)
        {
            const double dir = (w.deltaY + w.deltaX) > 0 ? 1.0 : -1.0;
            setValue (juce::jlimit (getMinimum(), getMaximum(), getValue() + dir), juce::sendNotificationSync);
            return;
        }
        juce::Slider::mouseWheelMove (e, w);
    }
    void paint (juce::Graphics&) override;

    LearnHost& host;
    juce::String paramId;
    bool isSelector;
    int positions;
    float startDeg = -135.0f, endDeg = 135.0f;
};

//==============================================================================
class PushSwitch : public juce::Button
{
public:
    PushSwitch (LearnHost& h, juce::String id, bool red = false)
        : juce::Button (id), host (h), paramId (std::move (id)), isRed (red)
    {
        setClickingTogglesState (true);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { host.showControlMenu (*this, paramId); return; }
        juce::Button::mouseDown (e);
    }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    LearnHost& host;
    juce::String paramId;
    bool isRed;
};

//==============================================================================
class Keys : public juce::MidiKeyboardComponent
{
public:
    explicit Keys (juce::MidiKeyboardState& s);
    void drawWhiteNote (int, juce::Graphics&, juce::Rectangle<float>, bool, bool, juce::Colour, juce::Colour) override;
    void drawBlackNote (int, juce::Graphics&, juce::Rectangle<float>, bool, bool, juce::Colour) override;
    void paint (juce::Graphics& g) override;
};

//==============================================================================
// Il pannello è disegnato in "unità foto" (900 x 530) e scalato con una trasformazione.
class Panel : public juce::Component, public LearnHost, private juce::Timer
{
public:
    static constexpr int W = 900, H = 530;
    explicit Panel (VksProcessor&);
    ~Panel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void showControlMenu (juce::Component&, const juce::String& paramId) override;
    bool isLearning (const juce::String& paramId) const override;

private:
    struct KnobSpec { const char* id; float x, y; Scale scale; const char* label; const char* tip; };
    struct SelSpec  { const char* id; float x, y; int kind; const char* label; const char* tip; }; // kind 0 range, 1 shape, 2 lfo
    struct SwSpec   { const char* id; float x, y; const char* top; const char* bottom; const char* tip; bool red; };

    void timerCallback() override;
    void addKnob (const KnobSpec&);
    void addSelector (const SelSpec&);
    void addSwitch (const SwSpec&);
    void showSettingsMenu();
    static juce::Point<float> P (float x, float y) { return { x - 150.0f, y - 20.0f }; }

    VksProcessor& proc;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<PushSwitch> switches;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAtt;
    std::vector<KnobSpec> knobSpecs;
    std::vector<SelSpec> selSpecs;
    std::vector<SwSpec> swSpecs;
    Keys keys;
    juce::Rectangle<float> logoArea;
    float led = 0.0f;
    bool powerOn = true;
    juce::String learnId;
};

} // namespace vks::ui

//==============================================================================
class VksEditor : public juce::AudioProcessorEditor
{
public:
    explicit VksEditor (VksProcessor&);
    ~VksEditor() override = default;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0d0d0d)); }

private:
    VksProcessor& proc;
    vks::ui::Panel panel;
    juce::TooltipWindow tooltips { this, 600 };
};
