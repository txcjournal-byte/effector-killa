#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText { 0xffe8ecf2 };
const juce::Colour cInk  { 0xff848c98 };
const juce::Colour cIce  { 0xffe2ecf8 };
const juce::Colour cRed  { 0xffff2a3a };
const juce::Colour cDeep { 0xff101114 };

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
const float bodyX[] = { 56.0f, 134.0f, 212.0f };
const float bodyY[] = { 150.0f, 254.0f };
const float deckX[] = { 65.0f, 131.0f, 197.0f, 273.0f, 339.0f, 415.0f, 481.0f, 547.0f, 613.0f, 689.0f, 755.0f };
constexpr float deckY = 380.0f;
const juce::Point<float> glideC { 686.0f, 194.0f };
constexpr float bigKnob = 140.0f, smallKnob = 64.0f;
const juce::Rectangle<float> glideLcd { 631.0f, 262.0f, 110.0f, 22.0f };
const juce::Rectangle<int> scopeRect { 270, 102, 280, 146 };
const juce::Rectangle<int> satRect { 266, 262, 290, 36 };
const juce::Rectangle<int> keyRectArea { 26, 456, 768, 74 };

const char* modeNames[] = { "MONO", "LEGATO", "POLY" };
const char* satNames[] = { "TUBE", "TAPE", "HARD", "FOLD" };
const char* satSub[] = { "warm", "smooth", "clip", "folded" };

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c, float glow = 0.18f)
{
    g.setColour (c.withAlpha (glow * c.getFloatAlpha()));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (dx, dy), j);
    g.setColour (c);
    g.drawText (t, r, j);
}
} // namespace

// ================= Look =================

SubLook::SubLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cRed);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, cRed.brighter (0.3f));
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cRed.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void SubLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f;
    const bool big = R > 50.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float danger = (float) (double) s.getProperties().getWithDefault ("danger", 2.0); // normalised pos where the red zone starts
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cIce : cInk;

    if (big)
    {
        const float arcR = R - 23.0f;
        const float a0 = bipolar ? (start + end) * 0.5f : start;
        auto arcSeg = [&] (float from, float to, juce::Colour col)
        {
            if (to <= from) return;
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, from, to, true);
            g.setColour (col.withAlpha (0.16f));
            g.strokePath (arc, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (col);
            g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        const float lo = juce::jmin (a0, angle), hi = juce::jmax (a0, angle);
        const float dangerAngle = start + danger * (end - start);
        arcSeg (lo, juce::jmin (hi, dangerAngle), on);
        arcSeg (juce::jmax (lo, dangerAngle), hi, cRed);
        // red tip where the arc ends
        if (pos > 0.002f)
        {
            const juce::Point<float> tip (c.x + std::sin (angle) * arcR, c.y - std::cos (angle) * arcR);
            g.setColour (cRed.withAlpha (0.35f));
            g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (tip));
            g.setColour (cRed.brighter (0.3f));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (tip));
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
            const auto col = (float) k / 20.0f > danger ? cRed : on;
            g.setColour (col.withAlpha (0.22f));
            g.fillEllipse (lx - 3.6f, ly - 3.6f, 7.2f, 7.2f);
            g.setColour (col);
            g.fillEllipse (lx - 1.7f, ly - 1.7f, 3.4f, 3.4f);
        }
    }

    // photoreal knob (pre-rendered); only the pointer rotates
    const float body = big ? R - 30.0f : 22.0f;
    const float imgHalf = body / 0.80f;
    g.drawImage (big ? knobBig : knobSmall, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    const float pw = big ? 3.6f : 2.6f;
    juce::Path p;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.94f, pw, topR * 0.46f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    const auto ptr = big ? cRed : on;
    g.setColour (ptr.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (big ? 4.0f : 2.5f), tr);
    g.setColour (ptr.brighter (0.2f));
    g.fillPath (p, tr);
}

void SubLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff5c626e) : juce::Colour (0xff3c414a));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0c0e), 0, 0, juce::Colour (0xff15171a), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cRed);
    g.fillPath (arrow);
}

void SubLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font SubLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font SubLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void SubLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cIce.withAlpha (0.14f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xfff6f9fd) : juce::Colour (down ? 0xff1e2024 : 0xff2e3137), 0, 0,
                                             on ? juce::Colour (0xffa6aebb) : juce::Colour (0xff17181b), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (on ? juce::Colours::white : juce::Colour (over ? 0xff6a707c : 0xff42464e));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void SubLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colour (0xff121418) : cText.withAlpha (0.8f));
    g.setFont (fnt (Fonts::get().label, 14.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void SubLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
    if (b.getToggleState())
    {
        g.setColour (cRed.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.5f));
    }
    g.setGradientFill (juce::ColourGradient (b.getToggleState() ? cRed.brighter (0.5f) : juce::Colour (0xff3a2024), led.getCentreX(), led.getY(),
                                             b.getToggleState() ? cRed.darker (0.3f) : juce::Colour (0xff1c1012), led.getCentreX(), led.getBottom(), false));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (led, 1.0f);
    g.setColour (b.getToggleState() ? (over ? cText : cText.withAlpha (0.9f)) : cInk);
    g.setFont (fnt (Fonts::get().label, 12.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4), juce::Justification::centredLeft);
}

// ================= SatSelector =================

SatSelector::SatSelector (juce::RangedAudioParameter& p, SubKillaProcessor& pr)
    : proc (pr), attachment (p, [this] (float v)
      {
          selected = juce::jlimit (0, 3, juce::roundToInt (v));
          repaint();
      }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SatSelector::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 4; ++i)
        if (cell (i).contains (e.position))
        {
            attachment.setValueAsCompleteGesture ((float) i);
            return;
        }
}

void SatSelector::paint (juce::Graphics& g)
{
    const float drive = proc.apvts.getRawParameterValue (IDs::drive)->load();
    for (int i = 0; i < 4; ++i)
    {
        const auto r = cell (i);
        const bool on = i == selected;
        const auto hue = cRed;
        if (on)
        {
            const float heat = 0.25f + 0.75f * drive;
            juce::ColourGradient lit (hue.brighter (0.25f + 0.35f * heat), r.getCentreX(), r.getY() + r.getHeight() * 0.3f,
                                      hue.darker (0.9f - 0.3f * heat), r.getCentreX(), r.getBottom() + r.getHeight() * 0.5f, true);
            g.setGradientFill (lit);
        }
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a1215), 0, r.getY(), juce::Colour (0xff120809), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 7.0f);

        juce::Path top;
        top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 5.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.34f : 0.08f), 0, r.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
        g.fillPath (top);
        g.setColour (on ? hue.brighter (0.8f).withAlpha (0.9f) : hue.withAlpha (0.20f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.1f);

        g.setFont (fnt (Fonts::get().heavy, 14.0f));
        const auto txt = r.withTrimmedBottom (11.0f).translated (0.0f, 2.0f);
        if (on)
        {
            g.setColour (juce::Colour (0xff3a0006).withAlpha (0.6f));
            g.drawText (satNames[i], txt.translated (0.0f, 1.0f), juce::Justification::centred);
            g.setColour (juce::Colour (0xfffff4f4));
        }
        else
            g.setColour (hue.withAlpha (0.55f));
        g.drawText (satNames[i], txt, juce::Justification::centred);
        g.setFont (fnt (Fonts::get().semi, 10.5f));
        g.setColour (on ? juce::Colours::white.withAlpha (0.8f) : hue.withAlpha (0.35f));
        g.drawText (satSub[i], r.withTrimmedTop (r.getHeight() - 15.0f), juce::Justification::centred);
    }
}

// ================= Scope =================

Scope::Scope (SubKillaProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

void Scope::paint (juce::Graphics& g)
{
    const auto prm = proc.currentParams();
    const float hz = proc.uiHz.load();
    const bool sounding = proc.uiVoices.load() > 0 && hz > 1.0f;

    // ---- readout strip
    const auto strip = juce::Rectangle<float> (8.0f, 1.0f, (float) getWidth() - 16.0f, 16.0f);
    g.setFont (fnt (Fonts::get().mono, 13.5f));
    if (sounding)
    {
        const float nf = proc.uiNote.load();
        const int nn = juce::roundToInt (nf);
        const int cents = juce::roundToInt ((nf - (float) nn) * 100.0f);
        juce::String t = juce::String (subkilla::noteName (nn));
        if (cents != 0) t << (cents > 0 ? "+" : "") << cents;
        t << "  " << juce::String (hz, hz < 100.0f ? 1 : 0) << " Hz";
        glowText (g, t, strip, juce::Justification::centredLeft, cIce);
    }
    else
        glowText (g, "--   --- Hz", strip, juce::Justification::centredLeft, cIce.withAlpha (0.45f));
    glowText (g, modeNames[(int) prm.mode], strip, juce::Justification::centred, cIce.withAlpha (0.8f));
    const bool clean = prm.drive < 0.005f || prm.distMix < 0.005f;
    glowText (g, clean ? juce::String ("CLEAN") : juce::String (satNames[(int) prm.sat]) + " " + juce::String (juce::roundToInt (prm.drive * 100.0f)) + "%",
              strip, juce::Justification::centredRight, clean ? cIce.withAlpha (0.6f) : cRed.brighter (0.25f));

    // ---- trace
    const auto area = juce::Rectangle<float> (7.0f, 22.0f, 268.0f, 106.0f);
    const float midY = area.getCentreY(), halfH = area.getHeight() * 0.5f - 3.0f;
    const double rate = proc.getScopeRate();
    double window = sounding ? 2.5 * rate / hz : rate * 0.03;
    window = juce::jlimit (rate * 0.004, rate * 0.08, window);
    const int win = juce::jlimit (16, (int) snap.size() - 1, (int) window);
    const int wp = proc.getScopeWritePos();
    int left = wp - win - 1;
    if (sounding)
    {
        const int period = juce::jmax (4, (int) (rate / hz) + 2);
        for (int k = 0; k < period; ++k)
        {
            const int i = left - k;
            if (proc.getScopeSample (i - 1) < 0.0f && proc.getScopeSample (i) >= 0.0f) { left = i; break; }
        }
    }
    float pk = 0.0f;
    for (int i = 0; i < win; ++i)
    {
        snap[(size_t) i] = proc.getScopeSample (left + i);
        pk = juce::jmax (pk, std::abs (snap[(size_t) i]));
    }

    const float heat = clean ? 0.0f : juce::jlimit (0.0f, 1.0f, prm.drive * prm.distMix * 1.4f);
    if (pk < 1.0e-4f)
    {
        g.setColour (cIce.withAlpha (0.25f));
        g.fillRect (area.getX(), midY, area.getWidth(), 1.0f);
        g.setFont (fnt (Fonts::get().label, 12.0f));
        g.setColour (cInk.withAlpha (0.7f));
        g.drawText ("PLAY A NOTE", area.withTrimmedTop (area.getHeight() * 0.55f), juce::Justification::centredTop);
    }
    else
    {
        juce::Path path, fill;
        const int pts = (int) area.getWidth() * 2;
        for (int j = 0; j <= pts; ++j)
        {
            const float fpos = (float) j / (float) pts * (float) (win - 1);
            const int i0 = (int) fpos;
            const float fr = fpos - (float) i0;
            const float v = snap[(size_t) i0] + (snap[(size_t) juce::jmin (win - 1, i0 + 1)] - snap[(size_t) i0]) * fr;
            const float px = area.getX() + area.getWidth() * (float) j / (float) pts;
            const float py = midY - juce::jlimit (-1.05f, 1.05f, v / subkilla::limiterCeiling) * halfH;
            if (j == 0) { path.startNewSubPath (px, py); fill.startNewSubPath (px, midY); }
            path.lineTo (px, py);
            fill.lineTo (px, py);
        }
        fill.lineTo (area.getRight(), midY);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (cIce.withAlpha (0.10f), 0, area.getY(), cIce.withAlpha (0.0f), 0, midY, false));
        g.fillPath (fill);
        const auto haloCol = cIce.interpolatedWith (cRed, heat);
        g.setColour (haloCol.withAlpha (0.10f + 0.10f * heat));
        g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (haloCol.withAlpha (0.28f + 0.1f * heat));
        g.strokePath (path, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cIce.brighter (0.2f));
        g.strokePath (path, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---- status chips
    g.setFont (fnt (Fonts::get().label, 11.0f));
    float cx = area.getX();
    auto chip = [&] (const juce::String& t, juce::Colour col, bool filled)
    {
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 10.0f;
        auto r = juce::Rectangle<float> (cx, 133.0f, w, 12.0f);
        if (filled) { g.setColour (col); g.fillRoundedRectangle (r, 3.0f); g.setColour (juce::Colour (0xff12060a)); }
        else { g.setColour (col.withAlpha (0.65f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
        g.drawText (t, r, juce::Justification::centred);
        cx += w + 5.0f;
    };
    if (proc.uiGliding.load()) chip ("SLIDE", cRed, true);
    else if (prm.mode != subkilla::VoiceMode::poly && prm.glideMs > 0.5f) chip ("GLIDE READY", cIce, false);
    if (prm.mode == subkilla::VoiceMode::poly) chip ("VOICES " + juce::String (proc.uiVoices.load()) + "/4", cIce, false);
    const float gr = proc.uiGrDb.load();
    if (prm.limiter) chip (gr < -0.1f ? "LIMIT " + juce::String (gr, 1) + " dB" : juce::String ("LIMIT"), gr < -0.1f ? cRed : cIce, gr < -3.0f);
    g.setColour (cInk);
    const double spanMs = window / rate * 1000.0;
    g.drawText ("SPAN " + (spanMs < 10.0 ? juce::String (spanMs, 1) : juce::String (juce::roundToInt (spanMs))) + " ms",
                juce::Rectangle<float> (area.getRight() - 90.0f, 133.0f, 90.0f, 12.0f), juce::Justification::centredRight);
}

// ================= MiniKeyboard =================

MiniKeyboard::MiniKeyboard (SubKillaProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> MiniKeyboard::keyRect (int note) const
{
    const float ww = (float) getWidth() / (float) numWhite;
    const float top = railH + 1.0f, hgt = (float) getHeight() - top;
    int whiteIndex = 0;
    for (int n = lowNote; n < note; ++n)
        if (! isBlack (n)) ++whiteIndex;
    if (! isBlack (note))
        return { (float) whiteIndex * ww, top, ww, hgt };
    const float bw = ww * 0.62f;
    return { (float) whiteIndex * ww - bw * 0.5f, top, bw, hgt * 0.6f };
}

float MiniKeyboard::centreX (float note) const
{
    note = juce::jlimit ((float) lowNote, (float) (lowNote + numWhite / 7 * 12 - 1), note);
    const int n0 = (int) std::floor (note);
    const float f = note - (float) n0;
    const float x0 = keyRect (n0).getCentreX();
    const float x1 = n0 + 1 < lowNote + numWhite / 7 * 12 ? keyRect (n0 + 1).getCentreX() : x0;
    return x0 + (x1 - x0) * f;
}

int MiniKeyboard::noteAt (juce::Point<float> p) const
{
    const int hi = lowNote + numWhite / 7 * 12;
    for (int n = lowNote; n < hi; ++n)
        if (isBlack (n) && keyRect (n).contains (p)) return n;
    for (int n = lowNote; n < hi; ++n)
        if (! isBlack (n))
        {
            auto r = keyRect (n);
            if (p.x >= r.getX() && p.x < r.getRight()) return n;
        }
    return p.x < 0 ? lowNote : hi - 1;
}

float MiniKeyboard::velocityAt (juce::Point<float> p) const
{
    const float top = railH + 1.0f;
    return juce::jlimit (0.35f, 1.0f, 0.35f + 0.65f * (p.y - top) / ((float) getHeight() - top));
}

void MiniKeyboard::mouseDown (const juce::MouseEvent& e)
{
    mouseNote = noteAt (e.position);
    proc.uiNoteOn (mouseNote, velocityAt (e.position));
    repaint();
}

void MiniKeyboard::mouseDrag (const juce::MouseEvent& e)
{
    const int n = noteAt (e.position);
    if (n == mouseNote || mouseNote < 0) return;
    // new note first, then release the old one: an overlapping (legato) move = the 808 slide
    proc.uiNoteOn (n, velocityAt (e.position));
    proc.uiNoteOff (mouseNote);
    mouseNote = n;
    repaint();
}

void MiniKeyboard::mouseUp (const juce::MouseEvent&)
{
    if (mouseNote >= 0) proc.uiNoteOff (mouseNote);
    mouseNote = -1;
    repaint();
}

void MiniKeyboard::paint (juce::Graphics& g)
{
    const int hi = lowNote + numWhite / 7 * 12;
    auto held = [&] (int n) { return n == mouseNote || proc.isNoteHeld (n); };

    // white keys
    for (int n = lowNote; n < hi; ++n)
    {
        if (isBlack (n)) continue;
        const auto r = keyRect (n).reduced (0.6f, 0.0f);
        const bool on = held (n);
        if (on)
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff8f98), 0, r.getY(), cRed.darker (0.15f), 0, r.getBottom(), false));
        else
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9ced6), 0, r.getY(), juce::Colour (0xfff2f4f7), 0, r.getBottom() - 6.0f, false));
        g.fillRoundedRectangle (r.withTrimmedTop (-3.0f), 2.5f);
        // front lip
        g.setColour (on ? cRed.darker (0.5f) : juce::Colour (0xff9aa0aa));
        g.fillRect (r.withTop (r.getBottom() - 4.0f).reduced (1.0f, 0.0f));
        if (n % 12 == 0)
        {
            g.setFont (fnt (Fonts::get().label, 10.5f));
            g.setColour (on ? juce::Colours::white : juce::Colour (0xff6c727c));
            g.drawText ("C" + juce::String (n / 12 - 1), r.withTrimmedBottom (5.0f).withTop (r.getBottom() - 19.0f), juce::Justification::centred);
        }
    }
    for (int n = lowNote; n < hi; ++n)
        if (! isBlack (n) && held (n))
        {
            g.setColour (cRed.withAlpha (0.18f));
            g.fillRoundedRectangle (keyRect (n).expanded (3.0f, 0.0f).withTrimmedTop (-2.0f), 4.0f);
        }
    // shade under the felt
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0, railH + 1.0f, juce::Colours::transparentBlack, 0, railH + 9.0f, false));
    g.fillRect (0.0f, railH + 1.0f, (float) getWidth(), 8.0f);

    // black keys
    for (int n = lowNote; n < hi; ++n)
    {
        if (! isBlack (n)) continue;
        const auto r = keyRect (n);
        const bool on = held (n);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (1.5f, 2.0f), 2.0f);
        g.setGradientFill (juce::ColourGradient (on ? cRed.brighter (0.2f) : juce::Colour (0xff2a2c31), 0, r.getY(),
                                                 on ? cRed.darker (0.6f) : juce::Colour (0xff060607), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.12f));
        g.fillRoundedRectangle (r.reduced (2.5f, 0.0f).withTrimmedTop (2.0f).withHeight (r.getHeight() * 0.55f), 1.5f);
        g.setColour (on ? cRed.darker (0.8f) : juce::Colour (0xff000000));
        g.fillRect (r.withTop (r.getBottom() - 5.0f).reduced (1.5f, 0.0f));
    }
    // red glow around held black keys
    for (int n = lowNote; n < hi; ++n)
        if (isBlack (n) && held (n))
        {
            const auto r = keyRect (n);
            g.setColour (cRed.withAlpha (0.16f));
            g.fillRoundedRectangle (r.expanded (3.0f, 0.0f).withTrimmedTop (-2.0f), 4.0f);
        }

    // pitch rail: where the 808 actually is (follows glide and bend)
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (0.0f, 1.0f, (float) getWidth(), railH - 2.0f, 3.0f);
    if (proc.uiVoices.load() > 0 && proc.uiHz.load() > 1.0f)
    {
        const float x = centreX (proc.uiNote.load());
        const float cy = railH * 0.5f;
        g.setColour (cRed.withAlpha (0.25f));
        g.fillRoundedRectangle (juce::Rectangle<float> (26.0f, railH - 1.0f).withCentre ({ x, cy }), 4.0f);
        g.setColour (cRed.withAlpha (0.55f));
        g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, railH - 3.0f).withCentre ({ x, cy }), 3.0f);
        g.setColour (juce::Colour (0xffffd6da));
        g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, railH - 3.0f).withCentre ({ x, cy }), 2.0f);
    }
}

// ================= Panel =================

SubPanel::SubPanel (SubKillaProcessor& p)
    : proc (p), scope (p), satSelector (*p.apvts.getParameter (IDs::satType), p), keyboard (p)
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
    setupKnob (glideKnob, IDs::glide, "GLIDE: slide time between overlapping notes (the trap 808 slide). Constant time, whatever the interval.");

    const char* bodyIds[] = { IDs::punch, IDs::punchTime, IDs::click, IDs::sub, IDs::tune, IDs::fine };
    const char* bodyTips[] = { "Punch: how far above the note the pitch envelope starts (semitones)",
                               "Punch time: how fast the pitch drops onto the note",
                               "Click: transient layer at the start of each hit (cuts through on phones)",
                               "Sub octave: blends in a sine one octave below",
                               "Tune in semitones",
                               "Fine tune in cents" };
    for (size_t i = 0; i < bodyKnobs.size(); ++i) setupKnob (bodyKnobs[i], bodyIds[i], bodyTips[i]);
    bodyKnobs[4].s.getProperties().set ("bipolar", true);
    bodyKnobs[5].s.getProperties().set ("bipolar", true);
    bodyKnobs[4].s.setDoubleClickReturnValue (true, 0.0);
    bodyKnobs[5].s.setDoubleClickReturnValue (true, 0.0);

    const char* deckIds[] = { IDs::cutoff, IDs::reso, IDs::fltEnv, IDs::drive, IDs::distMix, IDs::attack, IDs::decay, IDs::sustain, IDs::release, IDs::width, IDs::output };
    const char* deckTips[] = { "Low-pass cutoff (OPEN at maximum)",
                               "Filter resonance",
                               "How far the amp envelope opens the filter: the note darkens as it decays",
                               "Saturation drive (type selected above). Level is compensated automatically.",
                               "Distortion dry / wet",
                               "Amp attack",
                               "Amp decay: time to fall to -60 dB (long 808 tails up to 12 s)",
                               "Sustain level while the key is held (0 = classic decaying 808)",
                               "Release after the key is let go",
                               "Stereo width of the harmonics above ~200 Hz. The sub always stays mono.",
                               "Output level (into the limiter)" };
    for (size_t i = 0; i < deckKnobs.size(); ++i) setupKnob (deckKnobs[i], deckIds[i], deckTips[i]);
    deckKnobs[3].s.getProperties().set ("danger", 0.7);
    deckKnobs[10].s.getProperties().set ("danger", 24.0 / 36.0);
    deckKnobs[10].s.setDoubleClickReturnValue (true, 0.0);

    addAndMakeVisible (scope);
    satSelector.setTooltip ("Saturation type: TUBE warm even harmonics, TAPE smooth, HARD clip, FOLD wavefolder");
    addAndMakeVisible (satSelector);
    addAndMakeVisible (keyboard);

    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::mode), [this] (float v)
    {
        modeState = juce::jlimit (0, 2, juce::roundToInt (v));
        refreshModeButtons();
    }, nullptr);
    const char* modeTips[] = { "Mono: every note retriggers the envelope; overlapping notes still glide",
                               "Legato: overlapping notes slide without retriggering (classic trap 808)",
                               "Poly: up to 4 voices, no glide" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.onClick = [this, i] { modeAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip (modeTips[i]);
        addAndMakeVisible (b);
    }
    modeAttachment->sendInitialUpdate();

    limiterToggle.setTooltip ("Soft limiter on the output: never goes over -0.3 dBFS");
    addAndMakeVisible (limiterToggle);
    limiterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::limiter, limiterToggle);

    // presets
    const auto& presets = subkilla::factoryPresets();
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

    startTimerHz (30);
}

SubPanel::~SubPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SubPanel::refreshModeButtons()
{
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setToggleState (i == modeState, juce::dontSendNotification);
    glideKnob.s.setEnabled (modeState != 2);
    repaint();
}

void SubPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    scope.repaint();
    keyboard.repaint();
    satSelector.repaint();
    repaint (glideLcd.toNearestInt().expanded (4));
}

void SubPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // bloom behind the lit saturation button
    const auto r = SatSelector::cell (satSelector.getSelected()).translated ((float) satRect.getX(), (float) satRect.getY());
    for (int k = 3; k >= 1; --k)
    {
        g.setColour (cRed.withAlpha (0.06f * (float) (4 - k)));
        g.fillRoundedRectangle (r.expanded ((float) k * 3.0f), 8.0f + (float) k * 3.0f);
    }
}

void SubPanel::paintOverChildren (juce::Graphics& g)
{
    // ---- glide LCD
    {
        const bool poly = modeState == 2;
        g.setFont (fnt (Fonts::get().mono, 18.0f));
        const auto txt = poly ? juce::String ("POLY") : glideKnob.s.getTextFromValue (glideKnob.s.getValue());
        glowText (g, txt, glideLcd.withTrimmedLeft (8.0f).withTrimmedRight (20.0f), juce::Justification::centred, poly ? cIce.withAlpha (0.4f) : cIce);
        const bool sliding = proc.uiGliding.load();
        const auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ glideLcd.getRight() - 12.0f, glideLcd.getCentreY() });
        if (sliding)
        {
            g.setColour (cRed.withAlpha (0.35f));
            g.fillEllipse (led.expanded (4.0f));
        }
        g.setColour (sliding ? cRed.brighter (0.3f) : juce::Colour (0xff3a1e22));
        g.fillEllipse (led);
    }

    // ---- small knob values
    g.setFont (fnt (Fonts::get().mono, 12.5f));
    auto value = [&] (Knob& k, juce::Point<float> c)
    {
        g.setColour (cText.withAlpha (0.88f));
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (74.0f, 14.0f).withCentre (c), juce::Justification::centred);
    };
    for (size_t i = 0; i < bodyKnobs.size(); ++i)
        value (bodyKnobs[i], { bodyX[i % 3], bodyY[i / 3] + 38.0f });
    for (size_t i = 0; i < deckKnobs.size(); ++i)
        value (deckKnobs[i], { deckX[i], deckY + 38.0f });
}

void SubPanel::resized()
{
    glideKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (glideC).toNearestInt());
    for (size_t i = 0; i < bodyKnobs.size(); ++i)
        bodyKnobs[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ bodyX[i % 3], bodyY[i / 3] }).toNearestInt());
    for (size_t i = 0; i < deckKnobs.size(); ++i)
        deckKnobs[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ deckX[i], deckY }).toNearestInt());

    scope.setBounds (scopeRect);
    satSelector.setBounds (satRect);
    keyboard.setBounds (keyRectArea);
    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setBounds (576 + 75 * i, 292, 70, 24);
    limiterToggle.setBounds (686, 428, 76, 16);

    prevButton.setBounds (470, 28, 30, 30);
    presetBox.setBounds (504, 28, 250, 30);
    nextButton.setBounds (758, 28, 30, 30);
}

// ================= Editor =================

SubKillaEditor::SubKillaEditor (SubKillaProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (SubPanel::baseW * 3 / 4, SubPanel::baseH * 3 / 4, SubPanel::baseW * 2, SubPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) SubPanel::baseW / SubPanel::baseH);
    setSize (SubPanel::baseW, SubPanel::baseH);
}

void SubKillaEditor::resized()
{
    panel.setBounds (0, 0, SubPanel::baseW, SubPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / SubPanel::baseW));
}
