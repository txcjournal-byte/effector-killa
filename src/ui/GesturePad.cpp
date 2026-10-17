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
    setTooltip ("Click: switch " + title + " on/off for live notes  |  Right-click: show its controls");
}

void GesturePad::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || selectOnly)
    {
        if (onSelect) onSelect();
        return;
    }

    juce::Button::mouseDown (e);
    if (onSelect) onSelect();
}

juce::Path GesturePad::iconPath (Icon icon, juce::Rectangle<float> r)
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

    // 2.0: compact tile  [icon  NAME            o]
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = pal::metric::corner;
    const bool on = getToggleState();

    g.setColour (down ? pal::ground1 : ((over || selected) ? pal::raised : pal::panel));
    g.fillRoundedRectangle (r, corner);
    if (on)
    {
        g.setColour (accent.withAlpha (0.12f));
        g.fillRoundedRectangle (r, corner);
    }
    g.setColour (selected ? pal::text.withAlpha (0.9f) : (on ? accent.withAlpha (0.5f) : (over ? pal::textFaint : pal::hairline)));
    g.drawRoundedRectangle (r, corner, selected ? 1.5f : 1.0f);

    auto content = r.reduced (r.getHeight() * 0.22f, r.getHeight() * 0.2f);
    auto iconArea = content.removeFromLeft (content.getHeight() * 1.5f);
    const auto iconPath = makeIcon (iconArea.reduced (0.0f, iconArea.getHeight() * 0.08f));
    g.setColour (on ? accent : pal::textDim);
    g.strokePath (iconPath, PathStrokeType (jmax (1.2f, r.getHeight() * 0.05f), PathStrokeType::curved, PathStrokeType::rounded));

    // on/off indicator
    auto dot = content.removeFromRight (content.getHeight() * 0.7f);
    g.setColour (on ? accent : pal::hairline);
    g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (dot.getCentre()));

    content.removeFromLeft (r.getHeight() * 0.2f);
    g.setColour (on || selected ? pal::text : pal::textDim);
    g.setFont (fonts::ui (jlimit (10.0f, 13.5f, r.getHeight() * 0.42f), true));
    g.drawFittedText (getButtonText(), content.toNearestInt(), Justification::centredLeft, 1, 0.7f);
    ignoreUnused (phase);
}

} // namespace bd
