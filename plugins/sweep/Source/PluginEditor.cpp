#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText { 0xffe6ece9 };
const juce::Colour cInk  { 0xff8a9490 };
const juce::Colour cAcc  { 0xff20d98a };   // emerald
const juce::Colour cAcc2 { 0xff7fe8ff };   // filter B / secondary
const juce::Colour cWarm { 0xffffc35a };   // ADSR
const juce::Colour cDeep { 0xff151918 };

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
const juce::Point<float> bigA { 88.0f, 204.0f }, bigB { 772.0f, 204.0f };
constexpr float bigKnob = 136.0f, smallKnob = 50.0f;
constexpr float colA = 204.0f, colB = 656.0f;
const float colY[3] = { 161.0f, 231.0f, 301.0f };
const float lfoKX[2] = { 384.0f, 442.0f };
const float rowY[2] = { 414.0f, 486.0f };
const float modKX[5] = { 520.0f, 590.0f, 660.0f, 730.0f, 800.0f };
const juce::Rectangle<int> screenInner { 265, 97, 330, 190 };
const juce::Rectangle<int> patternInner { 32, 390, 304, 76 };
const juce::Rectangle<float> lcdA { 38.0f, 284.0f, 100.0f, 22.0f }, lcdB { 722.0f, 284.0f, 100.0f, 22.0f };

const char* shapeShort[sweepfx::numShapes] = { "SIN", "TRI", "UP", "DOWN", "SQR", "S&H", "STEPS" };

juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
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

SweepLook::SweepLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cAcc);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff04140c));
    setColour (juce::PopupMenu::headerTextColourId, cAcc);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cAcc.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void SweepLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = (float) juce::jmin (w, h) * 0.5f;
    const bool big = R > 45.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const juce::Colour hue = s.getProperties().contains ("hue") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["hue"]) : cAcc;
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? hue : cInk.withAlpha (0.6f);

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
            g.strokePath (arc, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (on);
            g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
        const int lit = juce::roundToInt (pos * 20.0f);
        const int mid = bipolar ? 10 : 0;
        const float dr = R * 0.9f;
        for (int k = juce::jmin (mid, lit); k <= juce::jmax (mid, lit); ++k)
        {
            const float a = juce::degreesToRadians (225.0f - 270.0f * (float) k / 20.0f);
            const float lx = c.x + std::cos (a) * dr, ly = c.y - std::sin (a) * dr;
            g.setColour (on.withAlpha (0.22f));
            g.fillEllipse (lx - 3.2f, ly - 3.2f, 6.4f, 6.4f);
            g.setColour (on);
            g.fillEllipse (lx - 1.5f, ly - 1.5f, 3.0f, 3.0f);
        }
    }

    const float body = big ? R - 30.0f : R * 0.70f;
    const float imgHalf = body / 0.80f;
    g.drawImage (big ? knobBig : knobSmall, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    const float pw = big ? 3.6f : 2.4f;
    juce::Path p;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.94f, pw, topR * 0.46f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (on.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (big ? 3.6f : 2.3f), tr);
    g.setColour (on.brighter (0.25f));
    g.fillPath (p, tr);
}

void SweepLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff526a60) : juce::Colour (0xff3a4842));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c0e0d), 0, 0, juce::Colour (0xff161a18), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.2f, ax + 4.0f, ay - 2.2f, ax, ay + 3.2f);
    g.setColour (box.isEnabled() ? cAcc : cInk);
    g.fillPath (arrow);
}

void SweepLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, box.isEnabled() ? cText : cInk);
}

juce::Font SweepLook::getComboBoxFont (juce::ComboBox& box) { return fnt (Fonts::get().semi, box.getHeight() < 28 ? 15.0f : 17.0f); }
juce::Font SweepLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void SweepLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cAcc.withAlpha (0.16f));
        g.fillRoundedRectangle (r.expanded (2.0f), 5.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff6cf2b8) : juce::Colour (down ? 0xff1e2422 : 0xff303634), 0, 0,
                                             on ? juce::Colour (0xff12a868) : juce::Colour (0xff181b1a), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? juce::Colour (0xffb8ffe0) : juce::Colour (over ? 0xff5e6e66 : 0xff3e4844));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void SweepLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colour (0xff03140b) : cText.withAlpha (b.isEnabled() ? 0.82f : 0.4f));
    g.setFont (fnt (Fonts::get().label, b.getHeight() < 24 ? 13.0f : 14.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ================= ResponseDisplay =================

ResponseDisplay::ResponseDisplay (SweepProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    shownA = proc.getEngine().cutoffA.load();
    shownB = proc.getEngine().cutoffB.load();
}

void ResponseDisplay::tick()
{
    shownA = proc.getEngine().cutoffA.load();
    shownB = proc.getEngine().cutoffB.load();
    repaint();
}

void ResponseDisplay::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const auto& e = proc.getEngine();
    const double sr = proc.getDisplaySampleRate() > 0.0 ? proc.getDisplaySampleRate() : 48000.0;
    const auto plot = juce::Rectangle<float> (12.0f, 22.0f, (float) getWidth() - 24.0f, 136.0f);
    constexpr float dbTop = 24.0f, dbBot = -42.0f;
    const float fMin = 20.0f, fMax = 20000.0f;
    auto xFor = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / fMin) / std::log (fMax / fMin); };
    auto yFor = [&] (float db) { return plot.getY() + plot.getHeight() * (dbTop - juce::jlimit (dbBot, dbTop, db)) / (dbTop - dbBot); };

    // grid
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const bool major = f == 100.0f || f == 1000.0f || f == 10000.0f;
        g.setColour (cAcc.withAlpha (major ? 0.13f : 0.06f));
        g.drawVerticalLine (juce::roundToInt (xFor (f)), plot.getY(), plot.getBottom());
    }
    for (float db : { 12.0f, 0.0f, -12.0f, -24.0f, -36.0f })
    {
        g.setColour (cAcc.withAlpha (db == 0.0f ? 0.22f : 0.07f));
        g.drawHorizontalLine (juce::roundToInt (yFor (db)), plot.getX(), plot.getRight());
    }
    g.setFont (fnt (Fonts::get().label, 10.5f));
    g.setColour (cInk.withAlpha (0.8f));
    for (auto [f, t] : { std::pair { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
        g.drawText (t, juce::Rectangle<float> (xFor (f) - 15.0f, plot.getBottom() + 1.0f, 30.0f, 10.0f), juce::Justification::centred);
    for (float db : { 12.0f, 0.0f, -24.0f })
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (plot.getRight() - 24.0f, yFor (db) - 11.0f, 22.0f, 10.0f), juce::Justification::centredRight);

    // sweep range of filter A (where the LFO takes it)
    {
        const float span = p.lfoDepth * sweepfx::maxModOctaves * p.a.mod;
        const float lo = p.a.cutoff * std::pow (2.0f, juce::jmin (0.0f, span)), hi = p.a.cutoff * std::pow (2.0f, juce::jmax (0.0f, span));
        if (hi > lo * 1.02f)
        {
            const float x0 = xFor (juce::jlimit (fMin, fMax, lo)), x1 = xFor (juce::jlimit (fMin, fMax, hi));
            g.setGradientFill (juce::ColourGradient (cAcc.withAlpha (0.0f), 0, plot.getY(), cAcc.withAlpha (0.10f), 0, plot.getBottom(), false));
            g.fillRect (juce::Rectangle<float> (x0, plot.getY(), x1 - x0, plot.getHeight()));
            g.setColour (cAcc.withAlpha (0.30f));
            g.drawVerticalLine (juce::roundToInt (x0), plot.getY(), plot.getBottom());
            g.drawVerticalLine (juce::roundToInt (x1), plot.getY(), plot.getBottom());
        }
    }

    // curves: ghost at the base cutoffs, bright at the live (modulated) cutoffs
    auto curve = [&] (float ca, float cb)
    {
        juce::Path path;
        constexpr int N = 150;
        for (int i = 0; i <= N; ++i)
        {
            const float f = fMin * std::pow (fMax / fMin, (float) i / N);
            const float y = yFor (sweepfx::responseDb (p, ca, cb, f, sr));
            const float x = xFor (f);
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        return path;
    };
    const auto ghost = curve (p.a.cutoff, p.b.cutoff);
    g.setColour (cText.withAlpha (0.18f));
    g.strokePath (ghost, juce::PathStrokeType (1.0f));

    const auto live = curve (shownA, shownB);
    auto fill = live;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (cAcc.withAlpha (0.38f), 0, plot.getY(), cAcc.withAlpha (0.02f), 0, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (cAcc.withAlpha (0.25f));
    g.strokePath (live, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cAcc.brighter (0.3f));
    g.strokePath (live, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // cutoff markers
    auto marker = [&] (float f, juce::Colour col, const char* tag)
    {
        const float x = xFor (juce::jlimit (fMin, fMax, f));
        g.setColour (col.withAlpha (0.55f));
        const float dash[] = { 3.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (x, plot.getY(), x, plot.getBottom()), dash, 2, 1.0f);
        g.setColour (col);
        g.fillEllipse (x - 3.5f, plot.getY() - 1.0f, 7.0f, 7.0f);
        g.setFont (fnt (Fonts::get().label, 10.5f));
        g.drawText (tag, juce::Rectangle<float> (x + 4.0f, plot.getY() - 3.0f, 12.0f, 10.0f), juce::Justification::centredLeft);
    };
    if (p.b.on) marker (shownB, cAcc2, "B");
    marker (shownA, cAcc, "A");

    // ---- top strip
    g.setFont (fnt (Fonts::get().mono, 13.0f));
    const auto strip = juce::Rectangle<float> (10.0f, 2.0f, (float) getWidth() - 20.0f, 15.0f);
    glowText (g, "A " + hzText (shownA), strip, juce::Justification::centredLeft, cAcc);
    glowText (g, p.b.on ? "B " + hzText (shownB) : juce::String ("B OFF"), strip, juce::Justification::centredRight, p.b.on ? cAcc2 : cAcc2.withAlpha (0.35f));
    const juce::String mid = p.b.on ? (p.routing == sweepfx::Routing::serial ? "A > B" : "A + B")
                                    : juce::String (sweepfx::filterTypeNames[(int) p.a.type]);
    glowText (g, mid, strip, juce::Justification::centred, cAcc.withAlpha (0.8f));

    // ---- modulation meters
    const float my = (float) getHeight() - 15.0f;
    auto meter = [&] (float x, const char* name, float v, float depth, juce::Colour col)
    {
        g.setFont (fnt (Fonts::get().label, 10.5f));
        g.setColour (std::abs (depth) > 0.001f ? cText.withAlpha (0.75f) : cInk.withAlpha (0.5f));
        g.drawText (name, juce::Rectangle<float> (x, my - 1.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
        const auto bar = juce::Rectangle<float> (x + 30.0f, my + 3.0f, 60.0f, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 2.0f);
        if (std::abs (depth) > 0.001f)
        {
            g.setColour (col);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * juce::jlimit (0.0f, 1.0f, v))), 2.0f);
        }
    };
    meter (12.0f, "LFO", e.lfoOut.load(), p.lfoDepth, cAcc);
    meter (118.0f, "ENV", e.envOut.load(), p.envDepth, cAcc2);
    meter (224.0f, "ADSR", e.adsrOut.load(), p.adsrDepth, cWarm);
}

// ================= StepEditor =================

StepEditor::StepEditor (SweepProcessor& p) : proc (p)
{
    for (int i = 0; i < sweepfx::numSteps; ++i)
        params[(size_t) i] = proc.apvts.getParameter (IDs::step (i));
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

int StepEditor::stepAt (float x) const
{
    return juce::jlimit (0, sweepfx::numSteps - 1, (int) (x / (float) getWidth() * sweepfx::numSteps));
}

void StepEditor::setFromMouse (juce::Point<float> pt, bool zero)
{
    const int st = stepAt (pt.x);
    const float v = zero ? 0.0f : juce::jlimit (0.0f, 1.0f, 1.0f - pt.y / (float) getHeight());
    // fill skipped columns when dragging fast
    const int from = lastStep < 0 ? st : lastStep;
    for (int s = juce::jmin (from, st); s <= juce::jmax (from, st); ++s)
    {
        auto* prm = params[(size_t) s];
        if (! inGesture[(size_t) s]) { prm->beginChangeGesture(); inGesture[(size_t) s] = true; }
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }
    lastStep = st;
    repaint();
}

void StepEditor::mouseDown (const juce::MouseEvent& e)
{
    lastStep = -1;
    setFromMouse (e.position, e.mods.isPopupMenu());
}

void StepEditor::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e.position, e.mods.isPopupMenu()); }

void StepEditor::mouseUp (const juce::MouseEvent&)
{
    for (int s = 0; s < sweepfx::numSteps; ++s)
        if (inGesture[(size_t) s]) { params[(size_t) s]->endChangeGesture(); inGesture[(size_t) s] = false; }
    lastStep = -1;
}

void StepEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto* prm = params[(size_t) stepAt (e.position.x)];
    const float now = prm->convertFrom0to1 (prm->getValue());
    prm->beginChangeGesture();
    prm->setValueNotifyingHost (prm->convertTo0to1 (now > 0.5f ? 0.0f : 1.0f));
    prm->endChangeGesture();
    repaint();
}

void StepEditor::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const bool stepsMode = p.shape == sweepfx::Shape::pattern;
    const float w = (float) getWidth(), h = (float) getHeight();
    const float cw = w / sweepfx::numSteps;
    const float ph = proc.getEngine().lfoPhase.load();
    const int cur = juce::jlimit (0, sweepfx::numSteps - 1, (int) (ph * sweepfx::numSteps));
    const float dim = stepsMode ? 1.0f : 0.32f;

    for (int i = 0; i < sweepfx::numSteps; ++i)
    {
        const float v = juce::jlimit (0.0f, 1.0f, p.pattern[(size_t) i]);
        auto col = juce::Rectangle<float> ((float) i * cw + 1.5f, 0.0f, cw - 3.0f, h);
        const bool active = stepsMode && i == cur;
        if (active)
        {
            g.setColour (cAcc.withAlpha (0.10f));
            g.fillRect (col);
        }
        auto bar = col.withTop (h - v * (h - 2.0f)).withTrimmedBottom (1.0f);
        if (v > 0.001f)
        {
            g.setGradientFill (juce::ColourGradient (cAcc.brighter (active ? 0.5f : 0.1f).withAlpha (dim), 0, bar.getY(),
                                                     cAcc.darker (0.6f).withAlpha (0.8f * dim), 0, h, false));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (cAcc.brighter (0.6f).withAlpha (dim));
            g.fillRect (bar.withHeight (2.0f));
        }
        else
        {
            g.setColour (cAcc.withAlpha (0.18f * dim));
            g.fillRect (col.withTop (h - 2.0f));
        }
    }

    if (! stepsMode)
    {
        // waveform of the selected shape over one cycle + playhead
        juce::Path wave;
        for (int i = 0; i <= 200; ++i)
        {
            const double t = (double) i / 200.0;
            float v = sweepfx::lfoShapeValue (p.shape, t, p.pattern, (juce::uint32) (((i * 16) / 200 + 1) * 2654435761u));
            if (p.shape == sweepfx::Shape::sampleHold) v = 0.15f + 0.7f * v;
            const float x = (float) t * w, y = h - 3.0f - v * (h - 6.0f);
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        g.setColour (cAcc.withAlpha (0.25f));
        g.strokePath (wave, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (cAcc.brighter (0.3f));
        g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float px = ph * w;
    g.setColour (cText.withAlpha (0.55f));
    g.fillRect (juce::Rectangle<float> (px - 0.5f, 0.0f, 1.2f, h));
}

// ================= Panel =================

SweepPanel::Knob& SweepPanel::addKnob (const char* id, juce::Point<float> centre, bool big, const char* tip)
{
    auto k = std::make_unique<Knob>();
    k->centre = centre;
    k->big = big;
    k->s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->s.setTooltip (tip);
    k->s.onValueChange = [this] { repaint(); };
    addAndMakeVisible (k->s);
    k->a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->s);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

SweepPanel::SweepPanel (SweepProcessor& p)
    : proc (p), display (p), steps (p)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    auto bip = [] (Knob& k, double def) { k.s.getProperties().set ("bipolar", true); k.s.setDoubleClickReturnValue (true, def); };

    // filter A
    cutA = &addKnob (IDs::aCutoff, bigA, true, "Filter A cutoff (the modulation sweeps up / down from here)");
    addKnob (IDs::aReso, { colA, colY[0] }, false, "Filter A resonance (guarded: screams, never runs away)");
    addKnob (IDs::aDrive, { colA, colY[1] }, false, "Filter A drive: saturation before the filter");
    bip (addKnob (IDs::aMod, { colA, colY[2] }, false, "How much of the modulation filter A follows (negative = opposite direction)"), 1.0);
    // filter B
    cutB = &addKnob (IDs::bCutoff, bigB, true, "Filter B cutoff");
    bKnobs.push_back (cutB);
    bKnobs.push_back (&addKnob (IDs::bReso, { colB, colY[0] }, false, "Filter B resonance"));
    bKnobs.push_back (&addKnob (IDs::bDrive, { colB, colY[1] }, false, "Filter B drive"));
    auto& bm = addKnob (IDs::bMod, { colB, colY[2] }, false, "How much of the modulation filter B follows (negative = opposite direction, e.g. for gates)");
    bip (bm, 1.0);
    bKnobs.push_back (&bm);
    for (auto* k : bKnobs) k->s.getProperties().set ("hue", (juce::int64) cAcc2.getARGB());

    // LFO
    divKnob = &addKnob (IDs::division, { lfoKX[0], rowY[0] }, false, "LFO rate as a note value, locked to the host tempo (STEPS: length of one step)");
    divKnob->s.setVelocityBasedMode (false);
    divKnob->s.setMouseDragSensitivity (240);
    rateKnob = &addKnob (IDs::rate, { lfoKX[0], rowY[0] }, false, "LFO rate in Hz (STEPS: steps per second)");
    bip (addKnob (IDs::lfoDepth, { lfoKX[1], rowY[0] }, false, "LFO depth: up to +/- 6 octaves of cutoff movement"), 0.0);
    addKnob (IDs::stereo, { lfoKX[0], rowY[1] }, false, "Stereo: offsets the right channel's LFO phase (up to 180 deg) for wide movement");
    addKnob (IDs::glide, { lfoKX[1], rowY[1] }, false, "Glide between steps (STEPS / S&H / SQUARE)");
    // modulation
    bip (addKnob (IDs::envDepth, { modKX[0], rowY[0] }, false, "Envelope follower depth: louder input moves the cutoff (negative = closes)"), 0.0);
    addKnob (IDs::envSpeed, { modKX[1], rowY[0] }, false, "Envelope follower speed");
    addKnob (IDs::sens, { modKX[2], rowY[0] }, false, "Transient sensitivity (ADSR trigger = HIT)");
    addKnob (IDs::mix, { modKX[3], rowY[0] }, false, "Dry / filtered blend. 0 % = untouched");
    addKnob (IDs::output, { modKX[4], rowY[0] }, false, "Output level").s.setDoubleClickReturnValue (true, 0.0);
    addKnob (IDs::attack, { modKX[0], rowY[1] }, false, "ADSR attack");
    addKnob (IDs::decay, { modKX[1], rowY[1] }, false, "ADSR decay");
    addKnob (IDs::sustain, { modKX[2], rowY[1] }, false, "ADSR sustain level");
    addKnob (IDs::release, { modKX[3], rowY[1] }, false, "ADSR release");
    bip (addKnob (IDs::adsrDepth, { modKX[4], rowY[1] }, false, "ADSR depth: how far each MIDI note / hit moves the cutoff"), 0.0);

    addAndMakeVisible (display);
    addAndMakeVisible (steps);
    steps.setTooltip ("Draw the 16-step pattern (used by the STEPS shape)");

    // filter types
    juce::StringArray types;
    for (auto* t : sweepfx::filterTypeNames) types.add (t);
    for (auto* box : { &typeA, &typeB })
    {
        box->addItemList (types, 1);
        addAndMakeVisible (*box);
    }
    typeA.setTooltip ("Filter A type");
    typeB.setTooltip ("Filter B type");
    typeAAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::aType, typeA);
    typeBAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::bType, typeB);

    for (auto* b : { &bOnButton, &syncButton, &retrigButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    bOnButton.setTooltip ("Switch filter B on / off");
    syncButton.setTooltip ("Lock the LFO to the host tempo and song position");
    retrigButton.setTooltip ("Restart the LFO on every ADSR trigger (MIDI note or hit)");
    bOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::bOn, bOnButton);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::sync, syncButton);
    retrigAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::retrig, retrigButton);
    bOnButton.onStateChange = [this] { refreshSelectors(); };
    syncButton.onStateChange = [this] { refreshSelectors(); };

    // radio groups bound to choice parameters
    shapeAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::shape), [this] (float v) { shapeState = juce::roundToInt (v); refreshSelectors(); }, nullptr);
    routingAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::routing), [this] (float v) { routingState = juce::roundToInt (v); refreshSelectors(); }, nullptr);
    triggerAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::trigger), [this] (float v) { triggerState = juce::roundToInt (v); refreshSelectors(); }, nullptr);
    const char* shapeTips[] = { "Sine", "Triangle", "Ramp up (risers, pumping)", "Ramp down", "Square (hard gate)", "Sample & hold (random steps)", "Your drawn 16-step pattern" };
    for (int i = 0; i < sweepfx::numShapes; ++i)
    {
        auto& b = shapeButtons[(size_t) i];
        b.setButtonText (shapeShort[i]);
        b.setTooltip (shapeTips[i]);
        b.onClick = [this, i] { shapeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    const char* routNames[] = { "SERIAL", "PARALLEL" };
    const char* trigNames[] = { "MIDI", "HIT" };
    for (int i = 0; i < 2; ++i)
    {
        routingButtons[(size_t) i].setButtonText (routNames[i]);
        routingButtons[(size_t) i].setTooltip (i == 0 ? "A feeds B" : "A and B side by side, summed");
        routingButtons[(size_t) i].onClick = [this, i] { routingAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (routingButtons[(size_t) i]);
        triggerButtons[(size_t) i].setButtonText (trigNames[i]);
        triggerButtons[(size_t) i].setTooltip (i == 0 ? "ADSR follows MIDI notes sent to the plugin" : "ADSR fires on every detected hit / transient");
        triggerButtons[(size_t) i].onClick = [this, i] { triggerAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (triggerButtons[(size_t) i]);
    }
    shapeAtt->sendInitialUpdate();
    routingAtt->sendInitialUpdate();
    triggerAtt->sendInitialUpdate();

    // presets
    const auto& presets = sweepfx::factoryPresets();
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

SweepPanel::~SweepPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SweepPanel::refreshSelectors()
{
    for (int i = 0; i < sweepfx::numShapes; ++i)
        shapeButtons[(size_t) i].setToggleState (i == shapeState, juce::dontSendNotification);
    for (int i = 0; i < 2; ++i)
    {
        routingButtons[(size_t) i].setToggleState (i == routingState, juce::dontSendNotification);
        triggerButtons[(size_t) i].setToggleState (i == triggerState, juce::dontSendNotification);
    }
    const bool bOn = bOnButton.getToggleState();
    bOnButton.setButtonText (bOn ? "ON" : "OFF");
    for (auto* k : bKnobs) k->s.setEnabled (bOn);
    typeB.setEnabled (bOn);
    for (auto& b : routingButtons) b.setEnabled (bOn);
    const bool sync = syncButton.getToggleState();
    if (divKnob != nullptr) divKnob->s.setVisible (sync);
    if (rateKnob != nullptr) rateKnob->s.setVisible (! sync);
    repaint();
}

void SweepPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    display.tick();
    steps.repaint();
    repaint (juce::Rectangle<int> (20, 474, 320, 30));
}

void SweepPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void SweepPanel::paintOverChildren (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    // cutoff LCDs
    g.setFont (fnt (Fonts::get().mono, 17.0f));
    glowText (g, hzText (p.a.cutoff), lcdA, juce::Justification::centred, cAcc);
    glowText (g, hzText (p.b.cutoff), lcdB, juce::Justification::centred, p.b.on ? cAcc2 : cAcc2.withAlpha (0.35f));

    // small knob values
    g.setFont (fnt (Fonts::get().mono, 11.5f));
    for (auto& k : knobs)
    {
        if (k->big || ! k->s.isVisible()) continue;
        g.setColour (cText.withAlpha (k->s.isEnabled() ? 0.88f : 0.35f));
        g.drawText (k->s.getTextFromValue (k->s.getValue()), juce::Rectangle<float> (68.0f, 11.0f).withCentre ({ k->centre.x, k->centre.y + 26.0f }), juce::Justification::centred);
    }

    // LFO timing readout next to SYNC / RETRIG
    const double bpm = proc.uiBpm.load();
    const double cyc = sweepfx::cycleSeconds (p, bpm);
    juce::String info;
    if (p.sync)
        info = juce::String (sweepfx::divisionNames[(size_t) p.division]) + (p.shape == sweepfx::Shape::pattern ? " / step" : "") + "  @ " + juce::String (bpm, 1) + (proc.uiHostTempo.load() ? " BPM" : " BPM*");
    else
        info = juce::String (1.0 / cyc, 2) + " Hz cycle";
    g.setFont (fnt (Fonts::get().mono, 12.5f));
    glowText (g, info, juce::Rectangle<float> (160.0f, 479.0f, 180.0f, 20.0f), juce::Justification::centredRight, cAcc.withAlpha (0.85f));
}

void SweepPanel::resized()
{
    for (auto& k : knobs)
    {
        const float sz = k->big ? bigKnob : smallKnob;
        k->s.setBounds (juce::Rectangle<float> (sz, sz).withCentre (k->centre).toNearestInt());
    }
    display.setBounds (screenInner);
    steps.setBounds (patternInner);

    typeA.setBounds (112, 97, 124, 24);
    bOnButton.setBounds (700, 97, 38, 24);
    typeB.setBounds (742, 97, 94, 24);

    for (int i = 0; i < 2; ++i)
        routingButtons[(size_t) i].setBounds (346 + 86 * i, 300, 82, 24);

    int x = 66;
    const int widths[] = { 34, 34, 32, 44, 36, 36, 50 };
    for (int i = 0; i < sweepfx::numShapes; ++i)
    {
        shapeButtons[(size_t) i].setBounds (x, 354, widths[i], 21);
        x += widths[i] + 3;
    }
    syncButton.setBounds (30, 478, 54, 22);
    retrigButton.setBounds (88, 478, 62, 22);
    triggerButtons[0].setBounds (770, 354, 34, 21);
    triggerButtons[1].setBounds (806, 354, 30, 21);

    prevButton.setBounds (502, 28, 30, 30);
    presetBox.setBounds (536, 28, 250, 30);
    nextButton.setBounds (790, 28, 30, 30);
}

// ================= Editor =================

SweepEditor::SweepEditor (SweepProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (SweepPanel::baseW * 3 / 4, SweepPanel::baseH * 3 / 4, SweepPanel::baseW * 2, SweepPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) SweepPanel::baseW / SweepPanel::baseH);
    setSize (SweepPanel::baseW, SweepPanel::baseH);
}

void SweepEditor::resized()
{
    panel.setBounds (0, 0, SweepPanel::baseW, SweepPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / SweepPanel::baseW));
}
