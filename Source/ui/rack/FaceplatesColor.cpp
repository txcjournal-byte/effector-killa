#include "Faceplates.h"
#include "../Theme.h"
#include <map>

// Faceplates of the "colour" group: CassettePlug, Menace, Brainrot, ThroughTheWall, ToneUp, Squeeze,
// Overcooked, Slap (DESIGN-RACK.md section 6).
//
// Every plate is split into a static layer (material, ears, screws, name, printing, LCD bezel) that is
// rendered once per type/size/scale into a cached image, and a live layer (reels, LEDs, meters,
// curves) that is drawn on top every frame from the State snapshot.

namespace ek::faceplate
{
using namespace juce;
namespace th = ek::theme;

namespace
{
// ---------------------------------------------------------------------------------------------
// palette / identity
// ---------------------------------------------------------------------------------------------
Colour col (uint32 argb) { return Colour (argb); }

struct Identity { const char* type; const char* model; };

Identity identity (EffectType t)
{
    switch (t)
    {
        case EffectType::CassettePlug:   return { "TAPE / TUBE SATURATION", "CP-77" };
        case EffectType::Menace:         return { "DISTORTION", "MN-4" };
        case EffectType::Brainrot:       return { "BITCRUSHER / LO-FI", "BR-8" };
        case EffectType::ThroughTheWall: return { "RESONANT FILTER", "TW-6" };
        case EffectType::ToneUp:         return { "5-BAND EQUALIZER", "TU-5" };
        case EffectType::Squeeze:        return { "COMPRESSOR", "SQ-2" };
        case EffectType::Overcooked:     return { "3-BAND UP / DOWN DYNAMICS", "OC-3" };
        case EffectType::Slap:           return { "TRANSIENT SHAPER", "SL-1" };
        default:                         return { "", "" };
    }
}

String displayName (EffectType t)
{
    switch (t)
    {
        case EffectType::CassettePlug:   return "Cassette Plug";
        case EffectType::Menace:         return "MENACE";
        case EffectType::Brainrot:       return "BRAINROT";
        case EffectType::ThroughTheWall: return "Through The Wall";
        case EffectType::ToneUp:         return "Tone Up";
        case EffectType::Squeeze:        return "Squeeze";
        case EffectType::Overcooked:     return "OVERCOOKED";
        case EffectType::Slap:           return "SLAP!";
        default:                         return {};
    }
}

float realParam (EffectType t, const State& s, int i)
{
    return effectInfo (t).params[(size_t) i].toReal (s.params[(size_t) i]);
}

// ---------------------------------------------------------------------------------------------
// materials
// ---------------------------------------------------------------------------------------------
void lacquer (Graphics& g, Rectangle<float> r, Colour c, float corner, float gloss)
{
    ColourGradient gr (c.brighter (0.12f), 0.0f, r.getY(), c.darker (0.22f), 0.0f, r.getBottom(), false);
    gr.addColour (0.18, c.brighter (0.04f));
    gr.addColour (0.65, c);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r, corner);

    if (gloss > 0.0f) // broad soft sheen across the upper left
    {
        ColourGradient sheen (Colours::white.withAlpha (0.13f * gloss), r.getX() + r.getWidth() * 0.18f, r.getY(),
                              Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.42f, r.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (r, corner);
    }
}

void speckle (Graphics& g, Rectangle<float> r, int seed, int count, Colour light, Colour dark, float maxAlpha, float size)
{
    Random rnd (seed);
    for (int i = 0; i < count; ++i)
    {
        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
        const float y = r.getY() + rnd.nextFloat() * r.getHeight();
        g.setColour ((rnd.nextBool() ? light : dark).withAlpha (rnd.nextFloat() * maxAlpha));
        g.fillRect (x, y, size, size);
    }
}

void brushed (Graphics& g, Rectangle<float> r, int seed, float alpha, float step)
{
    Random rnd (seed);
    for (float y = r.getY(); y < r.getBottom(); y += step)
    {
        const float x0 = r.getX() + rnd.nextFloat() * r.getWidth() * 0.5f - r.getWidth() * 0.2f;
        const float len = r.getWidth() * (0.35f + rnd.nextFloat() * 0.9f);
        const float a = rnd.nextFloat() * alpha;
        g.setColour ((rnd.nextFloat() < 0.55f ? Colours::white : Colours::black).withAlpha (a));
        const float xa = jmax (r.getX(), x0), xb = jmin (r.getRight(), x0 + len);
        if (xb > xa) g.fillRect (xa, y, xb - xa, step * 0.6f);
    }
}

void plateEdges (Graphics& g, Rectangle<float> r, float corner, float strength = 1.0f)
{
    g.setColour (Colours::white.withAlpha (0.30f * strength));
    g.fillRect (r.getX() + corner, r.getY() + 0.6f, r.getWidth() - 2.0f * corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.40f * strength));
    g.fillRect (r.getX() + corner, r.getBottom() - 1.6f, r.getWidth() - 2.0f * corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

// ---------------------------------------------------------------------------------------------
// hardware: screws, ears, LCD bezel, recessed windows
// ---------------------------------------------------------------------------------------------
void screw (Graphics& g, Point<float> c, float r, float angle)
{
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (r * 2.3f, r * 2.3f).withCentre (c.translated (0.5f, 0.9f)));
    ColourGradient head (col (0xfff4f6f9), c.x - r * 0.45f, c.y - r * 0.55f, col (0xff5c626b), c.x + r * 0.9f, c.y + r * 0.9f, true);
    g.setGradientFill (head);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 0.6f);

    Path cross;
    cross.addRoundedRectangle (-r * 0.66f, -r * 0.14f, r * 1.32f, r * 0.28f, r * 0.08f);
    cross.addRoundedRectangle (-r * 0.14f, -r * 0.66f, r * 0.28f, r * 1.32f, r * 0.08f);
    const auto tf = AffineTransform::rotation (angle).translated (c);
    g.setColour (Colours::white.withAlpha (0.45f));
    g.fillPath (cross, tf.translated (0.0f, 0.55f));
    g.setColour (col (0xff23262b));
    g.fillPath (cross, tf);
}

constexpr float earW = 17.0f;

void ears (Graphics& g, Rectangle<float> p, Colour slotRim, bool hazard = false)
{
    Random rnd (77);
    for (int side = 0; side < 2; ++side)
    {
        auto e = side == 0 ? p.withWidth (earW) : p.withLeft (p.getRight() - earW);

        if (hazard) // Menace: hazard tape on the ears
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (e.toNearestInt());
            g.setColour (col (0xffc8202c));
            g.fillRect (e);
            g.setColour (col (0xff0e0e10));
            for (float y = e.getY() - 30.0f; y < e.getBottom() + 30.0f; y += 16.0f)
            {
                Path stripe;
                stripe.startNewSubPath (e.getX() - 4.0f, y);
                stripe.lineTo (e.getRight() + 4.0f, y - 22.0f);
                stripe.lineTo (e.getRight() + 4.0f, y - 14.0f);
                stripe.lineTo (e.getX() - 4.0f, y + 8.0f);
                stripe.closeSubPath();
                g.fillPath (stripe);
            }
            speckle (g, e, 31 + side, 140, Colours::white, Colours::black, 0.18f, 0.8f);
        }
        else
        {
            g.setColour (Colours::black.withAlpha (0.10f));
            g.fillRect (e);
        }

        // fold / seam between ear and the main panel
        const float sx = side == 0 ? e.getRight() : e.getX();
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRect (sx - 0.5f, p.getY() + 2.0f, 1.0f, p.getHeight() - 4.0f);
        g.setColour (Colours::white.withAlpha (0.18f));
        g.fillRect (sx + (side == 0 ? 0.5f : -1.5f), p.getY() + 2.0f, 1.0f, p.getHeight() - 4.0f);

        for (int k = 0; k < 2; ++k)
        {
            const Point<float> c (e.getCentreX(), k == 0 ? p.getY() + 21.0f : p.getBottom() - 21.0f);
            auto slot = Rectangle<float> (12.0f, 19.0f).withCentre (c);
            g.setColour (slotRim.withAlpha (0.9f));
            g.fillRoundedRectangle (slot.expanded (1.0f), 6.5f);
            ColourGradient hole (col (0xff050506), 0.0f, slot.getY(), col (0xff1d1f23), 0.0f, slot.getBottom(), false);
            g.setGradientFill (hole);
            g.fillRoundedRectangle (slot, 6.0f);
            g.setColour (Colours::white.withAlpha (0.16f));
            g.drawRoundedRectangle (slot.expanded (0.6f).translated (0.0f, 0.5f), 6.4f, 0.6f);
            screw (g, c.translated (0.0f, k == 0 ? 2.5f : -2.0f), 4.6f, 0.35f + rnd.nextFloat() * 1.2f);
        }
    }
}

void lcdFrame (Graphics& g, Rectangle<float> r)
{
    // recessed bezel: shadow on top, catch light on the bottom lip
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.expanded (4.0f), 5.0f);
    g.setColour (Colours::white.withAlpha (0.22f));
    g.fillRoundedRectangle (r.expanded (4.0f).withTrimmedTop (r.getHeight() + 6.5f), 3.0f);
    ColourGradient bez (col (0xff1a1b1f), 0.0f, r.getY() - 3.0f, col (0xff3a3c42), 0.0f, r.getBottom() + 3.0f, false);
    g.setGradientFill (bez);
    g.fillRoundedRectangle (r.expanded (3.0f), 4.0f);
    ColourGradient glass (col (0xff0c1214), 0.0f, r.getY(), col (0xff050708), 0.0f, r.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRect (r.getX() + 1.0f, r.getY(), r.getWidth() - 2.0f, 1.2f);
    // faint diagonal reflection
    Path refl;
    refl.startNewSubPath (r.getX() + r.getWidth() * 0.55f, r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.72f, r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.64f, r.getBottom());
    refl.lineTo (r.getX() + r.getWidth() * 0.47f, r.getBottom());
    refl.closeSubPath();
    g.setColour (Colours::white.withAlpha (0.035f));
    g.fillPath (refl);
}

// dark recessed window used by displays and meters
void window (Graphics& g, Rectangle<float> r, Colour glassTop, Colour glassBottom, float corner = 3.0f)
{
    g.setColour (Colours::black.withAlpha (0.30f));
    g.fillRoundedRectangle (r.expanded (2.5f), corner + 2.0f);
    g.setColour (Colours::white.withAlpha (0.20f));
    g.fillRoundedRectangle (r.expanded (2.5f).withTrimmedTop (r.getHeight() + 3.0f), corner);
    g.setColour (col (0xff0b0c0e));
    g.fillRoundedRectangle (r.expanded (1.5f), corner + 1.0f);
    ColourGradient gl (glassTop, 0.0f, r.getY(), glassBottom, 0.0f, r.getBottom(), false);
    g.setGradientFill (gl);
    g.fillRoundedRectangle (r, corner);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRect (r.getX() + corner, r.getY(), r.getWidth() - corner * 2.0f, 1.0f);
}

void glassShine (Graphics& g, Rectangle<float> r, float corner = 3.0f)
{
    ColourGradient sh (Colours::white.withAlpha (0.10f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.45f, false);
    g.setGradientFill (sh);
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), corner);
}

void grid (Graphics& g, Rectangle<float> r, Colour c, int nx, int ny)
{
    g.setColour (c);
    for (int i = 1; i < nx; ++i) g.fillRect (r.getX() + r.getWidth() * (float) i / (float) nx, r.getY(), 0.6f, r.getHeight());
    for (int i = 1; i < ny; ++i) g.fillRect (r.getX(), r.getY() + r.getHeight() * (float) i / (float) ny, r.getWidth(), 0.6f);
}

void glowLed (Graphics& g, Point<float> c, float r, Colour colour, float amount)
{
    amount = jlimit (0.0f, 1.0f, amount);
    th::drawLed (g, c, r, colour, amount > 0.04f, 0.35f + 0.9f * amount);
    if (amount > 0.04f && amount < 0.999f) // partially lit: blend towards the dark body
    {
        g.setColour (Colour (0xff0a0a08).withAlpha (0.75f * (1.0f - amount)));
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    }
}

// ---------------------------------------------------------------------------------------------
// printing
// ---------------------------------------------------------------------------------------------
Font nameFont (float h) { return th::stencilFont (h); }
Font smallFont (float h) { return th::monoLabel (h); }

void text (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j)
{
    g.setFont (f);
    g.setColour (c);
    g.drawFittedText (s, r.toNearestInt(), j, 1, 0.6f);
}

// printed text with a 1px catch light below (engraved / screen printed on a lit surface)
void engraved (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j, float hi = 0.35f)
{
    g.setFont (f);
    g.setColour (Colours::white.withAlpha (hi));
    g.drawFittedText (s, r.translated (0.0f, 1.0f).toNearestInt(), j, 1, 0.6f);
    g.setColour (c);
    g.drawFittedText (s, r.toNearestInt(), j, 1, 0.6f);
}

// "SATURATION ................ TRAPWIN  CP-77" under the name
void typeRow (Graphics& g, Rectangle<float> r, EffectType t, Colour ink, Colour accent)
{
    const auto id = identity (t);
    text (g, id.type, r, smallFont (8.5f), ink.withAlpha (0.85f), Justification::centredLeft);
    auto right = r.removeFromRight (78.0f);
    text (g, String ("TRAPWIN  ") + id.model, right, smallFont (7.5f).boldened(), accent, Justification::centredRight);
}

void knobBay (Graphics& g, Rectangle<float> r, Colour ink, float alpha)
{
    g.setColour (ink.withAlpha (alpha));
    g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 0.8f);
    // corner registration ticks
    const float t = 5.0f;
    g.setColour (ink.withAlpha (alpha * 1.8f));
    for (auto c : { r.reduced (1.0f).getTopLeft(), r.reduced (1.0f).getTopRight(), r.reduced (1.0f).getBottomLeft(), r.reduced (1.0f).getBottomRight() })
    {
        const float sx = c.x < r.getCentreX() ? 1.0f : -1.0f, sy = c.y < r.getCentreY() ? 1.0f : -1.0f;
        g.fillRect (jmin (c.x, c.x + sx * t), c.y - 0.4f, t, 0.8f);
        g.fillRect (c.x - 0.4f, jmin (c.y, c.y + sy * t), 0.8f, t);
    }
}

Rectangle<float> topBand (Rectangle<float> p)
{
    const auto lcd = presetLcdArea (p);
    const float x = p.getX() + 256.0f;
    return { x, p.getY() + 13.0f, lcd.getX() - 18.0f - x, 20.0f };
}

// miniature (264 x 41) layout
Rectangle<float> miniName (Rectangle<float> p) { return { p.getX() + 9.0f, p.getY() + 3.0f, 150.0f, p.getHeight() - 6.0f }; }
Rectangle<float> miniLive (Rectangle<float> p) { return { p.getRight() - 9.0f - 88.0f, p.getY() + 6.0f, 88.0f, p.getHeight() - 12.0f }; }

// ---------------------------------------------------------------------------------------------
// static layers per effect
// ---------------------------------------------------------------------------------------------
void staticCassette (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto cream = col (0xffe8dcc2), brown = col (0xff4a2e1c), orange = col (0xffc9552a), mustard = col (0xffd9a23a);
    lacquer (g, p, cream, 3.0f, 1.0f);
    speckle (g, p, 11, (int) (p.getWidth() * p.getHeight() / 9.0f), Colours::white, col (0xff7a5a32), 0.10f, 0.9f);

    // retro stripes running across the plate
    auto stripes = [&] (float y, float x0, float x1, float h)
    {
        const Colour cs[] = { orange, mustard, brown };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (cs[i].withAlpha (0.92f));
            g.fillRect (x0, y + (float) i * (h + 1.5f), x1 - x0, h);
        }
    };

    if (mini)
    {
        stripes (p.getBottom() - 9.0f, p.getX(), p.getRight(), 1.6f);
        auto n = miniName (p).withTrimmedBottom (6.0f);
        engraved (g, displayName (EffectType::CassettePlug), n, nameFont (19.0f), brown, Justification::centredLeft, 0.5f);
        plateEdges (g, p, 3.0f, 0.8f);
        return;
    }

    ears (g, p, col (0xffcbb995));
    const auto na = nameArea (p);
    stripes (p.getY() + 23.0f, na.getRight() + 8.0f, presetLcdArea (p).getX() - 14.0f, 2.2f);
    stripes (p.getBottom() - 12.0f, p.getX() + earW + 4.0f, p.getRight() - earW - 4.0f, 1.4f);

    // label: brown frame with the name in brown on cream
    auto label = na.withHeight (36.0f);
    g.setColour (col (0xfff3ead6));
    g.fillRoundedRectangle (label, 4.0f);
    g.setColour (brown);
    g.drawRoundedRectangle (label.reduced (1.0f), 3.5f, 1.6f);
    g.drawRoundedRectangle (label.reduced (3.5f), 2.0f, 0.6f);
    text (g, displayName (EffectType::CassettePlug), label.reduced (10.0f, 3.0f).translated (0.0f, 0.5f), nameFont (26.0f), brown, Justification::centred);
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::CassettePlug, brown, orange.darker (0.2f));

    knobBay (g, knobArea (EffectType::CassettePlug, p), brown, 0.22f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f);
}

void staticMenace (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto black = col (0xff161618), red = col (0xffd8222f), white = col (0xffeceef2);
    lacquer (g, p, black, 3.0f, 0.4f);
    brushed (g, p, 21, 0.05f, 0.7f);
    speckle (g, p, 22, (int) (p.getWidth() * p.getHeight() / 14.0f), Colours::white, Colours::black, 0.08f, 0.8f);

    auto hazardBand = [&] (Rectangle<float> b, float period)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (b.toNearestInt());
        g.setColour (red);
        g.fillRect (b);
        g.setColour (col (0xff0c0c0d));
        for (float x = b.getX() - b.getHeight(); x < b.getRight() + b.getHeight(); x += period)
        {
            Path s;
            s.startNewSubPath (x, b.getBottom());
            s.lineTo (x + b.getHeight(), b.getY());
            s.lineTo (x + b.getHeight() + period * 0.5f, b.getY());
            s.lineTo (x + period * 0.5f, b.getBottom());
            s.closeSubPath();
            g.fillPath (s);
        }
        speckle (g, b, 23, (int) (b.getWidth() * b.getHeight() / 10.0f), Colours::white, Colours::black, 0.22f, 0.9f);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawRect (b, 1.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.fillRect (b.getX(), b.getY() + 1.0f, b.getWidth(), 1.0f);
    };

    if (mini)
    {
        hazardBand (p.withHeight (5.0f).withY (p.getBottom() - 5.0f), 10.0f);
        auto n = miniName (p).withTrimmedBottom (4.0f);
        g.setColour (red);
        g.fillRect (n.getX(), n.getY() + 7.0f, 3.0f, n.getHeight() - 14.0f);
        text (g, displayName (EffectType::Menace), n.withTrimmedLeft (8.0f), nameFont (20.0f), white, Justification::centredLeft);
        plateEdges (g, p, 3.0f, 0.6f);
        return;
    }

    ears (g, p, col (0xff2a2a2e), true);
    hazardBand (topBand (p), 14.0f);
    const auto na = nameArea (p);
    // red bar + white stencil name
    g.setColour (red);
    g.fillRect (na.getX(), na.getY() + 3.0f, 5.0f, 30.0f);
    auto nr = na.withHeight (36.0f).withTrimmedLeft (12.0f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.setFont (nameFont (31.0f));
    g.drawFittedText (displayName (EffectType::Menace), nr.translated (1.5f, 1.5f).toNearestInt(), Justification::centredLeft, 1, 0.6f);
    text (g, displayName (EffectType::Menace), nr, nameFont (31.0f), white, Justification::centredLeft);
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::Menace, red.brighter (0.25f), white.withAlpha (0.6f));

    knobBay (g, knobArea (EffectType::Menace, p), white, 0.09f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f, 0.7f);
}

void pcbTraces (Graphics& g, Rectangle<float> r, int seed, int count, float alpha)
{
    Random rnd (seed);
    const auto trace = col (0xff35a35c).withAlpha (alpha);
    const auto gold = col (0xffd6b25a);
    for (int i = 0; i < count; ++i)
    {
        Path path;
        float x = r.getX() + rnd.nextFloat() * r.getWidth();
        float y = r.getY() + rnd.nextFloat() * r.getHeight();
        path.startNewSubPath (x, y);
        const int segs = 2 + rnd.nextInt (3);
        bool horiz = rnd.nextBool();
        for (int s = 0; s < segs; ++s)
        {
            const float len = 10.0f + rnd.nextFloat() * 60.0f;
            const float dir = rnd.nextBool() ? 1.0f : -1.0f;
            if (horiz) x = jlimit (r.getX(), r.getRight(), x + dir * len);
            else       y = jlimit (r.getY(), r.getBottom(), y + dir * len * 0.4f);
            // 45 degree chamfer into the next segment
            const float d = 4.0f;
            path.lineTo (x, y);
            if (s + 1 < segs)
            {
                x = jlimit (r.getX(), r.getRight(), x + (rnd.nextBool() ? d : -d));
                y = jlimit (r.getY(), r.getBottom(), y + (rnd.nextBool() ? d : -d));
                path.lineTo (x, y);
            }
            horiz = ! horiz;
        }
        g.setColour (trace);
        g.strokePath (path, PathStrokeType (1.4f, PathStrokeType::mitered, PathStrokeType::rounded));
        // pad / via at the end
        g.setColour (gold.withAlpha (alpha * 1.1f));
        g.fillEllipse (Rectangle<float> (4.2f, 4.2f).withCentre ({ x, y }));
        g.setColour (col (0xff0d2a17).withAlpha (alpha + 0.2f));
        g.fillEllipse (Rectangle<float> (1.6f, 1.6f).withCentre ({ x, y }));
    }
}

void staticBrainrot (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto pcb = col (0xff1d6a3b), silk = col (0xfff2f5ee), gold = col (0xffd6b25a);
    lacquer (g, p, pcb, 3.0f, 0.5f);
    speckle (g, p, 31, (int) (p.getWidth() * p.getHeight() / 12.0f), col (0xff7fe0a0), col (0xff0b2a16), 0.10f, 0.8f);

    if (mini)
    {
        pcbTraces (g, p, 32, 26, 0.45f);
        auto n = miniName (p).withTrimmedBottom (4.0f);
        text (g, displayName (EffectType::Brainrot), n.withTrimmedLeft (2.0f), nameFont (19.0f), silk, Justification::centredLeft);
        plateEdges (g, p, 3.0f, 0.6f);
        return;
    }

    const auto ka = knobArea (EffectType::Brainrot, p);
    {
        // dense traces outside the knob bay, faint ones inside
        Graphics::ScopedSaveState ss (g);
        RectangleList<int> outside (p.toNearestInt());
        outside.subtract (ka.toNearestInt());
        g.reduceClipRegion (outside);
        pcbTraces (g, p, 33, 70, 0.55f);
    }
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (ka.toNearestInt());
        pcbTraces (g, ka, 34, 26, 0.18f);
    }

    // gold edge-connector fingers along the bottom edge
    for (float x = p.getX() + 40.0f; x < p.getRight() - 40.0f; x += 9.0f)
    {
        g.setColour (gold.withAlpha (0.85f));
        g.fillRoundedRectangle (x, p.getBottom() - 5.0f, 5.0f, 6.0f, 1.0f);
    }

    ears (g, p, gold.darker (0.2f));

    const auto na = nameArea (p);
    auto nr = na.withHeight (36.0f);
    // silkscreen component outline around the name
    g.setColour (pcb.darker (0.35f).withAlpha (0.85f));
    g.fillRect (nr.reduced (1.0f));
    g.setColour (silk);
    g.drawRect (nr, 1.2f);
    g.fillEllipse (nr.getX() + 4.0f, nr.getY() + 4.0f, 3.0f, 3.0f); // pin-1 dot
    text (g, displayName (EffectType::Brainrot), nr.reduced (12.0f, 2.0f), nameFont (28.0f), silk, Justification::centred);
    text (g, "U1", { nr.getRight() - 18.0f, nr.getY() - 9.0f, 18.0f, 8.0f }, smallFont (7.0f), silk.withAlpha (0.8f), Justification::centredRight);
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::Brainrot, silk, gold);

    // silkscreen markings in the top band
    const auto tb = topBand (p);
    text (g, "REV 1.3   //   LO-FI ENGINE   //   DO NOT BEND", tb, smallFont (8.0f), silk.withAlpha (0.75f), Justification::centredLeft);
    for (int i = 0; i < 4; ++i) // tiny SMD resistors
    {
        auto rr = Rectangle<float> (10.0f, 5.0f).withCentre ({ tb.getRight() - 12.0f - (float) i * 16.0f, tb.getCentreY() });
        g.setColour (col (0xff1a1a1a)); g.fillRect (rr);
        g.setColour (col (0xffd8d8d8)); g.fillRect (rr.withWidth (2.0f)); g.fillRect (rr.withLeft (rr.getRight() - 2.0f));
    }

    knobBay (g, ka, silk, 0.28f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f, 0.7f);
}

void staticThroughTheWall (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto navy = col (0xff1d2c55), white = col (0xfff0f3fa), ice = col (0xff9fc3ff);
    lacquer (g, p, navy, 3.0f, 0.8f);
    speckle (g, p, 41, (int) (p.getWidth() * p.getHeight() / 12.0f), Colours::white, Colours::black, 0.07f, 0.8f);

    // faint brick wall printed into the lacquer
    {
        const float bh = mini ? 7.0f : 11.0f, bw = mini ? 20.0f : 32.0f;
        g.setColour (Colours::white.withAlpha (0.045f));
        int row = 0;
        for (float y = p.getY() + 2.0f; y < p.getBottom(); y += bh, ++row)
        {
            g.fillRect (p.getX(), y, p.getWidth(), 0.8f);
            for (float x = p.getX() + ((row & 1) ? bw * 0.5f : 0.0f); x < p.getRight(); x += bw)
                g.fillRect (x, y, 0.8f, bh);
        }
        ColourGradient fade (navy.withAlpha (0.0f), p.getX(), 0.0f, navy.withAlpha (0.85f), p.getX() + p.getWidth() * 0.6f, 0.0f, false);
        g.setGradientFill (fade);
        g.fillRect (p.withTrimmedLeft (p.getWidth() * 0.2f));
    }

    if (mini)
    {
        auto n = miniName (p);
        text (g, displayName (EffectType::ThroughTheWall), n.withTrimmedBottom (4.0f), nameFont (17.0f), white, Justification::centredLeft);
        g.setColour (ice.withAlpha (0.8f));
        g.fillRect (n.getX(), n.getBottom() - 6.0f, 60.0f, 1.2f);
        plateEdges (g, p, 3.0f, 0.7f);
        return;
    }

    ears (g, p, navy.darker (0.4f));
    const auto na = nameArea (p);
    auto nr = na.withHeight (34.0f);
    text (g, displayName (EffectType::ThroughTheWall), nr, nameFont (26.0f), white, Justification::centredLeft);
    ColourGradient line (ice, na.getX(), 0.0f, ice.withAlpha (0.0f), na.getRight(), 0.0f, false);
    g.setGradientFill (line);
    g.fillRect (na.getX(), na.getY() + 36.0f, na.getWidth(), 1.4f);
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::ThroughTheWall, white.withAlpha (0.8f), ice);

    // top band: printed response legend
    const auto tb = topBand (p);
    const char* types[] = { "LP12", "LP24", "HP", "BP", "NOTCH", "COMB" };
    float x = tb.getX();
    for (auto* tname : types)
    {
        text (g, tname, { x, tb.getY(), 44.0f, tb.getHeight() }, smallFont (8.0f), white.withAlpha (0.45f), Justification::centredLeft);
        x += 46.0f;
    }

    knobBay (g, knobArea (EffectType::ThroughTheWall, p), white, 0.12f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f, 0.8f);
}

void staticToneUp (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto alu = col (0xffc9ced6), ink = col (0xff15171b);
    lacquer (g, p, alu, 3.0f, 0.0f);
    brushed (g, p, 51, 0.22f, 0.45f);
    // anodised sheen band
    ColourGradient sheen (Colours::white.withAlpha (0.0f), p.getX(), p.getY(), Colours::white.withAlpha (0.0f), p.getRight(), p.getY(), false);
    sheen.addColour (0.3, Colours::white.withAlpha (0.16f));
    sheen.addColour (0.42, Colours::white.withAlpha (0.0f));
    sheen.addColour (0.75, Colours::white.withAlpha (0.10f));
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (p, 3.0f);

    const Colour bands[] = { col (0xffd8352f), col (0xffeb8a24), col (0xffe9c429), col (0xff35a85a), col (0xff2f6fd6) };

    if (mini)
    {
        auto n = miniName (p);
        engraved (g, displayName (EffectType::ToneUp), n.withTrimmedBottom (5.0f), nameFont (19.0f), ink, Justification::centredLeft, 0.6f);
        for (int i = 0; i < 5; ++i)
        {
            g.setColour (bands[i]);
            g.fillRect (n.getX() + (float) i * 12.0f, n.getBottom() - 4.0f, 10.0f, 2.0f);
        }
        plateEdges (g, p, 3.0f, 0.9f);
        return;
    }

    ears (g, p, alu.darker (0.3f));
    const auto na = nameArea (p);
    engraved (g, displayName (EffectType::ToneUp), na.withHeight (34.0f), nameFont (30.0f), ink, Justification::centredLeft, 0.7f);
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (bands[i]);
        g.fillRect (na.getX() + (float) i * 18.0f, na.getY() + 35.5f, 15.0f, 2.2f);
    }
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::ToneUp, ink, ink.withAlpha (0.65f));

    // top band: printed frequency ruler
    const auto tb = topBand (p);
    g.setColour (ink.withAlpha (0.55f));
    g.fillRect (tb.getX(), tb.getBottom() - 6.0f, tb.getWidth(), 0.8f);
    const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* lbl[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    for (int i = 0; i < 10; ++i)
    {
        const float x = tb.getX() + tb.getWidth() * std::log (freqs[i] / 20.0f) / std::log (1000.0f);
        g.fillRect (x - 0.4f, tb.getBottom() - 9.0f, 0.8f, 6.0f);
        text (g, lbl[i], { x - 14.0f, tb.getY() - 1.0f, 28.0f, 10.0f }, smallFont (7.0f), ink.withAlpha (0.6f),
              i == 0 ? Justification::centredLeft : (i == 9 ? Justification::centredRight : Justification::centred));
    }
    for (int d = 1; d < 9; ++d) // minor log ticks
        for (int k = 2; k < 10; ++k)
        {
            const float f = (float) k * std::pow (10.0f, (float) (d / 3 + 1));
            if (f < 20.0f || f > 20000.0f || d > 3) continue;
            const float x = tb.getX() + tb.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
            g.setColour (ink.withAlpha (0.25f));
            g.fillRect (x - 0.3f, tb.getBottom() - 7.5f, 0.6f, 3.0f);
        }

    knobBay (g, knobArea (EffectType::ToneUp, p), ink, 0.14f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f, 1.0f);
}

void staticSqueeze (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto graphite = col (0xff24262b), green = col (0xff3dff6e), light = col (0xffdfe3e8);
    lacquer (g, p, graphite, 3.0f, 0.5f);
    speckle (g, p, 61, (int) (p.getWidth() * p.getHeight() / 5.0f), Colours::white, Colours::black, 0.07f, 0.8f); // powder coat

    if (mini)
    {
        g.setColour (green.withAlpha (0.7f));
        g.fillRect (p.getX() + 3.0f, p.getY() + 3.0f, p.getWidth() - 6.0f, 0.9f);
        auto n = miniName (p);
        text (g, displayName (EffectType::Squeeze), n.withTrimmedBottom (4.0f), nameFont (19.0f), light, Justification::centredLeft);
        g.setColour (green);
        g.fillEllipse (n.getX() + 84.0f, n.getCentreY() - 4.5f, 4.0f, 4.0f);
        plateEdges (g, p, 3.0f, 0.6f);
        return;
    }

    ears (g, p, col (0xff3a3d44));
    // green pin-stripe through the panel
    g.setColour (green.withAlpha (0.55f));
    g.fillRect (p.getX() + earW + 6.0f, p.getY() + 6.0f, p.getWidth() - 2.0f * earW - 12.0f, 1.0f);
    g.setColour (green.withAlpha (0.12f));
    g.fillRect (p.getX() + earW + 6.0f, p.getY() + 5.0f, p.getWidth() - 2.0f * earW - 12.0f, 3.0f);

    const auto na = nameArea (p);
    // recessed black name inlay
    auto inlay = na.withHeight (35.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillRoundedRectangle (inlay.expanded (1.0f).translated (0.0f, 1.0f), 3.0f);
    g.setColour (col (0xff0d0e10));
    g.fillRoundedRectangle (inlay, 3.0f);
    text (g, displayName (EffectType::Squeeze), inlay.reduced (12.0f, 2.0f), nameFont (27.0f), light, Justification::centredLeft);
    g.setColour (green);
    g.fillEllipse (inlay.getRight() - 14.0f, inlay.getCentreY() - 2.5f, 5.0f, 5.0f);
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::Squeeze, light.withAlpha (0.8f), green);

    const auto tb = topBand (p);
    text (g, "VARIABLE RATIO  \xc2\xb7  PARALLEL MIX  \xc2\xb7  SOFT KNEE", tb, smallFont (8.0f), light.withAlpha (0.4f), Justification::centredLeft);

    knobBay (g, knobArea (EffectType::Squeeze, p), green, 0.16f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f, 0.7f);
}

void staticOvercooked (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto orange = col (0xffd9661f), dark = col (0xff2a1206), cream = col (0xfffff0d8);
    lacquer (g, p, orange, 3.0f, 1.0f);
    speckle (g, p, 71, (int) (p.getWidth() * p.getHeight() / 10.0f), Colours::white, col (0xff5a1a00), 0.08f, 0.8f);

    auto vents = [&] (Rectangle<float> b, float slotW, float gap)
    {
        for (float x = b.getX(); x + slotW <= b.getRight(); x += slotW + gap)
        {
            auto s = Rectangle<float> (x, b.getY(), slotW, b.getHeight());
            g.setColour (Colours::white.withAlpha (0.25f));
            g.fillRoundedRectangle (s.translated (0.0f, 1.0f), slotW * 0.5f);
            ColourGradient hole (col (0xff0d0502), 0.0f, s.getY(), col (0xff3a1606), 0.0f, s.getBottom(), false);
            g.setGradientFill (hole);
            g.fillRoundedRectangle (s, slotW * 0.5f);
        }
    };

    if (mini)
    {
        auto n = miniName (p);
        auto sticker = n.withWidth (128.0f).reduced (0.0f, 4.0f);
        g.setColour (dark);
        g.fillRoundedRectangle (sticker, 3.0f);
        text (g, displayName (EffectType::Overcooked), sticker.reduced (6.0f, 1.0f), nameFont (17.0f), cream, Justification::centred);
        plateEdges (g, p, 3.0f, 0.8f);
        return;
    }

    ears (g, p, orange.darker (0.35f));
    const auto tb = topBand (p);
    vents (tb.reduced (0.0f, 2.0f), 6.0f, 6.0f);

    const auto na = nameArea (p);
    auto sticker = na.withHeight (36.0f);
    g.setColour (Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (sticker.translated (1.0f, 1.5f), 4.0f);
    g.setColour (dark);
    g.fillRoundedRectangle (sticker, 4.0f);
    g.setColour (cream.withAlpha (0.8f));
    g.drawRoundedRectangle (sticker.reduced (2.5f), 2.5f, 0.8f);
    text (g, displayName (EffectType::Overcooked), sticker.reduced (10.0f, 2.0f).translated (0.0f, 0.5f), nameFont (27.0f), cream, Justification::centred);
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::Overcooked, dark, cream);

    knobBay (g, knobArea (EffectType::Overcooked, p), dark, 0.22f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f, 0.9f);
}

void halftone (Graphics& g, Rectangle<float> r, Point<float> focus, float radius, float step, Colour c)
{
    for (float y = r.getY(); y < r.getBottom(); y += step)
        for (float x = r.getX() + (std::fmod ((y - r.getY()) / step, 2.0f) < 1.0f ? 0.0f : step * 0.5f); x < r.getRight(); x += step)
        {
            const float d = focus.getDistanceFrom ({ x, y }) / radius;
            if (d >= 1.0f) continue;
            const float s = step * 0.62f * (1.0f - d);
            g.setColour (c);
            g.fillEllipse (Rectangle<float> (s, s).withCentre ({ x, y }));
        }
}

void staticSlap (Graphics& g, Rectangle<float> p, bool mini)
{
    const auto yellow = col (0xffe6c229), black = col (0xff0e0e0e);
    lacquer (g, p, yellow, 3.0f, 0.9f);
    speckle (g, p, 81, (int) (p.getWidth() * p.getHeight() / 12.0f), Colours::white, col (0xff6a5200), 0.08f, 0.8f);

    if (mini)
    {
        halftone (g, p, p.getBottomRight(), 90.0f, 4.0f, black.withAlpha (0.18f));
        auto n = miniName (p);
        auto sticker = Rectangle<float> (n.getX(), n.getY() + 3.0f, 70.0f, n.getHeight() - 6.0f);
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (-0.05f, sticker.getCentreX(), sticker.getCentreY()));
        g.setColour (black);
        g.fillRoundedRectangle (sticker, 2.0f);
        text (g, displayName (EffectType::Slap), sticker.reduced (4.0f, 1.0f), nameFont (19.0f), yellow, Justification::centred);
        return;
    }

    halftone (g, p, { p.getRight() - 40.0f, p.getBottom() }, 200.0f, 5.0f, black.withAlpha (0.12f));
    halftone (g, p, { p.getX() + 160.0f, p.getY() + 10.0f }, 120.0f, 5.0f, black.withAlpha (0.08f));
    ears (g, p, black);

    // black bar along the bottom
    g.setColour (black);
    g.fillRect (p.getX() + earW, p.getBottom() - 6.0f, p.getWidth() - 2.0f * earW, 6.0f);

    // chevrons in the top band
    const auto tb = topBand (p);
    for (float x = tb.getX(); x + 12.0f < tb.getRight(); x += 16.0f)
    {
        Path ch;
        ch.startNewSubPath (x, tb.getY() + 2.0f);
        ch.lineTo (x + 6.0f, tb.getY() + 2.0f);
        ch.lineTo (x + 12.0f, tb.getCentreY());
        ch.lineTo (x + 6.0f, tb.getBottom() - 2.0f);
        ch.lineTo (x, tb.getBottom() - 2.0f);
        ch.lineTo (x + 6.0f, tb.getCentreY());
        ch.closeSubPath();
        const float fade = 1.0f - (x - tb.getX()) / tb.getWidth();
        g.setColour (black.withAlpha (0.12f + 0.75f * fade * fade));
        g.fillPath (ch);
    }

    const auto na = nameArea (p);
    {
        auto sticker = Rectangle<float> (na.getX(), na.getY() + 1.0f, 128.0f, 35.0f);
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (-0.045f, sticker.getCentreX(), sticker.getCentreY()));
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (sticker.translated (2.0f, 2.0f), 3.0f);
        g.setColour (black);
        g.fillRoundedRectangle (sticker, 3.0f);
        g.setColour (yellow);
        g.drawRoundedRectangle (sticker.reduced (2.5f), 2.0f, 0.8f);
        text (g, displayName (EffectType::Slap), sticker.reduced (8.0f, 1.0f), nameFont (30.0f), yellow, Justification::centred);
    }
    typeRow (g, na.withTrimmedTop (39.0f), EffectType::Slap, black, black.withAlpha (0.7f));

    knobBay (g, knobArea (EffectType::Slap, p), black, 0.25f);
    lcdFrame (g, presetLcdArea (p));
    plateEdges (g, p, 3.0f, 0.9f);
}

void paintStatic (Graphics& g, EffectType t, Rectangle<float> p, bool mini)
{
    switch (t)
    {
        case EffectType::CassettePlug:   staticCassette (g, p, mini); break;
        case EffectType::Menace:         staticMenace (g, p, mini); break;
        case EffectType::Brainrot:       staticBrainrot (g, p, mini); break;
        case EffectType::ThroughTheWall: staticThroughTheWall (g, p, mini); break;
        case EffectType::ToneUp:         staticToneUp (g, p, mini); break;
        case EffectType::Squeeze:        staticSqueeze (g, p, mini); break;
        case EffectType::Overcooked:     staticOvercooked (g, p, mini); break;
        case EffectType::Slap:           staticSlap (g, p, mini); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------
// live elements
// ---------------------------------------------------------------------------------------------
void reelHub (Graphics& g, Point<float> c, float r, float angle)
{
    g.setColour (col (0xff0a0908));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    ColourGradient hub (col (0xfffbf8f1), c.x - r * 0.4f, c.y - r * 0.4f, col (0xffb9b2a4), c.x + r, c.y + r, true);
    g.setGradientFill (hub);
    g.fillEllipse (Rectangle<float> (r * 1.64f, r * 1.64f).withCentre (c));
    g.setColour (col (0xff151311));
    g.fillEllipse (Rectangle<float> (r * 1.0f, r * 1.0f).withCentre (c));
    // six drive teeth (these show the rotation)
    g.setColour (col (0xffe9e3d6));
    for (int k = 0; k < 6; ++k)
    {
        Path tooth;
        tooth.addRectangle (-r * 0.09f, -r * 0.52f, r * 0.18f, r * 0.26f);
        g.fillPath (tooth, AffineTransform::rotation (angle + (float) k * MathConstants<float>::twoPi / 6.0f).translated (c));
    }
}

void cassette (Graphics& g, Rectangle<float> r, float angle, bool tiny)
{
    // shell
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.6f, 1.0f), 3.0f);
    ColourGradient shell (col (0xff3a3836), 0.0f, r.getY(), col (0xff1c1b1a), 0.0f, r.getBottom(), false);
    g.setGradientFill (shell);
    g.fillRoundedRectangle (r, tiny ? 2.0f : 3.0f);

    const float w = r.getWidth(), h = r.getHeight();
    auto label = Rectangle<float> (r.getX() + w * 0.06f, r.getY() + h * 0.08f, w * 0.88f, h * 0.62f);
    g.setColour (col (0xfff1e7cf));
    g.fillRoundedRectangle (label, 1.5f);
    g.setColour (col (0xffc9552a));
    g.fillRect (label.getX(), label.getY() + label.getHeight() * 0.12f, label.getWidth(), label.getHeight() * 0.10f);
    g.setColour (col (0xffd9a23a));
    g.fillRect (label.getX(), label.getY() + label.getHeight() * 0.24f, label.getWidth(), label.getHeight() * 0.06f);
    if (! tiny)
        text (g, "TRAPWIN  C-60", { label.getX() + 3.0f, label.getY() + label.getHeight() * 0.0f - 1.0f, label.getWidth() - 6.0f, label.getHeight() * 0.14f + 2.0f },
              th::handFont (6.5f), col (0xff4a2e1c), Justification::centredLeft);

    // tape window with the tape packs
    auto win = Rectangle<float> (label.getX() + label.getWidth() * 0.16f, label.getY() + label.getHeight() * 0.42f,
                                 label.getWidth() * 0.68f, label.getHeight() * 0.46f);
    g.setColour (col (0xff1a1410));
    g.fillRoundedRectangle (win, win.getHeight() * 0.45f);
    const float hubR = win.getHeight() * 0.34f;
    const Point<float> lc (win.getX() + win.getHeight() * 0.5f, win.getCentreY()), rc (win.getRight() - win.getHeight() * 0.5f, win.getCentreY());
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (win.toNearestInt());
        g.setColour (col (0xff5a3720));
        g.fillEllipse (Rectangle<float> (win.getHeight() * 1.5f, win.getHeight() * 1.5f).withCentre (lc));
        g.setColour (col (0xff4a2d1a));
        g.fillEllipse (Rectangle<float> (win.getHeight() * 1.05f, win.getHeight() * 1.05f).withCentre (rc));
    }
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (win, win.getHeight() * 0.45f, 0.6f);
    reelHub (g, lc, hubR, angle);
    reelHub (g, rc, hubR, angle * 1.35f);

    // head opening trapezoid
    Path head;
    const float hy = r.getBottom() - h * 0.20f;
    head.startNewSubPath (r.getX() + w * 0.20f, r.getBottom());
    head.lineTo (r.getX() + w * 0.26f, hy);
    head.lineTo (r.getRight() - w * 0.26f, hy);
    head.lineTo (r.getRight() - w * 0.20f, r.getBottom());
    head.closeSubPath();
    g.setColour (col (0xff2a2826));
    g.fillPath (head);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (head, PathStrokeType (0.6f));
    if (! tiny)
        for (float fx : { 0.36f, 0.64f })
        {
            g.setColour (col (0xff0c0b0a));
            g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre ({ r.getX() + w * fx, hy + h * 0.10f }));
        }
}

void liveCassette (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    const float wow = s.params[4];
    float angle = 0.6f;
    if (! s.reduceMotion && s.powered)
    {
        const double t = s.timeSec;
        const double speed = 0.9 + 6.0 * wow;                       // rad/s
        angle = (float) std::fmod (t * speed + wow * 0.5 * std::sin (t * 3.1), MathConstants<double>::twoPi * 100.0);
    }

    const auto brown = col (0xff4a2e1c);
    if (mini)
    {
        window (g, a, col (0xff201a15), col (0xff0c0a08), 3.0f);
        cassette (g, Rectangle<float> (a.getHeight() * 1.6f, a.getHeight() - 4.0f).withCentre (a.getCentre()), angle, true);
        glassShine (g, a);
        return;
    }

    auto win = a.withWidth (128.0f);
    window (g, win, col (0xff261f19), col (0xff0b0907), 4.0f);
    cassette (g, Rectangle<float> (86.0f, 52.0f).withCentre (win.getCentre().translated (0.0f, 0.5f)), angle, false);
    glassShine (g, win, 4.0f);

    // mode lamps
    auto side = a.withTrimmedLeft (win.getWidth() + 14.0f);
    const bool tube = s.params[0] >= 0.5f;
    const auto amber = col (0xffffc861);
    auto row = [&] (int i, const char* label, bool on)
    {
        auto rr = side.withHeight (16.0f).translated (0.0f, 6.0f + (float) i * 18.0f);
        glowLed (g, { rr.getX() + 6.0f, rr.getCentreY() }, 3.4f, amber, on ? 1.0f : 0.0f);
        text (g, label, rr.withTrimmedLeft (16.0f), smallFont (9.0f).boldened(), brown, Justification::centredLeft);
    };
    row (0, "TAPE", ! tube);
    row (1, "TUBE", tube);
    text (g, "WOW " + String (roundToInt (wow * 100.0f)) + "%", side.withTrimmedTop (44.0f), smallFont (7.5f), brown.withAlpha (0.7f), Justification::centredLeft);
}

void liveMenace (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    const auto red = col (0xffff2e3e), white = col (0xffeceef2);
    const float drive = s.params[1];
    float lvl = drive;
    if (s.outLevel > 0.0f) lvl = drive * (0.7f + 0.3f * jmin (1.0f, s.outLevel * 1.5f));
    if (! s.powered) lvl = 0.0f;

    auto bezelLed = [&] (Point<float> c, float r)
    {
        ColourGradient ring (col (0xffd9dce2), c.x - r, c.y - r, col (0xff3b3e44), c.x + r, c.y + r, false);
        g.setGradientFill (ring);
        g.fillEllipse (Rectangle<float> (r * 3.1f, r * 3.1f).withCentre (c));
        g.setColour (Colours::black);
        g.fillEllipse (Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (c));
        glowLed (g, c, r, red, lvl);
    };

    if (mini)
    {
        bezelLed ({ a.getRight() - 12.0f, a.getCentreY() }, 5.0f);
        // drive ladder
        for (int i = 0; i < 8; ++i)
        {
            const bool on = (float) i / 8.0f < drive - 0.01f;
            g.setColour (on ? red : col (0xff3a1418));
            g.fillRect (a.getX() + 10.0f + (float) i * 7.0f, a.getCentreY() - 3.0f, 5.0f, 6.0f);
        }
        return;
    }

    const Point<float> c (a.getX() + 22.0f, a.getY() + 22.0f);
    bezelLed (c, 8.0f);
    text (g, "DRIVE", { c.x - 22.0f, a.getY() + 44.0f, 44.0f, 12.0f }, smallFont (8.5f).boldened(), white, Justification::centred);

    // type selector lamps
    const char* types[] = { "FUZZ", "CLIP", "FOLD", "RECT" };
    const int type = jlimit (0, 3, (int) std::round (s.params[0] * 3.0f));
    auto row = a.withTrimmedLeft (54.0f).withHeight (20.0f).translated (0.0f, 4.0f);
    const float cw = row.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = row.withX (row.getX() + (float) i * cw).withWidth (cw);
        th::drawLed (g, { cell.getX() + 5.0f, cell.getCentreY() }, 2.6f, red, i == type && s.powered, 0.7f);
        text (g, types[i], cell.withTrimmedLeft (12.0f), smallFont (8.5f), white.withAlpha (i == type ? 1.0f : 0.5f), Justification::centredLeft);
    }
    // drive ladder
    auto ladder = a.withTrimmedLeft (54.0f).withTrimmedTop (32.0f).withHeight (14.0f);
    window (g, ladder, col (0xff120607), col (0xff060203), 2.0f);
    const int n = 16;
    const float sw = (ladder.getWidth() - 6.0f) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float on = jlimit (0.0f, 1.0f, (lvl * (float) n - (float) i));
        auto seg = Rectangle<float> (ladder.getX() + 3.0f + (float) i * sw, ladder.getY() + 3.0f, sw - 2.0f, ladder.getHeight() - 6.0f);
        g.setColour (col (0xff3a1418));
        g.fillRect (seg);
        if (on > 0.0f)
        {
            g.setColour (red.interpolatedWith (col (0xffffb0a0), i > 12 ? 0.35f : 0.0f).withAlpha (on));
            g.fillRect (seg);
        }
    }
    glassShine (g, ladder, 2.0f);
}

void liveBrainrot (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    const auto green = col (0xff3dff6e), silk = col (0xfff2f5ee);
    const int bits = jlimit (4, 16, (int) std::round (realParam (EffectType::Brainrot, s, 0)));
    const float rate = realParam (EffectType::Brainrot, s, 1);
    const double t = (s.reduceMotion || ! s.powered) ? 0.0 : s.timeSec;

    auto bitColumns = [&] (Rectangle<float> r)
    {
        const float cw = r.getWidth() / 16.0f;
        for (int i = 0; i < 16; ++i) // MSB on the left
        {
            const bool active = i < bits;
            auto colR = Rectangle<float> (r.getX() + (float) i * cw + 0.6f, r.getY(), cw - 1.2f, r.getHeight());
            g.setColour (col (0xff0f2a18));
            g.fillRect (colR);
            if (! active) continue;
            const float flicker = 0.55f + 0.45f * (float) std::abs (std::sin (t * (2.0 + i * 0.37) + i * 1.3));
            const float hgt = colR.getHeight() * (s.reduceMotion ? 1.0f : flicker);
            g.setColour (green.withAlpha (0.85f));
            g.fillRect (colR.withTop (colR.getBottom() - hgt));
        }
    };

    if (mini)
    {
        window (g, a, col (0xff07170d), col (0xff030a06), 2.0f);
        bitColumns (a.reduced (4.0f, 4.0f));
        glassShine (g, a, 2.0f);
        return;
    }

    auto disp = a.withHeight (44.0f);
    window (g, disp, col (0xff07170d), col (0xff030a06), 3.0f);
    auto inner = disp.reduced (5.0f, 4.0f);
    auto waveR = inner.removeFromLeft (118.0f);
    inner.removeFromLeft (6.0f);
    grid (g, waveR, green.withAlpha (0.08f), 8, 4);

    // quantised sine: amplitude levels from BITS, hold width from RATE
    const float levels = std::pow (2.0f, (float) (bits - 1) * 0.55f);
    const float hold = jlimit (1.0f, 14.0f, 44100.0f / jmax (1000.0f, rate) * 1.2f);
    Path wave;
    const float phase = (float) (t * 2.2);
    for (float x = 0.0f; x <= waveR.getWidth(); x += 1.0f)
    {
        const float xs = std::floor (x / hold) * hold;
        float v = std::sin (xs / waveR.getWidth() * MathConstants<float>::twoPi * 1.5f + phase);
        v = std::round (v * levels) / levels;
        const float y = waveR.getCentreY() - v * waveR.getHeight() * 0.42f;
        if (x == 0.0f) wave.startNewSubPath (waveR.getX(), y);
        else wave.lineTo (waveR.getX() + x, y);
    }
    g.setColour (green.withAlpha (0.25f));
    g.strokePath (wave, PathStrokeType (3.0f));
    g.setColour (green);
    g.strokePath (wave, PathStrokeType (1.1f));

    bitColumns (inner);
    glassShine (g, disp, 3.0f);
    text (g, String (bits) + "-BIT  \xc2\xb7  " + String (rate / 1000.0f, rate < 10000.0f ? 1 : 0) + " kHz",
          a.withTrimmedTop (46.0f), smallFont (8.0f), silk, Justification::centredLeft);
    text (g, "MSB  >  LSB", a.withTrimmedTop (46.0f), smallFont (7.0f), silk.withAlpha (0.6f), Justification::centredRight);
}

float filterDb (int type, float f, float fc, float reso, float widthOct)
{
    const float x = f / fc;
    const float q = 0.55f + reso * 8.0f;
    auto denom = [&] (float qq) { return std::sqrt ((1.0f - x * x) * (1.0f - x * x) + (x / qq) * (x / qq)); };
    float m = 1.0f;
    switch (type)
    {
        case 0: m = 1.0f / denom (q); break;
        case 1: m = 1.0f / (denom (q) * denom (0.707f)); break;
        case 2: m = x * x / denom (q); break;
        case 3:
        {
            const float bw = std::pow (2.0f, widthOct);
            const float qb = std::sqrt (bw) / (bw - 1.0f) * (1.0f + reso * 3.0f);
            m = (x / qb) / denom (qb) * (1.0f + reso * 2.0f);
            break;
        }
        case 4: m = std::abs (1.0f - x * x) / denom (0.5f + reso); break;
        default:
        {
            const float gfb = 0.2f + reso * 0.75f;
            m = (1.0f - gfb) / std::sqrt (1.0f + gfb * gfb - 2.0f * gfb * std::cos (MathConstants<float>::twoPi * x));
            break;
        }
    }
    return 20.0f * std::log10 (jmax (1.0e-4f, m));
}

void curveDisplay (Graphics& g, Rectangle<float> r, Colour lineC, float dbMin, float dbMax, std::function<float (float)> dbAt,
                   bool fill, float zeroDb)
{
    Path p, area;
    const int n = jmax (24, (int) r.getWidth());
    auto yOf = [&] (float db) { return jmap (jlimit (dbMin, dbMax, db), dbMin, dbMax, r.getBottom(), r.getY()); };
    for (int i = 0; i <= n; ++i)
    {
        const float fx = (float) i / (float) n;
        const float f = 20.0f * std::pow (1000.0f, fx);
        const float y = yOf (dbAt (f));
        const float x = r.getX() + fx * r.getWidth();
        if (i == 0) { p.startNewSubPath (x, y); area.startNewSubPath (x, yOf (zeroDb)); }
        p.lineTo (x, y);
        area.lineTo (x, y);
    }
    area.lineTo (r.getRight(), yOf (zeroDb));
    area.closeSubPath();
    if (fill)
    {
        ColourGradient gr (lineC.withAlpha (0.35f), 0.0f, r.getY(), lineC.withAlpha (0.02f), 0.0f, r.getBottom(), false);
        g.setGradientFill (gr);
        g.fillPath (area);
    }
    g.setColour (lineC.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (lineC);
    g.strokePath (p, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
}

float xOfFreq (Rectangle<float> r, float f) { return r.getX() + r.getWidth() * std::log (jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f); }

void liveThroughTheWall (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    const auto ice = col (0xff9fc3ff), white = col (0xfff0f3fa);
    const int type = (int) realParam (EffectType::ThroughTheWall, s, 0);
    const float fc = realParam (EffectType::ThroughTheWall, s, 1);
    const float reso = realParam (EffectType::ThroughTheWall, s, 2);
    const float width = realParam (EffectType::ThroughTheWall, s, 7);
    auto dbAt = [&] (float f) { return filterDb (type, f, fc, reso, width); };

    auto disp = mini ? a : a.withHeight (46.0f);
    window (g, disp, col (0xff0b1530), col (0xff050a18), 3.0f);
    auto plot = disp.reduced (4.0f, 4.0f);
    grid (g, plot, ice.withAlpha (0.08f), mini ? 4 : 10, mini ? 2 : 4);
    curveDisplay (g, plot, ice, -30.0f, 18.0f, dbAt, true, -30.0f);
    if (! mini)
    {
        const float cx = xOfFreq (plot, fc);
        const float cy = jmap (jlimit (-30.0f, 18.0f, dbAt (fc)), -30.0f, 18.0f, plot.getBottom(), plot.getY());
        g.setColour (white);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ cx, cy }));
        g.setColour (ice.withAlpha (0.3f));
        g.fillRect (cx - 0.4f, plot.getY(), 0.8f, plot.getHeight());
        const char* names[] = { "LP12", "LP24", "HP", "BP", "NOTCH", "COMB" };
        text (g, names[jlimit (0, 5, type)], plot.withHeight (10.0f).reduced (2.0f, 0.0f), smallFont (7.5f), white.withAlpha (0.8f), Justification::centredRight);
        const String fs = fc >= 1000.0f ? String (fc / 1000.0f, fc < 10000.0f ? 2 : 1) + " kHz" : String (roundToInt (fc)) + " Hz";
        text (g, "CUTOFF " + fs + "   RESO " + String (roundToInt (reso * 100.0f)) + "%", a.withTrimmedTop (48.0f), smallFont (8.0f), white.withAlpha (0.75f), Justification::centredLeft);
    }
    glassShine (g, disp, 3.0f);
}

void liveToneUp (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    auto rp = [&] (int i) { return realParam (EffectType::ToneUp, s, i); };
    const float lowCut = rp (0), low = rp (1), lmG = rp (2), lmF = rp (3), hmG = rp (4), hmF = rp (5), high = rp (6), tilt = rp (7);
    auto dbAt = [=] (float f)
    {
        float db = 0.0f;
        if (lowCut > 11.0f)
        {
            const float x = f / lowCut;
            db += 20.0f * std::log10 (jmax (1.0e-4f, x * x / std::sqrt ((1.0f - x * x) * (1.0f - x * x) + 2.0f * x * x)));
        }
        { const float x2 = (f / 100.0f) * (f / 100.0f); db += low / (1.0f + x2); }
        { const float x2 = (f / 8000.0f) * (f / 8000.0f); db += high * x2 / (1.0f + x2); }
        auto peak = [&] (float gain, float fc) { const float x = f / fc, k = 1.1f * (x - 1.0f / x); return gain / (1.0f + k * k); };
        db += peak (lmG, lmF) + peak (hmG, hmF);
        db += tilt * jlimit (-1.0f, 1.0f, std::log10 (f / 1000.0f) / 1.3f);
        return db;
    };

    const Colour bands[] = { col (0xffd8352f), col (0xffeb8a24), col (0xffe9c429), col (0xff35a85a), col (0xff2f6fd6) };
    auto disp = mini ? a : a.withHeight (46.0f);
    window (g, disp, col (0xff15181d), col (0xff07080a), 3.0f);
    auto plot = disp.reduced (4.0f, 4.0f);
    grid (g, plot, Colours::white.withAlpha (0.06f), mini ? 4 : 10, 4);
    g.setColour (Colours::white.withAlpha (0.16f));
    g.fillRect (plot.getX(), plot.getCentreY() - 0.4f, plot.getWidth(), 0.8f);
    curveDisplay (g, plot, col (0xfff4f6fa), -18.0f, 18.0f, dbAt, true, 0.0f);
    if (! mini)
    {
        const float fs[] = { jmax (20.0f, lowCut), 100.0f, lmF, hmF, 8000.0f };
        for (int i = 0; i < 5; ++i)
        {
            const float x = xOfFreq (plot, fs[i]);
            const float y = jmap (jlimit (-18.0f, 18.0f, dbAt (fs[i])), -18.0f, 18.0f, plot.getBottom(), plot.getY());
            g.setColour (Colours::black);
            g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }));
            g.setColour (bands[i]);
            g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ x, y }));
        }
        text (g, "+18", plot.withHeight (9.0f), smallFont (6.5f), Colours::white.withAlpha (0.35f), Justification::topLeft);
        text (g, "-18", plot.withTop (plot.getBottom() - 9.0f), smallFont (6.5f), Colours::white.withAlpha (0.35f), Justification::bottomLeft);
        text (g, "TILT " + String (tilt, 1) + " dB", a.withTrimmedTop (48.0f), smallFont (8.0f), col (0xff15171b), Justification::centredLeft);
        text (g, "RESPONSE", a.withTrimmedTop (48.0f), smallFont (7.0f), col (0xff15171b).withAlpha (0.6f), Justification::centredRight);
    }
    glassShine (g, disp, 3.0f);
}

void liveSqueeze (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    const auto green = col (0xff3dff6e), light = col (0xffdfe3e8);
    const float gr = jlimit (0.0f, 20.0f, s.gainReductionDb);

    auto meter = [&] (Rectangle<float> m, bool tiny)
    {
        window (g, m, col (0xff0d0f0e), col (0xff0d0f0e), 3.0f);
        auto face = m.reduced (2.0f);
        ColourGradient bg (col (0xfff2eed8), face.getCentreX(), face.getBottom(), col (0xffc8c0a0), face.getCentreX(), face.getY() - face.getHeight() * 0.3f, true);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (face, 2.0f);
        // backlight tint
        g.setColour (green.withAlpha (0.06f));
        g.fillRoundedRectangle (face, 2.0f);

        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (face.toNearestInt());
        const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * (tiny ? 0.55f : 0.45f));
        const float rad = face.getHeight() * (tiny ? 1.25f : 1.15f);
        const float a0 = -0.78f, a1 = 0.78f; // 20 dB GR on the left .. 0 on the right
        auto angOf = [&] (float db) { return jmap (std::sqrt (db / 20.0f), 1.0f, 0.0f, a0, a1); };

        // scale arc
        Path arc;
        arc.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, a0, a1, true);
        g.setColour (col (0xff1c1c1a));
        g.strokePath (arc, PathStrokeType (tiny ? 0.7f : 0.9f));
        Path redArc;
        redArc.addCentredArc (pivot.x, pivot.y, rad + 2.0f, rad + 2.0f, 0.0f, a0, angOf (10.0f), true);
        g.setColour (col (0xffc8202c));
        g.strokePath (redArc, PathStrokeType (tiny ? 1.4f : 2.2f));

        const float marks[] = { 0, 1, 2, 3, 5, 7, 10, 15, 20 };
        for (float db : marks)
        {
            const float an = angOf (db);
            const auto p0 = pivot.getPointOnCircumference (rad, an), p1 = pivot.getPointOnCircumference (rad - (tiny ? 3.0f : 5.0f), an);
            g.setColour (col (0xff1c1c1a));
            g.drawLine ({ p0, p1 }, 0.8f);
            if (! tiny && (db == 0 || db == 2 || db == 5 || db == 10 || db == 20))
            {
                const auto pt = pivot.getPointOnCircumference (rad - 11.0f, an);
                text (g, String ((int) db), Rectangle<float> (16.0f, 9.0f).withCentre (pt), smallFont (7.0f), col (0xff1c1c1a), Justification::centred);
            }
        }
        if (! tiny)
            text (g, "GR dB", Rectangle<float> (40.0f, 9.0f).withCentre ({ face.getCentreX(), face.getBottom() - 8.0f }), smallFont (7.0f).boldened(), col (0xff1c1c1a), Justification::centred);

        // needle
        const float an = angOf (gr);
        const auto tip = pivot.getPointOnCircumference (rad + 1.0f, an);
        const auto base = pivot.getPointOnCircumference (rad * 0.35f, an);
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawLine ({ base.translated (1.2f, 1.2f), tip.translated (1.2f, 1.2f) }, 1.4f);
        g.setColour (col (0xff111111));
        g.drawLine ({ base, tip }, tiny ? 0.9f : 1.2f);
    };

    if (mini)
    {
        meter (a.withSizeKeepingCentre (56.0f, a.getHeight()), true);
        glassShine (g, a.withSizeKeepingCentre (56.0f, a.getHeight()), 3.0f);
        return;
    }

    auto m = a.withWidth (124.0f);
    meter (m, false);
    // pivot cap bezel strip under the face
    glassShine (g, m, 3.0f);

    auto side = a.withTrimmedLeft (m.getWidth() + 14.0f);
    glowLed (g, { side.getX() + 6.0f, side.getY() + 12.0f }, 3.4f, green, s.powered ? jlimit (0.0f, 1.0f, gr / 3.0f) : 0.0f);
    text (g, "COMP", { side.getX() + 16.0f, side.getY() + 5.0f, side.getWidth() - 16.0f, 14.0f }, smallFont (9.0f).boldened(), light, Justification::centredLeft);
    text (g, String (gr, 1) + " dB", { side.getX(), side.getY() + 24.0f, side.getWidth(), 14.0f }, smallFont (9.0f), green, Justification::centredLeft);
    text (g, "GAIN REDUCTION", { side.getX(), side.getY() + 40.0f, side.getWidth(), 12.0f }, smallFont (7.0f), light.withAlpha (0.55f), Justification::centredLeft);
}

void liveOvercooked (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    const auto dark = col (0xff2a1206), cream = col (0xfffff0d8);
    auto segColour = [] (int i, int n)
    {
        const float f = (float) i / (float) (n - 1);
        return f < 0.6f ? col (0xff3dff6e) : (f < 0.85f ? col (0xffffc861) : col (0xffff2e3e));
    };

    auto columns = [&] (Rectangle<float> r, int nSeg, bool labels)
    {
        const char* names[] = { "LOW", "MID", "HIGH" };
        const float cw = r.getWidth() / 3.0f;
        for (int b = 0; b < 3; ++b)
        {
            auto c = r.withX (r.getX() + (float) b * cw).withWidth (cw).reduced (cw * 0.2f, 0.0f);
            auto lab = labels ? c.removeFromBottom (10.0f) : Rectangle<float>();
            const float sh = c.getHeight() / (float) nSeg;
            const float v = s.powered ? jlimit (0.0f, 1.0f, s.bands[(size_t) b]) : 0.0f;
            for (int i = 0; i < nSeg; ++i)
            {
                auto seg = Rectangle<float> (c.getX(), c.getBottom() - (float) (i + 1) * sh + 0.6f, c.getWidth(), sh - 1.2f);
                const auto sc = segColour (i, nSeg);
                const float on = jlimit (0.0f, 1.0f, v * (float) nSeg - (float) i);
                g.setColour (sc.withMultipliedBrightness (0.22f).withMultipliedSaturation (0.6f));
                g.fillRect (seg);
                if (on > 0.0f)
                {
                    g.setColour (sc.withAlpha (0.25f * on));
                    g.fillRect (seg.expanded (1.5f, 1.0f));
                    g.setColour (sc.withAlpha (on));
                    g.fillRect (seg);
                }
            }
            if (labels) text (g, names[b], lab.expanded (6.0f, 0.0f), smallFont (7.0f), cream.withAlpha (0.8f), Justification::centredBottom);
        }
    };

    if (mini)
    {
        window (g, a, col (0xff140804), col (0xff070302), 2.0f);
        columns (a.reduced (2.0f, 3.0f), 6, false);
        glassShine (g, a, 2.0f);
        return;
    }

    auto w = a.withWidth (104.0f);
    window (g, w, col (0xff140804), col (0xff070302), 3.0f);
    columns (w.reduced (4.0f, 4.0f), 9, true);
    glassShine (g, w, 3.0f);

    auto side = a.withTrimmedLeft (w.getWidth() + 14.0f);
    text (g, "HEAT", side.withHeight (14.0f).translated (0.0f, 2.0f), nameFont (13.0f), dark, Justification::centredLeft);
    text (g, "UPWARD + DOWNWARD", side.withHeight (10.0f).translated (0.0f, 20.0f), smallFont (7.0f), dark.withAlpha (0.8f), Justification::centredLeft);
    text (g, "LOW / MID / HIGH", side.withHeight (10.0f).translated (0.0f, 31.0f), smallFont (7.0f), dark.withAlpha (0.8f), Justification::centredLeft);
    // thermometer-style bar of overall activity
    const float avg = s.powered ? (s.bands[0] + s.bands[1] + s.bands[2]) / 3.0f : 0.0f;
    auto bar = Rectangle<float> (side.getX(), side.getY() + 46.0f, side.getWidth() - 4.0f, 6.0f);
    g.setColour (dark.withAlpha (0.85f));
    g.fillRoundedRectangle (bar, 3.0f);
    ColourGradient heat (col (0xffffc861), bar.getX(), 0.0f, col (0xffff2e3e), bar.getRight(), 0.0f, false);
    g.setGradientFill (heat);
    g.fillRoundedRectangle (bar.reduced (1.0f).withWidth ((bar.getWidth() - 2.0f) * jlimit (0.0f, 1.0f, avg)), 2.0f);
}

void liveSlap (Graphics& g, Rectangle<float> a, const State& s, bool mini)
{
    const auto yellow = col (0xffe6c229), black = col (0xff0e0e0e), red = col (0xffff2e3e);
    const bool hit = s.transientHit && s.powered;

    auto starburst = [&] (Point<float> c, float r1, float r2, int n, Colour cc)
    {
        Path st;
        for (int i = 0; i < n * 2; ++i)
        {
            const float an = (float) i * MathConstants<float>::pi / (float) n;
            const float r = (i & 1) ? r1 : r2 * (0.85f + 0.15f * (float) ((i * 7) % 3) / 2.0f);
            const auto pt = c.getPointOnCircumference (r, an);
            if (i == 0) st.startNewSubPath (pt); else st.lineTo (pt);
        }
        st.closeSubPath();
        g.setColour (cc);
        g.fillPath (st);
    };

    if (mini)
    {
        const Point<float> c (a.getRight() - 14.0f, a.getCentreY());
        starburst (c, 7.0f, 13.0f, 9, black);
        glowLed (g, c, 4.2f, red, hit ? 1.0f : 0.0f);
        // tiny envelope
        auto e = a.withTrimmedRight (34.0f).reduced (4.0f, 4.0f);
        window (g, e, col (0xff141414), col (0xff070707), 2.0f);
        Path env;
        const float atk = s.params[0], sus = s.params[1];
        env.startNewSubPath (e.getX() + 2.0f, e.getBottom() - 2.0f);
        env.lineTo (e.getX() + 6.0f, e.getY() + 2.0f + (1.0f - atk) * e.getHeight() * 0.4f);
        env.quadraticTo (e.getX() + 10.0f, e.getBottom() - e.getHeight() * (0.2f + 0.5f * sus),
                         e.getRight() - 2.0f, e.getBottom() - 2.0f);
        g.setColour (yellow);
        g.strokePath (env, PathStrokeType (1.1f));
        return;
    }

    // envelope screen: spike height = ATTACK, tail = SUSTAIN (both bipolar)
    auto scr = a.withWidth (128.0f);
    window (g, scr, col (0xff161616), col (0xff060606), 3.0f);
    auto plot = scr.reduced (6.0f, 6.0f);
    grid (g, plot, yellow.withAlpha (0.07f), 8, 3);
    const float atk = s.params[0] * 2.0f - 1.0f, sus = s.params[1] * 2.0f - 1.0f;
    auto envAt = [&] (float x, float a1, float s1) // x 0..1
    {
        const float rise = 0.06f;
        if (x < rise) return x / rise * (0.62f + 0.38f * a1);
        const float peak = 0.62f + 0.38f * a1;
        const float floorV = 0.30f + 0.22f * s1;
        const float k = (x - rise) / (1.0f - rise);
        const float decay = std::exp (-k * (6.0f - 3.5f * s1));
        return floorV * (1.0f - k) * (1.0f - decay) + peak * decay;
    };
    for (int pass = 0; pass < 2; ++pass)
    {
        Path env;
        for (int i = 0; i <= (int) plot.getWidth(); ++i)
        {
            const float x = (float) i / plot.getWidth();
            const float v = pass == 0 ? envAt (x, 0.0f, 0.0f) : envAt (x, atk, sus);
            const auto pt = Point<float> (plot.getX() + x * plot.getWidth(), plot.getBottom() - v * plot.getHeight());
            if (i == 0) env.startNewSubPath (pt); else env.lineTo (pt);
        }
        if (pass == 0)
        {
            g.setColour (Colours::white.withAlpha (0.22f));
            g.strokePath (env, PathStrokeType (0.8f));
        }
        else
        {
            g.setColour (yellow.withAlpha (0.25f));
            g.strokePath (env, PathStrokeType (3.0f));
            g.setColour (yellow);
            g.strokePath (env, PathStrokeType (1.3f));
        }
    }
    glassShine (g, scr, 3.0f);

    // HIT lamp on a printed starburst
    const Point<float> c (a.getRight() - 40.0f, a.getCentreY() - 2.0f);
    starburst (c, 15.0f, 27.0f, 11, black);
    starburst (c, 10.0f, 18.0f, 11, hit ? col (0xffff6a3a) : yellow.darker (0.05f));
    glowLed (g, c, 6.0f, red, hit ? 1.0f : 0.0f);
    text (g, "HIT", { c.x + 18.0f, c.y + 12.0f, 26.0f, 12.0f }, nameFont (11.0f), black, Justification::centredLeft);
}

void paintLive (Graphics& g, EffectType t, Rectangle<float> p, const State& s, bool mini)
{
    const auto a = mini ? miniLive (p) : liveArea (t, p);
    switch (t)
    {
        case EffectType::CassettePlug:   liveCassette (g, a, s, mini); break;
        case EffectType::Menace:         liveMenace (g, a, s, mini); break;
        case EffectType::Brainrot:       liveBrainrot (g, a, s, mini); break;
        case EffectType::ThroughTheWall: liveThroughTheWall (g, a, s, mini); break;
        case EffectType::ToneUp:         liveToneUp (g, a, s, mini); break;
        case EffectType::Squeeze:        liveSqueeze (g, a, s, mini); break;
        case EffectType::Overcooked:     liveOvercooked (g, a, s, mini); break;
        case EffectType::Slap:           liveSlap (g, a, s, mini); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------
// static layer cache (UI thread only)
// ---------------------------------------------------------------------------------------------
const Image& staticLayer (EffectType t, Rectangle<float> plate, bool mini, float scale)
{
    static std::map<String, Image> cache;
    const int w = jmax (1, roundToInt (plate.getWidth() * scale)), h = jmax (1, roundToInt (plate.getHeight() * scale));
    const String key = String ((int) t) + (mini ? "m" : "f") + String (w) + "x" + String (h);
    auto it = cache.find (key);
    if (it != cache.end()) return it->second;

    if (cache.size() > 64) cache.clear();
    Image img (Image::ARGB, w, h, true);
    {
        Graphics ig (img);
        ig.addTransform (AffineTransform::scale ((float) w / plate.getWidth(), (float) h / plate.getHeight()));
        paintStatic (ig, t, { 0.0f, 0.0f, plate.getWidth(), plate.getHeight() }, mini);
    }
    return cache.emplace (key, img).first->second;
}
} // namespace

Palette paletteColorGroup (EffectType t)
{
    switch (t)
    {
        case EffectType::CassettePlug:   return { col (0xffe8dcc2), col (0xffc9b48f), col (0xff4a2e1c), col (0xffc9552a), col (0xffffc861) };
        case EffectType::Menace:         return { col (0xff161618), col (0xff0a0a0b), col (0xffeceef2), col (0xffff2e3e), col (0xffff2e3e) };
        case EffectType::Brainrot:       return { col (0xff1d6a3b), col (0xff0f3d22), col (0xfff2f5ee), col (0xffd6b25a), col (0xff3dff6e) };
        case EffectType::ThroughTheWall: return { col (0xff1d2c55), col (0xff111a36), col (0xfff0f3fa), col (0xff9fc3ff), col (0xff9fc3ff) };
        case EffectType::ToneUp:         return { col (0xffc9ced6), col (0xff9aa1ad), col (0xff15171b), col (0xff2f6fd6), col (0xff3dff6e) };
        case EffectType::Squeeze:        return { col (0xff24262b), col (0xff16171a), col (0xffdfe3e8), col (0xff3dff6e), col (0xff3dff6e) };
        case EffectType::Overcooked:     return { col (0xffd9661f), col (0xffa8481a), col (0xff2a1206), col (0xfffff0d8), col (0xffffc861) };
        case EffectType::Slap:           return { col (0xffe6c229), col (0xffb39414), col (0xff0e0e0e), col (0xff0e0e0e), col (0xffff2e3e) };
        default:                         return { col (0xff24262b), col (0xff15161a), Colours::white, col (0xff9fc3ff), col (0xff3dff6e) };
    }
}

void paintColorGroup (Graphics& g, EffectType t, Rectangle<float> plate, const State& s, bool miniature)
{
    if (plate.isEmpty()) return;
    const float scale = jlimit (1.0f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto& img = staticLayer (t, plate, miniature, scale);
    g.drawImage (img, plate, RectanglePlacement::stretchToFit);
    paintLive (g, t, plate, s, miniature);
}
} // namespace ek::faceplate
