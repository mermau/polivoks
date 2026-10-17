#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace {
    void showMidiMenu (juce::Component* target, PolivoksAudioProcessor& processor, int param) {
        juce::PopupMenu menu;
        menu.addItem(1,"Learn MIDI CC for " + juce::String(pv::definitions[static_cast<size_t>(param)].label));
        const auto cc = processor.getCcForParam(param);
        menu.addItem(2,cc < 0 ? "No MIDI CC assigned" : ("Clear CC " + juce::String(cc)), cc >= 0);
        juce::Component::SafePointer<juce::Component> safe(target);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
            [safe,&processor,param](int result) {
                if(safe == nullptr) return;
                if(result == 1) processor.beginMidiLearn(param);
                if(result == 2) processor.clearCc(param);
            });
    }
}

class PolivoksAudioProcessorEditor::PolivoksLook final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float start, float end, juce::Slider&) override {
        const auto b=juce::Rectangle<float>(static_cast<float>(x+2),static_cast<float>(y+2),
                                            static_cast<float>(width-4),static_cast<float>(height-4));
        const auto cx=b.getCentreX(), cy=b.getCentreY();
        const float radius=std::min(b.getWidth(),b.getHeight())*0.48f;
        g.setColour(juce::Colour(0xee080d0d));
        g.fillEllipse(cx-radius,cy-radius,2*radius,2*radius);
        g.setColour(juce::Colour(0xff48575c));
        g.drawEllipse(cx-radius,cy-radius,2*radius,2*radius,1.3f);
        for(int i=0;i<7;++i) {
            const auto a=start+(end-start)*static_cast<float>(i)/6.0f;
            g.setColour(juce::Colour(0xffb2c4c2).withAlpha(0.85f));
            g.drawLine(cx+std::sin(a)*radius*0.72f,cy-std::cos(a)*radius*0.72f,
                       cx+std::sin(a)*radius*0.88f,cy-std::cos(a)*radius*0.88f,0.8f);
        }
        const auto a=start+position*(end-start);
        g.setColour(juce::Colour(0xfff1eee3));
        g.drawLine(cx,cy,cx+std::sin(a)*radius*0.7f,cy-std::cos(a)*radius*0.7f,2.4f);
        g.setColour(juce::Colour(0xff333e40));
        g.fillEllipse(cx-2,cy-2,4,4);
    }
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                           bool, bool) override {
        const auto rect=b.getLocalBounds().toFloat().reduced(4);
        g.setColour(juce::Colour(0xff101515));
        g.fillRoundedRectangle(rect,3);
        g.setColour(juce::Colour(0xffbcc4bc));
        g.drawRoundedRectangle(rect,3,1.2f);
        const float swH=rect.getHeight()*0.47f;
        const float swY = b.getToggleState() ? rect.getY()+2 : rect.getBottom()-swH-2;
        juce::Rectangle<float> paddle(rect.getX()+4,swY,rect.getWidth()-8,swH);
        g.setColour(juce::Colour(0xffe3e2d5));
        g.fillRoundedRectangle(paddle,2);
        g.setColour(juce::Colour(0xff777972));
        g.drawLine(paddle.getX()+2,paddle.getBottom()-3,
                   paddle.getRight()-2,paddle.getBottom()-3,1.0f);
    }
};

class PolivoksAudioProcessorEditor::PanelKnob final : public juce::Slider {
public:
    PanelKnob (PolivoksAudioProcessorEditor& parent, int param)
        : juce::Slider(), editor(parent), index(param) {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        setMouseDragSensitivity(160);
        setScrollWheelEnabled(true);
        const auto& d=pv::definitions[static_cast<size_t>(index)];
        const float step=d.steps>1 ? 1.0f / static_cast<float>(d.steps-1) : 0.0f;
        setRange(0.0,1.0,step);
        setRotaryParameters(-2.45f,2.45f,true);
        setTooltip(juce::String(d.label) + ": " + d.detail + "; right-click for MIDI learn");
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            editor.processor.state, d.id, *this);
    }
    void mouseEnter(const juce::MouseEvent&) override { editor.showParameter(index); }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isPopupMenu()) { showMidiMenu(this,editor.processor,index); return; }
        juce::Slider::mouseDown(e);
    }
private:
    PolivoksAudioProcessorEditor& editor;
    int index;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class PolivoksAudioProcessorEditor::PanelSwitch final : public juce::ToggleButton {
public:
    PanelSwitch (PolivoksAudioProcessorEditor& parent,int param)
        : editor(parent), index(param) {
        setClickingTogglesState(true);
        const auto& d=pv::definitions[static_cast<size_t>(param)];
        setTooltip(juce::String(d.label) + ": " + d.detail + "; right-click for MIDI learn");
        attachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            editor.processor.state,d.id,*this);
    }
    void mouseEnter(const juce::MouseEvent&) override { editor.showParameter(index); }
    void mouseDown(const juce::MouseEvent& e) override {
        if(e.mods.isPopupMenu()) { showMidiMenu(this,editor.processor,index); return; }
        juce::ToggleButton::mouseDown(e);
    }
private:
    PolivoksAudioProcessorEditor& editor;
    int index;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class PolivoksAudioProcessorEditor::KeyboardOverlay final : public juce::Component {
public:
    explicit KeyboardOverlay(juce::MidiKeyboardState& s) : state(s) {
        setInterceptsMouseClicks(true,false);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void paint(juce::Graphics& g) override {
        if(active < 0) return;
        const float ww=static_cast<float>(getWidth())/28.0f;
        int note=36;
        for(int i=0;i<28;++i) {
            note=36+(i/7)*12+whiteOffsets[i%7];
            if(active==note) {
                g.setColour(juce::Colours::orange.withAlpha(0.28f));
                g.fillRect(juce::Rectangle<float>(ww*i,0,ww,static_cast<float>(getHeight())));
                return;
            }
            if(i%7!=2 && i%7!=6 && active==note+1) {
                const float bw=ww*0.52f;
                g.setColour(juce::Colours::orange.withAlpha(0.50f));
                g.fillRect(juce::Rectangle<float>(ww*(i+1)-bw/2,0,bw,static_cast<float>(getHeight())*0.62f));
                return;
            }
        }
    }
    void mouseDown(const juce::MouseEvent& e) override { updateAt(e.position); }
    void mouseDrag(const juce::MouseEvent& e) override { updateAt(e.position); }
    void mouseUp(const juce::MouseEvent&) override { release(); }
    void mouseExit(const juce::MouseEvent& e) override {
        if(!e.mods.isAnyMouseButtonDown()) release();
    }
    ~KeyboardOverlay() override { release(); }
private:
    inline static constexpr int whiteOffsets[7] {0,2,4,5,7,9,11};
    juce::MidiKeyboardState& state;
    int active=-1;
    int noteAt (juce::Point<float> p) const noexcept {
        if(p.x < 0 || p.x >= getWidth() || p.y < 0 || p.y >= getHeight()) return -1;
        const float ww=static_cast<float>(getWidth())/28.0f;
        if(p.y < getHeight()*0.62f) {
            const float bw=ww*0.52f;
            for(int i=0;i<27;++i) {
                if(i%7==2 || i%7==6) continue;
                const float keyX=ww*(i+1)-bw/2;
                if(p.x >= keyX && p.x < keyX+bw)
                    return 36+(i/7)*12+whiteOffsets[i%7]+1;
            }
        }
        const int i=juce::jlimit(0,27,static_cast<int>(p.x/ww));
        return 36+(i/7)*12+whiteOffsets[i%7];
    }
    void updateAt(juce::Point<float> p) {
        const int note=noteAt(p);
        if(note==active) return;
        release();
        active=note;
        if(active>=0) state.noteOn(1,active,0.90f);
        repaint();
    }
    void release() {
        if(active>=0) { state.noteOff(1,active,0.0f); active=-1; repaint(); }
    }
};

PolivoksAudioProcessorEditor::PolivoksAudioProcessorEditor (PolivoksAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p), theme(std::make_unique<PolivoksLook>()) {
    panelImage=juce::ImageCache::getFromMemory(BinaryData::panel_jpg,BinaryData::panel_jpgSize);
    setLookAndFeel(theme.get());
    for(int i=0; i<pv::Count; ++i) {
        if(pv::definitions[static_cast<size_t>(i)].toggle) {
            auto widget=std::make_unique<PanelSwitch>(*this,i);
            addAndMakeVisible(*widget);
            switches.push_back(std::move(widget));
        } else {
            auto widget=std::make_unique<PanelKnob>(*this,i);
            addAndMakeVisible(*widget);
            knobs.push_back(std::move(widget));
        }
    }
    keyboard=std::make_unique<KeyboardOverlay>(processor.keyboard);
    addAndMakeVisible(*keyboard);
    status.setColour(juce::Label::textColourId,juce::Colour(0xffe9e6db));
    status.setJustificationType(juce::Justification::centredLeft);
    status.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(status);
    initialise.onClick=[this] { processor.initPanel(); };
    allNotesOff.onClick=[this] { processor.panic(); };
    clearMidi.onClick=[this] { processor.clearAllCc(); };
    addAndMakeVisible(initialise);
    addAndMakeVisible(allNotesOff);
    addAndMakeVisible(clearMidi);
    setSize(1000,626);
    setResizable(true,true);
    if(auto* constrain=getConstrainer()) {
        constrain->setSizeLimits(800,501,1500,939);
        constrain->setFixedAspectRatio(1000.0/626.0);
    }
    startTimerHz(20);
}

PolivoksAudioProcessorEditor::~PolivoksAudioProcessorEditor() {
    stopTimer();
    keyboard.reset();
    knobs.clear(); switches.clear();
    setLookAndFeel(nullptr);
}

void PolivoksAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff101716));
    if(panelImage.isValid())
        g.drawImage(panelImage,juce::Rectangle<float>(0,0,static_cast<float>(getWidth()),
                                        static_cast<float>(getHeight())*563.0f/626.0f));
    const float sy=static_cast<float>(getHeight())/626.0f;
    const auto footerY=563.0f*sy;
    g.setColour(juce::Colour(0xff101a1b));
    g.fillRect(0,static_cast<int>(footerY),getWidth(),getHeight()-static_cast<int>(footerY));
    g.setColour(juce::Colour(0xff697d78));
    g.drawHorizontalLine(static_cast<int>(footerY),0,static_cast<float>(getWidth()));
    g.setColour(juce::Colour(0xffa6b8ad));
    g.setFont(juce::Font(juce::FontOptions(11.0f*sy)));
    g.drawText("POLIVOKS STUDY  |  2 VCO  •  12 dB LP/BP  •  MIDI CC LEARN",
               static_cast<int>(17*sy),static_cast<int>(567*sy),static_cast<int>(630*sy),
               static_cast<int>(18*sy),juce::Justification::left);
}

void PolivoksAudioProcessorEditor::resized() {
    const float sx=static_cast<float>(getWidth())/1000.0f;
    const float sy=static_cast<float>(getHeight())/626.0f;
    auto place=[sx,sy](juce::Component& widget,int x,int y,int w,int h) {
        widget.setBounds(juce::roundToInt((x-w/2)*sx),juce::roundToInt((y-h/2)*sy),
                         juce::roundToInt(w*sx),juce::roundToInt(h*sy));
    };
    int j=0,k=0;
    for(int i=0;i<pv::Count;++i) {
        const auto& d=pv::definitions[static_cast<size_t>(i)];
        if(d.toggle) place(*switches[static_cast<size_t>(j++)],d.x,d.y,34,40);
        else place(*knobs[static_cast<size_t>(k++)],d.x,d.y,42,42);
    }
    keyboard->setBounds(juce::roundToInt(91*sx),juce::roundToInt(309*sy),
                        juce::roundToInt(822*sx),juce::roundToInt(166*sy));
    status.setBounds(juce::roundToInt(14*sx),juce::roundToInt(585*sy),
                     juce::roundToInt(660*sx),juce::roundToInt(34*sy));
    initialise.setBounds(juce::roundToInt(692*sx),juce::roundToInt(586*sy),
                         juce::roundToInt(69*sx),juce::roundToInt(28*sy));
    allNotesOff.setBounds(juce::roundToInt(771*sx),juce::roundToInt(586*sy),
                           juce::roundToInt(77*sx),juce::roundToInt(28*sy));
    clearMidi.setBounds(juce::roundToInt(858*sx),juce::roundToInt(586*sy),
                         juce::roundToInt(130*sx),juce::roundToInt(28*sy));
}

void PolivoksAudioProcessorEditor::showParameter(int i) { highlighted=i; }

void PolivoksAudioProcessorEditor::timerCallback() {
    const int learn=processor.getLearningParam();
    juce::String message;
    if(learn >=0 && learn<pv::Count) {
        message="MIDI LEARN: move a KeyLab knob/fader for ";
        message+=pv::definitions[static_cast<size_t>(learn)].label;
    } else if(highlighted >= 0 && highlighted<pv::Count) {
        const auto& d=pv::definitions[static_cast<size_t>(highlighted)];
        const auto* param=processor.state.getParameter(d.id);
        const int cc=processor.getCcForParam(highlighted);
        message=juce::String(d.label)+" | "+d.detail;
        message+=" | Value: "+juce::String(param->getValue(),2);
        if(cc>=0) message+=" | MIDI CC "+juce::String(cc);
        else message+=" | right-click to assign CC";
    } else message="Play 48 on-screen keys or MIDI notes. Right-click any control for MIDI CC Learn.";
    if(status.getText()!=message) status.setText(message,juce::dontSendNotification);
}
