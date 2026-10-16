/*
    TrapWin BEND DEMON
    Fonts.h - embedded OFL fonts (Unbounded for display, JetBrains Mono for values/labels).
*/
#pragma once

#include <juce_graphics/juce_graphics.h>

namespace bd::fonts
{
    /** Wide geometric display face (Unbounded). weight: 0 = Medium, 1 = Bold, 2 = Black. */
    juce::Font display (float height, int weight = 1);

    /** Monospace for numbers, labels, status (JetBrains Mono). */
    juce::Font mono (float height, bool bold = false);
}
