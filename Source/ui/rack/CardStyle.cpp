#include "CardStyle.h"
#include "EKBinaryData.h"

namespace ek::card
{
using namespace juce;

namespace
{
Typeface::Ptr embedded (const char* resourceName)
{
    int size = 0;
    if (const char* data = EKData::getNamedResource (resourceName, size))
        return Typeface::createSystemTypefaceFor (data, (size_t) size);
    return nullptr;
}
Typeface::Ptr archivo()  { static auto t = embedded ("display_ttf"); return t; }   // Archivo Black
Typeface::Ptr plexMed()  { static auto t = embedded ("vfd_ttf");     return t; }   // IBM Plex Mono Medium
Typeface::Ptr plexBold() { static auto t = embedded ("osd_ttf");     return t; }   // IBM Plex Mono Bold

constexpr float pi = MathConstants<float>::pi;
}

Font displayFont (float h)
{
    if (auto t = archivo()) return Font (FontOptions (t).withHeight (h));
    return Font (FontOptions().withName (Font::getDefaultSansSerifFontName()).withHeight (h).withStyle ("Bold"));
}

Font monoFont (float h, bool bold)
{
    auto t = bold ? plexBold() : plexMed();
    Font f = t != nullptr ? Font (FontOptions (t).withHeight (h))
                          : Font (FontOptions().withName (Font::getDefaultMonospacedFontName()).withHeight (h));
    return f.withExtraKerningFactor (1.0f / jmax (6.0f, h));   // +1 px letter spacing
}

void paintCard (Graphics& g, Rectangle<float> r, Colour accent, bool mini)
{
    const float rad = mini ? radiusMini : radius;
    Path shape;
    shape.addRoundedRectangle (r, rad);

    // body: card-2 at the top fading into card / a touch darker at the bottom
    ColourGradient body (card2, r.getX(), r.getY(), Colour (0xff080c14), r.getX(), r.getBottom(), false);
    body.addColour (0.45, card);
    g.setGradientFill (body);
    g.fillPath (shape);

    g.saveState();
    g.reduceClipRegion (shape);
    // faint accent glow breathing in from the left edge
    {
        const float gw = mini ? r.getWidth() * 0.45f : jmin (360.0f, r.getWidth() * 0.42f);
        ColourGradient glow (accent.withAlpha (mini ? 0.10f : 0.075f), r.getX(), r.getCentreY(),
                             accent.withAlpha (0.0f), r.getX() + gw, r.getCentreY(), true);
        g.setGradientFill (glow);
        g.fillRect (r.withWidth (gw));
    }
    // very soft ice-blue haze in the top-right corner (the site's radial background glow)
    {
        ColourGradient haze (iceDeep.withAlpha (0.06f), r.getRight(), r.getY(),
                             iceDeep.withAlpha (0.0f), r.getRight() - r.getWidth() * 0.45f, r.getY() + r.getHeight(), true);
        g.setGradientFill (haze);
        g.fillRect (r);
    }
    // top sheen (inner gradient)
    {
        ColourGradient sheen (Colours::white.withAlpha (0.035f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f,
                              r.getY() + (mini ? 16.0f : 46.0f), false);
        g.setGradientFill (sheen);
        g.fillRect (r);
    }
    g.restoreState();

    // 1 px line
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    // brighter hairline along the top edge (light from above, very subtle)
    {
        ColourGradient top (ice.withAlpha (0.0f), r.getX() + rad, 0.0f, ice.withAlpha (0.0f), r.getRight() - rad, 0.0f, false);
        top.addColour (0.3, ice.withAlpha (0.16f));
        top.addColour (0.7, ice.withAlpha (0.10f));
        g.setGradientFill (top);
        g.fillRect (r.getX() + rad, r.getY() + 0.5f, r.getWidth() - 2.0f * rad, 0.8f);
    }

    // 2 px accent gradient edge on the left (follows the straight part of the side, fades at both ends)
    {
        const float x = r.getX() + 1.0f;
        const float y0 = r.getY() + rad * 0.55f, y1 = r.getBottom() - rad * 0.55f;
        ColourGradient edge (accent.withAlpha (0.0f), 0.0f, y0, accent.withAlpha (0.0f), 0.0f, y1, false);
        edge.addColour (0.18, accent.withAlpha (0.95f));
        edge.addColour (0.50, accent.brighter (0.35f));
        edge.addColour (0.82, accent.withAlpha (0.95f));
        // glow
        ColourGradient edgeGlow (edge);
        edgeGlow.multiplyOpacity (0.22f);
        g.setGradientFill (edgeGlow);
        g.fillRoundedRectangle (Rectangle<float> (x - 1.0f, y0, mini ? 5.0f : 7.0f, y1 - y0), 2.0f);
        g.setGradientFill (edge);
        g.fillRoundedRectangle (Rectangle<float> (x, y0, 2.0f, y1 - y0), 1.0f);
    }
}

Rectangle<float> paintChromeTitle (Graphics& g, const String& name, Rectangle<float> r, float maxHeight, float glowAmt)
{
    const String s = name.toUpperCase();
    Font f = displayFont (maxHeight);
    auto widthOf = [&] (const Font& ff) { return GlyphArrangement::getStringWidth (ff, s) + ff.getAscent() * 0.72f * shear; };
    float w = widthOf (f);
    if (w > r.getWidth())
    {
        // shrink first (down to 72 %), then squeeze horizontally
        const float h = jmax (maxHeight * 0.72f, maxHeight * r.getWidth() / w);
        f = displayFont (h);
        w = widthOf (f);
        if (w > r.getWidth()) f = f.withHorizontalScale (r.getWidth() / w);
    }
    const float cap = f.getAscent() * 0.72f;                     // Archivo cap height ~ 0.72 ascent
    const float baseline = r.getCentreY() + cap * 0.5f;

    Path p;
    {
        GlyphArrangement ga;
        ga.addLineOfText (f, s, r.getX(), baseline);
        ga.createPath (p);
    }
    p.applyTransform (AffineTransform::shear (-shear, 0.0f).translated (shear * baseline, 0.0f));
    const auto pb = p.getBounds();

    if (glowAmt > 0.0f)
    {
        // soft blue glow, like the "BUNDLES." headline
        DropShadow (iceDeep.withAlpha (0.55f * glowAmt), roundToInt (jmax (4.0f, cap * 0.75f)), {}).drawForPath (g, p);
        DropShadow (ice.withAlpha (0.18f * glowAmt), roundToInt (jmax (2.0f, cap * 0.22f)), {}).drawForPath (g, p);
    }
    ColourGradient chrome (Colour (0xfff4f6fb), 0.0f, pb.getY(), Colour (0xffe6eaf2), 0.0f, pb.getBottom(), false);
    chrome.addColour (0.48, Colour (0xffc9d0dd));
    chrome.addColour (0.52, Colour (0xff8e98ab));
    g.setGradientFill (chrome);
    g.fillPath (p);
    return pb;
}

void paintSparkle (Graphics& g, Point<float> c, float r, Colour colour, float glow)
{
    auto star = [&] (float rr)
    {
        Path p;
        const float k = 0.22f;   // waist of the four-point star
        p.startNewSubPath (c.x, c.y - rr);
        p.quadraticTo (c.x + rr * k * 0.5f, c.y - rr * k * 0.5f, c.x + rr, c.y);
        p.quadraticTo (c.x + rr * k * 0.5f, c.y + rr * k * 0.5f, c.x, c.y + rr);
        p.quadraticTo (c.x - rr * k * 0.5f, c.y + rr * k * 0.5f, c.x - rr, c.y);
        p.quadraticTo (c.x - rr * k * 0.5f, c.y - rr * k * 0.5f, c.x, c.y - rr);
        p.closeSubPath();
        return p;
    };
    if (glow > 0.0f)
    {
        ColourGradient halo (colour.withAlpha (0.35f * glow), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 1.4f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 2.8f, r * 2.8f).withCentre (c));
    }
    g.setColour (colour);
    g.fillPath (star (r));
}

void paintCategory (Graphics& g, const String& category, Rectangle<float> r, float h, Colour accent)
{
    const Font f = monoFont (h);
    const String cat = category.toUpperCase();
    g.setFont (f);
    g.setColour (muted);
    g.drawText (cat, r, Justification::centredLeft, false);
    float x = r.getX() + GlyphArrangement::getStringWidth (f, cat) + h * 1.1f;
    // separator dot
    g.setColour (muted.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (h * 0.22f, h * 0.22f).withCentre ({ x, r.getCentreY() }));
    x += h * 1.0f;
    paintSparkle (g, { x + h * 0.38f, r.getCentreY() }, h * 0.42f, accent.interpolatedWith (ice, 0.35f), 0.8f);
    x += h * 1.15f;
    g.setColour (text2.withAlpha (0.85f));
    g.drawText ("TRAPWIN", Rectangle<float> (x, r.getY(), jmax (0.0f, r.getRight() - x), r.getHeight()), Justification::centredLeft, false);
}

void paintScreen (Graphics& g, Rectangle<float> r, float corner, Colour accent)
{
    g.setColour (screen);
    g.fillRoundedRectangle (r, corner);
    Path shape;
    shape.addRoundedRectangle (r, corner);
    g.saveState();
    g.reduceClipRegion (shape);
    // accent tint glowing from below (backlight)
    ColourGradient tint (accent.withAlpha (0.10f), r.getCentreX(), r.getBottom() + r.getHeight() * 0.2f,
                         accent.withAlpha (0.0f), r.getCentreX(), r.getY() - r.getHeight() * 0.3f, true);
    g.setGradientFill (tint);
    g.fillRect (r);
    // inset shadow along the top edge
    ColourGradient sh (Colours::black.withAlpha (0.55f), 0.0f, r.getY(), Colours::transparentBlack, 0.0f, r.getY() + jmin (8.0f, r.getHeight() * 0.3f), false);
    g.setGradientFill (sh);
    g.fillRect (r);
    g.restoreState();
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), corner - 0.5f, 1.0f);
}

void paintScreenGlass (Graphics& g, Rectangle<float> r, float corner)
{
    Path shape;
    shape.addRoundedRectangle (r, corner);
    g.saveState();
    g.reduceClipRegion (shape);
    ColourGradient sheen (Colours::white.withAlpha (0.045f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getCentreY(), false);
    g.setGradientFill (sheen);
    g.fillRect (r.withHeight (r.getHeight() * 0.5f));
    g.restoreState();
    // thin bright lip at the lower edge (glass catching light)
    ColourGradient lip (ice.withAlpha (0.0f), r.getX() + corner, 0.0f, ice.withAlpha (0.0f), r.getRight() - corner, 0.0f, false);
    lip.addColour (0.5, ice.withAlpha (0.14f));
    g.setGradientFill (lip);
    g.fillRect (r.getX() + corner, r.getBottom() - 1.0f, r.getWidth() - 2.0f * corner, 0.7f);
}

void paintPill (Graphics& g, Rectangle<float> r, Colour accent)
{
    const float rad = r.getHeight() * 0.5f;
    g.setColour (bg);
    g.fillRoundedRectangle (r, rad);
    ColourGradient inner (Colours::white.withAlpha (0.03f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getBottom(), false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (r, rad);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    // tiny accent status dot on the left
    dotGlow (g, { r.getX() + rad, r.getCentreY() }, jmax (1.4f, r.getHeight() * 0.09f), accent, 0.8f);
}

void strokeGlow (Graphics& g, const Path& p, Colour c, float w, float glow)
{
    if (glow > 0.0f)
    {
        g.setColour (c.withAlpha (c.getFloatAlpha() * 0.07f * glow));
        g.strokePath (p, PathStrokeType (w * 6.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (c.withAlpha (c.getFloatAlpha() * 0.16f * glow));
        g.strokePath (p, PathStrokeType (w * 3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
}

void dotGlow (Graphics& g, Point<float> c, float r, Colour col, float level)
{
    level = jlimit (0.0f, 1.0f, level);
    if (level <= 0.02f)
    {
        // unlit: a faint ring
        g.setColour (col.withAlpha (0.16f));
        g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), jmin (1.0f, r * 0.35f));
        return;
    }
    ColourGradient halo (col.withAlpha (0.38f * level), c.x, c.y, col.withAlpha (0.0f), c.x + r * 3.2f, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (Rectangle<float> (r * 6.4f, r * 6.4f).withCentre (c));
    g.setColour (col.withAlpha (0.35f + 0.65f * level));
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.65f * level));
    g.fillEllipse (Rectangle<float> (r * 0.9f, r * 0.9f).withCentre (c));
}

void glowText (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c, Justification j, float glow)
{
    g.setFont (f);
    if (glow > 0.0f)
    {
        const float d = jmax (0.8f, f.getHeight() * 0.06f);
        g.setColour (c.withAlpha (c.getFloatAlpha() * 0.13f * glow));
        for (auto o : { Point<float> (-d, 0), Point<float> (d, 0), Point<float> (0, -d), Point<float> (0, d),
                        Point<float> (-2 * d, 0), Point<float> (2 * d, 0) })
            g.drawText (s, r.translated (o.x, o.y), j, false);
    }
    g.setColour (c);
    g.drawText (s, r, j, false);
}

void paintKnob (Graphics& g, Point<float> c, float r, float value, Colour accent)
{
    constexpr float a0 = -0.75f * pi, a1 = 0.75f * pi;
    value = jlimit (0.0f, 1.0f, value);
    const float av = a0 + (a1 - a0) * value;
    // track
    Path track;
    track.addCentredArc (c.x, c.y, r * 1.22f, r * 1.22f, 0.0f, a0, a1, true);
    g.setColour (ice.withAlpha (0.10f));
    g.strokePath (track, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    // value arc with glow
    Path arc;
    arc.addCentredArc (c.x, c.y, r * 1.22f, r * 1.22f, 0.0f, a0, av, true);
    strokeGlow (g, arc, accent, 2.0f, 0.9f);
    // disc
    ColourGradient disc (Colour (0xff121a28), c.x, c.y - r, Colour (0xff070b12), c.x, c.y + r, false);
    g.setGradientFill (disc);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (line);
    g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c).reduced (0.5f), 1.0f);
    // pointer dot
    const Point<float> pd (c.x + r * 0.66f * std::sin (av), c.y - r * 0.66f * std::cos (av));
    g.setColour (Colours::white.withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (r * 0.36f, r * 0.36f).withCentre (pd));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (r * 0.2f, r * 0.2f).withCentre (pd));
}
} // namespace ek::card
