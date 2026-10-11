#pragma once
// TrapWin look of the MOD patch cable overlay (Docs/TRAPWIN-STYLE.md: "modulation cables are thin
// ice-blue glowing curves"). PluginEditor::CableOverlay::paint() calls paintOverlay(); the helpers are
// separate so other views can draw the same cable / target highlight.
#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::ui::cable
{
// thin glowing curve from a MOD jack to the mouse / a knob, coloured by the source
void paintCable (juce::Graphics&, juce::Point<float> from, juce::Point<float> to, juce::Colour colour);
// rounded glow outline around the control under the mouse (the drop target)
void paintTarget (juce::Graphics&, juce::Rectangle<float> control, juce::Colour colour);
// the whole overlay: target highlight (if any) + cable; colour = ek::ui::modSourceColour (source)
void paintOverlay (juce::Graphics&, juce::Colour colour, juce::Point<float> from, juce::Point<float> to,
                   juce::Rectangle<int> target);
} // namespace ek::ui::cable
