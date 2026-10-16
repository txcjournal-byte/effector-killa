/*
    TrapWin BEND DEMON
    VoiceAllocator.h - MPE lower-zone member-channel allocation.

    Behaves like juce::MPEChannelAssigner, but also knows about voices that are still
    sounding after their key was released (glide windows, 808 dives), and can tell the
    caller which voice it had to steal:

      1. If a FREE channel last played the same note number, reuse it (keeps release tails
         of repeated notes on one channel, exactly like MPEChannelAssigner).
      2. Otherwise round-robin: the next free channel after the last one handed out.
      3. Otherwise steal: prefer the oldest *releasing* channel, then the oldest held one.

    Lock-free, allocation-free, audio-thread safe. Not thread-safe (single owner).
*/
#pragma once

#include <array>
#include <cstdint>

namespace bd
{

class VoiceAllocator
{
public:
    static constexpr int masterChannel   = 1;
    static constexpr int maxMemberChans  = 15;   // channels 2..16

    struct Result
    {
        int  channel = -1;   // 2..16
        bool stolen  = false;
    };

    /** numMemberChannels: 1..15 (member channels 2 .. 1 + n). Resets all state. */
    void setNumMemberChannels (int n) noexcept;
    int  getNumMemberChannels() const noexcept { return numMembers; }

    /** Allocates a channel for a note. `stamp` must increase monotonically (e.g. a sample clock). */
    Result allocate (int noteNumber, int64_t stamp) noexcept;

    /** Marks a channel as releasing (key up, voice still sounding: dive / glide window). */
    void markReleasing (int channel) noexcept;

    /** Marks a channel as busy again (e.g. a releasing voice was re-used for a glide). */
    void markHeld (int channel) noexcept;

    /** The note-off has been sent: channel is free again (its last note number is remembered). */
    void free (int channel) noexcept;

    /** Frees everything (keeps round-robin position). */
    void freeAll() noexcept;

    bool isBusy (int channel) const noexcept;
    int  numBusy() const noexcept;

private:
    enum class State : uint8_t { free, held, releasing };

    struct Slot
    {
        State   state     = State::free;
        int     lastNote  = -1;
        int64_t stamp     = 0;     // when the current/last note started
    };

    static int index (int channel) noexcept { return channel - 2; }

    std::array<Slot, maxMemberChans> slots {};
    int numMembers = maxMemberChans;
    int lastAllocated = 1;          // so the first allocation is channel 2
};

} // namespace bd
