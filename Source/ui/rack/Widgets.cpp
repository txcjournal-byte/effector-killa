#include "Widgets.h"

namespace ek::ui
{
using namespace juce;

void drawCard (Graphics& g, Rectangle<float> r, float corner)
{
    ColourGradient fill (Colour (0xff0d1422), r.getX(), r.getY(), Colour (0xff070b13), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, corner);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
}

void drawSmallLabel (Graphics& g, const String& t, Rectangle<float> r, Justification j, Colour c, float size)
{
    g.setFont (theme::monoLabel (size));
    g.setColour (c);
    g.drawText (t, r, j, false);
}

// ============================================================================
TwButton::TwButton (String t, Colour c, float fh) : text (std::move (t)), colour (c), fontHeight (fh)
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void TwButton::tick()
{
    const bool lit = isLit && isLit();
    const bool blink = blinkWhenLit && lit && (Time::getMillisecondCounter() / 450) % 2 == 0;
    const bool en = ! isEnabledFn || isEnabledFn();
    if (lit != lastLit || blink != lastBlink || en != lastEnabled) repaint();
}

void TwButton::paint (Graphics& g)
{
    lastLit = isLit && isLit();
    lastBlink = blinkWhenLit && lastLit && (Time::getMillisecondCounter() / 450) % 2 == 0;
    lastEnabled = ! isEnabledFn || isEnabledFn();
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool lit = lastLit && ! lastBlink;
    if (solid)
    {
        theme::drawLitButton (g, r, colour, true, down, text, fontHeight);
        return;
    }

    ColourGradient body (Colour (0xff1b2333), r.getX(), r.getY(), Colour (0xff0b0f18), r.getX(), r.getBottom(), false);
    if (down) body = ColourGradient (Colour (0xff0b0f18), r.getX(), r.getY(), Colour (0xff141b28), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 5.0f);
    if (lit)
    {
        g.setColour (colour.withAlpha (0.18f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    g.setColour (lit ? colour.withAlpha (0.9f) : (isMouseOver() ? theme::accent.withAlpha (0.45f) : Colour (0x40a9b6cc)));
    g.drawRoundedRectangle (r, 5.0f, lit ? 1.4f : 1.0f);

    g.setFont (theme::monoLabel (fontHeight).boldened());
    Colour tc = lit ? colour.brighter (0.4f) : theme::silver;
    if (! lastEnabled) tc = tc.withAlpha (0.3f);
    g.setColour (tc);
    g.drawFittedText (text, r.reduced (3.0f, 1.0f).toNearestInt(), Justification::centred, 2, 0.8f);
}

void TwButton::mouseDown (const MouseEvent& e)
{
    if (isEnabledFn && ! isEnabledFn()) return;
    if (e.mods.isPopupMenu() && ! onPress) { if (onClick) onClick(); return; }
    down = true;
    repaint();
    if (onPress) onPress (true);
}

void TwButton::mouseUp (const MouseEvent& e)
{
    if (! down) return;
    down = false;
    repaint();
    if (onPress) onPress (false);
    if (onClick && getLocalBounds().contains (e.getPosition())) onClick();
}

// ============================================================================
LabeledKnob::LabeledKnob (String n, Knob::Style style) : knob (style), name (std::move (n))
{
    addAndMakeVisible (knob);
    knob.onUserChange = [this] (float) { tick(); };
}

void LabeledKnob::tick()
{
    knob.tick();
    const auto v = valueText ? valueText() : String (roundToInt (knob.getNormValue() * 100.0f));
    if (v != shownValue) { shownValue = v; repaint(); }
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (14);
    r.removeFromBottom (14);
    const int s = jmin (r.getWidth(), r.getHeight());
    knob.setBounds (r.withSizeKeepingCentre (s, s));
}

void LabeledKnob::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setFont (theme::monoLabel (9.5f));
    g.setColour (theme::silverDim);
    g.drawFittedText (name.toUpperCase(), r.removeFromTop (14.0f).toNearestInt(), Justification::centred, 1, 0.6f);
    if (shownValue.isEmpty()) shownValue = valueText ? valueText() : String();
    auto v = r.removeFromBottom (14.0f);
    g.setFont (theme::monoLabel (10.0f).boldened());
    g.setColour (accent);
    g.drawFittedText (shownValue, v.toNearestInt(), Justification::centred, 1, 0.7f);
}

// ============================================================================
void PeakMeter::tick()
{
    const float p = getPeak ? getPeak() : 0.0f;
    const float db = jlimit (-60.0f, 6.0f, Decibels::gainToDecibels (p, -60.0f));
    const float v = (db + 60.0f) / 66.0f;
    const float next = v > shown ? v : shown - 0.025f;
    if (v >= hold) { hold = v; holdFrames = 45; }
    else if (--holdFrames < 0) hold = jmax (0.0f, hold - 0.01f);
    if (std::abs (next - shown) > 0.002f || holdFrames == 0) { shown = jmax (0.0f, next); repaint(); }
}

void PeakMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff05080e));
    g.fillRoundedRectangle (r, 2.0f);
    const auto fill = r.reduced (1.5f);
    const float h = fill.getHeight() * shown;
    ColourGradient grad (theme::accentDeep, fill.getX(), fill.getBottom(), theme::killRed, fill.getX(), fill.getY(), false);
    grad.addColour (0.82, theme::accent);
    g.setGradientFill (grad);
    g.fillRect (fill.withTop (fill.getBottom() - h));
    const float hy = fill.getBottom() - fill.getHeight() * hold;
    g.setColour (hold > 60.0f / 66.0f ? theme::killRed : theme::silver);
    g.fillRect (fill.getX(), hy, fill.getWidth(), 1.5f);
}

} // namespace ek::ui
