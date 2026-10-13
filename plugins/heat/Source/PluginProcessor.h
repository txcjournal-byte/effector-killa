#pragma once

#include "DSP/HeatEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* bands     = "bands";
static constexpr const char* xoverLo   = "xlo";
static constexpr const char* xoverHi   = "xhi";
static constexpr const char* os        = "os";
static constexpr const char* tiltPre   = "tiltpre";
static constexpr const char* tiltPost  = "tiltpost";
static constexpr const char* lowCut    = "lowcut";
static constexpr const char* highCut   = "highcut";
static constexpr const char* autoGain  = "autogain";
static constexpr const char* punch     = "punch";
static constexpr const char* dynamics  = "dynamics";
static constexpr const char* lowKeep   = "lowkeep";
static constexpr const char* mix       = "mix";
static constexpr const char* output    = "output";
// per band: "b1model", "b1drive", "b1mix", "b1level", "b1solo" (b1..b3)
inline juce::String band (int b, const char* what) { return "b" + juce::String (b + 1) + what; }
} // namespace IDs

class HeatProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    HeatProcessor();
    ~HeatProcessor() override { cancelPendingUpdate(); }

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

    heatfx::Params currentParams() const;
    double getSampleRateSafe() const { return sampleRateUi.load(); }

    juce::AudioProcessorValueTreeState apvts;

    // ---- UI feeds (audio thread writes, editor reads)
    static constexpr int scopeSize = 8192;          // power of two
    std::array<float, scopeSize> scopeIn {}, scopeOut {};
    std::atomic<int> scopePos { 0 };
    std::atomic<float> uiIn { 0.0f }, uiOut { 0.0f };
    std::array<std::atomic<float>, heatfx::maxBands> uiBand;
    std::atomic<float> uiAutoGainDb { 0.0f }, uiPunchGr { 0.0f }, uiDriveMod { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;

    heatfx::Engine engine;
    int currentProgram = 0;
    std::atomic<double> sampleRateUi { 44100.0 };

    std::atomic<float>* pBands = nullptr;
    std::atomic<float>* pXlo = nullptr;
    std::atomic<float>* pXhi = nullptr;
    std::atomic<float>* pOs = nullptr;
    std::atomic<float>* pTiltPre = nullptr;
    std::atomic<float>* pTiltPost = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pHighCut = nullptr;
    std::atomic<float>* pAutoGain = nullptr;
    std::atomic<float>* pPunch = nullptr;
    std::atomic<float>* pDynamics = nullptr;
    std::atomic<float>* pLowKeep = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    struct BandPtrs { std::atomic<float>* model = nullptr; std::atomic<float>* drive = nullptr; std::atomic<float>* mix = nullptr;
                      std::atomic<float>* level = nullptr; std::atomic<float>* solo = nullptr; };
    std::array<BandPtrs, heatfx::maxBands> pBand;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatProcessor)
};
