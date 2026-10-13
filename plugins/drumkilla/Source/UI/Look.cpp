#include "Look.h"
#include "BinaryData.h"

using namespace juce;

namespace Look
{

Fonts::Fonts()
{
    monoFace = Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    sansFace = Typeface::createSystemTypefaceFor (BinaryData::ShareTechRegular_ttf, BinaryData::ShareTechRegular_ttfSize);
    boldFace = Typeface::createSystemTypefaceFor (BinaryData::RajdhaniBold_ttf, BinaryData::RajdhaniBold_ttfSize);
}

Font Fonts::mono (float h) const { return Font (FontOptions (monoFace).withHeight (h)); }
Font Fonts::sans (float h) const { return Font (FontOptions (sansFace).withHeight (h)); }
Font Fonts::bold (float h) const { return Font (FontOptions (boldFace).withHeight (h)); }

void drawPanel (Graphics& g, Rectangle<float> r, bool dark)
{
    if (dark)
    {
        g.setColour (Colour (0xff0b0b0a));
        g.fillRect (r);
    }
    else
    {
        g.setColour (Colours::black.withAlpha (0.04f));
        g.fillRect (r);
    }

    g.setColour (Palette::panelEdge);
    g.drawRect (r, 3.0f);
    g.setColour (Colours::white.withAlpha (dark ? 0.08f : 0.28f));
    g.drawRect (r.reduced (3.5f), 1.0f);
}

void drawLabel (Graphics& g, const String& text, Rectangle<float> area, float height, Colour colour, Justification just)
{
    static SharedResourcePointer<Fonts> fonts;
    g.setColour (colour);
    g.setFont (fonts->bold (height));
    g.drawFittedText (text, area.toNearestInt(), just, 1, 0.75f);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    knobFace = ImageCache::getFromMemory (BinaryData::knob_face_png, BinaryData::knob_face_pngSize);

    setColour (PopupMenu::backgroundColourId, Colour (0xff0d0d0c));
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::headerTextColourId, Palette::redBright);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff8e1510));
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TooltipWindow::textColourId, Palette::text);
    setColour (AlertWindow::backgroundColourId, Colour (0xff111110));
    setColour (AlertWindow::textColourId, Palette::text);
    setColour (TextEditor::backgroundColourId, Colour (0xff050505));
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::outlineColourId, Palette::boxEdge);
    setColour (TextButton::buttonColourId, Colour (0xff1a1918));
    setColour (TextButton::textColourOffId, Palette::text);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, Slider& slider)
{
    const auto& props = slider.getProperties();
    const auto bipolar = (bool) props["bipolar"];
    const auto centre = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const auto outer = (float) jmin (w, h) * 0.5f;
    const auto arcR = outer - outer * 0.13f;
    const auto capR = arcR * 0.8f;
    const auto hero = (bool) props["hero"];
    const auto arcW = jlimit (4.0f, hero ? 20.0f : 14.0f, arcR * (hero ? 0.1f : 0.075f));
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const auto enabled = slider.isEnabled();

    auto polar = [centre] (float a, float r) { return centre + Point<float> (r * std::sin (a), -r * std::cos (a)); };

    // tick marks
    g.setColour ((bool) props["dark"] ? Palette::textDim : Palette::ink.withAlpha (0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto major = i == 0 || i == 5 || i == 10;
        g.drawLine (Line<float> (polar (a, arcR + arcW * 0.9f), polar (a, arcR + arcW * (major ? 2.6f : 1.9f))),
                    major ? 2.4f : 1.6f);
    }

    // track + value arc
    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::track);
    g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::butt));

    const auto from = bipolar ? (startAngle + endAngle) * 0.5f : ((bool) props["fromEnd"] ? endAngle : startAngle);
    if (enabled && std::abs (angle - from) > 0.01f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (Palette::redGlow.withAlpha (0.22f));
        g.strokePath (value, PathStrokeType (arcW * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::red);
        g.strokePath (value, PathStrokeType (arcW * 0.75f, PathStrokeType::curved, PathStrokeType::butt));
        g.setColour (Palette::redBright.withAlpha (0.7f));
        g.strokePath (value, PathStrokeType (jmax (1.0f, arcW * 0.2f), PathStrokeType::curved, PathStrokeType::butt));
    }

    // drop shadow
    for (int i = 4; i > 0; --i)
    {
        g.setColour (Colours::black.withAlpha (0.07f));
        g.fillEllipse (Rectangle<float> (capR * 2.0f + (float) i * 4.0f, capR * 2.0f + (float) i * 4.0f)
                           .withCentre (centre.translated (capR * 0.06f, capR * 0.1f)));
    }

    // body rim
    const auto body = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);

    if (hero && enabled)
    {
        // the main KILL knob glows red with its value
        for (int i = 3; i > 0; --i)
        {
            g.setColour (Palette::redGlow.withAlpha ((0.06f + 0.1f * pos) / (float) i));
            g.drawEllipse (body.expanded ((float) i * 5.0f), 6.0f);
        }
    }
    g.setGradientFill (ColourGradient (Colour (0xff3a3835), body.getX(), body.getY(),
                                       Colour (0xff030303), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colours::black);
    g.drawEllipse (body, 1.5f);

    // textured face
    const auto face = body.reduced (capR * 0.14f);
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addEllipse (face);
        g.reduceClipRegion (clip);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (knobFace, face);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), face.getX(), face.getY(),
                                           Colours::black.withAlpha (0.35f), face.getRight(), face.getBottom(), false));
        g.fillEllipse (face);
    }
    g.setColour (Colour (0xff5a5751).withAlpha (0.8f));
    g.drawEllipse (face, 1.2f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawEllipse (body.reduced (1.5f), 1.0f);

    // pointer
    const Line<float> pointer (polar (angle, capR * 0.22f), polar (angle, capR * 0.86f));
    const auto pw = jmax (3.0f, capR * 0.085f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawLine (pointer, pw + 3.0f);
    g.setColour (enabled ? Palette::red : Colour (0xff6a6660));
    g.drawLine (pointer, pw);
    g.setColour ((enabled ? Colour (0xffff7a66) : Colour (0xffa09b92)).withAlpha (0.85f));
    g.drawLine (pointer, jmax (1.0f, pw * 0.3f));
}

void LookAndFeel::drawPopupMenuBackground (Graphics& g, int width, int height)
{
    g.fillAll (findColour (PopupMenu::backgroundColourId));
    g.setColour (Palette::red.withAlpha (0.6f));
    g.drawRect (0, 0, width, height, 1);
}

Font LookAndFeel::getPopupMenuFont()
{
    return fonts->sans (18.0f);   // popup menus are separate windows: not scaled with the editor
}

void LookAndFeel::drawTooltip (Graphics& g, const String& text, int width, int height)
{
    g.fillAll (Colour (0xf00b0b0a));
    g.setColour (Palette::red.withAlpha (0.7f));
    g.drawRect (0, 0, width, height, 1);
    g.setColour (Palette::text);
    g.setFont (fonts->sans (19.0f));
    g.drawFittedText (text, Rectangle<int> (width, height).reduced (8, 4), Justification::centredLeft, 4);
}

Rectangle<int> LookAndFeel::getTooltipBounds (const String& text, Point<int> screenPos, Rectangle<int> parentArea)
{
    const auto f = fonts->sans (19.0f);
    const auto textW = jmin (400, (int) GlyphArrangement::getStringWidth (f, text) + 24);
    const auto lines = 1 + (int) (GlyphArrangement::getStringWidth (f, text) / 376.0f);
    const auto w = textW, h = 14 + lines * 23;
    return Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                           screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

} // namespace Look
