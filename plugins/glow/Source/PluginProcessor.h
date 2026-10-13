#pragma once

#include "DSP/GlowEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* mode     = "mode";
static constexpr const char* glow     = "glow";
static constexpr const char* contrast = "contrast";
static constexpr const char* colour   = "colour";
static constexpr const char* output   = "output";
static constexpr const char* autoGain = "autogain";
} // namespace IDs

class GlowProcessor final : public juce::AudioProcessor
{
public:
    GlowProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    glowfx::Params currentParams() const;

    juce::AudioProcessorValueTreeState apvts;

    // UI meters (peak-hold style, written by the audio thread)
    std::atomic<float> uiIn { 0.0f }, uiOut { 0.0f }, uiGr { 0.0f }, uiAg { 0.0f }, uiHarm { 0.0f };
    std::array<std::atomic<float>, 3> uiLift {};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    glowfx::Engine engine;
    int currentProgram = 0;

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pGlow = nullptr;
    std::atomic<float>* pContrast = nullptr;
    std::atomic<float>* pColour = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pAutoGain = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlowProcessor)
};
