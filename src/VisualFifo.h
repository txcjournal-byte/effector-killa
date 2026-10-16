/*
    TrapWin BEND DEMON
    VisualFifo.h - lock-free single-producer (audio thread) / single-consumer (UI thread) queue of
    VisualEvents, plus atomics describing the transport, so the Voice Field can draw what the
    engine is doing without ever locking the audio thread.
*/
#pragma once

#include <juce_core/juce_core.h>
#include "engine/GestureEngine.h"
#include <array>
#include <atomic>

namespace bd
{

class VisualFifo final : public VisualSink
{
public:
    static constexpr int capacity = 8192;

    // Producer (audio thread) -------------------------------------------------------------
    void pushVisualEvent (const VisualEvent& e) noexcept override
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);

        if (n1 > 0)       buffer[(size_t) s1] = e;
        else if (n2 > 0)  buffer[(size_t) s2] = e;
        else              { dropped.fetch_add (1, std::memory_order_relaxed); return; }   // UI not reading: drop

        fifo.finishedWrite (1);
    }

    /** Called once per block by the processor. */
    void publishTransport (int64_t clock, double sampleRate, double bpm, double ppq, bool hasPpq,
                           bool playing, int timeSigNum, int timeSigDen) noexcept
    {
        anchorClock.store (clock, std::memory_order_relaxed);
        anchorSampleRate.store (sampleRate, std::memory_order_relaxed);
        anchorBpm.store (bpm, std::memory_order_relaxed);
        anchorPpq.store (ppq, std::memory_order_relaxed);
        anchorHasPpq.store (hasPpq, std::memory_order_relaxed);
        anchorPlaying.store (playing, std::memory_order_relaxed);
        sigNum.store (timeSigNum, std::memory_order_relaxed);
        sigDen.store (timeSigDen, std::memory_order_relaxed);
        anchorWallMs.store (juce::Time::getMillisecondCounterHiRes(), std::memory_order_release);
    }

    // Consumer (UI thread) -----------------------------------------------------------------
    template <typename Fn>
    void drain (Fn&& fn)
    {
        const int ready = fifo.getNumReady();
        if (ready <= 0) return;

        int s1, n1, s2, n2;
        fifo.prepareToRead (ready, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) fn (buffer[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) fn (buffer[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
    }

    struct TransportView
    {
        double clock = 0.0;       // interpolated engine sample clock "now"
        double sampleRate = 44100.0;
        double bpm = 120.0;
        double ppq = 0.0;         // interpolated host ppq "now" (if hasPpq)
        bool   hasPpq = false;
        bool   playing = false;
        int    num = 4, den = 4;
    };

    /** Interpolates the last published transport anchor to "now" using the wall clock. */
    TransportView getTransportNow() const noexcept
    {
        TransportView v;
        const double wall = anchorWallMs.load (std::memory_order_acquire);
        v.sampleRate = juce::jmax (1.0, anchorSampleRate.load (std::memory_order_relaxed));
        v.bpm        = juce::jmax (1.0, anchorBpm.load (std::memory_order_relaxed));
        v.hasPpq     = anchorHasPpq.load (std::memory_order_relaxed);
        v.playing    = anchorPlaying.load (std::memory_order_relaxed);
        v.num        = juce::jmax (1, sigNum.load (std::memory_order_relaxed));
        v.den        = juce::jmax (1, sigDen.load (std::memory_order_relaxed));

        // Don't extrapolate further than 100 ms (host stopped calling processBlock)
        const double elapsedSec = juce::jlimit (0.0, 0.1, (juce::Time::getMillisecondCounterHiRes() - wall) * 0.001);
        v.clock = (double) anchorClock.load (std::memory_order_relaxed) + elapsedSec * v.sampleRate;
        v.ppq   = anchorPpq.load (std::memory_order_relaxed) + (v.playing ? elapsedSec * v.bpm / 60.0 : 0.0);
        return v;
    }

    /** Engine clock of the last published anchor, with its ppq - lets the UI map times to beats. */
    int64_t getAnchorClock() const noexcept { return anchorClock.load (std::memory_order_relaxed); }
    double  getAnchorPpq() const noexcept   { return anchorPpq.load (std::memory_order_relaxed); }

private:
    juce::AbstractFifo fifo { capacity };
    std::array<VisualEvent, capacity> buffer {};
    std::atomic<int> dropped { 0 };

    std::atomic<int64_t> anchorClock { 0 };
    std::atomic<double>  anchorSampleRate { 44100.0 }, anchorBpm { 120.0 }, anchorPpq { 0.0 }, anchorWallMs { 0.0 };
    std::atomic<bool>    anchorHasPpq { false }, anchorPlaying { false };
    std::atomic<int>     sigNum { 4 }, sigDen { 4 };
};

} // namespace bd
