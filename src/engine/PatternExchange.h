/*
    TrapWin BEND DEMON
    PatternExchange.h - hands immutable Pattern snapshots from the message thread to the audio
    thread without locks, allocations or deallocations on the audio thread.

        message thread                               audio thread
        --------------                               ------------
        publish (pattern)                            acquire()   (once per block)
          snapshot = new Snapshot {pattern, seq}       p = pending.exchange (nullptr)
          old = pending.exchange (snapshot)            if (p) { current = p; inUseSeq = p->seq; }
          old unconsumed -> freed right away           return current
        collectGarbage()   (timer)
          frees every snapshot with seq < inUseSeq

    Safety argument: the audio thread only ever switches to a pointer it took out of `pending`, and
    its sequence number only grows. A snapshot older than the one the audio thread reports in
    `inUseSeq` can therefore never be read again, so the message thread may delete it. The newest
    published snapshot is always kept (it is either pending or current).
*/
#pragma once

#include "Pattern.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

namespace bd
{

class PatternExchange
{
public:
    struct Snapshot
    {
        Pattern  pattern;
        uint64_t seq = 0;
    };

    PatternExchange() = default;
    ~PatternExchange() = default;

    //==============================================================================
    // Message thread
    void publish (const Pattern& p)
    {
        auto snap = std::make_unique<Snapshot>();
        snap->pattern = p;
        snap->seq = ++lastSeq;
        auto* raw = snap.get();
        owned.push_back (std::move (snap));

        if (auto* old = pending.exchange (raw, std::memory_order_acq_rel))
            release (old);   // never seen by the audio thread

        collectGarbage();
    }

    /** Frees snapshots the audio thread can no longer reach. Call from a timer. */
    void collectGarbage()
    {
        const auto inUse = inUseSeq.load (std::memory_order_acquire);
        owned.erase (std::remove_if (owned.begin(), owned.end(),
                                     [inUse, this] (const std::unique_ptr<Snapshot>& s)
                                     { return s->seq < inUse && s->seq != lastSeq; }),
                     owned.end());
    }

    size_t numOwnedSnapshots() const noexcept { return owned.size(); }

    //==============================================================================
    // Audio thread
    /** Returns the newest snapshot (or nullptr if nothing was ever published). `changed` is set
        when the snapshot differs from the previous call. */
    const Snapshot* acquire (bool& changed) noexcept
    {
        changed = false;
        if (auto* p = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            current = p;
            inUseSeq.store (p->seq, std::memory_order_release);
            changed = true;
        }
        return current;
    }

    const Snapshot* getCurrent() const noexcept { return current; }

private:
    void release (Snapshot* s)
    {
        owned.erase (std::remove_if (owned.begin(), owned.end(),
                                     [s] (const std::unique_ptr<Snapshot>& o) { return o.get() == s; }),
                     owned.end());
    }

    // message thread
    std::vector<std::unique_ptr<Snapshot>> owned;
    uint64_t lastSeq = 0;

    // shared
    std::atomic<Snapshot*> pending { nullptr };
    std::atomic<uint64_t>  inUseSeq { 0 };

    // audio thread
    const Snapshot* current = nullptr;

    JUCE_DECLARE_NON_COPYABLE (PatternExchange)
};

} // namespace bd
