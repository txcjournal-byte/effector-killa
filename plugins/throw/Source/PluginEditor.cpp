#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText  { 0xffece6dc };
const juce::Colour cInk   { 0xff928a7e };
const juce::Colour cAmber { 0xffffb22e };
const juce::Colour cHot   { 0xffff5a2a };
const juce::Colour cIce   { 0xff9fdcff };
const juce::Colour cDeep  { 0xff1a1816 };

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
const juce::Point<float> timeC { 130.0f, 196.0f }, fbC { 690.0f, 196.0f };
constexpr float bigKnob = 168.0f, smallKnob = 64.0f;
constexpr float smallY = 410.0f;
float smallX (int i) { return 264.0f + 72.0f * (float) i; }
const juce::Rectangle<int> screenInner { 257, 97, 306, 180 };
const juce::Rectangle<int> throwRect { 28, 356, 120, 84 }, freezeRect { 156, 356, 48, 84 };
const juce::Rectangle<float> timeLcd { 70.0f, 296.0f, 120.0f, 30.0f }, fbLcd { 630.0f, 296.0f, 120.0f, 30.0f };

const char* modeNames[] = { "STEREO", "PING-PONG", "MONO" };

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

ThrowLook::ThrowLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cAmber);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff140e06));
    setColour (juce::PopupMenu::headerTextColourId, cAmber);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cAmber.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void ThrowLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f;
    const bool big = R > 60.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float danger = (float) (double) s.getProperties().getWithDefault ("danger", 2.0); // normalised pos where the hot zone starts
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cAmber : cInk;

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

void ThrowLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff6a5e4c) : juce::Colour (0xff4a4238));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0d0c), 0, 0, juce::Colour (0xff181614), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cAmber);
    g.fillPath (arrow);
}

void ThrowLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font ThrowLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font ThrowLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void ThrowLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cAmber.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffffc560) : juce::Colour (down ? 0xff24211e : 0xff35312c), 0, 0,
                                             on ? juce::Colour (0xffd98a14) : juce::Colour (0xff1b1917), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (on ? juce::Colour (0xffffe0a0) : juce::Colour (over ? 0xff6e6352 : 0xff4a4238));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void ThrowLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colour (0xff1c1206) : cText.withAlpha (0.82f));
    g.setFont (fnt (Fonts::get().label, 14.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void ThrowLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.26f);
    if (b.getToggleState())
    {
        g.setColour (cAmber.withAlpha (0.3f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (b.getToggleState() ? cAmber : juce::Colour (0xff36312b));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (led, 1.0f);
    g.setColour (over ? cText : cText.withAlpha (0.85f));
    g.setFont (fnt (Fonts::get().label, 14.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (5), juce::Justification::centredLeft);
}

// ================= GlowButton =================

GlowButton::GlowButton (juce::RangedAudioParameter& p, Style st)
    : style (st), attachment (p, [this] (float v)
      {
          on = v > 0.5f;
          if (! on) latched = false;
          repaint();
      }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GlowButton::mouseDown (const juce::MouseEvent&)
{
    if (style == Style::freezeButton)
    {
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
        return;
    }
    if (on)
    {
        // a second click releases a latched throw
        latched = false;
        ignoreUp = true;
        attachment.setValueAsCompleteGesture (0.0f);
        return;
    }
    ignoreUp = false;
    downTime = juce::Time::getMillisecondCounter();
    attachment.setValueAsCompleteGesture (1.0f);
}

void GlowButton::mouseUp (const juce::MouseEvent&)
{
    if (style == Style::freezeButton || ignoreUp || ! on) return;
    if (juce::Time::getMillisecondCounter() - downTime > 300)
        attachment.setValueAsCompleteGesture (0.0f);   // held: momentary
    else
    {
        latched = true;                                 // tapped: latch
        repaint();
    }
}

void GlowButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const bool isThrow = style == Style::throwButton;
    const auto hue = isThrow ? cAmber : cIce;
    const float dim = (isThrow && ! armed) ? 0.55f : 1.0f;

    if (on)
    {
        juce::ColourGradient lit (hue.brighter (0.35f), r.getCentreX(), r.getY() + r.getHeight() * 0.35f,
                                  hue.darker (isThrow ? 0.55f : 0.35f), r.getCentreX(), r.getBottom() + r.getHeight() * 0.3f, true);
        g.setGradientFill (lit);
    }
    else
        g.setGradientFill (juce::ColourGradient (hue.withMultipliedBrightness (0.20f * dim).withMultipliedSaturation (0.7f), 0, 0,
                                                 hue.withMultipliedBrightness (0.08f * dim).withMultipliedSaturation (0.6f), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 9.0f);

    // glass: inner top highlight + edge
    juce::Path top;
    top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.38f : 0.10f), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
    g.fillPath (top);
    g.setColour (on ? hue.brighter (0.6f).withAlpha (0.9f) : hue.withAlpha (0.22f * dim));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.2f);

    const auto ink = on ? juce::Colour (0xff1e1206) : hue.withAlpha (0.55f * dim);
    if (isThrow)
    {
        g.setFont (fnt (Fonts::get().heavy, 25.0f));
        if (on)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawText ("THROW", r.withTrimmedBottom (18.0f).translated (0.0f, 1.0f), juce::Justification::centred);
        }
        g.setColour (ink);
        g.drawText ("THROW", r.withTrimmedBottom (18.0f), juce::Justification::centred);
        g.setFont (fnt (Fonts::get().label, 12.0f));
        const juce::String sub = on ? (latched ? "LATCHED" : "HOLDING") : (armed ? "READY" : "SEND ALWAYS ON");
        g.drawText (sub, r.withTrimmedTop (r.getHeight() - 26.0f), juce::Justification::centred);
    }
    else
    {
        // snowflake-ish hold glyph
        const auto c = juce::Point<float> (r.getCentreX(), r.getY() + 30.0f);
        g.setColour (ink);
        for (int k = 0; k < 3; ++k)
        {
            const float a = juce::MathConstants<float>::pi * (float) k / 3.0f;
            const juce::Point<float> d (std::sin (a) * 11.0f, std::cos (a) * 11.0f);
            g.drawLine ({ c - d, c + d }, 2.0f);
        }
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));
        g.setFont (fnt (Fonts::get().label, 12.5f));
        g.drawText ("FREEZE", r.withTrimmedTop (r.getHeight() - 28.0f), juce::Justification::centred);
    }
}

// ================= TapDisplay =================

TapDisplay::TapDisplay (ThrowProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    activity = juce::jlimit (0.0f, 1.0f, proc.uiWet.load() * 4.0f + proc.uiInput.load() * 2.0f);
}

void TapDisplay::tick()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double dt = lastTick == 0 ? 0.0 : (double) (now - lastTick);
    lastTick = now;
    const float target = juce::jlimit (0.0f, 1.0f, proc.uiWet.load() * 4.0f + proc.uiInput.load() * 2.0f);
    activity += (target > activity ? 0.5f : 0.08f) * (target - activity);
    cursorMs += dt;
    repaint();
}

void TapDisplay::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const double delayMs = throwfx::delayMsFor (p);
    const auto area = juce::Rectangle<float> (14.0f, 21.0f, 282.0f, 144.0f);   // matches the baked grid
    const float midY = area.getCentreY();

    // ---- readout strip
    g.setFont (fnt (Fonts::get().mono, 13.5f));
    const auto strip = juce::Rectangle<float> (12.0f, 1.0f, (float) getWidth() - 24.0f, 15.0f);
    const juce::String timeTxt = p.sync ? juce::String (throwfx::divisionNames[(size_t) juce::jlimit (0, throwfx::numDivisions - 1, p.division)]) + "  " + msText (delayMs)
                                        : msText (delayMs);
    glowText (g, timeTxt, strip, juce::Justification::centredLeft, cAmber);
    glowText (g, modeNames[(int) p.mode], strip, juce::Justification::centred, cAmber.withAlpha (0.85f));
    const juce::String bpmTxt = juce::String (p.bpm, 1) + (proc.uiHostTempo.load() ? " BPM" : " BPM*");
    glowText (g, bpmTxt, strip, juce::Justification::centredRight, p.sync ? cAmber : cAmber.withAlpha (0.4f));

    // ---- window: smallest "nice" span that shows at least ~4 repeats
    double windowMs = 250.0;
    while (windowMs < delayMs * 4.2 && windowMs < 64000.0) windowMs *= 2.0;
    const float axisX = area.getX() + 18.0f, axisW = area.getRight() - 6.0f - axisX;
    auto xFor = [&] (double ms) { return axisX + (float) (ms / windowMs) * axisW; };

    // centre line + channel tags
    g.setColour (cAmber.withAlpha (0.28f));
    g.drawHorizontalLine (juce::roundToInt (midY), area.getX(), area.getRight());
    g.setFont (fnt (Fonts::get().label, 11.0f));
    g.setColour (cInk.withAlpha (0.8f));
    g.drawText ("L", juce::Rectangle<float> (area.getX() + 2.0f, area.getY() + 2.0f, 12.0f, 12.0f), juce::Justification::centredLeft);
    g.drawText ("R", juce::Rectangle<float> (area.getX() + 2.0f, area.getBottom() - 14.0f, 12.0f, 12.0f), juce::Justification::centredLeft);

    // tone loss per repeat (for the bar colour / decay shape)
    const float lpAmt = juce::jlimit (0.0f, 1.0f, std::log (20000.0f / juce::jmax (1000.0f, p.highCutHz)) / std::log (20.0f));
    const float hpAmt = juce::jlimit (0.0f, 1.0f, std::log (juce::jmax (20.0f, p.lowCutHz) / 20.0f) / std::log (100.0f));
    const float tone = 1.0f - 0.10f * lpAmt - 0.06f * hpAmt - 0.04f * p.drive;
    const float fbEff = p.freeze ? 1.0f : juce::jmin (1.0f, p.feedback * tone);
    const auto barCol = p.freeze ? cIce : cAmber.interpolatedWith (juce::Colour (0xffd8621c), lpAmt * 0.8f);
    const float bright = 0.55f + 0.45f * activity;
    const float halfH = area.getHeight() * 0.5f - 6.0f;
    const bool sendLive = ! p.throwOnly || p.throwOn;

    auto bar = [&] (float x, float amp, bool up, juce::Colour col, float w)
    {
        if (amp < 0.004f) return;
        const float hgt = juce::jmax (2.0f, halfH * amp);
        auto rr = up ? juce::Rectangle<float> (x - w * 0.5f, midY - hgt, w, hgt) : juce::Rectangle<float> (x - w * 0.5f, midY, w, hgt);
        g.setColour (col.withAlpha (0.13f * bright));
        g.fillRoundedRectangle (rr.expanded (3.5f, 2.0f), 4.0f);
        g.setGradientFill (juce::ColourGradient (col.brighter (0.3f).withAlpha (bright), rr.getCentreX(), up ? rr.getY() : rr.getBottom(),
                                                 col.darker (0.4f).withAlpha (bright * 0.85f), rr.getCentreX(), midY, false));
        g.fillRoundedRectangle (rr, 1.5f);
    };

    // dry hit
    bar (xFor (0.0), 1.0f, true, cText.withAlpha (sendLive ? 0.75f : 0.3f), 4.0f);
    bar (xFor (0.0), 1.0f, false, cText.withAlpha (sendLive ? 0.75f : 0.3f), 4.0f);

    // echo taps
    juce::Path envUp, envDown;
    envUp.startNewSubPath (xFor (0.0), midY - halfH);
    envDown.startNewSubPath (xFor (0.0), midY + halfH);
    float amp = 1.0f;
    const float wL = 1.0f, wR = 1.0f;
    for (int n = 1; n <= 64; ++n)
    {
        const double t = delayMs * n;
        if (t > windowMs) break;
        const float x = xFor (t);
        const float a = amp * (0.35f + 0.65f * (sendLive || p.freeze ? 1.0f : 0.5f));
        const float bw = juce::jlimit (2.5f, 7.0f, (float) (delayMs / windowMs) * area.getWidth() * 0.22f);
        switch (p.mode)
        {
            case throwfx::Mode::pingPong:
                bar (x, a, (n % 2) == 1, barCol, bw);
                break;
            case throwfx::Mode::mono:
                bar (x, a, true, barCol, bw);
                bar (x, a, false, barCol, bw);
                break;
            case throwfx::Mode::stereo:
            default:
                bar (x, a * wL, true, barCol, bw);
                bar (x, a * wR, false, barCol, bw);
                break;
        }
        const bool upSide = p.mode != throwfx::Mode::pingPong || (n % 2) == 1;
        const bool downSide = p.mode != throwfx::Mode::pingPong || (n % 2) == 0;
        if (upSide) envUp.lineTo (x, midY - juce::jmax (2.0f, halfH * a));
        if (downSide) envDown.lineTo (x, midY + juce::jmax (2.0f, halfH * a));
        amp *= fbEff;
        if (amp < 0.004f) break;
    }
    {
        juce::Path dashed;
        const float dashes[] = { 3.0f, 4.0f };
        g.setColour (barCol.withAlpha (0.35f * bright));
        for (auto* e : { &envUp, &envDown })
        {
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, *e, dashes, 2);
            g.fillPath (dashed);
        }
    }

    // moving cursor while audio flows
    if (activity > 0.03f)
    {
        const float cx = xFor (std::fmod (cursorMs, windowMs));
        g.setGradientFill (juce::ColourGradient (cAmber.withAlpha (0.0f), cx - 18.0f, 0, cAmber.withAlpha (0.22f * activity), cx, 0, false));
        g.fillRect (juce::Rectangle<float> (cx - 18.0f, area.getY(), 18.0f, area.getHeight()));
        g.setColour (cAmber.withAlpha (0.6f * activity));
        g.fillRect (juce::Rectangle<float> (cx, area.getY(), 1.0f, area.getHeight()));
    }

    // ---- status chips + window span
    g.setFont (fnt (Fonts::get().label, 11.0f));
    float cx = area.getX();
    auto chip = [&] (const juce::String& t, juce::Colour col, bool filled)
    {
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 10.0f;
        auto r = juce::Rectangle<float> (cx, 167.0f, w, 12.0f);
        if (filled) { g.setColour (col); g.fillRoundedRectangle (r, 3.0f); g.setColour (juce::Colour (0xff140e06)); }
        else { g.setColour (col.withAlpha (0.7f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
        g.drawText (t, r, juce::Justification::centred);
        cx += w + 5.0f;
    };
    if (p.throwOnly) chip (p.throwOn ? "THROWING" : "THROW-ONLY", cAmber, p.throwOn);
    if (p.freeze) chip ("FREEZE", cIce, true);
    if (p.feedback > 1.0f && ! p.freeze) chip ("SELF-OSC", cHot, false);
    const float dg = proc.uiDuck.load();
    if (p.duck > 0.001f)
        chip (dg < 0.98f ? "DUCK " + juce::String (juce::Decibels::gainToDecibels (dg), 1) + " dB" : juce::String ("DUCK"), cAmber, false);
    g.setColour (cInk);
    g.drawText ("SPAN " + (windowMs >= 1000.0 ? juce::String (windowMs / 1000.0, windowMs < 10000.0 ? 1 : 0) + " s" : msText (windowMs)), juce::Rectangle<float> (area.getRight() - 90.0f, 167.0f, 90.0f, 12.0f), juce::Justification::centredRight);
}

// ================= Panel =================

ThrowPanel::ThrowPanel (ThrowProcessor& p)
    : proc (p), taps (p),
      throwButton (*p.apvts.getParameter (IDs::throwOn), GlowButton::Style::throwButton),
      freezeButton (*p.apvts.getParameter (IDs::freeze), GlowButton::Style::freezeButton)
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
    setupKnob (timeKnob, IDs::time, "Delay time in milliseconds (free mode). Changing it bends the pitch like tape.");
    setupKnob (divKnob, IDs::division, "Delay time as a note value, locked to the host tempo (T = triplet, D = dotted)");
    setupKnob (fbKnob, IDs::feedback, "Number of repeats. Above 100 % the echo builds into controlled self-oscillation.");
    fbKnob.s.getProperties().set ("danger", 1.0 / 1.1);
    divKnob.s.setVelocityBasedMode (false);
    divKnob.s.setMouseDragSensitivity (300);

    const char* ids[] = { IDs::drive, IDs::wobble, IDs::lowCut, IDs::highCut, IDs::duck, IDs::width, IDs::mix, IDs::output };
    const char* tips[] = { "Tape saturation and head-gap darkening on every repeat",
                           "Wow & flutter: tape speed wobble on the echoes",
                           "Low cut inside the feedback loop: thins every repeat (OFF at minimum)",
                           "High cut inside the feedback loop: darkens every repeat (OFF at maximum)",
                           "Ducks the echoes while the vocal plays, they bloom in the gaps",
                           "Stereo width of the echoes (0 = mono, 200 = extra wide)",
                           "0 = dry, 50 % = dry + full echo (insert / throw), 100 % = echo only",
                           "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
        setupKnob (knobs[i], ids[i], tips[i]);
    knobs[5].s.getProperties().set ("bipolar", true);
    knobs[5].s.setDoubleClickReturnValue (true, 1.0);
    knobs[7].s.setDoubleClickReturnValue (true, 0.0);

    addAndMakeVisible (taps);

    // sync selector
    syncAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::sync), [this] (float v)
    {
        syncState = v > 0.5f ? 1 : 0;
        refreshSelectors();
    }, nullptr);
    const char* syncNames[] = { "SYNC", "FREE" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = syncButtons[(size_t) i];
        b.setButtonText (syncNames[i]);
        b.onClick = [this, i] { syncAttachment->setValueAsCompleteGesture (i == 0 ? 1.0f : 0.0f); };
        b.setTooltip (i == 0 ? "Lock the delay time to the host tempo" : "Set the delay time in milliseconds");
        addAndMakeVisible (b);
    }
    // mode selector
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::mode), [this] (float v)
    {
        modeState = juce::roundToInt (v);
        refreshSelectors();
    }, nullptr);
    const char* modeTips[] = { "Left and right echo independently", "Echoes bounce left / right", "Single centred echo" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.onClick = [this, i] { modeAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip (modeTips[i]);
        addAndMakeVisible (b);
    }
    syncAttachment->sendInitialUpdate();
    modeAttachment->sendInitialUpdate();

    throwButton.setTooltip ("THROW: sends the vocal into the delay. Click = latch, hold = momentary. Use with THROW-ONLY SEND for end-of-phrase throws.");
    freezeButton.setTooltip ("FREEZE: holds the current echoes in an endless loop");
    addAndMakeVisible (throwButton);
    addAndMakeVisible (freezeButton);
    throwOnlyToggle.setTooltip ("On: only audio played while THROW is engaged reaches the delay. Off: everything is echoed.");
    addAndMakeVisible (throwOnlyToggle);
    throwOnlyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::throwOnly, throwOnlyToggle);
    throwButton.armed = throwOnlyToggle.getToggleState();

    // presets
    const auto& presets = throwfx::factoryPresets();
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

    refreshSelectors();
    startTimerHz (30);
}

ThrowPanel::~ThrowPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ThrowPanel::refreshSelectors()
{
    syncButtons[0].setToggleState (syncState == 1, juce::dontSendNotification);
    syncButtons[1].setToggleState (syncState == 0, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setToggleState (i == modeState, juce::dontSendNotification);
    divKnob.s.setVisible (syncState == 1);
    timeKnob.s.setVisible (syncState == 0);
    repaint();
}

void ThrowPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    const bool armed = throwOnlyToggle.getToggleState();
    if (armed != throwButton.armed)
    {
        throwButton.armed = armed;
        throwButton.repaint();
    }
    taps.tick();
    repaint (timeLcd.toNearestInt().expanded (2));
}

void ThrowPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // bloom behind lit buttons
    auto bloom = [&] (juce::Rectangle<int> r, juce::Colour c)
    {
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (c.withAlpha (0.07f * (float) (4 - k)));
            g.fillRoundedRectangle (r.toFloat().expanded ((float) k * 3.0f), 10.0f + (float) k * 3.0f);
        }
    };
    if (throwButton.isOn()) bloom (throwRect, cAmber);
    if (freezeButton.isOn()) bloom (freezeRect, cIce);
}

void ThrowPanel::paintOverChildren (juce::Graphics& g)
{
    // ---- LCD readouts under the big dials
    const auto p = proc.currentParams();
    const double ms = throwfx::delayMsFor (p);
    if (p.sync)
    {
        g.setFont (fnt (Fonts::get().mono, 21.0f));
        glowText (g, throwfx::divisionNames[(size_t) juce::jlimit (0, throwfx::numDivisions - 1, p.division)], timeLcd.withTrimmedLeft (9.0f),
                  juce::Justification::centredLeft, cAmber);
        g.setFont (fnt (Fonts::get().mono, 13.0f));
        glowText (g, msText (ms), timeLcd.withTrimmedRight (9.0f).withTrimmedTop (3.0f), juce::Justification::centredRight, cAmber.withAlpha (0.6f));
    }
    else
    {
        g.setFont (fnt (Fonts::get().mono, 21.0f));
        glowText (g, msText (ms), timeLcd, juce::Justification::centred, cAmber);
    }
    const float fb = p.feedback;
    g.setFont (fnt (Fonts::get().mono, 21.0f));
    glowText (g, juce::String (juce::roundToInt (fb * 100.0f)) + " %", fbLcd.withTrimmedLeft (8.0f), fb > 1.0f ? juce::Justification::centredLeft : juce::Justification::centred,
              fb > 1.0f ? cHot : cAmber);
    if (fb > 1.0f)
    {
        g.setFont (fnt (Fonts::get().label, 12.0f));
        glowText (g, "SELF-OSC", fbLcd.withTrimmedRight (9.0f), juce::Justification::centredRight, cHot);
    }

    // ---- small knob values
    g.setFont (fnt (Fonts::get().mono, 13.0f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        g.setColour (cText.withAlpha (0.9f));
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (70.0f, 14.0f).withCentre ({ smallX ((int) i), 449.0f }), juce::Justification::centred);
    }
}

void ThrowPanel::resized()
{
    timeKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (timeC).toNearestInt());
    divKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (timeC).toNearestInt());
    fbKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (fbC).toNearestInt());
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ smallX ((int) i), smallY }).toNearestInt());

    taps.setBounds (screenInner);
    syncButtons[0].setBounds (252, 292, 56, 26);
    syncButtons[1].setBounds (312, 292, 56, 26);
    modeButtons[0].setBounds (380, 292, 58, 26);
    modeButtons[1].setBounds (442, 292, 68, 26);
    modeButtons[2].setBounds (514, 292, 54, 26);

    throwButton.setBounds (throwRect);
    freezeButton.setBounds (freezeRect);
    throwOnlyToggle.setBounds (26, 445, 180, 22);

    prevButton.setBounds (470, 28, 30, 30);
    presetBox.setBounds (504, 28, 250, 30);
    nextButton.setBounds (758, 28, 30, 30);
}

// ================= Editor =================

ThrowEditor::ThrowEditor (ThrowProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (ThrowPanel::baseW * 3 / 4, ThrowPanel::baseH * 3 / 4, ThrowPanel::baseW * 2, ThrowPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) ThrowPanel::baseW / ThrowPanel::baseH);
    setSize (ThrowPanel::baseW, ThrowPanel::baseH);
}

void ThrowEditor::resized()
{
    panel.setBounds (0, 0, ThrowPanel::baseW, ThrowPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / ThrowPanel::baseW));
}
