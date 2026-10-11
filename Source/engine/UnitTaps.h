#pragma once

// Live per-unit telemetry for the rack UI ("taps"): input / output level, gain reduction,
// activity values (Overcooked bands, Slap transient) and a small stereo scope ring.
//
// Real-time rules: the audio thread only does relaxed atomic loads/stores here (no locks, no
// allocation). Peaks are "max since the last UI read": the audio thread raises them, the UI
// reads them with exchange (0). A lost update between the two only drops one peak - harmless.
//
// Written by Chain::processSlot (Rack.cpp) for the chain that is currently audible; read by the
// UI timer (RackView). Kept separate from the DSP so other engine features are not affected.

#include "Effect.h"
#include <atomic>

namespace ek
{
class UnitTaps
{
public:
    static constexpr int scopeSize = 256;   // stereo frames per slot
    static constexpr int scopeDecimation = 2;
    static constexpr int numActivity = 3;

    struct Slot
    {
        std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
        std::atomic<float> grDb { 0.0f };                         // max reduction since last read, >= 0
        std::array<std::atomic<float>, numActivity> activity {};  // max since last read, 0..1
        std::array<std::atomic<float>, scopeSize> scopeL {}, scopeR {};
        std::atomic<int> scopePos { 0 };
        int decim = 0;                                            // audio thread only
    };

    // ---- audio thread ------------------------------------------------------------------------
    void captureIn (int slot, const float* L, const float* R, int n) noexcept
    {
        if (! valid (slot)) return;
        raise (slots[(size_t) slot].inPeak, peak (L, R, n));
    }

    void captureOut (int slot, const float* L, const float* R, int n, const Effect* fx) noexcept
    {
        if (! valid (slot)) return;
        auto& s = slots[(size_t) slot];
        raise (s.outPeak, peak (L, R, n));
        if (fx != nullptr)
        {
            raise (s.grDb, std::max (0.0f, -fx->getGainReductionDb()));
            for (int a = 0; a < numActivity; ++a)
                raise (s.activity[(size_t) a], fx->getActivity (a));
        }
        int pos = s.scopePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            if (++s.decim < scopeDecimation) continue;
            s.decim = 0;
            s.scopeL[(size_t) pos].store (L[i], std::memory_order_relaxed);
            s.scopeR[(size_t) pos].store (R[i], std::memory_order_relaxed);
            pos = (pos + 1) % scopeSize;
        }
        s.scopePos.store (pos, std::memory_order_release);
    }

    // ---- UI thread ---------------------------------------------------------------------------
    float takeInPeak (int slot) noexcept       { return valid (slot) ? slots[(size_t) slot].inPeak.exchange (0.0f) : 0.0f; }
    float takeOutPeak (int slot) noexcept      { return valid (slot) ? slots[(size_t) slot].outPeak.exchange (0.0f) : 0.0f; }
    float takeGainReduction (int slot) noexcept { return valid (slot) ? slots[(size_t) slot].grDb.exchange (0.0f) : 0.0f; }
    float takeActivity (int slot, int a) noexcept
    {
        return valid (slot) && a >= 0 && a < numActivity ? slots[(size_t) slot].activity[(size_t) a].exchange (0.0f) : 0.0f;
    }

    // Copies the newest scopeSize frames, oldest first.
    void copyScope (int slot, float* destL, float* destR) const noexcept
    {
        if (! valid (slot)) return;
        const auto& s = slots[(size_t) slot];
        const int start = s.scopePos.load (std::memory_order_acquire);
        for (int i = 0; i < scopeSize; ++i)
        {
            const int k = (start + i) % scopeSize;
            destL[i] = s.scopeL[(size_t) k].load (std::memory_order_relaxed);
            destR[i] = s.scopeR[(size_t) k].load (std::memory_order_relaxed);
        }
    }

private:
    static bool valid (int slot) noexcept { return slot >= 0 && slot < kNumSlots; }

    static float peak (const float* L, const float* R, int n) noexcept
    {
        float p = 0.0f;
        for (int i = 0; i < n; ++i)
            p = std::max (p, std::max (std::abs (L[i]), std::abs (R[i])));
        return std::isfinite (p) ? p : 0.0f;
    }

    static void raise (std::atomic<float>& a, float v) noexcept
    {
        if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed);
    }

    std::array<Slot, kNumSlots> slots;
};
} // namespace ek
