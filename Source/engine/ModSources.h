#pragma once

// MOD SOURCES: the fixed "MOD" unit under the performer strip (DESIGN-RACK.md section 12, item 1).
//
//   * 2 LFOs        shape SINE / TRI / SAW / SQUARE / S&H, rate free (Hz) or tempo-synced, phase offset
//   * ENV FOLLOWER  attack / release / sensitivity, follows the plugin input (after the input gain)
//   * STEP SEQ      16 steps in tempo, rate per step, smoothing (glide between steps)
//
// Up to 16 assignments route a source to a slot parameter (P1..P8, 8 = slot MIX) or to a macro,
// with a bipolar depth (-1..+1). Offsets are added to the effective (macro-processed) normalised value
// and clamped to 0..1. A macro target is offset before the macro engine runs.
//
// Everything here is plain fixed-size data: the state travels to the audio thread inside the
// immutable ProgramExtras snapshot (lock-free exchange), the runtime never allocates or locks.
// No assignment (the default) = the engine takes exactly the same code path as before = identical sound.
//
// The file is named ModSources.* because Modulation.* is the older AV (XY / scribble) modulation.

#include "Rack.h"
#include <atomic>

namespace ek
{
constexpr int kNumModSources = 4;
constexpr int kMaxModAssignments = 16;
constexpr int kSeqSteps = 16;
constexpr int kModBlock = 32;          // modulation is applied every 32 samples while anything is patched
constexpr int kNumModSlotTargets = 9;  // P1..P8 + MIX per slot

enum class ModSourceId : int { Lfo1 = 0, Lfo2, Env, Seq };
enum class LfoShape : int { Sine = 0, Triangle, Saw, Square, SampleHold, Count };

const char* modSourceName (int source) noexcept;   // "LFO 1", "LFO 2", "ENV", "SEQ"
const char* lfoShapeName (LfoShape s) noexcept;    // "SINE" ...
bool modSourceIsBipolar (int source) noexcept;     // LFOs -1..+1, ENV and SEQ 0..1

// Tempo divisions for synced LFOs and the step sequencer (beats per cycle / per step).
struct ModDivision { const char* label; double beats; };
int numModDivisions() noexcept;
const ModDivision& modDivision (int index) noexcept;  // index is clamped
int findModDivision (const juce::String& label, int fallback) noexcept;

struct LfoSettings
{
    LfoShape shape = LfoShape::Sine;
    bool sync = false;
    float rateHz = 1.0f;   // free rate, 0.01..20 Hz
    int division = 5;      // synced rate (index into modDivision), default 1/4
    float phase = 0.0f;    // 0..1 start-phase offset

    static constexpr float minHz = 0.01f, maxHz = 20.0f;
    bool operator== (const LfoSettings& o) const noexcept
    { return shape == o.shape && sync == o.sync && rateHz == o.rateHz && division == o.division && phase == o.phase; }
};

struct EnvSettings
{
    float attackMs = 10.0f;    // 0.1..500 ms
    float releaseMs = 150.0f;  // 5..5000 ms
    float sensitivity = 0.5f;  // 0..1 = input gain 0..+36 dB before the follower
    static float sensitivityGainDb (float s) noexcept { return 36.0f * clamp01 (s); }
    bool operator== (const EnvSettings& o) const noexcept
    { return attackMs == o.attackMs && releaseMs == o.releaseMs && sensitivity == o.sensitivity; }
};

struct SeqSettings
{
    std::array<float, kSeqSteps> steps { 1.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.0f, 0.25f, 0.0f,
                                         1.0f, 0.0f, 0.5f, 0.0f, 0.75f, 0.25f, 0.5f, 0.0f };
    int division = 10;     // length of one step (index into modDivision), default 1/16
    float smooth = 0.0f;   // 0 = hard steps, 1 = glide over the whole step
    bool operator== (const SeqSettings& o) const noexcept { return steps == o.steps && division == o.division && smooth == o.smooth; }
};

struct ModAssignment
{
    int source = 0;      // ModSourceId
    int slot = 0;        // 0..7, or -1 = macro target
    int param = 0;       // slot target: 0..7 = P1..P8, 8 = MIX; macro target: macro index 0..4
    float depth = 0.0f;  // -1..+1 (full range of the normalised parameter)

    bool isMacro() const noexcept { return slot < 0; }
    bool isValid() const noexcept;
    bool sameTarget (const ModAssignment& o) const noexcept { return slot == o.slot && param == o.param; }
    bool operator== (const ModAssignment& o) const noexcept
    { return source == o.source && slot == o.slot && param == o.param && depth == o.depth; }
};

struct ModSourceState
{
    std::array<LfoSettings, 2> lfo {};
    EnvSettings env;
    SeqSettings seq;
    std::array<ModAssignment, kMaxModAssignments> assign {};
    int numAssign = 0;

    static constexpr int version = 1;

    ModSourceState()
    {
        lfo[1].shape = LfoShape::Triangle;
        lfo[1].sync = true;
        lfo[1].division = 2; // 1 bar
    }

    bool hasAssignments() const noexcept { return numAssign > 0; }
    bool isModulating() const noexcept;          // any valid assignment with depth != 0
    int add (const ModAssignment& a);            // replaces depth when source + target exist; -1 when full / invalid
    void remove (int index);
    void removeTarget (int slot, int param);     // all assignments of one target
    void removeSlot (int slot);                  // all assignments of one slot
    void remapSlots (const std::array<int, kNumSlots>& newIndexOfOld); // after a slot reorder (-1 = slot removed)

    bool operator== (const ModSourceState& o) const noexcept;
    bool operator!= (const ModSourceState& o) const noexcept { return ! (*this == o); }

    juce::var toVar() const;
    static ModSourceState fromVar (const juce::var& v); // missing / invalid = defaults (no modulation)
};

// Values for the UI (written by the audio thread, relaxed atomics).
struct ModSourceMeters
{
    std::array<std::atomic<float>, kNumModSources> value {};     // current source outputs
    std::array<std::atomic<float>, 2> lfoPhase {};               // 0..1, incl. the phase offset
    std::atomic<int> seqStep { 0 };
    std::atomic<bool> active { false };                          // something is patched and running
    // current smoothed offsets per target (slot param 0..8 / macro)
    std::array<std::array<std::atomic<float>, kNumModSlotTargets>, kNumSlots> slotOffset {};
    std::array<std::atomic<float>, kNumMacros> macroOffset {};
};

// Audio-thread runtime of the four sources plus the per-target offset smoothing.
class ModSourceEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    // Advance all sources by n samples. inL / inR = the input signal of these n samples (env follower).
    void advance (const ModSourceState& st, const ProcessContext& ctx, const float* inL, const float* inR, int n) noexcept;

    // Recompute the per-target offsets from the current source values (smoothed over ~4 ms, n = samples
    // since the last call). Returns true while any offset is non-zero (= the mod path has to run).
    bool updateOffsets (const ModSourceState& st, int n) noexcept;

    bool needsProcessing (const ModSourceState& st) const noexcept { return anyOffset || st.isModulating(); }

    // Apply the offsets of the current block (macros before MacroEngine::compute, slots after it).
    // A macro the host is automating (suspended) is left alone. Results are clamped to 0..1.
    void applyMacroOffsets (std::array<float, kNumMacros>& macros, const std::array<bool, kNumMacros>& suspended) const noexcept;
    void applySlotOffsets (RackParams& eff) const noexcept;

    // Offsets of the current block.
    float macroOffset (int m) const noexcept { return macroOff[(size_t) m]; }
    float slotOffset (int slot, int param) const noexcept { return slotOff[(size_t) slot][(size_t) param]; }

    float getValue (int source) const noexcept { return values[(size_t) source]; }
    float getLfoPhase (int i) const noexcept { return (float) lfoPhaseOut[(size_t) i]; }
    int getSeqStep() const noexcept { return seqStep; }

    void publish (ModSourceMeters& m) const noexcept;

    // Deterministic helpers shared with the UI and the tests.
    static float lfoShape (LfoShape s, double phase01, juce::int64 cycle, int lfoIndex) noexcept; // -1..+1
    static float seqValue (const SeqSettings& s, double stepPos) noexcept;                       // stepPos in steps

    static constexpr float smoothMs = 4.0f;

private:
    double sampleRate = 44100.0;
    double freeBeats = 0.0;                 // own clock when the host transport is not running
    std::array<double, 2> lfoFreePhase {};  // free-running LFO phase (cycles)
    std::array<double, 2> lfoPhaseOut {};
    std::array<float, kNumModSources> values {};
    float env = 0.0f;
    int seqStep = 0;

    std::array<std::array<float, kNumModSlotTargets>, kNumSlots> slotOff {};
    std::array<float, kNumMacros> macroOff {};
    bool anyOffset = false;
};
} // namespace ek
