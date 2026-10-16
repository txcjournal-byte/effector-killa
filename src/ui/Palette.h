/*
    TrapWin BEND DEMON
    Palette.h - the "living light" colour system.
*/
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace bd::pal
{
    // ground
    inline const juce::Colour ground0    { 0xff04050c };   // near-black navy (edges)
    inline const juce::Colour ground1    { 0xff0a1030 };   // radial centre
    inline const juce::Colour panel      { 0xff070a1c };
    inline const juce::Colour hairline   { 0x22a3aecb };

    // light
    inline const juce::Colour cyan       { 0xff46f2ff };
    inline const juce::Colour violet     { 0xffb07cff };
    inline const juce::Colour magenta    { 0xffff4fd8 };
    inline const juce::Colour lime       { 0xffd8ff3d };
    inline const juce::Colour amber      { 0xffffb547 };

    // text
    inline const juce::Colour text       { 0xffeaf2ff };
    inline const juce::Colour textDim    { 0xffa3aecb };
    inline const juce::Colour textFaint  { 0x66a3aecb };

    /** Position 0..1 along the cyan -> violet -> magenta ribbon gradient. */
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
}
