#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText { 0xffece6e4 };
const juce::Colour cInk  { 0xff928a8c };
const juce::Colour cAcc  { 0xffff3d7f };   // electric pink-red
const juce::Colour cCool { 0xff8fb4ff };   // negative amounts (softer / tighter)
const juce::Colour cHot  { 0xffffb03a };   // clipper working
const juce::Colour cDeep { 0xff1a1618 };

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
const juce::Point<float> attC { 132.0f, 208.0f }, susC { 708.0f, 208.0f };
constexpr float bigKnob = 168.0f, smallKnob = 64.0f;
constexpr float smallY = 424.0f;
float smallX (int i) { return 302.0f + 114.0f * (float) i; }
const juce::Rectangle<int> screenInner { 261, 101, 318, 186 };
const juce::Rectangle<int> lookRect { 32, 372, 84, 80 }, clipRect { 126, 372, 84, 80 };
const juce::Rectangle<float> attLcd { 72.0f, 310.0f, 120.0f, 30.0f }, susLcd { 648.0f, 310.0f, 120.0f, 30.0f };
// display plot areas, local to screenInner (panel ENV_AREA / GAIN_AREA minus the screen origin)
const juce::Rectangle<float> envArea { 11.0f, 21.0f, 296.0f, 100.0f }, gainArea { 11.0f, 129.0f, 296.0f, 42.0f };
constexpr float envFloorDb = -48.0f, gainRangeDb = 18.0f;

const char* bandNames[] = { "FULL", "ABOVE", "BELOW" };

juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

juce::String signedPct (double v)
{
    const int p = juce::roundToInt (v * 100.0);
    return (p > 0 ? "+" : "") + juce::String (p) + " %";
}

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c)
{
    g.setColour (c.withAlpha (0.18f));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (dx, dy), j);
    g.setColour (c);
    g.drawText (t, r, j);
}
} // namespace

// ================= Look =================

SnapLook::SnapLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cAcc);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff16060c));
    setColour (juce::PopupMenu::headerTextColourId, cAcc);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cAcc.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void SnapLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f;
    const bool big = R > 60.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);
    const bool negative = bipolar && pos < 0.5f;
    const auto on = ! s.isEnabled() ? cInk : (negative && big ? cCool : cAcc);

    if (big)
    {
        const float arcR = R - 23.0f;
        const float a0 = bipolar ? (start + end) * 0.5f : start;
        const float lo = juce::jmin (a0, angle), hi = juce::jmax (a0, angle);
        if (hi > lo + 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, lo, hi, true);
            g.setColour (on.withAlpha (0.20f));
            g.strokePath (arc, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (on);
            g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
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
    }

    const float body = big ? R - 30.0f : 22.0f;
    const float imgHalf = body / 0.80f;
    g.drawImage (big ? knobBig : knobSmall, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    const float pw = big ? 4.0f : 2.6f;
    juce::Path p;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.94f, pw, topR * 0.46f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (on.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (big ? 4.0f : 2.5f), tr);
    g.setColour (on.brighter (0.25f));
    g.fillPath (p, tr);
}

void SnapLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff6a5258) : juce::Colour (0xff4a3e42));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0c0d), 0, 0, juce::Colour (0xff181416), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cAcc);
    g.fillPath (arrow);
}

void SnapLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font SnapLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font SnapLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void SnapLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cAcc.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffff7aa6) : juce::Colour (down ? 0xff241e20 : 0xff352f31), 0, 0,
                                             on ? juce::Colour (0xffd8235f) : juce::Colour (0xff1b1718), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (on ? juce::Colour (0xffffc0d4) : juce::Colour (over ? 0xff6e5a60 : 0xff4a3e42));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void SnapLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colour (0xff22060f) : cText.withAlpha (0.82f));
    g.setFont (fnt (Fonts::get().label, 14.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ================= GlowButton =================

GlowButton::GlowButton (juce::RangedAudioParameter& p, juce::String t, juce::Colour h)
    : attachment (p, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr), title (std::move (t)), hue (h)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GlowButton::mouseDown (const juce::MouseEvent&) { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

void GlowButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    if (on)
        g.setGradientFill (juce::ColourGradient (hue.brighter (0.35f), r.getCentreX(), r.getY() + r.getHeight() * 0.35f,
                                                 hue.darker (0.55f), r.getCentreX(), r.getBottom() + r.getHeight() * 0.3f, true));
    else
        g.setGradientFill (juce::ColourGradient (hue.withMultipliedBrightness (0.20f).withMultipliedSaturation (0.6f), 0, 0,
                                                 hue.withMultipliedBrightness (0.08f).withMultipliedSaturation (0.5f), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 9.0f);

    juce::Path top;
    top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.38f : 0.10f), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
    g.fillPath (top);
    g.setColour (on ? hue.brighter (0.6f).withAlpha (0.9f) : hue.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.2f);

    const auto ink = on ? juce::Colour (0xff22060f) : hue.withAlpha (0.6f);
    g.setFont (fnt (Fonts::get().heavy, title.length() > 6 ? 13.0f : 17.0f));
    if (on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText (title, r.withTrimmedBottom (20.0f).translated (0.0f, 1.0f), juce::Justification::centred);
    }
    g.setColour (ink);
    g.drawText (title, r.withTrimmedBottom (20.0f), juce::Justification::centred);
    g.setFont (fnt (Fonts::get().label, 12.0f));
    g.setColour (warn && on ? juce::Colour (0xff3a0710) : ink);
    g.drawText (on ? subOn : subOff, r.withTrimmedTop (r.getHeight() - 26.0f), juce::Justification::centred);
}

// ================= SnapDisplay =================

SnapDisplay::SnapDisplay (SnapProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    tick();
}

void SnapDisplay::tick()
{
    const auto& e = proc.getEngine();
    const int w = e.histWrite.load (std::memory_order_acquire);
    for (int i = 0; i < points; ++i)
    {
        const int idx = ((w - (points - 1 - i)) % snapfx::historySize + snapfx::historySize) % snapfx::historySize;
        in[(size_t) i] = e.histIn[(size_t) idx].load (std::memory_order_relaxed);
        out[(size_t) i] = e.histOut[(size_t) idx].load (std::memory_order_relaxed);
        gain[(size_t) i] = e.histGain[(size_t) idx].load (std::memory_order_relaxed);
    }
    const float target = juce::jlimit (0.0f, 1.0f, proc.uiInput.load() * 3.0f);
    activity += (target > activity ? 0.5f : 0.06f) * (target - activity);
    clipFlash = proc.uiClip.load() > 0.002f ? 1.0f : clipFlash * 0.85f;
    repaint();
}

void SnapDisplay::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();

    // ---- readout strip
    g.setFont (fnt (Fonts::get().mono, 13.5f));
    const auto strip = juce::Rectangle<float> (12.0f, 1.0f, (float) getWidth() - 24.0f, 15.0f);
    const float gdb = proc.uiGainDb.load();
    const juce::String gTxt = juce::String ("GAIN ") + (gdb > 0.05f ? "+" : "") + juce::String (gdb, 1) + " dB";
    glowText (g, gTxt, strip, juce::Justification::centredLeft, gdb < -0.05f ? cCool : cAcc);
    const juce::String bandTxt = p.band == snapfx::Band::full ? juce::String ("FULL BAND")
                               : juce::String (p.band == snapfx::Band::above ? "ABOVE " : "BELOW ") + hzText (p.splitHz);
    glowText (g, bandTxt, strip, juce::Justification::centred, cAcc.withAlpha (0.85f));
    const int lat = proc.uiLatency.load();
    glowText (g, p.lookahead ? "LA " + juce::String (1000.0 * lat / juce::jmax (1.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0), 1) + " ms"
                             : juce::String ("0 LATENCY"),
              strip, juce::Justification::centredRight, p.lookahead ? cAcc : cAcc.withAlpha (0.45f));

    // ---- envelopes (dB scale)
    const float dx = envArea.getWidth() / (float) (points - 1);
    auto yEnv = [&] (float lin)
    {
        const float d = lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -120.0f;
        const float t = juce::jlimit (0.0f, 1.0f, (d - envFloorDb) / -envFloorDb);
        return envArea.getBottom() - t * envArea.getHeight();
    };
    juce::Path inFill, outFill, outLine, inLine;
    inFill.startNewSubPath (envArea.getX(), envArea.getBottom());
    outFill.startNewSubPath (envArea.getX(), envArea.getBottom());
    for (int i = 0; i < points; ++i)
    {
        const float x = envArea.getX() + dx * (float) i;
        const float yi = yEnv (in[(size_t) i]);
        inFill.lineTo (x, yi);
        if (i == 0) inLine.startNewSubPath (x, yi); else inLine.lineTo (x, yi);
        const float yo = yEnv (out[(size_t) i]);
        outFill.lineTo (x, yo);
        if (i == 0) outLine.startNewSubPath (x, yo); else outLine.lineTo (x, yo);
    }
    inFill.lineTo (envArea.getRight(), envArea.getBottom());
    outFill.lineTo (envArea.getRight(), envArea.getBottom());
    inFill.closeSubPath();
    outFill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (cText.withAlpha (0.30f), 0, envArea.getY(), cText.withAlpha (0.06f), 0, envArea.getBottom(), false));
    g.fillPath (inFill);
    g.setGradientFill (juce::ColourGradient (cAcc.withAlpha (0.42f), 0, envArea.getY(), cAcc.withAlpha (0.04f), 0, envArea.getBottom(), false));
    g.fillPath (outFill);
    g.setColour (cAcc.withAlpha (0.25f));
    g.strokePath (outLine, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cAcc.brighter (0.2f));
    g.strokePath (outLine, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cText.withAlpha (0.55f));
    g.strokePath (inLine, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // dB scale + legend
    g.setFont (fnt (Fonts::get().label, 10.5f));
    for (int k = 1; k < 4; ++k)
    {
        const float yy = envArea.getY() + envArea.getHeight() * (float) k / 4.0f;
        g.setColour (cInk.withAlpha (0.7f));
        g.drawText (juce::String (-12 * k), juce::Rectangle<float> (envArea.getRight() - 24.0f, yy - 11.0f, 22.0f, 10.0f), juce::Justification::centredRight);
    }
    auto legend = [&] (float x, juce::Colour col, const juce::String& t)
    {
        g.setColour (col);
        g.fillRoundedRectangle (x, envArea.getY() + 4.0f, 10.0f, 4.0f, 1.5f);
        g.setColour (cText.withAlpha (0.75f));
        g.drawText (t, juce::Rectangle<float> (x + 13.0f, envArea.getY(), 60.0f, 12.0f), juce::Justification::centredLeft);
    };
    g.setColour (juce::Colour (0xc80c090b));
    g.fillRoundedRectangle (envArea.getX() + 1.0f, envArea.getY() - 1.0f, 118.0f, 14.0f, 3.0f);
    legend (envArea.getX() + 4.0f, cText.withAlpha (0.7f), "ORIGINAL");
    legend (envArea.getX() + 64.0f, cAcc, "SHAPED");

    // ---- gain lane: bars up = boost, down = cut
    const float mid = gainArea.getCentreY(), half = gainArea.getHeight() * 0.5f - 1.0f;
    for (int i = 0; i < points; ++i)
    {
        // weight by the signal level: gain riding in near-silence is inaudible and would only clutter the lane
        const float lvl = in[(size_t) i] > 1.0e-6f ? 20.0f * std::log10 (in[(size_t) i]) : -120.0f;
        const float gv = juce::jlimit (-gainRangeDb, gainRangeDb, gain[(size_t) i]) * juce::jlimit (0.0f, 1.0f, (lvl + 60.0f) / 15.0f);
        if (std::abs (gv) < 0.05f) continue;
        const float hgt = half * std::abs (gv) / gainRangeDb;
        const float x = gainArea.getX() + dx * (float) i;
        g.setColour ((gv > 0.0f ? cAcc : cCool).withAlpha (0.85f));
        g.fillRect (juce::Rectangle<float> (x - 0.5f, gv > 0.0f ? mid - hgt : mid, juce::jmax (1.0f, dx), hgt));
    }
    g.setFont (fnt (Fonts::get().label, 10.5f));
    g.setColour (cInk.withAlpha (0.8f));
    g.drawText ("+" + juce::String ((int) gainRangeDb), juce::Rectangle<float> (gainArea.getRight() - 24.0f, gainArea.getY(), 22.0f, 10.0f), juce::Justification::centredRight);
    g.drawText ("-" + juce::String ((int) gainRangeDb), juce::Rectangle<float> (gainArea.getRight() - 24.0f, gainArea.getBottom() - 10.0f, 22.0f, 10.0f), juce::Justification::centredRight);
    g.drawText ("GAIN", juce::Rectangle<float> (gainArea.getX() + 3.0f, gainArea.getY(), 40.0f, 10.0f), juce::Justification::centredLeft);

    // live "now" marker
    if (activity > 0.03f)
    {
        const float xr = envArea.getRight();
        g.setColour (cAcc.withAlpha (0.6f * activity));
        g.fillRect (juce::Rectangle<float> (xr - 1.0f, envArea.getY(), 1.5f, gainArea.getBottom() - envArea.getY()));
    }

    if (p.clip && clipFlash > 0.05f)
    {
        g.setFont (fnt (Fonts::get().label, 11.0f));
        auto chip = juce::Rectangle<float> (envArea.getRight() - 56.0f, envArea.getY() + 1.0f, 30.0f, 12.0f);
        g.setColour (cHot.withAlpha (clipFlash));
        g.fillRoundedRectangle (chip, 3.0f);
        g.setColour (juce::Colour (0xff1a0e02));
        g.drawText ("CLIP", chip, juce::Justification::centred);
    }
}

// ================= Panel =================

SnapPanel::SnapPanel (SnapProcessor& p)
    : proc (p), display (p),
      lookButton (*p.apvts.getParameter (IDs::lookahead), "LOOK", cAcc),
      clipButton (*p.apvts.getParameter (IDs::clip), "CLIP", cAcc)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    auto setupKnob = [this] (Knob& k, const char* id, const char* tip)
    {
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setTooltip (tip);
        k.s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.s);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.s);
    };
    setupKnob (attackKnob, IDs::attack, "Attack: + makes the onset of every hit punchier, - softens it. Double-click = 0.");
    setupKnob (sustainKnob, IDs::sustain, "Sustain: + brings up the body / tail of every hit, - tightens it. Double-click = 0.");
    for (auto* k : { &attackKnob, &sustainKnob })
    {
        k->s.getProperties().set ("bipolar", true);
        k->s.setDoubleClickReturnValue (true, 0.0);
    }

    const char* ids[] = { IDs::split, IDs::speed, IDs::ceiling, IDs::mix, IDs::output };
    const char* tips[] = { "Split frequency: with ABOVE / BELOW only that part of the spectrum is shaped",
                           "Detector speed: higher = snappier, catches shorter transients; lower = smoother, broader shaping",
                           "Clipper ceiling: maximum output peak when CLIP is on",
                           "Dry / shaped blend (parallel shaping). 0 % = untouched",
                           "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
        setupKnob (knobs[i], ids[i], tips[i]);
    knobs[4].s.setDoubleClickReturnValue (true, 0.0);
    knobs[1].s.setDoubleClickReturnValue (true, 0.5);

    addAndMakeVisible (display);

    bandAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::band), [this] (float v)
    {
        bandState = juce::roundToInt (v);
        refreshSelectors();
    }, nullptr);
    const char* bandTips[] = { "Shape the whole signal", "Shape only the part above the split frequency (e.g. the click of a kick)",
                               "Shape only the part below the split frequency (e.g. the body of an 808)" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = bandButtons[(size_t) i];
        b.setButtonText (bandNames[i]);
        b.onClick = [this, i] { bandAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip (bandTips[i]);
        addAndMakeVisible (b);
    }
    bandAttachment->sendInitialUpdate();

    lookButton.subOn = "AHEAD 4 ms";
    lookButton.subOff = "AHEAD OFF";
    clipButton.subOn = "ACTIVE";
    clipButton.subOff = "OFF";
    lookButton.setTooltip ("Lookahead: the detector sees 4 ms ahead, so even the very first milliseconds of a hit are shaped (adds 4 ms latency, reported to the host)");
    clipButton.setTooltip ("Output clipper: soft-clips the shaped signal at the ceiling, great for loud punchy drums");
    addAndMakeVisible (lookButton);
    addAndMakeVisible (clipButton);

    const auto& presets = snapfx::factoryPresets();
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

    knobs[2].s.setEnabled (clipButton.isOn());
    refreshSelectors();
    startTimerHz (30);
}

SnapPanel::~SnapPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SnapPanel::refreshSelectors()
{
    for (int i = 0; i < 3; ++i)
        bandButtons[(size_t) i].setToggleState (i == bandState, juce::dontSendNotification);
    knobs[0].s.setEnabled (bandState != 0);
    repaint();
}

void SnapPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    const bool w = proc.uiClip.load() > 0.002f;
    if (w != clipButton.warn) { clipButton.warn = w; clipButton.repaint(); }
    knobs[2].s.setEnabled (clipButton.isOn());
    display.tick();
}

void SnapPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    auto bloom = [&] (juce::Rectangle<int> r, juce::Colour c)
    {
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (c.withAlpha (0.07f * (float) (4 - k)));
            g.fillRoundedRectangle (r.toFloat().expanded ((float) k * 3.0f), 10.0f + (float) k * 3.0f);
        }
    };
    if (lookButton.isOn()) bloom (lookRect, cAcc);
    if (clipButton.isOn()) bloom (clipRect, clipButton.warn ? cHot : cAcc);
}

void SnapPanel::paintOverChildren (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    g.setFont (fnt (Fonts::get().mono, 21.0f));
    glowText (g, signedPct (p.attack), attLcd, juce::Justification::centred, p.attack < -0.004f ? cCool : cAcc);
    glowText (g, signedPct (p.sustain), susLcd, juce::Justification::centred, p.sustain < -0.004f ? cCool : cAcc);

    g.setFont (fnt (Fonts::get().mono, 13.0f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        g.setColour (cText.withAlpha (k.s.isEnabled() ? 0.9f : 0.4f));
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (100.0f, 14.0f).withCentre ({ smallX ((int) i), 465.0f }), juce::Justification::centred);
    }
}

void SnapPanel::resized()
{
    attackKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (attC).toNearestInt());
    sustainKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (susC).toNearestInt());
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ smallX ((int) i), smallY }).toNearestInt());

    display.setBounds (screenInner);
    for (int i = 0; i < 3; ++i)
        bandButtons[(size_t) i].setBounds (300 + 82 * i, 302, 76, 26);

    lookButton.setBounds (lookRect);
    clipButton.setBounds (clipRect);

    prevButton.setBounds (490, 30, 30, 30);
    presetBox.setBounds (524, 30, 250, 30);
    nextButton.setBounds (778, 30, 30, 30);
}

// ================= Editor =================

SnapEditor::SnapEditor (SnapProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (SnapPanel::baseW * 3 / 4, SnapPanel::baseH * 3 / 4, SnapPanel::baseW * 2, SnapPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) SnapPanel::baseW / SnapPanel::baseH);
    setSize (SnapPanel::baseW, SnapPanel::baseH);
}

void SnapEditor::resized()
{
    panel.setBounds (0, 0, SnapPanel::baseW, SnapPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / SnapPanel::baseW));
}
