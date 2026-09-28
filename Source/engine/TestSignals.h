#pragma once

// Deterministic test signals: used by KILL loudness estimation (per source) and by the render tests.

#include "Preset.h"

namespace ek
{
namespace TestSignals
{
    enum class Kind { Sine55, PinkNoise, DrumLoop, Vocal, Melody, Bus };

    void generate (Kind k, double sampleRate, float* L, float* R, int numSamples, uint64_t seed = 1);
    Kind forSource (Source s);
}
} // namespace ek
