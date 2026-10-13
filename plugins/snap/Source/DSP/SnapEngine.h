// TrapWin SNAP – transient shaper engine.
//
// Level-independent transient design: four peak envelope followers run on a (stereo-linked) detector signal.
//   attack  amount  <- dB(fast-attack env)  - dB(slow-attack env)   (positive only while an onset rises)
//   sustain amount  <- dB(long-release env) - dB(short-release env) (grows while the sound decays)
// gainDb = attack * kA * attackDiff + sustain * kS * sustainDiff  (clamped, smoothed)
//
// The gain is applied as   y = x + (g - 1) * band   where band is the full signal, the part above the split
// (SVF high-pass) or below it (SVF low-pass). At g == 1 this is bit-exact, and the split always reconstructs.
// Optional lookahead delays the audio (fixed latency, reported) while the detector sees the undelayed input.
// Order: shaper -> optional soft clipper (ceiling) -> dry/wet mix -> output gain.
//
// Plain C++17, no allocation after prepare(), testable headless; wrapped by JUCE in PluginProcessor.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace snapfx
{

enum class Band { full = 0, above = 1, below = 2 };

constexpr double lookaheadSeconds = 0.004;   // 4 ms
constexpr float maxBoostDb = 18.0f, maxCutDb = -30.0f;

struct Params
{
    float attack = 0.0f;       // -1 .. +1   (-100 .. +100 %)
    float sustain = 0.0f;      // -1 .. +1
    Band band = Band::full;
    float splitHz = 200.0f;    // 40 .. 12000
    float speed = 0.5f;        // 0 = slow / smooth, 1 = fast / snappy
    bool lookahead = false;
    bool clip = false;
    float ceilingDb = -0.3f;   // -12 .. 0
    float mix = 1.0f;          // 0 .. 1
    float outputDb = 0.0f;     // -24 .. +12
};

// display history (written by the audio thread, read by the UI)
constexpr int historySize = 512;

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { params = p; }
    const Params& getParams() const { return params; }

    // channels: 1 or 2, in-place
    void process (float* const* channels, int numChannels, int numSamples);

    int getLatencySamples() const { return params.lookahead ? lookaheadSamples : 0; }
    int getLookaheadSamples() const { return lookaheadSamples; }
    float getGainDb() const { return gainDbNow; }       // last smoothed shaper gain
    float getClipAmount() const { return clipPeak; }     // how far the clipper worked in the last block (0..1)

    // history ring: one point per ~5 ms
    std::array<std::atomic<float>, historySize> histIn {}, histOut {}, histGain {};
    std::atomic<int> histWrite { 0 };

private:
    struct SVF
    {
        float g = 0, k = 1.41421356f, a1 = 0, a2 = 0, a3 = 0;
        float ic1 = 0, ic2 = 0;
        void set (float cutoff, float sr);
        void clear() { ic1 = ic2 = 0; }
        inline void tick (float x, float& lp, float& hp)
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            if (ic1 > -1e-20f && ic1 < 1e-20f) ic1 = 0.0f;
            if (ic2 > -1e-20f && ic2 < 1e-20f) ic2 = 0.0f;
            lp = v2;
            hp = x - k * v1 - v2;
        }
    };

    // linear ramp that lands exactly on its target (needed for bit-exact neutral settings)
    struct Ramp
    {
        float v = 0, target = 0, step = 0;
        int left = 0, len = 1;
        void setLength (int n) { len = n < 1 ? 1 : n; }
        void snap (float t) { v = target = t; left = 0; step = 0; }
        inline float next (float t)
        {
            if (t != target)
            {
                target = t;
                left = len;
                step = (target - v) / (float) len;
            }
            if (left > 0)
            {
                if (--left == 0) v = target;
                else v += step;
            }
            return v;
        }
    };

    void updateTimes();

    Params params;
    double sampleRate = 48000.0;
    bool first = true;

    Ramp rAttack, rSustain, rMix, rOut, rLook, rCeil;
    float splitSm = 200.0f, splitCoef = 0.0f, splitSet = -1.0f;
    std::array<SVF, 2> split;
    int coefCounter = 0;

    // envelopes (linear peak)
    float eFast = 0, eSlow = 0, eHold = 0, eShort = 0;
    float aFast = 0, aSlow = 0, rRel = 0, rHold = 0, rShort = 0;
    float lastSpeed = -1.0f;
    float gainSm = 1.0f, gainCoef = 0.0f, gainCoefLa = 0.0f;
    std::vector<float> gainLine;
    int gainSize = 1, gainPos = 0;
    int gainTap[4] = { 0, 0, 0, 0 };   // look-ahead taps (samples ahead of the delayed audio)
    float gainDbNow = 0.0f;

    // lookahead delay
    std::array<std::vector<float>, 2> delay;
    int delaySize = 0, delayPos = 0, lookaheadSamples = 0;

    // display accumulation
    int histHop = 240, histCount = 0;
    float accIn = 0, accOut = 0, accGain = 0;
    float clipPeak = 0.0f;
};

} // namespace snapfx
