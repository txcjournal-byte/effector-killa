#include "CaptureRecorder.h"
#include <algorithm>
#include <cmath>

namespace bd
{

namespace
{
    constexpr double kJumpBack = 0.25;     // beats: a backward step larger than this is a loop / seek
    constexpr double kJumpForward = 64.0;  // beats: a forward gap larger than this starts a new segment
}

void CaptureRecorder::add (const tools::CapturedEvent& e)
{
    if (segments.empty()
        || e.ppq < segments.back().end - kJumpBack
        || e.ppq > segments.back().end + kJumpForward)
    {
        if (! segments.empty()) segments.back().closed = true;
        Segment s;
        s.start = s.end = e.ppq;
        segments.push_back (std::move (s));
    }

    auto& seg = segments.back();
    seg.events.push_back (e);
    seg.end = std::max (seg.end, e.ppq);

    if (armed)
        recEvents.push_back (e);

    // bound the memory: drop whole old segments first, then the oldest events
    size_t total = 0;
    for (auto& s : segments) total += s.events.size();
    while (segments.size() > 16 || (total > maxEvents && segments.size() > 1))
    {
        total -= segments.front().events.size();
        segments.pop_front();
    }
    if (total > maxEvents)
    {
        auto& evs = segments.front().events;
        evs.erase (evs.begin(), evs.begin() + (std::ptrdiff_t) (total - maxEvents / 2));
        segments.front().start = evs.empty() ? segments.front().end : evs.front().ppq;
    }
}

void CaptureRecorder::clear()
{
    segments.clear();
    recEvents.clear();
}

size_t CaptureRecorder::numEvents() const noexcept
{
    size_t n = 0;
    for (auto& s : segments) n += s.events.size();
    return n;
}

bool CaptureRecorder::captureLast (int bars, int beatsPerBar, int bendRange,
                                   std::vector<PatternLine>& result, double& lengthBeats) const
{
    result.clear();
    if (segments.empty()) return false;

    const double bar = (double) juce::jmax (1, beatsPerBar);

    // The newest segment, unless it just started (the host looped a moment ago): then the pass
    // before it is the one the user means.
    const Segment* seg = &segments.back();
    if (segments.size() > 1 && seg->end - seg->start < bar)
        seg = &segments[segments.size() - 2];

    double endBar = seg->closed ? std::ceil (seg->end / bar - 1.0e-9) * bar
                                : std::floor (seg->end / bar) * bar;
    if (endBar <= seg->start + 1.0e-9)
        endBar = std::ceil (seg->end / bar + 1.0e-9) * bar;

    const double startBar = std::max (std::floor (seg->start / bar) * bar, endBar - bar * juce::jlimit (1, 16, bars));
    if (endBar - startBar < bar - 1.0e-9)
        return false;

    result = tools::linesFromCapture (seg->events, startBar, endBar - startBar, bendRange);
    lengthBeats = endBar - startBar;
    return ! result.empty();
}

//==============================================================================
void CaptureRecorder::arm (double len)
{
    armed = true;
    recording = false;
    havePos = false;
    recLength = std::max (1.0, len);
    recEvents.clear();
}

double CaptureRecorder::nextBoundary (double ppq) const noexcept
{
    // start right now if we are (nearly) on a boundary
    return std::ceil ((ppq - 1.0 / 32.0) / recLength) * recLength;
}

bool CaptureRecorder::poll (double now, bool playing, int bendRange, std::vector<PatternLine>& result)
{
    if (! armed)
        return false;

    if (! playing)
    {
        // waiting for the transport; a stop during recording restarts the pass
        recording = false;
        havePos = false;
        return false;
    }

    const bool wrapped = havePos && now < lastNow - 0.5;
    const double prev = lastNow;
    lastNow = now;

    if (! havePos)
    {
        havePos = true;
        recStart = nextBoundary (now);
        recording = false;
        recEvents.clear();
    }

    if (recording)
    {
        const double end = recStart + recLength;
        const bool done = now >= end + 0.125 || (wrapped && prev >= end - 0.25);

        if (done)
        {
            std::vector<tools::CapturedEvent> evs;
            for (auto& e : recEvents)
                if (e.ppq >= recStart - 1.0e-9) evs.push_back (e);

            result = tools::linesFromCapture (evs, recStart, recLength, bendRange);
            armed = recording = false;
            recEvents.clear();
            return true;
        }

        if (wrapped)
        {
            // looped / jumped before the pass was complete: start over
            recording = false;
            recStart = nextBoundary (now);
            recEvents.clear();
        }
        return false;
    }

    if (wrapped)
    {
        // the host looped back: if it landed on a pass boundary, record from there
        const double b = std::floor (now / recLength) * recLength;
        recStart = now - b < 1.0 ? b : nextBoundary (now);
        recEvents.clear();
    }

    if (now >= recStart)
    {
        recording = true;
        recEvents.erase (std::remove_if (recEvents.begin(), recEvents.end(),
                                         [this] (const tools::CapturedEvent& e) { return e.ppq < recStart - 1.0e-9; }),
                         recEvents.end());
    }

    return false;
}

} // namespace bd
