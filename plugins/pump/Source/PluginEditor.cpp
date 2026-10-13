#include "PluginEditor.h"

namespace
{
const juce::Colour cBg    { 0xff0b0c0a };
const juce::Colour cPanel { 0xff161814 };
const juce::Colour cGrid  { 0xff2a2e26 };
const juce::Colour cText  { 0xffe8eee0 };
const juce::Colour cDim   { 0xff7d8676 };
const juce::Colour cLime  { 0xffb6ff3b };
const juce::Colour cHot   { 0xffff4f7b };

juce::Font font (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
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

void PumpLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float a = start + pos * (end - start);
    juce::Path track, val;
    track.addCentredArc (c.x, c.y, r - 3, r - 3, 0, start, end, true);
    val.addCentredArc (c.x, c.y, r - 3, r - 3, 0, start, a, true);
    g.setColour (cGrid);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.isEnabled() ? cLime : cDim);
    g.strokePath (val, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float kr = r - 12.0f;
    g.setColour (cPanel.brighter (0.08f));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (cGrid.brighter (0.2f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -kr + 3, 3.0f, kr * 0.5f, 1.5f);
    g.setColour (cText);
    g.fillPath (p, juce::AffineTransform::rotation (a).translated (c));
}

void PumpLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (cPanel.brighter (0.05f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (cGrid.brighter (0.3f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (cLime);
    g.fillPath (arrow);
}

juce::Font PumpLook::getComboBoxFont (juce::ComboBox&) { return font (14.0f); }
juce::Font PumpLook::getPopupMenuFont() { return font (14.0f, false); }

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
    g.setColour (cPanel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    // grid: 16 steps, beats brighter
    for (int i = 0; i <= 16; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 16.0f;
        g.setColour (i % 4 == 0 ? cGrid.brighter (0.35f) : cGrid);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        g.setColour (cGrid);
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
    g.setColour (cLime);
    g.strokePath (curvePath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

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
    addAndMakeVisible (curve);

    const char* ids[] = { IDs::depth, IDs::smooth, IDs::crossover, IDs::threshold, IDs::mix, IDs::output };
    const char* names[] = { "DEPTH", "SMOOTH", "X-OVER", "THRESH", "MIX", "OUTPUT" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k.s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.s.setColour (juce::Slider::textBoxTextColourId, cText);
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (k.s);
        k.l.setText (names[i], juce::dontSendNotification);
        k.l.setJustificationType (juce::Justification::centred);
        k.l.setFont (font (12.0f));
        addAndMakeVisible (k.l);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.s);
    }

    const char* cids[] = { IDs::rate, IDs::trigger, IDs::band };
    const char* cnames[] = { "RATE", "TRIGGER", "BAND" };
    for (size_t i = 0; i < choices.size(); ++i)
    {
        auto& c = choices[i];
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (cids[i])))
            c.box.addItemList (param->choices, 1);
        addAndMakeVisible (c.box);
        c.l.setText (cnames[i], juce::dontSendNotification);
        c.l.setFont (font (11.0f));
        c.l.setColour (juce::Label::textColourId, cDim);
        addAndMakeVisible (c.l);
        c.a = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, cids[i], c.box);
    }

    for (int i = 0; i < pump::Curve::numPresets(); ++i)
        shapeBox.addItem (pump::Curve::presetName (i), i + 1);
    shapeBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    shapeBox.onChange = [this] { proc.setCurrentProgram (shapeBox.getSelectedId() - 1); };
    addAndMakeVisible (shapeBox);
    shapeLabel.setText ("SHAPE", juce::dontSendNotification);
    shapeLabel.setFont (font (11.0f));
    shapeLabel.setColour (juce::Label::textColourId, cDim);
    addAndMakeVisible (shapeLabel);

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
}

void PumpPanel::paint (juce::Graphics& g)
{
    g.fillAll (cBg);
    g.setColour (cLime);
    g.setFont (font (44.0f));
    g.drawText ("PUMP", 24, 10, 200, 52, juce::Justification::centredLeft);
    g.setColour (cDim);
    g.setFont (font (11.0f));
    g.drawText ("TRAPWIN  /  VOLUME SHAPER & SIDECHAIN", 28, 58, 320, 14, juce::Justification::centredLeft);
    g.setColour (cDim.withAlpha (0.8f));
    g.setFont (font (11.0f, false));
    g.drawText ("drag points  /  drag a line to bend it  /  double-click to add or delete  /  shift = snap to grid",
                300, 62, baseW - 316, 16, juce::Justification::centredRight);
    g.setColour (cPanel);
    g.fillRoundedRectangle (16, 372, (float) baseW - 32, 116, 10.0f);
}

void PumpPanel::resized()
{
    // top bar: shape + rate + trigger + band
    int x = 300;
    shapeLabel.setBounds (x, 12, 160, 14);
    shapeBox.setBounds (x, 28, 160, 30);
    x += 172;
    const int widths[] = { 110, 110, 100 };
    for (size_t i = 0; i < choices.size(); ++i)
    {
        choices[i].l.setBounds (x, 12, widths[i], 14);
        choices[i].box.setBounds (x, 28, widths[i], 30);
        x += widths[i] + 12;
    }

    curve.setBounds (16, 82, baseW - 32, 278);

    const int kw = (baseW - 32) / (int) knobs.size();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int kx = 16 + (int) i * kw;
        knobs[i].l.setBounds (kx, 378, kw, 16);
        knobs[i].s.setBounds (kx + kw / 2 - 45, 394, 90, 90);
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
