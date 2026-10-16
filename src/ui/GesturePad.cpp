#include "GesturePad.h"
#include "Palette.h"
#include "Fonts.h"
#include "LookAndFeel.h"

namespace bd
{

GesturePad::GesturePad (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                        const juce::String& title, Icon i, juce::Colour acc)
    : juce::Button (title), icon (i), accent (acc)
{
    setClickingTogglesState (true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
    setTooltip ("Click: toggle " + title + "   |   Right-click: show its controls");
}

void GesturePad::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onSelect) onSelect();
        return;
    }

    juce::Button::mouseDown (e);
    if (onSelect) onSelect();
}

juce::Path GesturePad::makeIcon (juce::Rectangle<float> r) const
{
    using namespace juce;
    Path p;
    const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom();
    const float w = r.getWidth(), h = r.getHeight(), cy = r.getCentreY();

    switch (icon)
    {
        case Icon::glide:
            p.startNewSubPath (x0, y1);
            p.lineTo (x0 + w * 0.28f, y1);
            p.cubicTo (x0 + w * 0.55f, y1, x0 + w * 0.5f, y0, x0 + w * 0.78f, y0);
            p.lineTo (x1, y0);
            break;

        case Icon::chordGlide:
            for (int i = 0; i < 3; ++i)
            {
                const float off = (float) i * h * 0.3f;
                p.startNewSubPath (x0, y0 + h * 0.55f - off * 0.4f + off);
                p.cubicTo (x0 + w * 0.5f, y0 + h * 0.55f + off - off * 0.4f, x0 + w * 0.5f, y0 + off * 0.8f, x1, y0 + off * 0.8f);
            }
            break;

        case Icon::vibrato:
            p.startNewSubPath (x0, cy);
            p.lineTo (x0 + w * 0.3f, cy);
            for (int i = 0; i <= 28; ++i)
            {
                const float t = (float) i / 28.0f;
                const float amp = h * 0.42f * t;
                p.lineTo (x0 + w * 0.3f + w * 0.7f * t, cy + amp * std::sin (t * MathConstants<float>::twoPi * 3.0f));
            }
            break;

        case Icon::dive:
            p.startNewSubPath (x0, y0 + h * 0.15f);
            p.lineTo (x0 + w * 0.45f, y0 + h * 0.15f);
            p.quadraticTo (x0 + w * 0.7f, y0 + h * 0.15f, x1, y1);
            break;

        case Icon::rise:
            p.startNewSubPath (x0, y1 - h * 0.15f);
            p.lineTo (x0 + w * 0.45f, y1 - h * 0.15f);
            p.quadraticTo (x0 + w * 0.7f, y1 - h * 0.15f, x1, y0);
            break;

        case Icon::wobble:
            p.startNewSubPath (x0, cy);
            for (int i = 0; i <= 32; ++i)
            {
                const float t = (float) i / 32.0f;
                p.lineTo (x0 + w * t, cy - h * 0.45f * std::sin (t * MathConstants<float>::twoPi * 1.25f));
            }
            break;

        case Icon::shake:
        {
            Random rng (7);
            p.startNewSubPath (x0, cy);
            for (int i = 1; i <= 14; ++i)
                p.lineTo (x0 + w * (float) i / 14.0f, cy + h * 0.32f * (rng.nextFloat() * 2.0f - 1.0f));
            break;
        }

        case Icon::trill:
        {
            p.startNewSubPath (x0, y1 - h * 0.2f);
            const int steps = 6;
            for (int i = 0; i < steps; ++i)
            {
                const float xa = x0 + w * (float) i / steps;
                const float xb = x0 + w * (float) (i + 1) / steps;
                const float yy = (i % 2 == 0) ? y1 - h * 0.2f : y0 + h * 0.2f;
                p.lineTo (xa, yy);
                p.lineTo (xb, yy);
            }
            break;
        }
    }

    return p;
}

void GesturePad::paintButton (juce::Graphics& g, bool over, bool down)
{
    using namespace juce;

    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float corner = jmin (14.0f, r.getHeight() * 0.22f);
    const bool on = getToggleState();
    const float breathe = on ? 0.5f + 0.5f * std::sin (phase * MathConstants<float>::twoPi) : 0.0f;

    // outer glow
    if (on)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (accent.withAlpha (0.05f + 0.035f * breathe));
            g.fillRoundedRectangle (r.expanded ((float) i * 2.5f), corner + (float) i * 2.5f);
        }
    }

    // glass body
    {
        ColourGradient body (on ? accent.withAlpha (0.22f) : pal::ground1.withAlpha (0.75f), r.getTopLeft(),
                             pal::ground0.withAlpha (0.92f), r.getBottomRight(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, corner);
    }

    // border
    g.setColour (on ? accent.withAlpha (0.85f) : (over ? pal::cyan.withAlpha (0.45f) : pal::hairline.withAlpha (0.35f)));
    g.drawRoundedRectangle (r, corner, on ? 1.4f : 1.0f);

    if (selected)
    {
        g.setColour (pal::lime.withAlpha (0.9f));
        const auto bar = Rectangle<float> (r.getWidth() * 0.32f, 2.5f).withCentre ({ r.getCentreX(), r.getBottom() - 4.0f });
        g.fillRoundedRectangle (bar, 1.2f);
    }

    // icon
    auto content = r.reduced (r.getWidth() * 0.14f, r.getHeight() * 0.14f);
    auto iconArea = content.removeFromTop (content.getHeight() * 0.56f).reduced (content.getWidth() * 0.08f, 2.0f);
    const auto iconPath = makeIcon (iconArea);
    const auto iconCol = on ? accent.interpolatedWith (pal::text, 0.25f) : pal::textDim.withAlpha (0.65f);

    if (on)
        BendDemonLookAndFeel::strokeGlow (g, iconPath, iconCol, 2.0f, 9.0f + 4.0f * breathe, 4);
    else
    {
        g.setColour (iconCol);
        g.strokePath (iconPath, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // node on icon end
    if (on)
    {
        const auto end = iconPath.getCurrentPosition();
        g.setColour (pal::lime.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (end));
        g.setColour (pal::lime);
        g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (end));
    }

    // title
    g.setColour (on ? pal::text : pal::textDim);
    g.setFont (fonts::display (jlimit (9.0f, 14.0f, r.getHeight() * 0.14f), on ? 2 : 1));
    g.drawFittedText (getButtonText(), content.toNearestInt(), Justification::centred, 1, 0.6f);

    if (down)
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (r, corner);
    }
}

} // namespace bd
