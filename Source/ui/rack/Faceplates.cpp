#include "Faceplates.h"
#include "CardStyle.h"

namespace ek::faceplate
{
static bool isColorGroup (EffectType t) { return (int) t >= (int) EffectType::CassettePlug && (int) t <= (int) EffectType::Slap; }

// ------------------------------------------------------------------------------------------------
// Card layout (TrapWin CHAIN view, UX pass: readable at 100 %, nothing below 11 px)
//
//   FULL  (width >= compactBelow, height 120) - 724 wide in the 752 chain viewport
//     +------------------------------------------------------------------------------------+
//     | NAME (chrome 22 px)                 [ST|MID|SIDE] [ preset name  v ] (g)(P)(S)(L)(x) |
//     | +--------------+  LABEL  LABEL  LABEL  LABEL ...                         DRY/WET     |
//     | | live screen  |  (knob) (knob) (knob) (knob)                            (knob)      |
//     | +--------------+  value  value  value  value                             value       |
//     +------------------------------------------------------------------------------------+
//   The screen gives way (128..184 px) so the knob row always fits at full text size.
//
//   COMPACT (SPLIT branch cards, ~350 wide, height compactHeight())
//     +-------------------------------------------+
//     | (A) NAME (chrome 18)         [ST|MID|SIDE] |
//     | +---------------+  [ preset name      v ] |
//     | | live screen   |  (g)(P)(S)(L)(x)         |
//     | +---------------+                          |
//     | knobs, one or two rows                     |
//     +-------------------------------------------+
// ------------------------------------------------------------------------------------------------
namespace
{
constexpr float iconRowWidth = 5.0f * iconPill + 4.0f * iconGap;
constexpr float padX = 16.0f;
constexpr float screenGap = 14.0f;   // live screen -> knob row
constexpr float compactTop = 104.0f;   // first knob row of a compact card

bool compact (juce::Rectangle<float> p) { return p.getWidth() < compactBelow; }

float lcdWidth (juce::Rectangle<float> p) { return juce::jlimit (136.0f, 176.0f, p.getWidth() * 0.21f); }

float screenWidth (EffectType t, juce::Rectangle<float> p)
{
    const float avail = p.getWidth() - padX * 2.0f - screenGap - knobRowWidth (t);
    return juce::jlimit (128.0f, 184.0f, avail);
}

bool isLedRow (const ParamSpec& s) { return s.unit == Unit::Choice && s.choices.size() <= 4; }
}

juce::String displayLabel (const ParamSpec* s)
{
    if (s == nullptr) return "DRY/WET";
    const auto n = s->name.toUpperCase();
    // long engine names -> short card labels (the tooltip keeps the full name)
    if (n == "LOW-MID FREQ")  return "LM FREQ";
    if (n == "HIGH-MID FREQ") return "HM FREQ";
    if (n == "LOW-MID")       return "LO-MID";
    if (n == "HIGH-MID")      return "HI-MID";
    if (n == "LFO DEPTH")     return "LFO AMT";
    if (n == "THRESHOLD")     return "THRESH";
    if (n == "PRE-DELAY")     return "PREDELAY";
    return n;
}

juce::String displayChoice (const juce::String& c)
{
    const auto u = c.toUpperCase();
    if (u == "NOISE GATE") return "GATE";
    if (u == "FOLDBACK")   return "FOLD";
    if (u == "RECTIFY")    return "RECT";
    if (u == "PATTERN")    return "PATTRN";
    return u;
}

float controlWidth (const ParamSpec* s)
{
    const auto lf = ek::card::labelFont (ek::card::labelSize, false, labelTracking);
    const float lw = juce::GlyphArrangement::getStringWidth (lf, displayLabel (s)) + 6.0f;
    constexpr float knobCell = 54.0f;   // 38 px face + mod ring room; fits a 7-character 12 px value
    if (s == nullptr) return juce::jmax (knobCell, lw);
    if (s->unit == Unit::Toggle) return juce::jmax (48.0f, lw);
    if (isLedRow (*s))
    {
        const auto cf = ek::card::labelFont (ek::card::labelSize, true, labelTracking);
        float longest = 0.0f;
        for (auto& c : s->choices) longest = juce::jmax (longest, juce::GlyphArrangement::getStringWidth (cf, displayChoice (c)));
        return juce::jmax (lw, longest + 26.0f);
    }
    return juce::jmax (knobCell, lw);   // rotary / stepped knob
}

float knobRowWidth (EffectType t)
{
    if (t == EffectType::None) return 0.0f;
    const auto& info = effectInfo (t);
    float w = controlWidth (nullptr) + mixGap;
    int n = 0;
    for (auto& p : info.params)
        if (p.used) { w += controlWidth (&p); ++n; }
    return w + controlGap * (float) juce::jmax (0, n - 1);
}

int knobRows (EffectType t, float plateWidth)
{
    if (plateWidth >= compactBelow) return 1;
    return knobRowWidth (t) > plateWidth - 24.0f ? 2 : 1;
}

int compactHeight (EffectType t, float plateWidth)
{
    return (int) (compactTop + (float) knobRows (t, plateWidth) * controlRowHeight + 8.0f);
}

juce::Rectangle<float> msArea (juce::Rectangle<float> p)
{
    if (compact (p)) return { p.getRight() - 14.0f - msWidth, p.getY() + 10.0f, msWidth, 22.0f };
    return { presetLcdArea (p).getX() - 8.0f - msWidth, p.getY() + 10.0f, msWidth, 22.0f };
}

juce::Rectangle<float> nameArea (juce::Rectangle<float> p)
{
    if (compact (p))
    {
        const float x = p.getX() + 44.0f;   // room for the branch tag (A) / (B)
        return { x, p.getY() + 7.0f, msArea (p).getX() - 10.0f - x, 28.0f };
    }
    const float x = p.getX() + padX + 2.0f;
    return { x, p.getY() + 6.0f, juce::jmin (320.0f, msArea (p).getX() - 12.0f - x), 30.0f };
}

juce::Rectangle<float> presetLcdArea (juce::Rectangle<float> p)
{
    if (compact (p))
    {
        const auto live = liveArea (EffectType::None, p);
        const float x = live.getRight() + 10.0f;
        return { x, live.getY(), p.getRight() - 14.0f - x, 22.0f };
    }
    const float w = lcdWidth (p);
    return { p.getRight() - 16.0f - iconRowWidth - 10.0f - w, p.getY() + 10.0f, w, 22.0f };
}

juce::Rectangle<float> iconArea (juce::Rectangle<float> p, int index)
{
    float x, y;
    if (compact (p))
    {
        const auto lcd = presetLcdArea (p);
        x = lcd.getX();
        y = lcd.getBottom() + 10.0f;
    }
    else
    {
        x = p.getRight() - 16.0f - iconRowWidth;
        y = p.getY() + 10.0f;
    }
    return { x + (float) index * (iconPill + iconGap), y, iconPill, iconPill };
}

juce::Rectangle<float> liveArea (EffectType t, juce::Rectangle<float> p)
{
    if (compact (p))
    {
        const float w = juce::jlimit (120.0f, 200.0f, p.getWidth() - 28.0f - 10.0f - iconRowWidth);
        return { p.getX() + 14.0f, p.getY() + 42.0f, w, 54.0f };
    }
    return { p.getX() + padX, p.getY() + 44.0f, screenWidth (t, p), 64.0f };
}

juce::Rectangle<float> knobArea (EffectType t, juce::Rectangle<float> p)
{
    if (compact (p))
        return { p.getX() + 12.0f, p.getY() + compactTop, p.getWidth() - 24.0f, p.getHeight() - compactTop - 8.0f };
    const float x = liveArea (t, p).getRight() + screenGap;
    return { x, p.getY() + 40.0f, p.getRight() - padX - x, controlRowHeight };
}

juce::Rectangle<float> statusArea (EffectType t, juce::Rectangle<float> p)
{
    const auto live = liveArea (t, p);
    return live.withSizeKeepingCentre (live.getWidth() - 10.0f, 24.0f);
}

juce::Rectangle<float> miniGlyphArea (juce::Rectangle<float> p)
{
    const float h = juce::jmax (8.0f, p.getHeight() - 10.0f);
    const float w = juce::jlimit (34.0f, 64.0f, juce::jmin (h * 2.3f, p.getWidth() * 0.3f));
    return { p.getRight() - 6.0f - w, p.getY() + 5.0f, w, h };
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
