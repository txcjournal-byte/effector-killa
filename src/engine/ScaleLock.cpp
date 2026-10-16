#include "ScaleLock.h"
#include <cmath>

namespace bd
{

namespace
{
    const std::array<std::array<int, 13>, ScaleLock::numScaleTypes>& scaleTable()
    {
        // -1 terminated lists
        static const std::array<std::array<int, 13>, ScaleLock::numScaleTypes> t {{
            {{ 0, 2, 4, 5, 7, 9, 11, -1 }},              // major
            {{ 0, 2, 3, 5, 7, 8, 10, -1 }},              // natural minor
            {{ 0, 2, 3, 5, 7, 8, 11, -1 }},              // harmonic minor
            {{ 0, 1, 3, 5, 7, 8, 10, -1 }},              // phrygian
            {{ 0, 2, 3, 5, 7, 9, 10, -1 }},              // dorian
            {{ 0, 2, 4, 7, 9, -1 }},                     // major pentatonic
            {{ 0, 3, 5, 7, 10, -1 }},                    // minor pentatonic
            {{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, -1 }} // chromatic
        }};
        return t;
    }
}

const char* const* ScaleLock::getScaleNames() noexcept
{
    static const char* const names[] = { "Major", "Minor", "Harmonic Minor", "Phrygian", "Dorian",
                                         "Major Pentatonic", "Minor Pentatonic", "Chromatic", nullptr };
    return names;
}

const char* const* ScaleLock::getKeyNames() noexcept
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", nullptr };
    return names;
}

void ScaleLock::set (int newKey, int newType) noexcept
{
    key  = ((newKey % 12) + 12) % 12;
    type = juce::jlimit (0, (int) numScaleTypes - 1, newType);

    mask.fill (false);
    for (auto step : scaleTable()[(size_t) type])
    {
        if (step < 0) break;
        mask[(size_t) ((key + step) % 12)] = true;
    }
}

bool ScaleLock::isInScale (int note) const noexcept
{
    return mask[(size_t) (((note % 12) + 12) % 12)];
}

int ScaleLock::snap (int note) const noexcept
{
    if (isInScale (note))
        return note;

    for (int d = 1; d <= 6; ++d)
    {
        if (isInScale (note - d)) return note - d;   // ties go down
        if (isInScale (note + d)) return note + d;
    }

    return note;
}

float ScaleLock::snapPitch (float pitch) const noexcept
{
    const int lower = (int) std::floor (pitch);

    // Search outward from the nearest integer for the closest scale tone.
    float best = pitch;
    float bestDist = 1.0e9f;

    for (int n = lower - 6; n <= lower + 7; ++n)
    {
        if (! isInScale (n)) continue;
        const float dist = std::abs ((float) n - pitch);
        if (dist < bestDist - 1.0e-6f) { best = (float) n; bestDist = dist; }
    }

    return best;
}

int ScaleLock::stepDegrees (int note, int steps) const noexcept
{
    int n = snap (note);
    const int dir = steps > 0 ? 1 : -1;

    for (int i = 0; i < std::abs (steps); ++i)
    {
        int guard = 0;
        do { n += dir; } while (! isInScale (n) && ++guard < 12);
    }

    return n;
}

float ScaleLock::snapOffsetTarget (int note, float semitoneOffset) const noexcept
{
    return snapPitch ((float) note + semitoneOffset);
}

} // namespace bd
