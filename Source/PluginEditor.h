#pragma once

#include "PluginProcessor.h"

class EffectorKillaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor&);
    ~EffectorKillaAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    EffectorKillaAudioProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessorEditor)
};
