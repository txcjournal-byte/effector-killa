#pragma once

// Drawing helpers of the PERFORMER unit (DESIGN-RACK.md section 5) and the black-glass
// preset browser (section 10). Pure functions of their arguments - no state, no processor access.

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::ui::performer
{
// palette of the unit
inline const juce::Colour plateBlue { 0xff0e1626 };   // dark blue anodised metal
inline const juce::Colour iceLine { 0xff9fc3ff };     // ice-blue line / LEDs
inline const juce::Colour iceText { 0xff9fdcff };     // VFD-style text
inline const juce::Colour amber { 0xffffc861 };
inline const juce::Colour vuWarm { 0xfff6e3b4 };      // warm-white backlit VU scale

// macro ring colours: VILLAIN ARC, CRASH OUT, AURA, DRIP, KNOCK
juce::Colour macroColour (int macro) noexcept;

// ---- metalwork --------------------------------------------------------------------------------
// Anodised plate with fine horizontal grain, bevel and an ice-blue hairline just inside the edge.
void drawAnodisedPlate (juce::Graphics&, juce::Rectangle<float> r);
// Rack ear with two oval slots (left or right side of a unit).
void drawRackEar (juce::Graphics&, juce::Rectangle<float> r, bool leftSide);
// Chrome cross-head screw; `rotation` in radians so every screw sits a little differently.
void drawScrew (juce::Graphics&, juce::Point<float> c, float radius, float rotation);
// Engraved vertical divider between sections.
void drawEngravedDivider (juce::Graphics&, float x, float y0, float y1);
// Small silkscreen label (IBM Plex Mono style, spaced).
void drawSilkscreen (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r, juce::Colour c,
                     float height, juce::Justification j = juce::Justification::centred);

// ---- controls ---------------------------------------------------------------------------------
// Big red backlit KILL key. `flash` 0..1 brightens the backlight (after a KILL), `hover` lifts it slightly.
void drawKillButton (juce::Graphics&, juce::Rectangle<float> r, bool pressed, bool hover, float flash);
// Small LED push key (BOOTLEG). `lit` 0..1.
void drawLedKey (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text, juce::Colour led,
                 float lit, bool pressed, bool hover);
// Macro knob with a coloured ring: dim track, bright arc to the modulated value, a small tick at the
// knob's own (unmodulated) value, and a dark knurled cap pointing at the modulated value.
void drawMacroKnob (juce::Graphics&, juce::Rectangle<float> bounds, juce::Colour ring, float baseValue,
                    float shownValue, bool active);

// ---- VU meter ---------------------------------------------------------------------------------
// Analogue VU: bezel, warm-white backlit scale (-20 .. +3 VU, red zone above 0), needle at `vu`
// (dB relative to 0 VU), `name` printed small in the lower left corner of the card. `backlight` 0..1.
void drawVuMeter (juce::Graphics&, juce::Rectangle<float> r, float vu, const juce::String& name, float backlight);

// ---- black glass (preset browser) -------------------------------------------------------------
void drawBlackGlass (juce::Graphics&, juce::Rectangle<float> r, float corner);
// Category tab with a status LED on the left; `selected` lights the LED and the tab edge.
void drawLedTab (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text, juce::Colour led,
                 bool selected, bool hover);
} // namespace ek::ui::performer
