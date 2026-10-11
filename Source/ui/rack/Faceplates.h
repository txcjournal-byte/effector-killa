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

// Layout of a full-size faceplate (898 x 140 at 1x). Knob components are placed by RackView
// inside knobArea(); the live element is drawn by paintFaceplate inside liveArea().
juce::Rectangle<float> nameArea (juce::Rectangle<float> plate);
juce::Rectangle<float> liveArea (EffectType, juce::Rectangle<float> plate);
juce::Rectangle<float> presetLcdArea (juce::Rectangle<float> plate);
juce::Rectangle<float> knobArea (EffectType, juce::Rectangle<float> plate);

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
