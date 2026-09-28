#pragma once

// Small, allocation-free DSP building blocks shared by all Effector Killa effects.
// Everything that needs memory allocates in prepare(); process paths never allocate.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ek
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

// Flush denormals / NaN / Inf to zero – used on feedback paths as a final safety net.
inline float sanitize (float x) noexcept
{
    if (! std::isfinite (x)) return 0.0f;
    return std::abs (x) < 1.0e-20f ? 0.0f : x;
}

// Fast tanh (Padé 7/6, clamped) – accurate enough for saturation, very cheap.
inline float fastTanh (float x) noexcept
{
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    const float x2 = x * x;
    return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)))
         / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
}

// ---------------------------------------------------------------------------
// Deterministic RNG (identical results on every platform / standard library).
// ---------------------------------------------------------------------------
struct Rng
{
    uint64_t s = 0x9E3779B97F4A7C15ull;

    explicit Rng (uint64_t seed = 1) noexcept { setSeed (seed); }
    void setSeed (uint64_t seed) noexcept { s = seed * 0x9E3779B97F4A7C15ull + 0xD1B54A32D192ED03ull; next(); }

    uint64_t next() noexcept // splitmix64
    {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uniform() noexcept { return (float) (next() >> 40) * (1.0f / 16777216.0f); } // [0,1)
    float uniform (float lo, float hi) noexcept { return lo + (hi - lo) * uniform(); }
    int integer (int lo, int hiInclusive) noexcept
    {
        const auto span = (uint64_t) (hiInclusive - lo + 1);
        return lo + (int) (next() % span);
    }
    bool chance (float p) noexcept { return uniform() < p; }
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }
};

// Cheap audio-rate white noise (xorshift32), deterministic.
struct NoiseGen
{
    uint32_t state = 22222u;
    float next() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};

// ---------------------------------------------------------------------------
// Parameter smoothing
// ---------------------------------------------------------------------------
class Smoothed
{
public:
    void reset (double sampleRate, float timeMs, float value) noexcept
    {
        coeff = (float) std::exp (-1.0 / (std::max (1.0e-3, (double) timeMs) * 0.001 * sampleRate));
        current = target = value;
    }
    void setTarget (float t) noexcept { target = t; }
    void snap (float v) noexcept { current = target = v; }
    float next() noexcept
    {
        current = target + (current - target) * coeff;
        if (std::abs (current - target) < 1.0e-6f) current = target;
        return current;
    }
    // Advance by n samples at once (for block-rate control values).
    float skip (int n) noexcept
    {
        current = target + (current - target) * std::pow (coeff, (float) n);
        if (std::abs (current - target) < 1.0e-6f) current = target;
        return current;
    }
    float get() const noexcept { return current; }
    float getTarget() const noexcept { return target; }
    bool isSmoothing() const noexcept { return current != target; }

private:
    float coeff = 0.0f, current = 0.0f, target = 0.0f;
};

// Linear ramp (used for crossfades where an exact duration matters).
class LinearRamp
{
public:
    void reset (int lengthSamples, float value) noexcept { len = std::max (1, lengthSamples); cur = tgt = value; remaining = 0; }
    void setTarget (float t) noexcept
    {
        if (t == tgt) return;
        tgt = t; remaining = len; step = (tgt - cur) / (float) len;
    }
    float next() noexcept
    {
        if (remaining > 0) { cur += step; if (--remaining == 0) cur = tgt; }
        return cur;
    }
    float get() const noexcept { return cur; }
    float getTarget() const noexcept { return tgt; }
    bool isRamping() const noexcept { return remaining > 0; }

private:
    int len = 1, remaining = 0;
    float cur = 0.0f, tgt = 0.0f, step = 0.0f;
};

// ---------------------------------------------------------------------------
// Filters
// ---------------------------------------------------------------------------
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float hz, float sr) noexcept { a = std::exp (-kTwoPi * std::min (hz, sr * 0.49f) / sr); }
    float lp (float x) noexcept { z = x + (z - x) * a; return z; }
    float hp (float x) noexcept { return x - lp (x); }
    void reset() noexcept { z = 0.0f; }
};

struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    void prepare (double sr) noexcept { r = (float) std::exp (-kTwoPi * 10.0 / sr); }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = sanitize (y);
        return y1;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
};

// Topology-preserving-transform state variable filter (Zavalishin). Stable under modulation.
struct Svf
{
    float g = 0.0f, k = 1.414f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
    float lpOut = 0.0f, bpOut = 0.0f, hpOut = 0.0f;

    void set (float hz, float q, float sr) noexcept
    {
        hz = juce::jlimit (5.0f, sr * 0.47f, hz);
        g = std::tan (kPi * hz / sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = sanitize (2.0f * v1 - ic1);
        ic2 = sanitize (2.0f * v2 - ic2);
        lpOut = v2; bpOut = v1; hpOut = x - k * v1 - v2;
    }
    float lp (float x) noexcept { tick (x); return lpOut; }
    float hp (float x) noexcept { tick (x); return hpOut; }
    float bp (float x) noexcept { tick (x); return bpOut; }
    float bpNorm (float x) noexcept { tick (x); return bpOut * k; }
    float notch (float x) noexcept { tick (x); return lpOut + hpOut; }
    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

// RBJ biquad (transposed direct form II).
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = sanitize (b1 * x - a1 * y + z2);
        z2 = sanitize (b2 * x - a2 * y);
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
    void copyCoeffs (const Biquad& o) noexcept { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void setPeak (float hz, float q, float gainDb, float sr) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * juce::jlimit (10.0f, sr * 0.45f, hz) / sr;
        const float alpha = std::sin (w) / (2.0f * q);
        const float c = std::cos (w);
        norm (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }
    void setLowShelf (float hz, float gainDb, float sr, float slope = 1.0f) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * juce::jlimit (10.0f, sr * 0.45f, hz) / sr;
        const float c = std::cos (w), s = std::sin (w);
        const float alpha = s / 2.0f * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const float sq = 2 * std::sqrt (A) * alpha;
        norm (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
              (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void setHighShelf (float hz, float gainDb, float sr, float slope = 1.0f) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = kTwoPi * juce::jlimit (10.0f, sr * 0.45f, hz) / sr;
        const float c = std::cos (w), s = std::sin (w);
        const float alpha = s / 2.0f * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2);
        const float sq = 2 * std::sqrt (A) * alpha;
        norm (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
              (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void setHighPass (float hz, float q, float sr) noexcept
    {
        const float w = kTwoPi * juce::jlimit (5.0f, sr * 0.45f, hz) / sr;
        const float alpha = std::sin (w) / (2.0f * q), c = std::cos (w);
        norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }
    void setLowPass (float hz, float q, float sr) noexcept
    {
        const float w = kTwoPi * juce::jlimit (5.0f, sr * 0.45f, hz) / sr;
        const float alpha = std::sin (w) / (2.0f * q), c = std::cos (w);
        norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

private:
    void norm (float nb0, float nb1, float nb2, float na0, float na1, float na2) noexcept
    {
        const float inv = 1.0f / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv; a1 = na1 * inv; a2 = na2 * inv;
    }
};

// 4th-order Linkwitz-Riley crossover: low + high sums to an allpass (flat magnitude).
// Implemented with two identical cascaded TPT SVF Butterworth sections per band.
class LR4Crossover
{
public:
    void prepare (double sr) noexcept { sampleRate = (float) sr; reset(); setCutoff (100.0f, true); }
    void setCutoff (float hz, bool force = false) noexcept
    {
        if (! force && std::abs (hz - cutoff) < 0.01f) return;
        cutoff = hz;
        for (auto* f : { &lp1, &lp2, &hp1, &hp2 })
            f->set (hz, 0.70710678f, sampleRate);
    }
    void process (float x, float& low, float& high) noexcept
    {
        low = lp2.lp (lp1.lp (x));
        high = hp2.hp (hp1.hp (x));
    }
    void reset() noexcept { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    float getCutoff() const noexcept { return cutoff; }

private:
    Svf lp1, lp2, hp1, hp2;
    float sampleRate = 44100.0f, cutoff = -1.0f;
};

// Integer + fractional delay line with power-of-two ring buffer.
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }
    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; }
    void push (float x) noexcept { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }
    // delay >= 1 means the previous pushed sample is at delay 1
    float readInt (int delay) const noexcept { return buffer[(size_t) ((writePos - delay) & mask)]; }
    float readLinear (float delay) const noexcept
    {
        const int i = (int) delay;
        const float f = delay - (float) i;
        const float a = readInt (i), b = readInt (i + 1);
        return a + (b - a) * f;
    }
    float readCubic (float delay) const noexcept
    {
        const int i = (int) delay;
        const float f = delay - (float) i;
        const float y0 = readInt (i - 1), y1 = readInt (i), y2 = readInt (i + 1), y3 = readInt (i + 2);
        const float c0 = y1, c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
    int capacity() const noexcept { return mask + 1; }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

// Fixed-latency compensation delay (stereo).
class LatencyDelay
{
public:
    void prepare (int delaySamples)
    {
        delay = std::max (0, delaySamples);
        for (auto& d : lines) d.prepare (delay + 1);
    }
    void reset() noexcept { for (auto& d : lines) d.reset(); }
    inline float process (int ch, float x) noexcept
    {
        if (delay == 0) return x;
        lines[(size_t) ch].push (x);
        return lines[(size_t) ch].readInt (delay);
    }
    void processBlock (float* L, float* R, int n) noexcept
    {
        if (delay == 0) return;
        for (int i = 0; i < n; ++i) { L[i] = process (0, L[i]); R[i] = process (1, R[i]); }
    }
    int getDelay() const noexcept { return delay; }

private:
    int delay = 0;
    std::array<DelayLine, 2> lines;
};

// Peak/RMS envelope follower with separate attack / release.
struct EnvelopeFollower
{
    float att = 0.0f, rel = 0.0f, env = 0.0f;
    void set (float attackMs, float releaseMs, float sr) noexcept
    {
        att = std::exp (-1.0f / (std::max (0.01f, attackMs) * 0.001f * sr));
        rel = std::exp (-1.0f / (std::max (0.01f, releaseMs) * 0.001f * sr));
    }
    float process (float x) noexcept
    {
        const float a = std::abs (x);
        env = a > env ? a + (env - a) * att : a + (env - a) * rel;
        env = sanitize (env);
        return env;
    }
    void reset() noexcept { env = 0.0f; }
};

// ---------------------------------------------------------------------------
// Tempo sync helpers
// ---------------------------------------------------------------------------
struct SyncDivision { const char* label; float beats; };

// Ordered from longest to shortest. Beat = quarter note.
inline const std::array<SyncDivision, 18>& syncDivisions()
{
    static const std::array<SyncDivision, 18> d { {
        { "4 BAR", 16.0f }, { "2 BAR", 8.0f }, { "1 BAR", 4.0f }, { "1/2D", 3.0f }, { "1/2", 2.0f }, { "1/2T", 4.0f / 3.0f },
        { "1/4D", 1.5f }, { "1/4", 1.0f }, { "1/4T", 2.0f / 3.0f }, { "1/8D", 0.75f }, { "1/8", 0.5f }, { "1/8T", 1.0f / 3.0f },
        { "1/16D", 0.375f }, { "1/16", 0.25f }, { "1/16T", 1.0f / 6.0f }, { "1/32D", 0.1875f }, { "1/32", 0.125f }, { "1/64", 0.0625f } } };
    return d;
}

inline juce::StringArray syncDivisionLabels()
{
    juce::StringArray s;
    for (auto& d : syncDivisions()) s.add (d.label);
    return s;
}

inline int syncIndexOf (const juce::String& label)
{
    const auto& d = syncDivisions();
    for (size_t i = 0; i < d.size(); ++i)
        if (label.equalsIgnoreCase (d[i].label)) return (int) i;
    return -1;
}

} // namespace ek
