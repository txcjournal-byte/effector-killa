/*
    TrapWin BEND DEMON
    Palette.h - 2.0 colour system: calm graphite surfaces, one TrapWin accent gradient
    (cyan -> violet -> magenta) reserved for lines, selection and active states.
*/
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace bd::pal
{
    // surfaces (darkest -> lightest)
    inline const juce::Colour ground0    { 0xff0b0d12 };   // window / canvas background
    inline const juce::Colour ground1    { 0xff11141a };   // panels
    inline const juce::Colour panel      { 0xff171b23 };   // controls
    inline const juce::Colour raised     { 0xff1e232d };   // hover / raised controls
    inline const juce::Colour hairline   { 0xff2a303c };   // 1 px borders, separators
    inline const juce::Colour gridLine   { 0xff1a1e27 };   // canvas grid
    inline const juce::Colour gridStrong { 0xff2b3140 };   // bar lines

    // accent (TrapWin): used for lines, selection and "on" states only
    inline const juce::Colour cyan       { 0xff3ad7f2 };
    inline const juce::Colour violet     { 0xff8b7bff };
    inline const juce::Colour magenta    { 0xffe0479e };

    // semantic
    inline const juce::Colour lime       { 0xff6fe0c4 };   // "on" / onset (soft mint, not neon)
    inline const juce::Colour amber      { 0xfff0b25a };   // curve points, warnings
    inline const juce::Colour record     { 0xffe5484d };

    // text
    inline const juce::Colour text       { 0xffe4e7ee };
    inline const juce::Colour textDim    { 0xff8a92a3 };
    inline const juce::Colour textFaint  { 0xff555d6d };

    /** Position 0..1 along the cyan -> violet -> magenta accent gradient. */
    inline juce::Colour ribbon (float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return t < 0.5f ? cyan.interpolatedWith (violet, t * 2.0f)
                        : violet.interpolatedWith (magenta, (t - 0.5f) * 2.0f);
    }

    inline juce::ColourGradient ribbonGradient (juce::Point<float> a, juce::Point<float> b)
    {
        juce::ColourGradient g (cyan, a, magenta, b, false);
        g.addColour (0.5, violet);
        return g;
    }

    /** Standard metrics at 100 % (1040 px wide editor); multiply by the editor scale. */
    namespace metric
    {
        inline constexpr float gap      = 8.0f;
        inline constexpr float gapSmall = 4.0f;
        inline constexpr float control  = 24.0f;   // control height
        inline constexpr float corner   = 4.0f;
        inline constexpr float font     = 12.0f;
        inline constexpr float fontSmall = 11.0f;
    }
}
