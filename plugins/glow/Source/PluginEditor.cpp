#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText { 0xffeee8da };
const juce::Colour cInk  { 0xff968e7c };
const juce::Colour cSun  { 0xffffe45c };
const juce::Colour cHot  { 0xffff7a3c };
const juce::Colour cDeep { 0xff1a1814 };

struct Fonts
{
    juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    juce::Typeface::Ptr heavy = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    static const Fonts& get() { static Fonts f; return f; }
};

juce::Font fnt (const juce::Typeface::Ptr& t, float h) { return juce::Font (juce::FontOptions (t).withHeight (h)); }

// ---- layout (base px) – must match tools/make_background.py
const juce::Point<float> knobC { 420.0f, 300.0f };
constexpr float litHalf = 204.0f, knobBody = 112.0f, ringR = 126.0f;
constexpr float smallKnob = 64.0f;
const juce::Point<float> smallC[3] = { { 662.0f, 180.0f }, { 762.0f, 180.0f }, { 662.0f, 300.0f } };
const juce::Rectangle<int> agRect { 722, 276, 84, 50 };
const juce::Rectangle<float> glowLcd { 366.0f, 440.0f, 108.0f, 28.0f };
const juce::Rectangle<float> enhLcd { 32.0f, 374.0f, 194.0f, 116.0f };
const juce::Rectangle<float> metLcd { 614.0f, 374.0f, 194.0f, 116.0f };
constexpr int modeX = 34, modeW = 190, modeH = 48;
const int modeY[4] = { 132, 188, 244, 300 };

const char* modeSubs[4] = { "tube body  ·  smooth top", "air  ·  presence  ·  sparkle", "weight  ·  octave  ·  thump", "dense  ·  pushed  ·  limited" };

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c)
{
    g.setColour (c.withAlpha (0.18f));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (dx, dy), j);
    g.setColour (c);
    g.drawText (t, r, j);
}

// segmented LCD bar: value 0..1 from 'zero' (0..1 position of the zero mark) – lit segments between zero and value
void lcdBar (juce::Graphics& g, juce::Rectangle<float> r, float zero, float value, juce::Colour col, float hotFrom = 2.0f)
{
    const int segs = 22;
    const float sw = r.getWidth() / (float) segs;
    const float lo = juce::jmin (zero, value), hi = juce::jmax (zero, value);
    for (int k = 0; k < segs; ++k)
    {
        const float f0 = (float) k / (float) segs, f1 = (float) (k + 1) / (float) segs;
        const bool lit = f1 > lo + 1e-4f && f0 < hi - 1e-4f;
        const auto seg = juce::Rectangle<float> (r.getX() + (float) k * sw + 0.8f, r.getY(), sw - 1.6f, r.getHeight());
        const auto c = (f0 >= hotFrom) ? cHot : col;
        if (lit)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillRect (seg.expanded (1.0f, 1.5f));
            g.setColour (c);
        }
        else
            g.setColour (col.withAlpha (0.10f));
        g.fillRect (seg);
    }
    if (zero > 0.0f)
    {
        g.setColour (cText.withAlpha (0.55f));
        g.fillRect (r.getX() + zero * r.getWidth() - 0.5f, r.getY() - 3.0f, 1.0f, r.getHeight() + 6.0f);
    }
}
} // namespace

// ================= Look =================

GlowLook::GlowLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cSun);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff1a1404));
    setColour (juce::PopupMenu::headerTextColourId, cSun);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cSun.withAlpha (0.5f));
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void GlowLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cSun : cInk;

    const int lit = juce::roundToInt (pos * 20.0f);
    const int mid = bipolar ? 10 : 0;
    for (int k = juce::jmin (mid, lit); k <= juce::jmax (mid, lit); ++k)
    {
        const float a = juce::degreesToRadians (225.0f - 270.0f * (float) k / 20.0f);
        const float lx = c.x + std::cos (a) * 29.0f, ly = c.y - std::sin (a) * 29.0f;
        g.setColour (on.withAlpha (0.22f));
        g.fillEllipse (lx - 3.6f, ly - 3.6f, 7.2f, 7.2f);
        g.setColour (on);
        g.fillEllipse (lx - 1.7f, ly - 1.7f, 3.4f, 3.4f);
    }

    const float imgHalf = 22.0f / 0.80f;
    g.drawImage (knobSmall, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    juce::Path p;
    p.addRoundedRectangle (-1.3f, -topR * 0.94f, 2.6f, topR * 0.46f, 1.3f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (on.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (2.5f), tr);
    g.setColour (on.brighter (0.25f));
    g.fillPath (p, tr);
}

void GlowLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff6c6248) : juce::Colour (0xff4a4436));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0d0b), 0, 0, juce::Colour (0xff181612), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cSun);
    g.fillPath (arrow);
}

void GlowLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font GlowLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font GlowLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void GlowLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff24221d : 0xff36322a), 0, 0, juce::Colour (0xff1b1915), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (juce::Colour (over ? 0xff70664c : 0xff4a4436));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
}

void GlowLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (cText.withAlpha (0.85f));
    g.setFont (fnt (Fonts::get().label, 15.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ================= Big knob =================

BigGlowKnob::BigGlowKnob() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
{
    knob = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    rays = juce::ImageCache::getFromMemory (BinaryData::rays_lit_png, BinaryData::rays_lit_pngSize);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (360);
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
}

bool BigGlowKnob::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) < 182.0f;
}

void BigGlowKnob::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const auto rp = getRotaryParameters();
    const float pos = (float) valueToProportionOfLength (getValue());
    const float start = rp.startAngleRadians, end = rp.endAngleRadians;
    const float angle = start + pos * (end - start);
    const float act = juce::jlimit (0.0f, 1.0f, activity);

    // 1. lit sunburst up to the value (a ray is lit once the pointer reaches it)
    if (pos > 0.0005f)
    {
        const float raySpan = (end - start) / 40.0f;
        juce::Path pie;
        pie.addPieSegment (b, start - raySpan * 0.5f, angle + raySpan * 0.45f, 0.55f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pie);
        g.setOpacity (0.62f + 0.38f * act);
        g.drawImage (rays, b, juce::RectanglePlacement::stretchToFit);
    }

    // 2. illuminated ring around the knob: brightness follows GLOW, breathes with the output
    {
        const float lvl = juce::jlimit (0.0f, 1.0f, 0.15f + pos * (0.55f + 0.45f * act));
        for (auto [wd, a] : { std::pair { 14.0f, 0.06f }, { 8.0f, 0.12f }, { 4.0f, 0.30f }, { 1.6f, 0.95f } })
        {
            g.setColour (cSun.withAlpha (a * lvl));
            g.drawEllipse (juce::Rectangle<float> (ringR * 2.0f, ringR * 2.0f).withCentre (c), wd);
        }
    }

    // 3. the knob itself + luminous pointer
    const float imgHalf = knobBody / 0.80f;
    g.drawImage (knob, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;

    // warm reflection of the lit rays on the skirt and top face
    {
        const float lvl = pos * (0.7f + 0.3f * act);
        juce::ColourGradient refl (cSun.withAlpha (0.0f), c.x, c.y, cSun.withAlpha (0.16f * lvl), c.x, c.y - knobBody, true);
        refl.addColour (0.70, cSun.withAlpha (0.0f));
        refl.addColour (0.86, cSun.withAlpha (0.10f * lvl));
        g.setGradientFill (refl);
        g.fillEllipse (juce::Rectangle<float> (knobBody * 2.0f, knobBody * 2.0f).withCentre (c));
        g.setColour (cSun.withAlpha (0.10f + 0.25f * lvl));
        g.drawEllipse (juce::Rectangle<float> (topR * 2.0f, topR * 2.0f).withCentre (c), 1.2f);
    }

    juce::Path p;
    p.addRoundedRectangle (-3.0f, -topR * 0.93f, 6.0f, topR * 0.40f, 3.0f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (cSun.withAlpha (0.18f));
    g.strokePath (p, juce::PathStrokeType (9.0f), tr);
    g.setColour (cSun.withAlpha (0.4f));
    g.strokePath (p, juce::PathStrokeType (4.0f), tr);
    g.setColour (juce::Colour (0xfffff6c8));
    g.fillPath (p, tr);
}

// ================= LitButton =================

LitButton::LitButton (juce::RangedAudioParameter& p, int choiceIndex, juce::String t, juce::String s)
    : sub (std::move (s)),
      attachment (p, [this] (float v)
      {
          on = index >= 0 ? juce::roundToInt (v) == index : v > 0.5f;
          repaint();
      }, nullptr),
      index (choiceIndex), title (std::move (t))
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void LitButton::mouseDown (const juce::MouseEvent&)
{
    if (index >= 0) attachment.setValueAsCompleteGesture ((float) index);
    else attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void LitButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    if (on)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff3a8), r.getCentreX(), r.getY() + r.getHeight() * 0.3f,
                                                 juce::Colour (0xffc99a12), r.getCentreX(), r.getBottom() + r.getHeight() * 0.4f, true));
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2a22), 0, 0, juce::Colour (0xff15140f), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 8.0f);

    juce::Path top;
    top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.40f : 0.08f), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
    g.fillPath (top);
    g.setColour (on ? juce::Colour (0xfffff6c0) : cSun.withAlpha (0.16f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.2f);

    const auto ink = on ? juce::Colour (0xff221a02) : cText.withAlpha (0.78f);
    const auto subInk = on ? juce::Colour (0xff4a3a08) : cInk;
    const bool hasSub = sub.isNotEmpty();
    g.setFont (fnt (Fonts::get().heavy, index >= 0 ? 19.0f : 12.5f));
    g.setColour (ink);
    g.drawText (title, r.withTrimmedBottom (hasSub ? r.getHeight() * 0.40f : 0.0f).withTrimmedTop (hasSub ? 3.0f : 0.0f), juce::Justification::centred);
    if (hasSub)
    {
        g.setFont (fnt (Fonts::get().semi, 12.5f));
        g.setColour (subInk);
        g.drawText (sub, r.withTrimmedTop (r.getHeight() * 0.56f).withTrimmedBottom (3.0f), juce::Justification::centred);
    }
}

// ================= Panel =================

GlowPanel::GlowPanel (GlowProcessor& p) : proc (p)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    glowKnob.setTooltip ("GLOW: how much enhancement. Turn it up until it shines. Double-click = 50 %.");
    glowKnob.setDoubleClickReturnValue (true, 0.5);
    glowKnob.onValueChange = [this] { repaint (glowLcd.toNearestInt().expanded (3)); };
    addAndMakeVisible (glowKnob);
    glowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, IDs::glow, glowKnob);

    const char* ids[] = { IDs::contrast, IDs::colour, IDs::output };
    const char* tips[] = { "CONTRAST: left = dense and glued (more upward lift), right = punchy and open (transients jump out)",
                           "COLOUR: tilts the enhancement darker or brighter",
                           "Output level (after auto-gain)" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setTooltip (tips[i]);
        k.s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.s);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.s);
        k.s.setDoubleClickReturnValue (true, 0.0);
    }
    knobs[0].s.getProperties().set ("bipolar", true);
    knobs[1].s.getProperties().set ("bipolar", true);

    auto& modeParam = *proc.apvts.getParameter (IDs::mode);
    const char* modeTips[] = { "WARM: smooth body, rounded top, tube-style even harmonics",
                               "BRIGHT: lifts air and detail, exciter on the top end",
                               "FAT: weight and thump, octave harmonics make 808s heard on small speakers",
                               "LOUD: everything lifted and pushed into a clean limiter (use with auto-gain to compare)" };
    for (int i = 0; i < glowfx::numModes; ++i)
    {
        auto b = std::make_unique<LitButton> (modeParam, i, juce::String (glowfx::modeNames[(size_t) i]).toUpperCase(), juce::String (juce::CharPointer_UTF8 (modeSubs[i])));
        b->setTooltip (modeTips[i]);
        addAndMakeVisible (*b);
        modeButtons.push_back (std::move (b));
    }
    agButton = std::make_unique<LitButton> (*proc.apvts.getParameter (IDs::autoGain), -1, "AUTO GAIN", "matched");
    agButton->setTooltip ("AUTO GAIN: keeps the output as loud as the input, so you hear the enhancement, not just more volume");
    addAndMakeVisible (*agButton);

    // presets
    const auto& presets = glowfx::factoryPresets();
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

    timerCallback();
    startTimerHz (30);
}

GlowPanel::~GlowPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GlowPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    auto toDb = [] (float v) { return juce::Decibels::gainToDecibels (v, -60.0f); };
    auto ease = [] (float& v, float t, float up, float down) { v += (t > v ? up : down) * (t - v); };
    for (int b = 0; b < 3; ++b) ease (lift[b], proc.uiLift[(size_t) b].load(), 0.4f, 0.2f);
    ease (harm, proc.uiHarm.load(), 0.3f, 0.3f);
    ease (inDb, toDb (proc.uiIn.load()), 0.6f, 0.12f);
    ease (outDb, toDb (proc.uiOut.load()), 0.6f, 0.12f);
    ease (gr, proc.uiGr.load(), 0.6f, 0.15f);
    ease (ag, proc.uiAg.load(), 0.3f, 0.3f);

    const float act = juce::jlimit (0.0f, 1.0f, (outDb + 42.0f) / 40.0f);
    if (std::abs (act - glowKnob.activity) > 0.004f)
    {
        glowKnob.activity = act;
        glowKnob.repaint();
    }
    const auto agTxt = proc.currentParams().autoGain ? juce::String (ag > 0.05f ? "+" : "") + juce::String (ag, 1) + " dB" : juce::String ("off");
    if (agTxt != agButton->sub)
    {
        agButton->sub = agTxt;
        agButton->repaint();
    }
    repaint (enhLcd.toNearestInt().expanded (3));
    repaint (metLcd.toNearestInt().expanded (3));
}

void GlowPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // bloom behind lit buttons
    auto bloom = [&] (juce::Rectangle<int> r)
    {
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (cSun.withAlpha (0.06f * (float) (4 - k)));
            g.fillRoundedRectangle (r.toFloat().expanded ((float) k * 3.0f), 9.0f + (float) k * 3.0f);
        }
    };
    for (auto& b : modeButtons) if (b->isOn()) bloom (b->getBounds());
    if (agButton->isOn()) bloom (agButton->getBounds());
}

void GlowPanel::paintOverChildren (juce::Graphics& g)
{
    const auto p = proc.currentParams();

    // ---- GLOW readout
    g.setFont (fnt (Fonts::get().mono, 22.0f));
    glowText (g, juce::String (juce::roundToInt (p.glow * 100.0f)) + " %", glowLcd, juce::Justification::centred, cSun);

    // ---- small knob values
    g.setFont (fnt (Fonts::get().mono, 13.0f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        g.setColour (cText.withAlpha (0.9f));
        const float vy = i < 2 ? smallC[i].y + 42.0f : smallC[i].y + 42.0f;
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (70.0f, 14.0f).withCentre ({ smallC[i].x, vy }), juce::Justification::centred);
    }

    // ---- ENHANCE LCD: per-band gain the engine is applying + harmonics
    {
        const auto r = enhLcd.reduced (9.0f, 8.0f);
        const char* names[] = { "LOW", "MID", "HIGH" };
        const float rowH = r.getHeight() / 4.0f;
        for (int b = 0; b < 4; ++b)
        {
            auto row = juce::Rectangle<float> (r.getX(), r.getY() + rowH * (float) b, r.getWidth(), rowH);
            g.setFont (fnt (Fonts::get().label, 12.5f));
            g.setColour (cSun.withAlpha (0.75f));
            g.drawText (b < 3 ? names[b] : "HARM", row.removeFromLeft (34.0f), juce::Justification::centredLeft);
            auto val = row.removeFromRight (44.0f);
            const auto bar = row.reduced (2.0f, rowH * 0.30f);
            g.setFont (fnt (Fonts::get().mono, 12.5f));
            if (b < 3)
            {
                const float db = lift[b];
                lcdBar (g, bar, 6.0f / 18.0f, juce::jlimit (0.0f, 1.0f, (db + 6.0f) / 18.0f), cSun);
                glowText (g, (db > 0.05f ? "+" : "") + juce::String (db, 1), val, juce::Justification::centredRight, cSun);
            }
            else
            {
                lcdBar (g, bar, 0.0f, juce::jlimit (0.0f, 1.0f, harm / 1.4f), cSun);
                glowText (g, juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, harm / 1.4f) * 100.0f)) + "%", val, juce::Justification::centredRight, cSun);
            }
        }
    }

    // ---- LEVELS LCD
    {
        const auto r = metLcd.reduced (9.0f, 8.0f);
        const float rowH = r.getHeight() / 4.0f;
        const char* names[] = { "IN", "OUT", "LIMIT", "MATCH" };
        for (int b = 0; b < 4; ++b)
        {
            auto row = juce::Rectangle<float> (r.getX(), r.getY() + rowH * (float) b, r.getWidth(), rowH);
            g.setFont (fnt (Fonts::get().label, 12.5f));
            g.setColour (cSun.withAlpha (0.75f));
            g.drawText (names[b], row.removeFromLeft (38.0f), juce::Justification::centredLeft);
            auto val = row.removeFromRight (46.0f);
            const auto bar = row.reduced (2.0f, rowH * 0.30f);
            g.setFont (fnt (Fonts::get().mono, 12.5f));
            if (b < 2)
            {
                const float db = b == 0 ? inDb : outDb;
                lcdBar (g, bar, 0.0f, juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f), cSun, 44.0f / 48.0f);
                glowText (g, db <= -59.0f ? juce::String ("-inf") : juce::String (db, 1), val, juce::Justification::centredRight, db > -0.5f ? cHot : cSun);
            }
            else if (b == 2)
            {
                lcdBar (g, bar, 0.0f, juce::jlimit (0.0f, 1.0f, gr / 12.0f), cSun);
                glowText (g, "-" + juce::String (gr, 1), val, juce::Justification::centredRight, cSun);
            }
            else
            {
                const bool on = p.autoGain;
                lcdBar (g, bar, 0.5f, on ? juce::jlimit (0.0f, 1.0f, 0.5f + ag / 24.0f) : 0.5f, cSun);
                glowText (g, on ? (ag > 0.05f ? "+" : "") + juce::String (ag, 1) : juce::String ("OFF"), val, juce::Justification::centredRight,
                          on ? cSun : cInk);
            }
        }
    }
}

void GlowPanel::resized()
{
    glowKnob.setBounds (juce::Rectangle<float> (litHalf * 2.0f, litHalf * 2.0f).withCentre (knobC).toNearestInt());
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre (smallC[i]).toNearestInt());
    for (size_t i = 0; i < modeButtons.size(); ++i)
        modeButtons[i]->setBounds (modeX, modeY[i], modeW, modeH);
    agButton->setBounds (agRect);

    prevButton.setBounds (490, 30, 30, 30);
    presetBox.setBounds (524, 30, 250, 30);
    nextButton.setBounds (778, 30, 30, 30);
}

// ================= Editor =================

GlowEditor::GlowEditor (GlowProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (GlowPanel::baseW * 3 / 4, GlowPanel::baseH * 3 / 4, GlowPanel::baseW * 2, GlowPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) GlowPanel::baseW / GlowPanel::baseH);
    setSize (GlowPanel::baseW, GlowPanel::baseH);
}

void GlowEditor::resized()
{
    panel.setBounds (0, 0, GlowPanel::baseW, GlowPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / GlowPanel::baseW));
}
