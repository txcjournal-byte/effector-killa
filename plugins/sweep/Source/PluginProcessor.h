#pragma once

#include "DSP/SweepEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* aType     = "atype";
static constexpr const char* aCutoff   = "acutoff";
static constexpr const char* aReso     = "areso";
static constexpr const char* aDrive    = "adrive";
static constexpr const char* aMod      = "amod";
static constexpr const char* bOn       = "bon";
static constexpr const char* bType     = "btype";
static constexpr const char* bCutoff   = "bcutoff";
static constexpr const char* bReso     = "breso";
static constexpr const char* bDrive    = "bdrive";
static constexpr const char* bMod      = "bmod";
static constexpr const char* routing   = "routing";
static constexpr const char* sync      = "sync";
static constexpr const char* division  = "division";
static constexpr const char* rate      = "rate";
static constexpr const char* shape     = "shape";
static constexpr const char* lfoDepth  = "lfodepth";
static constexpr const char* stereo    = "stereo";
static constexpr const char* glide     = "glide";
static constexpr const char* retrig    = "retrig";
static constexpr const char* envDepth  = "envdepth";
static constexpr const char* envSpeed  = "envspeed";
static constexpr const char* trigger   = "trigger";
static constexpr const char* sens      = "sens";
static constexpr const char* attack    = "attack";
static constexpr const char* decay     = "decay";
static constexpr const char* sustain   = "sustain";
static constexpr const char* release   = "release";
static constexpr const char* adsrDepth = "adsrdepth";
static constexpr const char* mix       = "mix";
static constexpr const char* output    = "output";
inline juce::String step (int i) { return "step" + juce::String (i + 1).paddedLeft ('0', 2); }   // step01..step16
} // namespace IDs

class SweepProcessor final : public juce::AudioProcessor
{
public:
    SweepProcessor();

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
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    sweepfx::Params currentParams() const;
    const sweepfx::Engine& getEngine() const { return engine; }
    double getDisplaySampleRate() const { return lastSampleRate; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<double> uiBpm { 140.0 };
    std::atomic<bool> uiHostTempo { false };
    std::atomic<float> uiLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    sweepfx::Engine engine;
    int currentProgram = 0;
    double lastSampleRate = 48000.0;

    struct Raw
    {
        std::atomic<float>* p = nullptr;
        float get() const { return p->load(); }
    };
    Raw aType, aCutoff, aReso, aDrive, aMod, bOn, bType, bCutoff, bReso, bDrive, bMod, routing, sync, division, rate, shape,
        lfoDepth, stereo, glide, retrig, envDepth, envSpeed, trigger, sens, attack, decay, sustain, release, adsrDepth, mix, output;
    std::array<Raw, sweepfx::numSteps> steps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SweepProcessor)
};
