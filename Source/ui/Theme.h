#pragma once

// Palette, fonts and drawing primitives of the 80s VHS living-room cabinet.

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::theme
{
// palette
inline const juce::Colour killRed { 0xffff2e3e };      // only KILL, OFF AIR LED and warnings
inline const juce::Colour amber { 0xffffb238 };
inline const juce::Colour amberDeep { 0xffe07a1f };
inline const juce::Colour fadedOrange { 0xffd9793a };
inline const juce::Colour cream { 0xffe9dcc0 };
inline const juce::Colour creamLabel { 0xffe8d4ae };
inline const juce::Colour ink { 0xff1b1712 };
inline const juce::Colour panelDark { 0xff17100b };
inline const juce::Colour metalLight { 0xffd6d2cb };
inline const juce::Colour metalDark { 0xff4a4744 };
inline const juce::Colour vfdGreen { 0xff38ff7a };
inline const juce::Colour osdWhite { 0xfff4f1ea };
inline const juce::Colour ledGreen { 0xff3dff6e };

// fonts (built-in placeholders until the licensed OSD / VFD / handwritten fonts arrive)
juce::Font osdFont (float height);          // blocky VCR on-screen display
juce::Font vfdFont (float height);          // video recorder display
juce::Font handFont (float height);         // marker on cream labels
juce::Font stencilFont (float height);      // printed cabinet labels

// drawing primitives
void drawKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle, bool hot = false);
void drawChickenHeadKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle);
void drawLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool on, float glow = 1.0f);
void drawSocket (juce::Graphics& g, juce::Rectangle<float> r, float corner);
void drawLitButton (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, bool lit, bool pressed,
                    const juce::String& text, float fontHeight);
void drawTrack (juce::Graphics& g, juce::Rectangle<float> r);
void drawSliderThumb (juce::Graphics& g, juce::Rectangle<float> r, bool lit = false);
void drawGlowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Font f,
                   juce::Colour c, juce::Justification j, float glow);
void drawPanelGloss (juce::Graphics& g, juce::Rectangle<float> r, float corner);

// angle convention for rotary knobs: 0 = straight up, clockwise positive (radians)
constexpr float knobStart = -2.35619449f; // -135 deg
constexpr float knobEnd = 2.35619449f;    // +135 deg
} // namespace ek::theme
