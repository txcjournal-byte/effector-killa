#include "Controls.h"

namespace ek::ui
{
using namespace juce;

// ============================================================================
Knob::Knob (Style s) : style (s) { setRepaintsOnMouseActivity (false); }
Knob::~Knob() = default;

void Knob::bindTo (RangedAudioParameter& param)
{
    parameter = &param;
    defaultValue = param.getDefaultValue();
    attachment = std::make_unique<ParameterAttachment> (param, [this] (float denorm)
    {
        value = parameter->convertTo0to1 (denorm);
        repaint();
    });
    attachment->sendInitialUpdate();
}

void Knob::setNormValue (float v, NotificationType n)
{
    v = jlimit (0.0f, 1.0f, v);
    if (v == value) return;
    value = v;
    repaint();
    if (n != dontSendNotification && onUserChange) onUserChange (v);
}

void Knob::tick()
{
    if (! getDisplayValue) return;
    const float d = getDisplayValue();
    if (std::abs (d - lastDisplayed) > 0.002f) repaint();
}

void Knob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float shownValue = getDisplayValue ? getDisplayValue() : value;
    lastDisplayed = shownValue;
    const float angle = theme::knobStart + shownValue * (theme::knobEnd - theme::knobStart);

    if (style == Style::Small)
    {
        // tick marks around the knob
        const float r = b.getWidth() * 0.5f / 1.45f;
        g.setColour (theme::cream.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = theme::knobStart + (float) i / 10.0f * (theme::knobEnd - theme::knobStart);
            const float r0 = r * 1.18f, r1 = r * (i % 5 == 0 ? 1.4f : 1.3f);
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), 1.6f);
        }
        theme::drawKnob (g, c, r, angle, hot || dragging);
    }
    else
    {
        theme::drawKnob (g, c, b.getWidth() * 0.5f / 1.3f, angle, hot || dragging);
    }
}

void Knob::userSet (float v)
{
    v = jlimit (0.0f, 1.0f, v);
    if (attachment != nullptr) attachment->setValueAsPartOfGesture (parameter->convertFrom0to1 (v));
    else { value = v; repaint(); }
    if (onUserChange) onUserChange (v);
}

void Knob::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    dragging = true;
    dragStartValue = value;
    lastDragPos = e.position;
    if (onGestureStart) onGestureStart();
    if (attachment != nullptr) attachment->beginGesture();
    if (onUserChange) onUserChange (value);
    repaint();
}

void Knob::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    // incremental: Shift (fine) can be pressed or released mid-drag without jumps
    const float sensitivity = e.mods.isShiftDown() ? 1500.0f : 260.0f;
    const auto d = e.position - lastDragPos;
    lastDragPos = e.position;
    dragStartValue = jlimit (0.0f, 1.0f, dragStartValue + (d.x - d.y) / sensitivity);
    userSet (dragStartValue);
}

void Knob::mouseUp (const MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    if (attachment != nullptr) attachment->endGesture();
    if (onGestureEnd) onGestureEnd();
    repaint();
}

void Knob::mouseDoubleClick (const MouseEvent&)
{
    if (onGestureStart) onGestureStart();
    if (attachment != nullptr) attachment->setValueAsCompleteGesture (parameter->convertFrom0to1 (defaultValue));
    else { value = defaultValue; repaint(); }
    if (onUserChange) onUserChange (defaultValue);
    if (onGestureEnd) onGestureEnd();
}

void Knob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const float step = (e.mods.isShiftDown() ? 0.005f : 0.02f) * (w.deltaY > 0 ? 1.0f : (w.deltaY < 0 ? -1.0f : 0.0f)) * (w.isReversed ? -1.0f : 1.0f);
    if (step == 0.0f) return;
    const float v = jlimit (0.0f, 1.0f, value + step);
    if (onGestureStart) onGestureStart();
    if (attachment != nullptr) attachment->setValueAsCompleteGesture (parameter->convertFrom0to1 (v));
    else { value = v; repaint(); }
    if (onUserChange) onUserChange (v);
    if (onGestureEnd) onGestureEnd();
}

// ============================================================================
LitButton::LitButton (String t, Colour c, float fh) : text (std::move (t)), colour (c), fontHeight (fh) {}

void LitButton::paint (Graphics& g)
{
    bool lit = isLit ? isLit() : down;
    if (lit && blinkWhenLit) lit = (Time::getMillisecondCounter() / 450) % 2 == 0;
    lastLit = isLit ? isLit() : down;
    lastBlink = lit;
    theme::drawLitButton (g, getLocalBounds().toFloat(), colour, lit || down, down, text, fontHeight);
}

void LitButton::refresh()
{
    bool lit = isLit ? isLit() : down;
    bool blink = lit && blinkWhenLit ? (Time::getMillisecondCounter() / 450) % 2 == 0 : lit;
    if (lit != lastLit || blink != lastBlink) repaint();
}

void LitButton::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    down = true;
    if (onPress) onPress (true);
    repaint();
}

void LitButton::mouseUp (const MouseEvent& e)
{
    if (! down) return;
    down = false;
    if (onPress) onPress (false);
    if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    repaint();
}

// ============================================================================
ThreeWaySwitch::ThreeWaySwitch (std::array<float, 3> p, Rectangle<float> t, float w, bool lit)
    : pos (p), track (t), thumbW (w), litThumb (lit) {}

int ThreeWaySwitch::nearest (float x) const
{
    int best = 0;
    for (int i = 1; i < 3; ++i)
        if (std::abs (pos[(size_t) i] - x) < std::abs (pos[(size_t) best] - x)) best = i;
    return best;
}

void ThreeWaySwitch::paint (Graphics& g)
{
    const int p = getPosition ? getPosition() : 0;
    shown = p;
    theme::drawTrack (g, track);
    if (showLeds)
        for (int i = 0; i < 3; ++i)
            theme::drawLed (g, leds[(size_t) i], 6.0f, i == p ? Colour (0xffffe14a) : Colour (0xff2fae4f), i == p);
    const float x = dragX >= 0.0f ? jlimit (pos[0], pos[2], dragX) : pos[(size_t) p];
    const auto thumb = Rectangle<float> (thumbW, track.getHeight() + 6.0f).withCentre ({ x, track.getCentreY() });
    theme::drawSliderThumb (g, thumb, litThumb);
}

void ThreeWaySwitch::refresh()
{
    const int p = getPosition ? getPosition() : 0;
    if (p != shown) repaint();
}

void ThreeWaySwitch::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
        if (clickZones[(size_t) i].contains (e.position))
        {
            if (onChange) onChange (i);
            repaint();
            return;
        }
    dragX = e.position.x;
    repaint();
}

void ThreeWaySwitch::mouseDrag (const MouseEvent& e)
{
    if (dragX < 0.0f) return;
    dragX = e.position.x;
    repaint();
}

void ThreeWaySwitch::mouseUp (const MouseEvent& e)
{
    if (dragX < 0.0f) return;
    dragX = -1.0f;
    if (onChange) onChange (nearest (e.position.x));
    repaint();
}

// ============================================================================
CableToggle::CableToggle (Rectangle<float> t, Point<float> l) : track (t), led (l) {}

void CableToggle::paint (Graphics& g)
{
    const bool on = isOn ? isOn() : false;
    shown = on;
    theme::drawTrack (g, track);
    const float r = track.getHeight() * 0.47f;
    const Point<float> c (on ? track.getRight() - r - 2.0f : track.getX() + r + 2.0f, track.getCentreY());
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (r * 2.1f, r * 2.1f).withCentre (c.translated (1.5f, 2.0f)));
    ColourGradient metal (Colour (0xffe9e5de), c.x - r, c.y - r, Colour (0xff4f4a44), c.x + r, c.y + r, false);
    metal.addColour (0.5, Colour (0xffa8a198));
    g.setGradientFill (metal);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.25f));
    for (int i = 0; i < 12; ++i)
    {
        const float a = (float) i * MathConstants<float>::pi / 6.0f;
        g.drawLine (c.x, c.y, c.x + r * 0.9f * std::cos (a), c.y + r * 0.9f * std::sin (a), 0.8f);
    }
    theme::drawLed (g, led, 7.0f, theme::amber, on);
}

void CableToggle::refresh() { if ((isOn ? isOn() : false) != shown) repaint(); }

void CableToggle::mouseDown (const MouseEvent&)
{
    if (onChange) onChange (! (isOn ? isOn() : false));
    repaint();
}

// ============================================================================
ArrowButton::ArrowButton (bool u) : up (u) {}

void ArrowButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    theme::drawSocket (g, r, 4.0f);
    auto face = r.reduced (4.0f);
    if (down) face = face.translated (0.0f, 1.0f);
    ColourGradient grad (Colour (0xff3c3833), face.getX(), face.getY(), Colour (0xff141210), face.getX(), face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 3.0f);
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (face.reduced (1.0f), 3.0f, 1.0f);
    Path tri;
    const auto c = face.getCentre();
    const float s = face.getHeight() * 0.28f;
    if (up) tri.addTriangle (c.x - s * 1.2f, c.y + s * 0.7f, c.x + s * 1.2f, c.y + s * 0.7f, c.x, c.y - s * 0.8f);
    else tri.addTriangle (c.x - s * 1.2f, c.y - s * 0.7f, c.x + s * 1.2f, c.y - s * 0.7f, c.x, c.y + s * 0.8f);
    g.setColour (down ? theme::amber : theme::cream);
    g.fillPath (tri);
}

void ArrowButton::mouseDown (const MouseEvent&) { down = true; repaint(); }
void ArrowButton::mouseUp (const MouseEvent& e)
{
    down = false;
    repaint();
    if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
}

} // namespace ek::ui
