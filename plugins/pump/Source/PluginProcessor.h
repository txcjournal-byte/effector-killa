#pragma once

#include "DSP/PumpEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* rate      = "rate";
static constexpr const char* trigger   = "trigger";
static constexpr const char* band      = "band";
static constexpr const char* crossover = "crossover";
static constexpr const char* depth     = "depth";
static constexpr const char* smooth    = "smooth";
static constexpr const char* threshold = "threshold";
static constexpr const char* mix       = "mix";
static constexpr const char* output    = "output";
} // namespace IDs

class PumpProcessor final : public juce::AudioProcessor
{
public:
    PumpProcessor();

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
    double getTailLengthSeconds() const override { return 0.0; }

    // Programs = factory curve shapes
    int getNumPrograms() override { return pump::Curve::numPresets(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return pump::Curve::presetName (index); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Curve editing from the UI (message thread). The audio thread picks it up lock-free-ish.
    std::vector<pump::Point> getCurvePoints() const;
    void setCurvePoints (std::vector<pump::Point> pts);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiPhase { 0.0f }, uiGain { 1.0f };
    std::atomic<int> uiTriggerCount { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    pump::Engine engine;
    int currentProgram = 0;

    mutable juce::SpinLock curveLock;
    std::vector<pump::Point> curvePoints;
    std::atomic<bool> curveDirty { true };

    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pTrigger = nullptr;
    std::atomic<float>* pBand = nullptr;
    std::atomic<float>* pCrossover = nullptr;
    std::atomic<float>* pDepth = nullptr;
    std::atomic<float>* pSmooth = nullptr;
    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PumpProcessor)
};
