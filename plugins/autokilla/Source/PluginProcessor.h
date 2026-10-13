#pragma once

#include "DSP/TunerEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace IDs
{
// Parameter IDs are fixed forever after release 1.0.
static constexpr const char* mode      = "mode";
static constexpr const char* key       = "key";
static constexpr const char* scale     = "scale";
static constexpr const char* retune    = "retune";
static constexpr const char* humanize  = "humanize";
static constexpr const char* threshold = "threshold";
static constexpr const char* vibrato   = "vibrato";
static constexpr const char* vibRate   = "vibrate";
static constexpr const char* transpose = "transpose";
static constexpr const char* throat    = "throat";
static constexpr const char* formant   = "formant";
static constexpr const char* midi      = "midi";
static constexpr const char* autoKey   = "autokey";
static constexpr const char* mix       = "mix";
static constexpr const char* output    = "output";
inline juce::String note (int pc) { return "note" + juce::String (pc); }   // custom scale: note0 (C) .. note11 (B)
} // namespace IDs

class AutoKillaProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    AutoKillaProcessor();
    ~AutoKillaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    akill::Params currentParams() const;
    const akill::Engine& getEngine() const { return engine; }
    // copies the auto-detected key into the KEY / SCALE parameters (message thread)
    bool applyDetectedKey();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> uiLevel { 0.0f };
    std::atomic<int> uiMidiMask { 0 };
    double currentSampleRate = 48000.0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void run (juce::AudioBuffer<float>&, juce::MidiBuffer&, bool bypassed);

    akill::Engine engine;
    int currentProgram = 0;
    std::array<bool, 128> held {};
    std::atomic<int> pendingLatency { 0 };

    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pRetune = nullptr;
    std::atomic<float>* pHumanize = nullptr;
    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pVibrato = nullptr;
    std::atomic<float>* pVibRate = nullptr;
    std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pThroat = nullptr;
    std::atomic<float>* pFormant = nullptr;
    std::atomic<float>* pMidi = nullptr;
    std::atomic<float>* pAutoKey = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::array<std::atomic<float>*, 12> pNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoKillaProcessor)
};
