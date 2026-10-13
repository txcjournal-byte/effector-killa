#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"
#include "DSP/Engine.h"

class K808Processor : public juce::AudioProcessor
{
public:
    K808Processor();
    ~K808Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    EngineParams readParams() const;

    juce::UndoManager undoManager { 3000, 20 };   // bounded: automation also lands here, keep long sessions light
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    Engine engine;

private:
    // cached raw parameter pointers (lookups by name would allocate on the audio thread)
    struct Raw
    {
        std::atomic<float>* inputGain, *phaseInvert, *crossover, *subMono, *subCut, *drive, *satMode, *focus, *focusFreq,
                          *duckDepth, *duckRelease, *clipDrive, *clipKnee, *ceiling, *phone, *outputGain, *bypass, *hit;
    } raw {};
    juce::AudioProcessorParameter* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (K808Processor)
};
