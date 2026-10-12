#pragma once
// Shared contract for the hardware faceplates of the rack units (see DESIGN-RACK.md section 6).
// RackView paints a unit as: paintFaceplate() (material, screws, ears, name, live element)
// and then places the real knob/switch components on top. GearList calls the same function
// with miniature = true to render the gear thumbnails.
//
// Drawing per effect lives in two files so it can be worked on in parallel:
//   FaceplatesColor.cpp  – CassettePlug, Menace, Brainrot, ThroughTheWall, ToneUp, Squeeze, Overcooked, Slap
//   FaceplatesSpace.cpp  – Doubles, Swirl, AdLibThrow, AuraRoom, WideBody, Chopped, RedLine
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "../../engine/Effect.h"

namespace ek::faceplate
{
// Everything a live element may need. All values are UI-thread snapshots (no audio-thread access here).
struct State
{
    std::array<float, 8> params {};  // P1..P8 normalised 0..1 (current, incl. modulation)
    float mix = 1.0f;                // slot DRY/WET 0..1
    bool powered = true;             // POWER (false = dim to ~60 %)
    bool soloDimmed = false;         // another unit is soloed
    bool locked = false;
    float inLevel = 0.0f;            // slot input / output level, linear 0..1+ (0 when unknown)
    float outLevel = 0.0f;
    float gainReductionDb = 0.0f;    // dynamics units (>= 0)
    std::array<float, 3> bands {};   // Overcooked LOW/MID/HIGH activity 0..1
    bool transientHit = false;       // Slap
    const float* scopeL = nullptr;   // Wide Body vectorscope (may be null)
    const float* scopeR = nullptr;
    int scopeN = 0;
    double beats = 0.0;              // transport / free-running beat position (for tempo animations)
    double bpm = 140.0;
    double timeSec = 0.0;            // monotonically increasing animation clock
    bool reduceMotion = false;
};

// Card layout (TrapWin CHAIN view, UX pass 10/2026 - readable at 100 %). Two variants by width:
//   FULL    (width >= compactBelow): height fullHeight (120). Name top-left, live screen under it,
//           knob row on the right, pills (ST/MID/SIDE, preset, 5 icon pills) top-right.
//   COMPACT (SPLIT branch columns, ~350 wide): name + ST/MID/SIDE on row 1, screen | preset pill +
//           icon pills on row 2, knobs in one or two rows below (height from compactHeight()).
// Knob components are placed by RackView inside knobArea(); the live element is drawn by
// paintFaceplate inside liveArea(). All button positions come from here so cards and controls agree.
constexpr float compactBelow = 600.0f;
constexpr int fullHeight = 120;
constexpr float iconPill = 22.0f, iconGap = 5.0f, msWidth = 104.0f;
constexpr float controlRowHeight = 74.0f;     // label 14 + knob face 44 + value 16
constexpr float controlGap = 4.0f, mixGap = 14.0f;
constexpr float labelTracking = 0.3f, valueTracking = 0.2f;   // px, Plex Mono labels 11 px / values 12 px

juce::Rectangle<float> nameArea (juce::Rectangle<float> plate);
juce::Rectangle<float> liveArea (EffectType, juce::Rectangle<float> plate);
juce::Rectangle<float> presetLcdArea (juce::Rectangle<float> plate);
juce::Rectangle<float> msArea (juce::Rectangle<float> plate);
juce::Rectangle<float> iconArea (juce::Rectangle<float> plate, int index);   // 0 menu, 1 power, 2 solo, 3 lock, 4 remove
juce::Rectangle<float> knobArea (EffectType, juce::Rectangle<float> plate);
juce::Rectangle<float> statusArea (EffectType, juce::Rectangle<float> plate);  // POWER OFF / SOLO / audition pill
juce::Rectangle<float> miniGlyphArea (juce::Rectangle<float> plate);          // live glyph of a library miniature

// Control row metrics shared with ek::ui::RackControl.
juce::String displayLabel (const ParamSpec* spec);        // short UI label (nullptr = unit DRY/WET)
juce::String displayChoice (const juce::String& choice);  // short choice text for segmented pills
float controlWidth (const ParamSpec* spec);               // preferred width of one control (nullptr = DRY/WET)
float knobRowWidth (EffectType);                          // all used params + DRY/WET with minimum gaps
int knobRows (EffectType, float plateWidth);              // 1, or 2 on narrow compact cards
int compactHeight (EffectType, float plateWidth);         // card height of the compact variant

// Base colours so RackView can tint knobs/labels to match the panel.
struct Palette { juce::Colour plate, plateDark, ink, accent, led; };
Palette palette (EffectType);

void paintFaceplate (juce::Graphics&, EffectType, juce::Rectangle<float> plate, const State&, bool miniature);

// Implemented per group (called by paintFaceplate after the common frame is drawn).
void paintColorGroup (juce::Graphics&, EffectType, juce::Rectangle<float> plate, const State&, bool miniature);
void paintSpaceGroup (juce::Graphics&, EffectType, juce::Rectangle<float> plate, const State&, bool miniature);
Palette paletteColorGroup (EffectType);
Palette paletteSpaceGroup (EffectType);
} // namespace ek::faceplate
