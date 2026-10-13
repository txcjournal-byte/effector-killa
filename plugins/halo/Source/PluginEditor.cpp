#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

namespace
{
const juce::Colour cBg   { 0xff070e13 };
const juce::Colour cText { 0xffdef2fa };
const juce::Colour cInk  { 0xff7098aa };
const juce::Colour cAcc  { 0xff5fe3ff };
const juce::Colour cIce  { 0xffe6fcff };
const juce::Colour cDeep { 0xff0c1a22 };

struct Fonts
{
    juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    static const Fonts& get() { static Fonts f; return f; }
};

juce::Font fnt (const juce::Typeface::Ptr& t, float h) { return juce::Font (juce::FontOptions (t).withHeight (h)); }

// ---- layout (must match tools/make_background.py) ----
namespace Layout
{
constexpr float visX = 410.0f, visY = 214.0f, visR = 114.0f;
constexpr float bigArcR = 55.0f, bigBody = 46.0f;
constexpr float smallDotR = 29.0f, smallBody = 19.0f;
constexpr int smallDots = 24;
const juce::Rectangle<int> freeze { 358, 340, 104, 28 };
} // namespace Layout

struct KnobDef { const char* id; float x, y; bool big; const char* tip; };
const KnobDef knobDefs[] = {
    { IDs::decay,     150, 228, true,  "Reverb time (RT60). Double-click = default" },
    { IDs::mix,       670, 228, true,  "Dry / wet blend" },
    { IDs::predelay,   68, 434, false, "Gap before the reverb starts: keeps vocals upfront" },
    { IDs::size,      150, 434, false, "Room size: small chamber to endless space" },
    { IDs::diffusion, 232, 434, false, "Echo density: low = grainy echoes, high = smooth wash" },
    { IDs::mod,       314, 434, false, "Chorus-like movement in the tail (kills metallic ringing)" },
    { IDs::lowcut,    410, 434, false, "Removes low end from the reverb (keeps 808s and kicks clean)" },
    { IDs::damping,   492, 434, false, "Darkens the reverb: lower = warmer, softer tail" },
    { IDs::width,     588, 434, false, "Stereo width of the reverb (0 = mono, 150 % = extra wide)" },
    { IDs::duck,      670, 434, false, "Reverb ducks while the input plays and blooms in the gaps" },
    { IDs::output,    752, 434, false, "Output level" },
};

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour col, float glow = 0.35f)
{
    g.setColour (col.withAlpha (glow * 0.5f));
    for (auto off : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (off.x, off.y), juce::Justification::centred);
    g.setColour (col);
    g.drawText (t, r, juce::Justification::centred);
}

void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour col, float width, float alpha)
{
    g.setColour (col.withAlpha (alpha * 0.10f));
    g.strokePath (p, juce::PathStrokeType (width * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col.withAlpha (alpha * 0.22f));
    g.strokePath (p, juce::PathStrokeType (width * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

float toLevel (float lin) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (lin, -80.0f) + 48.0f) / 48.0f); }
} // namespace

// ================================================================ Look

HaloLook::HaloLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cAcc.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff04161c));
    setColour (juce::PopupMenu::headerTextColourId, cAcc);
    setColour (juce::TooltipWindow::textColourId, cText);
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void HaloLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const bool big = juce::jmin (w, h) > 100;
    const float origin = (float) s.getProperties().getWithDefault ("origin", 0.0);
    const float angle = start + pos * (end - start);
    const auto col = s.isEnabled() ? cAcc : cInk;

    if (big)
    {
        const float a0 = start + origin * (end - start);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, Layout::bigArcR, Layout::bigArcR, 0.0f, juce::jmin (a0, angle), juce::jmax (a0, angle), true);
        g.setColour (col.withAlpha (0.10f));
        g.strokePath (arc, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setGradientFill (juce::ColourGradient (cIce, c.x, c.y - Layout::bigArcR, col, c.x, c.y + Layout::bigArcR, false));
        g.strokePath (arc, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // bright tip
        const float tx = c.x + std::sin (angle) * Layout::bigArcR, ty = c.y - std::cos (angle) * Layout::bigArcR;
        g.setColour (cIce.withAlpha (0.35f));
        g.fillEllipse (tx - 6.0f, ty - 6.0f, 12.0f, 12.0f);
        g.setColour (cIce);
        g.fillEllipse (tx - 2.6f, ty - 2.6f, 5.2f, 5.2f);
    }
    else
    {
        const int lit = juce::roundToInt (pos * Layout::smallDots), o = juce::roundToInt (origin * Layout::smallDots);
        for (int k = juce::jmin (lit, o); k <= juce::jmax (lit, o); ++k)
        {
            const float a = juce::degreesToRadians (225.0f - 270.0f * (float) k / Layout::smallDots);
            const float lx = c.x + std::cos (a) * Layout::smallDotR, ly = c.y - std::sin (a) * Layout::smallDotR;
            g.setColour (col.withAlpha (0.22f));
            g.fillEllipse (lx - 4.2f, ly - 4.2f, 8.4f, 8.4f);
            g.setColour (k == lit ? cIce : col);
            g.fillEllipse (lx - 1.8f, ly - 1.8f, 3.6f, 3.6f);
        }
    }

    // photoreal knob (pre-rendered); only the pointer rotates
    const float body = big ? Layout::bigBody : Layout::smallBody;
    const float imgHalf = body / 0.80f;
    g.drawImage (big ? knobBig : knobSmall, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    juce::Path p;
    const float pw = big ? 3.4f : 2.6f;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.93f, pw, topR * (big ? 0.40f : 0.55f), pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (col.withAlpha (0.30f));
    g.strokePath (p, juce::PathStrokeType (3.0f), tr);
    g.setColour (s.isEnabled() ? cAcc.interpolatedWith (cIce, 0.3f) : cInk);
    g.fillPath (p, tr);
}

void HaloLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (juce::Colour (0xff2c4b58).withAlpha (box.isMouseOver (true) ? 1.0f : 0.8f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a161c), 0, 0, juce::Colour (0xff050c10), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    g.setColour (cAcc.withAlpha (0.08f));
    g.fillRoundedRectangle (r.reduced (1.0f).withHeight ((float) h * 0.45f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 15.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cAcc);
    g.fillPath (arrow);
}

void HaloLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font HaloLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 18.0f); }
juce::Font HaloLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void HaloLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (cDeep);
    g.setColour (cAcc.withAlpha (0.35f));
    g.drawRect (0, 0, w, h, 1);
}

void HaloLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool isFreeze = (bool) b.getProperties().getWithDefault ("freeze", false);
    const bool on = b.getToggleState();

    if (isFreeze)
    {
        if (on)
        {
            // illuminated ice key
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (cAcc.withAlpha (0.10f * (float) (4 - i)));
                g.fillRoundedRectangle (r.expanded ((float) i * 2.5f), 7.0f + (float) i);
            }
            g.setGradientFill (juce::ColourGradient (cIce, 0, r.getY(), cAcc.darker (0.15f), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillRoundedRectangle (r.reduced (3.0f).withHeight (r.getHeight() * 0.38f), 4.0f);
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r, 6.0f, 1.2f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff0b171d : 0xff17303a), 0, r.getY(),
                                                     juce::Colour (0xff08131a), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (cAcc.withAlpha (over ? 0.6f : 0.35f));
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
        }
        return;
    }

    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff0b171d : 0xff1a323c), 0, 0,
                                             juce::Colour (0xff09141a), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (over ? cAcc.withAlpha (0.7f) : juce::Colour (0xff2c4b58));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void HaloLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const auto txt = b.getButtonText();
    if (txt == "<" || txt == ">")
    {
        const float cx = r.getCentreX(), cy = r.getCentreY(), s = 4.5f, dir = txt == "<" ? -1.0f : 1.0f;
        juce::Path p;
        p.startNewSubPath (cx - dir * s * 0.6f, cy - s);
        p.lineTo (cx + dir * s * 0.6f, cy);
        p.lineTo (cx - dir * s * 0.6f, cy + s);
        g.setColour (over ? cIce : cAcc);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    const bool on = b.getToggleState();
    // snowflake icon + label
    const float iconR = 6.5f;
    g.setFont (fnt (Fonts::get().label, 16.0f));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt);
    const float total = iconR * 2.0f + 8.0f + tw;
    const float x0 = r.getCentreX() - total * 0.5f;
    const juce::Point<float> ic (x0 + iconR, r.getCentreY());
    const auto col = on ? juce::Colour (0xff053040) : (over ? cIce : cAcc);
    juce::Path flake;
    for (int i = 0; i < 3; ++i)
    {
        const float a = juce::MathConstants<float>::pi * (float) i / 3.0f + juce::MathConstants<float>::halfPi;
        const float dx = std::cos (a) * iconR, dy = std::sin (a) * iconR;
        flake.startNewSubPath (ic.x - dx, ic.y - dy);
        flake.lineTo (ic.x + dx, ic.y + dy);
        for (float sgn : { -1.0f, 1.0f })
        {
            const float bx = ic.x + sgn * dx * 0.62f, by = ic.y + sgn * dy * 0.62f;
            for (float side : { -1.0f, 1.0f })
            {
                const float ba = a + side * 0.75f;
                flake.startNewSubPath (bx, by);
                flake.lineTo (bx + sgn * std::cos (ba) * iconR * 0.38f, by + sgn * std::sin (ba) * iconR * 0.38f);
            }
        }
    }
    if (! on)
    {
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (flake, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (col);
    g.strokePath (flake, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawText (txt, juce::Rectangle<float> (x0 + iconR * 2.0f + 8.0f, r.getY(), tw + 4.0f, r.getHeight()), juce::Justification::centredLeft);
}

void HaloLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (cDeep);
    g.setColour (cAcc.withAlpha (0.5f));
    g.drawRect (0, 0, w, h, 1);
    g.setColour (cText);
    g.setFont (fnt (Fonts::get().semi, 15.0f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (6, 2), juce::Justification::centredLeft, 3);
}

// ================================================================ Panel

HaloPanel::HaloPanel (HaloProcessor& p) : proc (p)
{
    setLookAndFeel (&look);
    tooltips.setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        const auto& def = knobDefs[i];
        k.centre = { def.x, def.y };
        k.big = def.big;
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setMouseDragSensitivity (def.big ? 260 : 200);
        k.s.setTooltip (def.tip);
        auto* param = proc.apvts.getParameter (def.id);
        if (std::strcmp (def.id, IDs::output) == 0)
            k.s.getProperties().set ("origin", param->convertTo0to1 (0.0f));
        k.s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.s);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, def.id, k.s);
        k.s.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    }

    freezeButton.getProperties().set ("freeze", true);
    freezeButton.setClickingTogglesState (true);
    freezeButton.setTooltip ("Holds the current reverb tail forever (new input is not added)");
    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::freeze, freezeButton);

    const auto& presets = halo::factoryPresets();
    juce::String lastCat;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (lastCat != presets[(size_t) i].category)
        {
            lastCat = presets[(size_t) i].category;
            presetBox.addSectionHeading (lastCat.toUpperCase());
        }
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    }
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
    };
    presetBox.setTooltip ("Factory presets");
    addAndMakeVisible (presetBox);
    auto step = [this] (int dir)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + n) % n);
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    juce::Random rnd (1337);
    for (auto& pt : particles)
    {
        pt.angle = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        pt.radius = rnd.nextFloat() * 2.0f - 1.0f;
        pt.speed = 0.35f + rnd.nextFloat() * 0.9f;
        pt.size = 1.0f + rnd.nextFloat() * 1.8f;
        pt.twinkle = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    freezeGlow = param (IDs::freeze) > 0.5f ? 1.0f : 0.0f;
    startTimerHz (60);
}

HaloPanel::~HaloPanel()
{
    stopTimer();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void HaloPanel::timerCallback()
{
    constexpr float dt = 1.0f / 60.0f;
    const bool frozen = param (IDs::freeze) > 0.5f;
    if (! demo)
    {
        const float in = toLevel (proc.uiIn.load()), wet = toLevel (proc.uiWet.load());
        lvIn += (in > lvIn ? 0.5f : 0.08f) * (in - lvIn);
        lvWet += (wet > lvWet ? 0.3f : 0.05f) * (wet - lvWet);
    }
    freezeGlow += 0.12f * ((frozen ? 1.0f : 0.0f) - freezeGlow);
    const float speed = (0.25f + 0.9f * param (IDs::mod)) * (1.0f - freezeGlow * 0.92f);
    phase += dt * speed;

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    repaint (juce::Rectangle<float> (Layout::visR * 2.0f, Layout::visR * 2.0f).withCentre ({ Layout::visX, Layout::visY }).toNearestInt());
}

void HaloPanel::paintVisualiser (juce::Graphics& g)
{
    using namespace Layout;
    const juce::Point<float> c (visX, visY);
    const float sizeN = param (IDs::size);
    const auto* decayParam = proc.apvts.getParameter (IDs::decay);
    const float decayN = decayParam->convertTo0to1 (param (IDs::decay));
    const float modN = param (IDs::mod);
    const float widthN = param (IDs::width);
    const float mixN = param (IDs::mix);
    const float duckG = demo ? 1.0f : proc.uiDuck.load();
    const float fr = freezeGlow;
    const auto col = cAcc.interpolatedWith (cIce, 0.55f * fr);

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addEllipse (juce::Rectangle<float> ((visR - 2.0f) * 2.0f, (visR - 2.0f) * 2.0f).withCentre (c));
    g.reduceClipRegion (clip);

    const float energy = juce::jlimit (0.0f, 1.0f, 0.18f + 0.82f * juce::jmax (lvWet * (0.4f + 0.6f * mixN), fr * 0.85f)) * (0.35f + 0.65f * duckG);

    // core glow driven by the input
    {
        const float coreR = 16.0f + 30.0f * lvIn + 8.0f * fr;
        juce::ColourGradient cg (col.withAlpha (0.20f + 0.55f * lvIn + 0.25f * fr), c.x, c.y, col.withAlpha (0.0f), c.x + coreR * 2.2f, c.y, true);
        g.setGradientFill (cg);
        g.fillEllipse (juce::Rectangle<float> (coreR * 4.4f, coreR * 4.4f).withCentre (c));
        g.setColour (cIce.withAlpha ((0.25f + 0.6f * lvIn) * (1.0f - fr)));
        g.fillEllipse (juce::Rectangle<float> (4.0f + 5.0f * lvIn, 4.0f + 5.0f * lvIn).withCentre (c));
    }

    // halo rings: radius = size, ring count / persistence = decay, wobble = modulation, oval = width
    const float baseR = 34.0f + 38.0f * sizeN;
    const float spacing = 5.0f + 8.0f * decayN;
    const float persist = 0.8f + 3.2f * decayN + 6.0f * fr;
    const float xs = 0.84f + 0.11f * widthN, ys = 1.0f - 0.04f * widthN;
    const int numRings = 6;

    // soft bloom between the main ring and the tail
    {
        const float outer = juce::jmin (visR - 6.0f, baseR + spacing * (float) numRings * 0.9f + 10.0f);
        juce::ColourGradient bloom (col.withAlpha (0.0f), c.x, c.y, col.withAlpha (0.0f), c.x + outer, c.y, true);
        const float inner = juce::jmax (0.05f, (baseR - 18.0f) / outer);
        bloom.addColour (inner, col.withAlpha (0.0f));
        bloom.addColour (juce::jlimit (inner + 0.01f, 0.98f, baseR / outer), col.withAlpha (0.20f * energy));
        bloom.addColour (0.99, col.withAlpha (0.0f));
        g.setGradientFill (bloom);
        g.fillEllipse (juce::Rectangle<float> (outer * 2.0f * xs, outer * 2.0f * ys).withCentre (c));
    }

    for (int k = numRings - 1; k >= 0; --k)
    {
        const float rk = baseR + (float) k * spacing;
        if (rk > visR - 10.0f) continue;
        const float alpha = std::exp (-(float) k / persist) * energy * (k == 0 ? 1.0f : 0.75f);
        if (alpha < 0.02f) continue;
        juce::Path ring;
        const int segs = 120;
        const float amt = modN * (0.6f + 0.25f * (float) k);
        for (int i = 0; i <= segs; ++i)
        {
            const float th = juce::MathConstants<float>::twoPi * (float) i / (float) segs;
            const float wob = amt * (2.4f * std::sin (3.0f * th + phase * (1.3f + 0.35f * (float) k))
                                     + 1.4f * std::sin (5.0f * th - phase * 1.9f + (float) k))
                              + lvWet * 1.6f * std::sin (7.0f * th + phase * 3.1f + (float) k * 0.7f);
            const float r = rk + wob;
            const float px = c.x + std::cos (th) * r * xs, py = c.y + std::sin (th) * r * ys;
            if (i == 0) ring.startNewSubPath (px, py); else ring.lineTo (px, py);
        }
        ring.closeSubPath();
        glowStroke (g, ring, k == 0 ? col.interpolatedWith (cIce, 0.45f) : col, k == 0 ? 2.6f : 1.1f, alpha);
    }

    // orbiting particles (the "air" of the tail)
    for (const auto& pt : particles)
    {
        const float a = pt.angle + phase * pt.speed * 0.8f;
        const float r = baseR + spacing * 2.0f + pt.radius * spacing * (1.5f + 2.5f * decayN);
        if (r > visR - 8.0f || r < 8.0f) continue;
        const float tw = 0.5f + 0.5f * std::sin (pt.twinkle + phase * 4.0f * pt.speed);
        const float al = juce::jlimit (0.0f, 1.0f, energy * (0.25f + 0.75f * tw));
        const float px = c.x + std::cos (a) * r * xs, py = c.y + std::sin (a) * r * ys;
        g.setColour (col.withAlpha (al * 0.25f));
        g.fillEllipse (px - pt.size * 2.0f, py - pt.size * 2.0f, pt.size * 4.0f, pt.size * 4.0f);
        g.setColour (cIce.withAlpha (al));
        g.fillEllipse (px - pt.size * 0.5f, py - pt.size * 0.5f, pt.size, pt.size);
    }

    // centre read-out
    g.setFont (fnt (Fonts::get().mono, 13.0f));
    if (fr > 0.05f)
        {
        g.setColour (juce::Colour (0xff04161c).withAlpha (0.55f * fr));
        g.fillRoundedRectangle (juce::Rectangle<float> (64.0f, 18.0f).withCentre (c), 4.0f);
        glowText (g, "FROZEN", juce::Rectangle<float> (100.0f, 16.0f).withCentre (c), cIce.withAlpha (fr), 0.6f);
    }

    // glass reflection
    juce::ColourGradient refl (juce::Colours::white.withAlpha (0.07f), c.x - visR * 0.5f, c.y - visR * 0.8f,
                               juce::Colours::white.withAlpha (0.0f), c.x, c.y, false);
    g.setGradientFill (refl);
    g.fillEllipse (c.x - visR * 0.9f, c.y - visR * 0.98f, visR * 1.5f, visR * 1.1f);
}

void HaloPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    paintVisualiser (g);
}

void HaloPanel::paintOverChildren (juce::Graphics& g)
{
    // LCD read-outs
    for (auto& k : knobs)
    {
        const auto txt = k.s.getTextFromValue (k.s.getValue());
        if (k.big)
        {
            g.setFont (fnt (Fonts::get().mono, 17.0f));
            glowText (g, txt, juce::Rectangle<float> (98.0f, 20.0f).withCentre ({ k.centre.x, k.centre.y + 96.0f }), cAcc);
        }
        else
        {
            g.setFont (fnt (Fonts::get().mono, 14.0f));
            glowText (g, txt, juce::Rectangle<float> (68.0f, 16.0f).withCentre ({ k.centre.x, 476.0f }), cAcc);
        }
    }
}

void HaloPanel::resized()
{
    for (auto& k : knobs)
    {
        const float sz = k.big ? 150.0f : 66.0f;
        k.s.setBounds (juce::Rectangle<float> (sz, sz).withCentre (k.centre).toNearestInt());
    }
    freezeButton.setBounds (Layout::freeze);
    prevButton.setBounds (470, 28, 30, 30);
    presetBox.setBounds (504, 28, 250, 30);
    nextButton.setBounds (758, 28, 30, 30);
}

// ================================================================ Editor

HaloEditor::HaloEditor (HaloProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (HaloPanel::baseW * 3 / 4, HaloPanel::baseH * 3 / 4, HaloPanel::baseW * 2, HaloPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) HaloPanel::baseW / HaloPanel::baseH);
    setSize (HaloPanel::baseW, HaloPanel::baseH);
}

void HaloEditor::resized()
{
    panel.setBounds (0, 0, HaloPanel::baseW, HaloPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / HaloPanel::baseW));
}
