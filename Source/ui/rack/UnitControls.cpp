#include "UnitControls.h"
#include "CardStyle.h"

namespace ek::ui
{
using namespace juce;
namespace cs = ek::card;

namespace
{
constexpr float labelH = 13.0f;
constexpr float valueH = 16.0f;
constexpr float ringRoom = 7.0f;   // space around the value arc reserved for mod rings

Point<float> polar (Point<float> c, float r, float angle) { return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) }; }
float angleFor (float norm) { return theme::knobStart + jlimit (0.0f, 1.0f, norm) * (theme::knobEnd - theme::knobStart); }

double nowSec() { return Time::getMillisecondCounterHiRes() * 0.001; }

// flat TrapWin knob body: dark disc, thin line, white pointer dot
void paintDisc (Graphics& g, Point<float> c, float r, float angle, Colour accent, bool hot)
{
    if (hot)
    {
        ColourGradient halo (accent.withAlpha (0.16f), c.x, c.y, accent.withAlpha (0.0f), c.x + r * 1.7f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 3.4f, r * 3.4f).withCentre (c));
    }
    // soft contact shadow
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.translated (0.0f, 1.5f)).expanded (1.0f));
    ColourGradient disc (Colour (0xff141d2c), c.x, c.y - r, Colour (0xff070b12), c.x, c.y + r, false);
    g.setGradientFill (disc);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    // faint top sheen
    ColourGradient sheen (Colours::white.withAlpha (0.06f), c.x, c.y - r, Colours::white.withAlpha (0.0f), c.x, c.y, false);
    g.setGradientFill (sheen);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c).reduced (1.0f));
    g.setColour (hot ? accent.withAlpha (0.55f) : cs::line.withMultipliedAlpha (1.3f));
    g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c).reduced (0.5f), 1.0f);
    // pointer dot
    const auto pd = polar (c, r * 0.64f, angle);
    g.setColour (accent.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (r * 0.46f, r * 0.46f).withCentre (pd));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (r * 0.22f, r * 0.22f).withCentre (pd));
}
} // namespace

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
        case Kind::Toggle:   return 56;
        case Kind::Selector: return 66;
        case Kind::LedRow:
        {
            int longest = 4;
            if (s != nullptr) for (auto& c : s->choices) longest = jmax (longest, c.length());
            return jlimit (58, 84, 30 + longest * 6);
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
    const float real = spec->toReal (value);
    // slow LFO rates etc.: the engine's text rounds to whole Hz, the card shows decimals
    if (spec->unit == Unit::Hz && real < 100.0f && std::abs (real - std::round (real)) > 0.005f)
        return String (real, real < 10.0f ? 2 : 1) + " Hz";
    return spec->format (real);
}

float RackControl::labelHeight() const { return labelH; }
float RackControl::valueHeight() const { return valueH; }

Rectangle<float> RackControl::getFaceBounds() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (labelH);
    if (kind != Kind::LedRow) r.removeFromBottom (valueH);
    else r.removeFromBottom (3.0f);
    if (kind == Kind::Rotary || kind == Kind::Selector)
    {
        // the square that hugs the value arc; mod rings (face / 2 + 3 ...) land in the reserved margin
        const float s = jmax (12.0f, jmin (r.getWidth(), r.getHeight()) - ringRoom * 2.0f);
        return r.withSizeKeepingCentre (s, s);
    }
    return r.reduced (4.0f, 1.0f);
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
    const bool hot = isMouseOver() || dragging;
    g.setFont (cs::monoFont (8.0f));
    g.setColour (hot ? cs::text : cs::text2.withAlpha (0.82f));
    g.drawFittedText (label, r.reduced (1.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
}

void RackControl::paintValue (Graphics& g, const String& text)
{
    auto r = getLocalBounds().toFloat().removeFromBottom (valueH).reduced (2.0f, 1.0f);
    const bool hot = isMouseOver() || dragging;
    if (hot)
    {
        // a dark pill appears under the value while touching the control
        g.setColour (cs::bg.withAlpha (0.85f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (pal.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f - 0.5f, 1.0f);
    }
    const auto f = cs::monoFont (8.8f);
    if (hot) cs::glowText (g, text, r, f, cs::text, Justification::centred, 0.6f);
    else
    {
        g.setFont (f);
        g.setColour (cs::text.withAlpha (0.78f));
        g.drawFittedText (text, r.reduced (2.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.6f);
    }
}

void RackControl::paintRotary (Graphics& g, bool selector)
{
    const auto face = getFaceBounds();
    const auto c = face.getCentre();
    const float arcR = face.getWidth() * 0.5f - 1.5f;
    const float discR = arcR * 0.74f;
    const bool hot = dragging || isMouseOver();
    const auto accent = pal.accent;

    // track
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, theme::knobStart, theme::knobEnd, true);
    g.setColour (cs::ice.withAlpha (0.10f));
    g.strokePath (track, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (! selector)
    {
        const bool bipolar = spec != nullptr && spec->min < 0.0f && spec->max > 0.0f;
        const float a0 = bipolar ? 0.0f : theme::knobStart, a1 = angleFor (value);
        if (bipolar)
        {
            g.setColour (cs::ice.withAlpha (0.35f));
            g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (polar (c, arcR + 3.2f, 0.0f)));
        }
        if (std::abs (a1 - a0) > 0.01f)
        {
            Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
            cs::strokeGlow (g, arc, accent, 2.2f, hot ? 1.3f : 0.9f);
            // bright tip
            g.setColour (accent.interpolatedWith (Colours::white, 0.55f));
            g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (polar (c, arcR, a1)));
        }
    }
    else
    {
        const int n = numSteps();
        const int cur = stepIndex();
        const bool many = n > 13;
        for (int i = 0; i < n; ++i)
        {
            if (many && i != 0 && i != n - 1 && i != cur && i % 2 == 1) continue;
            const auto p = polar (c, arcR, angleFor ((float) i / (float) (n - 1)));
            if (i == cur) cs::dotGlow (g, p, 2.1f, accent, 1.0f);
            else
            {
                g.setColour (cs::ice.withAlpha (i < cur ? 0.42f : 0.22f));
                g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (p));
            }
        }
    }
    paintDisc (g, c, discR, angleFor (value), accent, hot);

    // modulated / macro value marker
    if (getEffectiveValue)
    {
        shownEffective = getEffectiveValue();
        if (std::abs (shownEffective - value) > 0.006f)
            cs::dotGlow (g, polar (c, arcR, angleFor (shownEffective)), 2.0f, cs::ice, 1.0f);
    }
}

Rectangle<float> RackControl::ledRowItem (int i) const
{
    const auto face = getFaceBounds();
    const int n = jmax (1, numSteps());
    const float h = jmin (16.0f, face.getHeight() / (float) n);
    const float top = face.getY() + (face.getHeight() - h * (float) n) * 0.5f;
    return { face.getX(), top + h * (float) i, face.getWidth(), h };
}

void RackControl::paintLedRow (Graphics& g)
{
    // vertical segmented pill: one capsule, the active segment lit in the accent colour
    const int n = numSteps();
    const int cur = stepIndex();
    const auto block = ledRowItem (0).getUnion (ledRowItem (n - 1));
    const float rad = jmin (8.0f, ledRowItem (0).getHeight() * 0.5f + 1.0f);
    g.setColour (cs::bg.withAlpha (0.9f));
    g.fillRoundedRectangle (block, rad);
    g.setColour (isMouseOver() ? pal.accent.withAlpha (0.35f) : cs::line);
    g.drawRoundedRectangle (block.reduced (0.5f), rad - 0.5f, 1.0f);

    for (int i = 0; i < n; ++i)
    {
        auto row = ledRowItem (i);
        const bool on = i == cur;
        if (on)
        {
            auto lit = row.reduced (2.0f, 1.5f);
            g.setColour (pal.accent.withAlpha (0.10f));
            g.fillRoundedRectangle (lit.expanded (1.0f), rad);
            g.setColour (pal.accent.withAlpha (0.22f));
            g.fillRoundedRectangle (lit, rad - 1.5f);
            g.setColour (pal.accent.withAlpha (0.65f));
            g.drawRoundedRectangle (lit.reduced (0.5f), rad - 2.0f, 1.0f);
        }
        else if (i > 0 && i - 1 != cur)
        {
            g.setColour (cs::line.withMultipliedAlpha (0.6f));
            g.fillRect (row.getX() + 7.0f, row.getY(), row.getWidth() - 14.0f, 0.7f);
        }
        const auto dotC = Point<float> (row.getX() + 9.0f, row.getCentreY());
        if (on) cs::dotGlow (g, dotC, 1.8f, pal.accent, 1.0f);
        else
        {
            g.setColour (cs::muted.withAlpha (0.45f));
            g.fillEllipse (Rectangle<float> (2.6f, 2.6f).withCentre (dotC));
        }
        const String t = spec != nullptr ? spec->choices[i].toUpperCase() : String();
        g.setFont (cs::monoFont (7.8f, on));
        g.setColour (on ? cs::text : cs::muted);
        g.drawFittedText (t, row.withTrimmedLeft (15.0f).withTrimmedRight (4.0f).toNearestInt(), Justification::centredLeft, 1, 0.6f);
    }
}

void RackControl::paintToggle (Graphics& g)
{
    const auto face = getFaceBounds();
    const bool on = value > 0.5f;
    auto b = Rectangle<float> (jmin (38.0f, face.getWidth()), 20.0f).withCentre (face.getCentre());
    const float rad = b.getHeight() * 0.5f;
    if (on)
    {
        ColourGradient halo (pal.accent.withAlpha (0.22f), b.getCentreX(), b.getCentreY(), pal.accent.withAlpha (0.0f),
                             b.getRight() + 10.0f, b.getCentreY(), true);
        g.setGradientFill (halo);
        g.fillRoundedRectangle (b.expanded (8.0f), rad + 8.0f);
    }
    g.setColour (cs::bg);
    g.fillRoundedRectangle (b, rad);
    if (on)
    {
        g.setColour (pal.accent.withAlpha (0.24f));
        g.fillRoundedRectangle (b, rad);
    }
    g.setColour (on ? pal.accent.withAlpha (0.8f) : (isMouseOver() ? pal.accent.withAlpha (0.4f) : cs::line.withMultipliedAlpha (1.4f)));
    g.drawRoundedRectangle (b.reduced (0.5f), rad - 0.5f, 1.0f);
    const auto knob = Rectangle<float> (b.getHeight() - 6.0f, b.getHeight() - 6.0f)
                          .withCentre ({ on ? b.getRight() - rad : b.getX() + rad, b.getCentreY() });
    if (on)
    {
        g.setColour (pal.accent.withAlpha (0.4f));
        g.fillEllipse (knob.expanded (2.0f));
    }
    g.setColour (on ? Colours::white : cs::muted.withAlpha (0.7f));
    g.fillEllipse (knob);
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

void RackControl::mouseMove (const MouseEvent& e)
{
    if (kind != Kind::Rotary && kind != Kind::Selector) return;
    const auto face = getFaceBounds();
    const float d = e.position.getDistanceFrom (face.getCentre());
    const bool ring = hitRing && d > face.getWidth() * 0.5f && hitRing (e.position - face.getCentre(), face.getWidth() * 0.5f) >= 0;
    if (ring != ringHover)
    {
        ringHover = ring;
        setMouseCursor (ring ? MouseCursor::DraggingHandCursor : MouseCursor::UpDownResizeCursor);
    }
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
    ringDrag = -1;

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
    // a click on a modulation ring drags that ring's depth
    if (hitRing && onRingDrag)
    {
        const auto face = getFaceBounds();
        const float d = e.position.getDistanceFrom (face.getCentre());
        if (d > face.getWidth() * 0.5f)
            ringDrag = hitRing (e.position - face.getCentre(), face.getWidth() * 0.5f);
        if (ringDrag >= 0)
        {
            if (onRingDragStart) onRingDragStart (ringDrag);
            return;
        }
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
    if (ringDrag >= 0)
    {
        const float sens = e.mods.isShiftDown() ? 1200.0f : 220.0f;
        if (onRingDrag) onRingDrag (ringDrag, (d.x - d.y) / sens);
        return;
    }
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
    if (ringDrag >= 0)
    {
        if (onRingDragEnd) onRingDragEnd (ringDrag);
        ringDrag = -1;
        repaint();
        return;
    }
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
            const float ro = s * 0.48f, ri = s * 0.35f;
            for (int i = 0; i < teeth * 4; ++i)
            {
                const float a = (float) i / (float) (teeth * 4) * MathConstants<float>::twoPi;
                const bool outer = (i % 4) == 1 || (i % 4) == 2;
                const auto pt = polar (cen, outer ? ro : ri, a);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            p.addEllipse (Rectangle<float> (s * 0.32f, s * 0.32f).withCentre (cen));
            p.setUsingNonZeroWinding (false);
            g.fillPath (p);
            break;
        }
        case Icon::Power:
        {
            Path arc;
            const float rr = s * 0.38f;
            arc.addCentredArc (cen.x, cen.y + s * 0.04f, rr, rr, 0.0f, 0.7f, MathConstants<float>::twoPi - 0.7f, true);
            g.strokePath (arc, PathStrokeType (s * 0.12f, PathStrokeType::curved, PathStrokeType::rounded));
            Path stem;
            stem.startNewSubPath (cen.x, cen.y - s * 0.46f);
            stem.lineTo (cen.x, cen.y - s * 0.02f);
            g.strokePath (stem, PathStrokeType (s * 0.12f, PathStrokeType::curved, PathStrokeType::rounded));
            break;
        }
        case Icon::Solo:
        {
            // headphones
            Path band;
            const float rr = s * 0.38f;
            band.addCentredArc (cen.x, cen.y + s * 0.06f, rr, rr, 0.0f, -MathConstants<float>::halfPi - 0.25f,
                                MathConstants<float>::halfPi + 0.25f, true);
            g.strokePath (band, PathStrokeType (s * 0.1f, PathStrokeType::curved, PathStrokeType::rounded));
            for (float dx : { -1.0f, 1.0f })
                g.fillRoundedRectangle (Rectangle<float> (s * 0.2f, s * 0.36f).withCentre ({ cen.x + dx * rr, cen.y + s * 0.22f }), s * 0.07f);
            break;
        }
        case Icon::Lock:
        {
            auto body = Rectangle<float> (s * 0.66f, s * 0.46f).withCentre (cen.translated (0.0f, s * 0.15f));
            Path shackle;
            const float sw = s * 0.21f;
            shackle.startNewSubPath (cen.x - sw, body.getY() + 0.5f);
            shackle.lineTo (cen.x - sw, body.getY() - s * 0.1f);
            shackle.addCentredArc (cen.x, body.getY() - s * 0.1f, sw, sw, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi);
            shackle.lineTo (cen.x + sw, body.getY() + 0.5f);
            g.strokePath (shackle, PathStrokeType (s * 0.1f, PathStrokeType::curved, PathStrokeType::rounded));
            g.fillRoundedRectangle (body, s * 0.08f);
            g.setColour (cs::bg.withAlpha (0.8f));
            g.fillEllipse (Rectangle<float> (s * 0.11f, s * 0.11f).withCentre (body.getCentre()));
            break;
        }
        case Icon::Remove:
        {
            const float d = s * 0.3f;
            Path x;
            x.startNewSubPath (cen.x - d, cen.y - d); x.lineTo (cen.x + d, cen.y + d);
            x.startNewSubPath (cen.x + d, cen.y - d); x.lineTo (cen.x - d, cen.y + d);
            g.strokePath (x, PathStrokeType (s * 0.11f, PathStrokeType::curved, PathStrokeType::rounded));
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
    const bool over = isMouseOver() && lastEnabled;
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = r.getCentre();
    const float rad = jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto circle = Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c);
    const Colour hoverCol = dangerHover ? theme::killRed : cs::ice;

    g.setColour (cs::bg);
    g.fillEllipse (circle);
    if (lit)
    {
        ColourGradient fill (colour.withAlpha (0.34f), c.x, c.y, colour.withAlpha (0.08f), c.x + rad, c.y, true);
        g.setGradientFill (fill);
        g.fillEllipse (circle);
    }
    else if (over)
    {
        g.setColour (hoverCol.withAlpha (0.10f));
        g.fillEllipse (circle);
    }
    if (down)
    {
        g.setColour (Colours::black.withAlpha (0.3f));
        g.fillEllipse (circle);
    }
    g.setColour (lit ? colour.withAlpha (0.85f) : (over ? hoverCol.withAlpha (0.6f) : cs::line.withMultipliedAlpha (1.3f)));
    g.drawEllipse (circle.reduced (0.5f), 1.0f);

    auto ir = circle.reduced (rad * 0.5f).translated (0.0f, down ? 0.5f : 0.0f);
    Colour ic = lit ? colour.interpolatedWith (Colours::white, 0.35f) : cs::text2.withAlpha (0.8f);
    if (! lastEnabled) ic = cs::muted.withAlpha (0.3f);
    else if (over && ! lit) ic = dangerHover ? theme::killRed.brighter (0.3f) : cs::text;
    if (lit)
    {
        Graphics::ScopedSaveState ss (g);
        g.setOpacity (0.4f);
        drawIcon (g, icon, ir.expanded (1.2f), colour);
    }
    drawIcon (g, icon, ir, ic);
}

SideButton::~SideButton()
{
    stopTimer();
    if (held && onHoldEnd) onHoldEnd(); // never leave a hold (e.g. an audition) running
}

void SideButton::mouseDown (const MouseEvent& e)
{
    if (isEnabledFn && ! isEnabledFn()) return;
    if (e.mods.isPopupMenu()) return;
    down = true;
    held = false;
    if (onHoldStart) startTimer (juce::jmax (1, holdMs));
    repaint();
}

void SideButton::timerCallback()
{
    stopTimer();
    if (! down || held) return;
    held = true;
    if (onHoldStart) onHoldStart();
    repaint();
}

void SideButton::mouseUp (const MouseEvent& e)
{
    stopTimer();
    if (! down) return;
    down = false;
    repaint();
    if (held)
    {
        held = false;
        if (onHoldEnd) onHoldEnd();
        return; // a hold is not a click
    }
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
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    const bool over = isMouseOver();
    g.setColour (cs::bg);
    g.fillRoundedRectangle (r, rad);
    ColourGradient inner (Colours::white.withAlpha (0.035f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getBottom(), false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (r, rad);
    g.setColour (over ? accent.withAlpha (0.55f) : cs::line.withMultipliedAlpha (1.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);

    cs::dotGlow (g, { r.getX() + rad, r.getCentreY() }, 1.9f, accent, 0.85f);

    auto content = r.withTrimmedLeft (rad + 8.0f).withTrimmedRight (6.0f);
    auto arrow = content.removeFromRight (12.0f);
    float reveal = 1.0f;
    if (scanStart >= 0.0) reveal = (float) jlimit (0.0, 1.0, (nowSec() - scanStart) / 0.35);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (content.withWidth (content.getWidth() * reveal).toNearestInt());
        g.setFont (cs::monoFont (9.6f));
        g.setColour (over ? cs::text : cs::text.withAlpha (0.9f));
        g.drawFittedText (text.toUpperCase(), content.toNearestInt(), Justification::centredLeft, 1, 0.7f);
    }
    if (reveal < 1.0f)
    {
        const float x = content.getX() + content.getWidth() * reveal;
        g.setColour (accent.withAlpha (0.8f));
        g.fillRect (x - 0.75f, r.getY() + 5.0f, 1.5f, r.getHeight() - 10.0f);
    }
    // chevron
    Path chev;
    const auto ac = arrow.getCentre();
    chev.startNewSubPath (ac.x - 3.2f, ac.y - 1.5f);
    chev.lineTo (ac.x, ac.y + 1.7f);
    chev.lineTo (ac.x + 3.2f, ac.y - 1.5f);
    g.setColour (over ? cs::ice : cs::text2.withAlpha (0.8f));
    g.strokePath (chev, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
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

int MsSwitch::segmentAt (Point<float> p) const
{
    const float w = (float) getWidth() / 3.0f;
    return jlimit (0, 2, (int) (p.x / jmax (1.0f, w)));
}

void MsSwitch::paint (Graphics& g)
{
    shown = getMode ? getMode() : 0;
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float rad = r.getHeight() * 0.5f;
    g.setColour (cs::bg);
    g.fillRoundedRectangle (r, rad);
    g.setColour (isMouseOver() ? cs::ice.withAlpha (0.4f) : cs::line.withMultipliedAlpha (1.2f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);

    static const char* names[] = { "ST", "MID", "SIDE" };
    const float sw = r.getWidth() / 3.0f;
    for (int i = 0; i < 3; ++i)
    {
        auto seg = Rectangle<float> (r.getX() + sw * (float) i, r.getY(), sw, r.getHeight());
        const bool on = i == shown;
        if (on)
        {
            auto lit = seg.reduced (2.0f, 2.0f);
            const float lr = lit.getHeight() * 0.5f;
            g.setColour (cs::iceDeep.withAlpha (0.25f));
            g.fillRoundedRectangle (lit.expanded (1.5f), lr + 1.5f);
            g.setColour (cs::iceDeep.withAlpha (0.55f));
            g.fillRoundedRectangle (lit, lr);
            g.setColour (cs::ice.withAlpha (0.7f));
            g.drawRoundedRectangle (lit.reduced (0.5f), lr - 0.5f, 0.9f);
        }
        else if (i == hover)
        {
            g.setColour (cs::ice.withAlpha (0.07f));
            g.fillRoundedRectangle (seg.reduced (2.0f), (seg.getHeight() - 4.0f) * 0.5f);
        }
        g.setFont (cs::monoFont (7.6f, on));
        g.setColour (on ? Colours::white : (i == hover ? cs::text : cs::muted));
        g.drawText (names[i], seg, Justification::centred, false);
    }
}

void MsSwitch::mouseMove (const MouseEvent& e)
{
    const int h = segmentAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void MsSwitch::mouseExit (const MouseEvent&)
{
    hover = -1;
    repaint();
}

void MsSwitch::mouseDown (const MouseEvent& e)
{
    if (onSelect) onSelect (segmentAt (e.position));
    else if (onClick) onClick();
}

} // namespace ek::ui
