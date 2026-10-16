/*
    TrapWin BEND DEMON
    PluginProcessor.h - the instrument: plain MIDI in -> GestureEngine (MPE) -> hosted synth -> audio out.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Parameters.h"
#include "VisualFifo.h"
#include "engine/GestureEngine.h"
#include "host/HostedPluginManager.h"
#include "presets/PresetManager.h"

class BendDemonProcessor final : public juce::AudioProcessor,
                                 private juce::Timer
{
public:
    BendDemonProcessor();
    ~BendDemonProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return true; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override                    { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept     { return apvts; }
    bd::HostedPluginManager& getHost() noexcept                  { return *host; }
    bd::PresetManager& getPresets() noexcept                     { return presets; }
    bd::VisualFifo& getVisualFifo() noexcept                     { return visualFifo; }

    int  getActiveVoiceCount() const noexcept    { return activeVoices.load (std::memory_order_relaxed); }
    int  getConfiguredVoices() const noexcept    { return configuredVoices.load (std::memory_order_relaxed); }
    int  getBendRange() const noexcept           { return bendRangeNow.load (std::memory_order_relaxed); }
    bool isMidiOutMode() const noexcept          { return snapshot.midiOutMode(); }

    /** Sends note-offs + recentres everything (UI panic button). */
    void panic() noexcept                        { engine.requestReleaseAll(); engine.requestZoneConfiguration(); }

    /** UI state stored with the project. */
    juce::ValueTree getUiState()                 { return apvts.state.getOrCreateChildWithName ("UI", nullptr); }

private:
    /** Forwards BEND DEMON's own host playhead to the hosted synth (tempo, ppq, transport). */
    struct ForwardingPlayHead final : public juce::AudioPlayHead
    {
        explicit ForwardingPlayHead (BendDemonProcessor& p) : owner (p) {}

        juce::Optional<PositionInfo> getPosition() const override
        {
            if (auto* ph = owner.getPlayHead())
                return ph->getPosition();
            return {};
        }

        bool canControlTransport() override
        {
            if (auto* ph = owner.getPlayHead()) return ph->canControlTransport();
            return false;
        }

        void transportPlay (bool b) override              { if (auto* ph = owner.getPlayHead()) ph->transportPlay (b); }
        void transportRecord (bool b) override            { if (auto* ph = owner.getPlayHead()) ph->transportRecord (b); }
        void transportRewind() override                   { if (auto* ph = owner.getPlayHead()) ph->transportRewind(); }

        BendDemonProcessor& owner;
    };

    void timerCallback() override;
    void processChunk (juce::AudioBuffer<float>& audio, int startSample, int numSamples,
                       const juce::MidiBuffer& midiIn, juce::MidiBuffer& midiOut, bool midiOutMode,
                       bool sendToBoth);
    bd::TransportState readTransport();
    void updateLatency();

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    bd::ParameterSnapshot snapshot;
    ForwardingPlayHead forwardingPlayHead { *this };
    std::unique_ptr<bd::HostedPluginManager> host;
    bd::PresetManager presets;

    bd::GestureEngine engine;
    bd::VisualFifo visualFifo;

    juce::MidiBuffer mpeBuffer, chunkIn, chunkOut, hostedMidi;
    juce::SmoothedValue<float> gain { 1.0f };

    double currentSampleRate = 44100.0;
    int    maxBlockSize = 512;
    bool   wasPlaying = false;
    bool   lastMidiOutMode = false;
    bool   wasBypassed = false;

    std::atomic<int>  activeVoices { 0 }, configuredVoices { 15 }, bendRangeNow { 48 };
    std::atomic<int>  engineLatency { 0 };
    int currentProgram = 0;
    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BendDemonProcessor)
};
