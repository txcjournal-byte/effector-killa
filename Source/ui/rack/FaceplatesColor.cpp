// Hardware faceplates of the "colour" group: Cassette Plug, Menace, Brainrot, Through The Wall, Tone Up,
// Squeeze, Overcooked and Slap (DESIGN-RACK.md section 6). Original panel designs, no product copies.
//
// Drawing is split in two layers:
//  - a static layer (material, texture, ears, screws, name sticker, printing, LCD glass) that is rendered
//    once per type / size / pixel scale into an image cache, so the per-frame cost stays tiny;
//  - the live element (reels, LEDs, meters, curves) that is drawn every frame from the State snapshot.
// knobArea() is left calm on purpose: RackView places the real knob components on top of it.
#include "Faceplates.h"
#include "../Theme.h"
#include <functional>
#include <map>

namespace ek::faceplate
{
namespace
{
using namespace juce;
namespace th = ek::theme;

// ---------------------------------------------------------------------------------------------
// identity / palette
// ---------------------------------------------------------------------------------------------
Colour col (uint32 argb) { return Colour (argb); }

struct Look
{
    Colour plate, plateDark, ink, accent, led;
    const char* name;       // big name on the panel
    const char* type;       // small category print under the name
    const char* model;      // engraved model line above the knob row
    bool lightPanel;        // dark ink on a light panel
};

Look look (EffectType t)
{
    switch (t)
    {
        case EffectType::CassettePlug:   return { col (0xffe8dcc2), col (0xffc9b48f), col (0xff4a2e1c), col (0xffc9552a), col (0xffffc861), "CASSETTE PLUG",    "SATURATION", "TW-01  TAPE / TUBE DRIVE",       true };
        case EffectType::Menace:         return { col (0xff161618), col (0xff0a0a0b), col (0xffeceef2), col (0xffd8222f), col (0xffff2e3e), "MENACE",           "DISTORTION", "TW-02  FOUR-MODE DISTORTION",    false };
        case EffectType::Brainrot:       return { col (0xff1d6a3b), col (0xff0f3d22), col (0xfff2f5ee), col (0xffd6b25a), col (0xff3dff6e), "BRAINROT",         "LO-FI",      "TW-03  BIT / RATE REDUCER",      false };
        case EffectType::ThroughTheWall: return { col (0xff1d2c55), col (0xff111a36), col (0xfff0f3fa), col (0xff9fc3ff), col (0xff9fc3ff), "THROUGH THE WALL", "FILTER",     "TW-04  MULTIMODE FILTER",        false };
        case EffectType::ToneUp:         return { col (0xffc9ced6), col (0xff9aa1ad), col (0xff15171b), col (0xff2f6fd6), col (0xff3dff6e), "TONE UP",          "EQUALIZER",  "TW-05  5-BAND EQUALIZER",        true };
        case EffectType::Squeeze:        return { col (0xff24262b), col (0xff16171a), col (0xffdfe3e8), col (0xff3dff6e), col (0xff3dff6e), "SQUEEZE",          "COMPRESSOR", "TW-06  PARALLEL COMPRESSOR",     false };
        case EffectType::Overcooked:     return { col (0xffd9661f), col (0xffa8481a), col (0xff2a1206), col (0xfffff0d8), col (0xffffc861), "OVERCOOKED",       "MULTIBAND",  "TW-07  3-BAND UP / DOWN DYNAMICS", true };
        case EffectType::Slap:           return { col (0xffe6c229), col (0xffb39414), col (0xff0e0e0e), col (0xff0e0e0e), col (0xffff2e3e), "SLAP!",            "TRANSIENT",  "TW-08  TRANSIENT SHAPER",        true };
        default:                         return { col (0xff24262b), col (0xff15161a), Colours::white, col (0xff9fc3ff), col (0xff3dff6e), "", "", "", false };
    }
}

float realParam (EffectType t, const State& s, int i)
{
    return effectInfo (t).params[(size_t) i].toReal (s.params[(size_t) i]);
}

const String& midDot()
{
    static const String d (CharPointer_UTF8 ("\xc2\xb7"));
    return d;
}

// ---------------------------------------------------------------------------------------------
// text helpers
// ---------------------------------------------------------------------------------------------
Font nameFont (float h) { return th::stencilFont (h); }
Font smallFont (float h) { return th::monoLabel (h); }

// font squeezed horizontally so that `s` fits into maxW
Font fitted (Font f, const String& s, float maxW)
{
    const float w = GlyphArrangement::getStringWidth (f, s);
    if (w > maxW && w > 0.0f) f = f.withHorizontalScale (f.getHorizontalScale() * maxW / w);
    return f;
}

void text (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j)
{
    g.setFont (f);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

// "printed" text with a soft debossed edge (light below on light panels, dark above on dark ones)
void printed (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j, bool lightPanel)
{
    g.setFont (f);
    g.setColour ((lightPanel ? Colours::white : Colours::black).withAlpha (lightPanel ? 0.40f : 0.55f));
    g.drawText (s, r.translated (0.0f, lightPanel ? 0.8f : 1.0f), j, false);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

Path textPath (const String& s, Font f, float x, float baseline)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, x, baseline);
    Path p;
    ga.createPath (p);
    return p;
}

// name glyphs centred (or left aligned) in a box; returns the outline so callers can style it
Path namePath (const String& s, Font f, Rectangle<float> box, bool centred, float capRatio = 0.36f)
{
    const float w = GlyphArrangement::getStringWidth (f, s);
    const float x = centred ? box.getCentreX() - w * 0.5f : box.getX();
    return textPath (s, f, x, box.getCentreY() + f.getHeight() * capRatio);
}

// ---------------------------------------------------------------------------------------------
// materials
// ---------------------------------------------------------------------------------------------
void lacquer (Graphics& g, Rectangle<float> r, Colour c, float gloss)
{
    ColourGradient gr (c.brighter (0.12f), 0.0f, r.getY(), c.darker (0.22f), 0.0f, r.getBottom(), false);
    gr.addColour (0.18, c.brighter (0.04f));
    gr.addColour (0.65, c);
    g.setGradientFill (gr);
    g.fillRect (r);

    if (gloss > 0.0f)
    {
        // broad soft clear-coat sheen across the upper left + a diagonal specular streak
        ColourGradient sheen (Colours::white.withAlpha (0.13f * gloss), r.getX() + r.getWidth() * 0.18f, r.getY(),
                              Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.42f, r.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRect (r);
        Path streak;
        const float w = r.getWidth(), h = r.getHeight();
        streak.startNewSubPath (r.getX() + w * 0.36f, r.getY());
        streak.lineTo (r.getX() + w * 0.43f, r.getY());
        streak.lineTo (r.getX() + w * 0.43f - h * 0.9f, r.getBottom());
        streak.lineTo (r.getX() + w * 0.36f - h * 0.9f, r.getBottom());
        streak.closeSubPath();
        ColourGradient sg (Colours::white.withAlpha (0.06f * gloss), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getBottom(), false);
        g.setGradientFill (sg);
        g.fillPath (streak);
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
        for (int k = 0; k < 2; ++k)
        {
            const float x0 = r.getX() + rnd.nextFloat() * r.getWidth() * 0.8f - r.getWidth() * 0.2f;
            const float len = r.getWidth() * (0.25f + rnd.nextFloat() * 0.8f);
            const float a = rnd.nextFloat() * alpha;
            g.setColour ((rnd.nextFloat() < 0.55f ? Colours::white : Colours::black).withAlpha (a));
            const float xa = jmax (r.getX(), x0), xb = jmin (r.getRight(), x0 + len);
            if (xb > xa) g.fillRect (xa, y, xb - xa, step * 0.6f);
        }
    }
}

void hazardStripes (Graphics& g, Rectangle<float> b, float period, Colour red, int seed)
{
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (b.toNearestInt());
    g.setColour (red);
    g.fillRect (b);
    g.setColour (col (0xff0c0c0d));
    for (float x = b.getX() - b.getHeight() - period; x < b.getRight() + b.getHeight(); x += period)
    {
        Path s;
        s.startNewSubPath (x, b.getBottom());
        s.lineTo (x + b.getHeight(), b.getY());
        s.lineTo (x + b.getHeight() + period * 0.5f, b.getY());
        s.lineTo (x + period * 0.5f, b.getBottom());
        s.closeSubPath();
        g.fillPath (s);
    }
    // worn paint
    speckle (g, b, seed, (int) (b.getWidth() * b.getHeight() / 8.0f), Colours::white, Colours::black, 0.22f, 0.9f);
}

void pcbTraces (Graphics& g, Rectangle<float> r, int seed, int count, float alpha)
{
    Random rnd (seed);
    const auto trace = col (0xff3fb466).withAlpha (alpha);
    const auto gold = col (0xffd6b25a);
    for (int i = 0; i < count; ++i)
    {
        Path path;
        float x = r.getX() + rnd.nextFloat() * r.getWidth();
        float y = r.getY() + rnd.nextFloat() * r.getHeight();
        const Point<float> start (x, y);
        path.startNewSubPath (x, y);
        const int segs = 2 + rnd.nextInt (3);
        bool horiz = rnd.nextBool();
        for (int s = 0; s < segs; ++s)
        {
            const float len = 10.0f + rnd.nextFloat() * 60.0f;
            const float dir = rnd.nextBool() ? 1.0f : -1.0f;
            if (horiz) x = jlimit (r.getX(), r.getRight(), x + dir * len);
            else       y = jlimit (r.getY(), r.getBottom(), y + dir * len * 0.4f);
            path.lineTo (x, y);
            if (s + 1 < segs) // 45 degree chamfer into the next segment
            {
                const float d = 4.0f;
                x = jlimit (r.getX(), r.getRight(), x + (rnd.nextBool() ? d : -d));
                y = jlimit (r.getY(), r.getBottom(), y + (rnd.nextBool() ? d : -d));
                path.lineTo (x, y);
            }
            horiz = ! horiz;
        }
        // copper under solder mask: darker edge, lighter core
        g.setColour (col (0xff0b2a16).withAlpha (alpha * 0.8f));
        g.strokePath (path, PathStrokeType (2.2f, PathStrokeType::mitered, PathStrokeType::rounded), AffineTransform::translation (0.0f, 0.6f));
        g.setColour (trace);
        g.strokePath (path, PathStrokeType (1.4f, PathStrokeType::mitered, PathStrokeType::rounded));
        for (auto pt : { start, Point<float> (x, y) })   // pads / vias at both ends
        {
            g.setColour (gold.withAlpha (jmin (1.0f, alpha * 1.4f)));
            g.fillEllipse (Rectangle<float> (4.2f, 4.2f).withCentre (pt));
            g.setColour (col (0xff0d2a17).withAlpha (jmin (1.0f, alpha + 0.3f)));
            g.fillEllipse (Rectangle<float> (1.6f, 1.6f).withCentre (pt));
        }
    }
}

void halftone (Graphics& g, Rectangle<float> r, Point<float> focus, float radius, float step, Colour c)
{
    g.setColour (c);
    int row = 0;
    for (float y = r.getY(); y < r.getBottom(); y += step, ++row)
        for (float x = r.getX() + ((row & 1) ? step * 0.5f : 0.0f); x < r.getRight(); x += step)
        {
            const float d = focus.getDistanceFrom ({ x, y }) / radius;
            if (d >= 1.0f) continue;
            const float s = step * 0.62f * (1.0f - d);
            g.fillEllipse (Rectangle<float> (s, s).withCentre ({ x, y }));
        }
}

// whole-panel material, clipped to the plate
void paintMaterial (Graphics& g, EffectType t, const Look& lk, Rectangle<float> p, bool mini)
{
    const float area = p.getWidth() * p.getHeight();
    switch (t)
    {
        case EffectType::CassettePlug:
            // cream lacquer with a fine paper-like grain
            lacquer (g, p, lk.plate, 1.0f);
            speckle (g, p, 11, (int) (area / 9.0f), Colours::white, col (0xff7a5a32), 0.10f, 0.9f);
            break;

        case EffectType::Menace:
        {
            // black anodised aluminium: fine brushing + a cold vignette
            lacquer (g, p, lk.plate, 0.35f);
            brushed (g, p, 21, 0.05f, 0.6f);
            speckle (g, p, 22, (int) (area / 14.0f), Colours::white, Colours::black, 0.07f, 0.8f);
            ColourGradient vign (Colours::black.withAlpha (0.0f), p.getCentreX(), p.getCentreY(), Colours::black.withAlpha (0.35f), p.getX(), p.getY(), true);
            g.setGradientFill (vign);
            g.fillRect (p);
            break;
        }

        case EffectType::Brainrot:
        {
            // green solder mask over FR4 with copper traces; traces stay faint behind the knob row
            lacquer (g, p, lk.plate, 0.5f);
            speckle (g, p, 31, (int) (area / 12.0f), col (0xff7fe0a0), col (0xff0b2a16), 0.10f, 0.8f);
            // keep the traces away from printed text, displays and the knob row
            RectangleList<int> outside (p.toNearestInt());
            Rectangle<float> calm;
            if (mini)
            {
                outside.subtract (Rectangle<float> (p.getX() + 10.0f, p.getY() + 2.0f, 150.0f, 25.0f).toNearestInt());
                outside.subtract (Rectangle<float> (p.getX() + 10.0f, p.getBottom() - 14.0f, 112.0f, 12.0f).toNearestInt());
                outside.subtract (Rectangle<float> (p.getRight() - 101.0f, p.getY() + 3.0f, 90.0f, p.getHeight() - 6.0f).toNearestInt());
            }
            else
            {
                calm = knobArea (t, p).expanded (2.0f, 0.0f);
                outside.subtract (calm.toNearestInt());
                outside.subtract (nameArea (p).expanded (4.0f, 5.0f).toNearestInt());
                outside.subtract (liveArea (t, p).expanded (4.0f, 4.0f).toNearestInt());
                outside.subtract (Rectangle<float> (knobArea (t, p).getX() - 2.0f, p.getY() + 10.0f, 640.0f, 22.0f).toNearestInt());
                outside.subtract (presetLcdArea (p).expanded (6.0f).toNearestInt());
            }
            {
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (outside);
                pcbTraces (g, p, mini ? 32 : 33, mini ? 26 : 80, mini ? 0.45f : 0.55f);
            }
            if (! mini)
            {
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (calm.toNearestInt());
                pcbTraces (g, calm, 34, 22, 0.12f);
            }
            break;
        }

        case EffectType::ThroughTheWall:
        {
            // navy lacquer with a faint brick wall printed into it, fading out towards the knobs
            lacquer (g, p, lk.plate, 0.8f);
            speckle (g, p, 41, (int) (area / 12.0f), Colours::white, Colours::black, 0.07f, 0.8f);
            const float bh = mini ? 7.0f : 11.0f, bw = mini ? 20.0f : 32.0f;
            int row = 0;
            for (float y = p.getY() + 2.0f; y < p.getBottom(); y += bh, ++row)
            {
                g.setColour (Colours::black.withAlpha (0.10f));
                g.fillRect (p.getX(), y + 0.8f, p.getWidth(), 0.8f);
                g.setColour (Colours::white.withAlpha (0.05f));
                g.fillRect (p.getX(), y, p.getWidth(), 0.8f);
                for (float x = p.getX() + ((row & 1) ? bw * 0.5f : 0.0f); x < p.getRight(); x += bw)
                    g.fillRect (x, y, 0.8f, bh);
            }
            ColourGradient fade (lk.plate.withAlpha (0.0f), p.getX() + p.getWidth() * 0.05f, 0.0f, lk.plate.withAlpha (0.9f), p.getX() + p.getWidth() * 0.55f, 0.0f, false);
            g.setGradientFill (fade);
            g.fillRect (p.withTrimmedLeft (p.getWidth() * 0.05f));
            ColourGradient glow (th::accentDeep.withAlpha (0.18f), p.getX() + p.getWidth() * 0.12f, p.getBottom(),
                                 th::accentDeep.withAlpha (0.0f), p.getX() + p.getWidth() * 0.45f, p.getY(), true);
            g.setGradientFill (glow);
            g.fillRect (p);
            break;
        }

        case EffectType::ToneUp:
        {
            // light brushed aluminium with an anisotropic highlight band
            lacquer (g, p, lk.plate, 0.0f);
            brushed (g, p, 51, 0.16f, 0.45f);
            ColourGradient band (Colours::white.withAlpha (0.0f), p.getX(), p.getY(), Colours::white.withAlpha (0.0f), p.getRight(), p.getY(), false);
            band.addColour (0.28, Colours::white.withAlpha (0.20f));
            band.addColour (0.40, Colours::white.withAlpha (0.02f));
            band.addColour (0.72, Colours::white.withAlpha (0.12f));
            g.setGradientFill (band);
            g.fillRect (p);
            break;
        }

        case EffectType::Squeeze:
        {
            // graphite powder coat
            lacquer (g, p, lk.plate, 0.4f);
            speckle (g, p, 61, (int) (area / 5.0f), Colours::white, Colours::black, 0.07f, 0.7f);
            ColourGradient vign (Colours::black.withAlpha (0.0f), p.getCentreX(), p.getCentreY(), Colours::black.withAlpha (0.25f), p.getX(), p.getY(), true);
            g.setGradientFill (vign);
            g.fillRect (p);
            break;
        }

        case EffectType::Overcooked:
            // glossy orange enamel
            lacquer (g, p, lk.plate, 1.0f);
            speckle (g, p, 71, (int) (area / 10.0f), Colours::white, col (0xff5a1a00), 0.08f, 0.8f);
            break;

        case EffectType::Slap:
            // yellow enamel with comic halftone dots in the corners
            lacquer (g, p, lk.plate, 0.9f);
            speckle (g, p, 81, (int) (area / 12.0f), Colours::white, col (0xff6a5200), 0.08f, 0.8f);
            if (mini)
                halftone (g, p, p.getBottomRight(), 90.0f, 4.0f, lk.ink.withAlpha (0.14f));
            else
            {
                halftone (g, p, { p.getRight() - 30.0f, p.getBottom() }, 170.0f, 5.0f, lk.ink.withAlpha (0.10f));
                halftone (g, p, { p.getX() + 190.0f, p.getY() + 6.0f }, 110.0f, 5.0f, lk.ink.withAlpha (0.07f));
            }
            break;

        default:
            lacquer (g, p, lk.plate, 0.5f);
            break;
    }
}

// ---------------------------------------------------------------------------------------------
// hardware: ears, screws, LCD glass, bevel
// ---------------------------------------------------------------------------------------------
void paintScrew (Graphics& g, Point<float> c, float rad, float angle)
{
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (rad * 2.3f, rad * 2.3f).withCentre (c.translated (0.0f, rad * 0.25f)));
    ColourGradient head (col (0xfff4f7fc), c.x - rad * 0.5f, c.y - rad * 0.7f, col (0xff4a5160), c.x + rad * 0.6f, c.y + rad * 0.9f, true);
    head.addColour (0.55, col (0xffaab2c2));
    g.setGradientFill (head);
    g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), 0.5f);
    // cross recess
    const float l = rad * 0.62f, w = rad * 0.30f;
    for (int k = 0; k < 2; ++k)
    {
        const float a = angle + (float) k * MathConstants<float>::halfPi;
        const Point<float> d (std::cos (a) * l, std::sin (a) * l);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawLine ({ c - d, c + d }, w * 0.5f);
        g.setColour (col (0xff1a1d24));
        g.drawLine ({ (c - d).translated (-0.25f, -0.35f), (c + d).translated (-0.25f, -0.35f) }, w);
    }
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (rad * 0.5f, rad * 0.35f).withCentre (c.translated (-rad * 0.45f, -rad * 0.55f)));
}

void paintEar (Graphics& g, EffectType t, Rectangle<float> ear, bool left, const Look& lk, bool mini)
{
    if (t == EffectType::Menace) // hazard tape wrapped over the ears
        hazardStripes (g, ear, mini ? 8.0f : 16.0f, lk.accent, left ? 24 : 25);

    // the ear is the folded steel flange: slightly darker, with a bend highlight towards the panel
    g.setColour (Colours::black.withAlpha (0.16f));
    g.fillRect (ear);
    const float bx = left ? ear.getRight() - 1.0f : ear.getX();
    g.setColour (Colours::white.withAlpha (lk.lightPanel ? 0.35f : 0.12f));
    g.fillRect (bx, ear.getY() + 2.0f, 0.7f, ear.getHeight() - 4.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (left ? bx - 1.0f : bx + 0.7f, ear.getY() + 2.0f, 1.0f, ear.getHeight() - 4.0f);

    Random rnd ((int) t * 131 + (left ? 0 : 7));
    const float slotW = mini ? 4.6f : ear.getWidth() * 0.62f;
    const float slotH = mini ? 3.0f : 9.0f;
    const float inset = mini ? 7.0f : 20.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float cy = i == 0 ? ear.getY() + inset : ear.getBottom() - inset;
        const auto slot = Rectangle<float> (slotW, slotH).withCentre ({ ear.getCentreX() + (left ? -0.5f : 0.5f), cy });
        g.setColour (Colours::white.withAlpha (lk.lightPanel ? 0.4f : 0.12f));
        g.fillRoundedRectangle (slot.translated (0.0f, 0.8f), slotH * 0.5f);
        g.setColour (col (0xff020203));
        g.fillRoundedRectangle (slot, slotH * 0.5f);
        if (! mini) // chrome rack screw sitting in the oval slot, slightly off-centre like a real mounted unit
            paintScrew (g, { slot.getCentreX() + (rnd.nextFloat() - 0.5f) * 3.0f, cy }, 5.6f, rnd.nextFloat() * 3.1f);
    }
}

void paintLcdFrame (Graphics& g, Rectangle<float> r)
{
    // empty LCD glass for the unit preset display (the program name is drawn by RackView)
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.expanded (3.0f).translated (0.0f, 1.0f), 4.0f);
    ColourGradient bezel (col (0xff3a3f4a), 0.0f, r.getY() - 3.0f, col (0xff0b0c10), 0.0f, r.getBottom() + 3.0f, false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (r.expanded (2.5f), 3.5f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r.expanded (2.5f).reduced (0.4f), 3.5f, 0.6f);
    const Colour glass (0xff060a10);
    ColourGradient gl (glass.brighter (0.25f), 0.0f, r.getY(), glass, 0.0f, r.getBottom(), false);
    g.setGradientFill (gl);
    g.fillRoundedRectangle (r, 2.0f);
    ColourGradient sh (Colours::black.withAlpha (0.6f), 0.0f, r.getY(), Colours::transparentBlack, 0.0f, r.getY() + 5.0f, false);
    g.setGradientFill (sh);
    g.fillRoundedRectangle (r, 2.0f);
    Path refl;
    refl.startNewSubPath (r.getX(), r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.38f, r.getBottom());
    refl.lineTo (r.getX(), r.getBottom());
    refl.closeSubPath();
    ColourGradient rg (Colours::white.withAlpha (0.08f), r.getX(), r.getY(), Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.35f, r.getBottom(), false);
    g.setGradientFill (rg);
    g.fillPath (refl);
}

void paintBevel (Graphics& g, Rectangle<float> r, bool lightPanel, float corner)
{
    g.setColour (Colours::white.withAlpha (lightPanel ? 0.50f : 0.20f));
    g.fillRect (r.getX() + corner, r.getY(), r.getWidth() - 2.0f * corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRect (r.getX() + corner, r.getBottom() - 1.0f, r.getWidth() - 2.0f * corner, 1.0f);
    g.setColour (Colours::black.withAlpha (lightPanel ? 0.45f : 0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

// ---------------------------------------------------------------------------------------------
// name sticker / frame per effect + small print
// ---------------------------------------------------------------------------------------------
void paintSmallPrint (Graphics& g, const Look& lk, Rectangle<float> r, float h)
{
    // "SATURATION  ·  ✦TRAPWIN" under the name; TRAPWIN gets a tiny 4-point sparkle
    const Font f = smallFont (h).boldened();
    const Colour c = lk.ink.withAlpha (lk.lightPanel ? 0.85f : 0.78f);
    const String type (lk.type);
    const float tw = GlyphArrangement::getStringWidth (f, type);
    printed (g, type, r, f, c, Justification::centredLeft, lk.lightPanel);
    const float dotX = r.getX() + tw + h * 0.9f;
    g.setColour (lk.lightPanel ? lk.ink.withAlpha (0.7f) : lk.accent.withAlpha (0.9f));
    g.fillEllipse (Rectangle<float> (h * 0.28f, h * 0.28f).withCentre ({ dotX, r.getCentreY() }));
    const float bx = dotX + h * 0.9f;
    Path sp;
    const Point<float> sc (bx + h * 0.35f, r.getCentreY());
    const float s1 = h * 0.42f, s2 = h * 0.1f;
    sp.startNewSubPath (sc.x, sc.y - s1);
    sp.quadraticTo (sc.x + s2, sc.y - s2, sc.x + s1, sc.y);
    sp.quadraticTo (sc.x + s2, sc.y + s2, sc.x, sc.y + s1);
    sp.quadraticTo (sc.x - s2, sc.y + s2, sc.x - s1, sc.y);
    sp.quadraticTo (sc.x - s2, sc.y - s2, sc.x, sc.y - s1);
    g.setColour (lk.lightPanel ? lk.ink.withAlpha (0.8f) : th::silver);
    g.fillPath (sp);
    printed (g, "TRAPWIN", r.withLeft (bx + h * 0.95f), f, c, Justification::centredLeft, lk.lightPanel);
}

void paintName (Graphics& g, EffectType t, const Look& lk, Rectangle<float> area, bool mini)
{
    const String name (lk.name);
    const float k = mini ? 0.58f : 1.0f;                       // size factor for the miniature
    auto box = area.removeFromTop (mini ? 22.0f : 36.0f);

    switch (t)
    {
        case EffectType::CassettePlug:
        {
            // cream paper label with a double brown frame, like a cassette J-card spine
            const Colour brown = lk.ink, orange = lk.accent, mustard = col (0xffd9a23a);
            Font f = fitted (nameFont (27.0f * k), name, box.getWidth() - 26.0f * k);
            const auto label = box.withWidth (jmin (box.getWidth(), GlyphArrangement::getStringWidth (f, name) + 30.0f * k));
            g.setColour (Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (label.translated (0.0f, 1.2f), 4.0f * k);
            ColourGradient paper (col (0xfffbf4e4), 0.0f, label.getY(), col (0xffeadfc6), 0.0f, label.getBottom(), false);
            g.setGradientFill (paper);
            g.fillRoundedRectangle (label, 4.0f * k);
            // retro colour bands on the label's left edge
            const Colour bands[] = { orange, mustard, brown };
            for (int i = 0; i < 3; ++i)
            {
                g.setColour (bands[i]);
                g.fillRect (label.getX() + (2.5f + (float) i * 3.0f) * k, label.getY() + 2.5f * k, 2.0f * k, label.getHeight() - 5.0f * k);
            }
            g.setColour (brown);
            g.drawRoundedRectangle (label.reduced (0.8f), 3.5f * k, mini ? 0.9f : 1.5f);
            if (! mini) g.drawRoundedRectangle (label.reduced (3.5f), 2.0f, 0.5f);
            auto p = namePath (name, f, label.withTrimmedLeft (11.0f * k), true);
            g.setColour (brown);
            g.fillPath (p);
            break;
        }

        case EffectType::Menace:
        {
            // red bar + white stencil name with a hard drop shadow
            g.setColour (lk.accent);
            g.fillRect (box.getX(), box.getY() + 3.0f * k, 5.0f * k, box.getHeight() - 6.0f * k);
            Font f = fitted (nameFont (33.0f * k), name, box.getWidth() - 14.0f * k);
            auto p = namePath (name, f, box.withTrimmedLeft (12.0f * k), false);
            g.setColour (Colours::black.withAlpha (0.75f));
            g.fillPath (p, AffineTransform::translation (1.5f * k, 1.8f * k));
            ColourGradient metal (col (0xffffffff), 0.0f, box.getY(), col (0xffb9bec8), 0.0f, box.getBottom(), false);
            g.setGradientFill (metal);
            g.fillPath (p);
            break;
        }

        case EffectType::Brainrot:
        {
            // silkscreen component outline (U1) with a pin-1 dot
            const Colour silk = lk.ink;
            Font f = fitted (nameFont (28.0f * k), name, box.getWidth() - 24.0f * k);
            const auto outline = box.withWidth (jmin (box.getWidth(), GlyphArrangement::getStringWidth (f, name) + 30.0f * k));
            g.setColour (lk.plateDark.darker (0.3f).withAlpha (0.85f));
            g.fillRect (outline.reduced (1.0f));
            g.setColour (silk);
            g.drawRect (outline, mini ? 0.8f : 1.2f);
            g.fillEllipse (outline.getX() + 3.5f * k, outline.getY() + 3.5f * k, 3.0f * k, 3.0f * k);
            if (! mini)
            {
                text (g, "U1", { outline.getRight() + 3.0f, outline.getY() - 1.0f, 18.0f, 9.0f }, smallFont (7.0f), silk.withAlpha (0.8f), Justification::centredLeft);
                // pin stubs along the top and bottom edge
                for (float x = outline.getX() + 12.0f; x < outline.getRight() - 8.0f; x += 9.0f)
                {
                    g.setColour (lk.accent.withAlpha (0.9f));
                    g.fillRect (x, outline.getY() - 3.5f, 3.0f, 2.5f);
                    g.fillRect (x, outline.getBottom() + 1.0f, 3.0f, 2.5f);
                }
            }
            auto p = namePath (name, f, outline, true);
            g.setColour (silk);
            g.fillPath (p);
            break;
        }

        case EffectType::ThroughTheWall:
        {
            // white name, ice-blue fading rule underneath
            Font f = fitted (nameFont (30.0f * k), name, box.getWidth() - 2.0f);
            auto p = namePath (name, f, box.translated (0.0f, -1.0f * k), false);
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillPath (p, AffineTransform::translation (0.0f, 1.2f * k));
            g.setColour (lk.ink);
            g.fillPath (p);
            const float w = GlyphArrangement::getStringWidth (f, name);
            ColourGradient line (lk.accent, box.getX(), 0.0f, lk.accent.withAlpha (0.0f), box.getX() + w, 0.0f, false);
            g.setGradientFill (line);
            g.fillRect (box.getX(), box.getBottom() - 1.0f * k, w, 1.4f * k);
            break;
        }

        case EffectType::ToneUp:
        {
            // engraved black name + five band colour chips
            Font f = fitted (nameFont (33.0f * k), name, box.getWidth() - 4.0f);
            auto p = namePath (name, f, box.translated (0.0f, -2.0f * k), false);
            g.setColour (Colours::white.withAlpha (0.7f));
            g.fillPath (p, AffineTransform::translation (0.0f, 1.0f));
            g.setColour (lk.ink);
            g.fillPath (p);
            const Colour bands[] = { col (0xffd8352f), col (0xffeb8a24), col (0xffe9c429), col (0xff35a85a), col (0xff2f6fd6) };
            for (int i = 0; i < 5; ++i)
            {
                g.setColour (bands[i]);
                g.fillRect (box.getX() + (float) i * 18.0f * k, box.getBottom() - 1.5f * k, 15.0f * k, 2.4f * k);
            }
            break;
        }

        case EffectType::Squeeze:
        {
            // recessed black inlay with a green status dot
            Font f = fitted (nameFont (27.0f * k), name, box.getWidth() - 40.0f * k);
            const auto inlay = box.withWidth (jmin (box.getWidth(), GlyphArrangement::getStringWidth (f, name) + 42.0f * k));
            g.setColour (Colours::white.withAlpha (0.10f));
            g.fillRoundedRectangle (inlay.expanded (1.0f).translated (0.0f, 1.0f), 3.0f * k);
            g.setColour (col (0xff0d0e10));
            g.fillRoundedRectangle (inlay, 3.0f * k);
            ColourGradient sh (Colours::black.withAlpha (0.6f), 0.0f, inlay.getY(), Colours::transparentBlack, 0.0f, inlay.getY() + 6.0f * k, false);
            g.setGradientFill (sh);
            g.fillRoundedRectangle (inlay, 3.0f * k);
            auto p = namePath (name, f, inlay.withTrimmedLeft (11.0f * k), false);
            g.setColour (lk.ink);
            g.fillPath (p);
            const Point<float> dot (inlay.getRight() - 12.0f * k, inlay.getCentreY());
            g.setColour (lk.accent.withAlpha (0.25f));
            g.fillEllipse (Rectangle<float> (9.0f * k, 9.0f * k).withCentre (dot));
            g.setColour (lk.accent);
            g.fillEllipse (Rectangle<float> (4.5f * k, 4.5f * k).withCentre (dot));
            break;
        }

        case EffectType::Overcooked:
        {
            // dark burnt sticker with a cream keyline
            Font f = fitted (nameFont (28.0f * k), name, box.getWidth() - 22.0f * k);
            const auto sticker = box.withWidth (jmin (box.getWidth(), GlyphArrangement::getStringWidth (f, name) + 26.0f * k));
            g.setColour (Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (sticker.translated (1.0f * k, 1.5f * k), 4.0f * k);
            ColourGradient burnt (col (0xff3a1a0a), 0.0f, sticker.getY(), lk.ink, 0.0f, sticker.getBottom(), false);
            g.setGradientFill (burnt);
            g.fillRoundedRectangle (sticker, 4.0f * k);
            g.setColour (lk.accent.withAlpha (0.8f));
            g.drawRoundedRectangle (sticker.reduced (2.5f * k), 2.5f * k, mini ? 0.6f : 0.8f);
            auto p = namePath (name, f, sticker, true);
            ColourGradient heat (col (0xffffe9c4), 0.0f, sticker.getY(), col (0xffffb259), 0.0f, sticker.getBottom(), false);
            g.setGradientFill (heat);
            g.fillPath (p);
            break;
        }

        case EffectType::Slap:
        {
            // black sticker slapped on slightly askew, yellow print
            Font f = nameFont (31.0f * k);
            const auto sticker = Rectangle<float> (box.getX(), box.getY() + 1.0f * k,
                                                   GlyphArrangement::getStringWidth (f, name) + 26.0f * k, box.getHeight() - 1.0f * k);
            const auto rot = AffineTransform::rotation (-0.045f, sticker.getCentreX(), sticker.getCentreY());
            Path sp; sp.addRoundedRectangle (sticker, 3.0f * k);
            g.setColour (Colours::black.withAlpha (0.28f));
            g.fillPath (sp, rot.translated (2.0f * k, 2.0f * k));
            g.setColour (lk.ink);
            g.fillPath (sp, rot);
            Path frame; frame.addRoundedRectangle (sticker.reduced (2.5f * k), 2.0f * k);
            g.setColour (lk.plate);
            g.strokePath (frame, PathStrokeType (mini ? 0.6f : 0.8f), rot);
            auto p = namePath (name, f, sticker, true);
            g.fillPath (p, rot);
            break;
        }

        default: break;
    }

    if (! mini)
        paintSmallPrint (g, lk, area.withHeight (13.0f).translated (0.0f, 2.0f), 9.5f);
}

// ---------------------------------------------------------------------------------------------
// per-effect panel printing in the strip above the knob row (right of the model line)
// ---------------------------------------------------------------------------------------------
Rectangle<float> decorBand (EffectType t, Rectangle<float> p)
{
    const auto ka = knobArea (t, p);
    const float x = ka.getX() + 188.0f;
    return { x, p.getY() + 13.0f, presetLcdArea (p).getX() - 18.0f - x, 18.0f };
}

void paintDecor (Graphics& g, EffectType t, const Look& lk, Rectangle<float> p)
{
    const auto tb = decorBand (t, p);
    switch (t)
    {
        case EffectType::CassettePlug:
        {
            // three retro stripes running towards the LCD
            const Colour cs[] = { lk.accent, col (0xffd9a23a), lk.ink };
            for (int i = 0; i < 3; ++i)
            {
                g.setColour (cs[i].withAlpha (0.92f));
                g.fillRect (tb.getX(), tb.getCentreY() - 4.0f + (float) i * 3.6f, tb.getWidth(), 2.2f);
            }
            break;
        }
        case EffectType::Menace:
        {
            auto band = tb.reduced (0.0f, 2.0f);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.fillRect (band.expanded (1.0f));
            hazardStripes (g, band, 14.0f, lk.accent, 23);
            g.setColour (Colours::white.withAlpha (0.14f));
            g.fillRect (band.getX(), band.getY(), band.getWidth(), 0.8f);
            break;
        }
        case EffectType::Brainrot:
        {
            text (g, "REV 1.3  //  DO NOT BEND", tb, smallFont (8.0f), lk.ink.withAlpha (0.75f), Justification::centredLeft);
            for (int i = 0; i < 4; ++i) // tiny SMD resistors
            {
                auto rr = Rectangle<float> (10.0f, 5.0f).withCentre ({ tb.getRight() - 8.0f - (float) i * 16.0f, tb.getCentreY() });
                g.setColour (col (0xff1a1a1a)); g.fillRect (rr);
                g.setColour (col (0xffd8d8d8)); g.fillRect (rr.withWidth (2.0f)); g.fillRect (rr.withLeft (rr.getRight() - 2.0f));
            }
            break;
        }
        case EffectType::ToneUp:
        {
            // printed log frequency ruler
            const auto ink = lk.ink;
            g.setColour (ink.withAlpha (0.5f));
            g.fillRect (tb.getX(), tb.getBottom() - 4.0f, tb.getWidth(), 0.8f);
            auto xOf = [&] (float f) { return tb.getX() + tb.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
            const float freqs[] = { 20, 100, 1000, 10000, 20000 };
            const char* lbl[] = { "20", "100", "1k", "10k", "20k" };
            for (int i = 0; i < 5; ++i)
            {
                const float x = xOf (freqs[i]);
                g.setColour (ink.withAlpha (0.5f));
                g.fillRect (x - 0.4f, tb.getBottom() - 8.0f, 0.8f, 5.0f);
                text (g, lbl[i], { x - 14.0f, tb.getY() - 2.0f, 28.0f, 10.0f }, smallFont (7.0f), ink.withAlpha (0.6f),
                      i == 0 ? Justification::centredLeft : (i == 4 ? Justification::centredRight : Justification::centred));
            }
            for (float dec : { 10.0f, 100.0f, 1000.0f })
                for (int m = 2; m < 10; ++m)
                {
                    const float f = dec * (float) m;
                    if (f < 20.0f || f > 20000.0f) continue;
                    g.setColour (ink.withAlpha (0.28f));
                    g.fillRect (xOf (f) - 0.3f, tb.getBottom() - 6.0f, 0.6f, 2.5f);
                }
            break;
        }
        case EffectType::Squeeze:
        {
            // green pin-stripe along the top of the panel
            const float x0 = p.getX() + 24.0f, x1 = p.getRight() - 24.0f;
            g.setColour (lk.accent.withAlpha (0.12f));
            g.fillRect (x0, p.getY() + 4.5f, x1 - x0, 3.0f);
            g.setColour (lk.accent.withAlpha (0.6f));
            g.fillRect (x0, p.getY() + 5.5f, x1 - x0, 1.0f);
            break;
        }
        case EffectType::Overcooked:
        {
            // stamped cooling vents
            auto b = tb.reduced (0.0f, 2.0f);
            const float slotW = 5.0f, gap = 5.0f;
            for (float x = b.getX(); x + slotW <= b.getRight(); x += slotW + gap)
            {
                auto s = Rectangle<float> (x, b.getY(), slotW, b.getHeight());
                g.setColour (Colours::white.withAlpha (0.28f));
                g.fillRoundedRectangle (s.translated (0.0f, 1.0f), slotW * 0.5f);
                ColourGradient hole (col (0xff0d0502), 0.0f, s.getY(), col (0xff3a1606), 0.0f, s.getBottom(), false);
                g.setGradientFill (hole);
                g.fillRoundedRectangle (s, slotW * 0.5f);
            }
            break;
        }
        case EffectType::Slap:
        {
            // chevrons punching towards the LCD
            for (float x = tb.getX(); x + 12.0f < tb.getRight(); x += 15.0f)
            {
                Path ch;
                ch.startNewSubPath (x, tb.getY() + 2.0f);
                ch.lineTo (x + 5.0f, tb.getY() + 2.0f);
                ch.lineTo (x + 11.0f, tb.getCentreY());
                ch.lineTo (x + 5.0f, tb.getBottom() - 2.0f);
                ch.lineTo (x, tb.getBottom() - 2.0f);
                ch.lineTo (x + 6.0f, tb.getCentreY());
                ch.closeSubPath();
                const float fade = (x - tb.getX()) / tb.getWidth();
                g.setColour (lk.ink.withAlpha (0.10f + 0.75f * fade * fade));
                g.fillPath (ch);
            }
            break;
        }
        case EffectType::ThroughTheWall:
        default:
            break;
    }
}

// trim along the foot of the panel (outside the knob values)
void paintFoot (Graphics& g, EffectType t, const Look& lk, Rectangle<float> p, bool mini)
{
    const float earW = mini ? 8.0f : 18.0f;
    const float x0 = p.getX() + earW + 2.0f, x1 = p.getRight() - earW - 2.0f;
    switch (t)
    {
        case EffectType::CassettePlug:
        {
            const Colour cs[] = { lk.accent, col (0xffd9a23a), lk.ink };
            const float h = mini ? 0.8f : 1.3f, step = mini ? 1.2f : 2.4f;
            for (int i = 0; i < 3; ++i)
            {
                g.setColour (cs[i].withAlpha (0.9f));
                g.fillRect (x0, p.getBottom() - (mini ? 4.2f : 8.5f) + (float) i * step, x1 - x0, h);
            }
            break;
        }
        case EffectType::Menace:
            if (mini) hazardStripes (g, { x0, p.getBottom() - 3.5f, x1 - x0, 3.5f }, 8.0f, lk.accent, 26);
            break;
        case EffectType::Brainrot:
            // gold edge-connector fingers
            for (float x = x0 + (mini ? 6.0f : 20.0f); x < x1 - (mini ? 6.0f : 20.0f); x += mini ? 5.0f : 9.0f)
            {
                g.setColour (lk.accent.withAlpha (0.85f));
                g.fillRoundedRectangle (x, p.getBottom() - (mini ? 2.5f : 4.5f), mini ? 3.0f : 5.0f, 6.0f, 1.0f);
            }
            break;
        case EffectType::Slap:
            g.setColour (lk.ink);
            g.fillRect (x0 - 2.0f, p.getBottom() - (mini ? 3.0f : 5.0f), x1 - x0 + 4.0f, mini ? 3.0f : 5.0f);
            break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------
// static layer
// ---------------------------------------------------------------------------------------------
Rectangle<float> miniLiveArea (Rectangle<float> plate)
{
    return { plate.getRight() - 8.0f - 6.0f - 84.0f, plate.getY() + 6.0f, 84.0f, plate.getHeight() - 12.0f };
}

void paintStatic (Graphics& g, EffectType t, Rectangle<float> p, bool mini)
{
    const auto lk = look (t);
    const float corner = mini ? 2.0f : 3.0f;
    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (p, corner);
        g.reduceClipRegion (clip);
        paintMaterial (g, t, lk, p, mini);
        const float earW = mini ? 8.0f : 18.0f;
        paintEar (g, t, p.withWidth (earW), true, lk, mini);
        paintEar (g, t, p.withLeft (p.getRight() - earW), false, lk, mini);
        paintFoot (g, t, lk, p, mini);

        if (mini)
        {
            const auto inner = p.reduced (earW + 4.0f, 0.0f);
            paintName (g, t, lk, Rectangle<float> (inner.getX(), p.getY() + 3.0f, inner.getWidth() * 0.6f, 24.0f), true);
            printed (g, String (lk.type) + "  TRAPWIN", Rectangle<float> (inner.getX(), p.getBottom() - 13.0f, 110.0f, 10.0f),
                     smallFont (7.0f).boldened(), lk.ink.withAlpha (0.7f), Justification::centredLeft, lk.lightPanel);
        }
        else
        {
            paintName (g, t, lk, nameArea (p), false);
            paintDecor (g, t, lk, p);

            // engraved model line above the knob row (faint, so the knob area stays calm)
            const auto ka = knobArea (t, p);
            printed (g, lk.model, Rectangle<float> (ka.getX() + 2.0f, p.getY() + 14.0f, 190.0f, 12.0f), smallFont (8.5f),
                     lk.ink.withAlpha (lk.lightPanel ? 0.60f : 0.42f), Justification::centredLeft, lk.lightPanel);

            // engraved divider between the identity block and the knob row
            const float dx = ka.getX() - 7.0f;
            g.setColour (Colours::black.withAlpha (lk.lightPanel ? 0.28f : 0.45f));
            g.fillRect (dx, p.getY() + 14.0f, 1.0f, p.getHeight() - 28.0f);
            g.setColour (Colours::white.withAlpha (lk.lightPanel ? 0.40f : 0.08f));
            g.fillRect (dx + 1.0f, p.getY() + 14.0f, 0.8f, p.getHeight() - 28.0f);

            paintLcdFrame (g, presetLcdArea (p));
        }
    }
    paintBevel (g, p, lk.lightPanel, corner);
}

// ---------------------------------------------------------------------------------------------
// live-element helpers
// ---------------------------------------------------------------------------------------------
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
    const int type = jlimit (0, 3, roundToInt (realParam (EffectType::Menace, s, 0)));
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
    text (g, String (bits) + "-BIT  " + midDot() + "  " + String (rate / 1000.0f, rate < 10000.0f ? 1 : 0) + " kHz",
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
            text (g, "GR dB", Rectangle<float> (face.getX() + 5.0f, face.getBottom() - 12.0f, 40.0f, 9.0f), smallFont (7.0f).boldened(), col (0xff1c1c1a), Justification::centredLeft);

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
    const float atk = realParam (EffectType::Slap, s, 0), sus = realParam (EffectType::Slap, s, 1);
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
    const Point<float> c (a.getRight() - 30.0f, a.getCentreY());
    starburst (c, 15.0f, 27.0f, 11, black);
    starburst (c, 10.0f, 18.0f, 11, hit ? col (0xffff6a3a) : yellow.darker (0.05f));
    glowLed (g, c, 6.0f, red, hit ? 1.0f : 0.0f);
    const auto lab = Rectangle<float> (scr.getRight() + 8.0f, a.getCentreY() - 9.0f, c.x - 27.0f - scr.getRight() - 10.0f, 18.0f);
    text (g, "HIT", lab, nameFont (15.0f), black, Justification::centredRight);
}

void paintLive (Graphics& g, EffectType t, Rectangle<float> p, const State& s, bool mini)
{
    const auto a = mini ? miniLiveArea (p) : liveArea (t, p);
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
// static layer cache (UI thread only), keyed by type / size / pixel scale / miniature
// ---------------------------------------------------------------------------------------------
const Image& staticLayer (EffectType t, Rectangle<float> plate, float scale, bool mini)
{
    static std::map<int64, Image> cache;
    const int w = roundToInt (plate.getWidth()), h = roundToInt (plate.getHeight());
    const int sc = roundToInt (scale * 100.0f);
    const int64 key = ((int64) t << 48) ^ ((int64) w << 32) ^ ((int64) h << 16) ^ ((int64) sc << 1) ^ (mini ? 1 : 0);
    auto it = cache.find (key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 96) cache.clear();

    Image img (Image::ARGB, jmax (1, roundToInt ((float) w * scale)), jmax (1, roundToInt ((float) h * scale)), true);
    {
        Graphics ig (img);
        ig.addTransform (AffineTransform::scale (scale));
        paintStatic (ig, t, { 0.0f, 0.0f, (float) w, (float) h }, mini);
    }
    return cache.emplace (key, std::move (img)).first->second;
}
} // namespace

Palette paletteColorGroup (EffectType t)
{
    const auto lk = look (t);
    return { lk.plate, lk.plateDark, lk.ink, lk.accent, lk.led };
}

void paintColorGroup (Graphics& g, EffectType t, Rectangle<float> plate, const State& s, bool miniature)
{
    if (plate.isEmpty()) return;
    const float scale = jlimit (1.0f, 4.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto& img = staticLayer (t, plate, scale, miniature);
    g.setOpacity (1.0f);   // drawImage uses the current colour's alpha
    g.drawImage (img, plate);
    paintLive (g, t, plate, s, miniature);
}
} // namespace ek::faceplate
