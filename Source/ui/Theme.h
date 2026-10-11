#pragma once

// Palette, fonts and drawing primitives of the 80s VHS living-room cabinet.

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::theme
{
// palette
inline const juce::Colour killRed { 0xffff2e3e };      // only KILL, OFF AIR LED and warnings
// TrapWin "Chrome" brand (trapwin.com): near-black, chrome silver, ice-blue glow
inline const juce::Colour accent { 0xff9fc3ff };       // ice blue (LEDs, highlights, glow)
inline const juce::Colour accentDeep { 0xff4676c4 };   // deep glow
inline const juce::Colour accentSoft { 0xff7fa6e8 };
inline const juce::Colour silver { 0xffe6ecf7 };       // printed labels / text
inline const juce::Colour silverDim { 0xffa9b6cc };
inline const juce::Colour labelPaper { 0xffdde3ee };   // cassette label paper
inline const juce::Colour ink { 0xff05070b };
inline const juce::Colour bg { 0xff04060a };
inline const juce::Colour panel { 0xff0a0f18 };
inline const juce::Colour panel2 { 0xff0b111c };
inline const juce::Colour line { 0x2e9fc3ff };         // rgba(159,195,255,.18)
inline const juce::Colour vfd { 0xff9fdcff };          // video display
inline const juce::Colour osdWhite { 0xffeef3fb };
inline const juce::Colour ledGreen { 0xff3dff6e };

// fonts (built-in placeholders until the licensed OSD / VFD / handwritten fonts arrive)
juce::Font osdFont (float height);          // blocky VCR on-screen display
juce::Font vfdFont (float height);          // video recorder display
juce::Font handFont (float height);         // marker on cream labels
juce::Font stencilFont (float height);      // printed cabinet labels (Archivo Black)
juce::Font monoLabel (float height);        // small spaced labels (IBM Plex Mono)
void drawChromeText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Font f,
                     juce::Justification j, float glow = 1.0f);
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner);

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
