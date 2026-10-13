#pragma once

#include "DSP/SnapEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* attack    = "attack";
static constexpr const char* sustain   = "sustain";
static constexpr const char* band      = "band";
static constexpr const char* split     = "split";
static constexpr const char* speed     = "speed";
static constexpr const char* lookahead = "lookahead";
static constexpr const char* clip      = "clip";
static constexpr const char* ceiling   = "ceiling";
static constexpr const char* mix       = "mix";
static constexpr const char* output    = "output";
} // namespace IDs

class SnapProcessor final : public juce::AudioProcessor
{
public:
    SnapProcessor();

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

    snapfx::Params currentParams() const;
    const snapfx::Engine& getEngine() const { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiGainDb { 0.0f }, uiClip { 0.0f }, uiInput { 0.0f }, uiOutput { 0.0f };
    std::atomic<int> uiLatency { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateLatency();

    snapfx::Engine engine;
    int currentProgram = 0;
    bool lastLookahead = false;
    double lastSampleRate = 48000.0;

    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pSustain = nullptr;
    std::atomic<float>* pBand = nullptr;
    std::atomic<float>* pSplit = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pLookahead = nullptr;
    std::atomic<float>* pClip = nullptr;
    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapProcessor)
};
