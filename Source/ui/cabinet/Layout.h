#pragma once

// All positions of the cabinet UI in ONE place.
// Coordinates are in "cabinet space" = pixels of Resources/Design/reference.png (1630 x 965).
// The editor scales cabinet space to 1100 x 650 (x 1.0 / 1.25 / 1.5).

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::layout
{
using R = juce::Rectangle<float>;
using P = juce::Point<float>;

constexpr float kWidth = 1630.0f;
constexpr float kHeight = 965.0f;
constexpr int kEditorWidth = 1100;
constexpr int kEditorHeight = 650;

// ---- left panel ---------------------------------------------------------------------------
inline const R logo { 40.0f, 60.0f, 330.0f, 195.0f };
inline const R killBezel { 104.0f, 279.0f, 193.0f, 159.0f };
inline const R killFace { 117.0f, 292.0f, 166.0f, 131.0f };
inline const R ratingPanel { 38.0f, 452.0f, 334.0f, 108.0f };
inline const P ratingLed[3] { { 102.0f, 500.0f }, { 203.0f, 500.0f }, { 303.0f, 500.0f } };
inline const R ratingTrack { 76.0f, 513.0f, 255.0f, 31.0f };
inline const R bootleg { 52.0f, 578.0f, 90.0f, 89.0f };
inline const R pocketTv { 159.0f, 578.0f, 90.0f, 89.0f };
inline const R autoTracking { 264.0f, 578.0f, 92.0f, 89.0f };
inline const R cableTrack { 143.0f, 702.0f, 114.0f, 45.0f };
inline const P premiumLed { 277.0f, 735.0f };
inline const R staffPick { 60.0f, 788.0f, 70.0f, 62.0f };
inline const P antennaIn { 213.0f, 826.0f };
inline const P rfOut { 317.0f, 826.0f };
constexpr float smallKnobRadius = 32.0f;

// ---- TV -----------------------------------------------------------------------------------
inline const R tvScreen { 446.0f, 68.0f, 843.0f, 471.0f };
constexpr float tvCorner = 34.0f;
inline const R avTrack { 612.0f, 598.0f, 293.0f, 35.0f };
inline const float avPositions[3] { 646.0f, 740.0f, 868.0f };
inline const R avLabels[3] { { 596.0f, 578.0f, 100.0f, 22.0f }, { 705.0f, 578.0f, 110.0f, 22.0f }, { 822.0f, 578.0f, 130.0f, 22.0f } };
inline const P offAirLed { 1115.0f, 608.0f };
inline const R offAirButton { 1149.0f, 582.0f, 138.0f, 51.0f };

// ---- right column --------------------------------------------------------------------------
inline const P channelKnob { 1474.0f, 131.0f };
constexpr float channelKnobRadius = 60.0f;
constexpr float channelNumberRadius = 94.0f;
inline const R programUp { 1357.0f, 236.0f, 62.0f, 36.0f };
inline const R programDown { 1528.0f, 236.0f, 62.0f, 36.0f };
inline const R programLabel { 1420.0f, 236.0f, 107.0f, 36.0f };
inline const P macroKnob[5] { { 1403.0f, 313.0f }, { 1403.0f, 391.0f }, { 1403.0f, 466.0f }, { 1403.0f, 541.0f }, { 1403.0f, 616.0f } };
constexpr float macroKnobRadius = 30.0f;
inline const R macroLabel[5] { { 1449.0f, 290.0f, 138.0f, 48.0f }, { 1449.0f, 366.0f, 138.0f, 48.0f }, { 1449.0f, 441.0f, 138.0f, 48.0f },
                               { 1449.0f, 516.0f, 138.0f, 48.0f }, { 1449.0f, 591.0f, 138.0f, 48.0f } };

// ---- shelf with 8 cassettes ------------------------------------------------------------------
inline const R shelf { 412.0f, 670.0f, 604.0f, 236.0f };
constexpr float cassetteX0 = 437.0f;
constexpr float cassetteStep = 72.3f;
constexpr float cassetteW = 50.0f;
constexpr float cassetteY = 685.0f;
constexpr float cassetteH = 207.0f;
inline R cassette (int i) { return { cassetteX0 + cassetteStep * (float) i, cassetteY, cassetteW, cassetteH }; }

// ---- video recorder ----------------------------------------------------------------------------
inline const R vcrBody { 1036.0f, 700.0f, 552.0f, 200.0f };
inline const R vcrDisplay { 1073.0f, 732.0f, 478.0f, 62.0f };
inline const R vcrText { 1139.0f, 738.0f, 331.0f, 52.0f };
inline const R rec { 1061.0f, 826.0f, 68.0f, 62.0f };
inline const R rewind { 1139.0f, 826.0f, 90.0f, 62.0f };
inline const R fastFwd { 1239.0f, 826.0f, 93.0f, 62.0f };
inline const R eject { 1348.0f, 826.0f, 91.0f, 62.0f };
inline const R sideSwitch { 1470.0f, 851.0f, 97.0f, 28.0f };
inline const R sideLabels { 1455.0f, 826.0f, 125.0f, 22.0f };
} // namespace ek::layout
