#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText { 0xffe2eae8 };
const juce::Colour cInk  { 0xff808c8a };
const juce::Colour cMint { 0xff7dffd8 };
const juce::Colour cTeal { 0xff2fc7b0 };
const juce::Colour cHot  { 0xffff5a4e };
const juce::Colour cIce  { 0xffd4fff4 };
const juce::Colour cDeep { 0xff141818 };

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
const juce::Point<float> timeC { 132.0f, 238.0f }, fbC { 728.0f, 238.0f };
constexpr float bigKnob = 168.0f, smallKnob = 64.0f;
constexpr float smallY = 456.0f;
float smallX (int i) { return 254.0f + 68.0f * (float) i; }
const juce::Rectangle<int> screenInner { 253, 139, 354, 192 };
const juce::Rectangle<int> freezeRect { 34, 406, 144, 84 };
const juce::Rectangle<float> timeLcd { 68.0f, 326.0f, 128.0f, 28.0f }, fbLcd { 664.0f, 326.0f, 128.0f, 28.0f };

juce::String msText (double ms)
{
    if (ms < 100.0) return juce::String (ms, 1) + " ms";
    if (ms < 10000.0) return juce::String (juce::roundToInt (ms)) + " ms";
    return juce::String (ms / 1000.0, 2) + " s";
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

GhostLook::GhostLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cMint);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff06140f));
    setColour (juce::PopupMenu::headerTextColourId, cMint);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cMint.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void GhostLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f;
    const bool big = R > 60.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float danger = (float) (double) s.getProperties().getWithDefault ("danger", 2.0); // normalised pos where the hot zone starts
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cMint : cInk;

    if (big)
    {
        const float arcR = R - 23.0f;
        const float a0 = bipolar ? (start + end) * 0.5f : start;
        auto arcSeg = [&] (float from, float to, juce::Colour col)
        {
            if (to <= from) return;
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, from, to, true);
            g.setColour (col.withAlpha (0.20f));
            g.strokePath (arc, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (col);
            g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        const float lo = juce::jmin (a0, angle), hi = juce::jmax (a0, angle);
        const float dangerAngle = start + danger * (end - start);
        arcSeg (lo, juce::jmin (hi, dangerAngle), on);
        arcSeg (juce::jmax (lo, dangerAngle), hi, cHot);
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

    // photoreal knob (pre-rendered); only the pointer rotates
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

void GhostLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff4e6460) : juce::Colour (0xff364240));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0e0e), 0, 0, juce::Colour (0xff151a1a), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cMint);
    g.fillPath (arrow);
}

void GhostLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font GhostLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font GhostLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void GhostLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool dim = (bool) b.getProperties().getWithDefault ("dim", false);
    const bool mode = (bool) b.getProperties().getWithDefault ("mode", false);
    const float rad = mode ? 6.0f : 4.5f;
    if (on && ! dim)
    {
        g.setColour (cMint.withAlpha (0.16f));
        g.fillRoundedRectangle (r.expanded (2.5f), rad + 1.5f);
    }
    juce::Colour top, bot;
    if (on)
    {
        top = dim ? juce::Colour (0xff3d5a53) : juce::Colour (0xffa6ffe4);
        bot = dim ? juce::Colour (0xff22322f) : juce::Colour (0xff22b994);
    }
    else
    {
        top = juce::Colour (down ? 0xff1d2121 : 0xff2e3434);
        bot = juce::Colour (0xff171a1a);
    }
    g.setGradientFill (juce::ColourGradient (top, 0, 0, bot, 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? (dim ? juce::Colour (0xff5a7a72) : juce::Colour (0xffd8fff2)) : juce::Colour (over ? 0xff56625f : 0xff39423f));
    g.drawRoundedRectangle (r, rad, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
    if (mode)
    {
        // status LED on the left of every mode key
        const auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() });
        if (on)
        {
            g.setColour (juce::Colour (0xff04140e).withAlpha (0.8f));
            g.fillEllipse (led.expanded (1.0f));
            g.setColour (juce::Colours::white);
            g.fillEllipse (led.reduced (1.0f));
        }
        else
        {
            g.setColour (juce::Colour (0xff0c0f0f));
            g.fillEllipse (led);
            g.setColour (cMint.withAlpha (over ? 0.35f : 0.18f));
            g.drawEllipse (led, 1.0f);
        }
    }
}

void GhostLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const bool on = b.getToggleState();
    const bool dim = (bool) b.getProperties().getWithDefault ("dim", false);
    const bool mode = (bool) b.getProperties().getWithDefault ("mode", false);
    juce::Colour c = on ? (dim ? cText.withAlpha (0.55f) : juce::Colour (0xff04140f)) : cText.withAlpha (dim ? 0.35f : (over ? 0.95f : 0.78f));
    g.setColour (c);
    g.setFont (fnt (Fonts::get().label, mode ? 16.0f : 14.5f));
    auto r = b.getLocalBounds();
    if (mode) r.removeFromLeft (14);
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

// ================= FreezeButton =================

FreezeButton::FreezeButton (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FreezeButton::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void FreezeButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto hue = cMint;
    if (on)
        g.setGradientFill (juce::ColourGradient (cIce, r.getCentreX(), r.getY() + r.getHeight() * 0.35f,
                                                 hue.darker (0.55f), r.getCentreX(), r.getBottom() + r.getHeight() * 0.3f, true));
    else
        g.setGradientFill (juce::ColourGradient (hue.withMultipliedBrightness (0.20f).withMultipliedSaturation (0.6f), 0, 0,
                                                 hue.withMultipliedBrightness (0.07f).withMultipliedSaturation (0.5f), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 9.0f);

    juce::Path top;
    top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.38f : 0.10f), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
    g.fillPath (top);
    g.setColour (on ? cIce.withAlpha (0.9f) : hue.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.2f);

    const auto ink = on ? juce::Colour (0xff062019) : hue.withAlpha (0.6f);
    // snowflake glyph
    const auto c = juce::Point<float> (r.getX() + 30.0f, r.getCentreY());
    g.setColour (ink);
    for (int k = 0; k < 3; ++k)
    {
        const float a = juce::MathConstants<float>::pi * (float) k / 3.0f;
        const juce::Point<float> d (std::sin (a) * 14.0f, std::cos (a) * 14.0f);
        g.drawLine ({ c - d, c + d }, 2.2f);
        for (float sgn : { -1.0f, 1.0f })
        {
            const auto tip = c + d * (sgn * 0.62f);
            const juce::Point<float> n (std::cos (a) * 4.0f, -std::sin (a) * 4.0f);
            g.drawLine ({ tip, tip + d * (sgn * 0.25f) + n }, 1.6f);
            g.drawLine ({ tip, tip + d * (sgn * 0.25f) - n }, 1.6f);
        }
    }
    g.setFont (fnt (Fonts::get().heavy, 21.0f));
    g.drawText ("FREEZE", r.withTrimmedLeft (52.0f).withTrimmedBottom (14.0f), juce::Justification::centredLeft);
    g.setFont (fnt (Fonts::get().label, 12.0f));
    g.drawText (on ? "LOOP HELD" : "READY", r.withTrimmedLeft (53.0f).withTrimmedTop (30.0f), juce::Justification::centredLeft);
}

// ================= EchoDisplay =================

EchoDisplay::EchoDisplay (GhostDelayProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    activity = juce::jlimit (0.0f, 1.0f, proc.uiWet.load() * 4.0f + proc.uiInput.load() * 2.0f);
}

void EchoDisplay::tick()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double dt = lastTick == 0 ? 0.0 : (double) (now - lastTick);
    lastTick = now;
    const float target = juce::jlimit (0.0f, 1.0f, proc.uiWet.load() * 4.0f + proc.uiInput.load() * 2.0f);
    activity += (target > activity ? 0.5f : 0.08f) * (target - activity);
    cursorMs += dt;
    clockMs += dt;
    repaint();
}

void EchoDisplay::paint (juce::Graphics& g)
{
    using namespace ghostdelay;
    const auto p = proc.currentParams();
    const double T = delayMsFor (p);
    double fL, fR;
    ratioFactors (p.lrRatio, fL, fR);
    const double tL = T * fL, tR = T * fR;
    const auto area = juce::Rectangle<float> (14.0f, 21.0f, 330.0f, 154.0f);   // matches the baked grid
    const float midY = area.getCentreY();
    const Mode mode = p.mode;
    const auto& pat = rhythmPattern (p.pattern);

    // ---- readout strip
    g.setFont (fnt (Fonts::get().mono, 13.5f));
    const auto strip = juce::Rectangle<float> (12.0f, 1.0f, (float) getWidth() - 24.0f, 15.0f);
    const juce::String timeTxt = p.sync ? juce::String (divisionNames[(size_t) p.division]) + "  " + msText (T) : msText (T);
    glowText (g, timeTxt, strip, juce::Justification::centredLeft, cMint);
    glowText (g, modeNames[(size_t) mode], strip, juce::Justification::centred, cMint.withAlpha (0.85f));
    const juce::String bpmTxt = juce::String (p.bpm, 1) + (proc.uiHostTempo.load() ? " BPM" : " BPM*");
    glowText (g, bpmTxt, strip, juce::Justification::centredRight, p.sync ? cMint : cMint.withAlpha (0.4f));

    // ---- window: smallest "nice" span that shows at least ~4 repeats (two pattern loops in RHYTHM)
    const double need = mode == Mode::rhythm ? T * pat.loop * 2.1 : std::max (tL, tR) * 4.2;
    double windowMs = 250.0;
    while (windowMs < need && windowMs < 64000.0) windowMs *= 2.0;
    const float axisX = area.getX() + 18.0f, axisW = area.getRight() - 6.0f - axisX;
    auto xFor = [&] (double ms) { return axisX + (float) (ms / windowMs) * axisW; };

    g.setColour (cMint.withAlpha (0.26f));
    g.drawHorizontalLine (juce::roundToInt (midY), area.getX(), area.getRight());
    g.setFont (fnt (Fonts::get().label, 11.0f));
    g.setColour (cInk.withAlpha (0.8f));
    g.drawText ("L", juce::Rectangle<float> (area.getX() + 2.0f, area.getY() + 2.0f, 12.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("R", juce::Rectangle<float> (area.getX() + 2.0f, area.getBottom() - 14.0f, 12.0f, 12.0f), juce::Justification::centredLeft);

    const float lpAmt = juce::jlimit (0.0f, 1.0f, std::log (20000.0f / juce::jmax (1000.0f, p.highCutHz)) / std::log (20.0f));
    const float hpAmt = juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (20.0f, p.lowCutHz) / 20.0f) / std::log (100.0f));
    const float tone = 1.0f - 0.10f * lpAmt - 0.06f * hpAmt - 0.05f * p.age;
    const float fbEff = p.freeze ? 1.0f : juce::jmin (1.0f, p.feedback * tone);
    const auto baseCol = p.freeze ? cIce : cMint.interpolatedWith (cTeal, lpAmt * 0.7f);
    const float bright = 0.55f + 0.45f * activity;
    const float halfH = area.getHeight() * 0.5f - 6.0f;
    const float bwBase = juce::jlimit (2.5f, 7.0f, (float) (T / windowMs) * area.getWidth() * 0.22f);

    auto bar = [&] (float x, float amp, bool up, juce::Colour col, float w)
    {
        if (amp < 0.004f || x > area.getRight()) return;
        const float hgt = juce::jmax (2.0f, halfH * juce::jmin (1.0f, amp));
        auto rr = up ? juce::Rectangle<float> (x - w * 0.5f, midY - hgt, w, hgt) : juce::Rectangle<float> (x - w * 0.5f, midY, w, hgt);
        g.setColour (col.withAlpha (0.13f * bright));
        g.fillRoundedRectangle (rr.expanded (3.5f, 2.0f), 4.0f);
        g.setGradientFill (juce::ColourGradient (col.brighter (0.3f).withAlpha (bright), rr.getCentreX(), up ? rr.getY() : rr.getBottom(),
                                                 col.darker (0.4f).withAlpha (bright * 0.85f), rr.getCentreX(), midY, false));
        g.fillRoundedRectangle (rr, 1.5f);
    };
    // smooth hump (DIFFUSE): gaussian of the given width (ms) and height
    auto hump = [&] (double centreMs, double widthMs, float amp, bool up, juce::Colour col)
    {
        if (amp < 0.004f) return;
        juce::Path path;
        const float x0 = xFor (centreMs - widthMs * 2.5), x1 = xFor (centreMs + widthMs * 2.5);
        path.startNewSubPath (x0, midY);
        for (int k = 0; k <= 40; ++k)
        {
            const double t = centreMs - widthMs * 2.5 + widthMs * 5.0 * k / 40.0;
            const double e = std::exp (-0.5 * std::pow ((t - centreMs) / widthMs, 2.0));
            const float yy = halfH * juce::jmin (1.0f, amp) * (float) e;
            path.lineTo (xFor (t), up ? midY - yy : midY + yy);
        }
        path.lineTo (x1, midY);
        path.closeSubPath();
        g.saveState();
        g.reduceClipRegion (area.toNearestInt());
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.75f * bright), 0, up ? midY - halfH * amp : midY + halfH * amp,
                                                 col.withAlpha (0.08f), 0, midY, false));
        g.fillPath (path);
        g.setColour (col.withAlpha (0.8f * bright));
        g.strokePath (path, juce::PathStrokeType (1.0f));
        g.restoreState();
    };

    // dry hit
    bar (xFor (0.0), 1.0f, true, cText.withAlpha (0.75f), 4.0f);
    bar (xFor (0.0), 1.0f, false, cText.withAlpha (0.75f), 4.0f);

    const float wob = (float) (p.modDepth * 0.006 * windowMs);
    switch (mode)
    {
        case Mode::reverse:
        {
            // each grain plays the previous one backwards: a swell that ends where the next grain begins
            for (int side = 0; side < 2; ++side)
            {
                const double tg = side == 0 ? tL : tR;
                float amp = 1.0f;
                for (int n = 1; n <= 48; ++n)
                {
                    const double a = tg * n, b = tg * (n + 1);
                    if (a > windowMs) break;
                    juce::Path sw;
                    const float xa = xFor (a), xb = juce::jmin (area.getRight(), xFor (b) - 1.5f);
                    const float hh = halfH * amp * 0.95f;
                    const float sgn = side == 0 ? -1.0f : 1.0f;
                    sw.startNewSubPath (xa, midY);
                    for (int k = 0; k <= 24; ++k)
                    {
                        const float t = (float) k / 24.0f;
                        sw.lineTo (xa + (xb - xa) * t, midY + sgn * hh * std::pow (t, 1.8f));
                    }
                    sw.lineTo (xb, midY);
                    sw.closeSubPath();
                    g.setGradientFill (juce::ColourGradient (baseCol.withAlpha (0.05f), xa, midY, baseCol.withAlpha (0.8f * bright), xb, midY + sgn * hh, false));
                    g.fillPath (sw);
                    g.setColour (baseCol.withAlpha (bright));
                    g.drawLine (xb, midY, xb, midY + sgn * hh, 2.0f);
                    amp *= fbEff;
                    if (amp < 0.01f) break;
                }
            }
            // grain play-head
            if (activity > 0.03f)
            {
                const float x = xFor (tL * (1.0 + proc.uiRevPhase.load()));
                g.setColour (cIce.withAlpha (0.7f * activity));
                g.fillRect (juce::Rectangle<float> (x, area.getY(), 1.2f, area.getHeight() * 0.5f));
            }
            break;
        }
        case Mode::diffuse:
        {
            for (int side = 0; side < 2; ++side)
            {
                const double tg = side == 0 ? tL : tR;
                float amp = 1.0f;
                for (int n = 1; n <= 48; ++n)
                {
                    const double t = tg * n;
                    if (t - tg * 0.5 > windowMs) break;
                    const double width = std::min (tg * 0.45, 9.0 + 16.0 * n);
                    hump (t, width, amp / (1.0f + 0.35f * (float) (n - 1)) + 0.05f * amp, side == 0, baseCol);
                    amp *= fbEff;
                    if (amp < 0.01f) break;
                }
            }
            break;
        }
        case Mode::rhythm:
        {
            float loopAmp = 1.0f;
            for (int loop = 0; loop < 64; ++loop)
            {
                const double base = loop * pat.loop * T;
                if (base > windowMs) break;
                for (int k = 0; k < pat.numTaps; ++k)
                {
                    const double t = base + pat.times[(size_t) k] * T;
                    if (t > windowMs) break;
                    const float a = loopAmp * pat.gains[(size_t) k];
                    const float pan = pat.pans[(size_t) k];
                    const auto col = baseCol.withRotatedHue (0.03f * (float) k);
                    bar (xFor (t), a * juce::jmin (1.0f, 1.0f - pan), true, col, bwBase);
                    bar (xFor (t), a * juce::jmin (1.0f, 1.0f + pan), false, col, bwBase);
                }
                loopAmp *= fbEff;
                if (loopAmp < 0.01f) break;
            }
            break;
        }
        case Mode::ghost:
        {
            const int step = p.ghostPitch == 0 ? 12 : (p.ghostPitch == 1 ? 7 : -12);
            g.setFont (fnt (Fonts::get().mono, 10.5f));
            for (int side = 0; side < 2; ++side)
            {
                const double tg = side == 0 ? tL : tR;
                float amp = 1.0f;
                for (int n = 1; n <= 48; ++n)
                {
                    const double t = tg * n;
                    if (t > windowMs) break;
                    const auto col = baseCol.withRotatedHue ((float) (n - 1) * (step > 0 ? 0.05f : -0.05f));
                    bar (xFor (t), amp, side == 0, col, bwBase);
                    if (side == 0 && n > 1 && amp > 0.06f)
                    {
                        const int semis = (n - 1) * step;
                        const float hgt = juce::jmax (2.0f, halfH * amp);
                        g.setColour (col.withAlpha (0.9f * bright));
                        g.drawText ((semis > 0 ? "+" : "") + juce::String (semis), juce::Rectangle<float> (xFor (t) - 16.0f, midY - hgt - 13.0f, 32.0f, 11.0f),
                                    juce::Justification::centred);
                    }
                    amp *= fbEff;
                    if (amp < 0.01f) break;
                }
            }
            break;
        }
        case Mode::tape:
        case Mode::bbd:
        case Mode::digital:
        default:
        {
            const float quant = mode == Mode::digital && p.age > 0.05f ? halfH / std::exp2 (4.5f - 3.0f * p.age) : 0.0f;
            for (int side = 0; side < 2; ++side)
            {
                const double tg = side == 0 ? tL : tR;
                float amp = 1.0f;
                for (int n = 1; n <= 64; ++n)
                {
                    double t = tg * n;
                    if (mode == Mode::tape)
                        t += wob * std::sin (clockMs * 0.001 * juce::MathConstants<double>::twoPi * p.modRate * 0.5 + n * 1.7 + side);
                    if (t > windowMs) break;
                    float w = bwBase;
                    auto col = baseCol;
                    if (mode == Mode::bbd) { w *= 1.0f + 0.25f * (float) (n - 1) * (0.3f + p.age); col = col.withMultipliedSaturation (0.85f); }
                    if (mode == Mode::tape) col = col.interpolatedWith (juce::Colour (0xffe8f5c8), juce::jmin (0.5f, 0.08f * (float) n * p.age));
                    if (mode == Mode::digital) w = juce::jmax (2.0f, w * 0.7f);
                    bar (xFor (t), amp, side == 0, col, w);
                    if (quant > 0.0f)
                    {
                        // stepped bit-depth look
                        const float hgt = halfH * amp;
                        g.setColour (juce::Colour (0xff050908).withAlpha (0.85f));
                        for (float yy = quant; yy < hgt; yy += quant)
                            g.fillRect (juce::Rectangle<float> (xFor (t) - w * 0.5f, side == 0 ? midY - yy : midY + yy - 1.0f, w, 1.0f));
                    }
                    amp *= fbEff;
                    if (amp < 0.004f) break;
                }
            }
            break;
        }
    }

    // moving cursor while audio flows
    if (activity > 0.03f)
    {
        const float cx = xFor (std::fmod (cursorMs, windowMs));
        g.setGradientFill (juce::ColourGradient (cMint.withAlpha (0.0f), cx - 18.0f, 0, cMint.withAlpha (0.2f * activity), cx, 0, false));
        g.fillRect (juce::Rectangle<float> (cx - 18.0f, area.getY(), 18.0f, area.getHeight()));
        g.setColour (cMint.withAlpha (0.55f * activity));
        g.fillRect (juce::Rectangle<float> (cx, area.getY(), 1.0f, area.getHeight()));
    }

    // ---- status chips + window span
    g.setFont (fnt (Fonts::get().label, 11.0f));
    float cx = area.getX();
    auto chip = [&] (const juce::String& t, juce::Colour col, bool filled)
    {
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 10.0f;
        auto r = juce::Rectangle<float> (cx, 177.0f, w, 12.0f);
        if (filled) { g.setColour (col); g.fillRoundedRectangle (r, 3.0f); g.setColour (juce::Colour (0xff06140f)); }
        else { g.setColour (col.withAlpha (0.7f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
        g.drawText (t, r, juce::Justification::centred);
        cx += w + 5.0f;
    };
    if (p.freeze) chip ("FREEZE", cIce, true);
    if (p.feedback > 1.0f && ! p.freeze) chip ("SELF-OSC", cHot, false);
    if (mode == Mode::ghost) chip ("PITCH " + juce::String (ghostPitchNames[(size_t) p.ghostPitch]), cMint, false);
    if (mode == Mode::rhythm) chip (patternNames[(size_t) p.pattern], cMint, false);
    if (std::abs (p.lrRatio) > 0.005f && mode != Mode::rhythm)
        chip ("L:R " + juce::String (fL, 2) + " : " + juce::String (fR, 2), cMint, false);
    const float dg = proc.uiDuck.load();
    if (p.duck > 0.001f)
        chip (dg < 0.98f ? "DUCK " + juce::String (juce::Decibels::gainToDecibels (dg), 1) + " dB" : juce::String ("DUCK"), cMint, false);
    g.setColour (cInk);
    g.drawText ("SPAN " + (windowMs >= 1000.0 ? juce::String (windowMs / 1000.0, windowMs < 10000.0 ? 1 : 0) + " s" : msText (windowMs)),
                juce::Rectangle<float> (area.getRight() - 70.0f, 177.0f, 70.0f, 12.0f), juce::Justification::centredRight);
}

// ================= Panel =================

GhostPanel::GhostPanel (GhostDelayProcessor& p)
    : proc (p), display (p), freezeButton (*p.apvts.getParameter (IDs::freeze))
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
    setupKnob (timeKnob, IDs::time, "Delay time in milliseconds (free mode). Changing it glides the pitch like tape.");
    setupKnob (divKnob, IDs::division, "Delay time as a note value locked to the host tempo, 1/64 to 2 bars (T = triplet, D = dotted)");
    setupKnob (fbKnob, IDs::feedback, "Number of repeats. Above 100 % the echoes build into guarded self-oscillation.");
    fbKnob.s.getProperties().set ("danger", 1.0 / 1.1);
    divKnob.s.setVelocityBasedMode (false);
    divKnob.s.setMouseDragSensitivity (360);

    const char* ids[] = { IDs::lrRatio, IDs::lowCut, IDs::highCut, IDs::modDepth, IDs::modRate, IDs::age, IDs::duck, IDs::mix, IDs::output };
    const char* tips[] = { "L/R ratio: left of centre shortens the left time, right of centre the right time (down to half) for wide, offset echoes",
                           "Low cut inside the feedback loop: every repeat gets thinner (OFF at minimum)",
                           "High cut inside the feedback loop: every repeat gets darker (OFF at maximum)",
                           "Modulation depth: wow & flutter (TAPE), chorus (BBD), drift (DIGITAL / GHOST / RHYTHM), smear motion (DIFFUSE)",
                           "Modulation rate",
                           "Age: tape wear and hiss, BBD darkness and grit, DIGITAL bit / rate crush, GHOST / DIFFUSE darkening",
                           "Ducks the echoes while the input plays; they bloom in the gaps",
                           "0 = dry, 50 % = dry + full echo (insert), 100 % = echo only (send)",
                           "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
        setupKnob (knobs[i], ids[i], tips[i]);
    knobs[0].s.getProperties().set ("bipolar", true);
    knobs[0].s.setDoubleClickReturnValue (true, 0.0);
    knobs[8].s.setDoubleClickReturnValue (true, 0.0);

    addAndMakeVisible (display);

    setupSelector (modeSel, IDs::mode, { "TAPE", "BBD ANALOG", "DIGITAL", "GHOST", "REVERSE", "DIFFUSE", "RHYTHM" },
                   { "Saturated tape echo: wow, flutter, head-gap darkening, hiss with AGE",
                     "Bucket-brigade analog echo: chorused, darker the longer the time",
                     "Clean, exact digital repeats; AGE crushes bits and sample rate",
                     "Pitch-shifted feedback: every repeat climbs or falls by the GHOST PITCH interval",
                     "Tempo-locked grains played backwards (grain = delay time)",
                     "Allpass-smeared echoes that melt into ambience",
                     "Multi-tap rhythm patterns: dotted, triplet or swing" });
    for (auto& b : modeSel.buttons) b->getProperties().set ("mode", true);
    setupSelector (syncSel, IDs::sync, { "SYNC", "FREE" }, { "Lock the delay time to the host tempo", "Set the delay time in milliseconds" }, true);
    setupSelector (pitchSel, IDs::ghostPitch, { "+12", "+7", "-12" },
                   { "GHOST mode: each repeat one octave up", "GHOST mode: each repeat a fifth up", "GHOST mode: each repeat one octave down" });
    setupSelector (patternSel, IDs::pattern, { "DOTTED", "TRIPLET", "SWING" },
                   { "RHYTHM mode: dotted taps bouncing left / right", "RHYTHM mode: triplet taps left, right, centre", "RHYTHM mode: swung long-short pairs" });

    freezeButton.setTooltip ("FREEZE: holds the current echoes in an endless loop (no new input)");
    addAndMakeVisible (freezeButton);

    // presets
    const auto& presets = ghostdelay::factoryPresets();
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
    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    refreshSelectors();
    startTimerHz (30);
}

GhostPanel::~GhostPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GhostPanel::setupSelector (Selector& sel, const char* paramId, const juce::StringArray& names, const juce::StringArray& tips, bool invertBool)
{
    sel.attachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (paramId), [this, &sel, invertBool] (float v)
    {
        sel.state = invertBool ? (v > 0.5f ? 0 : 1) : juce::roundToInt (v);
        refreshSelectors();
    }, nullptr);
    for (int i = 0; i < names.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (names[i]);
        b->setTooltip (tips[i]);
        b->onClick = [&sel, i, invertBool] { sel.attachment->setValueAsCompleteGesture (invertBool ? (i == 0 ? 1.0f : 0.0f) : (float) i); };
        addAndMakeVisible (*b);
        sel.buttons.push_back (std::move (b));
    }
    sel.attachment->sendInitialUpdate();
}

void GhostPanel::refreshSelectors()
{
    for (auto* sel : { &modeSel, &syncSel, &pitchSel, &patternSel })
        for (size_t i = 0; i < sel->buttons.size(); ++i)
            sel->buttons[i]->setToggleState ((int) i == sel->state, juce::dontSendNotification);
    const bool ghost = modeSel.state == (int) ghostdelay::Mode::ghost;
    const bool rhythm = modeSel.state == (int) ghostdelay::Mode::rhythm;
    for (auto& b : pitchSel.buttons) { b->getProperties().set ("dim", ! ghost); b->repaint(); }
    for (auto& b : patternSel.buttons) { b->getProperties().set ("dim", ! rhythm); b->repaint(); }
    divKnob.s.setVisible (syncSel.state == 0);
    timeKnob.s.setVisible (syncSel.state == 1);
    knobs[0].s.setEnabled (! rhythm); // RHYTHM is a mono-in pattern: the L/R ratio has no effect
    repaint();
}

void GhostPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    display.tick();
    repaint (timeLcd.toNearestInt().expanded (2));
}

void GhostPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    if (freezeButton.isOn())
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (cMint.withAlpha (0.07f * (float) (4 - k)));
            g.fillRoundedRectangle (freezeRect.toFloat().expanded ((float) k * 3.0f), 10.0f + (float) k * 3.0f);
        }
}

void GhostPanel::paintOverChildren (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const double ms = ghostdelay::delayMsFor (p);
    if (p.sync)
    {
        g.setFont (fnt (Fonts::get().mono, 20.0f));
        glowText (g, ghostdelay::divisionNames[(size_t) p.division], timeLcd.withTrimmedLeft (9.0f), juce::Justification::centredLeft, cMint);
        g.setFont (fnt (Fonts::get().mono, 13.0f));
        glowText (g, msText (ms), timeLcd.withTrimmedRight (9.0f).withTrimmedTop (3.0f), juce::Justification::centredRight, cMint.withAlpha (0.6f));
    }
    else
    {
        g.setFont (fnt (Fonts::get().mono, 20.0f));
        glowText (g, msText (ms), timeLcd, juce::Justification::centred, cMint);
    }
    const float fb = p.feedback;
    g.setFont (fnt (Fonts::get().mono, 20.0f));
    glowText (g, juce::String (juce::roundToInt (fb * 100.0f)) + " %", fbLcd.withTrimmedLeft (9.0f), fb > 1.0f ? juce::Justification::centredLeft : juce::Justification::centred,
              fb > 1.0f ? cHot : cMint);
    if (fb > 1.0f)
    {
        g.setFont (fnt (Fonts::get().label, 12.0f));
        glowText (g, "SELF-OSC", fbLcd.withTrimmedRight (9.0f), juce::Justification::centredRight, cHot);
    }

    g.setFont (fnt (Fonts::get().mono, 12.5f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        g.setColour (cText.withAlpha (k.s.isEnabled() ? 0.9f : 0.35f));
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (68.0f, 14.0f).withCentre ({ smallX ((int) i), 496.0f }), juce::Justification::centred);
    }
}

void GhostPanel::resized()
{
    timeKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (timeC).toNearestInt());
    divKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (timeC).toNearestInt());
    fbKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (fbC).toNearestInt());
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ smallX ((int) i), smallY }).toNearestInt());

    display.setBounds (screenInner);
    for (size_t i = 0; i < modeSel.buttons.size(); ++i)
        modeSel.buttons[i]->setBounds (26 + 116 * (int) i, 94, 112, 30);
    syncSel.buttons[0]->setBounds (76, 362, 58, 22);
    syncSel.buttons[1]->setBounds (138, 362, 58, 22);
    for (size_t i = 0; i < pitchSel.buttons.size(); ++i)
        pitchSel.buttons[i]->setBounds (254 + 44 * (int) i, 344, 40, 22);
    for (size_t i = 0; i < patternSel.buttons.size(); ++i)
        patternSel.buttons[i]->setBounds (426 + 62 * (int) i, 344, 58, 22);

    freezeButton.setBounds (freezeRect);

    prevButton.setBounds (500, 28, 30, 30);
    presetBox.setBounds (534, 28, 260, 30);
    nextButton.setBounds (798, 28, 30, 30);
}

// ================= Editor =================

GhostDelayEditor::GhostDelayEditor (GhostDelayProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (GhostPanel::baseW * 3 / 4, GhostPanel::baseH * 3 / 4, GhostPanel::baseW * 2, GhostPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) GhostPanel::baseW / GhostPanel::baseH);
    setSize (GhostPanel::baseW, GhostPanel::baseH);
}

void GhostDelayEditor::resized()
{
    panel.setBounds (0, 0, GhostPanel::baseW, GhostPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / GhostPanel::baseW));
}
