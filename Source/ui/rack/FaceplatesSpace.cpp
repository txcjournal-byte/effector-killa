// Hardware faceplates of the "space" group: Doubles, Swirl, Ad-Lib Throw, Aura Room, Wide Body,
// Chopped and Red Line (DESIGN-RACK.md section 6). Original panel designs, no product copies.
//
// Drawing is split in two layers:
//  - a static layer (material, texture, ears, screws, name sticker, glass frames) that is rendered
//    once per type / size / pixel scale into an image cache, so the per-frame cost stays tiny;
//  - the live element (LEDs, displays, scope...) that is drawn every frame from the State snapshot.
// knobArea() is left calm on purpose: RackView places the real knob components on top of it.
#include "Faceplates.h"
#include "../Theme.h"
#include "../../engine/effects/AllEffects.h"
#include <map>

namespace ek::faceplate
{
namespace
{
using namespace juce;
namespace th = ek::theme;

// ---------------------------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------------------------
constexpr float twoPi = MathConstants<float>::twoPi;

uint32 hashU (uint32 x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
float h01 (uint32 a, uint32 b = 0) noexcept { return (float) (hashU (a * 0x9E3779B1u ^ hashU (b + 0x51ed27u)) & 0xffffffu) / 16777215.0f; }

float real (EffectType t, const State& s, int i) { return effectInfo (t).params[(size_t) i].toReal (s.params[(size_t) i]); }
String fmt (EffectType t, const State& s, int i) { const auto& p = effectInfo (t).params[(size_t) i]; return p.format (p.toReal (s.params[(size_t) i])); }

// animation clock that freezes when the user asked for reduced motion
double animTime (const State& s) { return s.reduceMotion ? 0.0 : s.timeSec; }

Font mono (float h) { return th::monoLabel (h); }

void text (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j = Justification::centredLeft)
{
    g.setColour (c);
    g.setFont (f);
    g.drawText (s, r, j, false);
}

// font squeezed horizontally so that `s` fits into maxW
Font fitted (Font f, const String& s, float maxW)
{
    const float w = GlyphArrangement::getStringWidth (f, s);
    if (w > maxW && w > 0.0f) f = f.withHorizontalScale (f.getHorizontalScale() * maxW / w);
    return f;
}

void textPath (Path& p, const String& s, Font f, float x, float baseline)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, x, baseline);
    ga.createPath (p);
}

// "printed" text with a soft debossed edge (light below, dark above) - used on painted panels
void printed (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j, bool lightPanel)
{
    g.setFont (f);
    g.setColour ((lightPanel ? Colours::white : Colours::black).withAlpha (lightPanel ? 0.35f : 0.55f));
    g.drawText (s, r.translated (0.0f, lightPanel ? 0.8f : 1.0f), j, false);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

// recessed window (display glass / porthole frame)
void recess (Graphics& g, Rectangle<float> r, float corner, Colour glass)
{
    g.setColour (Colours::white.withAlpha (0.13f));                         // lower lip catches light
    g.fillRoundedRectangle (r.expanded (1.6f).translated (0.0f, 0.9f), corner + 1.6f);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.fillRoundedRectangle (r.expanded (1.4f), corner + 1.4f);
    ColourGradient gl (glass.brighter (0.12f), r.getX(), r.getY(), glass.darker (0.5f), r.getX(), r.getBottom(), false);
    g.setGradientFill (gl);
    g.fillRoundedRectangle (r, corner);
    // inner shadow along the top edge
    ColourGradient sh (Colours::black.withAlpha (0.55f), 0.0f, r.getY(), Colours::transparentBlack, 0.0f, r.getY() + 7.0f, false);
    g.setGradientFill (sh);
    g.fillRoundedRectangle (r, corner);
}

// glass reflection painted on top of live content
void glassSheen (Graphics& g, Rectangle<float> r, float corner)
{
    Path p;
    p.addRoundedRectangle (r, corner);
    g.saveState();
    g.reduceClipRegion (p);
    ColourGradient sh (Colours::white.withAlpha (0.09f), r.getX(), r.getY(), Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.35f, r.getBottom(), false);
    g.setGradientFill (sh);
    Path wedge;
    wedge.startNewSubPath (r.getX(), r.getY());
    wedge.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
    wedge.lineTo (r.getX() + r.getWidth() * 0.38f, r.getBottom());
    wedge.lineTo (r.getX(), r.getBottom());
    wedge.closeSubPath();
    g.fillPath (wedge);
    g.restoreState();
}

void led (Graphics& g, Point<float> c, float r, Colour col, float level)
{
    // level 0 = off lens, 1 = fully lit; in between dims the glow
    if (level <= 0.02f) { th::drawLed (g, c, r, col, false); return; }
    th::drawLed (g, c, r, col.withMultipliedBrightness (0.55f + 0.45f * level), true, level);
}

// ---------------------------------------------------------------------------------------------
// per-effect style
// ---------------------------------------------------------------------------------------------
enum class Finish { Brushed, Lacquer, Satin, Space, Powder, Crackle, Graphite };

struct Style
{
    Colour plate, plateDark, ink, accent, led;
    Finish finish;
    const char* type;       // small category print
    const char* model;      // engraved model line
    bool lightPanel;        // dark ink on a light panel
};

Style style (EffectType t)
{
    switch (t)
    {
        case EffectType::Doubles:    return { Colour (0xff2aa6c4), Colour (0xff12627a), Colour (0xff04212b), Colour (0xffd9fbff), Colour (0xff9ff4ff), Finish::Brushed,  "CHORUS",  "TW-09  VOICE MULTIPLIER", true };
        case EffectType::Swirl:      return { Colour (0xff5b2a86), Colour (0xff2a0f45), Colour (0xfff1e4ff), Colour (0xffe0b25c), Colour (0xffd59bff), Finish::Lacquer,  "PHASER",  "TW-10  ALL-PASS ROTOR", false };
        case EffectType::AdLibThrow: return { Colour (0xff14203d), Colour (0xff080d1c), Colour (0xffdbe8ff), th::accent,         th::accent,         Finish::Satin,    "DELAY",   "TW-11  TEMPO ECHO", false };
        case EffectType::AuraRoom:   return { Colour (0xff0b0c12), Colour (0xff020205), Colour (0xffe9eefb), Colour (0xffb79cff), Colour (0xffbfd4ff), Finish::Space,    "REVERB",  "TW-12  SPACE GENERATOR", false };
        case EffectType::WideBody:   return { Colour (0xff5fc9a0), Colour (0xff3a9c78), Colour (0xff0b2a20), Colour (0xff0b2a20), Colour (0xff7dffc6), Finish::Powder,   "STEREO",  "TW-13  IMAGE SHAPER", true };
        case EffectType::Chopped:    return { Colour (0xff163a23), Colour (0xff0a1d11), Colour (0xffd5f0d8), Colour (0xff3dff6e), th::ledGreen,       Finish::Crackle,  "GATE",    "TW-14  STEP SEQUENCED GATE", false };
        case EffectType::RedLine:    return { Colour (0xff2f3136), Colour (0xff17181b), Colour (0xffeceef2), th::killRed,        th::killRed,        Finish::Graphite, "CLIPPER", "TW-15  CEILING LIMITER", false };
        default:                     return { Colour (0xff24262b), Colour (0xff15161a), Colours::white, th::accent, th::ledGreen, Finish::Graphite, "", "", false };
    }
}

// ---------------------------------------------------------------------------------------------
// static layer
// ---------------------------------------------------------------------------------------------
void paintTexture (Graphics& g, const Style& st, Rectangle<float> r, uint32 seed)
{
    const int area = (int) (r.getWidth() * r.getHeight());
    switch (st.finish)
    {
        case Finish::Brushed:
        case Finish::Graphite:
        {
            // horizontal hairlines of random length/brightness (brushed metal)
            const bool strong = st.finish == Finish::Brushed;
            for (float y = r.getY(); y < r.getBottom(); y += 0.5f)
            {
                const uint32 k = (uint32) ((y - r.getY()) * 2.0f);
                const int segs = 3;
                for (int s = 0; s < segs; ++s)
                {
                    const float x0 = r.getX() + h01 (seed + k, (uint32) s * 3u) * r.getWidth();
                    const float len = r.getWidth() * (0.15f + 0.6f * h01 (seed + k, (uint32) s * 3u + 1u));
                    const float a = (strong ? 0.075f : 0.045f) * h01 (seed + k, (uint32) s * 3u + 2u);
                    g.setColour ((s & 1) ? Colours::white.withAlpha (a) : Colours::black.withAlpha (a * 1.3f));
                    g.fillRect (x0, y, jmin (len, r.getRight() - x0), 0.5f);
                }
            }
            break;
        }
        case Finish::Lacquer:
        case Finish::Satin:
        case Finish::Space:
        case Finish::Powder:
        case Finish::Crackle:
        {
            // fine speckle (powder coat / metal flake in the lacquer)
            const float density = st.finish == Finish::Powder ? 18.0f : st.finish == Finish::Crackle ? 16.0f : 30.0f;
            const int n = (int) ((float) area / density);
            for (int i = 0; i < n; ++i)
            {
                const float x = r.getX() + h01 (seed, (uint32) i * 4u) * r.getWidth();
                const float y = r.getY() + h01 (seed, (uint32) i * 4u + 1u) * r.getHeight();
                const float v = h01 (seed, (uint32) i * 4u + 2u);
                const float a = (st.finish == Finish::Powder ? 0.07f : 0.05f) * h01 (seed, (uint32) i * 4u + 3u);
                g.setColour (v > 0.5f ? Colours::white.withAlpha (a) : Colours::black.withAlpha (a * 1.4f));
                g.fillRect (x, y, 0.6f, 0.6f);
            }
            if (st.finish == Finish::Lacquer)
            {
                // metal flake sparkles in the violet lacquer
                for (int i = 0; i < n / 24; ++i)
                {
                    const float x = r.getX() + h01 (seed + 7u, (uint32) i * 3u) * r.getWidth();
                    const float y = r.getY() + h01 (seed + 7u, (uint32) i * 3u + 1u) * r.getHeight();
                    g.setColour (Colour (0xffe8c8ff).withAlpha (0.10f + 0.18f * h01 (seed + 7u, (uint32) i * 3u + 2u)));
                    g.fillRect (x, y, 0.5f, 0.5f);
                }
            }
            if (st.finish == Finish::Crackle)
            {
                // fine hex-dot grip pattern of a field unit
                g.setColour (Colours::black.withAlpha (0.22f));
                for (float y = r.getY() + 3.0f; y < r.getBottom() - 2.0f; y += 4.0f)
                {
                    const float off = std::fmod ((y - r.getY()) * 0.5f, 4.0f);
                    for (float x = r.getX() + 2.0f + off; x < r.getRight() - 2.0f; x += 4.0f)
                        g.fillEllipse (x, y, 1.1f, 1.1f);
                }
                g.setColour (Colours::white.withAlpha (0.035f));
                for (float y = r.getY() + 3.0f; y < r.getBottom() - 2.0f; y += 4.0f)
                {
                    const float off = std::fmod ((y - r.getY()) * 0.5f, 4.0f);
                    for (float x = r.getX() + 2.0f + off; x < r.getRight() - 2.0f; x += 4.0f)
                        g.fillEllipse (x, y + 1.0f, 1.1f, 0.6f);
                }
            }
            break;
        }
    }
}

void paintStarfield (Graphics& g, Rectangle<float> r, uint32 seed, bool mini)
{
    // stars stay sparse and faint behind the knob row so the controls read cleanly
    const auto calm = mini ? Rectangle<float>() : knobArea (EffectType::AuraRoom, r).expanded (6.0f, 8.0f);
    const auto calmText = mini ? Rectangle<float>() : nameArea (r).withTrimmedTop (36.0f).expanded (4.0f, 3.0f);
    const auto calmLive = mini ? Rectangle<float>() : liveArea (EffectType::AuraRoom, r).expanded (2.0f);
    // faint nebula clouds
    for (int i = 0; i < (mini ? 2 : 4); ++i)
    {
        const float cx = r.getX() + r.getWidth() * (0.2f + 0.7f * h01 (seed, (uint32) i * 5u));
        const float cy = r.getY() + r.getHeight() * h01 (seed, (uint32) i * 5u + 1u);
        const float rad = r.getHeight() * (0.6f + 0.9f * h01 (seed, (uint32) i * 5u + 2u));
        const Colour c = (i & 1) ? Colour (0xff6a4cff) : Colour (0xff2d7bff);
        ColourGradient neb (c.withAlpha (0.10f), cx, cy, c.withAlpha (0.0f), cx + rad, cy, true);
        g.setGradientFill (neb);
        g.fillEllipse (Rectangle<float> (rad * 2.6f, rad * 1.3f).withCentre ({ cx, cy }));
    }
    const int n = (int) (r.getWidth() * r.getHeight() / (mini ? 60.0f : 90.0f));
    for (int i = 0; i < n; ++i)
    {
        const float x = r.getX() + h01 (seed + 3u, (uint32) i * 4u) * r.getWidth();
        const float y = r.getY() + h01 (seed + 3u, (uint32) i * 4u + 1u) * r.getHeight();
        const float m = h01 (seed + 3u, (uint32) i * 4u + 2u);
        const bool inCalm = calm.contains (x, y) || calmText.contains (x, y) || calmLive.contains (x, y);
        if (inCalm && m < 0.55f) continue;
        const float sz = (0.4f + 1.1f * m * m * m) * (inCalm ? 0.7f : 1.0f);
        const float tint = h01 (seed + 3u, (uint32) i * 4u + 3u);
        const Colour c = tint < 0.33f ? Colour (0xffcfe0ff) : tint < 0.66f ? Colour (0xffffffff) : Colour (0xffe0d2ff);
        g.setColour (c.withAlpha ((0.25f + 0.6f * m) * (inCalm ? 0.45f : 1.0f)));
        g.fillEllipse (Rectangle<float> (sz, sz).withCentre ({ x, y }));
        if (m > 0.93f && ! mini && ! inCalm)
        {
            ColourGradient halo (c.withAlpha (0.35f), x, y, c.withAlpha (0.0f), x + 5.0f, y, true);
            g.setGradientFill (halo);
            g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre ({ x, y }));
            g.setColour (c.withAlpha (0.4f));
            g.fillRect (x - 4.0f, y - 0.25f, 8.0f, 0.5f);
            g.fillRect (x - 0.25f, y - 4.0f, 0.5f, 8.0f);
        }
    }
}

void paintMaterial (Graphics& g, EffectType t, const Style& st, Rectangle<float> r, bool mini)
{
    const float corner = mini ? 2.0f : 3.0f;
    // base gradient
    ColourGradient base (st.plate.brighter (st.finish == Finish::Brushed ? 0.18f : 0.06f), r.getX(), r.getY(),
                         st.plateDark, r.getX(), r.getBottom(), false);
    base.addColour (0.55, st.plate);
    g.setGradientFill (base);
    g.fillRoundedRectangle (r, corner);

    g.saveState();
    Path clip; clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);

    const uint32 seed = 0x5eed00u + (uint32) t * 977u;
    if (st.finish == Finish::Space) paintStarfield (g, r, seed, mini);
    paintTexture (g, st, r, seed);

    switch (st.finish)
    {
        case Finish::Brushed:
        {
            // anisotropic metallic highlight band
            ColourGradient band (Colours::white.withAlpha (0.0f), r.getX(), r.getY(), Colours::white.withAlpha (0.0f), r.getRight(), r.getY(), false);
            band.addColour (0.30, Colours::white.withAlpha (0.16f));
            band.addColour (0.42, Colours::white.withAlpha (0.02f));
            band.addColour (0.70, Colours::white.withAlpha (0.10f));
            g.setGradientFill (band);
            g.fillRect (r);
            break;
        }
        case Finish::Lacquer:
        {
            // deep gloss: diagonal specular streak + bright clear-coat edge
            Path streak;
            const float w = r.getWidth(), h = r.getHeight();
            streak.startNewSubPath (r.getX() + w * 0.18f, r.getY());
            streak.lineTo (r.getX() + w * 0.30f, r.getY());
            streak.lineTo (r.getX() + w * 0.30f - h * 0.9f, r.getBottom());
            streak.lineTo (r.getX() + w * 0.18f - h * 0.9f, r.getBottom());
            streak.closeSubPath();
            ColourGradient sg (Colours::white.withAlpha (0.10f), r.getX() + w * 0.2f, r.getY(), Colours::white.withAlpha (0.0f), r.getX() + w * 0.2f, r.getBottom(), false);
            g.setGradientFill (sg);
            g.fillPath (streak);
            ColourGradient top (Colours::white.withAlpha (0.16f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + h * 0.35f, false);
            g.setGradientFill (top);
            g.fillRect (r);
            break;
        }
        case Finish::Satin:
        {
            ColourGradient glow (th::accentDeep.withAlpha (0.16f), r.getX() + r.getWidth() * 0.15f, r.getY() + r.getHeight() * 0.7f,
                                 th::accentDeep.withAlpha (0.0f), r.getX() + r.getWidth() * 0.55f, r.getY(), true);
            g.setGradientFill (glow);
            g.fillRect (r);
            ColourGradient top (Colours::white.withAlpha (0.07f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.4f, false);
            g.setGradientFill (top);
            g.fillRect (r);
            break;
        }
        case Finish::Powder:
        {
            ColourGradient top (Colours::white.withAlpha (0.14f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.5f, false);
            g.setGradientFill (top);
            g.fillRect (r);
            break;
        }
        case Finish::Graphite:
        {
            ColourGradient vign (Colours::black.withAlpha (0.0f), r.getCentreX(), r.getCentreY(), Colours::black.withAlpha (0.25f), r.getX(), r.getY(), true);
            g.setGradientFill (vign);
            g.fillRect (r);
            break;
        }
        case Finish::Space:
        case Finish::Crackle:
            break;
    }
    g.restoreState();

    // bevel: bright top edge, dark bottom edge, soft side edges
    g.setColour (Colours::white.withAlpha (st.lightPanel ? 0.45f : 0.20f));
    g.fillRect (r.getX() + corner, r.getY(), r.getWidth() - 2.0f * corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRect (r.getX() + corner, r.getBottom() - 1.0f, r.getWidth() - 2.0f * corner, 1.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

void paintScrew (Graphics& g, Point<float> c, float rad, float angle)
{
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (rad * 2.3f, rad * 2.3f).withCentre (c.translated (0.0f, rad * 0.25f)));
    ColourGradient head (Colour (0xfff4f7fc), c.x - rad * 0.5f, c.y - rad * 0.7f, Colour (0xff4a5160), c.x + rad * 0.6f, c.y + rad * 0.9f, true);
    head.addColour (0.55, Colour (0xffaab2c2));
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
        g.drawLine (Line<float> (c - d, c + d).withShortenedStart (0.0f).withShortenedEnd (0.0f), w * 0.5f);
        g.setColour (Colour (0xff1a1d24));
        g.drawLine ({ (c - d).translated (-0.25f, -0.35f), (c + d).translated (-0.25f, -0.35f) }, w);
    }
    // top-left highlight
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (rad * 0.5f, rad * 0.35f).withCentre (c.translated (-rad * 0.45f, -rad * 0.55f)));
}

void paintEar (Graphics& g, Rectangle<float> ear, bool left, const Style& st, bool mini, uint32 seed)
{
    // the ear is the folded steel flange: slightly darker, with a bend highlight towards the panel
    g.setColour (Colours::black.withAlpha (0.16f));
    g.fillRect (ear);
    const float bx = left ? ear.getRight() - 1.0f : ear.getX();
    g.setColour (Colours::white.withAlpha (st.lightPanel ? 0.35f : 0.12f));
    g.fillRect (bx, ear.getY() + 2.0f, 0.7f, ear.getHeight() - 4.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (left ? bx - 1.0f : bx + 0.7f, ear.getY() + 2.0f, 1.0f, ear.getHeight() - 4.0f);

    const float slotW = mini ? 4.6f : ear.getWidth() * 0.62f;
    const float slotH = mini ? 3.0f : 9.0f;
    const float inset = mini ? 7.0f : 20.0f;
    for (int i = 0; i < 2; ++i)
    {
        const float cy = i == 0 ? ear.getY() + inset : ear.getBottom() - inset;
        const auto slot = Rectangle<float> (slotW, slotH).withCentre ({ ear.getCentreX() + (left ? -0.5f : 0.5f), cy });
        g.setColour (Colours::white.withAlpha (st.lightPanel ? 0.4f : 0.12f));
        g.fillRoundedRectangle (slot.translated (0.0f, 0.8f), slotH * 0.5f);
        g.setColour (Colour (0xff020203));
        g.fillRoundedRectangle (slot, slotH * 0.5f);
        if (! mini)
        {
            // rack screw sitting in the oval slot, slightly off-centre like a real mounted unit
            const float off = (h01 (seed, (uint32) i + (left ? 0u : 4u)) - 0.5f) * 3.0f;
            paintScrew (g, { slot.getCentreX() + off, cy }, 5.6f, h01 (seed, (uint32) i + (left ? 10u : 20u)) * 3.1f);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// name sticker / frame styles
// ---------------------------------------------------------------------------------------------
void paintSmallPrint (Graphics& g, const Style& st, Rectangle<float> r, float h)
{
    // "CHORUS  ·  TRAPWIN" under the name, TRAPWIN gets a tiny 4-point sparkle
    const Font f = mono (h).boldened();
    const Colour c = st.ink.withAlpha (st.lightPanel ? 0.85f : 0.78f);
    const String type (st.type);
    const float tw = GlyphArrangement::getStringWidth (f, type);
    printed (g, type, r, f, c, Justification::centredLeft, st.lightPanel);
    const float dotX = r.getX() + tw + h * 0.9f;
    g.setColour (st.accent.withAlpha (st.lightPanel ? 0.9f : 0.85f));
    if (st.lightPanel) g.setColour (st.ink.withAlpha (0.7f));
    g.fillEllipse (Rectangle<float> (h * 0.28f, h * 0.28f).withCentre ({ dotX, r.getCentreY() }));
    const float bx = dotX + h * 0.9f;
    // sparkle
    Path sp;
    const Point<float> sc (bx + h * 0.35f, r.getCentreY());
    const float s1 = h * 0.42f, s2 = h * 0.1f;
    sp.startNewSubPath (sc.x, sc.y - s1);
    sp.quadraticTo (sc.x + s2, sc.y - s2, sc.x + s1, sc.y);
    sp.quadraticTo (sc.x + s2, sc.y + s2, sc.x, sc.y + s1);
    sp.quadraticTo (sc.x - s2, sc.y + s2, sc.x - s1, sc.y);
    sp.quadraticTo (sc.x - s2, sc.y - s2, sc.x, sc.y - s1);
    g.setColour (st.lightPanel ? st.ink.withAlpha (0.8f) : th::silver);
    g.fillPath (sp);
    printed (g, "TRAPWIN", r.withLeft (bx + h * 0.95f), f, c, Justification::centredLeft, st.lightPanel);
}

void paintName (Graphics& g, EffectType t, const Style& st, Rectangle<float> area, bool mini)
{
    const String name = effectInfo (t).name.toUpperCase();
    const float nameH = mini ? 17.0f : 30.0f;
    auto nameBox = area.removeFromTop (mini ? 22.0f : 36.0f);
    const float maxW = nameBox.getWidth() - (mini ? 4.0f : 16.0f);
    Font f = fitted (th::stencilFont (nameH), name, maxW);
    const float tw = GlyphArrangement::getStringWidth (f, name);

    switch (t)
    {
        case EffectType::Doubles:
        {
            // double strike print: a pale offset ghost under the dark ink, framed by a thin engraved rule
            Path p, ghost;
            const float base = nameBox.getY() + nameBox.getHeight() * 0.80f;
            textPath (ghost, name, f, nameBox.getX() + (mini ? 1.6f : 3.0f), base - (mini ? 1.0f : 2.0f));
            textPath (p, name, f, nameBox.getX(), base);
            g.setColour (Colour (0xffe8fdff).withAlpha (0.55f));
            g.fillPath (ghost);
            g.setColour (Colours::white.withAlpha (0.4f));
            g.fillPath (p, AffineTransform::translation (0.0f, 0.8f));
            g.setColour (st.ink);
            g.fillPath (p);
            if (! mini)
            {
                g.setColour (st.ink.withAlpha (0.55f));
                g.fillRect (nameBox.getX(), nameBox.getBottom() - 1.0f, tw + 6.0f, 1.2f);
                g.setColour (Colours::white.withAlpha (0.45f));
                g.fillRect (nameBox.getX(), nameBox.getBottom() + 0.2f, tw + 6.0f, 0.8f);
            }
            break;
        }
        case EffectType::Swirl:
        {
            // gold-foil name inside an engraved pill frame
            const auto frame = Rectangle<float> (tw + (mini ? 12.0f : 26.0f), nameBox.getHeight() - (mini ? 2.0f : 4.0f))
                                   .withPosition (nameBox.getX() - (mini ? 2.0f : 0.0f), nameBox.getY() + (mini ? 1.0f : 2.0f));
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (frame, frame.getHeight() * 0.5f);
            g.setColour (st.accent.withAlpha (0.85f));
            g.drawRoundedRectangle (frame.reduced (0.5f), frame.getHeight() * 0.5f, mini ? 0.8f : 1.3f);
            g.setColour (st.accent.withAlpha (0.35f));
            g.drawRoundedRectangle (frame.reduced (mini ? 2.0f : 3.0f), frame.getHeight() * 0.5f, 0.6f);
            Path p;
            textPath (p, name, f, frame.getX() + (frame.getWidth() - tw) * 0.5f, frame.getY() + frame.getHeight() * 0.5f + nameH * 0.36f);
            const auto pb = p.getBounds();
            ColourGradient gold (Colour (0xfffff0c4), 0.0f, pb.getY(), Colour (0xffa26f1c), 0.0f, pb.getBottom(), false);
            gold.addColour (0.45, Colour (0xffe8bb62));
            gold.addColour (0.55, Colour (0xff8a5a14));
            gold.addColour (0.75, Colour (0xfff3d38a));
            g.setColour (Colours::black.withAlpha (0.5f));
            g.fillPath (p, AffineTransform::translation (0.0f, 1.0f));
            g.setGradientFill (gold);
            g.fillPath (p);
            break;
        }
        case EffectType::AdLibThrow:
        {
            // ice-white vinyl label tape, slightly askew, navy print
            const auto tape = Rectangle<float> (tw + (mini ? 10.0f : 22.0f), nameBox.getHeight() - (mini ? 2.0f : 6.0f))
                                  .withPosition (nameBox.getX() - (mini ? 2.0f : 0.0f), nameBox.getY() + (mini ? 1.0f : 3.0f));
            const auto rot = AffineTransform::rotation (-0.022f, tape.getCentreX(), tape.getCentreY());
            Path tp; tp.addRoundedRectangle (tape, 1.5f);
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillPath (tp, rot.translated (0.0f, 1.4f));
            ColourGradient paper (Colour (0xffeef4ff), 0.0f, tape.getY(), Colour (0xffc7d6ef), 0.0f, tape.getBottom(), false);
            g.setGradientFill (paper);
            g.fillPath (tp, rot);
            // notched tape ends
            g.setColour (st.plate);
            for (int k = 0; k < 3; ++k)
            {
                const float yy = tape.getY() + tape.getHeight() * (0.2f + 0.3f * (float) k);
                Path n; n.addTriangle (tape.getRight() + 0.5f, yy - 2.5f, tape.getRight() - 2.5f, yy, tape.getRight() + 0.5f, yy + 2.5f);
                g.fillPath (n, rot);
            }
            Path p;
            textPath (p, name, f, tape.getX() + (mini ? 5.0f : 10.0f), tape.getCentreY() + nameH * 0.36f);
            g.setColour (Colour (0xff14203d));
            g.fillPath (p, rot);
            break;
        }
        case EffectType::AuraRoom:
        {
            th::drawChromeText (g, name, nameBox.withWidth (maxW + 8.0f).translated (mini ? 0.0f : 2.0f, 0.0f), f,
                                Justification::centredLeft, mini ? 0.6f : 1.2f);
            break;
        }
        case EffectType::WideBody:
        {
            // extra-wide print inside a double rule frame (black on mint)
            Font wf = th::stencilFont (nameH).withHorizontalScale (mini ? 1.0f : 1.12f);
            wf = fitted (wf, name, maxW - (mini ? 6.0f : 20.0f));
            const float ww = GlyphArrangement::getStringWidth (wf, name);
            const auto frame = Rectangle<float> (ww + (mini ? 8.0f : 22.0f), nameBox.getHeight() - (mini ? 2.0f : 4.0f))
                                   .withPosition (nameBox.getX() - (mini ? 2.0f : 0.0f), nameBox.getY() + (mini ? 1.0f : 2.0f));
            if (! mini)
            {
                g.setColour (st.ink);
                g.drawRect (frame, 2.0f);
                g.drawRect (frame.reduced (3.5f), 0.8f);
                // stereo arrows on the frame sides
                for (int k = 0; k < 2; ++k)
                {
                    const float x = k == 0 ? frame.getX() : frame.getRight();
                    const float d = k == 0 ? -5.0f : 5.0f;
                    Path a; a.addTriangle (x + d, frame.getCentreY(), x + d * 0.1f, frame.getCentreY() - 4.0f, x + d * 0.1f, frame.getCentreY() + 4.0f);
                    g.fillPath (a);
                }
            }
            printed (g, name, frame, wf, st.ink, Justification::centred, true);
            break;
        }
        case EffectType::Chopped:
        {
            // stencil name with a dashed "cut line" frame around it
            const auto frame = Rectangle<float> (tw + (mini ? 10.0f : 22.0f), nameBox.getHeight() - (mini ? 2.0f : 4.0f))
                                   .withPosition (nameBox.getX() - (mini ? 2.0f : 0.0f), nameBox.getY() + (mini ? 1.0f : 2.0f));
            if (! mini)
            {
                Path fr; fr.addRectangle (frame);
                Path dashed;
                const float dash[] = { 5.0f, 3.0f };
                PathStrokeType (1.3f).createDashedStroke (dashed, fr, dash, 2);
                g.setColour (st.accent.withAlpha (0.75f));
                g.fillPath (dashed);
                // scissors-like tick marks at the corners
                g.setColour (st.accent.withAlpha (0.9f));
                g.fillRect (frame.getX() - 3.0f, frame.getY() - 0.6f, 3.0f, 1.3f);
                g.fillRect (frame.getRight(), frame.getBottom() - 0.6f, 3.0f, 1.3f);
            }
            Path p;
            textPath (p, name, f, frame.getX() + (frame.getWidth() - tw) * 0.5f, frame.getCentreY() + nameH * 0.36f);
            // stencil bridges: thin horizontal gaps cut through the letters
            g.setColour (Colours::black.withAlpha (0.5f));
            g.fillPath (p, AffineTransform::translation (0.0f, 1.0f));
            g.saveState();
            const auto pb = p.getBounds();
            g.excludeClipRegion (Rectangle<float> (pb.getX() - 2.0f, pb.getCentreY() - 0.7f, pb.getWidth() + 4.0f, 1.4f).toNearestInt());
            g.setColour (st.ink);
            g.fillPath (p);
            g.restoreState();
            break;
        }
        case EffectType::RedLine:
        {
            // white name on a black plate, red underline (the "red line")
            const auto box = Rectangle<float> (tw + (mini ? 10.0f : 24.0f), nameBox.getHeight() - (mini ? 2.0f : 4.0f))
                                 .withPosition (nameBox.getX() - (mini ? 2.0f : 0.0f), nameBox.getY() + (mini ? 1.0f : 2.0f));
            g.setColour (Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (box.translated (0.0f, 1.0f), 2.0f);
            g.setColour (Colour (0xff0d0d0f));
            g.fillRoundedRectangle (box, 2.0f);
            g.setColour (Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (box.reduced (0.5f), 2.0f, 0.8f);
            const float lineY = box.getBottom() - (mini ? 3.0f : 5.0f);
            g.setColour (st.accent.withAlpha (0.35f));
            g.fillRect (box.getX() + 3.0f, lineY - 1.5f, box.getWidth() - 6.0f, mini ? 2.5f : 4.0f);
            g.setColour (st.accent);
            g.fillRect (box.getX() + 3.0f, lineY - 0.5f, box.getWidth() - 6.0f, mini ? 1.0f : 2.0f);
            Path p;
            textPath (p, name, f, box.getX() + (box.getWidth() - tw) * 0.5f, box.getCentreY() + nameH * 0.30f - (mini ? 1.0f : 2.0f));
            g.setColour (th::osdWhite);
            g.fillPath (p);
            break;
        }
        default: break;
    }

    if (! mini)
        paintSmallPrint (g, st, area.withHeight (13.0f).translated (0.0f, 2.0f), 9.5f);
}

void paintLcdFrame (Graphics& g, const Style& st, Rectangle<float> r)
{
    // empty LCD glass for the unit preset display (text drawn by RackView)
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.expanded (3.0f).translated (0.0f, 1.0f), 4.0f);
    ColourGradient bezel (Colour (0xff3a3f4a), 0.0f, r.getY() - 3.0f, Colour (0xff0b0c10), 0.0f, r.getBottom() + 3.0f, false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (r.expanded (2.5f), 3.5f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r.expanded (2.5f).reduced (0.4f), 3.5f, 0.6f);
    const Colour glass = st.finish == Finish::Space ? Colour (0xff070812) : Colour (0xff060a10);
    ColourGradient gl (glass.brighter (0.25f), 0.0f, r.getY(), glass, 0.0f, r.getBottom(), false);
    g.setGradientFill (gl);
    g.fillRoundedRectangle (r, 2.0f);
    ColourGradient sh (Colours::black.withAlpha (0.6f), 0.0f, r.getY(), Colours::transparentBlack, 0.0f, r.getY() + 5.0f, false);
    g.setGradientFill (sh);
    g.fillRoundedRectangle (r, 2.0f);
    glassSheen (g, r, 2.0f);
}

// window that hosts the live element (static part)
Rectangle<float> liveWindow (EffectType t, Rectangle<float> live)
{
    switch (t)
    {
        case EffectType::Doubles:
        case EffectType::AdLibThrow:
        case EffectType::Chopped:
        case EffectType::RedLine:   return live.reduced (1.0f, 2.0f);
        case EffectType::WideBody:  return live.withWidth (live.getHeight()).reduced (2.0f);
        case EffectType::Swirl:     return live.withWidth (live.getHeight()).reduced (1.0f);
        default:                    return live;
    }
}

void paintStaticLive (Graphics& g, EffectType t, const Style& st, Rectangle<float> live, bool mini)
{
    const auto w = liveWindow (t, live);
    const float corner = mini ? 2.0f : 4.0f;
    switch (t)
    {
        case EffectType::Doubles:    recess (g, w, corner, Colour (0xff04161c)); break;
        case EffectType::AdLibThrow: recess (g, w, corner, Colour (0xff040812)); break;
        case EffectType::Chopped:    recess (g, w, corner, Colour (0xff050c07)); break;
        case EffectType::RedLine:    recess (g, w, corner, Colour (0xff0a0a0b)); break;
        case EffectType::WideBody:
        {
            // CRT tube in a black bezel
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (w.expanded (mini ? 1.5f : 3.0f).translated (0.0f, 1.2f), corner + 3.0f);
            g.setColour (Colour (0xff15181a));
            g.fillRoundedRectangle (w.expanded (mini ? 1.5f : 3.0f), corner + 3.0f);
            recess (g, w, corner + 2.0f, Colour (0xff04140d));
            break;
        }
        case EffectType::Swirl:
        {
            // chrome porthole
            const auto c = w.getCentre();
            const float R = w.getWidth() * 0.5f;
            g.setColour (Colours::black.withAlpha (0.45f));
            g.fillEllipse (w.expanded (1.5f).translated (0.0f, 1.5f));
            ColourGradient ring (Colour (0xfff1f3f8), c.x - R, c.y - R, Colour (0xff3a3446), c.x + R, c.y + R, false);
            ring.addColour (0.5, Colour (0xff9a96a6));
            g.setGradientFill (ring);
            g.fillEllipse (w);
            const auto inner = w.reduced (mini ? 1.6f : 3.2f);
            g.setColour (Colour (0xff09040f));
            g.fillEllipse (inner);
            break;
        }
        case EffectType::AuraRoom:
        {
            if (! mini)
            {
                // fine hairline "viewport" corners only - the tail glows straight on the starfield
                g.setColour (st.ink.withAlpha (0.22f));
                const float L = 8.0f;
                for (int k = 0; k < 4; ++k)
                {
                    const float x = (k & 1) ? w.getRight() : w.getX();
                    const float y = (k & 2) ? w.getBottom() : w.getY();
                    const float dx = (k & 1) ? -L : L, dy = (k & 2) ? -L : L;
                    g.drawLine (x, y, x + dx, y, 0.8f);
                    g.drawLine (x, y, x, y + dy, 0.8f);
                }
            }
            break;
        }
        default: break;
    }
}

void paintStatic (Graphics& g, EffectType t, Rectangle<float> plate, bool mini)
{
    const auto st = style (t);
    paintMaterial (g, t, st, plate, mini);
    const uint32 seed = 0xa11ceu + (uint32) t * 131u;

    const float earW = mini ? 8.0f : 18.0f;
    paintEar (g, plate.withWidth (earW), true, st, mini, seed);
    paintEar (g, plate.withLeft (plate.getRight() - earW), false, st, mini, seed);

    if (mini)
    {
        const auto inner = plate.reduced (earW + 4.0f, 0.0f);
        paintName (g, t, st, Rectangle<float> (inner.getX(), plate.getY() + 2.0f, inner.getWidth() * 0.62f, 24.0f), true);
        // small type line under the mini name
        const Font f = mono (7.0f).boldened();
        printed (g, String (st.type) + "  TRAPWIN", Rectangle<float> (inner.getX(), plate.getBottom() - 14.0f, 110.0f, 10.0f), f,
                 st.ink.withAlpha (0.7f), Justification::centredLeft, st.lightPanel);
        auto live = Rectangle<float> (plate.getRight() - earW - 6.0f - 84.0f, plate.getY() + 6.0f, 84.0f, plate.getHeight() - 12.0f);
        paintStaticLive (g, t, st, live, true);
        return;
    }

    paintName (g, t, st, nameArea (plate), false);
    paintLcdFrame (g, st, presetLcdArea (plate));
    paintStaticLive (g, t, st, liveArea (t, plate), false);

    // engraved model line above the knob row (kept faint so the knob area stays calm)
    const auto ka = knobArea (t, plate);
    printed (g, st.model, Rectangle<float> (ka.getX() + 2.0f, plate.getY() + 14.0f, 260.0f, 12.0f), mono (8.5f),
             st.ink.withAlpha (st.lightPanel ? 0.55f : 0.40f), Justification::centredLeft, st.lightPanel);

    // engraved divider between the identity block and the knob row
    const float dx = ka.getX() - 7.0f;
    g.setColour (Colours::black.withAlpha (st.lightPanel ? 0.28f : 0.45f));
    g.fillRect (dx, plate.getY() + 14.0f, 1.0f, plate.getHeight() - 28.0f);
    g.setColour (Colours::white.withAlpha (st.lightPanel ? 0.35f : 0.08f));
    g.fillRect (dx + 1.0f, plate.getY() + 14.0f, 0.8f, plate.getHeight() - 28.0f);

    if (t == EffectType::RedLine)
    {
        // the red line: a thin glowing rule along the foot of the panel, ear to ear
        const float y = plate.getBottom() - 5.0f;
        const float x0 = plate.getX() + earW + 3.0f, x1 = plate.getRight() - earW - 3.0f;
        g.setColour (st.accent.withAlpha (0.18f));
        g.fillRect (x0, y - 2.0f, x1 - x0, 5.0f);
        g.setColour (st.accent);
        g.fillRect (x0, y - 0.6f, x1 - x0, 1.6f);
        g.setColour (Colour (0xffff8a92).withAlpha (0.8f));
        g.fillRect (x0, y - 0.6f, x1 - x0, 0.5f);
    }
}

// cache of the static layer, keyed by type / size / pixel scale / miniature
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

// ---------------------------------------------------------------------------------------------
// live elements
// ---------------------------------------------------------------------------------------------
void liveDoubles (Graphics& g, const State& s, Rectangle<float> w, bool mini)
{
    constexpr auto T = EffectType::Doubles;
    const int voices = jlimit (1, 4, roundToInt (real (T, s, 2)));
    const float rate = real (T, s, 0), depth = real (T, s, 1), spread = real (T, s, 3);
    const double tm = animTime (s);
    const Colour cyan (0xff9ff4ff);

    auto in = w.reduced (mini ? 3.0f : 7.0f, mini ? 2.0f : 5.0f);
    auto ledArea = in.removeFromLeft (mini ? 30.0f : 74.0f);
    // 4 voice LEDs, each breathing with its own LFO phase
    const float lr = mini ? 2.4f : 4.2f;
    for (int i = 0; i < 4; ++i)
    {
        const float cx = ledArea.getX() + (mini ? 4.0f : 8.0f) + (float) i * (mini ? 7.0f : 18.0f);
        const float cy = mini ? ledArea.getCentreY() : ledArea.getY() + 16.0f;
        const bool on = i < voices;
        const float wob = on ? 0.78f + 0.22f * std::sin ((float) (tm * twoPi * rate) + (float) i * 1.7f) : 0.0f;
        led (g, { cx, cy }, lr, cyan, wob);
        if (! mini)
            text (g, String (i + 1), Rectangle<float> (16.0f, 10.0f).withCentre ({ cx, cy + 14.0f }), mono (8.0f),
                  cyan.withAlpha (on ? 0.75f : 0.25f), Justification::centred);
    }
    if (! mini)
        text (g, "VOICES", Rectangle<float> (ledArea.getX() + 2.0f, ledArea.getBottom() - 11.0f, 70.0f, 10.0f), mono (7.5f),
              cyan.withAlpha (0.45f), Justification::centredLeft);

    // overlapped voice traces: same wave, each voice drifting in phase/time like a real double
    in.removeFromLeft (mini ? 2.0f : 6.0f);
    g.setColour (cyan.withAlpha (0.10f));
    g.fillRect (in.getX(), in.getCentreY(), in.getWidth(), 0.6f);
    for (int v = voices - 1; v >= 0; --v)
    {
        Path p;
        const float drift = (0.25f + 1.4f * depth) * std::sin ((float) (tm * twoPi * rate) + (float) v * twoPi / (float) voices);
        const float amp = in.getHeight() * (0.36f - 0.035f * (float) v) * (0.8f + 0.2f * spread);
        const int n = mini ? 40 : 90;
        for (int i = 0; i <= n; ++i)
        {
            const float u = (float) i / (float) n;
            const float ph = u * twoPi * 2.2f + drift + (float) v * 0.35f * spread;
            const float y = in.getCentreY() - amp * (0.8f * std::sin (ph) + 0.2f * std::sin (ph * 2.0f + 0.7f));
            const float x = in.getX() + u * in.getWidth();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        const float a = v == 0 ? 0.95f : 0.55f - 0.08f * (float) v;
        g.setColour (cyan.withAlpha (a * 0.25f));
        g.strokePath (p, PathStrokeType (mini ? 2.0f : 3.2f));
        g.setColour (cyan.withAlpha (a));
        g.strokePath (p, PathStrokeType (mini ? 0.8f : 1.2f));
    }
}

void liveSwirl (Graphics& g, const State& s, Rectangle<float> live, Rectangle<float> port, bool mini)
{
    constexpr auto T = EffectType::Swirl;
    const bool sync = real (T, s, 1) > 0.5f;
    const float hz = sync ? (float) (s.bpm / 60.0) / jmax (0.0625f, syncBeats (real (T, s, 6))) : real (T, s, 0);
    const int stages = jlimit (2, 12, roundToInt (real (T, s, 2)));
    const float depth = real (T, s, 4), fb = real (T, s, 3);
    const int arms = jlimit (1, 6, stages / 2);

    // visual rotation speed follows the LFO rate (clamped so fast rates stay readable)
    const float revPerSec = jlimit (0.03f, 2.5f, hz);
    const float angle = (float) std::fmod (animTime (s) * (double) revPerSec, 1.0) * twoPi + 0.4f;

    const auto inner = port.reduced (mini ? 1.6f : 3.2f);
    const auto c = inner.getCentre();
    const float R = inner.getWidth() * 0.5f - 0.5f;
    g.saveState();
    Path clip; clip.addEllipse (inner);
    g.reduceClipRegion (clip);
    ColourGradient bg (Colour (0xff3a1260), c.x, c.y, Colour (0xff07020c), c.x + R, c.y, true);
    g.setGradientFill (bg);
    g.fillEllipse (inner);

    const float twist = 2.4f + 3.5f * depth;   // how tightly the arms wind
    for (int a = 0; a < arms; ++a)
    {
        Path p;
        const int n = 48;
        for (int i = 0; i <= n; ++i)
        {
            const float u = (float) i / (float) n;
            const float rr = R * (0.06f + 0.98f * u);
            const float th = angle + (float) a * twoPi / (float) arms + u * twist;
            const Point<float> pt (c.x + rr * std::cos (th), c.y + rr * std::sin (th));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        const float wdt = mini ? 1.2f : 2.4f;
        g.setColour (Colour (0xffb565ff).withAlpha (0.28f));
        g.strokePath (p, PathStrokeType (wdt * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xffe3c4ff).withAlpha (0.9f));
        g.strokePath (p, PathStrokeType (wdt, PathStrokeType::curved, PathStrokeType::rounded));
    }
    // glowing hub
    ColourGradient hub (Colours::white.withAlpha (0.9f), c.x, c.y, Colour (0xffb565ff).withAlpha (0.0f), c.x + R * 0.35f, c.y, true);
    g.setGradientFill (hub);
    g.fillEllipse (Rectangle<float> (R * 0.7f, R * 0.7f).withCentre (c));
    // glass dome highlight
    ColourGradient dome (Colours::white.withAlpha (0.22f), c.x - R * 0.4f, c.y - R * 0.7f, Colours::white.withAlpha (0.0f), c.x, c.y, true);
    g.setGradientFill (dome);
    g.fillEllipse (inner);
    g.restoreState();

    if (mini) return;

    // readouts printed on the lacquer beside the porthole
    const auto st = style (T);
    auto info = live.withLeft (port.getRight() + 10.0f);
    const Font lf = mono (8.0f);
    printed (g, sync ? "SYNC" : "RATE", info.removeFromTop (11.0f), lf, st.ink.withAlpha (0.55f), Justification::centredLeft, false);
    const float rateHz = real (T, s, 0);
    const String rateTxt = sync ? fmt (T, s, 6) : (rateHz < 10.0f ? String (rateHz, 2) + " Hz" : fmt (T, s, 0));
    th::drawGlowText (g, rateTxt, info.removeFromTop (14.0f), th::vfdFont (13.0f), Colour (0xffe3c4ff), Justification::centredLeft, 0.6f);
    info.removeFromTop (5.0f);
    printed (g, "STAGES", info.removeFromTop (10.0f), lf, st.ink.withAlpha (0.55f), Justification::centredLeft, false);
    auto pips = info.removeFromTop (12.0f);
    for (int i = 0; i < 12; ++i)
    {
        const float x = pips.getX() + 3.0f + (float) i * 11.0f;
        led (g, { x, pips.getCentreY() }, 2.3f, Colour (0xffd59bff), i < stages ? 0.9f : 0.0f);
    }
    // feedback hairline gauge
    auto fbBar = info.removeFromTop (8.0f).withWidth (128.0f).reduced (0.0f, 2.5f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (fbBar, 1.5f);
    g.setColour (st.accent.withAlpha (0.85f));
    g.fillRoundedRectangle (fbBar.withWidth (jmax (2.0f, fbBar.getWidth() * fb)), 1.5f);
}

void liveAdLib (Graphics& g, const State& s, Rectangle<float> w, bool mini)
{
    constexpr auto T = EffectType::AdLibThrow;
    const float idx = real (T, s, 0);
    const float beats = syncBeats (idx);
    const bool synced = real (T, s, 1) > 0.5f;
    const float fb = real (T, s, 2);
    const bool ping = real (T, s, 3) > 0.5f;
    const double bpm = synced ? jmax (20.0, s.bpm) : 120.0;   // free time uses the engine's 120 BPM mapping
    const float ms = (float) (beats * 60.0 / bpm * 1000.0);
    const String msTxt = ms >= 1000.0f ? String (ms / 1000.0f, 2) + " s" : String (roundToInt (ms)) + " ms";
    const String big = synced ? fmt (T, s, 0) : msTxt;
    const Colour ice = th::accent;

    auto in = w.reduced (mini ? 3.0f : 8.0f, mini ? 2.0f : 4.0f);
    auto disp = in.removeFromLeft (mini ? in.getWidth() * 0.62f : 110.0f);

    // dot-matrix backdrop (unlit segments) + glowing time
    if (! mini)
    {
        g.setColour (ice.withAlpha (0.05f));
        for (float y = disp.getY() + 4.0f; y < disp.getY() + 34.0f; y += 2.5f)
            for (float x = disp.getX(); x < disp.getRight(); x += 2.5f)
                g.fillRect (x, y, 1.2f, 1.2f);
    }
    th::drawGlowText (g, big, disp.withHeight (mini ? disp.getHeight() : 36.0f), th::vfdFont (mini ? 15.0f : 27.0f),
                      ice, Justification::centredLeft, mini ? 0.7f : 1.0f);
    if (! mini)
    {
        const String sub = synced ? msTxt + "  @ " + String (roundToInt (s.bpm)) + " BPM" : "FREE  @ 120 BPM GRID";
        text (g, sub, Rectangle<float> (disp.getX(), disp.getBottom() - 12.0f, disp.getWidth() + 10.0f, 11.0f), mono (7.5f),
              ice.withAlpha (0.55f), Justification::centredLeft);
    }

    // echo taps: bars decaying by FEEDBACK; the newest throw lights up in tempo
    in.removeFromLeft (mini ? 3.0f : 6.0f);
    const int taps = mini ? 5 : 7;
    const double pos = s.reduceMotion ? 0.0 : s.beats / jmax (0.0625, (double) beats);
    const int cur = (int) std::floor (pos) % taps;
    const float frac = (float) (pos - std::floor (pos));
    const float rowH = ping ? in.getHeight() * 0.5f : in.getHeight();
    const float tw = in.getWidth() / (float) taps;
    for (int k = 0; k < taps; ++k)
    {
        const float level = std::pow (jmax (0.05f, fb), (float) k) * 0.95f + 0.05f;
        const bool right = ping && (k & 1);
        auto row = Rectangle<float> (in.getX() + tw * (float) k, in.getY() + (right ? rowH : 0.0f), tw, rowH).reduced (mini ? 0.7f : 1.5f, mini ? 0.6f : 1.5f);
        const float hgt = row.getHeight() * level;
        const float bw = jmax (1.5f, row.getWidth() * 0.62f);
        auto lane = row.withSizeKeepingCentre (bw, row.getHeight());
        auto bar = lane.withTop (lane.getBottom() - hgt);
        const float flash = (! s.reduceMotion && k == cur) ? (1.0f - frac) : 0.0f;
        g.setColour (ice.withAlpha (0.08f));
        g.fillRoundedRectangle (lane, 1.0f);
        g.setColour (ice.withAlpha (0.10f + 0.25f * flash));
        g.fillRoundedRectangle (bar.expanded (1.5f), 2.0f);
        ColourGradient bg (ice.withAlpha (0.55f + 0.45f * level), 0.0f, bar.getY(), th::accentDeep.withAlpha (0.5f), 0.0f, bar.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bar, 1.0f);
        if (flash > 0.0f)
        {
            g.setColour (Colours::white.withAlpha (0.6f * flash));
            g.fillRect (bar.withHeight (jmin (bar.getHeight(), 2.0f)));
        }
    }
    if (! mini && ping)
    {
        text (g, "L", Rectangle<float> (in.getRight() + 1.0f, in.getY(), 8.0f, rowH), mono (7.0f), ice.withAlpha (0.5f));
        text (g, "R", Rectangle<float> (in.getRight() + 1.0f, in.getY() + rowH, 8.0f, rowH), mono (7.0f), ice.withAlpha (0.5f));
    }
}

void liveAura (Graphics& g, const State& s, Rectangle<float> w, bool mini)
{
    constexpr auto T = EffectType::AuraRoom;
    const float decay = real (T, s, 2), pre = real (T, s, 3), size = real (T, s, 1), damp = real (T, s, 4);
    const int type = roundToInt (real (T, s, 0));
    const double tm = animTime (s);
    // decay 0.2..20 s on a log scale -> tail length 18..100 % of the window
    const float lenN = jlimit (0.0f, 1.0f, std::log (decay / 0.2f) / std::log (100.0f));
    auto in = w.reduced (mini ? 2.0f : 6.0f, mini ? 2.0f : 4.0f);
    if (! mini) in.removeFromBottom (11.0f);
    const float x0 = in.getX() + in.getWidth() * 0.10f * (pre / 200.0f) + 1.0f;
    const float len = (in.getRight() - x0) * (0.18f + 0.82f * lenN);
    const float base = in.getBottom();
    const float H = in.getHeight() * 0.95f;
    const float energy = 0.75f + 0.25f * jlimit (0.0f, 1.0f, s.outLevel * 2.0f);

    const Colour hot = type == 3 ? Colour (0xff8f7cff) : Colour (0xffcfe0ff);
    const Colour mid = type == 1 ? Colour (0xff7fd6ff) : Colour (0xff9a7dff);

    // early reflections (count by SIZE)
    const int refl = 3 + roundToInt (size * 6.0f);
    for (int i = 0; i < refl; ++i)
    {
        const float u = (float) (i + 1) / (float) (refl + 2) * 0.22f;
        const float x = x0 + len * u;
        const float hgt = H * (0.95f - u * 2.2f) * (0.6f + 0.4f * h01 (77u, (uint32) i));
        g.setColour (hot.withAlpha (0.55f * energy));
        g.fillRect (x, base - hgt, mini ? 0.6f : 1.0f, hgt);
    }
    // glowing tail envelope
    Path tail;
    tail.startNewSubPath (x0, base);
    const int n = mini ? 30 : 70;
    const float k = 3.2f + 2.0f * damp;
    for (int i = 0; i <= n; ++i)
    {
        const float u = (float) i / (float) n;
        const float shimmer = s.reduceMotion ? 0.0f : 0.05f * std::sin ((float) (tm * 2.1) + u * 23.0f) * (1.0f - u);
        const float env = std::exp (-k * u) * (1.0f - std::exp (-u * 40.0f)) + shimmer;
        tail.lineTo (x0 + len * u, base - H * jmax (0.0f, env) * 0.92f);
    }
    tail.lineTo (x0 + len, base);
    tail.closeSubPath();
    ColourGradient fill (hot.withAlpha (0.85f * energy), x0, base, mid.withAlpha (0.0f), x0 + len, base, false);
    fill.addColour (0.35, mid.withAlpha (0.5f * energy));
    g.setGradientFill (fill);
    g.fillPath (tail);
    ColourGradient glow (Colours::white.withAlpha (0.0f), 0.0f, base - H, Colours::white.withAlpha (0.0f), 0.0f, base, false);
    glow.addColour (0.8, hot.withAlpha (0.25f));
    g.setGradientFill (glow);
    g.fillPath (tail);

    // drifting particles along the tail (dust in the room)
    const int parts = mini ? 5 : 14;
    for (int i = 0; i < parts; ++i)
    {
        const float sp = 0.03f + 0.05f * h01 (31u, (uint32) i);
        const float u = (float) std::fmod (h01 (32u, (uint32) i) + tm * (double) sp, 1.0);
        const float env = std::exp (-k * u);
        const float x = x0 + len * u;
        const float y = base - H * env * 0.9f * h01 (33u, (uint32) i);
        const float a = env * (0.5f + 0.5f * std::sin ((float) tm * 3.0f + (float) i));
        g.setColour (Colours::white.withAlpha (jlimit (0.0f, 1.0f, a) * 0.8f));
        g.fillEllipse (Rectangle<float> (1.4f, 1.4f).withCentre ({ x, y }));
    }
    // floor line
    g.setColour (hot.withAlpha (0.35f));
    g.fillRect (in.getX(), base, in.getWidth(), 0.6f);

    if (! mini)
    {
        static const char* types[] = { "ROOM", "PLATE", "HALL", "DARK" };
        const auto info = Rectangle<float> (w.getX() + 6.0f, w.getBottom() - 11.0f, w.getWidth() - 12.0f, 10.0f);
        text (g, String (types[jlimit (0, 3, type)]) + "   DECAY " + fmt (T, s, 2), info, mono (7.5f), Colour (0xffcfe0ff).withAlpha (0.7f));
        text (g, "PRE " + fmt (T, s, 3), info, mono (7.5f), Colour (0xffcfe0ff).withAlpha (0.45f), Justification::centredRight);
    }
}

void liveWideBody (Graphics& g, const State& s, Rectangle<float> live, Rectangle<float> scr, bool mini)
{
    constexpr auto T = EffectType::WideBody;
    const float width = real (T, s, 0);   // 0..2
    const Colour ph (0xff7dffc6);
    const auto c = scr.getCentre();
    const float R = scr.getWidth() * 0.46f;

    g.saveState();
    g.reduceClipRegion (scr.toNearestInt());
    // graticule: L / R diagonals, M vertical, ring
    g.setColour (ph.withAlpha (0.13f));
    g.drawLine (c.x, scr.getY(), c.x, scr.getBottom(), 0.6f);
    g.drawLine (scr.getX(), c.y, scr.getRight(), c.y, 0.6f);
    g.setColour (ph.withAlpha (0.08f));
    g.drawLine (c.x - R, c.y - R, c.x + R, c.y + R, 0.5f);
    g.drawLine (c.x + R, c.y - R, c.x - R, c.y + R, 0.5f);
    g.drawEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 0.5f);
    if (! mini)
    {
        text (g, "L", Rectangle<float> (scr.getX() + 2.0f, scr.getY() + 1.0f, 8.0f, 8.0f), mono (6.5f), ph.withAlpha (0.4f));
        text (g, "R", Rectangle<float> (scr.getRight() - 8.0f, scr.getY() + 1.0f, 8.0f, 8.0f), mono (6.5f), ph.withAlpha (0.4f));
    }

    // points: (S, M) plot; real scope data when available, otherwise an animated synthetic signal
    double corrNum = 0.0, eL = 0.0, eR = 0.0;
    auto plot = [&] (float l, float r, float alpha)
    {
        const float x = c.x + (r - l) * 0.7071f * R;
        const float y = c.y - (l + r) * 0.7071f * R;
        g.setColour (ph.withAlpha (alpha));
        g.fillRect (x - 0.5f, y - 0.5f, mini ? 0.9f : 1.1f, mini ? 0.9f : 1.1f);
        corrNum += (double) l * r; eL += (double) l * l; eR += (double) r * r;
    };
    if (s.scopeL != nullptr && s.scopeR != nullptr && s.scopeN > 1)
    {
        const int step = jmax (1, s.scopeN / (mini ? 180 : 512));
        for (int i = 0; i < s.scopeN; i += step)
            plot (jlimit (-1.4f, 1.4f, s.scopeL[i]), jlimit (-1.4f, 1.4f, s.scopeR[i]), 0.55f);
    }
    else
    {
        const double t0 = animTime (s);
        const int n = mini ? 160 : 420;
        const float side = jlimit (0.0f, 1.6f, width * 0.55f);   // 0 = mono line, 1.1 at 200 %
        for (int i = 0; i < n; ++i)
        {
            const float u = (float) i / (float) n;
            const float tt = (float) t0 * 0.8f + u * 9.0f;
            const float m = 0.55f * std::sin (tt * 3.1f) + 0.25f * std::sin (tt * 7.3f + 1.0f) + 0.08f * std::sin (tt * 17.0f);
            const float sd = side * (0.35f * std::sin (tt * 4.7f + 0.5f) + 0.18f * std::sin (tt * 11.3f));
            plot (0.7071f * (m - sd), 0.7071f * (m + sd), 0.25f + 0.5f * u);
        }
    }
    // phosphor bloom
    ColourGradient bloom (ph.withAlpha (0.10f), c.x, c.y, ph.withAlpha (0.0f), c.x + R, c.y, true);
    g.setGradientFill (bloom);
    g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    g.restoreState();
    glassSheen (g, scr, 4.0f);

    if (mini) return;

    // readouts + correlation meter on the mint panel
    const auto st = style (T);
    auto info = live.withLeft (scr.getRight() + 12.0f);
    const float corr = (eL > 1e-9 && eR > 1e-9) ? (float) (corrNum / std::sqrt (eL * eR)) : 1.0f;
    auto row = [&] (const String& k, const String& v)
    {
        auto r = info.removeFromTop (12.0f);
        printed (g, k, r, mono (8.0f).boldened(), st.ink.withAlpha (0.6f), Justification::centredLeft, true);
        printed (g, v, r.withTrimmedRight (4.0f), mono (8.0f).boldened(), st.ink, Justification::centredRight, true);
    };
    row ("WIDTH", fmt (T, s, 0));
    row ("HAAS", fmt (T, s, 1));
    row ("MONO <", fmt (T, s, 2));
    info.removeFromTop (5.0f);
    auto meter = info.removeFromTop (11.0f).withTrimmedRight (4.0f);
    // recessed slot with a moving phosphor needle: -1 (left) .. +1 (right)
    auto slot = meter.withTrimmedLeft (18.0f).withTrimmedRight (18.0f).reduced (0.0f, 2.0f);
    g.setColour (Colour (0xff0b2a20));
    g.fillRoundedRectangle (slot, 2.0f);
    g.setColour (Colours::white.withAlpha (0.4f));
    g.fillRect (slot.getX() + 1.0f, slot.getBottom(), slot.getWidth() - 2.0f, 0.7f);
    const float nx = slot.getX() + slot.getWidth() * (0.5f + 0.5f * jlimit (-1.0f, 1.0f, corr));
    g.setColour (ph.withAlpha (0.25f));
    g.fillRect (nx - 3.0f, slot.getY() + 1.0f, 6.0f, slot.getHeight() - 2.0f);
    g.setColour (ph);
    g.fillRect (nx - 1.0f, slot.getY() + 1.0f, 2.0f, slot.getHeight() - 2.0f);
    g.setColour (ph.withAlpha (0.3f));
    g.fillRect (slot.getCentreX() - 0.3f, slot.getY() + 1.0f, 0.6f, slot.getHeight() - 2.0f);
    printed (g, "-1", meter.withWidth (16.0f), mono (7.0f), st.ink.withAlpha (0.6f), Justification::centredLeft, true);
    printed (g, "+1", meter.withTrimmedLeft (meter.getWidth() - 16.0f), mono (7.0f), st.ink.withAlpha (0.6f), Justification::centredRight, true);
}

void liveChopped (Graphics& g, const State& s, Rectangle<float> w, bool mini)
{
    constexpr auto T = EffectType::Chopped;
    const int mode = roundToInt (real (T, s, 0));
    const int patIdx = roundToInt (real (T, s, 1));
    const float stepBeats = jmax (0.0625f, syncBeats (real (T, s, 2)));
    const float depth = real (T, s, 3);
    const uint16 pat = choppedPattern (patIdx);
    const int cur = s.reduceMotion ? -1 : (int) std::floor (s.beats / (double) stepBeats) % 16;
    const float frac = s.reduceMotion ? 0.0f : (float) std::fmod (s.beats / (double) stepBeats, 1.0);
    const Colour green = th::ledGreen, amber (0xffffc861);

    auto in = w.reduced (mini ? 3.0f : 8.0f, mini ? 2.0f : 4.0f);
    if (! mini)
    {
        static const char* modes[] = { "PATTERN", "PUMP", "NOISE GATE" };
        auto top = in.removeFromTop (12.0f);
        String head = modes[jlimit (0, 2, mode)];
        if (mode == 0) head << " " << String (patIdx + 1).paddedLeft ('0', 2);
        if (mode != 2) head << "  /  " << fmt (T, s, 2);
        text (g, head, top, mono (7.5f), green.withAlpha (0.65f));
        text (g, String (roundToInt (depth * 100.0f)) + "% DEPTH", top, mono (7.5f), green.withAlpha (0.4f), Justification::centredRight);
    }

    // noise-gate mode: the row turns into an input meter with a threshold marker
    const float thrDb = real (T, s, 5);
    const float inDb = s.inLevel > 0.0f ? Decibels::gainToDecibels (s.inLevel, -80.0f) : -80.0f;

    const float pitch = in.getWidth() / 16.0f;
    const float lr = mini ? jmin (1.9f, pitch * 0.30f) : 4.0f;
    const float cy = mini ? in.getCentreY() : in.getY() + 15.0f;
    for (int i = 0; i < 16; ++i)
    {
        const float cx = in.getX() + pitch * ((float) i + 0.5f) + (float) (i / 4) * 0.0f;
        float level = 0.0f;
        Colour col = green;
        if (mode == 0)
        {
            const bool on = (pat >> (15 - i)) & 1;
            level = on ? 0.55f : 0.0f;
            if (i == cur) { level = on ? 1.0f : 0.35f; if (! on) col = amber; }
        }
        else if (mode == 1)
        {
            // pump: each beat ducks then swells back
            const float stepsPerBeat = jmax (1.0f, 1.0f / stepBeats);
            const float ph = std::fmod ((float) i / stepsPerBeat, 1.0f);
            level = 0.1f + 0.75f * ph * depth + 0.15f * (1.0f - depth);
            if (i == cur) level = jmin (1.0f, level + 0.35f);
        }
        else
        {
            const float db = -80.0f + 80.0f * (float) i / 15.0f;
            level = inDb >= db ? 0.9f : 0.0f;
            col = db >= thrDb ? green : amber;
        }
        led (g, { cx, cy }, lr, col, level);
        if (! mini && i % 4 == 0)
        {
            g.setColour (green.withAlpha (0.3f));
            g.fillRect (cx - lr, cy + 9.0f, pitch * 4.0f - 4.0f, 0.8f);
            text (g, String (i + 1), Rectangle<float> (cx - lr, cy + 10.0f, 20.0f, 10.0f), mono (7.0f), green.withAlpha (0.5f));
        }
    }
    if (mode == 2 && ! mini)
    {
        const float tx = in.getX() + pitch * (0.5f + 15.0f * (thrDb + 80.0f) / 80.0f);
        g.setColour (amber.withAlpha (0.9f));
        Path tri; tri.addTriangle (tx - 3.0f, cy - 9.0f, tx + 3.0f, cy - 9.0f, tx, cy - 5.5f);
        g.fillPath (tri);
    }
    ignoreUnused (frac);
}

void liveRedLine (Graphics& g, const State& s, Rectangle<float> w, bool mini)
{
    constexpr auto T = EffectType::RedLine;
    const float gr = jmax (0.0f, s.gainReductionDb);
    const Colour red = th::killRed, amber (0xffffc861);
    auto in = w.reduced (mini ? 3.0f : 8.0f, mini ? 2.0f : 4.0f);

    if (! mini)
    {
        auto top = in.removeFromTop (12.0f);
        text (g, "CEILING " + fmt (T, s, 0), top, mono (7.5f), Colour (0xffeceef2).withAlpha (0.6f));
        const bool la = real (T, s, 3) > 0.5f;
        text (g, la ? "LOOKAHEAD" : "ZERO LAT", top, mono (7.5f), Colour (0xffeceef2).withAlpha (0.35f), Justification::centredRight);
    }

    // GR bar: segments 0..12 dB, lit from the left
    static const float marks[] = { 0.0f, 1.0f, 2.0f, 3.0f, 6.0f, 9.0f, 12.0f };
    const int segs = mini ? 12 : 24;
    auto bar = mini ? in.withTrimmedLeft (12.0f) : in.removeFromTop (14.0f).withTrimmedLeft (34.0f);
    const float sw = bar.getWidth() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        const float db = 12.0f * (float) (i + 1) / (float) segs;
        const bool on = gr >= db - 12.0f / (float) segs * 0.5f;
        const Colour c = db <= 3.0f ? amber : red;
        auto seg = Rectangle<float> (bar.getX() + sw * (float) i, bar.getY(), sw, bar.getHeight()).reduced (mini ? 0.4f : 0.8f, mini ? 1.0f : 2.0f);
        g.setColour (c.withAlpha (on ? 0.0f : 0.12f));
        g.fillRect (seg);
        if (on)
        {
            g.setColour (c.withAlpha (0.35f));
            g.fillRect (seg.expanded (1.0f, 1.5f));
            g.setColour (c);
            g.fillRect (seg);
            g.setColour (Colours::white.withAlpha (0.35f));
            g.fillRect (seg.withHeight (seg.getHeight() * 0.3f));
        }
    }
    // LIMIT LED
    const float lr = mini ? 2.4f : 3.6f;
    led (g, { mini ? in.getX() + 4.0f : in.getX() + 6.0f, bar.getCentreY() }, lr, red, gr > 0.3f ? jmin (1.0f, 0.5f + gr / 6.0f) : 0.0f);
    if (mini) return;
    text (g, "GR", Rectangle<float> (in.getX() + 14.0f, bar.getY(), 20.0f, bar.getHeight()), mono (7.5f), red.withAlpha (0.7f));
    auto scale = in.removeFromTop (11.0f).withTrimmedLeft (34.0f);
    for (float m : marks)
    {
        const float x = scale.getX() + scale.getWidth() * m / 12.0f;
        g.setColour (Colour (0xffeceef2).withAlpha (0.3f));
        g.fillRect (x - 0.3f, scale.getY(), 0.6f, 2.5f);
        text (g, String ((int) m), Rectangle<float> (18.0f, 8.0f).withCentre ({ x, scale.getY() + 7.0f }), mono (6.5f),
              Colour (0xffeceef2).withAlpha (0.45f), Justification::centred);
    }
    text (g, "DRIVE " + fmt (T, s, 1), in, mono (7.5f), Colour (0xffeceef2).withAlpha (0.45f), Justification::centredLeft);
    text (g, "-" + String (gr, 1) + " dB", in, mono (7.5f), red.withAlpha (0.8f), Justification::centredRight);
}

void paintLive (Graphics& g, EffectType t, const State& s, Rectangle<float> live, bool mini)
{
    const auto w = liveWindow (t, live);
    switch (t)
    {
        case EffectType::Doubles:    liveDoubles (g, s, w, mini); break;
        case EffectType::Swirl:      liveSwirl (g, s, live, w, mini); return;
        case EffectType::AdLibThrow: liveAdLib (g, s, w, mini); break;
        case EffectType::AuraRoom:   liveAura (g, s, w, mini); return;
        case EffectType::WideBody:   liveWideBody (g, s, live, w, mini); return;
        case EffectType::Chopped:    liveChopped (g, s, w, mini); break;
        case EffectType::RedLine:    liveRedLine (g, s, w, mini); break;
        default: return;
    }
    glassSheen (g, w, mini ? 2.0f : 4.0f);
}

// the mini thumbnail places its live element on the right edge
Rectangle<float> miniLiveArea (Rectangle<float> plate)
{
    return { plate.getRight() - 8.0f - 6.0f - 84.0f, plate.getY() + 6.0f, 84.0f, plate.getHeight() - 12.0f };
}
} // namespace

Palette paletteSpaceGroup (EffectType t)
{
    const auto st = style (t);
    return { st.plate, st.plateDark, st.ink, st.accent, st.led };
}

void paintSpaceGroup (Graphics& g, EffectType t, Rectangle<float> plate, const State& s, bool miniature)
{
    const float scale = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto& img = staticLayer (t, plate, scale, miniature);
    g.setOpacity (1.0f);   // drawImage uses the current colour's alpha
    g.drawImage (img, plate);
    paintLive (g, t, s, miniature ? miniLiveArea (plate) : liveArea (t, plate), miniature);
}
} // namespace ek::faceplate
