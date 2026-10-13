#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText   { 0xffe8e6f0 };
const juce::Colour cInk    { 0xff8a869c };
const juce::Colour cViolet { 0xff9b5cff };
const juce::Colour cGlow   { 0xffc4a3ff };
const juce::Colour cDeep   { 0xff15141b };

juce::Colour fxColour (chopper::Fx f)
{
    switch (f)
    {
        case chopper::Fx::repeat:    return juce::Colour (0xff9b5cff);
        case chopper::Fx::reverse:   return juce::Colour (0xff38d6ff);
        case chopper::Fx::tapeStop:  return juce::Colour (0xffff9f43);
        case chopper::Fx::gate:      return juce::Colour (0xffff4d7e);
        case chopper::Fx::pitchDown: return juce::Colour (0xff4df0a0);
        case chopper::Fx::halfSpeed: return juce::Colour (0xfff4e06a);
        case chopper::Fx::off:
        default:                     return juce::Colour (0xff6c6880);
    }
}

const char* fxLabel (chopper::Fx f)
{
    static const char* n[] = { "OFF", "REPEAT", "REVERSE", "TAPE STOP", "GATE", "PITCH DN", "HALF SPD" };
    return n[(int) f];
}

const char* fxTip (chopper::Fx f)
{
    static const char* n[] = {
        "OFF: the step passes the dry signal untouched",
        "REPEAT: captures a slice at the start of the run and retriggers it (slice length = REPEAT SIZE)",
        "REVERSE: plays the audio leading into the step backwards",
        "TAPE STOP: slows the audio down to a halt over the run (chain steps for a longer stop)",
        "GATE: mutes the step",
        "PITCH DOWN: drops the step an octave while keeping its rhythm",
        "HALF SPEED: plays the step at half speed (slow-mo, an octave down)"
    };
    return n[(int) f];
}

struct Fonts
{
    juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    juce::Typeface::Ptr heavy = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    static const Fonts& get() { static Fonts f; return f; }
};

juce::Font fnt (const juce::Typeface::Ptr& t, float h) { return juce::Font (juce::FontOptions (t).withHeight (h)); }

// ---- layout (base px) - must match tools/make_background.py
const juce::Rectangle<int> gridArea { 21, 101, 778, 184 };   // screen inner
constexpr float cellX0 = 32.0f, cellX1 = 788.0f, cellY0 = 142.0f, cellY1 = 256.0f, ledY = 132.0f;
constexpr int palY = 302, palH = 30, palX0 = 16, palW = 70, palGap = 5;
constexpr int rateX0 = 590, rateW = 50, rateGap = 4;
const juce::Rectangle<int> holdRect { 34, 366, 148, 86 };
constexpr float knobY = 414.0f, knobBox = 84.0f;
float knobX (int i) { return 212.0f + 592.0f * (float) (2 * i + 1) / 8.0f; }
constexpr float lcdW = 96.0f, lcdY0 = 452.0f, lcdY1 = 474.0f;

juce::Rectangle<float> cellRectPanel (int i)
{
    const float gap = 6.0f, ggap = 14.0f;
    const float w = (cellX1 - cellX0 - 12.0f * gap - 3.0f * ggap) / 16.0f;
    const float x = cellX0 + (float) i * (w + gap) + (float) (i / 4) * (ggap - gap);
    return { x, cellY0, w, cellY1 - cellY0 };
}

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c)
{
    g.setColour (c.withAlpha (0.20f));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (dx, dy), j);
    g.setColour (c);
    g.drawText (t, r, j);
}

// small effect pictogram centred in r
void drawGlyph (juce::Graphics& g, chopper::Fx f, juce::Rectangle<float> r, juce::Colour ink, float stroke)
{
    const auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    g.setColour (ink);
    juce::PathStrokeType st (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    switch (f)
    {
        case chopper::Fx::repeat:
        {
            // a slice and its fading retriggers
            for (int k = 0; k < 3; ++k)
            {
                g.setColour (ink.withMultipliedAlpha (1.0f - 0.3f * (float) k));
                g.fillRoundedRectangle (juce::Rectangle<float> (stroke * 1.6f, s * 1.5f).withCentre ({ c.x - s * 0.62f + (float) k * s * 0.62f, c.y }), stroke * 0.6f);
            }
            break;
        }
        case chopper::Fx::reverse:
        {
            juce::Path p;
            p.startNewSubPath (c.x + s * 0.75f, c.y - s * 0.35f);
            p.lineTo (c.x - s * 0.55f, c.y - s * 0.35f);
            p.startNewSubPath (c.x - s * 0.15f, c.y - s * 0.78f);
            p.lineTo (c.x - s * 0.62f, c.y - s * 0.35f);
            p.lineTo (c.x - s * 0.15f, c.y + s * 0.08f);
            p.startNewSubPath (c.x - s * 0.75f, c.y + s * 0.5f);
            p.lineTo (c.x + s * 0.75f, c.y + s * 0.5f);
            g.strokePath (p, st);
            g.setColour (ink.withMultipliedAlpha (0.45f));
            g.strokePath ([&] { juce::Path q; q.startNewSubPath (c.x - s * 0.75f, c.y + s * 0.5f); q.lineTo (c.x + s * 0.75f, c.y + s * 0.5f); return q; }(), st);
            break;
        }
        case chopper::Fx::tapeStop:
        {
            // speed curve: full speed, braking down to a halt
            juce::Path p;
            p.startNewSubPath (c.x - s * 0.85f, c.y - s * 0.55f);
            p.cubicTo (c.x - s * 0.1f, c.y - s * 0.55f, c.x + s * 0.2f, c.y + s * 0.55f, c.x + s * 0.55f, c.y + s * 0.55f);
            g.strokePath (p, st);
            g.fillRoundedRectangle (juce::Rectangle<float> (stroke * 1.4f, s * 0.75f).withCentre ({ c.x + s * 0.82f, c.y + s * 0.2f }), stroke * 0.5f);
            break;
        }
        case chopper::Fx::gate:
        {
            auto box = juce::Rectangle<float> (s * 1.3f, s * 1.3f).withCentre (c);
            g.drawRoundedRectangle (box, stroke, stroke);
            juce::Path p;
            p.startNewSubPath (box.getX() + stroke * 1.5f, box.getBottom() - stroke * 1.5f);
            p.lineTo (box.getRight() - stroke * 1.5f, box.getY() + stroke * 1.5f);
            g.strokePath (p, st);
            break;
        }
        case chopper::Fx::pitchDown:
        {
            juce::Path p;
            p.startNewSubPath (c.x, c.y - s * 0.8f);
            p.lineTo (c.x, c.y + s * 0.7f);
            p.startNewSubPath (c.x - s * 0.5f, c.y + s * 0.2f);
            p.lineTo (c.x, c.y + s * 0.75f);
            p.lineTo (c.x + s * 0.5f, c.y + s * 0.2f);
            g.strokePath (p, st);
            g.setFont (fnt (Fonts::get().label, s * 0.62f));
            g.drawText ("8", juce::Rectangle<float> (s * 0.6f, s * 0.7f).withCentre ({ c.x + s * 0.62f, c.y - s * 0.45f }), juce::Justification::centred);
            break;
        }
        case chopper::Fx::halfSpeed:
        {
            g.setFont (fnt (Fonts::get().heavy, s * 1.1f));
            g.drawText ("1/2", r.withSizeKeepingCentre (s * 2.6f, s * 1.6f), juce::Justification::centred);
            break;
        }
        case chopper::Fx::off:
        default:
            g.setColour (ink.withMultipliedAlpha (0.5f));
            g.fillRoundedRectangle (juce::Rectangle<float> (s * 0.5f, stroke).withCentre (c), stroke * 0.5f);
            break;
    }
}
} // namespace

// ================= Look =================

ChopperLook::ChopperLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cViolet);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, cGlow);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cViolet.withAlpha (0.5f));
    knob = juce::ImageCache::getFromMemory (BinaryData::knob_med_png, BinaryData::knob_med_pngSize);
}

void ChopperLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cViolet : cInk;

    // lit arc in the baked groove (radius 30)
    const float arcR = 30.0f;
    const float a0 = bipolar ? start + (end - start) * (24.0f / 36.0f) : start;   // output: 0 dB point
    const float lo = juce::jmin (a0, angle), hi = juce::jmax (a0, angle);
    if (hi > lo + 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, lo, hi, true);
        g.setColour (on.withAlpha (0.22f));
        g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (on.brighter (0.15f));
        g.strokePath (arc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // photoreal knob (pre-rendered); only the pointer rotates
    const float body = 22.0f;
    const float imgHalf = body / 0.80f;
    g.drawImage (knob, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    const float pw = 3.0f;
    juce::Path p;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.94f, pw, topR * 0.5f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (on.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (3.0f), tr);
    g.setColour (cGlow);
    g.fillPath (p, tr);
}

void ChopperLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff6a5e8c) : juce::Colour (0xff3e3a52));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c0b10), 0, 0, juce::Colour (0xff17161e), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cViolet.brighter (0.2f));
    g.fillPath (arrow);
}

void ChopperLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font ChopperLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font ChopperLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void ChopperLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb98aff), 0, 0, juce::Colour (0xff6a2fd6), 0, r.getHeight(), false));
        g.fillRoundedRectangle (r, 4.5f);
        g.setColour (juce::Colour (0xffe2d0ff));
        g.drawRoundedRectangle (r, 4.5f, 1.0f);
        return;
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff201f28 : 0xff302e3a), 0, 0, juce::Colour (0xff18171e), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (juce::Colour (over ? 0xff6e6588 : 0xff423e54));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
}

void ChopperLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colours::white : cText.withAlpha (0.82f));
    g.setFont (fnt (Fonts::get().label, b.getHeight() < 26 ? 13.0f : 14.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ================= StepGrid =================

StepGrid::StepGrid (ChopperProcessor& p, std::function<chopper::Fx()> paletteFx) : proc (p), palette (std::move (paletteFx))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> StepGrid::cellRect (int i) const
{
    return cellRectPanel (i).translated (-(float) gridArea.getX(), -(float) gridArea.getY());
}

int StepGrid::cellAt (juce::Point<float> pt) const
{
    for (int i = 0; i < chopper::numSteps; ++i)
    {
        auto r = cellRect (i).expanded (3.0f, 0.0f);
        r = r.withTop (r.getY() - 18.0f).withBottom (r.getBottom() + 18.0f);   // LED + number rows count too
        if (r.contains (pt)) return i;
    }
    return -1;
}

void StepGrid::paintCell (int i, chopper::Fx fx)
{
    if (i < 0) return;
    proc.setStep (i, fx);
    flash[i] = 1.0f;
    lastPainted = i;
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int c = cellAt (e.position);
    downCell = c;
    painting = false;
    lastPainted = -1;
    if (c < 0) return;
    if (e.mods.isPopupMenu())
    {
        painting = true;
        paintCell (c, palette());
    }
    else if (e.mods.isAltDown() || e.mods.isCommandDown())
    {
        painting = true;
        paintCell (c, chopper::Fx::off);
    }
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    const int c = cellAt (e.position);
    if (! painting && downCell >= 0 && (c != downCell || e.getDistanceFromDragStart() > 6))
    {
        painting = true;
        paintCell (downCell, palette());
    }
    if (painting && c >= 0 && c != lastPainted)
    {
        // fill any cells skipped by a fast drag
        const int from = lastPainted < 0 ? c : lastPainted;
        const auto fx = e.mods.isAltDown() || e.mods.isCommandDown() ? chopper::Fx::off : palette();
        for (int k = juce::jmin (from, c); k <= juce::jmax (from, c); ++k)
            proc.setStep (k, fx), flash[k] = 1.0f;
        lastPainted = c;
        repaint();
    }
    hover = c;
}

void StepGrid::mouseUp (const juce::MouseEvent& e)
{
    if (! painting && downCell >= 0 && cellAt (e.position) == downCell && ! e.mods.isPopupMenu())
    {
        // click: cycle through the effects (shift = backwards)
        const int cur = (int) proc.getStep (downCell);
        const int next = (cur + (e.mods.isShiftDown() ? chopper::numFx - 1 : 1)) % chopper::numFx;
        paintCell (downCell, (chopper::Fx) next);
    }
    painting = false;
    downCell = -1;
}

void StepGrid::mouseMove (const juce::MouseEvent& e)
{
    const int c = cellAt (e.position);
    if (c != hover) { hover = c; repaint(); }
}

void StepGrid::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void StepGrid::tick()
{
    bool dirty = false;
    for (auto& f : flash)
        if (f > 0.0f) { f = juce::jmax (0.0f, f - 0.12f); dirty = true; }
    const int step = proc.uiStep.load();
    if (step != shownStep || proc.patternVersion.load() != shownVersion) dirty = true;
    if (dirty || proc.uiLevel.load() > 0.0001f || proc.uiHostPlaying.load()) repaint();
}

void StepGrid::paint (juce::Graphics& g)
{
    shownStep = proc.uiStep.load();
    shownVersion = proc.patternVersion.load();
    const auto pattern = proc.getPattern();
    const bool hold = proc.apvts.getRawParameterValue (IDs::hold)->load() > 0.5f;
    const auto curFx = (chopper::Fx) juce::jlimit (0, chopper::numFx - 1, proc.uiFx.load());
    const float phase = (float) juce::jlimit (0.0, 1.0, proc.uiPhase.load());

    // ---- readout strip
    {
        const auto strip = juce::Rectangle<float> (12.0f, 3.0f, (float) getWidth() - 24.0f, 17.0f);
        g.setFont (fnt (Fonts::get().mono, 14.0f));
        const juce::String stepTxt = "STEP " + juce::String (shownStep + 1).paddedLeft ('0', 2) + "/16";
        glowText (g, stepTxt, strip, juce::Justification::centredLeft, cGlow);
        g.setFont (fnt (Fonts::get().label, 14.0f));
        const auto fxTxt = hold ? juce::String ("HOLD > REPEAT") : juce::String (fxLabel (curFx));
        glowText (g, fxTxt, strip.withTrimmedLeft (100.0f), juce::Justification::centredLeft, hold ? cGlow : fxColour (curFx).brighter (0.2f));

        g.setFont (fnt (Fonts::get().mono, 14.0f));
        const int rate = (int) proc.apvts.getRawParameterValue (IDs::rate)->load();
        const int size = (int) proc.apvts.getRawParameterValue (IDs::size)->load();
        const int swing = juce::roundToInt (proc.apvts.getRawParameterValue (IDs::swing)->load() * 100.0f);
        const juce::String mid = "RATE " + juce::String (chopper::rateNames[juce::jlimit (0, chopper::numRates - 1, rate)])
                               + "   SLICE " + juce::String (chopper::sizeNames[juce::jlimit (0, chopper::numSizes - 1, size)])
                               + "   SWING " + juce::String (swing) + "%";
        glowText (g, mid, strip, juce::Justification::centred, cGlow.withAlpha (0.8f));
        const bool host = proc.uiHostPlaying.load();
        const juce::String bpm = juce::String (proc.uiBpm.load(), 1) + " BPM";
        glowText (g, bpm, strip.withTrimmedRight (52.0f), juce::Justification::centredRight, cGlow);
        g.setFont (fnt (Fonts::get().label, 12.0f));
        auto chip = juce::Rectangle<float> (strip.getRight() - 44.0f, strip.getY() + 2.0f, 44.0f, 13.0f);
        if (host)
        {
            g.setColour (cViolet);
            g.fillRoundedRectangle (chip, 3.0f);
            g.setColour (juce::Colours::white);
            g.drawText ("SYNC", chip, juce::Justification::centred);
        }
        else
        {
            g.setColour (cInk.withAlpha (0.8f));
            g.drawRoundedRectangle (chip.reduced (0.5f), 3.0f, 1.0f);
            g.drawText ("FREE", chip, juce::Justification::centred);
        }
    }

    // ---- cells
    for (int i = 0; i < chopper::numSteps; ++i)
    {
        const auto r = cellRect (i);
        const auto fx = pattern[(size_t) i];
        const bool isPlay = i == shownStep;
        const auto col = fxColour (fx);
        const float lift = flash[i];

        if (fx != chopper::Fx::off)
        {
            // glow around a lit cell
            g.setColour (col.withAlpha (0.10f + 0.10f * lift + (isPlay ? 0.12f : 0.0f)));
            g.fillRoundedRectangle (r.expanded (3.0f), 7.0f);
            juce::ColourGradient fill (col.brighter (0.25f + 0.3f * lift).withAlpha (isPlay ? 1.0f : 0.92f), r.getX(), r.getY(),
                                       col.darker (0.85f).withAlpha (0.95f), r.getX(), r.getBottom(), false);
            fill.addColour (0.45, col.withAlpha (isPlay ? 1.0f : 0.88f));
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, 5.0f);
            // glass top
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), 0, r.getY(), juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.35f, false));
            g.fillRoundedRectangle (r.reduced (2.0f, 1.5f).withHeight (r.getHeight() * 0.35f), 4.0f);
            g.setColour (col.brighter (0.7f).withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
            const auto ink = juce::Colour (0xff120c1c).withAlpha (0.88f);
            drawGlyph (g, fx, r.withTrimmedBottom (r.getHeight() * 0.42f).withTrimmedTop (r.getHeight() * 0.14f).reduced (7.0f, 0.0f), ink, 2.6f);
            g.setColour (ink);
            g.setFont (fnt (Fonts::get().label, 12.5f));
            g.drawText (chopper::fxShort (fx), r.withTrimmedTop (r.getHeight() - 30.0f), juce::Justification::centred);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1b26).withMultipliedBrightness (1.0f + lift), 0, r.getY(),
                                                     juce::Colour (0xff111017), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (i % 4 == 0 ? 0.10f : 0.06f));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) r.getY() + 2, r.getX() + 4.0f, r.getRight() - 4.0f);
            drawGlyph (g, fx, r.withSizeKeepingCentre (r.getWidth(), 20.0f), cInk, 2.0f);
        }

        if (i == hover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawRoundedRectangle (r.expanded (1.5f), 6.0f, 1.0f);
        }

        // playhead
        const float lx = r.getCentreX(), ly = ledY - (float) gridArea.getY();
        if (isPlay)
        {
            const auto pc = fx == chopper::Fx::off ? cGlow : col.brighter (0.4f);
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.drawRoundedRectangle (r.expanded (2.0f), 6.5f, 1.6f);
            g.setColour (pc.withAlpha (0.35f));
            g.drawRoundedRectangle (r.expanded (4.0f), 8.0f, 2.0f);
            // step progress
            auto bar = juce::Rectangle<float> (r.getX() + 4.0f, r.getBottom() - 5.0f, (r.getWidth() - 8.0f) * phase, 2.0f);
            g.setColour (fx == chopper::Fx::off ? cGlow.withAlpha (0.8f) : juce::Colour (0xff120c1c).withAlpha (0.7f));
            g.fillRect (bar);
            // LED
            g.setColour (pc.withAlpha (0.30f));
            g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ lx, ly }));
            g.setColour (pc);
            g.fillEllipse (juce::Rectangle<float> (6.4f, 6.4f).withCentre ({ lx, ly }));
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillEllipse (juce::Rectangle<float> (2.4f, 2.4f).withCentre ({ lx - 0.8f, ly - 0.8f }));
        }
        else if (fx != chopper::Fx::off)
        {
            g.setColour (col.withAlpha (0.45f));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ lx, ly }));
        }
    }
}

// ================= FxButton =================

FxButton::FxButton (chopper::Fx f, std::function<void (chopper::Fx)> onPick) : fx (f), pick (std::move (onPick))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (fxTip (f));
}

void FxButton::mouseDown (const juce::MouseEvent&) { pick (fx); }

void FxButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto col = fxColour (fx);
    const bool over = isMouseOver();
    if (selected)
    {
        g.setGradientFill (juce::ColourGradient (col.withMultipliedBrightness (0.55f), 0, 0, col.withMultipliedBrightness (0.22f), 0, r.getHeight(), false));
        g.fillRoundedRectangle (r, 4.5f);
        g.setColour (col.brighter (0.4f));
        g.drawRoundedRectangle (r, 4.5f, 1.3f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2a36), 0, 0, juce::Colour (0xff17161d), 0, r.getHeight(), false));
        g.fillRoundedRectangle (r, 4.5f);
        g.setColour (over ? col.withAlpha (0.7f) : juce::Colour (0xff423e54));
        g.drawRoundedRectangle (r, 4.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
    // colour LED
    const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ 11.0f, r.getCentreY() });
    if (selected)
    {
        g.setColour (col.withAlpha (0.4f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (selected ? col.brighter (0.3f) : col.withMultipliedBrightness (0.8f));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (led, 0.8f);
    g.setColour (selected ? juce::Colours::white : cText.withAlpha (0.8f));
    g.setFont (fnt (Fonts::get().label, 13.5f));
    g.drawText (fxLabel (fx), r.withTrimmedLeft (18.0f).withTrimmedRight (3.0f), juce::Justification::centred);
}

// ================= HoldButton =================

HoldButton::HoldButton (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v)
      {
          on = v > 0.5f;
          if (! on) latched = false;
          repaint();
      }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void HoldButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        // right-click: latch / unlatch
        latched = ! on;
        momentary = false;
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
        repaint();
        return;
    }
    if (latched)
    {
        latched = false;
        momentary = false;
        attachment.setValueAsCompleteGesture (0.0f);
        return;
    }
    momentary = true;
    attachment.beginGesture();
    attachment.setValueAsPartOfGesture (1.0f);
}

void HoldButton::mouseUp (const juce::MouseEvent&)
{
    if (! momentary) return;
    momentary = false;
    attachment.setValueAsPartOfGesture (0.0f);
    attachment.endGesture();
}

void HoldButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto hue = cViolet;
    if (on)
    {
        juce::ColourGradient lit (hue.brighter (0.7f), r.getCentreX(), r.getY() + r.getHeight() * 0.35f,
                                  hue.darker (0.6f), r.getCentreX(), r.getBottom() + r.getHeight() * 0.3f, true);
        g.setGradientFill (lit);
    }
    else
        g.setGradientFill (juce::ColourGradient (hue.withMultipliedBrightness (0.24f).withMultipliedSaturation (0.75f), 0, 0,
                                                 hue.withMultipliedBrightness (0.09f).withMultipliedSaturation (0.6f), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 9.0f);

    juce::Path top;
    top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 7.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.40f : 0.10f), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
    g.fillPath (top);
    g.setColour (on ? hue.brighter (0.9f).withAlpha (0.95f) : hue.withAlpha (0.30f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.2f);

    const auto ink = on ? juce::Colour (0xff180c2c) : cGlow.withAlpha (0.60f);
    g.setFont (fnt (Fonts::get().heavy, 30.0f));
    if (on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawText ("HOLD", r.withTrimmedBottom (20.0f).translated (0.0f, 1.0f), juce::Justification::centred);
    }
    g.setColour (ink);
    g.drawText ("HOLD", r.withTrimmedBottom (20.0f), juce::Justification::centred);
    g.setFont (fnt (Fonts::get().label, 12.5f));
    const juce::String sub = on ? (latched ? "LATCHED" : "REPEATING") : "REPEAT NOW";
    g.drawText (sub, r.withTrimmedTop (r.getHeight() - 28.0f), juce::Justification::centred);
}

// ================= Panel =================

ChopperPanel::ChopperPanel (ChopperProcessor& p)
    : proc (p), grid (p, [this] { return paletteFx; }),
      holdButton (*p.apvts.getParameter (IDs::hold))
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    const char* ids[] = { IDs::size, IDs::swing, IDs::mix, IDs::output };
    const char* tips[] = { "Length of the slice a Repeat step captures and retriggers (1/4 ... 1/64, 1/16T)",
                           "Swing: delays every second step (0 = straight, 100 % = hard shuffle)",
                           "Wet / dry mix (100 % = full chop)",
                           "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setTooltip (tips[i]);
        k.s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.s);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.s);
    }
    knobs[0].s.setVelocityBasedMode (false);
    knobs[0].s.setMouseDragSensitivity (220);
    knobs[1].s.setDoubleClickReturnValue (true, 0.0);
    knobs[2].s.setDoubleClickReturnValue (true, 1.0);
    knobs[3].s.setDoubleClickReturnValue (true, 0.0);
    knobs[3].s.getProperties().set ("bipolar", true);

    addAndMakeVisible (grid);

    for (int i = 0; i < chopper::numFx; ++i)
    {
        fxButtons.push_back (std::make_unique<FxButton> ((chopper::Fx) i, [this] (chopper::Fx f) { selectFx (f); }));
        addAndMakeVisible (*fxButtons.back());
    }
    selectFx (paletteFx);

    rateAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::rate), [this] (float v)
    {
        rateState = juce::roundToInt (v);
        refreshRate();
    }, nullptr);
    const char* rateTips[] = { "8th-note steps (16 steps = 2 bars)", "16th-note steps (16 steps = 1 bar)",
                               "32nd-note steps (16 steps = half a bar)", "16th-triplet steps" };
    for (int i = 0; i < chopper::numRates; ++i)
    {
        auto& b = rateButtons[(size_t) i];
        b.setButtonText (chopper::rateNames[i]);
        b.setTooltip (rateTips[i]);
        b.onClick = [this, i] { rateAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    rateAttachment->sendInitialUpdate();

    holdButton.setTooltip ("HOLD: forces a step-synced Repeat while pressed (beat-repeat performance). Right-click latches it.");
    addAndMakeVisible (holdButton);

    randomButton.setTooltip ("Generate a new random chop pattern");
    clearButton.setTooltip ("Set every step to Off");
    randomButton.onClick = [this] { randomise(); };
    clearButton.onClick = [this]
    {
        proc.setPattern ({});
        proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    };
    addAndMakeVisible (randomButton);
    addAndMakeVisible (clearButton);

    // presets
    const auto& presets = chopper::factoryPresets();
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

ChopperPanel::~ChopperPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ChopperPanel::selectFx (chopper::Fx f)
{
    paletteFx = f;
    for (auto& b : fxButtons)
    {
        b->selected = b->fx == f;
        b->repaint();
    }
    repaint();
}

void ChopperPanel::refreshRate()
{
    for (int i = 0; i < chopper::numRates; ++i)
        rateButtons[(size_t) i].setToggleState (i == rateState, juce::dontSendNotification);
    grid.repaint();
}

void ChopperPanel::randomise()
{
    using chopper::Fx;
    chopper::Pattern p {};
    const float density = 0.3f + rng.nextFloat() * 0.3f;
    const Fx pool[] = { Fx::repeat, Fx::repeat, Fx::repeat, Fx::reverse, Fx::gate, Fx::gate, Fx::pitchDown, Fx::halfSpeed, Fx::reverse };
    for (int i = 0; i < chopper::numSteps;)
    {
        if (rng.nextFloat() < density)
        {
            const Fx f = pool[rng.nextInt ((int) std::size (pool))];
            const int len = 1 + rng.nextInt (f == Fx::gate ? 2 : 4);
            for (int k = 0; k < len && i < chopper::numSteps; ++k) p[(size_t) i++] = f;
        }
        else
            ++i;
    }
    if (rng.nextFloat() < 0.4f)   // end the bar with a tape stop now and then
        for (int i = 12 + rng.nextInt (3); i < chopper::numSteps; ++i) p[(size_t) i] = Fx::tapeStop;
    proc.setPattern (p);
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
}

void ChopperPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    grid.tick();
}

void ChopperPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    auto bloom = [&] (juce::Rectangle<int> r, juce::Colour c, float strength, float rad)
    {
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (c.withAlpha (strength * (float) (4 - k)));
            g.fillRoundedRectangle (r.toFloat().expanded ((float) k * 3.0f), rad + (float) k * 3.0f);
        }
    };
    if (holdButton.isOn()) bloom (holdRect, cViolet, 0.08f, 10.0f);
    for (auto& b : fxButtons)
        if (b->selected) bloom (b->getBounds(), fxColour (b->fx), 0.045f, 5.0f);
}

void ChopperPanel::paintOverChildren (juce::Graphics& g)
{
    // LCD readouts under the knobs
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        const auto box = juce::Rectangle<float> (knobX ((int) i) - lcdW * 0.5f, lcdY0, lcdW, lcdY1 - lcdY0);
        g.setFont (fnt (Fonts::get().mono, 15.0f));
        glowText (g, k.s.getTextFromValue (k.s.getValue()), box.translated (0.0f, 0.5f), juce::Justification::centred, cGlow);
    }
}

void ChopperPanel::resized()
{
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].s.setBounds (juce::Rectangle<float> (knobBox, knobBox).withCentre ({ knobX ((int) i), knobY }).toNearestInt());

    grid.setBounds (gridArea);
    for (int i = 0; i < chopper::numFx; ++i)
        fxButtons[(size_t) i]->setBounds (palX0 + i * (palW + palGap), palY, palW, palH);
    for (int i = 0; i < chopper::numRates; ++i)
        rateButtons[(size_t) i].setBounds (rateX0 + i * (rateW + rateGap), palY, rateW, palH);

    holdButton.setBounds (holdRect);

    prevButton.setBounds (470, 22, 30, 30);
    presetBox.setBounds (504, 22, 250, 30);
    nextButton.setBounds (758, 22, 30, 30);
    randomButton.setBounds (640, 60, 72, 22);
    clearButton.setBounds (718, 60, 70, 22);
}

// ================= Editor =================

ChopperEditor::ChopperEditor (ChopperProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (ChopperPanel::baseW * 3 / 4, ChopperPanel::baseH * 3 / 4, ChopperPanel::baseW * 2, ChopperPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) ChopperPanel::baseW / ChopperPanel::baseH);
    setSize (ChopperPanel::baseW, ChopperPanel::baseH);
}

void ChopperEditor::resized()
{
    panel.setBounds (0, 0, ChopperPanel::baseW, ChopperPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / ChopperPanel::baseW));
}
