/*
    TrapWin BEND DEMON
    CaptureRecorder.h - CAPTURE (2.0): turns what the live gesture engine played (the DAW's piano
    roll notes, with all their 1.x glides, dives and vibrato as MPE) into editable lines.

    Two ways:
      * captureLast (N bars)  from a rolling buffer of everything played recently
      * arm()                 records exactly one pass of the pattern length, starting on the
                              next pattern-length boundary (or right after the host loops back),
                              then converts it automatically

    The rolling buffer is split into "segments" wherever the musical clock jumps (host loop, seek,
    restart), so a looping FL Studio pattern does not mix two passes into one capture.

    Message thread only (events arrive through CaptureFifo). No GUI dependencies -> unit tested.
*/
#pragma once

#include "PatternTools.h"
#include <deque>
#include <vector>

namespace bd
{

class CaptureRecorder
{
public:
    static constexpr size_t maxEvents = 400000;

    void add (const tools::CapturedEvent&);
    void clear();
    size_t numEvents() const noexcept;

    /** Lines from the last `bars` bars that contain playing. Returns false if there is nothing. */
    bool captureLast (int bars, int beatsPerBar, int bendRange,
                      std::vector<PatternLine>& result, double& lengthBeats) const;

    //==============================================================================
    void arm (double lengthBeats);
    void disarm() noexcept                 { armed = false; recording = false; }
    bool isArmed() const noexcept          { return armed; }
    bool isRecording() const noexcept      { return armed && recording; }
    double getRecordStart() const noexcept { return recStart; }
    double getRecordLength() const noexcept { return recLength; }

    /** Call regularly with the current musical position. Returns true once when an armed pass
        has completed; the converted lines are written to `result`. */
    bool poll (double nowPpq, bool playing, int bendRange, std::vector<PatternLine>& result);

private:
    struct Segment
    {
        double start = 0.0, end = 0.0;
        bool closed = false;
        std::vector<tools::CapturedEvent> events;
    };

    double nextBoundary (double ppq) const noexcept;

    std::deque<Segment> segments;

    bool   armed = false, recording = false, havePos = false;
    double recStart = 0.0, recLength = 16.0, lastNow = 0.0;
    std::vector<tools::CapturedEvent> recEvents;
};

} // namespace bd
