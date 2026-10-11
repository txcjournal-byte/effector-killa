#pragma once

// Per-unit taps on the audio thread (DESIGN-RACK.md section 12, items 4 and 5).
//
//  * SPECTRAL VIEW: the signal before and after ONE selected rack slot (the hovered unit) is copied
//    into a lock-free single-producer / single-consumer FIFO. The UI drains it and runs the FFT
//    (ui/rack/SpectrumOverlay). Slot -1 = off: nothing is copied, zero cost.
//
//  * LEVEL-MATCHED POWER: while a unit's POWER button is held, that one unit is replaced by its
//    latency-compensated bypass (the unit keeps running in the background, so its tails are still
//    there when the button is released), gain-matched to the with-unit loudness (approx. K-weighted
//    short-term RMS, +-12 dB clamp) and crossfaded in/out over 20 ms. Reported latency never changes
//    (the bypass is the chain's own dry delay of that slot), and with no audition the audio is
//    bit-identical.
//
// A SlotTap is owned by the Rack and outlives every Chain. Each Chain is attached to one of two
// "lanes" of audition state so that during a 40 ms chain-swap crossfade the old and new chain both
// keep auditioning consistently without advancing the same state twice.

#include "Effect.h"
#include <atomic>

namespace ek
{
// Lock-free SPSC FIFO of mono (before, after) sample pairs. Writer: audio thread. Reader: UI.
class SpectrumFifo
{
public:
    static constexpr int kCapacity = 16384;

    SpectrumFifo() : before ((size_t) kCapacity, 0.0f), after ((size_t) kCapacity, 0.0f) {}

    // Audio thread. Stereo is folded to mono. Samples that do not fit are dropped (never blocks).
    // Returns the number of samples written.
    int push (const float* bL, const float* bR, const float* aL, const float* aR, int n) noexcept;

    // Reader. Copies up to maxN pairs into the given buffers, returns the count.
    int pop (float* beforeOut, float* afterOut, int maxN) noexcept;

    int getNumReady() const noexcept { return fifo.getNumReady(); }
    void discardAll() noexcept; // reader side

private:
    juce::AbstractFifo fifo { kCapacity };
    std::vector<float> before, after;
};

// Approximate K-weighted (60 Hz high-pass + 1.5 kHz +4 dB shelf, same as LoudnessMeter) mean-square
// meter with a ~400 ms exponential window. It starts as an exact running mean, so a usable
// estimate exists from the very first block.
class ShortTermLoudness
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void process (const float* L, const float* R, int n) noexcept;
    double getMeanSquare() const noexcept { return ms; }
    float getDb() const noexcept { return (float) (10.0 * std::log10 (ms + 1.0e-12)); }
    bool hasSignal() const noexcept { return ms > 1.0e-7; } // > -70 dB

    static constexpr float windowMs = 400.0f;

private:
    std::array<Biquad, 2> shelf, hp;
    double ms = 0.0, coeff = 0.0;
    double count = 0.0;
};

class SlotTap
{
public:
    SlotTap() = default;

    // Message thread, audio stopped (called from Rack::prepare).
    void prepare (double sampleRate);

    // ---- control (any thread) ----------------------------------------------------------
    void setAnalyzedSlot (int slot) noexcept { analyzedSlot.store (slot >= 0 && slot < kNumSlots ? slot : -1); }
    int getAnalyzedSlot() const noexcept { return analyzedSlot.load (std::memory_order_relaxed); }
    void setAuditionSlot (int slot) noexcept { auditionSlot.store (slot >= 0 && slot < kNumSlots ? slot : -1); }
    int getAuditionSlot() const noexcept { return auditionSlot.load (std::memory_order_relaxed); }

    // Gain (dB) applied to the bypass while auditioning (0 when idle), for the UI readout.
    float getAuditionGainDb() const noexcept { return appliedGainDb.load (std::memory_order_relaxed); }
    // Current match (with-unit loudness minus without-unit loudness, dB, clamped) of the metered slot.
    float getMatchDb() const noexcept { return matchDbOut.load (std::memory_order_relaxed); }
    // 0..1 position of the audition crossfade (1 = fully auditioning), for UI.
    float getAuditionAmount() const noexcept { return auditionAmount.load (std::memory_order_relaxed); }

    SpectrumFifo& getFifo() noexcept { return fifo; }
    double getSampleRate() const noexcept { return sampleRate; }

    // ---- audio thread (called by Chain) -------------------------------------------------
    // Cheap test whether the slot needs afterSlot() at all.
    bool wants (int lane, int slot) const noexcept
    {
        return slot == analyzedSlot.load (std::memory_order_relaxed)
            || slot == auditionSlot.load (std::memory_order_relaxed)
            || slot == lanes[(size_t) (lane & 1)].activeSlot;
    }

    // inL/inR: latency-aligned input of the slot (= its bypass). L/R: the slot's output, replaced
    // in place by the level-matched bypass while auditioning. primary = this chain is the one that
    // receives live parameters (only it feeds the analyzer FIFO and the UI readouts).
    void afterSlot (int lane, bool primary, int slot, const float* inL, const float* inR, float* L, float* R, int n) noexcept;

    // Audio thread, once per Chain::process: an audition whose slot was not processed in this pass
    // (unit removed / not heard any more) is dropped so it cannot block other slots.
    void endOfChain (int lane, bool primary) noexcept;

    // Audio thread: a chain swap starts – the new chain continues with a copy of the old lane.
    void cloneLane (int from, int to) noexcept { lanes[(size_t) (to & 1)] = lanes[(size_t) (from & 1)]; }

    static constexpr float fadeMs = 20.0f;
    static constexpr float maxMatchDb = 12.0f;
    static constexpr float gainSmoothMs = 50.0f;

private:
    struct Lane
    {
        int activeSlot = -1;   // slot the audition crossfade belongs to (-1 = idle)
        int meteredSlot = -1;  // slot the loudness meters currently follow
        bool activeSeen = false;
        ShortTermLoudness withUnit, withoutUnit;
        float matchDb = 0.0f;
        LinearRamp ramp;       // 0 = unit heard, 1 = level-matched bypass heard
        Smoothed gain;         // linear gain applied to the bypass
    };

    double sampleRate = 44100.0;
    std::atomic<int> analyzedSlot { -1 }, auditionSlot { -1 };
    std::atomic<float> appliedGainDb { 0.0f }, matchDbOut { 0.0f }, auditionAmount { 0.0f };
    std::array<Lane, 2> lanes;
    SpectrumFifo fifo;
};

} // namespace ek
