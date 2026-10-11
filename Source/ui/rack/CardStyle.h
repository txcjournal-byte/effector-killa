#pragma once
// TrapWin effect-card look (Docs/TRAPWIN-STYLE.md), shared by FaceplatesColor.cpp and FaceplatesSpace.cpp.
// Everything here is flat vector drawing: dark navy card, 1 px ice-blue line, a 2 px accent edge on the
// left, chrome italic title (Archivo Black, sheared), IBM Plex Mono labels and "glowing glass" screens.
// No materials, screws, ears or bevels.
#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::card
{
// tokens (trapwin.com)
inline const juce::Colour bg       { 0xff04060a };
inline const juce::Colour card     { 0xff0a0f18 };
inline const juce::Colour card2    { 0xff0b111c };
inline const juce::Colour screen   { 0xff05080f };
inline const juce::Colour line     { 0x2e9fc3ff };   // rgba(159,195,255,.18)
inline const juce::Colour text     { 0xffe6ecf7 };
inline const juce::Colour text2    { 0xffa9b6cc };
inline const juce::Colour muted    { 0xff8794ab };
inline const juce::Colour ice      { 0xff9fc3ff };
inline const juce::Colour iceDeep  { 0xff4676c4 };   // glow rgba(70,118,196,.5)

constexpr float radius = 20.0f;       // full-size card
constexpr float radiusMini = 10.0f;   // 264 x 41 chip
constexpr float shear = 0.21f;        // fake italic of the display font

juce::Font displayFont (float height);              // Archivo Black (900)
juce::Font monoFont (float height, bool bold = false); // IBM Plex Mono 500 / 700, +1 px tracking

// Card body: inner gradient, faint accent glow from the left, 1 px line, 2 px accent gradient edge.
void paintCard (juce::Graphics&, juce::Rectangle<float> r, juce::Colour accent, bool mini);

// Effect name in chrome italic (#F4F6FB -> #C9D0DD 48 % -> #8E98AB 52 % -> #E6EAF2) with a soft blue
// glow. The text is fitted into r (font shrinks first, then squeezes). Returns the painted bounds.
juce::Rectangle<float> paintChromeTitle (juce::Graphics&, const juce::String& name, juce::Rectangle<float> r,
                                         float maxHeight, float glow = 1.0f);

// "CATEGORY  ✦ TRAPWIN" in muted Plex Mono (the sparkle is drawn as a path, tinted with accent).
void paintCategory (juce::Graphics&, const juce::String& category, juce::Rectangle<float> r, float height,
                    juce::Colour accent);

// four-point TrapWin sparkle ✦
void paintSparkle (juce::Graphics&, juce::Point<float> c, float r, juce::Colour colour, float glow = 0.0f);

// Glass screen: dark inset #05080F, thin line, faint accent tint. paintScreenGlass goes on top of the content.
void paintScreen (juce::Graphics&, juce::Rectangle<float> r, float corner, juce::Colour accent);
void paintScreenGlass (juce::Graphics&, juce::Rectangle<float> r, float corner);

// Small dark pill with a thin line (unit preset display; the text is drawn by RackView).
void paintPill (juce::Graphics&, juce::Rectangle<float> r, juce::Colour accent);

// glow helpers for live vector graphics (layered, no blur -> cheap enough per frame)
void strokeGlow (juce::Graphics&, const juce::Path&, juce::Colour, float width, float glow = 1.0f);
void dotGlow (juce::Graphics&, juce::Point<float> c, float r, juce::Colour, float level);
void glowText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font, juce::Colour,
               juce::Justification, float glow = 1.0f);

// flat TrapWin knob: dark disc, thin line, accent value arc with glow, small white pointer dot
// (angle: 0 = up, clockwise, -135..+135 deg; value 0..1 drives the arc)
void paintKnob (juce::Graphics&, juce::Point<float> c, float r, float value, juce::Colour accent);
} // namespace ek::card
