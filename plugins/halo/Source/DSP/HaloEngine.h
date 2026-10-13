// TrapWin HALO - lush stereo algorithmic reverb engine (plain C++, no JUCE).
//
// Signal flow (per sample):
//   input -> pre-delay -> hi-cut (SVF LP) -> 4-stage allpass diffusion (per channel)
//         -> 8-line feedback delay network (modulated lines, in-loop allpass, damping, decay gain,
//            8x8 Hadamard mixing) -> low-cut (SVF HP) -> width (M/S) -> ducking -> mix -> output
//
// Real-time safe: all memory is allocated in prepare(); process() never allocates or locks.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace halo
{

struct Params
{
    float predelayMs = 20.0f;   // 0 .. 500 ms
    float size = 0.6f;          // 0 .. 1
    float decaySec = 3.5f;      // 0.2 .. 30 s (RT60)
    float diffusion = 0.8f;     // 0 .. 1
    float mod = 0.4f;           // 0 .. 1
    float hiCutHz = 9000.0f;    // 1 k .. 20 k (input low-pass + in-loop damping)
    float lowCutHz = 150.0f;    // 20 .. 1000 Hz (wet high-pass)
    float width = 1.0f;         // 0 .. 1.5
    float duck = 0.0f;          // 0 .. 1 (wet ducks under the dry input)
    bool freeze = false;        // infinite hold
    float mix = 0.35f;          // 0 .. 1
    float outputDb = 0.0f;      // -24 .. +12
};

class Engine
{
public:
    static constexpr int numLines = 8;
    static constexpr float maxPredelayMs = 500.0f;
    static constexpr float minDecay = 0.2f, maxDecay = 30.0f;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const Params& p) { target = p; }
    void process (float* const* channels, int numChannels, int numSamples);
    int getLatencySamples() const { return 0; }

    // meters (audio thread writes, read from anywhere; approximate)
    float getInputLevel() const { return inLevel; }
    float getWetLevel() const { return wetLevel; }
    float getDuckGain() const { return duckGainNow; }   // 1 = no ducking

    // helpers used by the tests / UI
    static float sizeToScale (float size);

private:
    struct Smooth
    {
        float v = 0.0f, t = 0.0f, c = 1.0f;
        void setTime (double sr, double seconds) { c = seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * sr))); }
        void snap (float x) { v = t = x; }
        inline float next()
        {
            v += c * (t - v);
            if (std::abs (t - v) < 1.0e-7f) v = t;
            return v;
        }
    };

    struct Allpass
    {
        std::vector<float> buf;
        int len = 1, pos = 0;
        void init (int length) { len = length < 1 ? 1 : length; buf.assign ((size_t) len, 0.0f); pos = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
        inline float process (float x, float g)
        {
            const float d = buf[(size_t) pos];
            const float v = x - g * d;
            buf[(size_t) pos] = v;
            if (++pos >= len) pos = 0;
            return d + g * v;
        }
    };

    struct ModDelay
    {
        std::vector<float> buf;
        int mask = 0, w = 0;
        void init (int minLength);
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        inline void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        inline float readCubic (float delay) const;   // delay >= 3 samples
        inline float readLinear (float delay) const;  // delay >= 1 sample
    };

    struct Svf   // TPT state variable filter (stable under modulation)
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void clear() { ic1 = ic2 = 0.0f; }
        inline float lp (float x, float g, float k)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            return v2;
        }
        inline float hp (float x, float g, float k)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            return x - k * v1 - v2;
        }
    };

    void updateControl();   // once per control chunk

    double sr = 48000.0;
    bool prepared = false;
    Params target;

    // pre-delay
    ModDelay preL, preR;
    Smooth preSamples;

    // input conditioning
    Svf inLpL, inLpR;
    std::array<Allpass, 4> diffL, diffR;
    Smooth diffCoef;

    // tank
    std::array<ModDelay, numLines> lines;
    std::array<Allpass, numLines> loopAp;
    std::array<float, numLines> baseDelay {};     // samples at scale 1
    std::array<float, numLines> damp {};          // one-pole low-pass state
    std::array<Smooth, numLines> gain;
    std::array<float, numLines> lfoS {}, lfoC {}, lfoRotS {}, lfoRotC {};
    Smooth sizeScale, modDepth, dampCoef, inGain, loopApCoef;

    // output
    Svf hpL, hpR;
    float lpG = 1.0f, hpG = 0.1f;   // SVF coefficients (updated per chunk)
    Smooth widthS, dryS, wetS, outS;

    // ducking
    float duckEnv = 0.0f, duckAtt = 0.0f, duckRel = 0.0f, duckUp = 0.0f;
    Smooth duckGain;
    float duckGainNow = 1.0f;

    // misc
    int controlCounter = 0;
    int lfoRenorm = 0;
    float denormFlip = 1.0e-20f;
    float inLevel = 0.0f, wetLevel = 0.0f;
    float wetNorm = 1.0f;
};

} // namespace halo
