#pragma once

#include "DSP/ChopperEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* rate   = "rate";
static constexpr const char* size   = "size";
static constexpr const char* swing  = "swing";
static constexpr const char* mix    = "mix";
static constexpr const char* output = "output";
static constexpr const char* hold   = "hold";
} // namespace IDs

class ChopperProcessor final : public juce::AudioProcessor
{
public:
    ChopperProcessor();

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

    // ---- step pattern (lock-free: the editor writes, the audio thread reads)
    chopper::Fx getStep (int i) const;
    void setStep (int i, chopper::Fx fx);          // message thread
    chopper::Pattern getPattern() const;
    void setPattern (const chopper::Pattern&);     // message thread
    std::atomic<int> patternVersion { 0 };        // bumps on every pattern change (editor repaint)

    juce::AudioProcessorValueTreeState apvts;

    // UI feedback
    std::atomic<int> uiStep { 0 }, uiFx { 0 };
    std::atomic<double> uiPhase { 0.0 }, uiBpm { 120.0 };
    std::atomic<bool> uiHostPlaying { false };
    std::atomic<float> uiLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    chopper::Engine engine;
    std::array<std::atomic<uint8_t>, chopper::numSteps> steps {};
    int currentProgram = 0;

    std::atomic<float>* pRate = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pHold = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopperProcessor)
};
