#pragma once

#include "DSP/SubEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* mode     = "mode";
static constexpr const char* glide    = "glide";
static constexpr const char* tune     = "tune";
static constexpr const char* fine     = "fine";
static constexpr const char* punch    = "punch";
static constexpr const char* punchTime = "punchtime";
static constexpr const char* click    = "click";
static constexpr const char* sub      = "sub";
static constexpr const char* drive    = "drive";
static constexpr const char* satType  = "sattype";
static constexpr const char* distMix  = "distmix";
static constexpr const char* cutoff   = "cutoff";
static constexpr const char* reso     = "reso";
static constexpr const char* fltEnv   = "fltenv";
static constexpr const char* attack   = "attack";
static constexpr const char* decay    = "decay";
static constexpr const char* sustain  = "sustain";
static constexpr const char* release  = "release";
static constexpr const char* width    = "width";
static constexpr const char* output   = "output";
static constexpr const char* limiter  = "limiter";
} // namespace IDs

class SubKillaProcessor final : public juce::AudioProcessor
{
public:
    SubKillaProcessor();

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
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    subkilla::Params currentParams() const;

    // ---- on-screen keyboard (message thread -> audio thread, lock-free)
    void uiNoteOn (int note, float velocity);
    void uiNoteOff (int note);
    bool isNoteHeld (int note) const;

    // ---- scope: mono output history (decimated to <= 48 kHz)
    static constexpr int scopeSize = 8192;
    int getScopeWritePos() const { return scopeWrite.load (std::memory_order_acquire); }
    float getScopeSample (int index) const { return scope[(size_t) (index & (scopeSize - 1))].load (std::memory_order_relaxed); }
    double getScopeRate() const { return scopeRate.load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiHz { 0.0f }, uiNote { 0.0f }, uiEnv { 0.0f }, uiGrDb { 0.0f }, uiPeak { 0.0f };
    std::atomic<bool> uiGliding { false };
    std::atomic<int> uiVoices { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setHeld (int note, bool on);

    subkilla::Engine engine;
    int currentProgram = 0;

    std::array<subkilla::Event, 1024> events {};
    juce::AbstractFifo uiFifo { 256 };
    std::array<subkilla::Event, 256> uiEvents {};
    std::array<std::atomic<juce::uint32>, 4> heldMask {};

    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<double> scopeRate { 48000.0 };
    int scopeDecim = 1, scopeCount = 0;
    std::array<float, 512> scratchL {}, scratchR {};
    std::array<subkilla::Event, 1024> chunkEvents {};

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pGlide = nullptr;
    std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pFine = nullptr;
    std::atomic<float>* pPunch = nullptr;
    std::atomic<float>* pPunchTime = nullptr;
    std::atomic<float>* pClick = nullptr;
    std::atomic<float>* pSub = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pSatType = nullptr;
    std::atomic<float>* pDistMix = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pReso = nullptr;
    std::atomic<float>* pFltEnv = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pSustain = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pLimiter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubKillaProcessor)
};
