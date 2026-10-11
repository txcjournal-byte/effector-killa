#include "Faceplates.h"
#include "CardStyle.h"

namespace ek::faceplate
{
static bool isColorGroup (EffectType t) { return (int) t >= (int) EffectType::CassettePlug && (int) t <= (int) EffectType::Slap; }

// ------------------------------------------------------------------------------------------------
// Card layout (TrapWin CHAIN view). Two variants, chosen by the plate width:
//
//   FULL  (width >= compactBelow, height 140) - any width from ~680 up (852 chain, ~1088 without the library)
//     +--------------------------------------------------------------------------------------+
//     | NAME (chrome)          role text ...      [ST|MID|SIDE] [ preset name  v ] (g)(P)(S)(L)(x) |
//     | CATEGORY * TRAPWIN      LABEL  LABEL  LABEL ...                         DRY/WET      |
//     | [ live screen      ]    (knob) (knob) (knob) ...                        (knob)       |
//     |                         value  value  value                             value        |
//     +--------------------------------------------------------------------------------------+
//
//   COMPACT (SPLIT branch cards, ~400 wide, height from UnitComponent::idealHeight)
//     +-------------------------------------------+
//     | NAME (chrome, smaller)  [ live screen   ] |
//     | CATEGORY * TRAPWIN      [               ] |
//     | (A) [ preset  v ] [ST|MID|SIDE] (g)(P)(S)(L)(x)|
//     | knobs, one or two rows                    |
//     +-------------------------------------------+
// The unit's own buttons (icon pills, ST/MID/SIDE, preset pill) are positioned by RackView from
// presetLcdArea() with the constants below, so faceplates and controls always agree.
// ------------------------------------------------------------------------------------------------
namespace
{
constexpr float compactBelow = 600.0f;
constexpr float iconPill = 22.0f, iconGap = 5.0f;
constexpr float msWidth = 84.0f;
constexpr float iconRowWidth = 5.0f * iconPill + 4.0f * iconGap;

bool compact (juce::Rectangle<float> p) { return p.getWidth() < compactBelow; }
}

juce::Rectangle<float> nameArea (juce::Rectangle<float> p)
{
    if (compact (p))
        return { p.getX() + 18.0f, p.getY() + 9.0f, juce::jmin (230.0f, p.getWidth() * 0.5f - 12.0f), 52.0f };
    return { p.getX() + 22.0f, p.getY() + 12.0f, 220.0f, 52.0f };
}

juce::Rectangle<float> presetLcdArea (juce::Rectangle<float> p)
{
    if (compact (p))
    {
        // second row: [A] tag, preset pill, ST/MID/SIDE, icon pills
        const float x0 = p.getX() + 44.0f;
        const float x1 = p.getRight() - 14.0f - iconRowWidth - 8.0f - msWidth - 8.0f;
        return { x0, p.getY() + 68.0f, juce::jmax (60.0f, x1 - x0), 22.0f };
    }
    const float w = juce::jlimit (130.0f, 190.0f, p.getWidth() * 0.18f);
    return { p.getRight() - 18.0f - iconRowWidth - 10.0f - w, p.getY() + 12.0f, w, 22.0f };
}

juce::Rectangle<float> liveArea (EffectType, juce::Rectangle<float> p)
{
    if (compact (p))
    {
        const float x = nameArea (p).getRight() + 10.0f;
        return { x, p.getY() + 10.0f, p.getRight() - 14.0f - x, 50.0f };
    }
    return { p.getX() + 22.0f, p.getY() + 70.0f, 220.0f, 58.0f };
}

juce::Rectangle<float> knobArea (EffectType, juce::Rectangle<float> p)
{
    if (compact (p))
        return { p.getX() + 12.0f, p.getY() + 98.0f, p.getWidth() - 24.0f, p.getHeight() - 104.0f };
    return { p.getX() + 258.0f, p.getY() + 40.0f, p.getWidth() - 280.0f, 94.0f };
}

Palette palette (EffectType t)
{
    if (t == EffectType::None) return { ek::card::card, ek::card::screen, ek::card::text2, ek::card::ice, ek::card::ice };
    return isColorGroup (t) ? paletteColorGroup (t) : paletteSpaceGroup (t);
}

void paintFaceplate (juce::Graphics& g, EffectType t, juce::Rectangle<float> plate, const State& s, bool miniature)
{
    if (t == EffectType::None) return;
    if (isColorGroup (t)) paintColorGroup (g, t, plate, s, miniature);
    else                  paintSpaceGroup (g, t, plate, s, miniature);

    // POWER off / dimmed by SOLO: a rounded veil that follows the card's corners
    if (! s.powered || s.soloDimmed)
    {
        const float rad = miniature ? ek::card::radiusMini : ek::card::radius;
        g.setColour (ek::card::bg.withAlpha (s.powered ? 0.5f : 0.62f));
        g.fillRoundedRectangle (plate, rad);
    }
}
} // namespace ek::faceplate
