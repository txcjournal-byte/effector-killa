// TrapWin effect cards of the "colour" group: Cassette Plug, Menace, Brainrot, Through The Wall, Tone Up,
// Squeeze, Overcooked and Slap. Style: Docs/TRAPWIN-STYLE.md (trapwin.com) - dark card, chrome italic name,
// per-effect accent used only as a 2 px left edge, a faint glow and the tint of the live glass screen.
// No hardware materials (no ears, screws, wood, lacquer, PCB, tape or illustrations).
//
// Drawing is split in two layers:
//  - a static layer (card body, accent edge, chrome name, category line, screen inset, preset pill) rendered
//    once per type / size / pixel scale into an image cache (it uses real blurs for the glows);
//  - the live element (rings, heat bar, curves, meters...) drawn every frame from the State snapshot with
//    cheap layered-stroke glows.
// knobArea() is left calm on purpose: RackView places the real knob components on top of it.
#include "Faceplates.h"
#include "../Theme.h"
#include <functional>
#include <map>
#include "../SharedResources.h"

namespace ek::faceplate
{
namespace
{
using namespace juce;
namespace th = ek::theme;

// ---------------------------------------------------------------------------------------------
// TrapWin tokens
// ---------------------------------------------------------------------------------------------
const Colour cCard   { 0xff0a0f18 };
const Colour cCard2  { 0xff0b111c };
const Colour cInset  { 0xff05080f };
const Colour cLine   { 0x2e9fc3ff };   // rgba(159,195,255,.18)
const Colour cText   { 0xffe6ecf7 };
const Colour cText2  { 0xffa9b6cc };
const Colour cMuted  { 0xff8794ab };
const Colour cIce    { 0xff9fc3ff };
const Colour cGlow   { 0x804676c4 };   // rgba(70,118,196,.5)

constexpr float cardCorner = 20.0f, miniCorner = 10.0f, screenCorner = 10.0f;

struct Look
{
    Colour accent;
    const char* name;       // chrome title
    const char* category;   // Plex Mono line under the title
    const char* role;       // muted descriptor above the knob row
};

Look look (EffectType t)
{
    switch (t)
    {
        case EffectType::CassettePlug:   return { Colour (0xffe8b86b), "CASSETTE PLUG",    "SATURATION", "TAPE / TUBE DRIVE" };
        case EffectType::Menace:         return { Colour (0xffff4d5e), "MENACE",           "DISTORTION", "FOUR-MODE DISTORTION" };
        case EffectType::Brainrot:       return { Colour (0xff7cfc6b), "BRAINROT",         "LO-FI",      "BIT / RATE REDUCER" };
        case EffectType::ThroughTheWall: return { Colour (0xff5b8cff), "THROUGH THE WALL", "FILTER",     "MULTIMODE FILTER" };
        case EffectType::ToneUp:         return { Colour (0xffc9d0dd), "TONE UP",          "EQUALIZER",  "5-BAND EQUALIZER" };
        case EffectType::Squeeze:        return { Colour (0xff3ddc97), "SQUEEZE",          "COMPRESSOR", "PARALLEL COMPRESSOR" };
        case EffectType::Overcooked:     return { Colour (0xffff8a3d), "OVERCOOKED",       "MULTIBAND",  "3-BAND UP / DOWN DYNAMICS" };
        case EffectType::Slap:           return { Colour (0xffffd23f), "SLAP",             "TRANSIENT",  "TRANSIENT SHAPER" };
        default:                         return { cIce, "", "", "" };
    }
}

float realParam (EffectType t, const State& s, int i)
{
    return effectInfo (t).params[(size_t) i].toReal (s.params[(size_t) i]);
}

// animation clock that freezes for reduceMotion / power off
double animTime (const State& s) { return (s.reduceMotion || ! s.powered) ? 0.0 : s.timeSec; }

// ---------------------------------------------------------------------------------------------
// text
// ---------------------------------------------------------------------------------------------
Font displayFont (float h) { return th::stencilFont (h).withHorizontalScale (1.0f); } // Archivo Black (900)
Font mono (float h)        { return th::monoLabel (h); }                              // IBM Plex Mono Medium
Font monoBold (float h)    { return th::osdFont (h); }                                // IBM Plex Mono Bold

void text (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j)
{
    g.setFont (f);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

// glowing text for readouts on the screens (cheap: one offset-free halo pass)
void glowText (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j, float glow = 1.0f)
{
    g.setFont (f);
    if (glow > 0.0f)
    {
        g.setColour (c.withAlpha (0.18f * glow));
        for (auto d : { Point<float> (-0.8f, 0.0f), Point<float> (0.8f, 0.0f), Point<float> (0.0f, -0.8f), Point<float> (0.0f, 0.8f) })
            g.drawText (s, r.translated (d.x, d.y), j, false);
    }
    g.setColour (c);
    g.drawText (s, r, j, false);
}

// Archivo 900 italic chrome title (trapwin.com .tw-chrome): vertical chrome gradient + soft blue glow.
// Archivo Black is the bundled 900 cut; the italic is a 12 degree shear (no italic TTF bundled).
Path chromePath (const String& s, float height, Rectangle<float> box, float baseline)
{
    Font f = displayFont (height);
    float w = GlyphArrangement::getStringWidth (f, s);
    const float maxW = box.getWidth() - height * 0.25f;
    if (w > maxW && w > 0.0f)
    {
        f = displayFont (height * maxW / w);
        w = GlyphArrangement::getStringWidth (f, s);
    }
    GlyphArrangement ga;
    ga.addLineOfText (f, s, box.getX(), baseline);
    Path p;
    ga.createPath (p);
    p.applyTransform (AffineTransform::shear (-0.21f, 0.0f).translated (baseline * 0.21f, 0.0f));
    return p;
}

void paintChrome (Graphics& g, const Path& p, float glow)
{
    const auto b = p.getBounds();
    if (glow > 0.0f)
    {
        DropShadow (cGlow.withMultipliedAlpha (glow), jmax (4, roundToInt (b.getHeight() * 0.55f)), {}).drawForPath (g, p);
        DropShadow (cIce.withAlpha (0.16f * glow), jmax (2, roundToInt (b.getHeight() * 0.12f)), {}).drawForPath (g, p);
    }
    ColourGradient chrome (Colour (0xfff4f6fb), 0.0f, b.getY(), Colour (0xffe6eaf2), 0.0f, b.getBottom(), false);
    chrome.addColour (0.48, Colour (0xffc9d0dd));
    chrome.addColour (0.52, Colour (0xff8e98ab));
    g.setGradientFill (chrome);
    g.fillPath (p);
}

// four-point TrapWin sparkle
Path sparkle (Point<float> c, float r)
{
    Path p;
    const float k = r * 0.22f;
    p.startNewSubPath (c.x, c.y - r);
    p.quadraticTo (c.x + k, c.y - k, c.x + r, c.y);
    p.quadraticTo (c.x + k, c.y + k, c.x, c.y + r);
    p.quadraticTo (c.x - k, c.y + k, c.x - r, c.y);
    p.quadraticTo (c.x - k, c.y - k, c.x, c.y - r);
    p.closeSubPath();
    return p;
}

// "SATURATION  ✦ TRAPWIN" in muted Plex Mono
void categoryLine (Graphics& g, const String& category, Rectangle<float> r, float h)
{
    const auto f = mono (h);
    text (g, category, r, f, cMuted, Justification::centredLeft);
    const float x = r.getX() + GlyphArrangement::getStringWidth (f, category) + h * 1.1f;
    g.setColour (cIce.withAlpha (0.85f));
    g.fillPath (sparkle ({ x + h * 0.45f, r.getCentreY() }, h * 0.5f));
    text (g, "TRAPWIN", r.withLeft (x + h * 1.25f), f, cMuted, Justification::centredLeft);
}

// ---------------------------------------------------------------------------------------------
// card + screen (static)
// ---------------------------------------------------------------------------------------------
void paintCard (Graphics& g, Rectangle<float> p, Colour accent, float corner, bool mini)
{
    Path card;
    card.addRoundedRectangle (p.reduced (0.5f), corner);

    // body with a subtle inner gradient (card -> card-2, slightly lifted at the top)
    ColourGradient body (Colour (0xff0e1421), 0.0f, p.getY(), cCard, 0.0f, p.getY() + p.getHeight() * 0.55f, false);
    body.addColour (1.0, Colour (0xff080c14));
    g.setGradientFill (body);
    g.fillPath (card);

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (card);
        // faint accent glow behind the identity block
        const float gr = mini ? 110.0f : 330.0f;
        ColourGradient ag (accent.withAlpha (mini ? 0.10f : 0.085f), p.getX(), p.getY() + p.getHeight() * 0.35f,
                           accent.withAlpha (0.0f), p.getX() + gr, p.getY() + p.getHeight() * 0.35f, true);
        g.setGradientFill (ag);
        g.fillRect (p);
        // very faint ice-blue lift on the right side (trapwin.com radial background)
        ColourGradient ig (cIce.withAlpha (0.025f), p.getRight() - p.getWidth() * 0.25f, p.getY(),
                           cIce.withAlpha (0.0f), p.getRight() - p.getWidth() * 0.25f, p.getY() + p.getHeight() * 1.2f, true);
        g.setGradientFill (ig);
        g.fillRect (p);
        // top sheen line
        ColourGradient sh (Colours::white.withAlpha (0.035f), 0.0f, p.getY(), Colours::transparentWhite, 0.0f, p.getY() + p.getHeight() * 0.35f, false);
        g.setGradientFill (sh);
        g.fillRect (p);

        // 2 px accent edge on the left, following the corner radius and fading out on the corners
        const float r = corner - 0.5f, x = p.getX() + 1.5f, top = p.getY() + 0.5f, bot = p.getBottom() - 0.5f;
        Path edge;
        edge.addCentredArc (x + r - 1.0f, top + r, r - 1.0f, r - 1.0f, 0.0f, -0.55f, -MathConstants<float>::halfPi, true);
        edge.lineTo (x, bot - r);
        edge.addCentredArc (x + r - 1.0f, bot - r, r - 1.0f, r - 1.0f, 0.0f, -MathConstants<float>::halfPi, -MathConstants<float>::pi + 0.55f, false);
        ColourGradient eg (accent.withAlpha (0.35f), 0.0f, top, accent.withAlpha (0.25f), 0.0f, bot, false);
        eg.addColour (0.18, accent);
        eg.addColour (0.62, accent.withAlpha (0.75f));
        Path stroked;
        PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (stroked, edge);
        DropShadow (accent.withAlpha (0.55f), mini ? 5 : 9, {}).drawForPath (g, stroked);
        g.setGradientFill (eg);
        g.fillPath (stroked);
    }

    // 1 px line
    g.setColour (cLine);
    g.strokePath (card, PathStrokeType (1.0f));
}

void paintScreenBase (Graphics& g, Rectangle<float> r, Colour accent, float corner, bool grid)
{
    Path s;
    s.addRoundedRectangle (r, corner);
    g.setColour (cInset);
    g.fillPath (s);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (s);
        // accent tint from the centre
        ColourGradient tint (accent.withAlpha (0.10f), r.getCentreX(), r.getCentreY() + r.getHeight() * 0.2f,
                             accent.withAlpha (0.0f), r.getRight() + r.getWidth() * 0.1f, r.getCentreY(), true);
        g.setGradientFill (tint);
        g.fillRect (r);
        // inner shadow from the top edge
        ColourGradient sh (Colours::black.withAlpha (0.55f), 0.0f, r.getY(), Colours::transparentBlack, 0.0f, r.getY() + 7.0f, false);
        g.setGradientFill (sh);
        g.fillRect (r.withHeight (8.0f));
        if (grid)
        {
            g.setColour (cIce.withAlpha (0.035f));
            for (float x = r.getX() + 11.0f; x < r.getRight() - 4.0f; x += 11.0f) g.fillRect (x, r.getY() + 3.0f, 0.7f, r.getHeight() - 6.0f);
            for (float y = r.getY() + 9.5f; y < r.getBottom() - 3.0f; y += 9.5f) g.fillRect (r.getX() + 3.0f, y, r.getWidth() - 6.0f, 0.7f);
        }
    }
    g.setColour (cLine);
    g.strokePath (s, PathStrokeType (1.0f));
}

// glass sheen, drawn over the live content
void glass (Graphics& g, Rectangle<float> r, float corner)
{
    Path top;
    const auto tr = r.reduced (1.0f).withHeight (r.getHeight() * 0.48f);
    top.addRoundedRectangle (tr.getX(), tr.getY(), tr.getWidth(), tr.getHeight(), corner - 1.0f, corner - 1.0f, true, true, false, false);
    ColourGradient sh (Colours::white.withAlpha (0.045f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getY() + r.getHeight() * 0.48f, false);
    g.setGradientFill (sh);
    g.fillPath (top);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.fillRect (r.getX() + corner, r.getY() + 1.0f, r.getWidth() - corner * 2.0f, 0.6f);
}

// ---------------------------------------------------------------------------------------------
// static layer
// ---------------------------------------------------------------------------------------------
Rectangle<float> miniLiveArea (Rectangle<float> plate)
{
    return { plate.getRight() - 8.0f - 84.0f, plate.getY() + 6.0f, 84.0f, plate.getHeight() - 12.0f };
}

void paintStatic (Graphics& g, EffectType t, Rectangle<float> p, bool mini)
{
    const auto lk = look (t);
    if (mini)
    {
        paintCard (g, p, lk.accent, miniCorner, true);
        // accent dot
        const Point<float> dot (p.getX() + 13.0f, p.getCentreY());
        DropShadow (lk.accent.withAlpha (0.8f), 6, {}).drawForPath (g, [&] { Path d; d.addEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (dot)); return d; }());
        g.setColour (lk.accent);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (dot));
        g.setColour (Colours::white.withAlpha (0.7f));
        g.fillEllipse (Rectangle<float> (1.8f, 1.8f).withCentre (dot.translated (-0.6f, -0.6f)));

        const auto live = miniLiveArea (p);
        const auto nameBox = Rectangle<float> (p.getX() + 24.0f, p.getY() + 3.0f, live.getX() - p.getX() - 30.0f, 22.0f);
        paintChrome (g, chromePath (lk.name, 15.5f, nameBox, p.getY() + 20.5f), 0.8f);
        categoryLine (g, lk.category, { nameBox.getX() + 1.0f, p.getY() + 24.5f, nameBox.getWidth(), 10.0f }, 7.0f);
        paintScreenBase (g, live, lk.accent, 7.0f, false);
        return;
    }

    paintCard (g, p, lk.accent, cardCorner, false);

    const auto na = nameArea (p);
    paintChrome (g, chromePath (lk.name, 30.0f, na, na.getY() + 31.0f), 1.0f);
    categoryLine (g, lk.category, { na.getX() + 1.0f, na.getY() + 38.0f, na.getWidth(), 12.0f }, 8.5f);

    // muted descriptor above the knob row (no frame: the knob area stays calm)
    const auto ka = knobArea (t, p);
    if (ka.getY() < p.getY() + 60.0f)   // full-width card only (compact SPLIT cards have no room for it)
        text (g, lk.role, { ka.getX() + 4.0f, p.getY() + 13.0f, 300.0f, 20.0f }, mono (8.0f), cMuted.withAlpha (0.75f), Justification::centredLeft);

    paintScreenBase (g, liveArea (t, p), lk.accent, screenCorner, true);

    // preset pill (text is drawn by RackView)
    const auto lcd = presetLcdArea (p);
    Path pill;
    pill.addRoundedRectangle (lcd, lcd.getHeight() * 0.5f);
    g.setColour (cInset);
    g.fillPath (pill);
    g.setColour (cLine);
    g.strokePath (pill, PathStrokeType (1.0f));
}

// ---------------------------------------------------------------------------------------------
// live-element helpers (cheap per-frame glows: layered strokes)
// ---------------------------------------------------------------------------------------------
Colour hot (Colour accent, float amount) { return accent.interpolatedWith (Colours::white, amount); }

void glowStroke (Graphics& g, const Path& p, Colour c, float w, float glow = 1.0f)
{
    if (glow > 0.0f)
    {
        g.setColour (c.withAlpha (0.08f * glow));
        g.strokePath (p, PathStrokeType (w * 6.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.20f * glow));
        g.strokePath (p, PathStrokeType (w * 2.8f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (hot (c, 0.25f));
    g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
}

void glowDot (Graphics& g, Point<float> c, float r, Colour colour, float amount)
{
    amount = jlimit (0.0f, 1.0f, amount);
    if (amount > 0.0f)
    {
        ColourGradient halo (colour.withAlpha (0.45f * amount), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 4.0f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 8.0f, r * 8.0f).withCentre (c));
    }
    g.setColour (colour.withMultipliedBrightness (0.25f + 0.75f * amount).withAlpha (0.35f + 0.65f * amount));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    if (amount > 0.0f)
    {
        g.setColour (Colours::white.withAlpha (0.85f * amount));
        g.fillEllipse (Rectangle<float> (r * 0.9f, r * 0.9f).withCentre (c));
    }
}

void glowFillRounded (Graphics& g, Rectangle<float> r, float corner, Colour c, float glow)
{
    if (glow > 0.0f)
    {
        g.setColour (c.withAlpha (0.10f * glow));
        g.fillRoundedRectangle (r.expanded (3.0f), corner + 3.0f);
        g.setColour (c.withAlpha (0.18f * glow));
        g.fillRoundedRectangle (r.expanded (1.5f), corner + 1.5f);
    }
    g.fillRoundedRectangle (r, corner);
}

// small pill used for mode lamps on the screens
void modePill (Graphics& g, Rectangle<float> r, const String& label, bool on, Colour accent, float fontH)
{
    const float rad = r.getHeight() * 0.5f;
    if (on)
    {
        g.setColour (accent.withAlpha (0.10f));
        g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (accent.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        glowText (g, label, r, mono (fontH), hot (accent, 0.55f), Justification::centred, 0.8f);
    }
    else
    {
        g.setColour (cLine);
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        text (g, label, r, mono (fontH), cMuted.withAlpha (0.8f), Justification::centred);
    }
}

String hz (float f) { return f >= 1000.0f ? String (f / 1000.0f, f < 10000.0f ? 2 : 1) + " kHz" : String (roundToInt (f)) + " Hz"; }

// ---------------------------------------------------------------------------------------------
// Cassette Plug: two glowing reels (rings + spokes), speed follows WOW
// ---------------------------------------------------------------------------------------------
void reel (Graphics& g, Point<float> c, float r, float angle, float pack, Colour accent, bool tiny)
{
    // tape pack as a faint glowing disc
    ColourGradient pk (accent.withAlpha (0.16f), c.x, c.y, accent.withAlpha (0.03f), c.x + r * pack, c.y, true);
    g.setGradientFill (pk);
    g.fillEllipse (Rectangle<float> (r * pack * 2.0f, r * pack * 2.0f).withCentre (c));

    Path ring;
    ring.addEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    glowStroke (g, ring, accent, tiny ? 0.9f : 1.3f, tiny ? 0.7f : 1.0f);

    Path spokes;
    const float hub = r * 0.30f;
    spokes.addEllipse (Rectangle<float> (hub * 2.0f, hub * 2.0f).withCentre (c));
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 3.0f;
        spokes.startNewSubPath (c.getPointOnCircumference (hub, a));
        spokes.lineTo (c.getPointOnCircumference (r * 0.78f, a));
    }
    glowStroke (g, spokes, accent, tiny ? 0.8f : 1.1f, tiny ? 0.5f : 0.8f);
}

void liveCassette (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
{
    const float wow = s.params[4];
    const double t = animTime (s);
    const double speed = 0.9 + 6.0 * wow;
    const float angle = 0.4f + (float) std::fmod (t * speed + wow * 0.5 * std::sin (t * 3.1), MathConstants<double>::twoPi * 100.0);

    auto pairOfReels = [&] (Rectangle<float> r, float rad, bool tiny)
    {
        const Point<float> c1 (r.getX() + rad + (tiny ? 3.0f : 10.0f), r.getCentreY());
        const Point<float> c2 (r.getRight() - rad - (tiny ? 3.0f : 10.0f), r.getCentreY());
        // tape path along the bottom, through a small head
        Path tape;
        tape.startNewSubPath (c1.x, c1.y + rad);
        tape.lineTo (c2.x, c2.y + rad);
        glowStroke (g, tape, accent, tiny ? 0.7f : 1.0f, 0.6f);
        if (! tiny)
        {
            const float hx = (c1.x + c2.x) * 0.5f;
            glowFillRounded (g, { hx - 5.0f, c1.y + rad - 1.5f, 10.0f, 3.0f }, 1.5f, accent, 0.5f);
            g.setColour (accent.withAlpha (0.9f));
            g.fillRoundedRectangle (hx - 5.0f, c1.y + rad - 1.5f, 10.0f, 3.0f, 1.5f);
        }
        reel (g, c1, rad, angle, 0.92f, accent, tiny);
        reel (g, c2, rad, angle * 1.18f + 0.7f, 0.62f, accent, tiny);
    };

    if (mini)
    {
        pairOfReels (a.reduced (14.0f, 2.0f), a.getHeight() * 0.32f, true);
        glass (g, a, 7.0f);
        return;
    }

    pairOfReels (a.withWidth (120.0f).reduced (0.0f, 6.0f), 17.0f, false);

    // right: TAPE / TUBE pills, wow wave and readout
    auto side = a.withTrimmedLeft (124.0f).reduced (6.0f, 0.0f);
    const bool tube = s.params[0] >= 0.5f;
    modePill (g, { side.getX(), a.getY() + 8.0f, 39.0f, 14.0f }, "TAPE", ! tube, accent, 7.5f);
    modePill (g, { side.getX() + 44.0f, a.getY() + 8.0f, 39.0f, 14.0f }, "TUBE", tube, accent, 7.5f);

    Path w;
    const auto wr = Rectangle<float> (side.getX(), a.getY() + 27.0f, side.getWidth(), 12.0f);
    const float ph = (float) (t * 2.0);
    for (int i = 0; i <= (int) wr.getWidth(); ++i)
    {
        const float x = (float) i / wr.getWidth();
        const float y = wr.getCentreY() - std::sin (x * MathConstants<float>::twoPi * 2.0f + ph) * wr.getHeight() * 0.5f * (0.12f + 0.88f * wow);
        if (i == 0) w.startNewSubPath (wr.getX(), y); else w.lineTo (wr.getX() + (float) i, y);
    }
    glowStroke (g, w, accent, 1.0f, 0.8f);
    text (g, "WOW", { side.getX(), a.getY() + 42.0f, 40.0f, 11.0f }, mono (7.5f), cMuted, Justification::centredLeft);
    glowText (g, String (roundToInt (wow * 100.0f)) + " %", { side.getX(), a.getY() + 42.0f, side.getWidth(), 11.0f }, mono (8.0f),
              hot (accent, 0.4f), Justification::centredRight, 0.6f);
    glass (g, a, screenCorner);
}

// ---------------------------------------------------------------------------------------------
// Menace: heat bar + DRIVE dot + mode row
// ---------------------------------------------------------------------------------------------
void heatBar (Graphics& g, Rectangle<float> bar, float lvl, Colour accent, bool ticks)
{
    const float rad = bar.getHeight() * 0.5f;
    g.setColour (cCard2);
    g.fillRoundedRectangle (bar, rad);
    g.setColour (cLine);
    g.drawRoundedRectangle (bar.reduced (0.5f), rad, 0.8f);
    if (lvl > 0.001f)
    {
        auto fill = bar.reduced (1.5f).withWidth (jmax (bar.getHeight() - 3.0f, (bar.getWidth() - 3.0f) * jlimit (0.0f, 1.0f, lvl)));
        ColourGradient heat (accent.withAlpha (0.35f), bar.getX(), 0.0f, hot (accent, 0.75f), bar.getX() + (bar.getWidth()) * jmax (0.2f, lvl), 0.0f, false);
        heat.addColour (0.7, accent);
        g.setGradientFill (heat);
        glowFillRounded (g, fill, fill.getHeight() * 0.5f, accent, 1.0f);
        g.setGradientFill (heat);
        g.fillRoundedRectangle (fill, fill.getHeight() * 0.5f);
        // hot tip
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillEllipse (Rectangle<float> (fill.getHeight() * 0.7f, fill.getHeight() * 0.7f).withCentre ({ fill.getRight() - fill.getHeight() * 0.5f, fill.getCentreY() }));
    }
    if (ticks)
    {
        for (int i = 0; i <= 10; ++i)
        {
            const float x = bar.getX() + rad + (bar.getWidth() - rad * 2.0f) * (float) i / 10.0f;
            g.setColour (cIce.withAlpha (i % 5 == 0 ? 0.30f : 0.14f));
            g.fillRect (x - 0.35f, bar.getBottom() + 3.0f, 0.7f, i % 5 == 0 ? 4.0f : 2.5f);
        }
    }
}

void liveMenace (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
{
    const float drive = s.params[1];
    float lvl = drive;
    if (s.outLevel > 0.0f) lvl = drive * (0.7f + 0.3f * jmin (1.0f, s.outLevel * 1.5f));
    if (! s.powered) lvl = 0.0f;

    if (mini)
    {
        heatBar (g, { a.getX() + 7.0f, a.getCentreY() - 3.0f, a.getWidth() - 30.0f, 6.0f }, lvl, accent, false);
        glowDot (g, { a.getRight() - 11.0f, a.getCentreY() }, 3.2f, accent, 0.25f + 0.75f * lvl);
        glass (g, a, 7.0f);
        return;
    }

    // DRIVE dot
    const Point<float> c (a.getX() + 25.0f, a.getY() + 23.0f);
    Path ring;
    ring.addEllipse (Rectangle<float> (22.0f, 22.0f).withCentre (c));
    g.setColour (cLine);
    g.strokePath (ring, PathStrokeType (1.0f));
    glowDot (g, c, 5.5f, accent, 0.2f + 0.8f * lvl);
    text (g, "DRIVE", { c.x - 22.0f, a.getY() + 39.0f, 44.0f, 11.0f }, mono (7.5f), cMuted, Justification::centred);

    // mode row
    const char* types[] = { "FUZZ", "CLIP", "FOLD", "RECT" };
    const int type = jlimit (0, 3, roundToInt (realParam (EffectType::Menace, s, 0)));
    auto row = Rectangle<float> (a.getX() + 54.0f, a.getY() + 7.0f, a.getWidth() - 62.0f, 13.0f);
    const float cw = row.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
        modePill (g, row.withX (row.getX() + (float) i * cw).withWidth (cw).reduced (1.5f, 0.0f), types[i], i == type && s.powered, accent, 7.0f);

    // heat bar + readout
    heatBar (g, { a.getX() + 54.0f, a.getY() + 27.0f, a.getWidth() - 62.0f, 9.0f }, lvl, accent, true);
    text (g, "HEAT", { a.getX() + 54.0f, a.getY() + 43.0f, 60.0f, 11.0f }, mono (7.5f), cMuted, Justification::centredLeft);
    glowText (g, String (roundToInt (lvl * 100.0f)) + " %", { a.getX() + 54.0f, a.getY() + 43.0f, a.getWidth() - 62.0f, 11.0f }, mono (8.0f),
              hot (accent, 0.4f), Justification::centredRight, 0.6f);
    glass (g, a, screenCorner);
}

// ---------------------------------------------------------------------------------------------
// Brainrot: stepped wave + bit columns
// ---------------------------------------------------------------------------------------------
void liveBrainrot (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
{
    const int bits = jlimit (4, 16, (int) std::round (realParam (EffectType::Brainrot, s, 0)));
    const float rate = realParam (EffectType::Brainrot, s, 1);
    const double t = animTime (s);

    auto bitColumns = [&] (Rectangle<float> r, int n)
    {
        const float cw = r.getWidth() / (float) n;
        for (int i = 0; i < n; ++i) // MSB on the left
        {
            const bool active = (float) i < (float) bits * (float) n / 16.0f;
            auto colR = Rectangle<float> (r.getX() + (float) i * cw + cw * 0.22f, r.getY(), cw * 0.56f, r.getHeight());
            g.setColour (accent.withAlpha (0.08f));
            g.fillRoundedRectangle (colR, colR.getWidth() * 0.5f);
            if (! active) continue;
            const float flicker = s.reduceMotion ? 0.8f : 0.45f + 0.55f * (float) std::abs (std::sin (t * (2.0 + i * 0.37) + i * 1.3));
            auto lit = colR.withTop (colR.getBottom() - colR.getHeight() * flicker);
            g.setColour (accent.withAlpha (0.85f));
            glowFillRounded (g, lit, lit.getWidth() * 0.5f, accent, 0.8f);
            g.setColour (hot (accent, 0.3f));
            g.fillRoundedRectangle (lit, lit.getWidth() * 0.5f);
        }
    };

    auto steppedWave = [&] (Rectangle<float> wr, float w)
    {
        const float levels = std::pow (2.0f, (float) (bits - 1) * 0.55f);
        const float hold = jlimit (1.0f, 14.0f, 44100.0f / jmax (1000.0f, rate) * 1.2f) * (wr.getWidth() / 120.0f);
        Path wave;
        const float phase = (float) (t * 2.2);
        for (float x = 0.0f; x <= wr.getWidth(); x += 0.5f)
        {
            const float xs = std::floor (x / hold) * hold;
            float v = std::sin (xs / wr.getWidth() * MathConstants<float>::twoPi * 1.5f + phase);
            v = std::round (v * levels) / levels;
            const float y = wr.getCentreY() - v * wr.getHeight() * 0.44f;
            if (x == 0.0f) wave.startNewSubPath (wr.getX(), y);
            else wave.lineTo (wr.getX() + x, y);
        }
        glowStroke (g, wave, accent, w);
    };

    if (mini)
    {
        steppedWave (a.reduced (6.0f, 6.0f).withWidth (44.0f), 0.9f);
        bitColumns (a.withTrimmedLeft (54.0f).reduced (2.0f, 6.0f).withTrimmedRight (4.0f), 6);
        glass (g, a, 7.0f);
        return;
    }

    auto inner = a.reduced (8.0f, 6.0f);
    auto readout = inner.removeFromBottom (11.0f);
    inner.removeFromBottom (3.0f);
    steppedWave (inner.removeFromLeft (118.0f), 1.2f);
    inner.removeFromLeft (8.0f);
    bitColumns (inner, 16);

    glowText (g, String (bits) + "-BIT  /  " + String (rate / 1000.0f, rate < 10000.0f ? 1 : 0) + " kHz", readout, mono (8.0f),
              hot (accent, 0.35f), Justification::centredLeft, 0.6f);
    text (g, "MSB > LSB", readout, mono (7.0f), cMuted, Justification::centredRight);
    glass (g, a, screenCorner);
}

// ---------------------------------------------------------------------------------------------
// curves (Through The Wall, Tone Up)
// ---------------------------------------------------------------------------------------------
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

float xOfFreq (Rectangle<float> r, float f) { return r.getX() + r.getWidth() * std::log (jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f); }

void curveDisplay (Graphics& g, Rectangle<float> r, Colour lineC, float dbMin, float dbMax, const std::function<float (float)>& dbAt,
                   float zeroDb, float w)
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
    ColourGradient gr (lineC.withAlpha (0.26f), 0.0f, r.getY(), lineC.withAlpha (0.0f), 0.0f, r.getBottom(), false);
    g.setGradientFill (gr);
    g.fillPath (area);
    glowStroke (g, p, lineC, w);
}

void liveThroughTheWall (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
{
    const int type = (int) realParam (EffectType::ThroughTheWall, s, 0);
    const float fc = realParam (EffectType::ThroughTheWall, s, 1);
    const float reso = realParam (EffectType::ThroughTheWall, s, 2);
    const float width = realParam (EffectType::ThroughTheWall, s, 7);
    auto dbAt = [&] (float f) { return filterDb (type, f, fc, reso, width); };

    if (mini)
    {
        curveDisplay (g, a.reduced (6.0f, 5.0f), accent, -30.0f, 18.0f, dbAt, -30.0f, 0.9f);
        glass (g, a, 7.0f);
        return;
    }

    auto inner = a.reduced (8.0f, 6.0f);
    auto readout = inner.removeFromBottom (11.0f);
    inner.removeFromBottom (2.0f);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (inner.expanded (0.0f, 2.0f).toNearestInt());
        curveDisplay (g, inner, accent, -30.0f, 18.0f, dbAt, -30.0f, 1.3f);
    }
    const float cx = xOfFreq (inner, fc);
    const float cy = jmap (jlimit (-30.0f, 18.0f, dbAt (fc)), -30.0f, 18.0f, inner.getBottom(), inner.getY());
    g.setColour (accent.withAlpha (0.25f));
    for (float y = inner.getY(); y < inner.getBottom(); y += 4.0f) g.fillRect (cx - 0.4f, y, 0.8f, 2.0f);
    glowDot (g, { cx, jmax (inner.getY() + 3.0f, cy) }, 2.6f, accent, 1.0f);

    const char* names[] = { "LP12", "LP24", "HP", "BP", "NOTCH", "COMB" };
    glowText (g, names[jlimit (0, 5, type)], inner.withHeight (10.0f), mono (7.5f), hot (accent, 0.5f), Justification::centredRight, 0.6f);
    glowText (g, "CUTOFF " + hz (fc), readout, mono (8.0f), hot (accent, 0.4f), Justification::centredLeft, 0.6f);
    text (g, "RESO " + String (roundToInt (reso * 100.0f)) + " %", readout, mono (7.5f), cMuted, Justification::centredRight);
    glass (g, a, screenCorner);
}

void liveToneUp (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
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
    // Tone Up's accent is chrome silver: tint the curve slightly towards ice so it still reads as light
    const auto lineC = accent.interpolatedWith (cIce, 0.35f);

    if (mini)
    {
        auto plot = a.reduced (6.0f, 5.0f);
        g.setColour (cIce.withAlpha (0.14f));
        g.fillRect (plot.getX(), plot.getCentreY() - 0.3f, plot.getWidth(), 0.6f);
        curveDisplay (g, plot, lineC, -18.0f, 18.0f, dbAt, 0.0f, 0.9f);
        glass (g, a, 7.0f);
        return;
    }

    auto inner = a.reduced (8.0f, 6.0f);
    auto readout = inner.removeFromBottom (11.0f);
    inner.removeFromBottom (2.0f);
    g.setColour (cIce.withAlpha (0.16f));
    g.fillRect (inner.getX(), inner.getCentreY() - 0.35f, inner.getWidth(), 0.7f);
    curveDisplay (g, inner, lineC, -18.0f, 18.0f, dbAt, 0.0f, 1.3f);
    const float fs[] = { jmax (20.0f, lowCut), 100.0f, lmF, hmF, 8000.0f };
    for (int i = 0; i < 5; ++i)
    {
        const float x = xOfFreq (inner, fs[i]);
        const float y = jmap (jlimit (-18.0f, 18.0f, dbAt (fs[i])), -18.0f, 18.0f, inner.getBottom(), inner.getY());
        g.setColour (cInset);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ x, y }));
        Path node;
        node.addEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ x, y }));
        glowStroke (g, node, lineC, 1.0f, 0.7f);
    }
    text (g, "+18", inner.withHeight (9.0f), mono (6.5f), cMuted.withAlpha (0.7f), Justification::topLeft);
    glowText (g, "TILT " + String (tilt >= 0.0f ? "+" : "") + String (tilt, 1) + " dB", readout, mono (8.0f), hot (lineC, 0.3f), Justification::centredLeft, 0.6f);
    text (g, "5 BANDS", readout, mono (7.5f), cMuted, Justification::centredRight);
    glass (g, a, screenCorner);
}

// ---------------------------------------------------------------------------------------------
// Squeeze: arc gain-reduction meter
// ---------------------------------------------------------------------------------------------
void grArc (Graphics& g, Point<float> c, float r, float gr, Colour accent, bool tiny)
{
    const float a0 = -2.0f, a1 = 2.0f;                  // left end = 20 dB GR, right end = 0 dB
    auto angOf = [&] (float db) { return jmap (std::sqrt (jlimit (0.0f, 20.0f, db) / 20.0f), 1.0f, 0.0f, a0, a1); };

    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
    g.setColour (cIce.withAlpha (0.10f));
    g.strokePath (track, PathStrokeType (tiny ? 2.5f : 4.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (! tiny)
    {
        const float marks[] = { 0, 1, 2, 3, 5, 7, 10, 15, 20 };
        for (float db : marks)
        {
            const float an = angOf (db);
            const bool major = db == 0 || db == 5 || db == 10 || db == 20;
            g.setColour (cIce.withAlpha (major ? 0.40f : 0.18f));
            g.drawLine ({ c.getPointOnCircumference (r + 4.0f, an), c.getPointOnCircumference (r + (major ? 7.5f : 6.0f), an) }, 0.8f);
        }
    }

    if (gr > 0.05f)
    {
        Path fill;
        fill.addCentredArc (c.x, c.y, r, r, 0.0f, angOf (gr), a1, true);
        glowStroke (g, fill, accent, tiny ? 2.2f : 3.4f, 1.0f);
        glowDot (g, c.getPointOnCircumference (r, angOf (gr)), tiny ? 1.6f : 2.4f, accent, 1.0f);
    }
}

void liveSqueeze (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
{
    const float gr = s.powered ? jlimit (0.0f, 20.0f, s.gainReductionDb) : 0.0f;

    if (mini)
    {
        grArc (g, { a.getX() + 22.0f, a.getCentreY() + 2.5f }, 9.5f, gr, accent, true);
        glowText (g, String (gr, 1), { a.getX() + 40.0f, a.getY(), a.getWidth() - 46.0f, a.getHeight() }, monoBold (10.0f),
                  hot (accent, 0.45f), Justification::centredRight, 0.7f);
        glass (g, a, 7.0f);
        return;
    }

    grArc (g, { a.getX() + 52.0f, a.getY() + 31.0f }, 19.0f, gr, accent, false);
    text (g, "GR", { a.getX() + 40.0f, a.getY() + 25.0f, 24.0f, 12.0f }, mono (7.5f), cMuted, Justification::centred);
    text (g, "20      0", { a.getX() + 22.0f, a.getY() + 44.0f, 60.0f, 10.0f }, mono (6.5f), cMuted.withAlpha (0.7f), Justification::centred);

    auto side = a.withTrimmedLeft (106.0f).reduced (6.0f, 6.0f);
    glowText (g, String (gr, 1), side.withHeight (24.0f), monoBold (21.0f), hot (accent, 0.5f), Justification::centredLeft, 1.0f);
    text (g, "dB", { side.getX() + 52.0f, side.getY() + 9.0f, 30.0f, 12.0f }, mono (8.0f), cMuted, Justification::centredLeft);
    const float ratio = realParam (EffectType::Squeeze, s, 1);
    text (g, "GAIN REDUCTION", { side.getX(), side.getY() + 26.0f, side.getWidth(), 10.0f }, mono (7.0f), cMuted, Justification::centredLeft);
    text (g, "RATIO " + String (ratio, 1) + ":1", { side.getX(), side.getBottom() - 10.0f, side.getWidth(), 10.0f }, mono (7.0f), cText2.withAlpha (0.8f), Justification::centredLeft);
    glass (g, a, screenCorner);
}

// ---------------------------------------------------------------------------------------------
// Overcooked: three band bars
// ---------------------------------------------------------------------------------------------
void bandBars (Graphics& g, Rectangle<float> r, const State& s, Colour accent, bool labels, float barW)
{
    const char* names[] = { "LOW", "MID", "HIGH" };
    const float cw = r.getWidth() / 3.0f;
    for (int b = 0; b < 3; ++b)
    {
        auto cell = r.withX (r.getX() + (float) b * cw).withWidth (cw);
        auto lab = labels ? cell.removeFromBottom (10.0f) : Rectangle<float>();
        if (labels) cell.removeFromBottom (3.0f);
        auto bar = cell.withSizeKeepingCentre (barW, cell.getHeight());
        const float v = s.powered ? jlimit (0.0f, 1.0f, s.bands[(size_t) b]) : 0.0f;
        g.setColour (accent.withAlpha (0.09f));
        g.fillRoundedRectangle (bar, barW * 0.5f);
        if (v > 0.01f)
        {
            auto lit = bar.withTop (bar.getBottom() - jmax (barW, bar.getHeight() * v));
            ColourGradient gr (accent.withAlpha (0.55f), 0.0f, bar.getBottom(), hot (accent, 0.55f), 0.0f, lit.getY(), false);
            g.setGradientFill (gr);
            glowFillRounded (g, lit, barW * 0.5f, accent, 1.0f);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (lit, barW * 0.5f);
        }
        if (labels) text (g, names[b], lab.expanded (8.0f, 0.0f), mono (7.0f), cMuted, Justification::centred);
    }
}

void liveOvercooked (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
{
    if (mini)
    {
        bandBars (g, a.reduced (14.0f, 5.0f), s, accent, false, 6.0f);
        glass (g, a, 7.0f);
        return;
    }

    bandBars (g, a.withWidth (104.0f).reduced (8.0f, 6.0f), s, accent, true, 9.0f);

    auto side = a.withTrimmedLeft (110.0f).reduced (4.0f, 6.0f).withTrimmedRight (4.0f);
    const float avg = s.powered ? (s.bands[0] + s.bands[1] + s.bands[2]) / 3.0f : 0.0f;
    text (g, "HEAT", side.withHeight (10.0f), mono (7.5f), cMuted, Justification::centredLeft);
    glowText (g, String (roundToInt (avg * 100.0f)), { side.getX(), side.getY() + 10.0f, 60.0f, 22.0f }, monoBold (19.0f), hot (accent, 0.45f),
              Justification::centredLeft, 1.0f);
    text (g, "%", { side.getX() + 26.0f, side.getY() + 18.0f, 20.0f, 12.0f }, mono (8.0f), cMuted, Justification::centredLeft);
    text (g, "UP + DOWN", { side.getX(), side.getBottom() - 10.0f, side.getWidth(), 10.0f }, mono (7.0f), cText2.withAlpha (0.8f), Justification::centredLeft);
    glass (g, a, screenCorner);
}

// ---------------------------------------------------------------------------------------------
// Slap: transient envelope + HIT flash
// ---------------------------------------------------------------------------------------------
float envAt (float x, float a1, float s1) // x 0..1
{
    const float rise = 0.06f;
    const float peak = 0.62f + 0.38f * a1;
    if (x < rise) return x / rise * peak;
    const float floorV = 0.30f + 0.22f * s1;
    const float k = (x - rise) / (1.0f - rise);
    const float decay = std::exp (-k * (6.0f - 3.5f * s1));
    return floorV * (1.0f - k) * (1.0f - decay) + peak * decay;
}

void envelope (Graphics& g, Rectangle<float> plot, float atk, float sus, Colour accent, float w, bool ghost)
{
    for (int pass = ghost ? 0 : 1; pass < 2; ++pass)
    {
        Path env;
        const int n = (int) (plot.getWidth() * 2.0f);
        for (int i = 0; i <= n; ++i)
        {
            const float x = (float) i / (float) n;
            const float v = pass == 0 ? envAt (x, 0.0f, 0.0f) : envAt (x, atk, sus);
            const auto pt = Point<float> (plot.getX() + x * plot.getWidth(), plot.getBottom() - v * plot.getHeight());
            if (i == 0) env.startNewSubPath (pt); else env.lineTo (pt);
        }
        if (pass == 0)
        {
            g.setColour (cIce.withAlpha (0.18f));
            g.strokePath (env, PathStrokeType (0.8f));
        }
        else
        {
            Path area (env);
            area.lineTo (plot.getRight(), plot.getBottom());
            area.lineTo (plot.getX(), plot.getBottom());
            area.closeSubPath();
            ColourGradient gr (accent.withAlpha (0.22f), 0.0f, plot.getY(), accent.withAlpha (0.0f), 0.0f, plot.getBottom(), false);
            g.setGradientFill (gr);
            g.fillPath (area);
            glowStroke (g, env, accent, w);
        }
    }
}

void liveSlap (Graphics& g, Rectangle<float> a, const State& s, bool mini, Colour accent)
{
    const bool hit = s.transientHit && s.powered;
    const float atk = realParam (EffectType::Slap, s, 0), sus = realParam (EffectType::Slap, s, 1);

    auto flash = [&] (Rectangle<float> r, float corner)
    {
        if (! hit) return;
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        ColourGradient fl (accent.withAlpha (0.22f), r.getRight() - r.getHeight() * 0.8f, r.getCentreY(), accent.withAlpha (0.0f), r.getX() + r.getWidth() * 0.3f, r.getCentreY(), true);
        g.setGradientFill (fl);
        g.fillRect (r);
    };

    if (mini)
    {
        flash (a, 7.0f);
        envelope (g, a.reduced (6.0f, 5.0f).withTrimmedRight (20.0f), s.params[0] * 2.0f - 1.0f, s.params[1] * 2.0f - 1.0f, accent, 0.9f, false);
        glowDot (g, { a.getRight() - 11.0f, a.getCentreY() }, 3.2f, accent, hit ? 1.0f : 0.15f);
        glass (g, a, 7.0f);
        return;
    }

    flash (a, screenCorner);
    auto inner = a.reduced (8.0f, 6.0f);
    auto right = inner.removeFromRight (56.0f);
    inner.removeFromRight (6.0f);
    envelope (g, inner, atk, sus, accent, 1.3f, true);

    // HIT pill
    auto pill = right.withSizeKeepingCentre (right.getWidth(), 20.0f).translated (0.0f, -5.0f);
    if (hit)
    {
        g.setColour (accent.withAlpha (0.12f));
        g.fillRoundedRectangle (pill.expanded (6.0f), 16.0f);
        g.setColour (accent.withAlpha (0.25f));
        g.fillRoundedRectangle (pill.expanded (2.5f), 12.5f);
        g.setColour (hot (accent, 0.25f));
        g.fillRoundedRectangle (pill, 10.0f);
        text (g, "HIT", pill, monoBold (10.0f), Colour (0xff05070b), Justification::centred);
    }
    else
    {
        g.setColour (cLine);
        g.drawRoundedRectangle (pill.reduced (0.5f), 10.0f, 1.0f);
        text (g, "HIT", pill, monoBold (10.0f), cMuted, Justification::centred);
    }
    text (g, "TRANSIENT", { right.getX() - 6.0f, pill.getBottom() + 4.0f, right.getWidth() + 12.0f, 10.0f }, mono (6.5f), cMuted, Justification::centred);
    glass (g, a, screenCorner);
}

void paintLive (Graphics& g, EffectType t, Rectangle<float> p, const State& s, bool mini)
{
    const auto a = mini ? miniLiveArea (p) : liveArea (t, p);
    const auto accent = look (t).accent;
    Graphics::ScopedSaveState ss (g);
    Path clip;
    clip.addRoundedRectangle (a, mini ? 7.0f : screenCorner);
    g.reduceClipRegion (clip);
    switch (t)
    {
        case EffectType::CassettePlug:   liveCassette (g, a, s, mini, accent); break;
        case EffectType::Menace:         liveMenace (g, a, s, mini, accent); break;
        case EffectType::Brainrot:       liveBrainrot (g, a, s, mini, accent); break;
        case EffectType::ThroughTheWall: liveThroughTheWall (g, a, s, mini, accent); break;
        case EffectType::ToneUp:         liveToneUp (g, a, s, mini, accent); break;
        case EffectType::Squeeze:        liveSqueeze (g, a, s, mini, accent); break;
        case EffectType::Overcooked:     liveOvercooked (g, a, s, mini, accent); break;
        case EffectType::Slap:           liveSlap (g, a, s, mini, accent); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------
// static layer cache (UI thread only), keyed by type / size / pixel scale / miniature
// ---------------------------------------------------------------------------------------------
Image staticLayer (EffectType t, Rectangle<float> plate, float scale, bool mini)
{
    // cached in the shared UI resources (released with the last editor), never in a static
    const int w = roundToInt (plate.getWidth()), h = roundToInt (plate.getHeight());
    const int sc = roundToInt (scale * 100.0f);
    const int64 key = ((int64) t << 48) ^ ((int64) w << 32) ^ ((int64) h << 16) ^ ((int64) sc << 1) ^ (mini ? 1 : 0);
    return ui::cachedImage ("faceplates.color", key, 96, [&]
    {
        Image img (Image::ARGB, jmax (1, roundToInt ((float) w * scale)), jmax (1, roundToInt ((float) h * scale)), true);
        {
            Graphics ig (img);
            ig.addTransform (AffineTransform::scale (scale));
            paintStatic (ig, t, { 0.0f, 0.0f, (float) w, (float) h }, mini);
        }
        return img;
    });
}
} // namespace

Palette paletteColorGroup (EffectType t)
{
    const auto lk = look (t);
    // plate / plateDark = card and inset, ink = text-2 for knob labels, accent = value arcs, led = glow colour
    return { cCard, cInset, cText2, lk.accent, lk.accent };
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
