#pragma once

#include "DSP/VoxEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* pitch   = "pitch";
static constexpr const char* formant = "formant";
static constexpr const char* tune    = "tune";
static constexpr const char* key     = "key";
static constexpr const char* scale   = "scale";
static constexpr const char* retune  = "retune";
static constexpr const char* midi    = "midi";
static constexpr const char* mix     = "mix";
static constexpr const char* output  = "output";
} // namespace IDs

class VoxProcessor final : public juce::AudioProcessor
{
public:
    VoxProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiDetected { 0.0f }, uiTarget { 0.0f }, uiLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    vox::Engine engine;
    int currentProgram = 0;
    std::vector<int> heldNotes;

    std::atomic<float>* pPitch = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pRetune = nullptr;
    std::atomic<float>* pMidi = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxProcessor)
};
