#include "Hardware.h"
#include "../../engine/Preset.h"
#include "CardStyle.h"

namespace ek::ui::hw
{
using namespace juce;

namespace
{
inline Point<float> polar (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}

// ---- reduce-motion preference ---------------------------------------------------------------
File settingsFile() { return PresetBank::getUserPresetDir().getParentDirectory().getChildFile ("ui-settings.xml"); }

std::atomic<int>& motionFlag()
{
    static std::atomic<int> flag { -1 };   // -1 = not loaded yet
    return flag;
}
} // namespace

bool reduceMotion()
{
    auto& f = motionFlag();
    int v = f.load();
    if (v < 0)
    {
        v = 0;
        if (auto xml = parseXML (settingsFile())) v = xml->getBoolAttribute ("reduceMotion", false) ? 1 : 0;
        f.store (v);
    }
    return v == 1;
}

void setReduceMotion (bool on)
{
    motionFlag().store (on ? 1 : 0);
    XmlElement xml ("TRAPWIN_UI");
    if (auto old = parseXML (settingsFile())) xml = *old;
    xml.setAttribute ("reduceMotion", on);
    auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}

double animSeconds() { return Time::getMillisecondCounterHiRes() * 0.001; }


// ============================================================================================
// fonts / surfaces
Font mono (float h, bool bold) { return card::monoFont (h, bold); }
Font display (float h) { return card::displayFont (h); }

void drawGlow (Graphics& g, Rectangle<float> r, float corner, Colour c, float amount, float spread)
{
    if (amount <= 0.0f) return;
    const int steps = 7;
    for (int i = steps; i >= 1; --i)
    {
        const float k = (float) i / (float) steps;
        const float e = spread * k;
        g.setColour (c.withAlpha (0.055f * amount * (1.0f - k * 0.7f)));
        g.fillRoundedRectangle (r.expanded (e), corner + e);
    }
}

void drawCard (Graphics& g, Rectangle<float> r, float corner, float glow)
{
    if (glow > 0.0f) drawGlow (g, r, corner, col::iceDeep, glow, 14.0f);
    Path shape;
    shape.addRoundedRectangle (r, corner);
    ColourGradient body (col::card2, r.getX(), r.getY(), Colour (0xff080c14), r.getX(), r.getBottom(), false);
    body.addColour (0.4, col::card);
    g.setGradientFill (body);
    g.fillPath (shape);
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shape);
        ColourGradient haze (col::iceDeep.withAlpha (0.07f), r.getRight(), r.getY(), col::iceDeep.withAlpha (0.0f),
                             r.getRight() - r.getWidth() * 0.9f, r.getY() + r.getHeight() * 0.35f, true);
        g.setGradientFill (haze);
        g.fillRect (r);
        ColourGradient sheen (Colours::white.withAlpha (0.035f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + 46.0f, false);
        g.setGradientFill (sheen);
        g.fillRect (r);
    }
    g.setColour (col::line);
    g.drawRoundedRectangle (r.reduced (0.5f), corner - 0.5f, 1.0f);
    ColourGradient top (col::ice.withAlpha (0.0f), r.getX() + corner, 0.0f, col::ice.withAlpha (0.0f), r.getRight() - corner, 0.0f, false);
    top.addColour (0.3, col::ice.withAlpha (0.16f));
    top.addColour (0.7, col::ice.withAlpha (0.10f));
    g.setGradientFill (top);
    g.fillRect (r.getX() + corner, r.getY() + 0.5f, r.getWidth() - 2.0f * corner, 0.8f);
}

Rectangle<float> drawRackPanel (Graphics& g, Rectangle<float> r, Colour, float, int)
{
    drawCard (g, r.reduced (1.0f), jmin (20.0f, r.getHeight() * 0.25f));
    return r.reduced (14.0f, 4.0f);
}

void drawSectionHeader (Graphics& g, Rectangle<float> r, const String& label)
{
    const auto f = mono (jmin (10.0f, r.getHeight() * 0.8f), true);
    g.setFont (f);
    g.setColour (col::printDim);
    g.drawText (label, r, Justification::centredLeft, false);
    const float x = r.getX() + GlyphArrangement::getStringWidth (f, label) + 8.0f;
    if (x < r.getRight() - 4.0f)
    {
        ColourGradient lineG (col::ice.withAlpha (0.22f), x, 0.0f, col::ice.withAlpha (0.0f), r.getRight(), 0.0f, false);
        g.setGradientFill (lineG);
        g.fillRect (Rectangle<float> (x, r.getCentreY() - 0.5f, r.getRight() - x, 1.0f));
    }
}

void drawPrint (Graphics& g, const String& text, Rectangle<float> r, Justification j, float size, Colour c)
{
    g.setFont (mono (size));
    g.setColour (c);
    g.drawText (text, r, j, false);
}

// ============================================================================================
// pills
void drawPill (Graphics& g, Rectangle<float> r, const String& text, Colour accent, bool lit, bool pressed, bool hover,
               PillStyle style, bool dot, float fontSize)
{
    const float rad = r.getHeight() * 0.5f;
    if (pressed) r = r.reduced (0.6f);
    if (style == PillStyle::Chrome)
    {
        drawGlow (g, r, rad, col::iceDeep, hover || lit ? 1.0f : 0.5f, 8.0f);
        ColourGradient chrome (Colour (0xfff4f6fb), 0.0f, r.getY(), Colour (0xffe6eaf2), 0.0f, r.getBottom(), false);
        chrome.addColour (0.48, Colour (0xffc9d0dd));
        chrome.addColour (0.52, Colour (0xff8e98ab));
        g.setGradientFill (chrome);
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::white.withAlpha (hover ? 0.6f : 0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
        if (text.isNotEmpty())
        {
            g.setFont (mono (fontSize > 0.0f ? fontSize : jmin (11.0f, r.getHeight() * 0.42f), true));
            g.setColour (Colour (0xff05070b));
            g.drawFittedText (text, r.reduced (rad * 0.6f, 0.0f).toNearestInt(), Justification::centred, 1, 0.75f);
        }
        return;
    }

    if (lit) drawGlow (g, r, rad, accent, 1.0f, 9.0f);
    else if (hover) drawGlow (g, r, rad, col::iceDeep, 0.6f, 7.0f);

    g.setColour (col::bg);
    g.fillRoundedRectangle (r, rad);
    if (lit)
    {
        ColourGradient fill (accent.withAlpha (0.30f), 0.0f, r.getY(), accent.withAlpha (0.12f), 0.0f, r.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, rad);
    }
    else
    {
        ColourGradient inner (Colours::white.withAlpha (hover ? 0.06f : 0.03f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getBottom(), false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (r, rad);
    }
    g.setColour (lit ? accent.withAlpha (0.9f) : (hover ? col::ice.withAlpha (0.45f) : col::line));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);

    auto tr = r.reduced (jmin (rad * 0.7f, 9.0f), 0.0f);
    if (dot)
    {
        const float dr = jmax (2.0f, r.getHeight() * 0.1f);
        card::dotGlow (g, { tr.getX() + dr + 1.0f, r.getCentreY() }, dr, accent, lit ? 1.0f : 0.0f);
        tr.removeFromLeft (dr * 2.0f + 6.0f);
    }
    if (text.isNotEmpty())
    {
        const auto f = mono (fontSize > 0.0f ? fontSize : jmin (10.5f, r.getHeight() * 0.42f), true);
        const Colour ink = lit ? accent.interpolatedWith (Colours::white, 0.55f) : (hover ? col::print : col::text2);
        if (lit) card::glowText (g, text, tr, f, ink, dot ? Justification::centredLeft : Justification::centred, 0.6f);
        else
        {
            g.setFont (f);
            g.setColour (ink);
            g.drawFittedText (text, tr.toNearestInt(), dot ? Justification::centredLeft : Justification::centred, 1, 0.7f);
        }
    }
}

// ============================================================================================
// knob
void drawFlatKnob (Graphics& g, Point<float> c, float r, float v, Colour accent, float glow, float base, bool hot)
{
    constexpr float a0 = theme::knobStart, a1 = theme::knobEnd;
    v = jlimit (0.0f, 1.0f, v);
    const float av = a0 + (a1 - a0) * v;
    const float ar = r * 1.24f;
    Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a1, true);
    g.setColour (col::ice.withAlpha (0.10f));
    g.strokePath (track, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    if (v > 0.001f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, av, true);
        card::strokeGlow (g, arc, accent, 2.2f, 0.9f * jmax (0.4f, glow));
    }
    if (hot)
    {
        g.setColour (accent.withAlpha (0.10f));
        g.fillEllipse (Rectangle<float> (r * 2.9f, r * 2.9f).withCentre (c));
    }
    ColourGradient disc (Colour (0xff141d2d), c.x, c.y - r, Colour (0xff070b12), c.x, c.y + r, false);
    g.setGradientFill (disc);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (hot ? col::ice.withAlpha (0.4f) : col::line);
    g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c).reduced (0.5f), 1.0f);
    // soft top highlight on the disc
    g.setColour (Colours::white.withAlpha (0.04f));
    g.fillEllipse (Rectangle<float> (r * 1.3f, r * 0.7f).withCentre (c.translated (0.0f, -r * 0.45f)));
    if (base >= 0.0f)
    {
        const float ab = a0 + (a1 - a0) * jlimit (0.0f, 1.0f, base);
        const auto p0 = polar (c, ar - 4.0f, ab), p1 = polar (c, ar + 3.0f, ab);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.drawLine ({ p0, p1 }, 1.6f);
    }
    const auto pd = polar (c, r * 0.64f, av);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (r * 0.38f, r * 0.38f).withCentre (pd));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (r * 0.2f, r * 0.2f).withCentre (pd));
}

// ============================================================================================
// icons
namespace icon
{
static Rectangle<float> sq (Rectangle<float> r, float k = 0.62f)
{
    const float s = jmin (r.getWidth(), r.getHeight()) * k;
    return Rectangle<float> (s, s).withCentre (r.getCentre());
}
static float sw (Rectangle<float> b) { return jmax (1.3f, b.getWidth() * 0.12f); }

void prev (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.5f);
    Path p;
    p.addTriangle (b.getRight(), b.getY(), b.getRight(), b.getBottom(), b.getX(), b.getCentreY());
    g.setColour (c);
    g.fillPath (p);
}

void next (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.5f);
    Path p;
    p.addTriangle (b.getX(), b.getY(), b.getX(), b.getBottom(), b.getRight(), b.getCentreY());
    g.setColour (c);
    g.fillPath (p);
}

void save (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r);
    const float t = sw (b) * 0.8f;
    Path body;
    body.startNewSubPath (b.getX(), b.getY());
    body.lineTo (b.getRight() - b.getWidth() * 0.22f, b.getY());
    body.lineTo (b.getRight(), b.getY() + b.getHeight() * 0.22f);
    body.lineTo (b.getRight(), b.getBottom());
    body.lineTo (b.getX(), b.getBottom());
    body.closeSubPath();
    g.setColour (c);
    g.strokePath (body, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded));
    // shutter + label
    g.fillRect (Rectangle<float> (b.getWidth() * 0.42f, b.getHeight() * 0.3f).withPosition (b.getX() + b.getWidth() * 0.22f, b.getY()));
    g.drawRect (Rectangle<float> (b.getWidth() * 0.6f, b.getHeight() * 0.32f).withPosition (b.getX() + b.getWidth() * 0.2f, b.getBottom() - b.getHeight() * 0.42f), t * 0.8f);
}

void revert (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r);
    const float t = sw (b);
    const auto cc = b.getCentre();
    const float rad = b.getWidth() * 0.42f;
    Path arc;
    arc.addCentredArc (cc.x, cc.y, rad, rad, 0.0f, -2.2f, 2.0f, true);
    g.setColour (c);
    g.strokePath (arc, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded));
    // arrow head at the start of the arc (counter-clockwise arrow)
    const auto tip = polar (cc, rad, -2.2f);
    Path head;
    const float hs = b.getWidth() * 0.3f;
    head.addTriangle (tip.x - hs * 0.55f, tip.y - hs * 0.55f, tip.x + hs * 0.55f, tip.y - hs * 0.15f, tip.x - hs * 0.05f, tip.y + hs * 0.55f);
    g.fillPath (head);
}

void star (Graphics& g, Rectangle<float> r, Colour c, bool filled)
{
    // TrapWin four-point sparkle; unfilled = thin outline
    auto b = sq (r, 0.78f);
    if (filled) { card::paintSparkle (g, b.getCentre(), b.getWidth() * 0.5f, c, 0.8f); return; }
    const auto cc = b.getCentre();
    const float rr = b.getWidth() * 0.5f, k = 0.22f;
    Path p;
    p.startNewSubPath (cc.x, cc.y - rr);
    p.quadraticTo (cc.x + rr * k * 0.5f, cc.y - rr * k * 0.5f, cc.x + rr, cc.y);
    p.quadraticTo (cc.x + rr * k * 0.5f, cc.y + rr * k * 0.5f, cc.x, cc.y + rr);
    p.quadraticTo (cc.x - rr * k * 0.5f, cc.y + rr * k * 0.5f, cc.x - rr, cc.y);
    p.quadraticTo (cc.x - rr * k * 0.5f, cc.y - rr * k * 0.5f, cc.x, cc.y - rr);
    p.closeSubPath();
    g.setColour (c);
    g.strokePath (p, PathStrokeType (1.1f, PathStrokeType::curved, PathStrokeType::rounded));
}

void library (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.66f);
    const float t = sw (b) * 0.75f;
    g.setColour (c);
    g.drawRoundedRectangle (b, b.getWidth() * 0.16f, t);
    g.fillRect (Rectangle<float> (t * 1.1f, b.getHeight()).withPosition (b.getX() + b.getWidth() * 0.36f, b.getY()));
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle (Rectangle<float> (b.getWidth() * 0.18f, t).withPosition (b.getX() + b.getWidth() * 0.1f,
                                b.getY() + b.getHeight() * (0.24f + 0.2f * (float) i)), t * 0.5f);
}

void chevronDown (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.42f);
    Path p;
    p.startNewSubPath (b.getX(), b.getY() + b.getHeight() * 0.3f);
    p.lineTo (b.getCentreX(), b.getY() + b.getHeight() * 0.75f);
    p.lineTo (b.getRight(), b.getY() + b.getHeight() * 0.3f);
    g.setColour (c);
    g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

static void curvedArrow (Graphics& g, Rectangle<float> r, Colour c, bool flip)
{
    auto b = sq (r);
    const float t = sw (b);
    Path p;
    p.startNewSubPath (b.getX() + b.getWidth() * 0.18f, b.getY() + b.getHeight() * 0.4f);
    p.lineTo (b.getX() + b.getWidth() * 0.62f, b.getY() + b.getHeight() * 0.4f);
    p.cubicTo (b.getRight() + b.getWidth() * 0.05f, b.getY() + b.getHeight() * 0.4f,
               b.getRight() + b.getWidth() * 0.05f, b.getBottom() - b.getHeight() * 0.05f,
               b.getX() + b.getWidth() * 0.55f, b.getBottom() - b.getHeight() * 0.05f);
    Path head;
    const float hy = b.getY() + b.getHeight() * 0.4f, hx = b.getX();
    head.addTriangle (hx, hy, hx + b.getWidth() * 0.32f, hy - b.getHeight() * 0.25f, hx + b.getWidth() * 0.32f, hy + b.getHeight() * 0.25f);
    const auto t2 = flip ? AffineTransform::scale (-1.0f, 1.0f, b.getCentreX(), b.getCentreY()) : AffineTransform();
    g.setColour (c);
    g.strokePath (p, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded), t2);
    g.fillPath (head, t2);
}

void undo (Graphics& g, Rectangle<float> r, Colour c) { curvedArrow (g, r, c, false); }
void redo (Graphics& g, Rectangle<float> r, Colour c) { curvedArrow (g, r, c, true); }

void cog (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.68f);
    const auto cc = b.getCentre();
    const float ro = b.getWidth() * 0.5f, ri = ro * 0.74f;
    Path p;
    const int teeth = 8;
    for (int i = 0; i < teeth * 4; ++i)
    {
        const float a = (float) i / (float) (teeth * 4) * MathConstants<float>::twoPi;
        const float rr = (i % 4 == 0 || i % 4 == 1) ? ro : ri;
        const auto pt = polar (cc, rr, a - 0.1f);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    p.addEllipse (Rectangle<float> (ro * 0.7f, ro * 0.7f).withCentre (cc));
    p.setUsingNonZeroWinding (false);
    g.setColour (c);
    g.fillPath (p);
}

void info (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.64f);
    g.setColour (c);
    g.drawEllipse (b, sw (b) * 0.8f);
    const float t = b.getWidth() * 0.13f;
    g.fillEllipse (Rectangle<float> (t * 1.2f, t * 1.2f).withCentre ({ b.getCentreX(), b.getY() + b.getHeight() * 0.28f }));
    g.fillRoundedRectangle (Rectangle<float> (t, b.getHeight() * 0.36f).withCentre ({ b.getCentreX(), b.getY() + b.getHeight() * 0.62f }), t * 0.3f);
}

void power (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.6f);
    const float t = sw (b);
    const auto cc = b.getCentre().translated (0.0f, b.getHeight() * 0.05f);
    Path arc;
    arc.addCentredArc (cc.x, cc.y, b.getWidth() * 0.42f, b.getWidth() * 0.42f, 0.0f, 0.6f, MathConstants<float>::twoPi - 0.6f, true);
    g.setColour (c);
    g.strokePath (arc, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded));
    g.drawLine (cc.x, b.getY() - b.getHeight() * 0.05f, cc.x, cc.y, t);
}

void lock (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.6f);
    const float t = sw (b);
    auto body = b.withTrimmedTop (b.getHeight() * 0.45f);
    g.setColour (c);
    g.fillRoundedRectangle (body, t * 0.6f);
    Path shackle;
    shackle.addCentredArc (b.getCentreX(), body.getY(), b.getWidth() * 0.28f, b.getHeight() * 0.32f, 0.0f,
                           -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
    g.strokePath (shackle, PathStrokeType (t));
}

void letter (Graphics& g, Rectangle<float> r, Colour c, const String& text)
{
    g.setColour (c);
    g.setFont (mono (r.getHeight() * 0.5f, true));
    g.drawText (text, r, Justification::centred, false);
}
} // namespace icon

// ============================================================================================
void BackgroundCache::draw (Graphics& g, Rectangle<int> area, const std::function<void (Graphics&)>& painter)
{
    const float scale = jlimit (1.0f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! image.isValid() || area != cachedArea || std::abs (scale - cachedScale) > 0.01f)
    {
        cachedArea = area;
        cachedScale = scale;
        image = Image (Image::ARGB, jmax (1, (int) std::ceil ((float) area.getWidth() * scale)),
                       jmax (1, (int) std::ceil ((float) area.getHeight() * scale)), true);
        Graphics ig (image);
        ig.addTransform (AffineTransform::translation ((float) -area.getX(), (float) -area.getY()).scaled (scale));
        painter (ig);
    }
    g.drawImageTransformed (image, AffineTransform::scale (1.0f / cachedScale).translated ((float) area.getX(), (float) area.getY()));
}

// ============================================================================================
HwButton::HwButton (Kind k, String l, Colour c) : kind (k), label (std::move (l)), led (c)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setPaintingIsUnclipped (true);
}

void HwButton::repaintGlow()
{
    if (auto* p = getParentComponent()) p->repaint (getBounds().expanded (glowMargin));
    else repaint();
}

bool HwButton::litNow() const
{
    return (isLit && isLit()) || Time::getMillisecondCounter() < flashUntil;
}

float HwButton::pulse() const
{
    if (! blinkWhenLit || ! (isLit && isLit()) || reduceMotion()) return 1.0f;
    // smooth ~1.1 s pulse, quantised so tick() only repaints when it visibly changes
    const float p = 0.5f + 0.5f * std::sin ((float) animSeconds() * 5.7f);
    return std::round ((0.35f + 0.65f * p) * 20.0f) / 20.0f;
}

void HwButton::tick()
{
    const bool en = ! isEnabledFn || isEnabledFn();
    if (litNow() != lastLit || en != lastEnabled || std::abs (pulse() - lastPulse) > 0.001f) repaintGlow();
}

void HwButton::paint (Graphics& g)
{
    lastLit = litNow();
    lastEnabled = ! isEnabledFn || isEnabledFn();
    lastPulse = pulse();
    const bool hover = isMouseOver() && lastEnabled;
    // leave room for the glow around the pill
    auto r = getLocalBounds().toFloat();
    if (! lastEnabled) g.beginTransparencyLayer (0.35f);

    switch (kind)
    {
        case Kind::Led:
        case Kind::Text:
        case Kind::Round:
        {
            if (lastLit && lastPulse < 1.0f) g.beginTransparencyLayer (0.45f + 0.55f * lastPulse);
            drawPill (g, r, label, led, lastLit, down, hover, PillStyle::Outline, kind == Kind::Led, fontSize);
            if (lastLit && lastPulse < 1.0f) g.endTransparencyLayer();
            break;
        }
        case Kind::Chrome:
            drawPill (g, r, label, led, lastLit, down, hover, PillStyle::Chrome, false, fontSize);
            break;
        case Kind::Icon:
        {
            drawPill (g, r, {}, led, lastLit, down, hover);
            const Colour ink = lastLit ? led.interpolatedWith (Colours::white, 0.45f) : (hover ? col::print : col::text2);
            if (drawIcon) drawIcon (g, r.reduced (r.getHeight() * 0.2f), ink);
            break;
        }
    }
    if (! lastEnabled) g.endTransparencyLayer();
}

void HwButton::mouseDown (const MouseEvent& e)
{
    if (isEnabledFn && ! isEnabledFn()) return;
    if (e.mods.isPopupMenu() && ! onPress) { if (onClick) onClick(); return; }
    down = true;
    repaintGlow();
    if (onPress) onPress (true);
}

void HwButton::mouseUp (const MouseEvent& e)
{
    if (! down) return;
    down = false;
    repaintGlow();
    if (onPress) onPress (false);
    if (onClick && getLocalBounds().contains (e.getPosition())) onClick();
}

// ============================================================================================
void HwKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float s = jmin (b.getWidth(), b.getHeight());
    const float r = s * 0.5f * radiusFactor;
    const float v = getDisplayValue ? getDisplayValue() : getNormValue();
    if (ringGlow > 0.0f && showArc)
    {
        // warning halo (RECYCLE)
        ColourGradient halo (ring.withAlpha (0.22f * ringGlow), b.getCentreX(), b.getCentreY(), ring.withAlpha (0.0f),
                             b.getCentreX() + r * 1.9f, b.getCentreY(), true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 3.8f, r * 3.8f).withCentre (b.getCentre()));
    }
    drawFlatKnob (g, b.getCentre(), r, showArc ? v : 0.0f, ring, 1.0f + ringGlow, -1.0f, isMouseButtonDown() || isMouseOver());
}
} // namespace ek::ui::hw
