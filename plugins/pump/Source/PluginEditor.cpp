#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
const juce::Colour cBg    { 0xff0b0c0a };
const juce::Colour cPanel { 0xff161814 };
const juce::Colour cGrid  { 0xff2a2e26 };
const juce::Colour cText  { 0xffe8eee0 };
const juce::Colour cDim   { 0xff7d8676 };
const juce::Colour cLime  { 0xffb6ff3b };
const juce::Colour cHot   { 0xffff4f7b };

struct Fonts
{
    juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr labelSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    static const Fonts& get() { static Fonts f; return f; }
};

juce::Font font (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (bold ? Fonts::get().label : Fonts::get().labelSemi).withHeight (h));
}

juce::Font mono (float h) { return juce::Font (juce::FontOptions (Fonts::get().mono).withHeight (h)); }

juce::Point<float> knobCentre (int i)
{
    const float kw = (820.0f - 32.0f) / 6.0f;
    return { 16.0f + (float) i * kw + kw * 0.5f, 430.0f };
}
} // namespace

// ---------------- Look ----------------

PumpLook::PumpLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::ComboBox::arrowColourId, cLime);
    setColour (juce::PopupMenu::backgroundColourId, cPanel);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cLime);
    setColour (juce::PopupMenu::highlightedTextColourId, cBg);
    setColour (juce::Label::textColourId, cText);
}

void PumpLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const bool on = s.isEnabled();

    // LED ring: 21 dots, lit up to the value (unlit dots are part of the background)
    const int lit = juce::roundToInt (pos * 20.0f);
    for (int k = 0; k <= 20; ++k)
    {
        if (k > lit) break;
        const float a = juce::degreesToRadians (225.0f - 270.0f * (float) k / 20.0f);
        const float lx = c.x + std::cos (a) * 38.0f, ly = c.y - std::sin (a) * 38.0f;
        const auto col = on ? cLime : cDim;
        g.setColour (col.withAlpha (0.25f));
        g.fillEllipse (lx - 4.0f, ly - 4.0f, 8.0f, 8.0f);
        g.setColour (col);
        g.fillEllipse (lx - 1.9f, ly - 1.9f, 3.8f, 3.8f);
    }

    // rubber body
    const float r = 28.0f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - r + 1.0f, c.y - r + 3.0f, r * 2, r * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c302a), c.x, c.y - r, juce::Colour (0xff0e100d), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    // grip ribs
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    const float angle = juce::degreesToRadians (-135.0f + 270.0f * pos);
    for (int i = 0; i < 30; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 30.0f;
        g.drawLine (c.x + std::sin (a) * r * 0.86f, c.y - std::cos (a) * r * 0.86f, c.x + std::sin (a) * r * 0.99f, c.y - std::cos (a) * r * 0.99f, 1.0f);
    }
    // brushed metal top
    const float t = r * 0.68f;
    juce::ColourGradient metal (juce::Colour (0xffc9cfc2), c.x - t, c.y - t, juce::Colour (0xff4b5047), c.x + t, c.y + t, false);
    metal.addColour (0.5, juce::Colour (0xff8a9083));
    g.setGradientFill (metal);
    g.fillEllipse (c.x - t, c.y - t, t * 2, t * 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - t, c.y - t, t * 2, t * 2, 1.0f);
    // indicator
    juce::Path p;
    p.addRoundedRectangle (-1.6f, -r * 0.98f, 3.2f, r * 0.42f, 1.6f);
    g.setColour (on ? cLime : cDim);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void PumpLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (juce::Colour (0xff3c4238));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff070906));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    g.setGradientFill (juce::ColourGradient (cLime.withAlpha (0.10f), 0, 0, cLime.withAlpha (0.02f), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (2.0f), 4.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cLime);
    g.fillPath (arrow);
}

void PumpLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cLime);
}

juce::Font PumpLook::getComboBoxFont (juce::ComboBox&) { return mono (15.0f); }
juce::Font PumpLook::getPopupMenuFont() { return font (16.0f, false); }

// ---------------- CurveEditor ----------------

CurveEditor::CurveEditor (PumpProcessor& p) : proc (p)
{
    pts = proc.getCurvePoints();
    startTimerHz (60);
}

juce::Rectangle<float> CurveEditor::plot() const { return getLocalBounds().toFloat().reduced (14.0f, 12.0f); }

juce::Point<float> CurveEditor::toScreen (float x, float y) const
{
    const auto r = plot();
    return { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
}

int CurveEditor::hitPoint (juce::Point<float> m) const
{
    for (int i = 0; i < (int) pts.size(); ++i)
        if (toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (m) < 10.0f)
            return i;
    return -1;
}

int CurveEditor::hitSegment (juce::Point<float> m) const
{
    const auto r = plot();
    const float x = (m.x - r.getX()) / r.getWidth();
    for (int i = 0; i + 1 < (int) pts.size(); ++i)
        if (x > pts[(size_t) i].x && x < pts[(size_t) i + 1].x)
            return i;
    return -1;
}

void CurveEditor::push() { proc.setCurvePoints (pts); }

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    pts = proc.getCurvePoints();
    dragPoint = hitPoint (e.position);
    dragSegment = dragPoint < 0 ? hitSegment (e.position) : -1;
    if (dragSegment >= 0)
    {
        dragStartTension = pts[(size_t) dragSegment].tension;
        dragStartY = e.position.y;
    }
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto r = plot();
    if (dragPoint >= 0)
    {
        auto& pt = pts[(size_t) dragPoint];
        float nx = juce::jlimit (0.0f, 0.999f, (e.position.x - r.getX()) / r.getWidth());
        const float ny = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
        if (dragPoint == 0) nx = 0.0f; // first point stays at the start of the cycle
        else
        {
            const float lo = pts[(size_t) dragPoint - 1].x + 0.005f;
            const float hi = dragPoint + 1 < (int) pts.size() ? pts[(size_t) dragPoint + 1].x - 0.005f : 0.999f;
            nx = juce::jlimit (lo, hi, nx);
        }
        if (e.mods.isShiftDown()) // snap to 1/16 grid
            nx = std::round (nx * 16.0f) / 16.0f;
        pt.x = nx;
        pt.y = ny;
        push();
    }
    else if (dragSegment >= 0)
    {
        const float dy = (e.position.y - dragStartY) / r.getHeight();
        const bool rising = pts[(size_t) dragSegment + 1].y >= pts[(size_t) dragSegment].y;
        pts[(size_t) dragSegment].tension = juce::jlimit (-1.0f, 1.0f, dragStartTension + (rising ? dy : -dy) * 2.0f);
        push();
    }
    repaint();
}

void CurveEditor::mouseUp (const juce::MouseEvent&) { dragPoint = dragSegment = -1; }

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    pts = proc.getCurvePoints();
    const int hit = hitPoint (e.position);
    if (hit > 0 && pts.size() > 1)
        pts.erase (pts.begin() + hit);
    else if (hit < 0)
    {
        const auto r = plot();
        pts.push_back ({ juce::jlimit (0.001f, 0.999f, (e.position.x - r.getX()) / r.getWidth()),
                         juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight()), 0.0f });
    }
    push();
    pts = proc.getCurvePoints();
    repaint();
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitPoint (e.position);
    if (h != hoverPoint) { hoverPoint = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor
                           : (hitSegment (e.position) >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor));
}

void CurveEditor::timerCallback()
{
    if (dragPoint < 0 && dragSegment < 0)
        pts = proc.getCurvePoints();
    phase = proc.uiPhase.load();
    gain = proc.uiGain.load();
    history[(size_t) histPos] = gain;
    histPos = (histPos + 1) % (int) history.size();
    const int t = proc.uiTriggerCount.load();
    if (t != lastTriggers) { lastTriggers = t; flash = 1.0f; }
    flash *= 0.85f;
    repaint();
}

void CurveEditor::paint (juce::Graphics& g)
{
    const auto r = plot();
    // grid: 16 steps, beats brighter
    for (int i = 0; i <= 16; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 16.0f;
        g.setColour (cLime.withAlpha (i % 4 == 0 ? 0.16f : 0.06f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        g.setColour (cLime.withAlpha (0.06f));
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());
    }

    // gain history (scope) in the background
    juce::Path hist;
    for (int i = 0; i < (int) history.size(); ++i)
    {
        const float v = history[(size_t) ((histPos + i) % (int) history.size())];
        const auto p = juce::Point<float> (r.getX() + r.getWidth() * (float) i / (float) (history.size() - 1), r.getBottom() - v * r.getHeight());
        if (i == 0) hist.startNewSubPath (p); else hist.lineTo (p);
    }
    g.setColour (cDim.withAlpha (0.25f));
    g.strokePath (hist, juce::PathStrokeType (1.2f));

    // the curve
    pump::Curve c;
    c.setPoints (pts);
    juce::Path curvePath, fill;
    const int steps = (int) r.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const auto p = toScreen (x, c.evaluate (std::min (x, 0.9999f)));
        if (i == 0) { curvePath.startNewSubPath (p); fill.startNewSubPath (r.getX(), r.getBottom()); fill.lineTo (p); }
        else { curvePath.lineTo (p); fill.lineTo (p); }
    }
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (cLime.withAlpha (0.28f), 0, r.getY(), cLime.withAlpha (0.02f), 0, r.getBottom(), false));
    g.fillPath (fill);
    // phosphor glow
    g.setColour (cLime.withAlpha (0.10f));
    g.strokePath (curvePath, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cLime.withAlpha (0.22f));
    g.strokePath (curvePath, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (cLime.brighter (0.3f));
    g.strokePath (curvePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // points
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const auto p = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
        const float rad = (i == hoverPoint || i == dragPoint) ? 7.0f : 5.0f;
        g.setColour (cBg);
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2, rad * 2);
        g.setColour (cLime);
        g.drawEllipse (p.x - rad, p.y - rad, rad * 2, rad * 2, 2.0f);
    }

    // playhead
    const float px = r.getX() + juce::jlimit (0.0f, 1.0f, phase) * r.getWidth();
    g.setColour (cText.withAlpha (0.8f));
    g.drawVerticalLine ((int) px, r.getY(), r.getBottom());
    g.setColour (cHot);
    g.fillEllipse (px - 4, r.getBottom() - gain * r.getHeight() - 4, 8, 8);

    if (flash > 0.05f)
    {
        g.setColour (cHot.withAlpha (flash * 0.5f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 10.0f, 3.0f);
    }

}

// ---------------- Panel ----------------

PumpPanel::PumpPanel (PumpProcessor& p) : proc (p), curve (p)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);
    addAndMakeVisible (curve);

    const char* ids[] = { IDs::depth, IDs::smooth, IDs::crossover, IDs::threshold, IDs::mix, IDs::output };
    const char* names[] = { "DEPTH", "SMOOTH", "X-OVER", "THRESH", "MIX", "OUTPUT" };
    const char* tips[] = { "How far the curve pulls the volume down", "Softens the curve edges (no clicks)",
                           "Crossover for the LOW / HIGH band modes", "Kick level that fires the curve in AUDIO mode",
                           "Dry / wet blend", "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setTooltip (tips[i]);
        k.s.onValueChange = [this] { repaint (16, 372, baseW - 32, 120); };
        addAndMakeVisible (k.s);
        k.l.setText (names[i], juce::dontSendNotification);
        k.l.setJustificationType (juce::Justification::centred);
        k.l.setFont (font (15.0f));
        k.l.setColour (juce::Label::textColourId, cText);
        k.l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (k.l);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.s);
    }

    const char* cids[] = { IDs::rate, IDs::trigger, IDs::band };
    for (size_t i = 0; i < choices.size(); ++i)
    {
        auto& c = choices[i];
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (cids[i])))
            c.box.addItemList (param->choices, 1);
        addAndMakeVisible (c.box);
        c.a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, cids[i], c.box);
    }

    for (int i = 0; i < pump::Curve::numPresets(); ++i)
        shapeBox.addItem (pump::Curve::presetName (i), i + 1);
    shapeBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    shapeBox.onChange = [this] { proc.setCurrentProgram (shapeBox.getSelectedId() - 1); };
    addAndMakeVisible (shapeBox);

    startTimerHz (10);
}

PumpPanel::~PumpPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PumpPanel::timerCallback()
{
    // dim the controls that do nothing in the current mode
    const int trig = (int) proc.apvts.getRawParameterValue (IDs::trigger)->load();
    const int band = (int) proc.apvts.getRawParameterValue (IDs::band)->load();
    knobs[3].s.setEnabled (trig == 2);
    knobs[2].s.setEnabled (band != 0);
    if (shapeBox.getSelectedId() != proc.getCurrentProgram() + 1)
        shapeBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void PumpPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    g.setColour (cDim);
    g.setFont (font (12.0f, false));
    g.drawText ("drag points  /  drag a line to bend it  /  double-click to add or delete  /  shift = snap to grid",
                300, 64, baseW - 316, 14, juce::Justification::centredRight);
}

void PumpPanel::paintOverChildren (juce::Graphics& g)
{
    g.setFont (mono (14.0f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto c = knobCentre ((int) i);
        auto& k = knobs[i];
        g.setColour (k.s.isEnabled() ? cLime : cDim);
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (110.0f, 16.0f).withCentre ({ c.x, c.y + 49.0f }),
                    juce::Justification::centred);
    }
}

void PumpPanel::resized()
{
    shapeBox.setBounds (300, 28, 160, 30);
    const int xs[] = { 472, 594, 716 };
    const int widths[] = { 110, 110, 96 };
    for (size_t i = 0; i < choices.size(); ++i)
        choices[i].box.setBounds (xs[i], 28, widths[i], 30);

    curve.setBounds (16, 82, baseW - 32, 280);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto c = knobCentre ((int) i);
        knobs[i].s.setBounds (juce::Rectangle<float> (90.0f, 90.0f).withCentre (c).toNearestInt());
        knobs[i].l.setBounds (juce::Rectangle<float> (120.0f, 16.0f).withCentre ({ c.x, c.y - 49.0f }).toNearestInt());
    }
}

// ---------------- Editor ----------------

PumpEditor::PumpEditor (PumpProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (PumpPanel::baseW * 3 / 4, PumpPanel::baseH * 3 / 4, PumpPanel::baseW * 2, PumpPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) PumpPanel::baseW / PumpPanel::baseH);
    setSize (PumpPanel::baseW, PumpPanel::baseH);
}

void PumpEditor::resized()
{
    panel.setBounds (0, 0, PumpPanel::baseW, PumpPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / PumpPanel::baseW));
}
