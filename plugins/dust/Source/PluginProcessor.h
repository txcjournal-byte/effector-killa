#pragma once

#include "DSP/DustEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* age    = "age";
static constexpr const char* tape   = "tape";
static constexpr const char* wobble = "wobble";
static constexpr const char* vinyl  = "vinyl";
static constexpr const char* crush  = "crush";
static constexpr const char* tone   = "tone";
static constexpr const char* width  = "width";
static constexpr const char* mix    = "mix";
static constexpr const char* output = "output";
static constexpr const char* follow = "follow";
} // namespace IDs

class DustProcessor final : public juce::AudioProcessor
{
public:
    DustProcessor();
    ~DustProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // input level for the editor's needle (0..1, peak)
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    dust::Params readParams() const;

    dust::Engine engine;
    int currentProgram = 0;

    std::atomic<float>* pAge = nullptr;
    std::atomic<float>* pTape = nullptr;
    std::atomic<float>* pWobble = nullptr;
    std::atomic<float>* pVinyl = nullptr;
    std::atomic<float>* pCrush = nullptr;
    std::atomic<float>* pTone = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pFollow = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DustProcessor)
};
