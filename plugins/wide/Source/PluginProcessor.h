#pragma once

#include "DSP/WideEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* lowX      = "lowx";
static constexpr const char* highX     = "highx";
static constexpr const char* widthLow  = "wlow";
static constexpr const char* widthMid  = "wmid";
static constexpr const char* widthHigh = "whigh";
static constexpr const char* monoBelow = "monobelow";
static constexpr const char* haas      = "haas";
static constexpr const char* haasSide  = "haasside";
static constexpr const char* balance   = "balance";
static constexpr const char* stereoize = "stereoize";
static constexpr const char* output    = "output";
} // namespace IDs

class WideProcessor final : public juce::AudioProcessor
{
public:
    WideProcessor();

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
    double getTailLengthSeconds() const override { return 0.04; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    widefx::Params currentParams() const;
    const widefx::Engine& getEngine() const { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiCorr { 1.0f };
    std::array<std::atomic<float>, 2> uiPeak {};
    std::array<std::atomic<float>, 3> uiBand {};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    widefx::Engine engine;
    int currentProgram = 0;
    std::atomic<float>* pLowX = nullptr;
    std::atomic<float>* pHighX = nullptr;
    std::atomic<float>* pWLow = nullptr;
    std::atomic<float>* pWMid = nullptr;
    std::atomic<float>* pWHigh = nullptr;
    std::atomic<float>* pMono = nullptr;
    std::atomic<float>* pHaas = nullptr;
    std::atomic<float>* pHaasSide = nullptr;
    std::atomic<float>* pBalance = nullptr;
    std::atomic<float>* pStereoize = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WideProcessor)
};
