#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText  { 0xffe2e8f2 };
const juce::Colour cInk   { 0xff808a9c };
const juce::Colour cBlue  { 0xff2f7bff };
const juce::Colour cIce   { 0xff9cc2ff };
const juce::Colour cHot   { 0xffff4f6a };
const juce::Colour cDeep  { 0xff13161d };

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
const juce::Point<float> retuneC { 111.0f, 200.0f };
constexpr float bigKnob = 168.0f, smallKnob = 64.0f, smallY = 436.0f;
float smallX (int i) { return 68.0f + 103.4f * (float) i; }
const juce::Rectangle<float> retuneLcd { 51.0f, 292.0f, 120.0f, 28.0f };
const juce::Rectangle<float> latencyLcd { 136.0f, 337.0f, 64.0f, 28.0f };
const juce::Rectangle<int> graphRect { 223, 101, 414, 216 };
const juce::Rectangle<int> noteRect { 659, 101, 180, 100 };
const juce::Rectangle<int> keysRect { 222, 334, 416, 34 };

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c, float spread = 0.16f)
{
    g.setColour (c.withAlpha (spread));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (dx, dy), j);
    g.setColour (c);
    g.drawText (t, r, j);
}

juce::String noteName (int midi)
{
    if (midi < 0) return "--";
    return juce::String (akill::keyNames[(size_t) (midi % 12)]) + juce::String (midi / 12 - 1);
}

juce::String keyText (int key, int scale)
{
    return juce::String (akill::keyNames[(size_t) juce::jlimit (0, 11, key)]) + " " + juce::String (akill::scaleNames[(size_t) juce::jlimit (0, akill::numScales - 1, scale)]).toUpperCase();
}
} // namespace

// ================= Look =================

AKLook::AKLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cBlue);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, cIce);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cBlue.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void AKLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f;
    const bool big = R > 60.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cBlue : cInk;

    if (big)
    {
        const float arcR = R - 23.0f;
        const float a0 = bipolar ? (start + end) * 0.5f : start;
        const float lo = juce::jmin (a0, angle), hi = juce::jmax (a0, angle);
        if (hi > lo)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, lo, hi, true);
            g.setColour (on.withAlpha (0.22f));
            g.strokePath (arc, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (on.brighter (0.15f));
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
            g.setColour (on.withAlpha (0.25f));
            g.fillEllipse (lx - 3.6f, ly - 3.6f, 7.2f, 7.2f);
            g.setColour (on.brighter (0.3f));
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
    g.setColour (on.withAlpha (0.4f));
    g.strokePath (p, juce::PathStrokeType (big ? 4.0f : 2.5f), tr);
    g.setColour (on.brighter (0.5f));
    g.fillPath (p, tr);
}

void AKLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff4f5d78) : juce::Colour (0xff363e4e));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0d12), 0, 0, juce::Colour (0xff151922), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cBlue.brighter (0.3f));
    g.fillPath (arrow);
}

void AKLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font AKLook::getComboBoxFont (juce::ComboBox& b) { return fnt (Fonts::get().semi, b.getHeight() > 28 ? 17.0f : 16.0f); }
juce::Font AKLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void AKLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cBlue.withAlpha (0.22f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff5d9bff) : juce::Colour (down ? 0xff1e222b : 0xff2e3440), 0, 0,
                                             on ? juce::Colour (0xff1c55c8) : juce::Colour (0xff171a21), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (on ? juce::Colour (0xffb9d3ff) : juce::Colour (over ? 0xff5a6680 : 0xff3c4456));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void AKLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colours::white : cText.withAlpha (b.isEnabled() ? 0.85f : 0.4f));
    g.setFont (fnt (Fonts::get().label, b.getHeight() > 27 ? 15.0f : 14.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ================= PitchGraph =================

PitchGraph::PitchGraph (AutoKillaProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    tick();
}

void PitchGraph::tick()
{
    count = proc.getEngine().readFrames (frames.data(), numFrames);
    // follow the singer: centre the view on the recent corrected pitch
    float sum = 0.0f; int n = 0;
    for (int i = juce::jmax (0, count - 120); i < count; ++i)
        if (frames[(size_t) i].corrected > 0.0f) { sum += frames[(size_t) i].corrected; ++n; }
    if (n > 8)
    {
        const float want = sum / (float) n;
        if (std::abs (want - viewCentre) > 3.0f) viewCentre += 0.12f * (want - viewCentre);
        else viewCentre += 0.03f * (want - viewCentre);
    }
    repaint();
}

void PitchGraph::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const auto& e = proc.getEngine();
    const int mask = e.uiMask.load();
    const auto plot = juce::Rectangle<float> (40.0f, 24.0f, (float) getWidth() - 50.0f, (float) getHeight() - 32.0f);
    constexpr float span = 13.0f;   // semitones visible
    const float lo = viewCentre - span * 0.5f;
    auto yFor = [&] (float midi) { return plot.getBottom() - (midi - lo) / span * plot.getHeight(); };

    // ---- readout strip
    g.setFont (fnt (Fonts::get().mono, 13.5f));
    const auto strip = juce::Rectangle<float> (10.0f, 1.0f, (float) getWidth() - 20.0f, 17.0f);
    juce::String keyStr;
    if (p.midiTarget && proc.uiMidiMask.load() != 0) keyStr = "MIDI TARGET NOTES";
    else if (p.autoKey && e.uiDetectedKey.load() >= 0) keyStr = "AUTO " + keyText (e.uiDetectedKey.load(), e.uiDetectedMinor.load() ? 1 : 0);
    else if (p.autoKey) keyStr = "AUTO KEY  LISTENING " + juce::String (juce::roundToInt (e.uiKeyProgress.load() * 100.0f)) + "%";
    else keyStr = keyText (p.key, p.scale);
    glowText (g, keyStr, strip, juce::Justification::centredLeft, cIce);
    const float corrC = e.uiCorrectionCents.load();
    const bool voicedNow = e.uiDetectedMidi.load() > 0.0f;
    glowText (g, voicedNow ? ("FIX " + juce::String (corrC > 0.5f ? "+" : "") + juce::String (juce::roundToInt (corrC)) + " ct") : juce::String ("FIX  --"),
              strip, juce::Justification::centredRight, voicedNow ? cBlue.brighter (0.4f) : cInk);

    // ---- 1 s time grid
    for (int k = 1; k < 4; ++k)
    {
        const float x = plot.getRight() - plot.getWidth() * (float) k / 4.0f;
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillRect (juce::Rectangle<float> (x, plot.getY(), 1.0f, plot.getHeight()));
    }

    // ---- semitone grid
    g.saveState();
    g.reduceClipRegion (juce::Rectangle<float> (0.0f, plot.getY() - 2.0f, (float) getWidth(), plot.getHeight() + 4.0f).toNearestInt());
    const int curTarget = e.uiTargetNote.load();
    g.setFont (fnt (Fonts::get().label, 11.5f));
    for (int n = (int) std::floor (lo) - 1; n <= (int) std::ceil (lo + span) + 1; ++n)
    {
        const float y = yFor ((float) n);
        const bool allowed = (mask >> (((n % 12) + 12) % 12)) & 1;
        const bool isTarget = n == curTarget;
        if (isTarget)
        {
            const float hh = plot.getHeight() / span;
            g.setColour (cBlue.withAlpha (0.13f));
            g.fillRect (juce::Rectangle<float> (plot.getX(), y - hh * 0.5f, plot.getWidth(), hh));
        }
        g.setColour (allowed ? cBlue.withAlpha (isTarget ? 0.75f : 0.32f) : juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        if (allowed && y > plot.getY() + 5.0f && y < plot.getBottom() - 5.0f)
        {
            g.setColour (isTarget ? cText : cIce.withAlpha (0.65f));
            g.drawText (noteName (n), juce::Rectangle<float> (2.0f, y - 7.0f, 34.0f, 14.0f), juce::Justification::centredRight);
        }
    }

    // ---- pitch traces (newest at the right edge)
    const float dx = plot.getWidth() / (float) (numFrames - 1);
    auto trace = [&] (bool corrected) -> juce::Path
    {
        juce::Path path;
        bool open = false;
        for (int i = 0; i < count; ++i)
        {
            const auto& f = frames[(size_t) i];
            const float v = corrected ? f.corrected : f.detected;
            const float x = plot.getRight() - (float) (count - 1 - i) * dx;
            if (v <= 0.0f) { open = false; continue; }
            const float y = yFor (v);
            if (! open) { path.startNewSubPath (x, y); open = true; }
            else path.lineTo (x, y);
        }
        return path;
    };
    const auto det = trace (false), cor = trace (true);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.strokePath (det, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cBlue.withAlpha (0.22f));
    g.strokePath (cor, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cBlue.brighter (0.25f));
    g.strokePath (cor, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (count > 0 && frames[(size_t) count - 1].corrected > 0.0f)
    {
        const float y = yFor (frames[(size_t) count - 1].corrected);
        g.setColour (cBlue.withAlpha (0.35f));
        g.fillEllipse (plot.getRight() - 7.0f, y - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (plot.getRight() - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }
    g.restoreState();

    // legend
    g.setFont (fnt (Fonts::get().label, 11.0f));
    const float ly = (float) getHeight() - 12.0f;
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (plot.getX(), ly + 5.0f, 14.0f, 1.6f));
    g.setColour (cInk);
    g.drawText ("DETECTED", juce::Rectangle<float> (plot.getX() + 18.0f, ly, 60.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour (cBlue.brighter (0.25f));
    g.fillRect (juce::Rectangle<float> (plot.getX() + 80.0f, ly + 4.5f, 14.0f, 2.6f));
    g.setColour (cIce.withAlpha (0.85f));
    g.drawText ("CORRECTED", juce::Rectangle<float> (plot.getX() + 98.0f, ly, 70.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour (cInk);
    g.drawText ("4 SEC", juce::Rectangle<float> (plot.getRight() - 60.0f, ly, 60.0f, 12.0f), juce::Justification::centredRight);
}

// ================= NoteDisplay =================

NoteDisplay::NoteDisplay (AutoKillaProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

void NoteDisplay::tick()
{
    const auto& e = proc.getEngine();
    const float det = e.uiDetectedMidi.load();
    const int tn = e.uiTargetNote.load();
    const auto now = juce::Time::getMillisecondCounter();
    if (det > 0.0f && tn >= 0)
    {
        shownNote = tn;
        shownCents += 0.35f * (juce::jlimit (-60.0f, 60.0f, (det - (float) tn) * 100.0f) - shownCents);
        const float out = e.uiCorrectedMidi.load();
        inTune += 0.3f * ((std::abs (out - (float) tn) < 0.06f ? 1.0f : 0.0f) - inTune);
        lastVoiced = now;
    }
    else if (now - lastVoiced > 700)
    {
        shownNote = -1;
        inTune *= 0.8f;
        shownCents *= 0.8f;
    }
    repaint();
}

void NoteDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const bool live = shownNote >= 0;
    // big note
    const juce::String name = live ? juce::String (akill::keyNames[(size_t) (shownNote % 12)]) : juce::String ("--");
    const juce::String oct = live ? juce::String (shownNote / 12 - 1) : juce::String();
    const auto noteArea = juce::Rectangle<float> (12.0f, 10.0f, 104.0f, 58.0f);
    g.setFont (fnt (Fonts::get().heavy, 46.0f));
    const auto col = live ? cText : cInk.withAlpha (0.5f);
    if (live)
    {
        g.setColour (cBlue.withAlpha (0.35f));
        for (auto [dx, dy] : { std::pair { -2.0f, 0.0f }, { 2.0f, 0.0f }, { 0.0f, -2.0f }, { 0.0f, 2.0f } })
            g.drawText (name, noteArea.translated (dx, dy), juce::Justification::centredLeft);
    }
    g.setColour (col);
    g.drawText (name, noteArea, juce::Justification::centredLeft);
    const float nameW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name);
    g.setFont (fnt (Fonts::get().label, 22.0f));
    g.setColour (live ? cIce : cInk.withAlpha (0.5f));
    g.drawText (oct, juce::Rectangle<float> (noteArea.getX() + nameW + 4.0f, 38.0f, 30.0f, 24.0f), juce::Justification::centredLeft);

    // in-tune lamp
    const auto lamp = juce::Rectangle<float> (r.getRight() - 58.0f, 14.0f, 46.0f, 18.0f);
    g.setColour (cBlue.withAlpha (0.12f + 0.6f * inTune));
    g.fillRoundedRectangle (lamp, 4.0f);
    g.setColour (cBlue.withAlpha (0.35f + 0.65f * inTune));
    g.drawRoundedRectangle (lamp, 4.0f, 1.0f);
    g.setFont (fnt (Fonts::get().label, 12.0f));
    g.setColour (inTune > 0.5f ? juce::Colours::white : cInk);
    g.drawText ("LOCKED", lamp, juce::Justification::centred);
    g.setFont (fnt (Fonts::get().mono, 13.0f));
    g.setColour (live ? cIce : cInk.withAlpha (0.5f));
    g.drawText (live ? ((shownCents >= 0.5f ? "+" : "") + juce::String (juce::roundToInt (shownCents)) + " ct") : juce::String ("-- ct"),
                juce::Rectangle<float> (r.getRight() - 70.0f, 38.0f, 58.0f, 16.0f), juce::Justification::centredRight);

    // cents meter (singer's raw deviation from the target note)
    const auto meter = juce::Rectangle<float> (12.0f, 74.0f, r.getWidth() - 24.0f, 12.0f);
    const int segs = 25;
    const float sw = meter.getWidth() / (float) segs;
    const int lit = juce::roundToInt ((juce::jlimit (-50.0f, 50.0f, shownCents) + 50.0f) / 100.0f * (float) (segs - 1));
    for (int i = 0; i < segs; ++i)
    {
        const auto s = juce::Rectangle<float> (meter.getX() + (float) i * sw + 1.0f, meter.getY(), sw - 2.0f, meter.getHeight());
        const bool centre = i == segs / 2;
        const bool on = live && ((i >= juce::jmin (lit, segs / 2) && i <= juce::jmax (lit, segs / 2)));
        const auto base = centre ? juce::Colours::white : (std::abs (i - segs / 2) > 8 ? cHot : cBlue);
        g.setColour (on ? base.withAlpha (centre ? 0.95f : 0.85f) : base.withAlpha (centre ? 0.25f : 0.10f));
        g.fillRoundedRectangle (s, 1.5f);
    }
    g.setFont (fnt (Fonts::get().label, 10.5f));
    g.setColour (cInk);
    g.drawText ("-50", juce::Rectangle<float> (meter.getX(), 87.0f, 30.0f, 11.0f), juce::Justification::centredLeft);
    g.drawText ("SINGER", juce::Rectangle<float> (meter.getCentreX() - 30.0f, 87.0f, 60.0f, 11.0f), juce::Justification::centred);
    g.drawText ("+50", juce::Rectangle<float> (meter.getRight() - 30.0f, 87.0f, 30.0f, 11.0f), juce::Justification::centredRight);
}

// ================= NoteKeyboard =================

NoteKeyboard::NoteKeyboard (AutoKillaProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Lit keys = notes the tuner snaps to. Click a key to switch it on / off (creates a Custom scale).");
    tick();
}

juce::Rectangle<float> NoteKeyboard::keyRect (int pc) const
{
    static const int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const float ww = (float) getWidth() / 7.0f, hgt = (float) getHeight();
    if (! isBlack (pc))
        return { ww * (float) whiteIndex[pc], 0.0f, ww, hgt };
    const float bw = ww * 0.58f;
    const int leftWhite = whiteIndex[pc - 1];
    return { ww * (float) (leftWhite + 1) - bw * 0.5f, 0.0f, bw, hgt * 0.6f };
}

int NoteKeyboard::keyAt (juce::Point<float> pt) const
{
    for (int pc = 0; pc < 12; ++pc)
        if (isBlack (pc) && keyRect (pc).contains (pt)) return pc;
    for (int pc = 0; pc < 12; ++pc)
        if (! isBlack (pc) && keyRect (pc).contains (pt)) return pc;
    return -1;
}

void NoteKeyboard::tick()
{
    const auto& e = proc.getEngine();
    const int m = e.uiMask.load();
    const int t = e.uiDetectedMidi.load() > 0.0f ? e.uiTargetNote.load() : -1;
    const int mm = proc.uiMidiMask.load();
    // when the engine is idle (no audio) show the mask the current parameters imply
    const auto p = proc.currentParams();
    const int implied = (p.midiTarget && mm) ? mm : akill::scaleMask (p.scale, p.key, p.customMask);
    const int show = (p.autoKey && e.uiDetectedKey.load() >= 0) ? m : implied;
    if (show != mask || t != target || mm != midiMask) { mask = show; target = t; midiMask = mm; repaint(); }
}

void NoteKeyboard::mouseMove (const juce::MouseEvent& ev)
{
    const int k = keyAt (ev.position);
    if (k != hover) { hover = k; repaint(); }
}

void NoteKeyboard::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

void NoteKeyboard::mouseDown (const juce::MouseEvent& ev)
{
    const int pc = keyAt (ev.position);
    if (pc < 0) return;
    auto p = proc.currentParams();
    int m = p.scale == akill::scaleCustom ? p.customMask : akill::scaleMask (p.scale, p.key, p.customMask);
    m ^= 1 << pc;
    for (int i = 0; i < 12; ++i)
        if (auto* prm = proc.apvts.getParameter (IDs::note (i)))
            prm->setValueNotifyingHost (((m >> i) & 1) ? 1.0f : 0.0f);
    if (p.scale != akill::scaleCustom)
        if (auto* prm = proc.apvts.getParameter (IDs::scale))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) akill::scaleCustom));
    tick();
}

void NoteKeyboard::paint (juce::Graphics& g)
{
    const int tpc = target >= 0 ? target % 12 : -1;
    auto drawKey = [&] (int pc)
    {
        auto r = keyRect (pc).reduced (isBlack (pc) ? 0.0f : 1.0f, 0.0f);
        const bool on = (mask >> pc) & 1;
        const bool isT = pc == tpc;
        if (! isBlack (pc))
        {
            g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffdfe9ff) : juce::Colour (0xff6c7380), 0, r.getY(),
                                                     on ? juce::Colour (0xffa9c4f5) : juce::Colour (0xff3d424c), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 3.0f);
            if (on)
            {
                g.setColour (cBlue.withAlpha (isT ? 0.75f : 0.35f));
                g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() - 6.0f).reduced (3.0f, 1.0f), 2.0f);
            }
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
            g.setFont (fnt (Fonts::get().label, 12.0f));
            g.setColour (on ? juce::Colour (0xff122244) : juce::Colour (0xff22252b));
            g.drawText (akill::keyNames[(size_t) pc], r.withTrimmedTop (r.getHeight() * 0.55f).withTrimmedBottom (5.0f).translated (0, 0), juce::Justification::centred);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff3b78e6) : juce::Colour (0xff22252c), 0, r.getY(),
                                                     on ? juce::Colour (0xff163a86) : juce::Colour (0xff0c0d10), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.5f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 2.5f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.25f : 0.07f));
            g.fillRoundedRectangle (r.reduced (2.0f).withHeight (3.0f), 1.5f);
        }
        if (isT)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (r.reduced (1.5f), 3.0f, 2.0f);
        }
        if (pc == hover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (r, 3.0f);
        }
        if ((midiMask >> pc) & 1)
        {
            const auto c = juce::Point<float> (r.getCentreX(), r.getY() + 7.0f);
            g.setColour (cHot);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c));
        }
    };
    for (int pc = 0; pc < 12; ++pc) if (! isBlack (pc)) drawKey (pc);
    for (int pc = 0; pc < 12; ++pc) if (isBlack (pc)) drawKey (pc);
}

// ================= Panel =================

AutoKillaPanel::AutoKillaPanel (AutoKillaProcessor& p)
    : proc (p), graph (p), noteDisplay (p), keyboard (p)
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
    setupKnob (retuneKnob, IDs::retune, "How fast notes snap to the target. Fully left = instant hard tune (robotic), right = slow natural glide.");
    const char* ids[] = { IDs::humanize, IDs::threshold, IDs::vibrato, IDs::vibRate, IDs::transpose, IDs::throat, IDs::mix, IDs::output };
    const char* tips[] = { "Keeps the singer's own vibrato and drift around the corrected note (0 = everything flattened)",
                           "Only notes within this distance of a target are corrected. ALL = correct everything; lower lets slides and bends through",
                           "Adds vibrato to the corrected note (fades in after each note starts)",
                           "Speed of the added vibrato",
                           "Shifts the whole vocal in semitones (formants stay natural with FORMANT on)",
                           "Throat / formant shift: lower = bigger, darker voice, higher = smaller, brighter voice",
                           "Dry / tuned balance (latency compensated). 50 % with TRANSPOSE = harmony / octave stack",
                           "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
        setupKnob (knobs[i], ids[i], tips[i]);
    knobs[4].s.getProperties().set ("bipolar", true);
    knobs[5].s.getProperties().set ("bipolar", true);
    knobs[4].s.setDoubleClickReturnValue (true, 0.0);
    knobs[5].s.setDoubleClickReturnValue (true, 0.0);
    knobs[7].s.setDoubleClickReturnValue (true, 0.0);
    knobs[4].s.setVelocityBasedMode (false);
    knobs[4].s.setMouseDragSensitivity (280);

    addAndMakeVisible (graph);
    addAndMakeVisible (noteDisplay);
    addAndMakeVisible (keyboard);

    // mode selector
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::mode), [this] (float v)
    {
        modeState = v > 0.5f ? 1 : 0;
        refreshMode();
    }, nullptr);
    const char* modeNames[] = { "LIVE", "MIX" };
    const char* modeTips[] = { "LIVE: low-latency tracking mode (about 17 ms) for recording and monitoring through the tuner",
                               "MIX: highest quality (about 42 ms, compensated by the DAW) for mixing" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.setTooltip (modeTips[i]);
        b.onClick = [this, i] { modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modeAttachment->sendInitialUpdate();

    // key / scale
    for (int i = 0; i < 12; ++i) keyBox.addItem (akill::keyNames[(size_t) i], i + 1);
    for (int i = 0; i < akill::numScales; ++i) scaleBox.addItem (akill::scaleNames[(size_t) i], i + 1);
    keyBox.setTooltip ("Key of the song");
    scaleBox.setTooltip ("Scale: the notes the tuner snaps to. Custom = pick the notes on the keyboard below the graph");
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::key, keyBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::scale, scaleBox);

    auto setupToggle = [this] (juce::TextButton& b, const char* id, const char* tip, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att)
    {
        b.setClickingTogglesState (true);
        b.setTooltip (tip);
        addAndMakeVisible (b);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);
    };
    setupToggle (autoKeyButton, IDs::autoKey, "AUTO KEY: listens to the last ~10 s of singing, finds the key and tunes to it", autoKeyAttachment);
    setupToggle (formantButton, IDs::formant, "FORMANT: keeps the natural voice colour when the pitch moves (off = classic chipmunk / monster resample)", formantAttachment);
    setupToggle (midiButton, IDs::midi, "MIDI NOTES: route a MIDI track to the plugin - held notes become the only target notes", midiAttachment);
    setKeyButton.setTooltip ("Detected key - click to copy it into KEY / SCALE");
    setKeyButton.onClick = [this] { proc.applyDetectedKey(); };
    addAndMakeVisible (setKeyButton);

    // presets
    const auto& presets = akill::factoryPresets();
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

    startTimerHz (30);
}

AutoKillaPanel::~AutoKillaPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AutoKillaPanel::refreshMode()
{
    for (int i = 0; i < 2; ++i)
        modeButtons[(size_t) i].setToggleState (i == modeState, juce::dontSendNotification);
    repaint();
}

void AutoKillaPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    graph.tick();
    noteDisplay.tick();
    keyboard.tick();

    const auto& e = proc.getEngine();
    const int k = e.uiDetectedKey.load();
    juce::String t = k >= 0 ? juce::String (akill::keyNames[(size_t) k]) + (e.uiDetectedMinor.load() ? " MIN" : " MAJ")
                            : (autoKeyButton.getToggleState() ? juce::String (juce::roundToInt (e.uiKeyProgress.load() * 100.0f)) + " %" : juce::String ("--"));
    if (k >= 0)
    {
        const auto p = proc.currentParams();
        const bool same = p.key == k && p.scale == (e.uiDetectedMinor.load() ? 1 : 0);
        if (! same) t << "  SET";
    }
    if (setKeyButton.getButtonText() != t) setKeyButton.setButtonText (t);
    setKeyButton.setEnabled (k >= 0);
    repaint (retuneLcd.toNearestInt().expanded (2));
    repaint (latencyLcd.toNearestInt().expanded (2));
}

void AutoKillaPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void AutoKillaPanel::paintOverChildren (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    // retune LCD
    g.setFont (fnt (Fonts::get().mono, 20.0f));
    if (p.retuneMs < 0.5f) glowText (g, "HARD TUNE", retuneLcd, juce::Justification::centred, cBlue.brighter (0.45f));
    else glowText (g, juce::String (juce::roundToInt (p.retuneMs)) + " ms", retuneLcd, juce::Justification::centred, cBlue.brighter (0.45f));

    // latency LCD (what the host compensates)
    const double ms = 1000.0 * akill::Engine::latencyFor (p.mode, proc.currentSampleRate) / proc.currentSampleRate;
    g.setFont (fnt (Fonts::get().mono, 14.0f));
    glowText (g, juce::String (ms, 1) + "ms", latencyLcd, juce::Justification::centred, p.mode == akill::Mode::live ? cBlue.brighter (0.45f) : cIce.withAlpha (0.8f));

    // small knob values
    g.setFont (fnt (Fonts::get().mono, 13.0f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        g.setColour (cText.withAlpha (0.9f));
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (90.0f, 14.0f).withCentre ({ smallX ((int) i), 474.0f }), juce::Justification::centred);
    }
}

void AutoKillaPanel::resized()
{
    retuneKnob.s.setBounds (juce::Rectangle<float> (bigKnob, bigKnob).withCentre (retuneC).toNearestInt());
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].s.setBounds (juce::Rectangle<float> (smallKnob, smallKnob).withCentre ({ smallX ((int) i), smallY }).toNearestInt());

    graph.setBounds (graphRect);
    noteDisplay.setBounds (noteRect);
    keyboard.setBounds (keysRect);

    modeButtons[0].setBounds (24, 337, 52, 28);
    modeButtons[1].setBounds (80, 337, 52, 28);

    keyBox.setBounds (714, 217, 122, 27);
    scaleBox.setBounds (714, 251, 122, 27);
    autoKeyButton.setBounds (662, 286, 84, 28);
    setKeyButton.setBounds (750, 286, 86, 28);
    formantButton.setBounds (662, 337, 84, 28);
    midiButton.setBounds (750, 337, 86, 28);

    prevButton.setBounds (500, 30, 30, 30);
    presetBox.setBounds (534, 30, 276, 30);
    nextButton.setBounds (814, 30, 30, 30);
}

// ================= Editor =================

AutoKillaEditor::AutoKillaEditor (AutoKillaProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (AutoKillaPanel::baseW * 3 / 4, AutoKillaPanel::baseH * 3 / 4, AutoKillaPanel::baseW * 2, AutoKillaPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) AutoKillaPanel::baseW / AutoKillaPanel::baseH);
    setSize (AutoKillaPanel::baseW, AutoKillaPanel::baseH);
}

void AutoKillaEditor::resized()
{
    panel.setBounds (0, 0, AutoKillaPanel::baseW, AutoKillaPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / AutoKillaPanel::baseW));
}
