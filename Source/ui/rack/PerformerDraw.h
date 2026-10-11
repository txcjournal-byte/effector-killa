#pragma once

// Small drawing helpers shared by the MASTER column and the preset browser (TrapWin look,
// Docs/TRAPWIN-STYLE.md). Pure functions of their arguments - no state, no processor access.
// (The rack-era metalwork - anodised plates, rack ears, screws, VU meters - was removed.)

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::ui::performer
{
// palette
inline const juce::Colour plateBlue { 0xff0a0f18 };   // card
inline const juce::Colour iceLine { 0xff9fc3ff };     // ice-blue line / active
inline const juce::Colour iceText { 0xffe6ecf7 };     // text
inline const juce::Colour amber { 0xffffc861 };

// macro colours: VILLAIN ARC #B07CFF, CRASH OUT #FF4D5E, AURA #3FD0E0, DRIP #7CFC6B, KNOCK #FF8A3D
juce::Colour macroColour (int macro) noexcept;

// Small spaced label (IBM Plex Mono).
void drawSilkscreen (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r, juce::Colour c,
                     float height, juce::Justification j = juce::Justification::centred);

// ---- preset browser ---------------------------------------------------------------------------
// Dark TrapWin card surface (name kept from the rack era).
void drawBlackGlass (juce::Graphics&, juce::Rectangle<float> r, float corner);
// Category tab: pill with a status dot; `selected` lights the dot and the outline.
void drawLedTab (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text, juce::Colour led,
                 bool selected, bool hover);
} // namespace ek::ui::performer
