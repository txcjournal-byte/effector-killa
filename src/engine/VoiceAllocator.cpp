#include "VoiceAllocator.h"
#include <algorithm>

namespace bd
{

void VoiceAllocator::setChannelRange (int first, int last) noexcept
{
    firstCh    = std::clamp (first, 2, 16);
    numMembers = std::clamp (last - firstCh + 1, 1, 17 - firstCh);
    slots = {};
    lastAllocated = firstCh - 1;
}

VoiceAllocator::Result VoiceAllocator::allocate (int noteNumber, int64_t stamp) noexcept
{
    Result r;
    const int lastCh = firstCh + numMembers - 1;

    auto take = [&] (int ch)
    {
        auto& s = slots[(size_t) index (ch)];
        s.state    = State::held;
        s.lastNote = noteNumber;
        s.stamp    = stamp;
        lastAllocated = ch;
        r.channel = ch;
    };

    // 1. free channel that last played this very note
    for (int ch = firstCh; ch <= lastCh; ++ch)
    {
        const auto& s = slots[(size_t) index (ch)];
        if (s.state == State::free && s.lastNote == noteNumber)
        {
            take (ch);
            return r;
        }
    }

    // 2. round robin over free channels, starting after the last allocated one
    for (int i = 1; i <= numMembers; ++i)
    {
        const int ch = firstCh + ((lastAllocated - firstCh + i) % numMembers + numMembers) % numMembers;
        if (slots[(size_t) index (ch)].state == State::free)
        {
            take (ch);
            return r;
        }
    }

    // 3. steal: oldest releasing voice first, otherwise oldest held voice
    int victim = -1;
    for (int pass = 0; pass < 2 && victim < 0; ++pass)
    {
        const auto wanted = pass == 0 ? State::releasing : State::held;
        int64_t oldest = INT64_MAX;

        for (int ch = firstCh; ch <= lastCh; ++ch)
        {
            const auto& s = slots[(size_t) index (ch)];
            if (s.state == wanted && s.stamp < oldest)
            {
                oldest = s.stamp;
                victim = ch;
            }
        }
    }

    if (victim < 0)
        victim = firstCh;   // unreachable in practice

    take (victim);
    r.stolen = true;
    return r;
}

void VoiceAllocator::markReleasing (int channel) noexcept
{
    if (channel >= firstCh && channel < firstCh + numMembers)
        slots[(size_t) index (channel)].state = State::releasing;
}

void VoiceAllocator::markHeld (int channel) noexcept
{
    if (channel >= firstCh && channel < firstCh + numMembers)
        slots[(size_t) index (channel)].state = State::held;
}

void VoiceAllocator::free (int channel) noexcept
{
    if (channel >= firstCh && channel < firstCh + numMembers)
        slots[(size_t) index (channel)].state = State::free;
}

void VoiceAllocator::freeAll() noexcept
{
    for (auto& s : slots)
        s.state = State::free;
}

bool VoiceAllocator::isBusy (int channel) const noexcept
{
    return channel >= firstCh && channel < firstCh + numMembers
        && slots[(size_t) index (channel)].state != State::free;
}

int VoiceAllocator::numBusy() const noexcept
{
    int n = 0;
    for (int i = 0; i < numMembers; ++i)
        if (slots[(size_t) index (firstCh + i)].state != State::free)
            ++n;
    return n;
}

} // namespace bd
