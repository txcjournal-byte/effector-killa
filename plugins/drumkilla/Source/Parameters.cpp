#include "Parameters.h"

using namespace juce;

namespace
{
    String dbText (float v)  { return (v > 0.04f ? "+" : "") + String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB"; }
    String hzText (float v)  { return String (roundToInt (v)) + " Hz"; }
    String msText (float v)  { return String (roundToInt (v)) + " ms"; }
    String pctText (float v) { return String (roundToInt (v)) + " %"; }

    auto floatParam (const char* id, const char* name, NormalisableRange<float> range, float def, String (*text) (float))
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, def,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([text] (float v, int) { return text (v); })
                .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); }));
    }
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout layout;

    NormalisableRange<float> crossover (80.0f, 200.0f);
    crossover.setSkewForCentre (120.0f);

    layout.add (floatParam (inputGain, "Input Gain", { -12.0f, 12.0f, 0.1f }, 0.0f, dbText));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { phaseInvert, 1 }, "Invert Phase", false));
    layout.add (floatParam (crossoverFreq, "Crossover", crossover, 120.0f, hzText));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { subMono, 1 }, "Sub Mono", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { subCut, 1 }, "28Hz Cut", true));
    layout.add (floatParam (driveAmount, "Drive", { 0.0f, 100.0f, 0.1f }, 25.0f, pctText));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { satMode, 1 }, "Saturation Type", Choices::satModes, 1));
    layout.add (floatParam (midFocus, "Mid Presence", { 0.0f, 12.0f, 0.1f }, 3.0f, dbText));
    layout.add (floatParam (duckDepth, "Kick Ducking", { 0.0f, 100.0f, 0.1f }, 50.0f, pctText));
    layout.add (floatParam (duckRelease, "Duck Release", { 10.0f, 150.0f, 1.0f }, 45.0f, msText));
    layout.add (floatParam (clipDrive, "Clip Drive", { 0.0f, 18.0f, 0.1f }, 4.0f, dbText));
    layout.add (floatParam (clipKnee, "Clip Knee", { 0.5f, 0.95f, 0.01f }, 0.8f,
                            [] (float v) { return String (v, 2); }));
    layout.add (floatParam (clipCeiling, "Ceiling", { -6.0f, 0.0f, 0.1f }, -0.2f,
                            [] (float v) { return String (v, 1) + " dBFS"; }));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { phonePreview, 1 }, "Phone Check", false));
    layout.add (floatParam (outputGain, "Output Trim", { -18.0f, 6.0f, 0.1f }, 0.0f, dbText));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));
    layout.add (floatParam (hit, "Hit", { 0.0f, 100.0f, 0.1f }, 70.0f, pctText));
    NormalisableRange<float> focusRange (60.0f, 8000.0f);
    focusRange.setSkewForCentre (600.0f);
    layout.add (floatParam (focusFreq, "Focus Freq", focusRange, 550.0f, hzText));
    return layout;
}
