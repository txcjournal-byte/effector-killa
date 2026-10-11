#include "UnitControls.h"

namespace ek::ui
{
using namespace juce;

namespace
{
constexpr float labelH = 13.0f;
constexpr float valueH = 15.0f;

Point<float> polar (Point<float> c, float r, float angle) { return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) }; }
float angleFor (float norm) { return theme::knobStart + jlimit (0.0f, 1.0f, norm) * (theme::knobEnd - theme::knobStart); }

double nowSec() { return Time::getMillisecondCounterHiRes() * 0.001; }
} // namespace

// ============================================================================
void drawRackKnob (Graphics& g, Point<float> c, float r, float angle, const faceplate::Palette& pal, bool hot)
{
    // contact shadow on the plate
    g.setColour (Colours::black.withAlpha (0.42f));
    g.fillEllipse (Rectangle<float> (r * 2.25f, r * 2.25f).withCentre (c.translated (0.0f, r * 0.2f)));

    // ribbed skirt
    ColourGradient skirt (Colour (0xff3a3d44), c.x - r, c.y - r, Colour (0xff060708), c.x + r * 0.7f, c.y + r, false);
    g.setGradientFill (skirt);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.07f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i / 28.0f * MathConstants<float>::twoPi + angle;
        g.drawLine ({ polar (c, r * 0.82f, a), polar (c, r * 0.98f, a) }, 0.9f);
    }

    // body
    ColourGradient body (Colour (0xff474a52), c.x - r * 0.45f, c.y - r * 0.6f, Colour (0xff0b0c0e), c.x + r * 0.4f, c.y + r * 0.7f, true);
    g.setGradientFill (body);
    g.fillEllipse (Rectangle<float> (r * 1.6f, r * 1.6f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawEllipse (Rectangle<float> (r * 1.6f, r * 1.6f).withCentre (c), 0.8f);

    // brushed chrome cap
    const auto cap = Rectangle<float> (r * 0.98f, r * 0.98f).withCentre (c);
    ColourGradient chrome (Colour (0xfff4f6fa), cap.getX(), cap.getY(), Colour (0xff6d737e), cap.getRight(), cap.getBottom(), false);
    chrome.addColour (0.45, Colour (0xffc3c8d1));
    chrome.addColour (0.55, Colour (0xff9aa0ab));
    g.setGradientFill (chrome);
    g.fillEllipse (cap);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (cap, 0.8f);
    g.setColour (Colours::white.withAlpha (0.12f));
    for (int i = 1; i < 4; ++i)
        g.drawEllipse (cap.reduced (cap.getWidth() * 0.11f * (float) i), 0.5f);

    // pointer: coloured line on the body + dark notch in the cap
    const auto accent = pal.accent;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawLine ({ polar (c, r * 0.5f, angle), polar (c, r * 0.94f, angle) }, r * 0.2f);
    g.setColour (accent.withMultipliedBrightness (1.15f));
    g.drawLine ({ polar (c, r * 0.52f, angle), polar (c, r * 0.92f, angle) }, r * 0.11f);
    g.setColour (Colour (0xff15171b));
    g.drawLine ({ polar (c, r * 0.08f, angle), polar (c, r * 0.44f, angle) }, r * 0.08f);

    if (hot)
    {
        g.setColour (pal.led.withAlpha (0.35f));
        g.drawEllipse (Rectangle<float> (r * 2.12f, r * 2.12f).withCentre (c), 1.4f);
    }
}

// ============================================================================
RackControl::RackControl (int s, int p, const ParamSpec* sp, RangedAudioParameter* hp, const faceplate::Palette& palette)
    : slot (s), param (p), kind (kindFor (sp)), spec (sp), hostParam (hp), pal (palette)
{
    label = spec != nullptr ? spec->name.toUpperCase() : String ("DRY/WET");
    defaultValue = spec != nullptr ? spec->defaultNorm() : 1.0f;
    if (hostParam != nullptr)
    {
        attachment = std::make_unique<ParameterAttachment> (*hostParam, [this] (float denorm)
        {
            value = hostParam->convertTo0to1 (denorm);
            repaint();
        });
        attachment->sendInitialUpdate();
    }
    setMouseCursor (kind == Kind::Rotary || kind == Kind::Selector ? MouseCursor::UpDownResizeCursor : MouseCursor::PointingHandCursor);
    setRepaintsOnMouseActivity (true);
}

RackControl::~RackControl() = default;

RackControl::Kind RackControl::kindFor (const ParamSpec* s)
{
    if (s == nullptr) return Kind::Rotary;
    switch (s->unit)
    {
        case Unit::Toggle:  return Kind::Toggle;
        case Unit::Choice:  return s->choices.size() <= 4 ? Kind::LedRow : Kind::Selector;
        case Unit::Sync:
        case Unit::Pattern:
        case Unit::Bits:
        case Unit::Voices:
        case Unit::Stages:  return Kind::Selector;
        default:            return Kind::Rotary;
    }
}

int RackControl::preferredWidth (Kind k, const ParamSpec* s)
{
    switch (k)
    {
        case Kind::Toggle:   return 54;
        case Kind::Selector: return 68;
        case Kind::LedRow:
        {
            int longest = 4;
            if (s != nullptr) for (auto& c : s->choices) longest = jmax (longest, c.length());
            return jlimit (60, 88, 26 + longest * 6);
        }
        case Kind::Rotary:
        default:             return 66;
    }
}

int RackControl::numSteps() const
{
    if (spec != nullptr && spec->isDiscrete()) return jmax (2, spec->numSteps());
    return 0;
}

int RackControl::stepIndex() const
{
    const int n = numSteps();
    return n > 1 ? roundToInt (value * (float) (n - 1)) : 0;
}

float RackControl::stepToNorm (int step) const
{
    const int n = numSteps();
    return n > 1 ? (float) jlimit (0, n - 1, step) / (float) (n - 1) : 0.0f;
}

String RackControl::valueText() const
{
    if (spec == nullptr) return String (roundToInt (value * 100.0f)) + " %";
    return spec->format (spec->toReal (value));
}

Rectangle<float> RackControl::getFaceBounds() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (labelH);
    if (kind != Kind::LedRow) r.removeFromBottom (valueH + 2.0f);
    else r.removeFromBottom (2.0f);
    if (kind == Kind::Rotary || kind == Kind::Selector)
    {
        const float s = jmin (r.getWidth(), r.getHeight());
        return r.withSizeKeepingCentre (s, s);
    }
    return r.reduced (3.0f, 1.0f);
}

void RackControl::tick()
{
    if (! getEffectiveValue) return;
    const float e = getEffectiveValue();
    if (std::abs (e - shownEffective) > 0.003f) repaint();
}

// ----------------------------------------------------------------------------
void RackControl::paintLabel (Graphics& g)
{
    auto r = getLocalBounds().toFloat().removeFromTop (labelH);
    g.setFont (theme::monoLabel (9.0f).boldened());
    g.setColour (pal.ink.withAlpha (0.92f));
    g.drawFittedText (label, r.reduced (1.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.55f);
}

void RackControl::paintValue (Graphics& g, const String& text)
{
    auto r = getLocalBounds().toFloat().removeFromBottom (valueH).reduced (3.0f, 0.5f);
    g.setColour (Colour (0xff07080a).withAlpha (0.88f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 3.0f, 0.8f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) r.getBottom(), r.getX() + 3.0f, r.getRight() - 3.0f);
    g.setFont (theme::monoLabel (9.5f).boldened());
    g.setColour (pal.led.withAlpha (isMouseOver() || dragging ? 1.0f : 0.88f));
    g.drawFittedText (text, r.reduced (2.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
}

void RackControl::paintRotary (Graphics& g, bool selector)
{
    const auto face = getFaceBounds();
    const auto c = face.getCentre();
    const float r = face.getWidth() * 0.5f * 0.6f;
    const float ringR = r + 5.0f;
    const auto ink = pal.ink;

    if (! selector)
    {
        // scale ticks
        g.setColour (ink.withAlpha (0.42f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = angleFor ((float) i / 10.0f);
            g.drawLine ({ polar (c, ringR + 2.5f, a), polar (c, ringR + (i % 5 == 0 ? 6.0f : 4.5f), a) }, i % 5 == 0 ? 1.4f : 1.0f);
        }
        // value arc (bipolar parameters grow from the centre)
        const bool bipolar = spec != nullptr && spec->min < 0.0f && spec->max > 0.0f;
        Path track;
        track.addCentredArc (c.x, c.y, ringR - 1.5f, ringR - 1.5f, 0.0f, theme::knobStart, theme::knobEnd, true);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.strokePath (track, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        const float a0 = bipolar ? 0.0f : theme::knobStart, a1 = angleFor (value);
        if (std::abs (a1 - a0) > 0.01f)
        {
            Path arc;
            arc.addCentredArc (c.x, c.y, ringR - 1.5f, ringR - 1.5f, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
            g.setColour (pal.led.withAlpha (0.25f));
            g.strokePath (arc, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (pal.led);
            g.strokePath (arc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        drawRackKnob (g, c, r, angleFor (value), pal, dragging || isMouseOver());
    }
    else
    {
        const int n = numSteps();
        const int cur = stepIndex();
        const bool many = n > 13;
        for (int i = 0; i < n; ++i)
        {
            if (many && i != 0 && i != n - 1 && i != cur && i % 2 == 1) continue;
            const auto p = polar (c, ringR + 1.5f, angleFor ((float) i / (float) (n - 1)));
            if (i == cur)
            {
                g.setColour (pal.led.withAlpha (0.35f));
                g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (p));
                g.setColour (pal.led);
                g.fillEllipse (Rectangle<float> (3.6f, 3.6f).withCentre (p));
            }
            else
            {
                g.setColour (ink.withAlpha (0.4f));
                g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (p));
            }
        }
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (r * 2.25f, r * 2.25f).withCentre (c.translated (0.0f, 1.5f)));
        theme::drawChickenHeadKnob (g, c, r * 0.98f, angleFor (value));
        if (dragging || isMouseOver())
        {
            g.setColour (pal.led.withAlpha (0.3f));
            g.drawEllipse (Rectangle<float> (r * 2.2f, r * 2.2f).withCentre (c), 1.3f);
        }
    }

    // modulated / macro value marker
    if (getEffectiveValue)
    {
        shownEffective = getEffectiveValue();
        if (std::abs (shownEffective - value) > 0.006f)
        {
            const auto p = polar (c, ringR - 1.5f, angleFor (shownEffective));
            g.setColour (Colours::white.withAlpha (0.9f));
            g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (p));
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (p), 0.7f);
        }
    }
}

Rectangle<float> RackControl::ledRowItem (int i) const
{
    const auto face = getFaceBounds();
    const int n = jmax (1, numSteps());
    const float h = jmin (17.0f, face.getHeight() / (float) n);
    const float top = face.getY() + (face.getHeight() - h * (float) n) * 0.5f;
    return { face.getX(), top + h * (float) i, face.getWidth(), h };
}

void RackControl::paintLedRow (Graphics& g)
{
    const auto face = getFaceBounds();
    const int n = numSteps();
    const int cur = stepIndex();
    auto bank = ledRowItem (0).getUnion (ledRowItem (n - 1)).expanded (1.5f, 2.0f);
    g.setColour (Colours::black.withAlpha (0.26f));
    g.fillRoundedRectangle (bank, 4.0f);
    g.setColour (pal.ink.withAlpha (0.18f));
    g.drawRoundedRectangle (bank, 4.0f, 0.8f);
    juce::ignoreUnused (face);

    for (int i = 0; i < n; ++i)
    {
        auto row = ledRowItem (i);
        const bool on = i == cur;
        const auto ledC = Point<float> (row.getX() + 8.0f, row.getCentreY());
        theme::drawLed (g, ledC, 2.7f, pal.led, on, 0.8f);
        const String t = spec != nullptr ? spec->choices[i] : String();
        g.setFont (theme::monoLabel (8.5f).boldened());
        g.setColour (on ? pal.ink : pal.ink.withAlpha (0.55f));
        g.drawFittedText (t, row.withTrimmedLeft (15.0f).reduced (1.0f, 0.0f).toNearestInt(), Justification::centredLeft, 1, 0.6f);
    }
}

void RackControl::paintToggle (Graphics& g)
{
    const auto face = getFaceBounds();
    const bool on = value > 0.5f;
    auto b = face.withSizeKeepingCentre (jmin (34.0f, face.getWidth()), jmin (30.0f, face.getHeight()));
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (b.translated (0.0f, 1.5f), 4.0f);
    ColourGradient body (Colour (0xff3a3d44), b.getX(), b.getY(), Colour (0xff121316), b.getX(), b.getBottom(), false);
    if (dragging) body = ColourGradient (Colour (0xff121316), b.getX(), b.getY(), Colour (0xff2a2d33), b.getX(), b.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) b.getY() + 1, b.getX() + 3.0f, b.getRight() - 3.0f);
    theme::drawLed (g, b.getCentre(), 3.6f, pal.led, on, 1.0f);
    if (isMouseOver())
    {
        g.setColour (pal.led.withAlpha (0.35f));
        g.drawRoundedRectangle (b.expanded (1.0f), 5.0f, 1.0f);
    }
}

void RackControl::paint (Graphics& g)
{
    paintLabel (g);
    switch (kind)
    {
        case Kind::Rotary:   paintRotary (g, false); paintValue (g, valueText()); break;
        case Kind::Selector: paintRotary (g, true);  paintValue (g, valueText()); break;
        case Kind::LedRow:   paintLedRow (g); break;
        case Kind::Toggle:   paintToggle (g); paintValue (g, value > 0.5f ? "ON" : "OFF"); break;
    }
    if (paintOverlay) paintOverlay (g, getFaceBounds());
}

// ----------------------------------------------------------------------------
void RackControl::setFromUser (float norm, bool complete)
{
    norm = jlimit (0.0f, 1.0f, norm);
    if (attachment == nullptr) { value = norm; repaint(); return; }
    const float denorm = hostParam->convertFrom0to1 (norm);
    if (complete) attachment->setValueAsCompleteGesture (denorm);
    else attachment->setValueAsPartOfGesture (denorm);
}

void RackControl::showChoiceMenu()
{
    if (spec == nullptr) return;
    PopupMenu m;
    m.addSectionHeader (label);
    const int n = numSteps();
    for (int i = 0; i < n; ++i)
        m.addItem (1 + i, spec->format (spec->toReal (stepToNorm (i))), true, i == stepIndex());
    SafePointer<RackControl> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (safe->onGestureStart) safe->onGestureStart();
        safe->setFromUser (safe->stepToNorm (r - 1), true);
    });
}

void RackControl::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onRightClick && onRightClick (e.getScreenPosition())) return;
        PopupMenu m;
        m.addSectionHeader ("S" + String (slot + 1) + "  " + label);
        m.addItem (1, "Reset to default");
        SafePointer<RackControl> safe (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr || r != 1) return;
            if (safe->onGestureStart) safe->onGestureStart();
            safe->setFromUser (safe->defaultValue, true);
        });
        return;
    }

    moved = false;
    dragging = true;
    dragValue = value;
    lastDrag = e.position;

    if (kind == Kind::LedRow)
    {
        for (int i = 0; i < numSteps(); ++i)
            if (ledRowItem (i).expanded (0.0f, 1.0f).contains (e.position) && i != stepIndex())
            {
                if (onGestureStart) onGestureStart();
                setFromUser (stepToNorm (i), true);
            }
        return;
    }
    if (kind == Kind::Toggle)
    {
        if (onGestureStart) onGestureStart();
        setFromUser (value > 0.5f ? 0.0f : 1.0f, true);
        repaint();
        return;
    }
    if (onGestureStart) onGestureStart();
    if (attachment != nullptr) { attachment->beginGesture(); gestureOpen = true; }
    repaint();
}

void RackControl::mouseDrag (const MouseEvent& e)
{
    if (! dragging || kind == Kind::LedRow || kind == Kind::Toggle) return;
    const auto d = e.position - lastDrag;
    lastDrag = e.position;
    if (e.getDistanceFromDragStart() > 2) moved = true;
    const float sensitivity = e.mods.isShiftDown() ? 1500.0f : (kind == Kind::Selector ? 360.0f : 260.0f);
    dragValue = jlimit (0.0f, 1.0f, dragValue + (d.x - d.y) / sensitivity);
    float v = dragValue;
    if (kind == Kind::Selector) v = stepToNorm (roundToInt (dragValue * (float) (numSteps() - 1)));
    if (std::abs (v - value) > 1.0e-6f) setFromUser (v, false);
}

void RackControl::mouseUp (const MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    if (gestureOpen && attachment != nullptr) attachment->endGesture();
    gestureOpen = false;
    if (kind == Kind::Selector && ! moved) showChoiceMenu();
    repaint();
}

void RackControl::mouseDoubleClick (const MouseEvent& e)
{
    if (kind == Kind::LedRow || kind == Kind::Toggle || e.mods.isPopupMenu()) return;
    if (onGestureStart) onGestureStart();
    setFromUser (defaultValue, true);
}

void RackControl::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const float dir = (w.deltaY > 0 ? 1.0f : (w.deltaY < 0 ? -1.0f : 0.0f)) * (w.isReversed ? -1.0f : 1.0f);
    if (dir == 0.0f) return;
    float v;
    if (numSteps() > 0) v = stepToNorm (stepIndex() + (int) dir);
    else v = value + dir * (e.mods.isShiftDown() ? 0.005f : 0.02f);
    if (onGestureStart) onGestureStart();
    setFromUser (v, true);
}

// ============================================================================
SideButton::SideButton (Icon i, Colour c) : icon (i), colour (c)
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool SideButton::litNow() const
{
    const bool lit = isLit && isLit();
    if (lit && blinkWhenLit) return (Time::getMillisecondCounter() / 420) % 2 == 0;
    return lit;
}

void SideButton::tick()
{
    const bool lit = isLit && isLit();
    const bool blink = blinkWhenLit && lit && (Time::getMillisecondCounter() / 420) % 2 == 0;
    const bool en = ! isEnabledFn || isEnabledFn();
    if (lit != lastLit || blink != lastBlink || en != lastEnabled) repaint();
}

void SideButton::drawIcon (Graphics& g, Icon icon, Rectangle<float> r, Colour c)
{
    const auto cen = r.getCentre();
    const float s = jmin (r.getWidth(), r.getHeight());
    g.setColour (c);
    switch (icon)
    {
        case Icon::Menu:
        {
            Path p;
            const int teeth = 8;
            const float ro = s * 0.46f, ri = s * 0.34f;
            for (int i = 0; i < teeth * 4; ++i)
            {
                const float a = (float) i / (float) (teeth * 4) * MathConstants<float>::twoPi;
                const bool outer = (i % 4) == 1 || (i % 4) == 2;
                const auto pt = polar (cen, outer ? ro : ri, a);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            p.addEllipse (Rectangle<float> (s * 0.3f, s * 0.3f).withCentre (cen));
            p.setUsingNonZeroWinding (false);
            g.fillPath (p);
            break;
        }
        case Icon::Power:
        {
            Path arc;
            const float rr = s * 0.36f;
            arc.addCentredArc (cen.x, cen.y + s * 0.03f, rr, rr, 0.0f, 0.65f, MathConstants<float>::twoPi - 0.65f, true);
            g.strokePath (arc, PathStrokeType (s * 0.1f, PathStrokeType::curved, PathStrokeType::rounded));
            g.drawLine ({ cen.x, cen.y - s * 0.45f, cen.x, cen.y - s * 0.02f }, s * 0.1f);
            break;
        }
        case Icon::Solo:
        {
            g.setFont (theme::stencilFont (s * 1.05f));
            g.drawText ("S", r.translated (0.0f, s * 0.02f), Justification::centred, false);
            break;
        }
        case Icon::Lock:
        {
            auto body = Rectangle<float> (s * 0.62f, s * 0.44f).withCentre (cen.translated (0.0f, s * 0.14f));
            Path shackle;
            const float sw = s * 0.21f;
            shackle.addCentredArc (cen.x, body.getY(), sw, sw * 1.15f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
            g.strokePath (shackle, PathStrokeType (s * 0.09f, PathStrokeType::curved, PathStrokeType::rounded));
            g.drawLine (cen.x - sw, body.getY() - 0.5f, cen.x - sw, body.getY() + 1.0f, s * 0.09f);
            g.drawLine (cen.x + sw, body.getY() - 0.5f, cen.x + sw, body.getY() + 1.0f, s * 0.09f);
            g.fillRoundedRectangle (body, s * 0.06f);
            g.setColour (Colours::black.withAlpha (0.7f));
            g.fillEllipse (Rectangle<float> (s * 0.1f, s * 0.1f).withCentre (body.getCentre().translated (0.0f, -s * 0.03f)));
            break;
        }
    }
}

void SideButton::paint (Graphics& g)
{
    lastLit = isLit && isLit();
    lastBlink = blinkWhenLit && lastLit && (Time::getMillisecondCounter() / 420) % 2 == 0;
    lastEnabled = ! isEnabledFn || isEnabledFn();
    const bool lit = litNow();
    auto r = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 3.5f);
    ColourGradient body (Colour (0xff363940), r.getX(), r.getY(), Colour (0xff121417), r.getX(), r.getBottom(), false);
    if (down) body = ColourGradient (Colour (0xff101214), r.getX(), r.getY(), Colour (0xff272a30), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 3.5f);
    if (lit)
    {
        ColourGradient glow (colour.withAlpha (0.42f), r.getCentreX(), r.getCentreY(), colour.withAlpha (0.06f), r.getRight(), r.getBottom(), true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
    }
    g.setColour (lit ? colour.withAlpha (0.85f) : Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r, 3.5f, lit ? 1.2f : 1.0f);
    if (! down)
    {
        g.setColour (Colours::white.withAlpha (isMouseOver() ? 0.24f : 0.14f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
    }

    auto ir = r.reduced (6.0f).translated (0.0f, down ? 0.5f : 0.0f);
    Colour ic = lit ? colour.brighter (0.5f) : Colour (0xffa3aab6);
    if (! lastEnabled) ic = ic.withAlpha (0.3f);
    else if (isMouseOver() && ! lit) ic = Colour (0xffd8dde6);
    if (lit)
    {
        Graphics::ScopedSaveState ss (g);
        g.setOpacity (0.45f);
        drawIcon (g, icon, ir.expanded (1.2f), colour);
    }
    drawIcon (g, icon, ir, ic);
}

void SideButton::mouseDown (const MouseEvent&)
{
    if (isEnabledFn && ! isEnabledFn()) return;
    down = true;
    repaint();
}

void SideButton::mouseUp (const MouseEvent& e)
{
    if (! down) return;
    down = false;
    repaint();
    if (onClick && getLocalBounds().contains (e.getPosition())) onClick();
}

// ============================================================================
UnitLcd::UnitLcd()
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void UnitLcd::setText (const String& t, bool animate)
{
    if (t == text) return;
    text = t;
    scanStart = animate && ! reduceMotion ? nowSec() : -1.0;
    repaint();
}

void UnitLcd::tick()
{
    if (scanStart < 0.0) return;
    if (nowSec() - scanStart > 0.45) scanStart = -1.0;
    repaint();
}

void UnitLcd::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    // bezel
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 3.0f);
    g.setColour (Colour (0xff2a2d33));
    g.fillRoundedRectangle (r, 3.0f);
    auto glass = r.reduced (2.0f);
    ColourGradient gl (Colour (0xff0c1118), glass.getX(), glass.getY(), Colour (0xff030406), glass.getX(), glass.getBottom(), false);
    g.setGradientFill (gl);
    g.fillRoundedRectangle (glass, 2.0f);
    g.setColour (isMouseOver() ? theme::vfd.withAlpha (0.5f) : Colours::black);
    g.drawRoundedRectangle (glass, 2.0f, 0.8f);

    auto content = glass.reduced (6.0f, 0.0f);
    auto arrow = content.removeFromRight (10.0f);
    float reveal = 1.0f;
    if (scanStart >= 0.0) reveal = (float) jlimit (0.0, 1.0, (nowSec() - scanStart) / 0.35);

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (content.withWidth (content.getWidth() * reveal).toNearestInt());
        theme::drawGlowText (g, text.toUpperCase(), content, theme::vfdFont (12.5f), theme::vfd, Justification::centredLeft, 0.8f);
    }
    if (reveal < 1.0f)
    {
        const float x = content.getX() + content.getWidth() * reveal;
        g.setColour (theme::vfd.withAlpha (0.8f));
        g.fillRect (x - 1.0f, glass.getY() + 2.0f, 2.0f, glass.getHeight() - 4.0f);
    }

    Path tri;
    const auto ac = arrow.getCentre();
    tri.addTriangle (ac.x - 3.5f, ac.y - 2.0f, ac.x + 3.5f, ac.y - 2.0f, ac.x, ac.y + 2.5f);
    g.setColour (theme::vfd.withAlpha (isMouseOver() ? 1.0f : 0.7f));
    g.fillPath (tri);

    // glass reflection
    g.setColour (Colours::white.withAlpha (0.045f));
    g.fillRoundedRectangle (glass.withHeight (glass.getHeight() * 0.45f), 2.0f);
}

void UnitLcd::mouseDown (const MouseEvent&) { if (onClick) onClick(); }

// ============================================================================
MsSwitch::MsSwitch()
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void MsSwitch::tick()
{
    const int m = getMode ? getMode() : 0;
    if (m != shown) repaint();
}

void MsSwitch::paint (Graphics& g)
{
    shown = getMode ? getMode() : 0;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 3.0f);
    ColourGradient body (Colour (0xff33363d), r.getX(), r.getY(), Colour (0xff111316), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (isMouseOver() ? pal.led.withAlpha (0.6f) : Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    const bool on = shown != 0;
    theme::drawLed (g, { r.getX() + 9.0f, r.getCentreY() }, 2.6f, Colour (0xffffc861), on, 0.8f);
    g.setFont (theme::monoLabel (9.0f).boldened());
    g.setColour (on ? Colour (0xffffd98f) : Colour (0xffc9ced8));
    g.drawText (shown == 0 ? "ST" : (shown == 1 ? "MID" : "SIDE"), r.withTrimmedLeft (15.0f), Justification::centred, false);
}

void MsSwitch::mouseDown (const MouseEvent&) { if (onClick) onClick(); }

} // namespace ek::ui
