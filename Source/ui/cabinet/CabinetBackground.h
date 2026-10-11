#pragma once

// Vector-drawn TrapWin "Chrome" cabinet (replaces the bitmap background): black panels with thin
// ice-blue lines, chrome logo and printed labels. Positions come from Layout.h.
// If Resources/Design/cabinet.png is supplied it is used instead.

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::ui
{
struct CabinetBackground
{
    // Paints the whole cabinet in cabinet space (1630 x 965).
    static void paint (juce::Graphics& g);
    // Chrome "TRAPWIN EFFECTOR" logo (also used by the AV3 screensaver).
    static void paintLogo (juce::Graphics& g, juce::Rectangle<float> r, bool tagline);
    static juce::Image renderLogo (int width, int height);
};
} // namespace ek::ui
