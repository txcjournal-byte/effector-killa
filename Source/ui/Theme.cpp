#include "Theme.h"
#include "EKBinaryData.h"

namespace ek::theme
{
using namespace juce;

// Licensed fonts are embedded when present in Resources/Fonts (osd.ttf, vfd.ttf, hand.ttf).
static Typeface::Ptr embedded (const char* resourceName)
{
    int size = 0;
    if (const char* data = EKData::getNamedResource (resourceName, size))
        return Typeface::createSystemTypefaceFor (data, (size_t) size);
    return nullptr;
}

static Typeface::Ptr osdFace()  { static auto t = embedded ("osd_ttf");  return t; }
static Typeface::Ptr vfdFace()  { static auto t = embedded ("vfd_ttf");  return t; }
static Typeface::Ptr handFace() { static auto t = embedded ("hand_ttf"); return t; }

Font osdFont (float h)
{
    if (auto t = osdFace()) return Font (FontOptions (t).withHeight (h));
    return Font (FontOptions().withName (Font::getDefaultMonospacedFontName()).withHeight (h).withStyle ("Bold"));
}
Font vfdFont (float h)
{
    if (auto t = vfdFace()) return Font (FontOptions (t).withHeight (h));
    return Font (FontOptions().withName (Font::getDefaultMonospacedFontName()).withHeight (h)).withHorizontalScale (1.05f);
}
Font handFont (float h)
{
    if (auto t = handFace()) return Font (FontOptions (t).withHeight (h));
    return Font (FontOptions().withName (Font::getDefaultSansSerifFontName()).withHeight (h).withStyle ("Bold Italic"))
        .withHorizontalScale (0.86f);
}
Font stencilFont (float h)
{
    return Font (FontOptions().withName (Font::getDefaultSansSerifFontName()).withHeight (h).withStyle ("Bold"))
        .withHorizontalScale (0.8f);
}

static Point<float> polar (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}

void drawKnob (Graphics& g, Point<float> c, float r, float angle, bool hot)
{
    // drop shadow + socket
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (r * 2.5f, r * 2.5f).withCentre (c.translated (0.0f, r * 0.12f)));
    g.setColour (Colour (0xff0b0806));
    g.fillEllipse (Rectangle<float> (r * 2.28f, r * 2.28f).withCentre (c));

    // skirt
    ColourGradient skirt (Colour (0xff4a4640), c.x - r, c.y - r, Colour (0xff050505), c.x + r, c.y + r, false);
    g.setGradientFill (skirt);
    g.fillEllipse (Rectangle<float> (r * 2.1f, r * 2.1f).withCentre (c));

    // body
    ColourGradient body (Colour (0xff3b3a38), c.x - r * 0.5f, c.y - r * 0.7f, Colour (0xff060606), c.x + r * 0.4f, c.y + r * 0.8f, true);
    g.setGradientFill (body);
    g.fillEllipse (Rectangle<float> (r * 1.8f, r * 1.8f).withCentre (c));

    // chrome rim highlight
    Path arc;
    arc.addCentredArc (c.x, c.y, r * 0.86f, r * 0.86f, 0.0f, -2.1f, -0.2f, true);
    g.setColour (Colours::white.withAlpha (0.55f));
    g.strokePath (arc, PathStrokeType (r * 0.07f, PathStrokeType::curved, PathStrokeType::rounded));
    Path arc2;
    arc2.addCentredArc (c.x, c.y, r * 0.86f, r * 0.86f, 0.0f, 1.2f, 2.4f, true);
    g.setColour (Colours::white.withAlpha (0.15f));
    g.strokePath (arc2, PathStrokeType (r * 0.05f));

    // pointer (orange, glowing)
    const auto p0 = polar (c, r * 0.18f, angle), p1 = polar (c, r * 0.9f, angle);
    g.setColour (amber.withAlpha (hot ? 0.45f : 0.25f));
    g.drawLine ({ p0, p1 }, r * 0.26f);
    g.setColour (Colour (0xffc75a14));
    g.drawLine ({ p0, p1 }, r * 0.14f);
    g.setColour (Colour (0xffffb04a));
    g.drawLine ({ p0, p1 }, r * 0.06f);
}

void drawChickenHeadKnob (Graphics& g, Point<float> c, float r, float angle)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (r * 2.3f, r * 2.3f).withCentre (c.translated (0.0f, r * 0.08f)));
    ColourGradient base (Colour (0xff2c2a27), c.x - r, c.y - r, Colour (0xff030303), c.x + r, c.y + r, false);
    g.setGradientFill (base);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawEllipse (Rectangle<float> (r * 1.9f, r * 1.9f).withCentre (c), r * 0.03f);
    Path rim;
    rim.addCentredArc (c.x, c.y, r * 0.95f, r * 0.95f, 0.0f, -1.9f, -0.4f, true);
    g.setColour (Colours::white.withAlpha (0.45f));
    g.strokePath (rim, PathStrokeType (r * 0.04f));

    // the chrome bar
    const float len = r * 1.05f, w = r * 0.26f;
    Path bar;
    bar.addRoundedRectangle (-w * 0.5f, -len, w, len * 2.0f, w * 0.45f);
    const auto t = AffineTransform::rotation (angle).translated (c);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillPath (bar, AffineTransform::rotation (angle).translated (c.translated (r * 0.05f, r * 0.1f)));
    // gradient across the bar width
    ColourGradient across (Colour (0xfff2efe9), polar (c, 0.0f, angle) + Point<float> (-w * 0.5f * std::cos (angle), -w * 0.5f * std::sin (angle)),
                           Colour (0xff66625d), polar (c, 0.0f, angle) + Point<float> (w * 0.5f * std::cos (angle), w * 0.5f * std::sin (angle)), false);
    across.addColour (0.5, Colour (0xffb6b2ac));
    g.setGradientFill (across);
    g.fillPath (bar, t);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.strokePath (bar, PathStrokeType (1.2f), t);
    // pointer end mark (the end that points at the channel number)
    g.setColour (Colour (0xff2a2622));
    g.drawLine ({ polar (c, len * 0.55f, angle), polar (c, len * 0.92f, angle) }, w * 0.18f);
    g.setColour (amber);
    g.fillEllipse (Rectangle<float> (w * 0.5f, w * 0.5f).withCentre (polar (c, len * 0.8f, angle)));
}

void drawLed (Graphics& g, Point<float> c, float r, Colour colour, bool on, float glow)
{
    g.setColour (Colour (0xff050403));
    g.fillEllipse (Rectangle<float> (r * 2.6f, r * 2.6f).withCentre (c));
    g.setColour (Colour (0xff5a5048).withAlpha (0.8f));
    g.drawEllipse (Rectangle<float> (r * 2.5f, r * 2.5f).withCentre (c), r * 0.18f);
    if (on)
    {
        ColourGradient halo (colour.withAlpha (0.55f * glow), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 4.0f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 8.0f, r * 8.0f).withCentre (c));
        ColourGradient body (colour.brighter (0.9f), c.x - r * 0.3f, c.y - r * 0.3f, colour, c.x + r, c.y + r, true);
        g.setGradientFill (body);
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillEllipse (Rectangle<float> (r * 0.7f, r * 0.7f).withCentre (c.translated (-r * 0.25f, -r * 0.3f)));
    }
    else
    {
        ColourGradient body (colour.darker (1.8f).withMultipliedSaturation (0.6f), c.x - r * 0.3f, c.y - r * 0.3f,
                             Colour (0xff0a0a08), c.x + r, c.y + r, true);
        g.setGradientFill (body);
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.15f));
        g.fillEllipse (Rectangle<float> (r * 0.6f, r * 0.6f).withCentre (c.translated (-r * 0.25f, -r * 0.3f)));
    }
}

void drawSocket (Graphics& g, Rectangle<float> r, float corner)
{
    g.setColour (Colour (0xff070504));
    g.fillRoundedRectangle (r, corner);
    ColourGradient edge (Colour (0xff8c8279).withAlpha (0.7f), r.getX(), r.getY(), Colour (0xff1a1410).withAlpha (0.7f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.8f), corner, 1.6f);
}

void drawLitButton (Graphics& g, Rectangle<float> r, Colour colour, bool lit, bool pressed, const String& text, float fontHeight)
{
    const float corner = r.getWidth() * 0.07f;
    drawSocket (g, r, corner * 1.4f);
    auto face = r.reduced (r.getWidth() * 0.075f);
    if (pressed) face = face.translated (0.0f, 1.5f).reduced (1.0f);

    if (lit)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillRoundedRectangle (face.expanded (5.0f), corner * 2.0f);
        ColourGradient grad (colour.brighter (0.55f), face.getCentreX(), face.getCentreY() - face.getHeight() * 0.1f,
                             colour.darker (0.35f), face.getRight(), face.getBottom(), true);
        g.setGradientFill (grad);
    }
    else
    {
        ColourGradient grad (colour.darker (1.2f).withMultipliedSaturation (0.75f), face.getCentreX(), face.getCentreY(),
                             colour.darker (2.6f), face.getRight(), face.getBottom(), true);
        g.setGradientFill (grad);
    }
    g.fillRoundedRectangle (face, corner);

    // bevel + gloss
    g.setColour (Colours::white.withAlpha (lit ? 0.45f : 0.12f));
    g.drawRoundedRectangle (face.reduced (1.5f), corner, 1.4f);
    ColourGradient gloss (Colours::white.withAlpha (lit ? 0.28f : 0.08f), face.getX(), face.getY(),
                          Colours::transparentWhite, face.getX(), face.getCentreY(), false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.5f).reduced (3.0f, 2.0f), corner);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (face, corner, 1.2f);

    if (text.isNotEmpty())
    {
        g.setColour (lit ? ink : cream.withAlpha (0.55f));
        g.setFont (stencilFont (fontHeight));
        g.drawFittedText (text, face.reduced (4.0f).toNearestInt(), Justification::centred, 3, 0.7f);
    }
}

void drawTrack (Graphics& g, Rectangle<float> r)
{
    const float corner = r.getHeight() * 0.5f;
    g.setColour (Colour (0xff030202));
    g.fillRoundedRectangle (r, corner);
    ColourGradient inner (Colours::black, r.getX(), r.getY(), Colour (0xff1c1612), r.getX(), r.getBottom(), false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (r.reduced (3.0f), corner - 3.0f);
    g.setColour (Colour (0xffa39686).withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (1.0f), corner, 1.5f);
    g.setColour (Colour (0xff4b433b));
    g.fillRoundedRectangle (r.reduced (r.getHeight() * 0.3f, r.getHeight() * 0.44f), 1.0f);
}

void drawSliderThumb (Graphics& g, Rectangle<float> r, bool lit)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (1.5f, 2.5f), 3.0f);
    ColourGradient metal (Colour (0xffdedad3), r.getX(), r.getY(), Colour (0xff5d5852), r.getRight(), r.getBottom(), false);
    metal.addColour (0.5, Colour (0xff9d978f));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 3.0f, 1.2f);
    if (lit)
    {
        auto inner = r.reduced (r.getWidth() * 0.3f, r.getHeight() * 0.15f);
        g.setColour (amber.withAlpha (0.5f));
        g.fillRect (inner.expanded (3.0f, 0.0f));
        g.setColour (amber);
        g.fillRect (inner.withWidth (inner.getWidth() * 0.3f));
        g.fillRect (inner.withTrimmedLeft (inner.getWidth() * 0.7f));
    }
    else
    {
        g.setColour (Colours::black.withAlpha (0.35f));
        for (int i = 1; i < 4; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 4.0f;
            g.drawLine (x, r.getY() + 4.0f, x, r.getBottom() - 4.0f, 1.0f);
        }
    }
}

void drawGlowText (Graphics& g, const String& text, Rectangle<float> r, Font f, Colour c, Justification j, float glow)
{
    g.setFont (f);
    if (glow > 0.0f)
    {
        g.setColour (c.withAlpha (0.18f * glow));
        for (auto d : { Point<float> (-2, 0), Point<float> (2, 0), Point<float> (0, -2), Point<float> (0, 2) })
            g.drawText (text, r.translated (d.x, d.y), j, false);
    }
    g.setColour (c);
    g.drawText (text, r, j, false);
}

void drawPanelGloss (Graphics& g, Rectangle<float> r, float corner)
{
    ColourGradient gloss (Colours::white.withAlpha (0.07f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.4f, false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (r, corner);
}

} // namespace ek::theme
