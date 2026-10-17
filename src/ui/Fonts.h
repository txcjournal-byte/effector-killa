/*
    TrapWin BEND DEMON
    Fonts.h - embedded OFL fonts: Unbounded (product name only) and Inter (every label, button
    and value - 2.0 switched from JetBrains Mono to Inter for readability).
*/
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace bd::fonts
{
    /** Wide geometric display face (Unbounded). weight: 0 = Medium, 1 = Bold, 2 = Black. */
    juce::Font display (float height, int weight = 1);

    /** UI text: labels, buttons, values (Inter Medium / SemiBold). Kept under the old name. */
    juce::Font mono (float height, bool bold = false);
    inline juce::Font ui (float height, bool bold = false) { return mono (height, bold); }
}
