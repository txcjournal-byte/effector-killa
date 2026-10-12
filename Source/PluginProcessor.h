#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Randomizer.h"
#include <map>

namespace ek
{
namespace ParamIDs
{
    juce::String slotParam (int slot, int param); // "s1_p1" .. "s8_p8"
    juce::String slotMix (int slot);              // "s1_mix"
    juce::String slotPause (int slot);            // "s1_pause"
    juce::String macro (int m);                   // "villain_arc" ...
    inline const juce::String inGain { "antenna_in" };
    inline const juce::String outGain { "rf_out" };
    inline const juce::String blend { "blend" };
    inline const juce::String autoTracking { "auto_tracking" };
    inline const juce::String pocketTv { "pocket_tv" };
    inline const juce::String offAir { "off_air" };
    inline const juce::String recycle { "recycle" };       // rack output fed back into its input
    inline const juce::String tempoMode { "tempo_mode" };  // 0 = HOST, 1 = MANUAL (also TAP)
    inline const juce::String tempoBpm { "tempo_bpm" };
}

// Live slot info for the host-facing parameter names ("S1: Cassette Plug: Drive").
struct HostInfo
{
    std::array<std::atomic<int>, ek::kNumSlots> type {};
};
} // namespace ek

class EffectorKillaAudioProcessor : public juce::AudioProcessor,
                                    public juce::ChangeBroadcaster,
                                    private juce::Timer,
                                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    EffectorKillaAudioProcessor();
    ~EffectorKillaAudioProcessor() override;

    // ---- juce::AudioProcessor --------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TrapWin Effector"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return meta.name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Effector Killa API (message thread) --------------------------------------------------
    std::shared_ptr<ek::HostInfo> hostInfo = std::make_shared<ek::HostInfo>();
    juce::AudioProcessorValueTreeState apvts;
    ek::Engine engine;
    ek::PresetBank bank;

    ek::ProgramState captureProgram() const;          // current sound incl. host parameter values
    const ek::ProgramState& getMeta() const noexcept { return meta; }
    void applyProgram (const ek::ProgramState& p, bool undoable = true);

    // presets / channels
    void loadFactory (int channel, int index, bool undoable = true);
    void stepPreset (int delta);
    float factoryTrimDb (int channel, int index); // loudness match of a factory preset (cached)
    void stepChannel (int delta);
    int getCurrentChannel() const noexcept { return meta.channel >= 0 ? meta.channel : lastChannel; }
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& f);
    bool isCurrentFavourite() const;
    void toggleFavourite();

    // KILL / BOOTLEG
    void kill();
    void bootleg();
    ek::KillLevel killLevel = ek::KillLevel::R;
    std::atomic<int> killEvents { 0 }, programEvents { 0 };

    // history / A-B
    void pushUndo();
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    int getSide() const noexcept { return side; }
    void setSide (int newSide);
    static constexpr int maxUndo = 64;

    // slots
    void setSlotType (int slot, ek::EffectType t);
    void playTape (int slot, ek::EffectType t, int programme);   // load a cassette programme into a slot
    int findTape (int slot) const;                               // programme matching the slot, or -1
    void ejectSlot (int slot) { setSlotType (slot, ek::EffectType::None); }
    void moveSlot (int from, int to);
    int insertEffect (int at, ek::EffectType t);   // insert before slot `at` (kNumSlots = append), returns the slot or -1 when full
    void removeEffect (int slot);                  // remove and close the gap
    void setSlotMs (int slot, ek::MSMode m);
    void setWriteProtect (int slot, bool on);
    void setScreening (int slot); // -1 = off
    int getScreening() const noexcept { return screening; }
    void setSolo (int slot);      // -1 = off; hear only this unit
    int getSolo() const noexcept { return solo; }
    void copySlot (int slot);
    void pasteSlot (int slot);
    bool hasSlotClipboard() const noexcept { return slotClipboard.has_value(); }
    bool saveSlotPreset (int slot, const juce::String& name);
    bool loadSlotPreset (int slot, const juce::File& f);
    void setSlotParam (int slot, int param, float normValue);  // param 8 = MIX, 9 = PAUSE
    float getSlotParam (int slot, int param) const;
    ek::EffectType getSlotType (int slot) const noexcept { return meta.slots[(size_t) slot].type; }
    const ek::SlotState& getSlotMeta (int slot) const noexcept { return meta.slots[(size_t) slot]; }
    ek::SlotState getSlotState (int slot) const; // meta + live parameter values (pause, mix, P1-P8)
    int selectedSlot = -1;

    // ---- SPLIT / MERGE (parallel branches, at most one split; see MANUAL.md "SPLIT") ----------
    // Layout: slots [0, at) run in series in front of the split; every unit in slots [at, 8) belongs to
    // branch A or B and runs in slot order inside its branch; MERGE sums A and B. Structural calls are
    // undo steps and crossfade like any rack change; the knob setters are not (call pushUndo() at the
    // start of a gesture if wanted). Host parameters are unchanged (the 8 slot sets keep their IDs).
    bool hasSplit() const noexcept { return meta.split.isActive(); }
    const ek::SplitState& getSplit() const noexcept { return meta.split; }
    int getSplitPosition() const noexcept { return meta.split.isActive() ? meta.split.at : -1; }
    void insertSplit (int at);                 // split in front of slot `at` (0..kNumSlots), moves an existing one
    void removeSplit();                        // back to a serial rack; units keep their order
    void setSplitMode (ek::SplitMode m);       // PARALLEL, L/R, M/S, LOW/HIGH, TRANSIENT/SUSTAIN
    void setSplitFreq (float hz);              // LOW/HIGH crossover, 40 Hz - 12 kHz
    void setSplitBalance (float balance);      // 0 = A only, 0.5 = both, 1 = B only
    void setSplitLevel (int branch, float db); // per-branch level, -24 .. +12 dB
    void setSlotBranch (int slot, int branch); // move a unit to branch A (0) / B (1); a unit in front of the split moves behind it
    int getSlotBranch (int slot) const noexcept; // -1 = serial (no split or in front of it), 0 = A, 1 = B
    int insertEffectInBranch (int branch, ek::EffectType t); // new unit at the end of branch A/B, returns its slot or -1
    int insertEffectInBranchAt (int at, int branch, ek::EffectType t) { return insertEffectImpl (at, t, branch < 0 ? -1 : (branch != 0 ? 1 : 0)); } // insert at `at` inside branch A/B
    std::vector<int> getBranchSlots (int branch) const; // non-empty slots of a branch in processing order (-1 = in front of the split)

    // source / modulation / settings
    void setSource (ek::Source s);
    void setModState (const ek::ModState& m, bool undoable);
    void setOversampling (int osFactorLog2);
    int getOversampling() const noexcept { return oversampling; }
    bool premiumCable = true;
    float uiScale = 0.0f;       // 0 = auto (fits the screen), else 0.6 - 2.0
    bool showGear = true;       // gear list visible
    bool softwareTv = false;    // force the software TV renderer

    // SPECTRAL VIEW: analyse the signal before / after this rack slot (-1 = off, zero cost).
    // Typically the hovered unit; the UI side is ui/rack/SpectrumOverlay.
    void setAnalyzedSlot (int slot) noexcept { engine.getRack().getSlotTap().setAnalyzedSlot (slot); }
    int getAnalyzedSlot() const noexcept { return engine.getRack().getSlotTap().getAnalyzedSlot(); }
    // LEVEL-MATCHED POWER: while held, hear the rack without this unit, matched to the with-unit
    // loudness (+-12 dB, 20 ms crossfade). -1 = off. Latency and default output are unchanged.
    void setLevelMatchedAudition (int slot) noexcept { engine.getRack().getSlotTap().setAuditionSlot (slot); }
    int getLevelMatchedAudition() const noexcept { return engine.getRack().getSlotTap().getAuditionSlot(); }
    float getAuditionGainDb() const noexcept { return engine.getRack().getSlotTap().getAuditionGainDb(); }
    ek::SlotTap& getSlotTap() noexcept { return engine.getRack().getSlotTap(); }

    // macro gestures from the UI (host automation of macros disconnects AV modulation)
    void beginUiGesture (int macro);
    void endUiGesture (int macro);

    juce::RangedAudioParameter* getParam (const juce::String& id) const { return apvts.getParameter (id); }
    void setParamNorm (const juce::String& id, float v);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (std::shared_ptr<ek::HostInfo> info = {});

    // ---- MOD SOURCES (message thread) -------------------------------------------------------
    // The MOD unit: LFO 1, LFO 2, ENV (envelope follower on the input) and SEQ (16-step sequencer).
    // Source ids: ek::ModSourceId (0 = LFO 1, 1 = LFO 2, 2 = ENV, 3 = SEQ).
    // A target is a slot parameter (slot 0..7, param 0..7 = P1..P8, 8 = slot MIX) or a macro (0..4).
    // Depth is bipolar -1..+1 in normalised units: the LFOs swing the value by +-depth, ENV and SEQ
    // (0..1 sources) push it from the knob value towards +depth. Max 16 assignments (ek::kMaxModAssignments).
    // Everything is part of the program: saved in presets, A/B, undo and the DAW project.
    // Default (no assignment) = no modulation, the audio path is bit-identical to a build without it.
    int addModAssignment (int source, int slot, int param, float depth);  // returns index, -1 = full / invalid;
                                                                          // an existing source+target only gets the new depth
    int addMacroModAssignment (int source, int macro, float depth);       // modulate a macro knob instead
    void removeModAssignment (int index);
    void removeModAssignmentsFor (int slot, int param);                  // slot = -1: param is a macro index
    void clearModAssignments();
    void setModDepth (int index, float depth);                           // no undo step (call pushUndo() at gesture start)
    std::vector<ek::ModAssignment> getModAssignments() const;            // in table order (index = position)
    std::vector<int> findModAssignments (int slot, int param) const;     // indices of all assignments of a target
    const ek::ModSourceState& getModSources() const noexcept { return meta.modSources; }
    void setModSources (const ek::ModSourceState& s, bool undoable = false); // replace everything (settings + table)
    void setLfo (int lfo, const ek::LfoSettings& s);                     // lfo 0 / 1
    void setEnvFollower (const ek::EnvSettings& s);
    void setStepSeq (const ek::SeqSettings& s);
    void setSeqStep (int step, float value01);
    // live values for the UI (audio thread atomics): LFO -1..1, ENV / SEQ 0..1
    float getModSourceValue (int source) const noexcept;
    float getLfoPhase (int lfo) const noexcept;                          // 0..1 (incl. phase offset)
    int getSeqStep() const noexcept { return engine.modMeters.seqStep.load (std::memory_order_relaxed); }
    float getModOffset (int slot, int param) const noexcept;             // current smoothed offset (slot = -1: macro)
    float getEffectiveSlotParam (int slot, int param) const noexcept;    // value the rack hears (macros + modulation)

    // ---- MACRO LEARN (message thread) ----------------------------------------------------------
    // Assign any slot parameter (0..7, 8 = slot MIX) to a macro: macro at 0 -> minNorm, macro at 0.5 ->
    // the knob value, macro at 1 -> maxNorm. Learned mappings add to the macro's built-in behaviour
    // (ek::MacroMapping::learned). Stored in ProgramState::maps -> saved in presets and the project.
    bool assignMacro (int macro, int slot, int param, float minNorm, float maxNorm); // replaces an existing one
    void removeMacroAssignment (int macro, int slot, int param);
    std::vector<ek::MacroMapping> getMacroAssignments() const;           // all mappings (preset + learned, see .learned)
    std::vector<ek::MacroMapping> getMacroAssignmentsFor (int slot, int param) const; // all mappings of a target

    // tempo: HOST follows the DAW, MANUAL uses tempo_bpm (TAP sets it)
    void tapTempo();
    double getEffectiveBpm() const noexcept { return engine.meters.bpm.load(); }

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float newValue) override;
    void pushConfigToEngine();
    void pushExtrasToEngine();
    void modChanged();   // after a meta.modSources / meta.maps edit: push to the engine, tell the UI
    void pushSplitParams();
    int insertEffectImpl (int at, ek::EffectType t, int branch);
    void notifyUi() { sendChangeMessage(); }
    ek::ProgramState stateForSide() const { return captureProgram(); }

    ek::ProgramState meta;             // non-parameter program data (types, names, maps, mod ...)
    int lastChannel = 0;
    // start-up preset: its loudness trim is computed on first use (prepareToPlay, state / program capture)
    void loadFactoryDeferred (int channel, int index);
    void resolvePendingTrim() const;
    std::optional<float> cachedFactoryTrimDb (int channel, int index) const;
    ek::ProgramState captureProgramUnresolved() const;
    int pendingTrimKey = -1;           // channel * 100 + index, -1 = none
    bool pendingTrimSide0 = false;
    int screening = -1, solo = -1;
    int oversampling = 1;
    double currentSampleRate = 44100.0;
    bool applying = false;

    std::vector<ek::ProgramState> undoStack, redoStack;
    std::array<std::optional<ek::ProgramState>, 2> sides;
    int side = 0;
    std::optional<ek::SlotState> slotClipboard;

    // raw parameter pointers for the audio thread
    std::array<std::array<std::atomic<float>*, ek::kNumParams>, ek::kNumSlots> pSlot {};
    std::array<std::atomic<float>*, ek::kNumSlots> pMix {}, pPause {};
    std::array<std::atomic<float>*, ek::kNumMacros> pMacro {};
    std::atomic<float>* pIn = nullptr, *pOut = nullptr, *pBlend = nullptr, *pAuto = nullptr, *pPocket = nullptr, *pOffAir = nullptr;
    std::atomic<float>* pRecycle = nullptr, *pTempoMode = nullptr, *pTempoBpm = nullptr;
    std::atomic<float> splitFreq { 250.0f }, splitBalance { 0.5f }, splitLevelA { 0.0f }, splitLevelB { 0.0f };
    double freePpq = 0.0;                       // free-running transport for MANUAL tempo
    std::vector<double> taps;                   // tap tempo timestamps (ms)
    void updateHostNames();

    std::array<std::atomic<int>, ek::kNumMacros> uiGesture {};
    std::array<std::atomic<juce::uint32>, ek::kNumMacros> lastHostMacroChange {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessor)
};
