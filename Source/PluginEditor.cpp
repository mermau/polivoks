#include "PluginEditor.h"
#include <cmath>

namespace
{
    constexpr float referenceWidth  = 1000.0f;
    constexpr float referenceHeight = 626.0f;
    constexpr float panelTopY       = 18.0f;
    constexpr float panelHeight     = 304.0f;
    constexpr float keyboardY       = 334.0f;
    constexpr float keyboardH       = 180.0f;

    juce::Colour panelBlack()    { return juce::Colour (0xff0b1112); }
    juce::Colour panelDark()     { return juce::Colour (0xff12191b); }
    juce::Colour panelLine()     { return juce::Colour (0xffe2e8e3); }
    juce::Colour panelSoft()     { return juce::Colour (0xff8b9c95); }
    juce::Colour accent()        { return juce::Colour (0xffff9830); }
    juce::Colour cabinetBlack()  { return juce::Colour (0xff07090a); }
    juce::Colour cabinetEdge()   { return juce::Colour (0xff2c3438); }
    juce::Colour whiteKey()      { return juce::Colour (0xfff2ede1); }
    juce::Colour blackKey()      { return juce::Colour (0xff0d1416); }

    struct LayoutRect { float x, y, w, h; };

    const LayoutRect modSection   {  90.0f,  38.0f, 152.0f, 236.0f };
    const LayoutRect osc1Section  { 267.0f,  95.0f, 126.0f, 119.0f };
    const LayoutRect osc2Section  { 402.0f,  95.0f, 126.0f, 119.0f };
    const LayoutRect mixSection   { 267.0f, 214.0f, 261.0f,  64.0f };
    const LayoutRect filtSection  { 555.0f,  38.0f, 153.0f, 239.0f };
    const LayoutRect ampSection   { 736.0f,  38.0f, 112.0f, 239.0f };

    // Source strings below are UTF-8: juce::String(const char*) would read them as Latin-1.
    juce::String u8 (const char* text) { return juce::String::fromUTF8 (text); }

    juce::String shortParamName (int param)
    {
        switch (param)
        {
            case pv::ModRate:    return u8 ("ЧАСТОТА");
            case pv::ModShape:   return u8 ("РЕЖИМ");
            case pv::Tone:       return u8 ("ТОН");
            case pv::MasterTune: return u8 ("НАСТРОЙКА");
            case pv::GlideTime:  return u8 ("ГЛИССАНДО");
            case pv::DuoMode:    return u8 ("2-ГОЛ.");
            case pv::MasterGain: return u8 ("ГРОМКОСТЬ");
            case pv::OutputOn:   return u8 ("ВЫХОД");
            case pv::Osc1Range:  return u8 ("ДИАПАЗОН");
            case pv::Osc1Wave:   return u8 ("ФОРМА");
            case pv::Osc1Lfo:    return u8 ("МОДУЛЯЦИЯ");
            case pv::CrossMod:   return u8 ("ГЕНЕРАТОР 2");
            case pv::Osc2Range:  return u8 ("ДИАПАЗОН");
            case pv::Osc2Wave:   return u8 ("ФОРМА");
            case pv::Osc2Lfo:    return u8 ("МОДУЛЯЦИЯ");
            case pv::Osc2Detune: return u8 ("РАССТРОЙКА");
            case pv::MixOsc1:    return u8 ("ГЕНЕРАТОР I");
            case pv::MixOsc2:    return u8 ("ГЕНЕРАТОР II");
            case pv::MixNoise:   return u8 ("ШУМ");
            case pv::MixExternal:return u8 ("ВНЕШ.");
            case pv::FAttack:    return u8 ("АТАКА");
            case pv::FDecay:     return u8 ("ЗАТУХАНИЕ");
            case pv::FSustain:   return u8 ("УСТАНОВ.");
            case pv::FRelease:   return u8 ("ПОСЛЕЗВУЧ.");
            case pv::FCutoff:    return u8 ("ЧАСТОТА СРЕЗА");
            case pv::FResonance: return u8 ("РЕЗОНАНС");
            case pv::FLfo:       return u8 ("МОДУЛЯЦИЯ");
            case pv::FEnv:       return u8 ("ОГИБАЮЩАЯ");
            case pv::FLoop:      return u8 ("ФИ");
            case pv::FMode:      return u8 ("П/Л");
            case pv::AAttack:    return u8 ("АТАКА");
            case pv::ADecay:     return u8 ("ЗАТУХАНИЕ");
            case pv::ASustain:   return u8 ("УСТАНОВ.");
            case pv::ARelease:   return u8 ("ПОСЛЕЗВУЧ.");
            case pv::ALfo:       return u8 ("МОДУЛЯЦИЯ");
            case pv::ALoop:      return u8 ("М");
            case pv::AmpOn:      return u8 ("ВКЛ.");
            default:             return {};
        }
    }

    void showMidiMenu (juce::Component* target, PolivoksAudioProcessor& processor, int param)
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Learn MIDI CC for " + juce::String (pv::definitions[(size_t) param].label));
        const auto cc = processor.getCcForParam (param);
        menu.addItem (2, cc < 0 ? "No MIDI CC assigned" : ("Clear CC " + juce::String (cc)), cc >= 0);

        juce::Component::SafePointer<juce::Component> safe (target);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                            [safe, &processor, param] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1) processor.beginMidiLearn (param);
                                if (result == 2) processor.clearCc (param);
                            });
    }
}

class PolivoksAudioProcessorEditor::PolivoksLook final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        auto centre = bounds.getCentre();
        const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;

        juce::ColourGradient ring (juce::Colour (0xff3d4a4f), centre.x, centre.y - radius,
                                   juce::Colour (0xff111719), centre.x, centre.y + radius, false);
        g.setGradientFill (ring);
        g.fillEllipse (bounds);
        g.setColour (juce::Colour (0xffdce3dc));
        g.drawEllipse (bounds, 1.3f);

        auto inner = bounds.reduced (radius * 0.19f);
        g.setColour (juce::Colour (0xff05090a));
        g.fillEllipse (inner);
        g.setColour (juce::Colour (0xff7e9088).withAlpha (0.75f));
        g.drawEllipse (inner, 0.8f);

        for (int i = 0; i < 11; ++i)
        {
            const auto a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * (float) i / 10.0f;
            const float innerR = radius * 0.86f;
            const float outerR = radius * 1.02f;
            const float thickness = (i == 0 || i == 5 || i == 10) ? 1.15f : 0.75f;
            g.setColour (panelLine().withAlpha (i == 5 ? 0.9f : 0.48f));
            g.drawLine (centre.x + std::sin (a) * innerR,
                        centre.y - std::cos (a) * innerR,
                        centre.x + std::sin (a) * outerR,
                        centre.y - std::cos (a) * outerR,
                        thickness);
        }

        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        g.setColour (juce::Colour (0xfff6f2e7));
        g.drawLine (centre.x,
                    centre.y,
                    centre.x + std::sin (angle) * radius * 0.62f,
                    centre.y - std::cos (angle) * radius * 0.62f,
                    2.25f);
        g.fillEllipse (centre.x - 1.8f, centre.y - 1.8f, 3.6f, 3.6f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (4.0f);
        auto slot = bounds.reduced (5.0f, 3.0f);

        g.setColour (juce::Colour (0xff0d1315));
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (panelLine().withAlpha (0.90f));
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

        const float rockerHeight = slot.getHeight() * 0.56f;
        const float y = button.getToggleState() ? slot.getY() : (slot.getBottom() - rockerHeight);
        auto rocker = juce::Rectangle<float> (slot.getX(), y, slot.getWidth(), rockerHeight);

        juce::ColourGradient grad (juce::Colour (0xfff7f1e5), rocker.getCentreX(), rocker.getY(),
                                   juce::Colour (0xffc5c3b6), rocker.getCentreX(), rocker.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (rocker, 1.5f);
        g.setColour (juce::Colour (0xff777b74));
        g.drawLine (rocker.getX() + 3.0f, rocker.getBottom() - 2.2f,
                    rocker.getRight() - 3.0f, rocker.getBottom() - 2.2f, 0.9f);
    }
};

class PolivoksAudioProcessorEditor::PanelKnob final : public juce::Slider
{
public:
    PanelKnob (PolivoksAudioProcessorEditor& owner, int param)
        : editor (owner), index (param)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity (160);
        setScrollWheelEnabled (true);

        const auto& d = pv::definitions[(size_t) index];
        const float step = d.steps > 1 ? 1.0f / (float) (d.steps - 1) : 0.0f;
        setRange (0.0, 1.0, step);
        setRotaryParameters (-2.45f, 2.45f, true);
        setTooltip (juce::String (d.label) + ": " + d.detail + "; right-click for MIDI learn");

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            editor.processor.state, d.id, *this);
    }

    void mouseEnter (const juce::MouseEvent&) override { editor.showParameter (index); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showMidiMenu (this, editor.processor, index);
            return;
        }
        juce::Slider::mouseDown (e);
    }

private:
    PolivoksAudioProcessorEditor& editor;
    int index;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class PolivoksAudioProcessorEditor::PanelSwitch final : public juce::ToggleButton
{
public:
    PanelSwitch (PolivoksAudioProcessorEditor& owner, int param)
        : editor (owner), index (param)
    {
        setClickingTogglesState (true);
        const auto& d = pv::definitions[(size_t) param];
        setTooltip (juce::String (d.label) + ": " + d.detail + "; right-click for MIDI learn");

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            editor.processor.state, d.id, *this);
    }

    void mouseEnter (const juce::MouseEvent&) override { editor.showParameter (index); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showMidiMenu (this, editor.processor, index);
            return;
        }
        juce::ToggleButton::mouseDown (e);
    }

private:
    PolivoksAudioProcessorEditor& editor;
    int index;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class PolivoksAudioProcessorEditor::KeyboardOverlay final : public juce::Component
{
public:
    explicit KeyboardOverlay (juce::MidiKeyboardState& s) : state (s)
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (cabinetBlack());
        g.fillRoundedRectangle (area, 6.0f);

        const float whiteW = area.getWidth() / 28.0f;
        const float blackW = whiteW * 0.56f;
        const float blackH = area.getHeight() * 0.64f;
        const float depression = std::max (2.0f, area.getHeight() * 0.035f);

        for (int i = 0; i < 28; ++i)
        {
            const int midi = whiteMidiForIndex (i);
            const bool down = noteIsOn (midi);
            const float offsetY = down ? depression : 0.0f;
            juce::Rectangle<float> key (area.getX() + whiteW * (float) i + 0.45f,
                                        area.getY() + 2.0f + offsetY,
                                        whiteW - 1.3f,
                                        area.getHeight() - 4.0f - offsetY);

            juce::ColourGradient grad (down ? juce::Colour (0xffffe0a8) : juce::Colour (0xfffaf6eb),
                                       key.getCentreX(), key.getY(),
                                       down ? juce::Colour (0xffedb24d) : juce::Colour (0xffdcd7cb),
                                       key.getCentreX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 1.9f);
            g.setColour (down ? accent().darker (0.2f) : juce::Colour (0xff4a4a45));
            g.drawRoundedRectangle (key, 1.9f, 0.9f);

            if (down)
            {
                g.setColour (juce::Colours::white.withAlpha (0.22f));
                g.fillRoundedRectangle (key.withHeight (key.getHeight() * 0.16f), 1.9f);
            }
        }

        for (int i = 0; i < 28; ++i)
        {
            if (i % 7 == 2 || i % 7 == 6)
                continue;

            const int midi = whiteMidiForIndex (i) + 1;
            const bool down = noteIsOn (midi);
            const float offsetY = down ? depression * 0.82f : 0.0f;
            juce::Rectangle<float> key (area.getX() + whiteW * (float) (i + 1) - blackW * 0.5f,
                                        area.getY() + 2.0f + offsetY,
                                        blackW,
                                        blackH - offsetY);

            juce::ColourGradient grad (down ? juce::Colour (0xffffb04d) : juce::Colour (0xff232c30),
                                       key.getCentreX(), key.getY(),
                                       down ? juce::Colour (0xff8b3f0c) : juce::Colour (0xff070b0c),
                                       key.getCentreX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 1.9f);
            g.setColour (down ? juce::Colour (0xffffd8a8) : juce::Colour (0xff545f61));
            g.drawRoundedRectangle (key, 1.9f, 0.9f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { updateAt (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { updateAt (e.position); }
    void mouseUp (const juce::MouseEvent&) override { release(); }
    void mouseExit (const juce::MouseEvent& e) override
    {
        if (! e.mods.isAnyMouseButtonDown())
            release();
    }
    ~KeyboardOverlay() override { release(); }

private:
    inline static constexpr int whiteOffsets[7] { 0, 2, 4, 5, 7, 9, 11 };
    juce::MidiKeyboardState& state;
    int active = -1;

    static int whiteMidiForIndex (int i) noexcept
    {
        return 36 + (i / 7) * 12 + whiteOffsets[i % 7];
    }

    bool noteIsOn (int note) const
    {
        if (active == note)
            return true;

        for (int ch = 1; ch <= 16; ++ch)
            if (state.isNoteOn (ch, note))
                return true;
        return false;
    }

    int noteAt (juce::Point<float> p) const noexcept
    {
        if (p.x < 0 || p.x >= (float) getWidth() || p.y < 0 || p.y >= (float) getHeight())
            return -1;

        const float whiteW = (float) getWidth() / 28.0f;
        const float blackW = whiteW * 0.56f;
        const float blackH = (float) getHeight() * 0.64f;

        if (p.y < blackH)
        {
            for (int i = 0; i < 28; ++i)
            {
                if (i % 7 == 2 || i % 7 == 6)
                    continue;
                const float keyX = whiteW * (float) (i + 1) - blackW * 0.5f;
                if (p.x >= keyX && p.x < keyX + blackW)
                    return whiteMidiForIndex (i) + 1;
            }
        }

        const int i = juce::jlimit (0, 27, (int) (p.x / whiteW));
        return whiteMidiForIndex (i);
    }

    void updateAt (juce::Point<float> p)
    {
        const int note = noteAt (p);
        if (note == active)
            return;
        release();
        active = note;
        if (active >= 0)
            state.noteOn (1, active, 0.95f);
        repaint();
    }

    void release()
    {
        if (active >= 0)
        {
            state.noteOff (1, active, 0.0f);
            active = -1;
            repaint();
        }
    }
};

PolivoksAudioProcessorEditor::PolivoksAudioProcessorEditor (PolivoksAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), theme (std::make_unique<PolivoksLook>())
{
    setLookAndFeel (theme.get());

    for (int i = 0; i < pv::Count; ++i)
    {
        if (pv::definitions[(size_t) i].toggle)
        {
            auto widget = std::make_unique<PanelSwitch> (*this, i);
            addAndMakeVisible (*widget);
            switches.push_back (std::move (widget));
        }
        else
        {
            auto widget = std::make_unique<PanelKnob> (*this, i);
            addAndMakeVisible (*widget);
            knobs.push_back (std::move (widget));
        }
    }

    keyboard = std::make_unique<KeyboardOverlay> (processor.keyboard);
    addAndMakeVisible (*keyboard);

    status.setColour (juce::Label::textColourId, juce::Colour (0xffebe7da));
    status.setJustificationType (juce::Justification::centredLeft);
    status.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (status);

    for (auto* b : { &initialise, &allNotesOff, &clearMidi })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff293437));
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff39494d));
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffebe7da));
        addAndMakeVisible (*b);
    }

    initialise.onClick = [this] { processor.initPanel(); };
    allNotesOff.onClick = [this] { processor.panic(); };
    clearMidi.onClick = [this] { processor.clearAllCc(); };

    setSize (1000, 626);
    setResizable (true, true);
    if (auto* constrain = getConstrainer())
    {
        constrain->setSizeLimits (800, 501, 1500, 939);
        constrain->setFixedAspectRatio (1000.0 / 626.0);
    }

    startTimerHz (30);
}

PolivoksAudioProcessorEditor::~PolivoksAudioProcessorEditor()
{
    stopTimer();
    keyboard.reset();
    knobs.clear();
    switches.clear();
    setLookAndFeel (nullptr);
}

void PolivoksAudioProcessorEditor::drawSectionFrame (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title) const
{
    g.setColour (panelLine().withAlpha (0.83f));
    g.drawRect (r, 1.05f);

    // Section titles are drawn once, by drawTopPanel(); drawing them here as well
    // produced doubled, overlapping headings.
    juce::ignoreUnused (title);
}

void PolivoksAudioProcessorEditor::drawCabinet (juce::Graphics& g)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, referenceWidth, referenceHeight);
    g.fillAll (juce::Colour (0xffd8d6d0));

    auto body = bounds.reduced (28.0f, 12.0f);
    g.setColour (cabinetBlack());
    g.fillRoundedRectangle (body, 13.0f);

    auto topLip = body.removeFromTop (14.0f);
    g.setColour (cabinetEdge());
    g.fillRoundedRectangle (topLip, 10.0f);

    const auto leftRail  = juce::Rectangle<float> (32.0f, 10.0f, 48.0f, 357.0f);
    const auto rightRail = juce::Rectangle<float> (920.0f, 10.0f, 48.0f, 357.0f);
    for (auto rail : { leftRail, rightRail })
    {
        g.setColour (juce::Colour (0xff1b2225));
        g.fillRoundedRectangle (rail, 7.0f);
        g.setColour (juce::Colour (0xff4b5659));
        for (int i = 0; i < 12; ++i)
        {
            const float y = rail.getY() + 15.0f + (float) i * 28.0f;
            g.drawLine (rail.getX() + 5.5f, y, rail.getRight() - 5.5f, y, 0.75f);
        }
    }

    auto panel = juce::Rectangle<float> (74.0f, panelTopY, 852.0f, panelHeight);
    g.setColour (panelBlack());
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (juce::Colour (0xff212a2d));
    g.drawRoundedRectangle (panel, 8.0f, 1.8f);

    auto underKeys = juce::Rectangle<float> (29.0f, 507.0f, 942.0f, 47.0f);
    juce::ColourGradient grad (juce::Colour (0xff151a1d), 0.0f, underKeys.getY(),
                               juce::Colour (0xff050708), 0.0f, underKeys.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (underKeys, 10.0f);

    g.setColour (juce::Colour (0xff111619));
    g.fillRoundedRectangle ({ 77.0f, keyboardY - 13.0f, 846.0f, 14.0f }, 4.0f);
}

void PolivoksAudioProcessorEditor::drawTopPanel (juce::Graphics& g)
{
    auto logoBar = juce::Rectangle<float> (276.0f, 24.0f, 226.0f, 48.0f);
    g.setColour (juce::Colour (0xff0d1416));
    g.fillRect (logoBar);
    g.setColour (panelLine());
    g.drawRect (logoBar, 1.1f);

    auto logoFont = juce::Font (juce::FontOptions (31.0f));
    logoFont.setBold (true);
    g.setFont (logoFont);
    g.drawText (juce::CharPointer_UTF8 ("ПОЛИВОКС"), logoBar.reduced (10.0f, 2.0f), juce::Justification::centredLeft, false);

    auto markFont = juce::Font (juce::FontOptions (14.5f));
    markFont.setBold (true);
    g.setFont (markFont);
    g.drawText (u8 ("®"), 488, 35, 18, 16, juce::Justification::centred, false);

    auto sectionFont = juce::Font (juce::FontOptions (11.7f));
    sectionFont.setBold (true);
    g.setFont (sectionFont);
    g.drawText (juce::CharPointer_UTF8 ("МОДУЛЯТОР"), 108, 20, 112, 15, juce::Justification::centred, false);
    g.drawText (juce::CharPointer_UTF8 ("ГЕНЕРАТОР I"), 279, 76, 116, 14, juce::Justification::centred, false);
    g.drawText (juce::CharPointer_UTF8 ("ГЕНЕРАТОР II"), 415, 76, 120, 14, juce::Justification::centred, false);
    g.drawText (juce::CharPointer_UTF8 ("МИКШЕР"), 349, 208, 96, 14, juce::Justification::centred, false);
    g.drawText (juce::CharPointer_UTF8 ("ФИЛЬТР"), 590, 20, 86, 15, juce::Justification::centred, false);
    g.drawText (juce::CharPointer_UTF8 ("УСИЛИТЕЛЬ"), 748, 20, 96, 15, juce::Justification::centred, false);
}

void PolivoksAudioProcessorEditor::drawPanelSections (juce::Graphics& g)
{
    drawSectionFrame (g, { modSection.x,  modSection.y,  modSection.w,  modSection.h },  juce::String (juce::CharPointer_UTF8 ("МОДУЛЯТОР")));
    drawSectionFrame (g, { osc1Section.x, osc1Section.y, osc1Section.w, osc1Section.h }, juce::String (juce::CharPointer_UTF8 ("ГЕНЕРАТОР I")));
    drawSectionFrame (g, { osc2Section.x, osc2Section.y, osc2Section.w, osc2Section.h }, juce::String (juce::CharPointer_UTF8 ("ГЕНЕРАТОР II")));
    drawSectionFrame (g, { mixSection.x,  mixSection.y,  mixSection.w,  mixSection.h },  juce::String (juce::CharPointer_UTF8 ("МИКШЕР")));
    drawSectionFrame (g, { filtSection.x, filtSection.y, filtSection.w, filtSection.h }, juce::String (juce::CharPointer_UTF8 ("ФИЛЬТР")));
    drawSectionFrame (g, { ampSection.x,  ampSection.y,  ampSection.w,  ampSection.h },  juce::String (juce::CharPointer_UTF8 ("УСИЛИТЕЛЬ")));

    g.setColour (panelLine().withAlpha (0.82f));
    g.drawLine (674.0f, 222.0f, 698.0f, 222.0f, 1.0f);
    g.drawLine (674.0f, 252.0f, 698.0f, 252.0f, 1.0f);
    g.setFont (juce::Font (juce::FontOptions (8.6f)));
    g.drawText (juce::CharPointer_UTF8 ("ФНЧ"), 671, 208, 30, 10, juce::Justification::centred, false);
    g.drawText (juce::CharPointer_UTF8 ("ПФ"), 676, 254, 18, 10, juce::Justification::centred, false);

    juce::Path envShape;
    envShape.startNewSubPath (669.0f, 289.0f);
    envShape.lineTo (678.0f, 261.0f);
    envShape.lineTo (689.0f, 286.0f);
    envShape.lineTo (705.0f, 286.0f);
    g.strokePath (envShape, juce::PathStrokeType (1.2f));

    juce::Path bpShape;
    bpShape.startNewSubPath (686.0f, 286.0f);
    bpShape.lineTo (692.0f, 279.0f);
    bpShape.lineTo (698.0f, 286.0f);
    g.strokePath (bpShape, juce::PathStrokeType (1.0f));
}

void PolivoksAudioProcessorEditor::drawScrewsAndTrim (juce::Graphics& g)
{
    const std::array<juce::Point<float>, 8> screws {{
        { 90.0f, 28.0f }, { 910.0f, 28.0f }, { 90.0f, 308.0f }, { 910.0f, 308.0f },
        { 246.0f, 28.0f }, { 754.0f, 28.0f }, { 246.0f, 308.0f }, { 754.0f, 308.0f }
    }};

    for (auto p : screws)
    {
        g.setColour (juce::Colour (0xff2c3538));
        g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff85918d));
        g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 0.8f);
        g.drawLine (p.x - 2.0f, p.y, p.x + 2.0f, p.y, 0.7f);
    }
}

juce::StringArray PolivoksAudioProcessorEditor::legendForParam (int index)
{
    const auto label = shortParamName (index);
    if (label.containsChar (' '))
        return juce::StringArray::fromTokens (label, " ", {});
    return { label };
}

void PolivoksAudioProcessorEditor::drawControlLegends (juce::Graphics& g)
{
    g.setColour (panelLine().withAlpha (0.96f));
    g.setFont (juce::Font (juce::FontOptions (9.1f)));

    for (int i = 0; i < pv::Count; ++i)
    {
        const auto& d = pv::definitions[(size_t) i];
        const auto lines = legendForParam (i);
        const float x = (float) d.x;
        const float y = (float) d.y;

        if (d.toggle)
        {
            juce::Rectangle<int> box ((int) (x - 34.0f), (int) (y + 22.0f), 68, 17);
            g.drawFittedText (lines.joinIntoString (" "), box, juce::Justification::centredTop, 1);
        }
        else
        {
            // Single-line legend below the knob: a second line above it collided with
            // the legend of the row above (e.g. ЧАСТОТА СРЕЗА vs ЗАТУХАНИЕ).
            juce::Rectangle<int> lower ((int) (x - 30.0f), (int) (y + 24.0f), 60, 12); // knob pitch is ~56 px: shrink long text rather than touch the neighbour
            g.drawFittedText (lines.joinIntoString (" "), lower, juce::Justification::centredTop, 1);
        }
    }
}

void PolivoksAudioProcessorEditor::drawFooter (juce::Graphics& g)
{
    auto footer = juce::Rectangle<int> (0, 566, (int) referenceWidth, (int) (referenceHeight - 566.0f));
    g.setColour (juce::Colour (0xff11191b));
    g.fillRect (footer);
    g.setColour (panelSoft());
    g.drawHorizontalLine (566, 0.0f, referenceWidth);
    g.setColour (juce::Colour (0xffdadfd8));
    g.setFont (juce::Font (juce::FontOptions (11.2f)));
    g.drawText ("POLIVOKS STUDY  |  REDESIGNED PANEL V2  |  EXTERNAL MIDI KEYS ANIMATE THE KEYBED", 14, 572, 760, 16, juce::Justification::left);
}

void PolivoksAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto sx = (float) getWidth() / referenceWidth;
    const auto sy = (float) getHeight() / referenceHeight;
    g.addTransform (juce::AffineTransform::scale (sx, sy));

    drawCabinet (g);
    drawTopPanel (g);
    drawPanelSections (g);
    drawControlLegends (g);
    drawScrewsAndTrim (g);
    drawFooter (g);
}

void PolivoksAudioProcessorEditor::resized()
{
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;

    auto place = [sx, sy] (juce::Component& c, int x, int y, int w, int h)
    {
        c.setBounds (juce::roundToInt ((x - w / 2.0f) * sx),
                     juce::roundToInt ((y - h / 2.0f) * sy),
                     juce::roundToInt (w * sx),
                     juce::roundToInt (h * sy));
    };

    int knobIndex = 0;
    int switchIndex = 0;

    for (int i = 0; i < pv::Count; ++i)
    {
        const auto& d = pv::definitions[(size_t) i];
        if (d.toggle)
            place (*switches[(size_t) switchIndex++], d.x, d.y, 34, 52);
        else
            place (*knobs[(size_t) knobIndex++], d.x, d.y, 48, 48);
    }

    keyboard->setBounds (juce::roundToInt (90.0f * sx), juce::roundToInt (335.0f * sy),
                         juce::roundToInt (820.0f * sx), juce::roundToInt (176.0f * sy));

    status.setBounds (juce::roundToInt (14.0f * sx), juce::roundToInt (586.0f * sy),
                      juce::roundToInt (620.0f * sx), juce::roundToInt (32.0f * sy));
    initialise.setBounds (juce::roundToInt (680.0f * sx), juce::roundToInt (586.0f * sy),
                          juce::roundToInt (70.0f * sx), juce::roundToInt (28.0f * sy));
    allNotesOff.setBounds (juce::roundToInt (760.0f * sx), juce::roundToInt (586.0f * sy),
                           juce::roundToInt (80.0f * sx), juce::roundToInt (28.0f * sy));
    clearMidi.setBounds (juce::roundToInt (850.0f * sx), juce::roundToInt (586.0f * sy),
                         juce::roundToInt (135.0f * sx), juce::roundToInt (28.0f * sy));
}

void PolivoksAudioProcessorEditor::showParameter (int index)
{
    highlighted = index;
}

void PolivoksAudioProcessorEditor::timerCallback()
{
    const int learn = processor.getLearningParam();
    juce::String message;

    if (learn >= 0 && learn < pv::Count)
    {
        message = "MIDI LEARN: move a KeyLab control for ";
        message += pv::definitions[(size_t) learn].label;
    }
    else if (highlighted >= 0 && highlighted < pv::Count)
    {
        const auto& d = pv::definitions[(size_t) highlighted];
        const auto* param = processor.state.getParameter (d.id);
        const int cc = processor.getCcForParam (highlighted);
        message = juce::String (d.label) + " | " + d.detail;
        message += " | Value: " + juce::String (param->getValue(), 2);
        if (cc >= 0) message += " | MIDI CC " + juce::String (cc);
        else         message += " | right-click to assign CC";
    }
    else
    {
        message = "Polivoks panel v2: redrawn cabinet, more faithful section layout, refined controls, and animated key travel from external MIDI.";
    }

    if (status.getText() != message)
        status.setText (message, juce::dontSendNotification);

    keyboard->repaint();
}
