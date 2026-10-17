/*
    TrapWin BEND DEMON
    PluginProcessor.h - the instrument.

        DAW MIDI ──► GestureEngine (live gestures, MPE) ─┐
                                                         ├─► hosted synth  | internal synth | MIDI out
        Pattern  ──► PatternPlayer (line sequencer) ─────┘

    LIVE mode = 1.x behaviour (the gesture engine owns every MPE member channel).
    DRAW mode = the drawn pattern plays in sync with the host (or the PREVIEW clock); incoming
    notes still play live through the gestures on a few low member channels, the sequencer uses
    the rest of the same MPE zone (or channel 1 in Mono/Legacy mode).
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Parameters.h"
#include "VisualFifo.h"
#include "engine/GestureEngine.h"
#include "host/HostedPluginManager.h"
#include "presets/PresetManager.h"
#include "PatternStore.h"
#include "engine/PatternPlayer.h"
#include "engine/InternalSynth.h"
#include "engine/CaptureFifo.h"
#include "engine/CaptureRecorder.h"

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
    void panic() noexcept                        { engine.requestReleaseAll(); engine.requestZoneConfiguration(); player.requestResync(); }

    //==============================================================================
    // 2.0: line sequencer
    bd::PatternStore& getPatternStore() noexcept { return patternStore; }
    bool isDrawMode() const noexcept             { return snapshot.drawMode(); }
    void setDrawMode (bool draw);

    /** PREVIEW: plays the pattern on an internal clock while the host is stopped (and is the only
        transport of the Standalone app). Stops by itself when the host starts playing. */
    void setPreviewPlaying (bool shouldPlay) noexcept { previewRequested.store (shouldPlay); }
    bool isPreviewPlaying() const noexcept       { return previewRequested.load(); }
    bool isSequencerRunning() const noexcept     { return seqRunning.load (std::memory_order_relaxed); }
    double getTempo() const noexcept             { return tempoNow.load (std::memory_order_relaxed); }

    //==============================================================================
    // 2.0: synth slot (INTERNAL or the hosted plugin)
    bool isUsingInternalSynth() const noexcept   { return useInternal.load() || ! host->hasPlugin(); }
    void setUseInternalSynth (bool) noexcept;
    int  getInternalSound() const noexcept       { return snapshot.internalSound(); }
    void setInternalSound (int);
    juce::String getSynthSlotText() const;

    /** Set when a synth without (known) MPE support was loaded: BEND DEMON switched to mono slides.
        The editor shows a banner until dismissNonMpeNotice(). */
    bool hasNonMpeNotice() const noexcept        { return nonMpeNotice.load(); }
    void dismissNonMpeNotice() noexcept          { nonMpeNotice.store (false); }
    void showNonMpeNoticeForTest() noexcept      { nonMpeNotice.store (true); }

    //==============================================================================
    // 2.0: CAPTURE (message thread)
    bd::CaptureRecorder& getCaptureRecorder() noexcept { return captureRecorder; }
    void armCapture();                            // one pass of the pattern length
    int  captureLastBars (int bars);              // returns the number of captured lines (0 = nothing)
    std::function<void (int numLines)> onCaptureComplete;   // UI hint after an armed capture

    /** Loads a factory pattern preset index (PresetManager) - also sets mode DRAW. */
    bd::EngineSettings getGestureSettings() const noexcept { return snapshot.read(); }

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

    enum Destination : int { destMidi = 1, destHosted = 2, destInternal = 4 };

    void timerCallback() override;
    void processChunk (juce::AudioBuffer<float>& audio, int startSample, int numSamples,
                       const juce::MidiBuffer& mpe, juce::MidiBuffer& midiOut, int destination, int sendMask);
    void applyCapturedLines (std::vector<bd::PatternLine>, double lengthBeats, const juce::String& actionName);
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
    bd::PatternStore patternStore;
    bd::PatternPlayer player;
    bd::InternalSynth synth;
    bd::CaptureFifo captureFifo;
    bd::CaptureRecorder captureRecorder;

    juce::MidiBuffer mpeBuffer, chunkIn, chunkOut, hostedMidi;
    juce::SmoothedValue<float> gain { 1.0f };

    double currentSampleRate = 44100.0;
    int    maxBlockSize = 512;
    bool   wasPlaying = false;
    bool   wasBypassed = false;
    int    lastDestination = 0;
    int    lastSound = -1;
    bool   previewWasOn = false;
    double previewPpq = 0.0;
    double freeBeats = 0.0;          // capture clock while nothing is playing

    std::atomic<bool> previewRequested { false }, useInternal { false }, seqRunning { false }, nonMpeNotice { false };
    bool suppressAutoMono = false;
    juce::MidiBuffer monoScratch;
    std::atomic<double> tempoNow { 140.0 };

    std::atomic<int>  activeVoices { 0 }, configuredVoices { 15 }, bendRangeNow { 48 };
    std::atomic<int>  engineLatency { 0 };
    int currentProgram = 0;
    int reportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BendDemonProcessor)
};
