#pragma once

#include "engine/Engine.h"
#include "engine/Randomizer.h"
#include "engine/TestSignals.h"
#include "EKBinaryData.h"

namespace ektest
{
using namespace ek;

inline bool allFinite (const float* x, int n, float limit = 1000.0f)
{
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (x[i]) || std::abs (x[i]) > limit) return false;
    return true;
}

inline double rms (const float* x, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / std::max (1, n));
}

// Goertzel magnitude of frequency f
inline double toneMagnitude (const float* x, int n, double f, double sr)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
    const double c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double s0 = x[i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / (n * 0.5);
}

inline PresetBank& factoryBank()
{
    static PresetBank bank = []
    {
        PresetBank b;
        b.loadFactory (juce::String::fromUTF8 (EKData::factory_json, EKData::factory_jsonSize));
        return b;
    }();
    return bank;
}

// Runs a program through a Chain (macros applied) in chunks of `block` samples.
inline void renderProgram (const ProgramState& p, double sr, int osLog2, float* L, float* R, int n, int block = 256)
{
    PrepareSpec spec;
    spec.sampleRate = sr;
    spec.osFactorLog2 = osLog2;
    Chain chain (p.rackConfig(), spec, Rack::computeFixedLatency (sr, osLog2));
    RackParams base, eff;
    for (int s = 0; s < kNumSlots; ++s)
    {
        base.slots[(size_t) s].p = p.slots[(size_t) s].p;
        base.slots[(size_t) s].mix = p.slots[(size_t) s].mix;
        base.slots[(size_t) s].pause = p.slots[(size_t) s].pause;
    }
    MacroEngine::compute (p.rackConfig(), base, p.macros, p.maps, p.source, eff);
    ProcessContext ctx;
    ctx.sampleRate = sr;
    for (int pos = 0; pos < n;)
    {
        const int len = std::min ({ block, n - pos, kMaxChunk });
        chain.process (L + pos, R + pos, len, &eff, ctx);
        ctx.ppq += len * ctx.beatsPerSample();
        pos += len;
    }
}
} // namespace ektest
