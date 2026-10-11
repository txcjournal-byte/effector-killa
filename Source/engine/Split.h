#pragma once

// SPLIT / MERGE – parallel branches inside the rack (DESIGN-RACK.md section 12, item 2).
//
// The rack can hold at most one SPLIT. It sits in front of slot `at`:
//
//     slots [0, at)   serial, before the split (as today)
//     SPLIT           divides the signal into A and B (PARALLEL, L/R, M/S, LOW/HIGH, TRANSIENT/SUSTAIN)
//     slots [at, 8)   every unit belongs to branch A or B (per-slot flag), processed in slot order
//     MERGE           A * gainA + B * gainB  (A/B balance + per-branch level)
//
// Structure (position, mode, branch flags) is part of RackConfig: changing it builds a new Chain and
// crossfades to it exactly like any other structural change. Continuous values (crossover frequency,
// balance, branch levels) travel with RackParams and are smoothed per sample – no rebuild while a knob
// is turned. With no split (at < 0) the Chain runs its original serial code path, bit for bit.
//
// Latency: both branches are padded to the longer one, so the chain still pads itself up to the
// fixed latency reported to the host. The splitter itself (IIR crossover, envelope detector) adds none.

#include "Effect.h"

namespace ek
{
enum class SplitMode : int { Parallel = 0, LeftRight, MidSide, LowHigh, TransientSustain, Count };
constexpr int kNumSplitModes = (int) SplitMode::Count;

const char* splitModeName (SplitMode m) noexcept;        // "PARALLEL", "L/R", "M/S", "LOW/HIGH", "TRANS/SUS"
const char* splitModeId (SplitMode m) noexcept;          // stable serialisation id
SplitMode splitModeFromId (const juce::String& id) noexcept;
const char* splitBranchName (SplitMode m, int branch) noexcept; // "A"/"B", "LEFT"/"RIGHT", "MID"/"SIDE", ...

// Structural part (lives in RackConfig, compared for chain rebuilds).
struct SplitConfig
{
    int at = -1;                                   // -1 = no split, else 0..kNumSlots: split in front of slot `at`
    SplitMode mode = SplitMode::Parallel;
    std::array<juce::uint8, kNumSlots> branch {};  // 0 = A, 1 = B (only meaningful for slots >= at)

    bool isActive() const noexcept { return at >= 0 && at <= kNumSlots; }
    bool operator== (const SplitConfig& o) const noexcept
    {
        if (isActive() != o.isActive()) return false;
        return ! isActive() || (at == o.at && mode == o.mode && branch == o.branch);
    }
    bool operator!= (const SplitConfig& o) const noexcept { return ! (*this == o); }
};

// Continuous part (lives in RackParams, smoothed on the audio thread).
struct SplitParams
{
    float freqHz = 250.0f;   // LOW/HIGH crossover, 40 Hz .. 12 kHz
    float balance = 0.5f;    // 0 = only A, 0.5 = both, 1 = only B
    float levelADb = 0.0f;   // per-branch level, -24 .. +12 dB
    float levelBDb = 0.0f;

    static constexpr float minFreq = 40.0f, maxFreq = 12000.0f;
    static constexpr float minLevelDb = -24.0f, maxLevelDb = 12.0f;
    SplitParams sanitised() const noexcept;
};

// Merge gains for a balance value. PARALLEL crossfades (0.5 -> 0.5 / 0.5 = unity for identical branches);
// the complementary modes keep both parts at full level in the middle and fade one out towards the ends.
void splitMergeGains (SplitMode mode, float balance, float& gA, float& gB) noexcept;

// Everything a preset / the project state stores about the split (the branch flags live in SlotState).
struct SplitState
{
    int at = -1;
    SplitMode mode = SplitMode::Parallel;
    SplitParams params;

    bool isActive() const noexcept { return at >= 0 && at <= kNumSlots; }
    juce::var toVar() const;                       // { "version": 1, "at": .., "mode": "LOWHIGH", ... }
    static SplitState fromVar (const juce::var& v); // missing / invalid -> inactive
    bool operator== (const SplitState& o) const noexcept;
};

// ---------------------------------------------------------------------------
// DSP: divides one stereo chunk into A and B and merges them back.
// ---------------------------------------------------------------------------
class Splitter
{
public:
    void prepare (double sampleRate, SplitMode mode, const SplitParams& initial);
    void reset() noexcept;

    // params == nullptr -> keep the last values (chain fading out)
    void setParams (const SplitParams* p) noexcept;

    void split (const float* L, const float* R, float* aL, float* aR, float* bL, float* bR, int n) noexcept;

    // focus: -1 = A + B with balance / levels, 0 / 1 = only that branch at unity (SOLO / SCREENING)
    void merge (float* L, float* R, const float* aL, const float* aR, const float* bL, const float* bR, int n, int focus) noexcept;

    SplitMode getMode() const noexcept { return mode; }

private:
    SplitMode mode = SplitMode::Parallel;
    double sr = 44100.0;
    SplitParams last;
    LR4Crossover xL, xR;
    Smoothed freq, gainA, gainB;
    int coeffCountdown = 0;
    // TRANSIENT / SUSTAIN detector (stereo linked)
    EnvelopeFollower fast, slow;
    Smoothed transientGain;
    bool paramsSet = false;
};

// Per-chain split state (owned by a Chain, built off the audio thread together with it).
struct SplitRuntime
{
    int at = kNumSlots;
    int focus = -1;                       // -1 = both branches, 0 / 1 = SOLO / SCREENING inside that branch
    std::array<int, kNumSlots> zone {};   // -1 = before the split, 0 = A, 1 = B
    Splitter splitter;
    LatencyDelay padA, padB;              // pads the shorter branch up to the longer one
    std::vector<float> aL, aR, bL, bR;
};

} // namespace ek
