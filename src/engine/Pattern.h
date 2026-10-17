/*
    TrapWin BEND DEMON
    Pattern.h - the 2.0 line sequencer data model.

    A Pattern is 1..16 bars long and holds Lines. A Line is one MPE note:

        start / length   in beats (quarter notes) from the pattern start
        pitch            a curve of (t, semitone) points, t in beats relative to the line start,
                         semitones as MIDI note numbers with free fractional pitch (60.5 = C4 + 50 ct)
        smooth           true  = monotone cubic interpolation (Fritsch-Carlson: passes through every
                                 point, never overshoots between them, so a hold stays a hold)
                         false = straight segments (what the LINE tool draws)
        velocity         0..1
        pressure, timbre optional (t, 0..1) curves, linear; empty = a constant default
        pitch mod        a per-note LFO on pitch ("vibrato"): shape (sine / triangle / square /
                         saw up / saw down), depth (semitones), rate (cycles per beat, so it follows
                         the tempo), fade-in (beats), bipolar or unipolar (only upwards)
        releaseVelocity  note-off velocity 0..1

    Everything here is plain value types (copyable, comparable) so the UI can keep undo copies and
    the audio thread can read an immutable snapshot. Evaluation functions never allocate.
*/
#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace bd
{

struct CurvePoint
{
    double t = 0.0;   // beats from the line start
    float  v = 0.0f;  // value (semitones for pitch, 0..1 for expression)

    bool operator== (const CurvePoint& o) const noexcept { return juce::exactlyEqual (t, o.t) && juce::exactlyEqual (v, o.v); }
    bool operator!= (const CurvePoint& o) const noexcept { return ! operator== (o); }
};

using Curve1D = std::vector<CurvePoint>;

namespace curves
{
    /** Straight-segment interpolation, clamped to the first/last value outside the points. */
    float evalLinear (const Curve1D&, double t) noexcept;

    /** Monotone cubic Hermite (Fritsch-Carlson). Clamped outside the points. */
    float evalMonotone (const Curve1D&, double t) noexcept;

    /** Sorts by time and merges points closer than `minDt` (keeps the later one). */
    void normalise (Curve1D&, double minDt = 1.0e-6);

    /** Ramer-Douglas-Peucker simplification with vertical (value) error <= `tolerance` against
        straight segments. Keeps the first and last point. */
    Curve1D simplify (const Curve1D&, float tolerance);
}

//==============================================================================
struct PatternLine
{
    static constexpr float defaultPressure = 0.75f;
    static constexpr float defaultTimbre   = 0.5f;

    uint32_t id      = 0;
    double   start   = 0.0;      // beats
    double   length  = 1.0;      // beats (> 0)
    bool     smooth  = true;
    float    velocity = 0.8f;    // 0..1

    Curve1D  pitch;              // at least one point
    Curve1D  pressure;           // empty = defaultPressure
    Curve1D  timbre;             // empty = defaultTimbre

    float    releaseVelocity = 0.5f;   // 0..1 (note-off velocity)

    enum LfoShape : int { sine = 0, triangle, square, sawUp, sawDown, numLfoShapes };
    float    vibDepth = 0.0f;    // semitones (0 = off)
    float    vibRate  = 2.5f;    // cycles per beat
    float    vibFade  = 0.5f;    // beats until full depth
    int      vibShape = sine;
    bool     vibUnipolar = false;

    /** LFO value -1..1 (bipolar) or 0..1 (unipolar) for a phase in cycles. */
    float lfoValue (double cycles) const noexcept;

    double end() const noexcept  { return start + length; }

    /** Pitch at `tRel` beats after the line start, including vibrato. */
    float pitchAt (double tRel) const noexcept;
    /** Pitch without vibrato (the drawn curve). */
    float curvePitchAt (double tRel) const noexcept;
    float pressureAt (double tRel) const noexcept;
    float timbreAt (double tRel) const noexcept;

    float startPitch() const noexcept   { return curvePitchAt (0.0); }
    void  pitchRange (float& lo, float& hi) const noexcept;

    /** Sorts curves, clamps values and makes sure there is a pitch point at t = 0. */
    void normalise();

    bool operator== (const PatternLine&) const noexcept;
    bool operator!= (const PatternLine& o) const noexcept { return ! operator== (o); }
};

//==============================================================================
struct Pattern
{
    static constexpr int minBars = 1, maxBars = 64, defaultBars = 4;

    int bars        = defaultBars;
    int beatsPerBar = 4;            // quarter-note beats per bar (4/4)
    std::vector<PatternLine> lines;
    uint32_t nextId = 1;

    double lengthBeats() const noexcept   { return (double) juce::jlimit (minBars, maxBars, bars) * (double) juce::jmax (1, beatsPerBar); }

    /** Adds a line, assigning a fresh id. Returns the id. */
    uint32_t add (PatternLine);

    PatternLine*       find (uint32_t id) noexcept;
    const PatternLine* find (uint32_t id) const noexcept;
    bool remove (uint32_t id);

    void normalise();               // normalises every line, sorts by start, repairs ids

    std::unique_ptr<juce::XmlElement> toXml() const;
    static Pattern fromXml (const juce::XmlElement&);
    static constexpr const char* xmlTag = "PATTERN";

    bool operator== (const Pattern&) const noexcept;
    bool operator!= (const Pattern& o) const noexcept { return ! operator== (o); }
};

} // namespace bd
