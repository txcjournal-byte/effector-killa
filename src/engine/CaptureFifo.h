/*
    TrapWin BEND DEMON
    CaptureFifo.h - lock-free audio -> UI queue of the MPE that the live gesture engine produced
    (ppq-stamped), feeding CAPTURE. Producer: audio thread. Consumer: message thread (timer).
*/
#pragma once

#include "PatternTools.h"
#include <array>
#include <atomic>

namespace bd
{

class CaptureFifo
{
public:
    static constexpr int capacity = 1 << 15;

    /** Audio thread: records the MPE messages of one chunk. `ppqAtStart` = musical position of
        sample 0, `beatsPerSample` converts sample positions. */
    void push (const juce::MidiBuffer& mpe, double ppqAtStart, double beatsPerSample) noexcept
    {
        for (const auto meta : mpe)
        {
            const auto& m = meta.getMessage();
            tools::CapturedEvent e;
            e.ppq = ppqAtStart + (double) meta.samplePosition * beatsPerSample;
            e.channel = (uint8_t) juce::jlimit (1, 16, m.getChannel());

            if (m.isNoteOn())                { e.type = tools::CapturedEvent::noteOn; e.note = (uint8_t) m.getNoteNumber(); e.velocity = m.getVelocity(); }
            else if (m.isNoteOff())          { e.type = tools::CapturedEvent::noteOff; e.note = (uint8_t) m.getNoteNumber(); }
            else if (m.isPitchWheel())       { e.type = tools::CapturedEvent::bend; e.value = (int16_t) m.getPitchWheelValue(); }
            else if (m.isChannelPressure())  { e.type = tools::CapturedEvent::pressure; e.value = (int16_t) m.getChannelPressureValue(); }
            else if (m.isControllerOfType (74)) { e.type = tools::CapturedEvent::timbre; e.value = (int16_t) m.getControllerValue(); }
            else continue;

            int s1, n1, s2, n2;
            fifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 > 0)      buffer[(size_t) s1] = e;
            else if (n2 > 0) buffer[(size_t) s2] = e;
            else             return;   // UI not draining: drop
            fifo.finishedWrite (1);
        }
    }

    /** Message thread. */
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

private:
    juce::AbstractFifo fifo { capacity };
    std::array<tools::CapturedEvent, capacity> buffer {};
};

} // namespace bd
