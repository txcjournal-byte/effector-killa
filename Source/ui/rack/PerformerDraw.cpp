#include "PerformerDraw.h"
#include "CardStyle.h"
#include "Hardware.h"

namespace ek::ui::performer
{
using namespace juce;

Colour macroColour (int m) noexcept
{
    static const Colour c[] = { Colour (0xffb07cff),   // VILLAIN ARC
                                Colour (0xffff4d5e),   // CRASH OUT
                                Colour (0xff3fd0e0),   // AURA
                                Colour (0xff7cfc6b),   // DRIP
                                Colour (0xffff8a3d) }; // KNOCK
    return c[jlimit (0, 4, m)];
}

void drawSilkscreen (Graphics& g, const String& text, Rectangle<float> r, Colour c, float height, Justification j)
{
    g.setFont (card::monoFont (height));
    g.setColour (c);
    g.drawText (text, r, j, false);
}

void drawBlackGlass (Graphics& g, Rectangle<float> r, float corner)
{
    hw::drawCard (g, r, jmax (corner, 16.0f), 0.6f);
}

void drawLedTab (Graphics& g, Rectangle<float> r, const String& text, Colour led, bool selected, bool hover)
{
    hw::drawPill (g, r, text, selected ? led : iceLine, selected, false, hover, hw::PillStyle::Outline, true,
                  jmin (10.5f, r.getHeight() * 0.42f));
}
} // namespace ek::ui::performer
