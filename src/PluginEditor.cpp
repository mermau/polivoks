#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
using namespace vks;
using namespace vks::ui;

namespace
{
const Colour kPanelBlack  (0xff121212);
const Colour kSilver      (0xffa9a8a3);
const Colour kPrint       (0xffecebe6);   // serigrafia bianca
const Colour kPlastic     (0xff0b0b0b);

float degToRad (float d) { return d * MathConstants<float>::pi / 180.0f; }
Point<float> polar (Point<float> c, float r, float deg)
{
    const float a = degToRad (deg);
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

void drawText (Graphics& g, const String& s, Point<float> c, float h, Justification j = Justification::centred,
               const Font* f = nullptr)
{
    g.setFont (f ? *f : Fonts::get().label (h));
    const float w = 120.0f;
    g.drawText (s, Rectangle<float> (c.x - w * 0.5f, c.y - h * 0.6f, w, h * 1.2f), j, false);
}

// simboli serigrafati (forme d'onda, inviluppi)
void drawSymbol (Graphics& g, const String& sym, Point<float> c, float s)
{
    Path p;
    const float x0 = c.x - s * 0.5f, x1 = c.x + s * 0.5f, yT = c.y - s * 0.32f, yB = c.y + s * 0.32f;
    auto pulse = [&] (float w)
    {
        p.startNewSubPath (x0, yB); p.lineTo (x0, yT); p.lineTo (x0 + s * w, yT);
        p.lineTo (x0 + s * w, yB); p.lineTo (x1, yB);
    };
    if (sym == "tri")      { p.startNewSubPath (x0, yB); p.lineTo (c.x, yT); p.lineTo (x1, yB); }
    else if (sym == "saw") { p.startNewSubPath (x0, yB); p.lineTo (x0, yT); p.lineTo (x1, yB); }
    else if (sym == "p50") pulse (0.5f);
    else if (sym == "p25") pulse (0.3f);
    else if (sym == "p10") pulse (0.14f);
    else if (sym == "sq")  { p.startNewSubPath (x0, yB); p.lineTo (x0, yT); p.lineTo (c.x, yT); p.lineTo (c.x, yB); p.lineTo (x1, yB); p.lineTo (x1, yT); }
    else if (sym == "noise")
    {
        const float ys[] = { 0.1f, -0.9f, 0.6f, -0.3f, 1.0f, -0.7f, 0.2f, -1.0f, 0.8f, -0.2f, 0.4f };
        for (int i = 0; i < 11; ++i)
        {
            const float x = x0 + s * (float) i / 10.0f, y = c.y + ys[i] * s * 0.32f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
    }
    else if (sym == "steps")
    {
        const float lv[] = { 0.3f, -0.6f, 0.9f, -0.1f, -0.9f };
        float y = c.y + lv[0] * s * 0.32f;
        p.startNewSubPath (x0, y);
        for (int i = 0; i < 5; ++i)
        {
            const float xa = x0 + s * (float) i / 5.0f, xb = x0 + s * (float) (i + 1) / 5.0f;
            const float ny = c.y + lv[i] * s * 0.32f;
            p.lineTo (xa, ny); p.lineTo (xb, ny);
        }
    }
    else if (sym == "env1") { p.startNewSubPath (x0, yB); p.lineTo (c.x - s * 0.1f, yT); p.lineTo (x1, yB); }
    else if (sym == "env2")
    {
        p.startNewSubPath (x0, yB); p.lineTo (x0 + s * 0.22f, yT); p.lineTo (c.x, yB);
        p.lineTo (c.x + s * 0.22f, yT); p.lineTo (x1, yB);
    }
    g.strokePath (p, PathStrokeType (s * 0.085f, PathStrokeType::mitered, PathStrokeType::butt));
}

// percorso del testo scalato in modo che l'inchiostro riempia esattamente il rettangolo
Path fitText (const Font& f, const String& text, Rectangle<float> target, float kerning)
{
    GlyphArrangement ga;
    ga.addLineOfText (f.withExtraKerningFactor (kerning), text, 0.0f, 0.0f);
    Path p; ga.createPath (p);
    float x0 = 1.0e9f, y0 = 1.0e9f, x1 = -1.0e9f, y1 = -1.0e9f;
    for (PathFlatteningIterator it (p); it.next();)
        for (auto pt : { Point<float> (it.x1, it.y1), Point<float> (it.x2, it.y2) })
        { x0 = jmin (x0, pt.x); y0 = jmin (y0, pt.y); x1 = jmax (x1, pt.x); y1 = jmax (y1, pt.y); }
    if (x1 <= x0 || y1 <= y0) return p;
    p.applyTransform (AffineTransform::translation (-x0, -y0)
                          .scaled (target.getWidth() / (x1 - x0), target.getHeight() / (y1 - y0))
                          .translated (target.getX(), target.getY()));
    return p;
}

const char* rangeLabels[] = { "32'", "16'", "8'", "4'", "2'" };
const char* shapeSyms[]   = { "tri", "saw", "p50", "p25", "p10" };
const char* lfoSyms[]     = { "tri", "sq", "noise", "steps" };
} // namespace

//==============================================================================
Fonts& Fonts::get() { static Fonts f; return f; }

Fonts::Fonts()
{
    logoFace    = Typeface::createSystemTypefaceFor (BinaryData::RussoOne_ttf, BinaryData::RussoOne_ttfSize);
    narrowBold  = Typeface::createSystemTypefaceFor (BinaryData::PTSansNarrowBold_ttf, BinaryData::PTSansNarrowBold_ttfSize);
    sansBold    = Typeface::createSystemTypefaceFor (BinaryData::PTSansBold_ttf, BinaryData::PTSansBold_ttfSize);
    sansRegular = Typeface::createSystemTypefaceFor (BinaryData::PTSansRegular_ttf, BinaryData::PTSansRegular_ttfSize);
}
Font Fonts::label (float h) const   { return Font (FontOptions (narrowBold).withHeight (h)).withExtraKerningFactor (0.06f); }
Font Fonts::logo() const          { return Font (FontOptions (logoFace).withHeight (40.0f)); }
Font Fonts::bold (float h) const    { return Font (FontOptions (sansBold).withHeight (h)); }
Font Fonts::regular (float h) const { return Font (FontOptions (sansRegular).withHeight (h)); }

//==============================================================================
void Knob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float prop = (float) valueToProportionOfLength (getValue());
    const float deg = startDeg + prop * (endDeg - startDeg);

    if (! isSelector)
    {
        const float rs = 11.6f, rc = 8.6f;
        // ombra portata
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - rs + 1.2f, c.y - rs + 2.0f, rs * 2.0f, rs * 2.0f);
        // gonna zigrinata
        g.setGradientFill (ColourGradient (Colour (0xff3c3c3c), c.x - rs, c.y - rs, Colour (0xff050505), c.x + rs, c.y + rs, false));
        g.fillEllipse (c.x - rs, c.y - rs, rs * 2.0f, rs * 2.0f);
        g.setColour (Colour (0xff000000).withAlpha (0.6f));
        for (int i = 0; i < 36; ++i)
        {
            const float a = (float) i * 10.0f;
            g.drawLine (Line<float> (polar (c, rc + 0.5f, a), polar (c, rs - 0.3f, a)), 0.45f);
        }
        // calotta
        g.setGradientFill (ColourGradient (Colour (0xff4a4a4a), c.x - rc * 0.6f, c.y - rc * 0.8f, Colour (0xff0a0a0a), c.x + rc * 0.5f, c.y + rc, true));
        g.fillEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f);
        g.setColour (Colour (0xff000000));
        g.drawEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f, 0.6f);
        // indice bianco
        g.setColour (kPrint);
        g.drawLine (Line<float> (polar (c, rc * 0.25f, deg), polar (c, rs - 0.6f, deg)), 1.5f);
    }
    else
    {
        const float r = 13.5f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - r + 1.5f, c.y - r + 2.5f, r * 2.0f, r * 2.0f);
        g.setGradientFill (ColourGradient (Colour (0xff383838), c.x - r, c.y - r, Colour (0xff030303), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (Colour (0xff000000));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.7f);
        // barra del selettore
        Path bar;
        bar.addRoundedRectangle (-3.6f, -r + 0.5f, 7.2f, r * 2.0f - 1.0f, 3.0f);
        bar.applyTransform (AffineTransform::rotation (degToRad (deg)).translated (c));
        g.setGradientFill (ColourGradient (Colour (0xff505050), c.x - r, c.y - r, Colour (0xff101010), c.x + r, c.y + r, false));
        g.fillPath (bar);
        g.setColour (Colours::black);
        g.strokePath (bar, PathStrokeType (0.6f));
        g.setColour (kPrint);
        g.drawLine (Line<float> (polar (c, 2.0f, deg), polar (c, r - 1.5f, deg)), 1.6f);
    }

    if (host.isLearning (paramId))
    {
        g.setColour (Colour (0xffff3b30));
        g.drawEllipse (b.reduced (0.8f), 1.3f);
    }
}

//==============================================================================
void PushSwitch::paintButton (Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat();
    const bool on = getToggleState();
    // alloggiamento
    g.setColour (Colour (0xff000000));
    g.fillRoundedRectangle (b, 1.6f);
    auto face = b.reduced (1.4f);
    if (on) face = face.reduced (0.6f).translated (0.0f, 0.9f);
    else    face = face.translated (0.0f, -0.4f);

    const Colour base = isRed ? Colour (0xffd3241c) : Colour (0xffeae7df);
    const Colour faceTop = on ? base.darker (0.25f) : base.brighter (0.15f);
    const Colour faceBot = on ? base.darker (0.05f) : base.darker (0.22f);
    g.setGradientFill (ColourGradient (faceTop, face.getX(), face.getY(), faceBot, face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 1.2f);
    // bordo superiore (tasto rilasciato = sporgente)
    g.setColour (on ? Colours::black.withAlpha (0.35f) : Colours::white.withAlpha (0.6f));
    g.drawLine (face.getX() + 1.0f, face.getY() + 0.5f, face.getRight() - 1.0f, face.getY() + 0.5f, 0.7f);
    // rigatura centrale del tasto
    g.setColour (Colours::black.withAlpha (on ? 0.22f : 0.14f));
    g.drawLine (face.getX() + 2.5f, face.getCentreY(), face.getRight() - 2.5f, face.getCentreY(), 0.6f);
    if (over) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (face, 1.2f); }

    if (host.isLearning (paramId))
    {
        g.setColour (Colour (0xffff3b30));
        g.drawRoundedRectangle (b.reduced (0.5f), 1.5f, 1.2f);
    }
}

//==============================================================================
Keys::Keys (MidiKeyboardState& s) : MidiKeyboardComponent (s, horizontalKeyboard)
{
    setAvailableRange (41, 88);           // 4 ottave, dal Fa al Mi, come l'originale
    setScrollButtonsVisible (false);
    setBlackNoteLengthProportion (0.62f);
    setBlackNoteWidthProportion (0.56f);
    setOctaveForMiddleC (4);
    setVelocity (1.0f, false);
    setColour (shadowColourId, Colours::black.withAlpha (0.55f));
    setColour (keySeparatorLineColourId, Colour (0xff6c6a64));
    setColour (mouseOverKeyOverlayColourId, Colours::transparentBlack);
    setColour (keyDownOverlayColourId, Colours::transparentBlack);
}

void Keys::drawWhiteNote (int, Graphics& g, Rectangle<float> a, bool down, bool, Colour, Colour)
{
    auto k = a.reduced (0.45f, 0.0f);
    const Colour top = down ? Colour (0xffcfccc3) : Colour (0xfff4f2ec);
    const Colour bot = down ? Colour (0xffbab6ac) : Colour (0xffdcd8cf);
    g.setGradientFill (ColourGradient (top, k.getX(), k.getY(), bot, k.getX(), k.getBottom(), false));
    g.fillRect (k.withTrimmedBottom (2.0f));
    g.setColour (down ? Colour (0xff9e9a90) : Colour (0xffc4c0b6));
    g.fillRect (k.withTop (k.getBottom() - (down ? 1.2f : 2.6f)));
    g.setColour (Colour (0xff4a4844));
    g.drawRect (a.withTrimmedRight (-0.2f), 0.5f);
}

void Keys::drawBlackNote (int, Graphics& g, Rectangle<float> a, bool down, bool, Colour)
{
    g.setColour (Colours::black);
    g.fillRect (a);
    auto top = a.reduced (a.getWidth() * 0.13f, 0.0f).withTrimmedBottom (a.getHeight() * (down ? 0.05f : 0.11f));
    g.setGradientFill (ColourGradient (down ? Colour (0xff2a2a2a) : Colour (0xff3d3d3d), top.getX(), top.getY(),
                                       Colour (0xff0d0d0d), top.getRight(), top.getY(), false));
    g.fillRect (top);
    g.setColour (Colours::white.withAlpha (down ? 0.05f : 0.14f));
    g.fillRect (top.withWidth (top.getWidth() * 0.25f).translated (top.getWidth() * 0.12f, 0.0f));
}

void Keys::paint (Graphics& g) { MidiKeyboardComponent::paint (g); }

//==============================================================================
Panel::Panel (VksProcessor& p) : proc (p), keys (p.keyboardState)
{
    setSize (W, H);

    // ---------------------------------------------------------- manopole
    knobSpecs = {
        { pid::lfoRate,  232, 80,  Scale::MinMax,  "ЧАСТОТА",  "Frequenza del modulatore (0.1 … 50 Hz)" },
        { pid::phones,   232, 152, Scale::ZeroSix, "Т.ЛФ.",    "Volume cuffia (uscita quando ВЫХОД è spento)" },
        { pid::tune,     305, 152, Scale::Bipolar, "НАСТРОЙКА","Accordatura generale (±1 tono)" },
        { pid::glide,    232, 222, Scale::MinMax,  "ГЛИССАНДО","Glissando del Generatore I" },
        { pid::volume,   232, 290, Scale::ZeroSix, "ГРОМКОСТЬ","Volume d'uscita" },
        { pid::g1Mod,    383, 203, Scale::ZeroSix, "МОДУЛЯЦИЯ","Modulazione di frequenza del Gen I dal modulatore" },
        { pid::g1Fm,     455, 203, Scale::ZeroSix, "ГЕНЕРАТОР II", "FM del Gen I dal Gen II (tenere il Gen II più basso)" },
        { pid::g2Mod,    535, 203, Scale::ZeroSix, "МОДУЛЯЦИЯ","Modulazione di frequenza del Gen II dal modulatore" },
        { pid::g2Detune, 610, 203, Scale::Bipolar, "РАССТРОЙКА","Scordatura del Gen II (±2 toni)" },
        { pid::mixG1,    383, 290, Scale::ZeroSix, "ГЕНЕРАТОР I", "Livello del Generatore I" },
        { pid::mixG2,    458, 290, Scale::ZeroSix, "ГЕНЕРАТОР II","Livello del Generatore II" },
        { pid::mixNoise, 535, 290, Scale::ZeroSix, "ШУМ",       "Livello del rumore bianco" },
        { pid::mixExt,   610, 290, Scale::ZeroSix, "ВНЕШ.",     "Livello della sorgente esterna (ingresso audio)" },
        { pid::fAtt,     680, 82,  Scale::MinMax,  "АТАКА",     "Filtro: attacco dell'inviluppo" },
        { pid::fDec,     680, 152, Scale::MinMax,  "ЗАТУХАНИЕ", "Filtro: decadimento" },
        { pid::fSus,     745, 152, Scale::ZeroSix, "ПЬЕДЕСТАЛ", "Filtro: piedistallo (sustain)" },
        { pid::fRel,     810, 152, Scale::MinMax,  "ПОСЛЕЗВУЧ.","Filtro: rilascio" },
        { pid::fCut,     680, 222, Scale::MinMax,  "ЧАСТОТА СРЕЗА", "Frequenza di taglio (a zero il filtro è chiuso)" },
        { pid::fRes,     745, 222, Scale::MinMax,  "РЕЗОНАНС",  "Risonanza: verso il massimo il filtro autooscilla" },
        { pid::fMod,     680, 290, Scale::ZeroSix, "МОДУЛЯЦИЯ", "Modulazione del taglio dal modulatore" },
        { pid::fEnv,     745, 290, Scale::ZeroSix, "ОГИБАЮЩАЯ", "Quantità d'inviluppo sul taglio" },
        { pid::aAtt,     868, 82,  Scale::MinMax,  "АТАКА",     "Amplificatore: attacco" },
        { pid::aDec,     930, 82,  Scale::MinMax,  "ЗАТУХАНИЕ", "Amplificatore: decadimento" },
        { pid::aSus,     868, 152, Scale::ZeroSix, "ПЬЕДЕСТАЛ", "Amplificatore: piedistallo (sustain)" },
        { pid::aRel,     930, 152, Scale::MinMax,  "ПОСЛЕЗВУЧ.","Amplificatore: rilascio" },
        { pid::aMod,     868, 290, Scale::ZeroSix, "МОДУЛЯЦИЯ", "Modulazione d'ampiezza dal modulatore" },
    };
    selSpecs = {
        { pid::lfoMode,  305, 80,  2, "РЕЖИМ",    "Forma del modulatore: triangolo, quadra, impulsi caotici, gradini casuali" },
        { pid::g1Range,  380, 136, 0, "ДИАПАЗОН", "Gamma del Generatore I (32' … 2')" },
        { pid::g1Shape,  455, 136, 1, "ФОРМА",    "Forma d'onda del Generatore I" },
        { pid::g2Range,  535, 136, 0, "ДИАПАЗОН", "Gamma del Generatore II (32' … 2')" },
        { pid::g2Shape,  610, 136, 1, "ФОРМА",    "Forma d'onda del Generatore II" },
    };
    swSpecs = {
        { pid::duo,     305, 222, "1-ГОЛ.", "2-ГОЛ.", "Premuto: duofonico (Gen I = nota più bassa, Gen II = più alta)", false },
        { pid::lineOut, 305, 290, "",       "ВЫХОД", "Premuto: uscita di linea (ГРОМКОСТЬ); rilasciato: uscita cuffia (Т.ЛФ.)", false },
        { pid::fLoop,   812, 92,  "env1",   "env2",  "Inviluppo del filtro: singolo / ripetuto (premuto)", false },
        { pid::fLp,     812, 222, "ПФ",     "ФНЧ",   "Premuto: passa-basso (ФНЧ); rilasciato: passa-banda (ПФ)", false },
        { pid::aLoop,   930, 222, "env1",   "env2",  "Inviluppo dell'amplificatore: singolo / ripetuto (premuto)", false },
        { pid::aOn,     930, 290, "",       "ВКЛ.",  "Premuto: amplificatore sempre aperto sull'ultima nota", false },
        { pid::power,   981, 292, "",       "СЕТЬ",  "Accensione", true },
    };

    for (auto& s : knobSpecs) addKnob (s);
    for (auto& s : selSpecs)  addSelector (s);
    for (auto& s : swSpecs)   addSwitch (s);

    addAndMakeVisible (keys);
    const auto k0 = P (201, 347);
    keys.setBounds (Rectangle<float> (k0.x, k0.y, 798.0f, 172.0f).toNearestInt());
    keys.setKeyWidth ((float) keys.getWidth() / 28.0f);
    keys.setLowestVisibleKey (41);

    logoArea = Rectangle<float> (P (347, 35).x, P (347, 35).y, 298.0f, 45.0f);
    startTimerHz (30);
}

Panel::~Panel() { stopTimer(); }

void Panel::addKnob (const KnobSpec& s)
{
    auto* k = knobs.add (new Knob (*this, s.id, false));
    addAndMakeVisible (k);
    const auto c = P (s.x, s.y);
    k->setBounds (Rectangle<float> (26.0f, 26.0f).withCentre (c).toNearestInt());
    k->setTooltip (String::fromUTF8 (s.tip));
    sliderAtt.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, s.id, *k));
}

void Panel::addSelector (const SelSpec& s)
{
    const int n = s.kind == 2 ? 4 : 5;
    auto* k = knobs.add (new Knob (*this, s.id, true, n));
    k->startDeg = -90.0f; k->endDeg = 90.0f;
    addAndMakeVisible (k);
    k->setBounds (Rectangle<float> (30.0f, 30.0f).withCentre (P (s.x, s.y)).toNearestInt());
    k->setTooltip (String::fromUTF8 (s.tip));
    sliderAtt.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, s.id, *k));
}

void Panel::addSwitch (const SwSpec& s)
{
    auto* b = switches.add (new PushSwitch (*this, s.id, s.red));
    addAndMakeVisible (b);
    b->setBounds (Rectangle<float> (s.red ? 15.0f : 14.0f, 20.0f).withCentre (P (s.x, s.y)).toNearestInt());
    b->setTooltip (String::fromUTF8 (s.tip));
    buttonAtt.add (new AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, s.id, *b));
}

void Panel::paint (Graphics& g)
{
    // --------------------------------------------------- guance laterali nervate
    for (float cx : { 152.0f, 1002.0f })
    {
        auto r = Rectangle<float> (P (cx, 22).x, P (cx, 22).y, 46.0f, 523.0f);
        g.setGradientFill (ColourGradient (Colour (0xff1c1c1c), r.getX(), 0, Colour (0xff050505), r.getRight(), 0, false));
        g.fillRoundedRectangle (r, 5.0f);
        for (float y = r.getY() + 10.0f; y < r.getBottom() - 8.0f; y += 15.5f)
        {
            g.setColour (Colour (0xff000000)); g.fillRect (r.getX() + 3.0f, y, r.getWidth() - 6.0f, 2.6f);
            g.setColour (Colour (0xff2b2b2b)); g.fillRect (r.getX() + 3.0f, y + 2.6f, r.getWidth() - 6.0f, 0.9f);
        }
    }
    // --------------------------------------------------- telaio e pannello
    {
        auto frame = Rectangle<float> (P (198, 22).x, P (198, 22).y, 806.0f, 326.0f);
        g.setColour (kPlastic); g.fillRect (frame);
        auto panel = Rectangle<float> (P (200, 30).x, P (200, 30).y, 800.0f, 305.0f);
        g.setGradientFill (ColourGradient (kSilver.brighter (0.12f), panel.getX(), panel.getY(), kSilver.darker (0.18f), panel.getX(), panel.getBottom(), false));
        g.fillRect (panel);
    }
    struct Box { float x, y, w, h; };
    const Box boxes[] = { { 203, 33, 140, 85 }, { 203, 122, 140, 210 }, { 347, 33, 298, 47 }, { 347, 84, 148, 156 },
                          { 498, 84, 147, 156 }, { 347, 244, 298, 88 }, { 649, 33, 188, 299 }, { 840, 33, 120, 299 },
                          { 963, 33, 35, 299 } };
    for (auto& b : boxes)
    {
        auto r = Rectangle<float> (P (b.x, b.y).x, P (b.x, b.y).y, b.w, b.h);
        g.setColour (kPanelBlack); g.fillRoundedRectangle (r, 2.2f);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (r, 2.2f, 0.6f);
    }
    // logo: targa scura come l'originale, scritta ВОЛКС82 che la riempie, marchio piccolo a destra
    {
        auto plate = logoArea.reduced (3.0f, 3.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3a3d), plate.getX(), plate.getY(),
                                           Colour (0xff27272a), plate.getX(), plate.getBottom(), false));
        g.fillRoundedRectangle (plate, 3.0f);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (plate, 3.0f, 0.8f);

        auto inner = plate.reduced (7.0f, 6.0f);
        auto brand = inner.removeFromRight (52.0f);
        inner.removeFromRight (7.0f);

        auto word = fitText (Fonts::get().logo(), String::fromUTF8 ("ВОЛКС82"), inner, 0.02f);
        g.setColour (Colour (0xff0c0c0c));
        g.strokePath (word, PathStrokeType (1.6f, PathStrokeType::curved));   // contorno scuro della stampa
        g.setColour (Colour (0xfff1f0ec));
        g.fillPath (word);

        // КОЛЛАТЕРАЛ / ЭФФЕКТС giustificati alla stessa larghezza
        const float lineH = 6.4f, gap = 3.2f;
        auto b = brand.withSizeKeepingCentre (brand.getWidth(), lineH * 2.0f + gap);
        g.setColour (kPrint);
        g.fillPath (fitText (Fonts::get().logo(), String::fromUTF8 ("КОЛЛАТЕРАЛ"), b.removeFromTop (lineH), 0.10f));
        b.removeFromTop (gap);
        g.fillPath (fitText (Fonts::get().logo(), String::fromUTF8 ("ЭФФЕКТС"), b, 0.10f));
    }

    g.setColour (kPrint);
    const float lh = 6.6f;
    // titoli delle sezioni
    drawText (g, String::fromUTF8 ("МОДУЛЯТОР"), P (273, 42), lh);
    drawText (g, String::fromUTF8 ("ГЕНЕРАТОР I"), P (421, 93), lh);
    drawText (g, String::fromUTF8 ("ГЕНЕРАТОР II"), P (571, 93), lh);
    drawText (g, String::fromUTF8 ("МИКШЕР"), P (496, 253), lh);
    drawText (g, String::fromUTF8 ("ФИЛЬТР"), P (725, 43), lh);
    drawText (g, String::fromUTF8 ("УСИЛИТЕЛЬ"), P (899, 43), lh);
    // linee sotto i titoli del mixer
    g.drawLine (Line<float> (P (360, 253), P (472, 253)), 0.7f);
    g.drawLine (Line<float> (P (520, 253), P (632, 253)), 0.7f);

    // ------------------------------------------------------------ scale delle manopole
    for (auto& s : knobSpecs)
    {
        const auto c = P (s.x, s.y);
        const float rr = 18.5f, nh = 6.0f;
        if (s.scale == Scale::ZeroSix)
            for (int i = 0; i <= 6; ++i) drawText (g, String (i), polar (c, rr, -135.0f + 45.0f * (float) i), nh);
        else if (s.scale == Scale::MinMax)
        {
            for (int i = 1; i <= 5; ++i) drawText (g, String (i), polar (c, rr, -135.0f + 45.0f * (float) i), nh);
            drawText (g, String::fromUTF8 ("МИН."), c.translated (-15.0f, 15.5f), 5.6f);
            drawText (g, String::fromUTF8 ("МАКС."), c.translated (15.0f, 15.5f), 5.6f);
        }
        else
        {
            const char* lbl[] = { "2", "1", "0", "1", "2" };
            for (int i = 0; i < 5; ++i) drawText (g, lbl[i], polar (c, rr, -90.0f + 45.0f * (float) i), nh);
            drawText (g, String::fromUTF8 ("−"), polar (c, rr, -135.0f), 7.0f);
            drawText (g, "+", polar (c, rr, 135.0f), 7.0f);
        }
        // tacche
        for (int i = 0; i <= 6; ++i)
        {
            const float a = -135.0f + 45.0f * (float) i;
            g.drawLine (Line<float> (polar (c, 12.6f, a), polar (c, 14.2f, a)), 0.6f);
        }
        drawText (g, String::fromUTF8 (s.label), c.translated (0.0f, s.scale == Scale::MinMax ? 22.5f : 21.5f), lh);
    }
    // ------------------------------------------------------------ selettori
    for (auto& s : selSpecs)
    {
        const auto c = P (s.x, s.y);
        const int n = s.kind == 2 ? 4 : 5;
        for (int i = 0; i < n; ++i)
        {
            const float a = -90.0f + 180.0f * (float) i / (float) (n - 1);
            const auto pt = polar (c, s.kind == 2 ? 23.0f : 24.0f, a);
            g.drawLine (Line<float> (polar (c, 15.2f, a), polar (c, 17.0f, a)), 0.7f);
            if (s.kind == 0) drawText (g, rangeLabels[i], pt, 7.2f, Justification::centred, nullptr);
            else drawSymbol (g, s.kind == 1 ? shapeSyms[i] : lfoSyms[i], pt, 8.0f);
        }
        drawText (g, String::fromUTF8 (s.label), c.translated (0.0f, 22.5f), lh);
    }
    // ------------------------------------------------------------ interruttori
    for (auto& s : swSpecs)
    {
        const auto c = P (s.x, s.y);
        auto lab = [&] (const char* t, float dy)
        {
            const String str = String::fromUTF8 (t);
            if (str.isEmpty()) return;
            if (str.startsWith ("env")) drawSymbol (g, str, c.translated (0.0f, dy), 9.0f);
            else drawText (g, str, c.translated (0.0f, dy), lh);
        };
        lab (s.top, -17.5f);
        lab (s.bottom, 18.5f);
    }
    // "РЕЖИМ ГЕНЕРАТОРА ОГИБАЮЩЕЙ" accanto ai due interruttori di modo
    for (auto pt : { P (758, 92), P (889, 222) })
    {
        drawText (g, String::fromUTF8 ("РЕЖИМ"), pt.translated (0, -8.0f), lh);
        drawText (g, String::fromUTF8 ("ГЕНЕРАТОРА"), pt, lh);
        drawText (g, String::fromUTF8 ("ОГИБАЮЩЕЙ"), pt.translated (0, 8.0f), lh);
    }
    // grafico ФНЧ / ПФ del filtro
    {
        const auto o = P (788, 276);
        g.drawLine (o.x, o.y + 24.0f, o.x + 44.0f, o.y + 24.0f, 0.7f);
        g.drawLine (o.x + 1.0f, o.y, o.x + 1.0f, o.y + 24.0f, 0.7f);
        Path lp; lp.startNewSubPath (o.x + 1.0f, o.y + 7.0f); lp.lineTo (o.x + 17.0f, o.y + 7.0f); lp.lineTo (o.x + 21.0f, o.y + 2.0f); lp.lineTo (o.x + 29.0f, o.y + 24.0f);
        Path bp; bp.startNewSubPath (o.x + 9.0f, o.y + 24.0f); bp.lineTo (o.x + 20.0f, o.y + 3.0f); bp.lineTo (o.x + 31.0f, o.y + 24.0f);
        g.strokePath (lp, PathStrokeType (0.8f));
        g.strokePath (bp, PathStrokeType (0.8f));
        drawText (g, String::fromUTF8 ("ФНЧ"), P (781, 282), 5.6f);
        drawText (g, String::fromUTF8 ("ПФ"), P (781, 297), 5.6f);
        g.drawLine (Line<float> (P (788, 282), P (796, 282)), 0.5f);
        g.drawLine (Line<float> (P (788, 297), P (801, 290)), 0.5f);
    }

    // ------------------------------------------------------------ mensola sopra la tastiera
    {
        auto ledge = Rectangle<float> (P (198, 335).x, P (198, 335).y, 806.0f, 13.0f);
        g.setGradientFill (ColourGradient (Colour (0xff222222), 0, ledge.getY(), Colour (0xff030303), 0, ledge.getBottom(), false));
        g.fillRect (ledge);
        auto lip = Rectangle<float> (P (198, 519).x, P (198, 519).y, 806.0f, 26.0f);
        g.setGradientFill (ColourGradient (Colour (0xff1a1a1a), 0, lip.getY(), Colour (0xff050505), 0, lip.getBottom(), false));
        g.fillRoundedRectangle (lip, 3.0f);
    }
}

void Panel::paintOverChildren (Graphics& g)
{
    // LED del modulatore (indicatore 21) e LED di rete (indicatore 40)
    auto drawLed = [&] (Point<float> c, float level)
    {
        const float r = 3.0f;
        g.setColour (Colour (0xff1a0000)); g.fillEllipse (c.x - r - 1.0f, c.y - r - 1.0f, (r + 1.0f) * 2.0f, (r + 1.0f) * 2.0f);
        const Colour on (0xffff2a1a), off (0xff4a0d08);
        g.setColour (off.interpolatedWith (on, level)); g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        if (level > 0.05f)
        {
            g.setGradientFill (ColourGradient (on.withAlpha (0.45f * level), c.x, c.y, on.withAlpha (0.0f), c.x + r * 3.2f, c.y, true));
            g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);
        }
        g.setColour (Colours::white.withAlpha (0.35f)); g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.65f, r * 0.6f, r * 0.45f);
    };
    drawLed (P (268, 58), powerOn ? led : 0.0f);
    drawLed (P (981, 262), powerOn ? 1.0f : 0.0f);
}

void Panel::timerCallback()
{
    const float l = proc.lfoLed();
    const bool pw = proc.apvts.getRawParameterValue (pid::power)->load() > 0.5f;
    if (std::abs (l - led) > 0.01f || pw != powerOn)
    {
        led = l; powerOn = pw;
        repaint (Rectangle<float> (16, 16).withCentre (P (268, 58)).toNearestInt());
        repaint (Rectangle<float> (16, 16).withCentre (P (981, 262)).toNearestInt());
    }
    // fine del MIDI Learn: ridisegna il comando appena assegnato
    if (learnId.isNotEmpty() && proc.learningIndex() < 0)
    {
        learnId.clear();
        for (auto* k : knobs) k->repaint();
        for (auto* s : switches) s->repaint();
    }
}

bool Panel::isLearning (const String& id) const
{
    const int li = proc.learningIndex();
    return li >= 0 && li == proc.paramIndexOf (id);
}

void Panel::showControlMenu (Component& c, const String& id)
{
    const int idx = proc.paramIndexOf (id);
    const int cc = proc.ccForParam (idx);
    PopupMenu m;
    m.addSectionHeader (proc.getParameters()[idx]->getName (64) + (cc >= 0 ? "  (CC " + String (cc) + ")" : String()));
    m.addItem (1, "MIDI Learn: muovi un controllo della tastiera MIDI");
    m.addItem (2, "Rimuovi assegnazione MIDI", cc >= 0);
    if (proc.learningIndex() >= 0) m.addItem (3, "Annulla MIDI Learn");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&c), [this, idx, id, &c] (int r)
    {
        if (r == 1) { proc.startLearn (idx); learnId = id; }
        else if (r == 2) proc.clearMapping (idx);
        else if (r == 3) proc.cancelLearn();
        for (auto* k : knobs) k->repaint();
        for (auto* s : switches) s->repaint();
        ignoreUnused (c);
    });
}

void Panel::mouseDown (const MouseEvent& e)
{
    if (logoArea.contains (e.position)) showSettingsMenu();
}

void Panel::showSettingsMenu()
{
    auto* bend = proc.apvts.getParameter (pid::bendRange);
    auto* analog = proc.apvts.getParameter (pid::analog);
    const int curBend = (int) proc.apvts.getRawParameterValue (pid::bendRange)->load();
    const float curAn = proc.apvts.getRawParameterValue (pid::analog)->load();

    PopupMenu bm;
    for (int s : { 0, 1, 2, 3, 5, 7, 12 }) bm.addItem (100 + s, s == 0 ? "Spento" : String (s) + " semitoni", true, curBend == s);
    PopupMenu am;
    am.addItem (200, "Spenta (strumento ideale)", true, curAn < 0.01f);
    am.addItem (201, "Ridotta", true, curAn > 0.01f && curAn < 0.9f);
    am.addItem (202, "Normale (come l'originale)", true, curAn >= 0.9f);

    PopupMenu m;
    m.addSectionHeader ("Volks82  -  Collateral Effects");
    m.addSubMenu ("Pitch bend", bm);
    m.addSubMenu ("Deriva analogica", am);
    m.addSeparator();
    m.addItem (300, "Ripristina le assegnazioni MIDI predefinite");
    m.addItem (301, "Annulla MIDI Learn", proc.learningIndex() >= 0);
    m.showMenuAsync (PopupMenu::Options(), [this, bend, analog] (int r)
    {
        if (r >= 100 && r <= 112) bend->setValueNotifyingHost (bend->convertTo0to1 ((float) (r - 100)));
        else if (r == 200) analog->setValueNotifyingHost (0.0f);
        else if (r == 201) analog->setValueNotifyingHost (0.4f);
        else if (r == 202) analog->setValueNotifyingHost (1.0f);
        else if (r == 300) proc.resetDefaultMapping();
        else if (r == 301) proc.cancelLearn();
    });
}

//==============================================================================
VksEditor::VksEditor (VksProcessor& p) : AudioProcessorEditor (p), proc (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) Panel::W / (double) Panel::H);
    setResizeLimits (630, 371, 1980, 1166);
    setSize (1170, 689);
}

void VksEditor::resized()
{
    const float s = (float) getWidth() / (float) Panel::W;
    panel.setBounds (0, 0, Panel::W, Panel::H);
    panel.setTransform (AffineTransform::scale (s));
}
