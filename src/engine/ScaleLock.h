/*
    TrapWin BEND DEMON
    ScaleLock.h - key + scale helpers used to land gesture targets on scale tones.
*/
#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace bd
{

class ScaleLock
{
public:
    enum ScaleType
    {
        major = 0,
        minor,
        harmonicMinor,
        phrygian,
        dorian,
        majorPentatonic,
        minorPentatonic,
        chromatic,
        numScaleTypes
    };

    static const char* const* getScaleNames() noexcept;
    static const char* const* getKeyNames() noexcept;

    ScaleLock() { set (0, chromatic); }

    /** key: 0 = C ... 11 = B. */
    void set (int key, int scaleType) noexcept;

    int getKey() const noexcept       { return key; }
    int getScaleType() const noexcept { return type; }

    /** True if the MIDI note (any octave) belongs to the scale. */
    bool isInScale (int note) const noexcept;

    /** Nearest scale tone. Ties resolve downwards (towards the lower note). */
    int snap (int note) const noexcept;

    /** Snaps a fractional pitch (in semitones/MIDI note units) to the nearest scale tone. */
    float snapPitch (float pitch) const noexcept;

    /** The scale tone `steps` degrees above (steps > 0) or below (steps < 0) `note`.
        If `note` is not in the scale it is snapped first. */
    int stepDegrees (int note, int steps) const noexcept;

    /** Scale tone nearest to note + semitoneOffset (used for dive / rise targets). */
    float snapOffsetTarget (int note, float semitoneOffset) const noexcept;

private:
    int key = 0, type = chromatic;
    std::array<bool, 12> mask {};
};

} // namespace bd
