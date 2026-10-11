#include "CableStyle.h"
#include "CardStyle.h"

namespace ek::ui::cable
{
using namespace juce;
namespace cs = ek::card;

void paintCable (Graphics& g, Point<float> from, Point<float> to, Colour colour)
{
    const float dist = from.getDistanceFrom (to);
    const float sag = jlimit (12.0f, 80.0f, dist * 0.22f);
    Path p;
    p.startNewSubPath (from);
    p.cubicTo (from.translated (0.0f, sag), to.translated (0.0f, sag), to);
    // soft glow, coloured line, bright core
    cs::strokeGlow (g, p, colour.withAlpha (0.9f), 1.8f, 1.2f);
    g.setColour (Colours::white.withAlpha (0.55f));
    g.strokePath (p, PathStrokeType (0.7f, PathStrokeType::curved, PathStrokeType::rounded));
    // jack end + plug end
    cs::dotGlow (g, from, 3.2f, colour, 1.0f);
    g.setColour (cs::bg);
    g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (to));
    g.setColour (colour);
    g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (to), 1.4f);
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (to));
}

void paintTarget (Graphics& g, Rectangle<float> r, Colour colour)
{
    r = r.expanded (3.0f);
    const float rad = jmin (14.0f, r.getWidth() * 0.5f);
    for (int i = 3; i >= 1; --i)
    {
        const float w = (float) i * 3.0f;
        g.setColour (colour.withAlpha (0.06f * (4.0f - (float) i)));
        g.drawRoundedRectangle (r.expanded (w * 0.5f), rad + w * 0.5f, w);
    }
    g.setColour (colour.withAlpha (0.10f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (colour.withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.3f);
}

void paintOverlay (Graphics& g, Colour colour, Point<float> from, Point<float> to, Rectangle<int> target)
{
    if (! target.isEmpty()) paintTarget (g, target.toFloat(), colour);
    paintCable (g, from, to, colour);
}
} // namespace ek::ui::cable
