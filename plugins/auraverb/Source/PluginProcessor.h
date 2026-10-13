#pragma once

#include "DSP/AuraEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* mode      = "mode";
static constexpr const char* mix       = "mix";
static constexpr const char* predelay  = "predelay";
static constexpr const char* preSync   = "presync";
static constexpr const char* preDiv    = "prediv";
static constexpr const char* decay     = "decay";
static constexpr const char* size      = "size";
static constexpr const char* earlyLate = "earlylate";
static constexpr const char* attack    = "attack";
static constexpr const char* diffusion = "diffusion";
static constexpr const char* modDepth  = "moddepth";
static constexpr const char* modRate   = "modrate";
static constexpr const char* damping   = "damping";
static constexpr const char* lowCut    = "lowcut";
static constexpr const char* bassMult  = "bassmult";
static constexpr const char* width     = "width";
static constexpr const char* duck      = "duck";
static constexpr const char* freeze    = "freeze";
static constexpr const char* colour    = "colour";
static constexpr const char* output    = "output";
} // namespace IDs

class AuraVerbProcessor final : public juce::AudioProcessor
{
public:
    AuraVerbProcessor();

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
    double getTailLengthSeconds() const override { return 60.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // engine parameters as set by the APVTS (+ host tempo); used by the editor's display too
    auraverb::Params currentParams() const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiIn { 0.0f }, uiWet { 0.0f }, uiDuck { 1.0f };
    std::atomic<double> uiBpm { 120.0 };
    std::atomic<bool> uiHostTempo { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    auraverb::Engine engine;
    int currentProgram = 0;

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pPredelay = nullptr;
    std::atomic<float>* pPreSync = nullptr;
    std::atomic<float>* pPreDiv = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pEarlyLate = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pDiffusion = nullptr;
    std::atomic<float>* pModDepth = nullptr;
    std::atomic<float>* pModRate = nullptr;
    std::atomic<float>* pDamping = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pBassMult = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pDuck = nullptr;
    std::atomic<float>* pFreeze = nullptr;
    std::atomic<float>* pColour = nullptr;
    std::atomic<float>* pOutput = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraVerbProcessor)
};
