#pragma once

#include "DSP/GhostEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* mode       = "mode";
static constexpr const char* sync       = "sync";
static constexpr const char* division   = "division";
static constexpr const char* time       = "time";
static constexpr const char* lrRatio    = "lrratio";
static constexpr const char* feedback   = "feedback";
static constexpr const char* lowCut     = "lowcut";
static constexpr const char* highCut    = "highcut";
static constexpr const char* modDepth   = "moddepth";
static constexpr const char* modRate    = "modrate";
static constexpr const char* age        = "age";
static constexpr const char* duck       = "duck";
static constexpr const char* freeze     = "freeze";
static constexpr const char* ghostPitch = "ghostpitch";
static constexpr const char* pattern    = "pattern";
static constexpr const char* mix        = "mix";
static constexpr const char* output     = "output";
} // namespace IDs

class GhostDelayProcessor final : public juce::AudioProcessor
{
public:
    GhostDelayProcessor();

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
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // current engine parameters as set by the APVTS (+ host tempo); used by the editor's display
    ghostdelay::Params currentParams() const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiWet { 0.0f }, uiInput { 0.0f }, uiDuck { 1.0f };
    std::atomic<double> uiBpm { 120.0 }, uiDelayMs { 0.0 }, uiRevPhase { 0.0 };
    std::atomic<bool> uiHostTempo { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ghostdelay::Engine engine;
    int currentProgram = 0;

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pSync = nullptr;
    std::atomic<float>* pDivision = nullptr;
    std::atomic<float>* pTime = nullptr;
    std::atomic<float>* pLrRatio = nullptr;
    std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pHighCut = nullptr;
    std::atomic<float>* pModDepth = nullptr;
    std::atomic<float>* pModRate = nullptr;
    std::atomic<float>* pAge = nullptr;
    std::atomic<float>* pDuck = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pGhostPitch = nullptr;
    std::atomic<float>* pPattern = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    double hostPpq = -1.0;
    bool hostPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GhostDelayProcessor)
};
