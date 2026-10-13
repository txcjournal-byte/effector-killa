#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs are part of saved projects and presets:
// once released they must never be renamed or removed, only added.
namespace ParamIDs
{
    inline constexpr auto inputGain     = "input_gain";
    inline constexpr auto phaseInvert   = "phase_invert";
    inline constexpr auto crossoverFreq = "crossover_freq";
    inline constexpr auto subMono       = "sub_mono";
    inline constexpr auto subCut        = "sub_cut";
    inline constexpr auto driveAmount   = "drive_amount";
    inline constexpr auto satMode       = "sat_mode";
    inline constexpr auto midFocus      = "mid_focus";
    inline constexpr auto duckDepth     = "duck_depth";
    inline constexpr auto duckRelease   = "duck_release";
    inline constexpr auto clipDrive     = "clip_drive";
    inline constexpr auto clipKnee      = "clip_knee";
    inline constexpr auto clipCeiling   = "clip_ceiling";
    inline constexpr auto phonePreview  = "phone_preview";
    inline constexpr auto outputGain    = "output_gain";
    inline constexpr auto bypass        = "bypass";
    inline constexpr auto hit           = "hit";
    inline constexpr auto focusFreq     = "focus_freq";
}

namespace Choices
{
    const juce::StringArray satModes { "Tape", "Tube", "Foldback" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
