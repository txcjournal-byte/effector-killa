#include "SlotTap.h"

namespace ek
{
// ============================================================================
// SpectrumFifo
// ============================================================================
int SpectrumFifo::push (const float* bL, const float* bR, const float* aL, const float* aR, int n) noexcept
{
    const auto scope = fifo.write (n); // clamped to the free space – excess samples are dropped
    auto copy = [&] (int start, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            before[(size_t) (start + i)] = 0.5f * (bL[offset + i] + bR[offset + i]);
            after[(size_t) (start + i)] = 0.5f * (aL[offset + i] + aR[offset + i]);
        }
    };
    copy (scope.startIndex1, scope.blockSize1, 0);
    copy (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    return scope.blockSize1 + scope.blockSize2;
}

int SpectrumFifo::pop (float* beforeOut, float* afterOut, int maxN) noexcept
{
    const auto scope = fifo.read (maxN);
    auto copy = [&] (int start, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            beforeOut[offset + i] = before[(size_t) (start + i)];
            afterOut[offset + i] = after[(size_t) (start + i)];
        }
    };
    copy (scope.startIndex1, scope.blockSize1, 0);
    copy (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    return scope.blockSize1 + scope.blockSize2;
}

void SpectrumFifo::discardAll() noexcept
{
    const auto scope = fifo.read (fifo.getNumReady());
    juce::ignoreUnused (scope);
}

// ============================================================================
// ShortTermLoudness
// ============================================================================
void ShortTermLoudness::prepare (double sr) noexcept
{
    for (auto& s : shelf) s.setHighShelf (1500.0f, 4.0f, (float) sr, 1.0f);
    for (auto& h : hp) h.setHighPass (60.0f, 0.5f, (float) sr);
    coeff = 1.0 - std::exp (-1.0 / (windowMs * 0.001 * sr));
    reset();
}

void ShortTermLoudness::reset() noexcept
{
    for (auto& s : shelf) s.reset();
    for (auto& h : hp) h.reset();
    ms = 0.0;
    count = 0.0;
}

void ShortTermLoudness::process (const float* L, const float* R, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const float l = hp[0].process (shelf[0].process (L[i]));
        const float r = hp[1].process (shelf[1].process (R[i]));
        const double x = 0.5 * ((double) l * l + (double) r * r);
        // exact running mean at first, then an exponential window
        count += 1.0;
        const double k = std::max (coeff, 1.0 / count);
        ms += (x - ms) * k;
    }
    if (! std::isfinite (ms)) reset();
}

// ============================================================================
// SlotTap
// ============================================================================
void SlotTap::prepare (double sr)
{
    sampleRate = sr;
    for (auto& lane : lanes)
    {
        lane.activeSlot = lane.meteredSlot = -1;
        lane.withUnit.prepare (sr);
        lane.withoutUnit.prepare (sr);
        lane.matchDb = 0.0f;
        lane.ramp.reset (std::max (1, (int) std::lround (fadeMs * 0.001 * sr)), 0.0f);
        lane.gain.reset (sr, gainSmoothMs, 1.0f);
    }
    appliedGainDb.store (0.0f);
    matchDbOut.store (0.0f);
    auditionAmount.store (0.0f);
}

void SlotTap::afterSlot (int laneIndex, bool primary, int slot, const float* inL, const float* inR, float* L, float* R, int n) noexcept
{
    auto& lane = lanes[(size_t) (laneIndex & 1)];
    const int requested = auditionSlot.load (std::memory_order_relaxed);
    const int analyzed = analyzedSlot.load (std::memory_order_relaxed);

    // ---- SPECTRAL VIEW: before / after this unit ---------------------------------------------
    if (primary && slot == analyzed)
        fifo.push (inL, inR, L, R, n);

    // ---- LEVEL-MATCHED POWER -----------------------------------------------------------------
    // The audition crossfade belongs to one slot at a time; switching to another slot first fades
    // the old one back in (its own calls see requested != slot and ramp to 0).
    if (lane.activeSlot < 0 && requested == slot)
        lane.activeSlot = slot;

    // Meters follow the auditioned slot, else the requested one, else the hovered unit – so the
    // match is usually already settled when POWER is pressed (the unit is hovered first).
    const int meterSlot = lane.activeSlot >= 0 ? lane.activeSlot : (requested >= 0 ? requested : analyzed);
    if (slot == meterSlot)
    {
        if (lane.meteredSlot != slot)
        {
            lane.withUnit.reset();
            lane.withoutUnit.reset();
            lane.meteredSlot = slot;
        }
        lane.withUnit.process (L, R, n);
        lane.withoutUnit.process (inL, inR, n);
        if (lane.withUnit.hasSignal() && lane.withoutUnit.hasSignal())
            lane.matchDb = juce::jlimit (-maxMatchDb, maxMatchDb, lane.withUnit.getDb() - lane.withoutUnit.getDb());
        if (primary)
            matchDbOut.store (lane.matchDb, std::memory_order_relaxed);
    }

    if (slot != lane.activeSlot)
        return;
    lane.activeSeen = true;

    lane.ramp.setTarget (requested == slot ? 1.0f : 0.0f);
    if (lane.ramp.get() <= 0.0f && ! lane.ramp.isRamping())
    {
        // fully released: idle again, bit-identical output from here on
        lane.activeSlot = -1;
        lane.gain.snap (dbToGain (lane.matchDb));
        if (primary)
        {
            appliedGainDb.store (0.0f, std::memory_order_relaxed);
            auditionAmount.store (0.0f, std::memory_order_relaxed);
        }
        return;
    }

    // Gain starts at the current match the moment the crossfade starts, then follows it smoothly.
    if (lane.ramp.get() <= 0.0f)
        lane.gain.snap (dbToGain (lane.matchDb));
    lane.gain.setTarget (dbToGain (lane.matchDb));

    for (int i = 0; i < n; ++i)
    {
        const float t = lane.ramp.next();
        const float g = lane.gain.next();
        L[i] += (inL[i] * g - L[i]) * t;
        R[i] += (inR[i] * g - R[i]) * t;
    }

    if (primary)
    {
        appliedGainDb.store (gainToDb (lane.gain.get()), std::memory_order_relaxed);
        auditionAmount.store (lane.ramp.get(), std::memory_order_relaxed);
    }
}

void SlotTap::endOfChain (int laneIndex, bool primary) noexcept
{
    auto& lane = lanes[(size_t) (laneIndex & 1)];
    if (lane.activeSlot >= 0 && ! lane.activeSeen)
    {
        lane.activeSlot = -1;
        lane.ramp.reset (std::max (1, (int) std::lround (fadeMs * 0.001 * sampleRate)), 0.0f);
        if (primary)
        {
            appliedGainDb.store (0.0f, std::memory_order_relaxed);
            auditionAmount.store (0.0f, std::memory_order_relaxed);
        }
    }
    lane.activeSeen = false;
}

} // namespace ek
