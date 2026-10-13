#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText  { 0xffeae6dc };
const juce::Colour cInk   { 0xff8a94aa };
const juce::Colour cGold  { 0xffe8c46a };
const juce::Colour cHot   { 0xffff7a4a };
const juce::Colour cIce   { 0xffbfe4ff };
const juce::Colour cDeep  { 0xff111628 };

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
const juce::Point<float> decayC { 130.0f, 232.0f }, mixC { 730.0f, 232.0f };
constexpr float bigKnob = 168.0f, smallKnob = 56.0f;
constexpr float row1Y = 392.0f, row2Y = 478.0f;
float row1X (int i) { return 252.0f + 90.0f * (float) i; }
float row2X (int i) { return 297.0f + 90.0f * (float) i; }
const juce::Rectangle<int> screenInner { 257, 133, 346, 164 };
const juce::Rectangle<int> freezeRect { 28, 358, 156, 74 };
const juce::Rectangle<float> decayLcd { 70.0f, 314.0f, 120.0f, 24.0f }, mixLcd { 670.0f, 314.0f, 120.0f, 24.0f };

const char* modeBlurb[auraverb::numModes] = { "tight natural room", "lush concert hall", "bright vintage plate", "dense live chamber",
                                              "grainy chorused digital", "octave + fifth bloom", "huge evolving darkness" };

juce::String msText (double ms)
{
    if (ms < 10.0) return juce::String (ms, 1) + " ms";
    if (ms < 1000.0) return juce::String (juce::roundToInt (ms)) + " ms";
    return juce::String (ms / 1000.0, 2) + " s";
}

juce::String secText (double s) { return juce::String (s, s < 10.0 ? 2 : 1) + " s"; }

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

AuraLook::AuraLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cGold);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff15110a));
    setColour (juce::PopupMenu::headerTextColourId, cGold);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cGold.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void AuraLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f;
    const bool big = R > 60.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cGold : cInk;

    if (big)
    {
        const float arcR = R - 23.0f;
        const float a0 = bipolar ? (start + end) * 0.5f : start;
        const float lo = juce::jmin (a0, angle), hi = juce::jmax (a0, angle);
        if (hi > lo)
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
        const float ringR = R - 2.5f;
        const int lit = juce::roundToInt (pos * 20.0f);
        const int mid = bipolar ? 10 : 0;
        for (int k = juce::jmin (mid, lit); k <= juce::jmax (mid, lit); ++k)
        {
            const float a = juce::degreesToRadians (225.0f - 270.0f * (float) k / 20.0f);
            const float lx = c.x + std::cos (a) * ringR, ly = c.y - std::sin (a) * ringR;
            g.setColour (on.withAlpha (0.22f));
            g.fillEllipse (lx - 3.4f, ly - 3.4f, 6.8f, 6.8f);
            g.setColour (on);
            g.fillEllipse (lx - 1.6f, ly - 1.6f, 3.2f, 3.2f);
        }
    }

    const float body = big ? R - 30.0f : R - 8.5f;
    const float imgHalf = body / 0.80f;
    g.drawImage (big ? knobBig : knobSmall, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    const float pw = big ? 4.0f : 2.5f;
    juce::Path p;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.94f, pw, topR * 0.46f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (on.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (big ? 4.0f : 2.5f), tr);
    g.setColour (on.brighter (0.25f));
    g.fillPath (p, tr);
}

void AuraLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff6a6450) : juce::Colour (0xff444a5c));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff080a14), 0, 0, juce::Colour (0xff121626), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cGold);
    g.fillPath (arrow);
}

void AuraLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font AuraLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font AuraLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void AuraLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cGold.withAlpha (0.16f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xfff6dc94) : juce::Colour (down ? 0xff1c2030 : 0xff2c3244), 0, 0,
                                             on ? juce::Colour (0xffc0943a) : juce::Colour (0xff141826), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (on ? juce::Colour (0xfffff0c4) : juce::Colour (over ? 0xff6a7088 : 0xff40465a));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void AuraLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colour (0xff1a1408) : cText.withAlpha (0.82f));
    g.setFont (fnt (Fonts::get().label, (float) b.getProperties().getWithDefault ("fontSize", 14.5)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
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
    if (on)
    {
        ignoreUp = true;
        attachment.setValueAsCompleteGesture (0.0f);
        return;
    }
    ignoreUp = false;
    downTime = juce::Time::getMillisecondCounter();
    attachment.setValueAsCompleteGesture (1.0f);
}

void FreezeButton::mouseUp (const juce::MouseEvent&)
{
    if (ignoreUp || ! on) return;
    if (juce::Time::getMillisecondCounter() - downTime > 300)
        attachment.setValueAsCompleteGesture (0.0f);   // held: momentary
}

void FreezeButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto hue = cIce;
    if (on)
        g.setGradientFill (juce::ColourGradient (hue.brighter (0.3f), r.getCentreX(), r.getY() + r.getHeight() * 0.35f,
                                                 hue.darker (0.45f), r.getCentreX(), r.getBottom() + r.getHeight() * 0.3f, true));
    else
        g.setGradientFill (juce::ColourGradient (hue.withMultipliedBrightness (0.20f).withMultipliedSaturation (0.7f), 0, 0,
                                                 hue.withMultipliedBrightness (0.08f).withMultipliedSaturation (0.6f), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 9.0f);

    juce::Path top;
    top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.38f : 0.10f), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
    g.fillPath (top);
    g.setColour (on ? hue.brighter (0.6f).withAlpha (0.9f) : hue.withAlpha (0.22f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.2f);

    const auto ink = on ? juce::Colour (0xff0c1a26) : hue.withAlpha (0.55f);
    // snowflake glyph + word
    const auto c = juce::Point<float> (r.getX() + 34.0f, r.getCentreY());
    g.setColour (ink);
    for (int k = 0; k < 3; ++k)
    {
        const float a = juce::MathConstants<float>::pi * (float) k / 3.0f;
        const juce::Point<float> d (std::sin (a) * 14.0f, std::cos (a) * 14.0f);
        g.drawLine ({ c - d, c + d }, 2.2f);
        for (float sgn : { -1.0f, 1.0f })
        {
            const auto tip = c + d * (0.62f * sgn);
            const juce::Point<float> nrm (d.y * 0.22f, -d.x * 0.22f);
            g.drawLine ({ tip, tip + d * (0.25f * sgn) + nrm }, 1.6f);
            g.drawLine ({ tip, tip + d * (0.25f * sgn) - nrm }, 1.6f);
        }
    }
    g.setFont (fnt (Fonts::get().heavy, 21.0f));
    g.drawText ("FREEZE", r.withTrimmedLeft (58.0f).withTrimmedBottom (14.0f), juce::Justification::centredLeft);
    g.setFont (fnt (Fonts::get().label, 11.5f));
    g.drawText (on ? "HOLDING THE SPACE" : "INFINITE HOLD", r.withTrimmedLeft (59.0f).withTrimmedTop (34.0f), juce::Justification::centredLeft);
}

// ================= SpaceDisplay =================

SpaceDisplay::SpaceDisplay (AuraVerbProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    activity = juce::jlimit (0.0f, 1.0f, proc.uiWet.load() * 4.0f);
}

void SpaceDisplay::tick()
{
    const auto now = juce::Time::getMillisecondCounter();
    const double dt = lastTick == 0 ? 0.0 : (double) (now - lastTick) * 0.001;
    lastTick = now;
    const float target = juce::jlimit (0.0f, 1.0f, proc.uiWet.load() * 4.0f + proc.uiIn.load() * 1.5f);
    activity += (target > activity ? 0.5f : 0.06f) * (target - activity);
    cursorSec += dt;
    repaint();
}

void SpaceDisplay::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const int mi = (int) p.mode;
    const float gx0 = 15.0f, gx1 = 339.0f, gy0 = 21.0f, gy1 = 143.0f;
    const double pre = auraverb::predelayMsFor (p) * 0.001;
    const double rt = juce::jlimit ((double) auraverb::Engine::minDecay, (double) auraverb::Engine::maxDecay, (double) p.decaySec);

    // ---- readout strip
    const auto strip = juce::Rectangle<float> (12.0f, 1.0f, (float) getWidth() - 24.0f, 15.0f);
    g.setFont (fnt (Fonts::get().label, 13.5f));
    glowText (g, auraverb::modeNames[mi], strip, juce::Justification::centredLeft, cGold);
    const float nameW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), auraverb::modeNames[mi]);
    g.setFont (fnt (Fonts::get().semi, 12.5f));
    g.setColour (cInk);
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc2\xb7  ")) + modeBlurb[mi], strip.withTrimmedLeft (nameW + 6.0f), juce::Justification::centredLeft);
    g.setFont (fnt (Fonts::get().mono, 12.5f));
    const juce::String preTxt = p.predelaySync ? juce::String (auraverb::divisionNames[p.predelayDiv]) + " " + msText (pre * 1000.0)
                                               : msText (pre * 1000.0);
    glowText (g, (p.freeze ? juce::String ("RT INF") : "RT " + secText (rt)) + "  PRE " + preTxt, strip, juce::Justification::centredRight,
              cGold.withAlpha (0.9f));

    // ---- log time axis
    const double total = std::max (0.25, pre + (p.freeze ? std::max (rt, 3.0) : rt) * 1.12 + 0.03);
    constexpr double tau = 0.02;
    const double lnTotal = std::log1p (total / tau);
    auto xFor = [&] (double t) { return gx0 + (gx1 - gx0) * (float) (std::log1p (std::max (0.0, t) / tau) / lnTotal); };
    auto yFor = [&] (double dbv) { return gy0 + (gy1 - gy0) * (float) juce::jlimit (0.0, 1.0, -dbv / 60.0); };

    g.setFont (fnt (Fonts::get().semi, 10.5f));
    for (double t : { 0.01, 0.03, 0.1, 0.3, 1.0, 3.0, 10.0, 30.0 })
    {
        if (t > total * 0.97) break;
        const float x = xFor (t);
        g.setColour (cGold.withAlpha (0.10f));
        g.fillRect (x, gy0, 1.0f, gy1 - gy0);
        g.setColour (cInk.withAlpha (0.85f));
        const juce::String lbl = t < 1.0 ? juce::String (juce::roundToInt (t * 1000.0)) + "ms" : juce::String (juce::roundToInt (t)) + "s";
        g.drawText (lbl, juce::Rectangle<float> (x - 20.0f, gy1 + 3.0f, 40.0f, 12.0f), juce::Justification::centred);
    }

    const float bright = 0.6f + 0.4f * activity;
    const float earlyG = std::min (1.0f, 2.0f * (1.0f - p.earlyLate)), lateG = std::min (1.0f, 2.0f * p.earlyLate);

    // dry hit
    g.setColour (cText.withAlpha (0.55f));
    g.fillRect (xFor (0.0) - 1.0f, gy0, 2.0f, gy1 - gy0);

    // early reflections
    float erT[auraverb::maxEarlyTaps], erG[auraverb::maxEarlyTaps];
    const int nEr = auraverb::earlyPattern (p.mode, p.size, erT, erG);
    float gMax = 1.0e-6f;
    for (int k = 0; k < nEr; ++k) gMax = std::max (gMax, std::abs (erG[k]));
    if (earlyG > 0.01f)
        for (int k = 0; k < nEr; ++k)
        {
            const double t = pre + erT[k] * 0.001;
            const double dbv = 20.0 * std::log10 (std::abs (erG[k]) / gMax * earlyG + 1.0e-6) - 4.0;
            const float x = xFor (t), y = yFor (dbv);
            g.setColour (cText.withAlpha (0.16f * bright));
            g.fillRect (x - 2.0f, y, 4.0f, gy1 - y);
            g.setColour (cText.withAlpha (0.75f * bright));
            g.fillRect (x - 0.6f, y, 1.2f, gy1 - y);
        }

    // late envelope (dB vs time): build-up then linear-in-dB decay
    const double span = (60.0 + 440.0 * p.size) * p.attack * 0.001;
    const double peak = -7.0 + 20.0 * std::log10 (lateG + 1.0e-3);
    const double rtHf = p.freeze ? 1.0e9 : rt * std::min (1.0, std::pow (juce::jlimit (1000.0, 20000.0, (double) p.dampingHz) / 17000.0, 0.7));
    const double rtLo = p.freeze ? 1.0e9 : rt * p.bassMult;
    auto envDb = [&] (double t, double rtx)
    {
        const double tt = t - pre;
        if (tt < 0.0) return -90.0;
        double build = 0.0;
        if (span > 0.002 && tt < span) build = -24.0 * std::pow (1.0 - tt / span, 1.6);
        const double onset = std::min (tt / 0.004, 1.0);
        const double dec = -60.0 * std::max (0.0, tt - span * 0.6) / rtx;
        return peak + build + dec + 20.0 * std::log10 (std::max (onset, 1.0e-3));
    };
    auto curve = [&] (double rtx, bool closed)
    {
        juce::Path path;
        const float xs = xFor (pre);
        if (closed) path.startNewSubPath (xs, gy1);
        bool started = closed;
        for (int i = 0; i <= 160; ++i)
        {
            const double t = pre + (total - pre) * std::pow ((double) i / 160.0, 2.2);
            const float x = xFor (t), y = yFor (envDb (t, rtx));
            if (! started) { path.startNewSubPath (x, y); started = true; }
            else path.lineTo (x, y);
        }
        if (closed) { path.lineTo (gx1, gy1); path.closeSubPath(); }
        return path;
    };
    if (lateG > 0.01f)
    {
        const auto col = p.freeze ? cIce : cGold;
        const double rtMain = p.freeze ? 1.0e9 : rt;
        auto fill = curve (rtMain, true);
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.42f * bright), 0, gy0, col.withAlpha (0.02f), 0, gy1, false));
        g.fillPath (fill);
        auto line = curve (rtMain, false);
        g.setColour (col.withAlpha (0.25f * bright));
        g.strokePath (line, juce::PathStrokeType (5.0f));
        g.setColour (col.withAlpha (bright));
        g.strokePath (line, juce::PathStrokeType (1.8f));
        if (! p.freeze)
        {
            juce::Path dashed;
            const float dashes[] = { 3.0f, 3.0f };
            if (std::abs (rtLo - rt) / rt > 0.04)
            {
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, curve (rtLo, false), dashes, 2);
                g.setColour (juce::Colour (0xff8fb0ff).withAlpha (0.75f));
                g.fillPath (dashed);
            }
            if (std::abs (rtHf - rt) / rt > 0.04)
            {
                dashed.clear();
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, curve (rtHf, false), dashes, 2);
                g.setColour (cText.withAlpha (0.55f));
                g.fillPath (dashed);
            }
        }
    }

    // moving playhead while audio flows
    if (activity > 0.03f)
    {
        const double t = std::fmod (cursorSec * (p.freeze ? 0.6 : 1.0), total);
        const float cx = xFor (t);
        g.setGradientFill (juce::ColourGradient (cGold.withAlpha (0.0f), cx - 16.0f, 0, cGold.withAlpha (0.22f * activity), cx, 0, false));
        g.fillRect (juce::Rectangle<float> (cx - 16.0f, gy0, 16.0f, gy1 - gy0));
        g.setColour (cGold.withAlpha (0.6f * activity));
        g.fillRect (juce::Rectangle<float> (cx, gy0, 1.0f, gy1 - gy0));
    }

    // ---- chips (top right inside the plot) + legend
    g.setFont (fnt (Fonts::get().label, 11.0f));
    float cx = gx1;
    auto chip = [&] (const juce::String& t, juce::Colour col, bool filled)
    {
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 10.0f;
        cx -= w;
        auto r = juce::Rectangle<float> (cx, gy0 + 4.0f, w, 13.0f);
        if (filled) { g.setColour (col); g.fillRoundedRectangle (r, 3.0f); g.setColour (juce::Colour (0xff0c0f1a)); }
        else
        {
            g.setColour (juce::Colour (0xd8070912));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (col.withAlpha (0.75f));
            g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        }
        g.drawText (t, r, juce::Justification::centred);
        cx -= 5.0f;
    };
    if (p.freeze) chip ("FROZEN", cIce, true);
    chip (p.vintage ? "VINTAGE" : "MODERN", cGold, p.vintage);
    if (p.predelaySync) chip (juce::String (p.bpm, 0) + (proc.uiHostTempo.load() ? " BPM" : " BPM*"), cGold, false);
    const float dg = proc.uiDuck.load();
    if (p.duck > 0.001f)
        chip (dg < 0.98f ? "DUCK " + juce::String (juce::Decibels::gainToDecibels (dg), 1) + " dB" : juce::String ("DUCK"), cGold, false);

    if (lateG > 0.01f && ! p.freeze)
    {
        g.setFont (fnt (Fonts::get().semi, 10.5f));
        g.setColour (juce::Colour (0xff8fb0ff).withAlpha (0.85f));
        g.drawText ("- - LOW", juce::Rectangle<float> (gx0 + 6.0f, gy0 + 4.0f, 44.0f, 11.0f), juce::Justification::centredLeft);
        g.setColour (cText.withAlpha (0.6f));
        g.drawText ("- - HIGH", juce::Rectangle<float> (gx0 + 6.0f, gy0 + 16.0f, 44.0f, 11.0f), juce::Justification::centredLeft);
    }
}

// ================= Panel =================

AuraPanel::AuraPanel (AuraVerbProcessor& p)
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
    setupKnob (decayKnob, IDs::decay, "Reverb time (RT60): how long the tail takes to fall by 60 dB. 0.1 s to 60 s.");
    setupKnob (mixKnob, IDs::mix, "Dry / wet balance. Use 100 % on a send / return bus.");
    setupKnob (preDivKnob, IDs::preDiv, "Pre-delay as a note value, locked to the host tempo (T = triplet, D = dotted)");
    preDivKnob.s.setVelocityBasedMode (false);
    preDivKnob.s.setMouseDragSensitivity (300);

    const char* ids1[] = { IDs::predelay, IDs::size, IDs::earlyLate, IDs::attack, IDs::diffusion, IDs::modDepth, IDs::modRate };
    const char* tips1[] = { "Gap before the reverb starts (keeps vocals upfront and clear)",
                            "Size of the space: longer reflections and a wider, slower build",
                            "Balance of early reflections (left) and the late tail (right)",
                            "Build / swell: the tail blooms in instead of starting instantly",
                            "Density: low = grainy, distinct echoes, high = smooth wash",
                            "Amount of tail modulation (chorus-like movement, removes metallic ringing)",
                            "Speed of the tail modulation" };
    const char* ids2[] = { IDs::damping, IDs::lowCut, IDs::bassMult, IDs::width, IDs::duck, IDs::output };
    const char* tips2[] = { "High frequencies decay faster above this frequency (OFF = bright, full-range tail)",
                            "Removes low end from the reverb only (keeps 808s and kicks clean)",
                            "Decay time multiplier for the lows: below x1 = tight bottom, above x1 = big warm bottom",
                            "Stereo width of the reverb (0 = mono, 200 % = extra wide)",
                            "Ducks the reverb while the dry signal plays, it blooms in the gaps",
                            "Output level" };
    for (size_t i = 0; i < row1.size(); ++i) setupKnob (row1[i], ids1[i], tips1[i]);
    for (size_t i = 0; i < row2.size(); ++i) setupKnob (row2[i], ids2[i], tips2[i]);
    row2[2].s.getProperties().set ("bipolar", true);
    row2[2].s.setDoubleClickReturnValue (true, 1.0);
    row2[3].s.getProperties().set ("bipolar", true);
    row2[3].s.setDoubleClickReturnValue (true, 1.0);
    row2[5].s.getProperties().set ("bipolar", true);
    row2[5].s.setDoubleClickReturnValue (true, 0.0);

    addAndMakeVisible (display);

    // mode selector
    const char* modeTips[] = { "ROOM: tight, natural small space - drums, booth vocals, glue",
                               "HALL: lush concert hall with smooth, wide tail",
                               "PLATE: bright, dense vintage plate - the classic vocal and snare verb",
                               "CHAMBER: dense live chamber, warm and present",
                               "80s DIGITAL: grainy, chorused colour of early digital reverbs",
                               "SHIMMER: octave + fifth pitch-shifted feedback for heavenly pads",
                               "DARK SPACE: huge, dark, slowly evolving space" };
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::mode), [this] (float v)
    {
        modeState = juce::roundToInt (v);
        refreshSelectors();
    }, nullptr);
    for (int i = 0; i < auraverb::numModes; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (auraverb::modeNames[i]);
        b.getProperties().set ("fontSize", 15.5);
        b.onClick = [this, i] { modeAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip (modeTips[i]);
        addAndMakeVisible (b);
    }
    syncAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::preSync), [this] (float v)
    {
        syncState = v > 0.5f ? 1 : 0;
        refreshSelectors();
    }, nullptr);
    const char* syncNames[] = { "FREE", "SYNC" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = syncButtons[(size_t) i];
        b.setButtonText (syncNames[i]);
        b.onClick = [this, i] { syncAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip (i == 0 ? "Pre-delay in milliseconds" : "Pre-delay as a note value locked to the host tempo");
        addAndMakeVisible (b);
    }
    colourAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::colour), [this] (float v)
    {
        colourState = juce::roundToInt (v);
        refreshSelectors();
    }, nullptr);
    const char* colNames[] = { "MODERN", "VINTAGE" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = colourButtons[(size_t) i];
        b.setButtonText (colNames[i]);
        b.onClick = [this, i] { colourAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip (i == 0 ? "Clean, full-bandwidth modern colour"
                             : "Vintage colour: band-limited, slower deeper modulation, gentle converter saturation");
        addAndMakeVisible (b);
    }
    modeAttachment->sendInitialUpdate();
    syncAttachment->sendInitialUpdate();
    colourAttachment->sendInitialUpdate();

    freezeButton.setTooltip ("FREEZE: holds the current reverb tail forever (new audio stays dry). Click = latch, hold = momentary.");
    addAndMakeVisible (freezeButton);

    // presets
    const auto& presets = auraverb::factoryPresets();
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

AuraPanel::~AuraPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AuraPanel::refreshSelectors()
{
    for (int i = 0; i < auraverb::numModes; ++i)
        modeButtons[(size_t) i].setToggleState (i == modeState, juce::dontSendNotification);
    for (int i = 0; i < 2; ++i)
    {
        syncButtons[(size_t) i].setToggleState (i == syncState, juce::dontSendNotification);
        colourButtons[(size_t) i].setToggleState (i == colourState, juce::dontSendNotification);
    }
    preDivKnob.s.setVisible (syncState == 1);
    row1[0].s.setVisible (syncState == 0);
    repaint();
}

void AuraPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    display.tick();
    repaint (juce::Rectangle<int> (26, 450, 162, 72));   // meters
}

void AuraPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    if (freezeButton.isOn())
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (cIce.withAlpha (0.07f * (float) (4 - k)));
            g.fillRoundedRectangle (freezeRect.toFloat().expanded ((float) k * 3.0f), 10.0f + (float) k * 3.0f);
        }
}

void AuraPanel::paintOverChildren (juce::Graphics& g)
{
    const auto p = proc.currentParams();

    // ---- LCDs under the big dials
    g.setFont (fnt (Fonts::get().mono, 19.0f));
    if (p.freeze)
    {
        glowText (g, "FROZEN", decayLcd.withTrimmedLeft (8.0f), juce::Justification::centredLeft, cIce);
        g.setFont (fnt (Fonts::get().mono, 12.0f));
        glowText (g, secText (p.decaySec), decayLcd.withTrimmedRight (8.0f).withTrimmedTop (2.0f), juce::Justification::centredRight, cGold.withAlpha (0.55f));
    }
    else
        glowText (g, secText (p.decaySec), decayLcd, juce::Justification::centred, cGold);
    g.setFont (fnt (Fonts::get().mono, 19.0f));
    glowText (g, juce::String (juce::roundToInt (p.mix * 100.0f)) + " %", mixLcd.withTrimmedLeft (8.0f), juce::Justification::centredLeft, cGold);
    g.setFont (fnt (Fonts::get().mono, 12.0f));
    glowText (g, p.mix <= 0.0005f ? "DRY" : (p.mix >= 0.9995f ? "WET" : "BLEND"), mixLcd.withTrimmedRight (8.0f).withTrimmedTop (2.0f),
              juce::Justification::centredRight, cGold.withAlpha (0.55f));

    // ---- small knob values
    g.setFont (fnt (Fonts::get().mono, 12.5f));
    g.setColour (cText.withAlpha (0.9f));
    for (size_t i = 0; i < row1.size(); ++i)
    {
        auto& k = (i == 0 && syncState == 1) ? preDivKnob : row1[i];
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (86.0f, 14.0f).withCentre ({ row1X ((int) i), row1Y + 34.0f }), juce::Justification::centred);
    }
    for (size_t i = 0; i < row2.size(); ++i)
    {
        auto& k = row2[i];
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (86.0f, 14.0f).withCentre ({ row2X ((int) i), row2Y + 34.0f }), juce::Justification::centred);
    }

    // ---- meters (IN / WET) + duck readout
    auto bar = [&] (float level, float y)
    {
        const float dbv = juce::Decibels::gainToDecibels (level, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (dbv + 60.0f) / 60.0f);
        auto r = juce::Rectangle<float> (59.0f, y - 2.0f, 124.0f * frac, 5.0f);
        g.setColour (cGold.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (1.5f), 3.0f);
        g.setGradientFill (juce::ColourGradient (cGold.darker (0.5f), 59.0f, 0, dbv > -1.0f ? cHot : cGold.brighter (0.2f), 59.0f + 124.0f * frac, 0, false));
        g.fillRoundedRectangle (r, 2.0f);
    };
    bar (proc.uiIn.load(), 462.0f);
    bar (proc.uiWet.load() * proc.uiDuck.load() * p.mix, 482.0f);
    g.setFont (fnt (Fonts::get().label, 12.0f));
    if (p.duck > 0.001f)
    {
        const float dgain = proc.uiDuck.load();
        g.setColour (cInk);
        g.drawText ("DUCK", juce::Rectangle<float> (30.0f, 496.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
        auto r = juce::Rectangle<float> (59.0f, 500.0f, 124.0f, 5.0f);
        g.setColour (juce::Colour (0xff04050a));
        g.fillRoundedRectangle (r, 2.5f);
        const float frac = juce::jlimit (0.0f, 1.0f, -juce::Decibels::gainToDecibels (dgain, -30.0f) / 26.0f);
        g.setColour (cIce.withAlpha (0.85f));
        g.fillRoundedRectangle (r.withLeft (r.getRight() - r.getWidth() * frac), 2.5f);
        g.setFont (fnt (Fonts::get().mono, 11.5f));
        g.setColour (cText.withAlpha (0.8f));
        g.drawText (juce::String (juce::Decibels::gainToDecibels (dgain), 1) + " dB", juce::Rectangle<float> (59.0f, 507.0f, 124.0f, 13.0f), juce::Justification::centredRight);
    }
    else
    {
        g.setColour (cInk.withAlpha (0.6f));
        g.drawText ("DUCK OFF", juce::Rectangle<float> (30.0f, 498.0f, 154.0f, 14.0f), juce::Justification::centredLeft);
    }
}

void AuraPanel::resized()
{
    decayKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (decayC).toNearestInt());
    mixKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (mixC).toNearestInt());
    for (size_t i = 0; i < row1.size(); ++i)
        row1[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ row1X ((int) i), row1Y }).toNearestInt());
    preDivKnob.s.setBounds (row1[0].s.getBounds());
    for (size_t i = 0; i < row2.size(); ++i)
        row2[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ row2X ((int) i), row2Y }).toNearestInt());

    display.setBounds (screenInner);
    for (int i = 0; i < auraverb::numModes; ++i)
        modeButtons[(size_t) i].setBounds (20 + i * 118, 94, 112, 26);
    syncButtons[0].setBounds (252, 308, 56, 24);
    syncButtons[1].setBounds (312, 308, 56, 24);
    colourButtons[0].setBounds (484, 308, 60, 24);
    colourButtons[1].setBounds (548, 308, 60, 24);
    freezeButton.setBounds (freezeRect);

    prevButton.setBounds (496, 28, 30, 30);
    presetBox.setBounds (530, 28, 266, 30);
    nextButton.setBounds (800, 28, 30, 30);
}

// ================= Editor =================

AuraVerbEditor::AuraVerbEditor (AuraVerbProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (AuraPanel::baseW * 3 / 4, AuraPanel::baseH * 3 / 4, AuraPanel::baseW * 2, AuraPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) AuraPanel::baseW / AuraPanel::baseH);
    setSize (AuraPanel::baseW, AuraPanel::baseH);
}

void AuraVerbEditor::resized()
{
    panel.setBounds (0, 0, AuraPanel::baseW, AuraPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / AuraPanel::baseW));
}
