#pragma once

// TrapWin brand artwork: the official logo (chrome TRAPWIN, sparkle stars, AUDIO · UNDERGROUND)
// from Resources/Brand/trapwin_logo.png, the same file the other TrapWin plugins use.

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::ui::brand
{
const juce::Image& logo();

// Logo on top, chrome product name ("EFFECTOR") under it, optional tagline.
void paintLogoBlock (juce::Graphics& g, juce::Rectangle<float> r, bool tagline);
} // namespace ek::ui::brand
