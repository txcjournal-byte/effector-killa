#pragma once

#include "DSP/HaloEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* predelay  = "predelay";
static constexpr const char* size      = "size";
static constexpr const char* decay     = "decay";
static constexpr const char* diffusion = "diffusion";
static constexpr const char* mod       = "mod";
static constexpr const char* damping   = "damping";   // hi-cut
static constexpr const char* lowcut    = "lowcut";
static constexpr const char* width     = "width";
static constexpr const char* duck      = "duck";
static constexpr const char* freeze    = "freeze";
static constexpr const char* mix       = "mix";
static constexpr const char* output    = "output";
} // namespace IDs

class HaloProcessor final : public juce::AudioProcessor
{
public:
    HaloProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiIn { 0.0f }, uiWet { 0.0f }, uiDuck { 1.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    halo::Params readParams() const;

    halo::Engine engine;
    int currentProgram = 0;

    std::atomic<float>* pPredelay = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pDiffusion = nullptr;
    std::atomic<float>* pMod = nullptr;
    std::atomic<float>* pDamping = nullptr;
    std::atomic<float>* pLowcut = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pDuck = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaloProcessor)
};
