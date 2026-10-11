#include "PerformerDraw.h"
#include "../Theme.h"
#include <cmath>

namespace ek::ui::performer
{
using namespace juce;

static Point<float> polar (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}

Colour macroColour (int m) noexcept
{
    static const Colour c[] = { Colour (0xffa45bff),   // VILLAIN ARC - violet
                                Colour (0xffff3346),   // CRASH OUT   - red
                                Colour (0xff3fe0ff),   // AURA        - cyan
                                Colour (0xff45f28a),   // DRIP        - green
                                Colour (0xffff8a1f) }; // KNOCK       - orange
    return c[jlimit (0, 4, m)];
}

// ============================================================================
void drawAnodisedPlate (Graphics& g, Rectangle<float> r)
{
    // base: dark blue anodised aluminium, a touch lighter at the top
    ColourGradient base (plateBlue.brighter (0.22f), r.getX(), r.getY(), plateBlue.darker (0.35f), r.getX(), r.getBottom(), false);
    base.addColour (0.45, plateBlue);
    g.setGradientFill (base);
    g.fillRect (r);

    // fine horizontal grain (deterministic, so snapshots are stable)
    Random rnd (0x7e11);
    for (float y = r.getY() + 1.0f; y < r.getBottom() - 1.0f; y += 1.0f)
    {
        const float a = 0.012f + rnd.nextFloat() * 0.03f;
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (a));
        const float x0 = r.getX() + rnd.nextFloat() * 40.0f;
        g.drawHorizontalLine ((int) y, x0, r.getRight() - rnd.nextFloat() * 40.0f);
    }

    // soft sheen across the middle
    ColourGradient sheen (Colours::transparentWhite, r.getX(), r.getY(), Colours::transparentWhite, r.getRight(), r.getY(), false);
    sheen.addColour (0.35, Colours::white.withAlpha (0.035f));
    sheen.addColour (0.55, Colours::white.withAlpha (0.06f));
    sheen.addColour (0.75, Colours::white.withAlpha (0.02f));
    g.setGradientFill (sheen);
    g.fillRect (r);

    // bevel: light top edge, dark bottom edge
    g.setColour (Colours::white.withAlpha (0.16f));
    g.drawHorizontalLine ((int) r.getY(), r.getX(), r.getRight());
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX(), r.getRight());
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX(), r.getRight());

    // ice-blue hairline inset (the unit's signature line)
    const auto line = r.reduced (5.0f, 4.0f);
    g.setColour (iceLine.withAlpha (0.10f));
    g.drawRoundedRectangle (line.expanded (1.0f), 3.0f, 2.0f);
    g.setColour (iceLine.withAlpha (0.38f));
    g.drawRoundedRectangle (line, 3.0f, 0.8f);
}

void drawRackEar (Graphics& g, Rectangle<float> r, bool leftSide)
{
    ColourGradient metal (Colour (0xff1a2232), r.getX(), r.getY(), Colour (0xff0a0e16), r.getRight(), r.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRect (r);
    // fold line towards the faceplate
    const float fx = leftSide ? r.getRight() - 1.0f : r.getX();
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawVerticalLine ((int) fx, r.getY(), r.getBottom());
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawVerticalLine ((int) (leftSide ? fx - 1.0f : fx + 1.0f), r.getY(), r.getBottom());
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) r.getY(), r.getX(), r.getRight());

    // two oval mounting slots
    const float w = r.getWidth() * 0.46f, h = jmin (14.0f, r.getHeight() * 0.16f);
    for (float fy : { 0.2f, 0.8f })
    {
        auto slot = Rectangle<float> (w, h).withCentre ({ r.getCentreX(), r.getY() + r.getHeight() * fy });
        g.setColour (Colour (0xff020305));
        g.fillRoundedRectangle (slot, h * 0.5f);
        g.setColour (Colours::white.withAlpha (0.14f));
        g.drawLine (slot.getX() + h * 0.4f, slot.getBottom() + 0.6f, slot.getRight() - h * 0.4f, slot.getBottom() + 0.6f, 0.8f);
    }
}

void drawScrew (Graphics& g, Point<float> c, float rad, float rotation)
{
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (rad * 2.4f, rad * 2.4f).withCentre (c.translated (0.3f, 0.8f)));
    ColourGradient head (Colour (0xffeef2f8), c.x - rad * 0.6f, c.y - rad * 0.7f, Colour (0xff5b6272), c.x + rad * 0.7f, c.y + rad, true);
    head.addColour (0.55, Colour (0xffaab2c0));
    g.setGradientFill (head);
    g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), 0.7f);
    // cross slot
    for (float a : { rotation, rotation + MathConstants<float>::halfPi })
    {
        const auto d = Point<float> (std::cos (a), std::sin (a)) * (rad * 0.7f);
        g.setColour (Colour (0xff20242c));
        g.drawLine ({ c - d, c + d }, rad * 0.32f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawLine ({ c - d + Point<float> (0.0f, rad * 0.18f), c + d + Point<float> (0.0f, rad * 0.18f) }, rad * 0.08f);
    }
}

void drawEngravedDivider (Graphics& g, float x, float y0, float y1)
{
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawLine (x, y0, x, y1, 1.0f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawLine (x + 1.0f, y0, x + 1.0f, y1, 1.0f);
}

void drawSilkscreen (Graphics& g, const String& text, Rectangle<float> r, Colour c, float height, Justification j)
{
    g.setFont (theme::monoLabel (height));
    g.setColour (c);
    g.drawText (text, r, j, false);
}

// ============================================================================
void drawKillButton (Graphics& g, Rectangle<float> r, bool pressed, bool hover, float flash)
{
    const Colour red = theme::killRed;
    const float corner = 6.0f;

    // bezel: recessed black socket with a chrome edge
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), corner + 2.0f);
    g.setColour (Colour (0xff050608));
    g.fillRoundedRectangle (r, corner + 2.0f);
    ColourGradient edge (Colour (0xffb9c3d6).withAlpha (0.65f), r.getX(), r.getY(), Colour (0xff0b0f16), r.getX(), r.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.6f), corner + 2.0f, 1.2f);

    auto cap = r.reduced (5.0f);
    if (pressed) cap = cap.translated (0.0f, 1.2f).reduced (0.8f);

    // backlight bleeding out around the cap
    const float glow = jlimit (0.0f, 1.0f, 0.55f + (hover ? 0.12f : 0.0f) + flash * 0.45f - (pressed ? 0.1f : 0.0f));
    g.setColour (red.withAlpha (0.25f * glow));
    g.fillRoundedRectangle (cap.expanded (3.0f), corner + 2.0f);

    // translucent red cap lit from behind: hot centre, darker edges
    ColourGradient body (red.brighter (0.35f + 0.6f * flash).withMultipliedSaturation (0.92f), cap.getCentreX(), cap.getCentreY(),
                         red.darker (0.9f - 0.3f * glow), cap.getRight(), cap.getBottom(), true);
    body.addColour (0.55, red.interpolatedWith (red.darker (0.5f), 1.0f - glow));
    g.setGradientFill (body);
    g.fillRoundedRectangle (cap, corner);

    // fresnel-like fine lines in the cap
    g.setColour (Colours::black.withAlpha (0.07f));
    for (float y = cap.getY() + 3.0f; y < cap.getBottom() - 2.0f; y += 3.0f)
        g.drawHorizontalLine ((int) y, cap.getX() + 3.0f, cap.getRight() - 3.0f);

    // gloss on the upper half
    ColourGradient gloss (Colours::white.withAlpha (pressed ? 0.12f : 0.28f), cap.getX(), cap.getY(),
                          Colours::transparentWhite, cap.getX(), cap.getCentreY(), false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (cap.withHeight (cap.getHeight() * 0.48f).reduced (3.0f, 1.5f), corner - 1.0f);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (cap.reduced (1.0f), corner - 0.5f, 0.9f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (cap, corner, 1.0f);

    // legend: dark engraved "KILL" with a hot halo behind
    const auto textR = cap.reduced (6.0f, 2.0f);
    const auto f = theme::stencilFont (cap.getHeight() * 0.62f).withExtraKerningFactor (0.08f);
    g.setFont (f);
    g.setColour (Colour (0xffffd6c8).withAlpha (0.25f + 0.35f * flash));
    g.drawText ("KILL", textR.translated (0.0f, 1.0f), Justification::centred, false);
    g.setColour (Colour (0xff2a0006).withAlpha (0.88f));
    g.drawText ("KILL", textR, Justification::centred, false);
}

void drawLedKey (Graphics& g, Rectangle<float> r, const String& text, Colour led, float lit, bool pressed, bool hover)
{
    const float corner = 3.5f;
    g.setColour (Colour (0xff04060a));
    g.fillRoundedRectangle (r, corner + 1.0f);
    auto face = r.reduced (1.5f);
    if (pressed) face = face.translated (0.0f, 1.0f);
    ColourGradient body (Colour (pressed ? 0xff121a28 : (hover ? 0xff26324a : 0xff1e2638)), face.getX(), face.getY(),
                         Colour (0xff0a0e17), face.getX(), face.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (face, corner);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawHorizontalLine ((int) face.getY() + 1, face.getX() + 3.0f, face.getRight() - 3.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (face, corner, 0.8f);

    // LED window on the left
    auto ledR = Rectangle<float> (4.0f, face.getHeight() - 9.0f).withCentre ({ face.getX() + 8.0f, face.getCentreY() });
    g.setColour (Colour (0xff020304));
    g.fillRoundedRectangle (ledR.expanded (1.0f), 1.5f);
    if (lit > 0.01f)
    {
        g.setColour (led.withAlpha (0.35f * lit));
        g.fillRoundedRectangle (ledR.expanded (3.0f), 3.0f);
    }
    g.setColour (led.darker (2.4f).interpolatedWith (led.brighter (0.3f), lit));
    g.fillRoundedRectangle (ledR, 1.5f);

    g.setFont (theme::monoLabel (jmin (11.0f, face.getHeight() * 0.5f)).boldened());
    g.setColour (theme::silver.withAlpha (0.65f + 0.35f * lit));
    g.drawText (text, face.withTrimmedLeft (14.0f).withTrimmedRight (3.0f), Justification::centred, false);
}

void drawMacroKnob (Graphics& g, Rectangle<float> b, Colour ring, float baseValue, float shownValue, bool active)
{
    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float ringR = R * 0.86f;
    const float a0 = theme::knobStart, a1 = theme::knobEnd;
    const float aVal = a0 + jlimit (0.0f, 1.0f, shownValue) * (a1 - a0);
    const float aBase = a0 + jlimit (0.0f, 1.0f, baseValue) * (a1 - a0);

    // recessed ring channel
    g.setColour (Colour (0xff04070d));
    g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawEllipse (Rectangle<float> (R * 2.0f - 1.0f, R * 2.0f - 1.0f).withCentre (c), 0.8f);

    // dim track
    Path track;
    track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, a0, a1, true);
    g.setColour (ring.darker (2.2f).withAlpha (0.9f));
    g.strokePath (track, PathStrokeType (R * 0.11f, PathStrokeType::curved, PathStrokeType::butt));

    // lit arc (with glow) up to the modulated value
    if (aVal > a0 + 0.01f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, a0, aVal, true);
        g.setColour (ring.withAlpha (0.22f));
        g.strokePath (arc, PathStrokeType (R * 0.30f, PathStrokeType::curved, PathStrokeType::butt));
        g.setColour (ring.withAlpha (0.95f));
        g.strokePath (arc, PathStrokeType (R * 0.11f, PathStrokeType::curved, PathStrokeType::butt));
        g.setColour (Colours::white.withAlpha (0.45f));
        g.strokePath (arc, PathStrokeType (R * 0.03f, PathStrokeType::curved, PathStrokeType::butt));
        // bright head at the end of the arc
        const auto head = polar (c, ringR, aVal);
        g.setColour (ring.brighter (0.6f));
        g.fillEllipse (Rectangle<float> (R * 0.15f, R * 0.15f).withCentre (head));
    }

    // tick at the knob's own position (only visible when modulation moves the arc away from it)
    if (std::abs (aBase - aVal) > 0.04f)
    {
        g.setColour (Colours::white.withAlpha (0.85f));
        g.drawLine ({ polar (c, ringR - R * 0.1f, aBase), polar (c, ringR + R * 0.1f, aBase) }, 1.6f);
    }

    // knob body: dark knurled skirt + satin cap
    const float kr = R * 0.66f;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (kr * 2.2f, kr * 2.2f).withCentre (c.translated (0.0f, kr * 0.12f)));
    ColourGradient skirt (Colour (0xff3a4152), c.x - kr, c.y - kr, Colour (0xff07090d), c.x + kr, c.y + kr, false);
    g.setGradientFill (skirt);
    g.fillEllipse (Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.45f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = (float) i / 36.0f * MathConstants<float>::twoPi + aVal;
        g.drawLine ({ polar (c, kr * 0.84f, a), polar (c, kr * 0.99f, a) }, 0.9f);
    }
    ColourGradient cap (Colour (0xff4a5163), c.x - kr * 0.4f, c.y - kr * 0.6f, Colour (0xff0d1017), c.x + kr * 0.4f, c.y + kr * 0.7f, true);
    g.setGradientFill (cap);
    g.fillEllipse (Rectangle<float> (kr * 1.62f, kr * 1.62f).withCentre (c));
    Path rim;
    rim.addCentredArc (c.x, c.y, kr * 0.8f, kr * 0.8f, 0.0f, -2.0f, -0.3f, true);
    g.setColour (Colours::white.withAlpha (active ? 0.5f : 0.35f));
    g.strokePath (rim, PathStrokeType (kr * 0.06f, PathStrokeType::curved, PathStrokeType::rounded));
    // coloured centre dot
    g.setColour (ring.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (kr * 0.22f, kr * 0.22f).withCentre (c));

    // pointer line on the cap
    const auto p0 = polar (c, kr * 0.28f, aVal), p1 = polar (c, kr * 0.78f, aVal);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawLine ({ p0.translated (0.0f, 0.8f), p1.translated (0.0f, 0.8f) }, kr * 0.13f);
    g.setColour (Colour (0xfff1f4fa));
    g.drawLine ({ p0, p1 }, kr * 0.1f);
}

// ============================================================================
void drawVuMeter (Graphics& g, Rectangle<float> r, float vu, const String& name, float backlight)
{
    // bezel
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 5.0f);
    ColourGradient bezel (Colour (0xff2a3244), r.getX(), r.getY(), Colour (0xff07090e), r.getX(), r.getBottom(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 0.8f);

    auto face = r.reduced (4.0f);
    g.saveState();
    Path clip;
    clip.addRoundedRectangle (face, 3.0f);
    g.reduceClipRegion (clip);

    // warm-white backlit scale: brighter in the middle (lamp behind the card)
    const float bl = jlimit (0.0f, 1.0f, backlight);
    const Colour paper = vuWarm.interpolatedWith (Colour (0xff6d6250), 1.0f - bl);
    ColourGradient card (paper.brighter (0.12f), face.getCentreX(), face.getBottom() - face.getHeight() * 0.15f,
                         paper.darker (0.55f), face.getX(), face.getY(), true);
    g.setGradientFill (card);
    g.fillRect (face);

    // geometry: pivot below the face, arc across the upper part
    const Point<float> pivot { face.getCentreX(), face.getBottom() + face.getHeight() * 0.42f };
    const float rad = face.getHeight() * 1.16f;
    const float span = 0.70f; // half angle in radians
    auto angleFor = [span] (float v)
    {
        // VU scale: -20 .. +3, roughly the classic non-linear (voltage) spacing
        const float lin = std::pow (10.0f, v / 20.0f);
        const float lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
        return -span + (lin - lo) / (hi - lo) * 2.0f * span;
    };
    const Colour ink (0xff1c1a17), redInk (0xffd0222d);

    // arcs: main scale line, red band above 0 VU
    Path main;
    main.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, angleFor (-20.0f), angleFor (0.0f), true);
    g.setColour (ink);
    g.strokePath (main, PathStrokeType (1.0f));
    Path red;
    red.addCentredArc (pivot.x, pivot.y, rad + 1.6f, rad + 1.6f, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
    g.setColour (redInk);
    g.strokePath (red, PathStrokeType (3.2f));

    // ticks + numbers
    struct Tick { float v; const char* label; };
    static const Tick ticks[] = { { -20.0f, "20" }, { -10.0f, "10" }, { -7.0f, "7" }, { -5.0f, "5" }, { -3.0f, "3" },
                                  { -2.0f, "" }, { -1.0f, "1" }, { 0.0f, "0" }, { 1.0f, "" }, { 2.0f, "2" }, { 3.0f, "3" } };
    const float fh = jmax (6.5f, face.getHeight() * 0.13f);
    g.setFont (Font (FontOptions().withName (Font::getDefaultSansSerifFontName()).withHeight (fh).withStyle ("Bold")));
    for (const auto& t : ticks)
    {
        const float a = angleFor (t.v);
        const bool major = *t.label != 0;
        const bool hot = t.v > 0.0f;
        g.setColour (hot ? redInk : ink);
        g.drawLine ({ polar (pivot, rad, a), polar (pivot, rad + (major ? 5.0f : 3.0f), a) }, major ? 1.1f : 0.8f);
        if (major)
        {
            const auto p = polar (pivot, rad + 5.0f + fh * 0.7f, a);
            g.drawText (t.label, Rectangle<float> (fh * 2.0f, fh).withCentre (p), Justification::centred, false);
        }
    }
    // percentage marks under the arc
    for (float v : { -20.0f, -10.0f, -6.0f, -3.0f, 0.0f })
    {
        const float a = angleFor (v);
        g.setColour (ink.withAlpha (0.7f));
        g.fillEllipse (Rectangle<float> (1.6f, 1.6f).withCentre (polar (pivot, rad - 3.5f, a)));
    }
    // legends
    g.setFont (Font (FontOptions().withName (Font::getDefaultSerifFontName()).withHeight (face.getHeight() * 0.2f).withStyle ("Bold Italic")));
    g.setColour (ink.withAlpha (0.85f));
    g.drawText ("VU", Rectangle<float> (40.0f, face.getHeight() * 0.22f).withCentre ({ face.getCentreX(), face.getBottom() - face.getHeight() * 0.27f }),
                Justification::centred, false);
    g.setFont (theme::monoLabel (jmax (6.5f, face.getHeight() * 0.1f)).boldened());
    g.setColour (ink.withAlpha (0.6f));
    g.drawText (name, face.withTrimmedBottom (face.getHeight() * 0.11f).reduced (5.0f, 1.0f), Justification::bottomLeft, false);
    // needle
    const float a = angleFor (jlimit (-22.0f, 4.0f, vu));
    const auto tip = polar (pivot, rad + 4.0f, a);
    const auto foot = polar (pivot, face.getHeight() * 0.48f, a);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawLine ({ foot.translated (1.5f, 1.5f), tip.translated (1.5f, 1.5f) }, 1.3f);
    g.setColour (Colour (0xff111111));
    g.drawLine ({ foot, tip }, 1.1f);

    // inner vignette + glass reflection
    ColourGradient vig (Colours::transparentBlack, face.getCentreX(), face.getCentreY(), Colours::black.withAlpha (0.35f),
                        face.getX(), face.getY(), true);
    g.setGradientFill (vig);
    g.fillRect (face);
    // needle pivot housing (dark bar at the bottom)
    g.setColour (Colour (0xff15181f));
    g.fillRect (face.withTop (face.getBottom() - face.getHeight() * 0.1f));
    Path refl;
    refl.startNewSubPath (face.getX(), face.getY());
    refl.lineTo (face.getX() + face.getWidth() * 0.55f, face.getY());
    refl.lineTo (face.getX() + face.getWidth() * 0.25f, face.getBottom());
    refl.lineTo (face.getX(), face.getBottom());
    refl.closeSubPath();
    g.setColour (Colours::white.withAlpha (0.07f));
    g.fillPath (refl);
    g.restoreState();

    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (face, 3.0f, 1.0f);
}

// ============================================================================
void drawBlackGlass (Graphics& g, Rectangle<float> r, float corner)
{
    ColourGradient glass (Colour (0xff0b0e14), r.getX(), r.getY(), Colour (0xff020305), r.getX(), r.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (r, corner);
    // diagonal reflection
    g.saveState();
    Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    Path refl;
    refl.startNewSubPath (r.getX(), r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.42f, r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.18f, r.getBottom());
    refl.lineTo (r.getX(), r.getBottom());
    refl.closeSubPath();
    ColourGradient rg (Colours::white.withAlpha (0.045f), r.getX(), r.getY(), Colours::transparentWhite, r.getX() + r.getWidth() * 0.4f,
                       r.getBottom(), false);
    g.setGradientFill (rg);
    g.fillPath (refl);
    g.restoreState();
    // edge: ice-blue hairline, bright top lip
    g.setColour (iceLine.withAlpha (0.28f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
}

void drawLedTab (Graphics& g, Rectangle<float> r, const String& text, Colour led, bool selected, bool hover)
{
    const float corner = 3.0f;
    ColourGradient body (Colour (selected ? 0xff111a2a : (hover ? 0xff0d1320 : 0xff080b12)), r.getX(), r.getY(),
                         Colour (selected ? 0xff0a101c : 0xff05070b), r.getX(), r.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, corner);
    g.setColour (selected ? iceLine.withAlpha (0.55f) : Colours::white.withAlpha (hover ? 0.10f : 0.05f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, selected ? 1.0f : 0.8f);
    if (selected)
    {
        // lit edge on the right, pointing at the list
        g.setColour (iceLine.withAlpha (0.25f));
        g.fillRect (r.withLeft (r.getRight() - 4.0f).reduced (0.0f, 3.0f).expanded (2.0f, 0.0f));
        g.setColour (iceLine);
        g.fillRect (r.withLeft (r.getRight() - 2.0f).reduced (0.0f, 4.0f));
    }

    // status LED
    const auto c = Point<float> (r.getX() + 11.0f, r.getCentreY());
    g.setColour (Colour (0xff020304));
    g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (c));
    if (selected)
    {
        ColourGradient halo (led.withAlpha (0.55f), c.x, c.y, led.withAlpha (0.0f), c.x + 9.0f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (18.0f, 18.0f).withCentre (c));
        g.setColour (led.brighter (0.4f));
        g.fillEllipse (Rectangle<float> (5.5f, 5.5f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillEllipse (Rectangle<float> (2.0f, 2.0f).withCentre (c.translated (-0.8f, -0.8f)));
    }
    else
    {
        g.setColour (led.darker (1.3f).withMultipliedSaturation (0.8f));
        g.fillEllipse (Rectangle<float> (5.5f, 5.5f).withCentre (c));
    }

    g.setFont (theme::monoLabel (11.0f));
    g.setColour (selected ? iceText : (hover ? iceText.withAlpha (0.8f) : iceText.withAlpha (0.52f)));
    g.drawText (text, r.withTrimmedLeft (22.0f).withTrimmedRight (6.0f), Justification::centredLeft, true);
}
} // namespace ek::ui::performer
