// TrapWin DUST TAPE – lo-fi & vintage color engine.
// Plain C++17, no framework dependency, so it can be tested headless and wrapped by JUCE.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace dust
{

struct Params
{
    float age     = 0.0f; // 0..1 macro: pushes every section at once
    float tape    = 0.3f; // 0..1 saturation + head bump
    float wobble  = 0.2f; // 0..1 wow & flutter depth
    float vinyl   = 0.2f; // 0..1 crackle + hiss level
    float crush   = 0.0f; // 0..1 bit depth + sample-rate reduction
    float tone    = 0.5f; // 0..1 dark .. bright (0.5 = neutral-ish)
    float width   = 0.5f; // 0..1 mono .. wide (0.5 = unchanged)
    float mix     = 1.0f; // 0..1 dry/wet
    float outputDb = 0.0f; // -24..+12 dB
    bool  noiseFollow = true; // noise only plays while there is signal
};

// ---------- small building blocks ----------

class OnePole
{
public:
    void setTime (float ms, double sr);
    float process (float target)
    {
        z += coeff * (target - z);
        return z;
    }
    void reset (float v) { z = v; }
    float current() const { return z; }

private:
    float coeff = 1.0f, z = 0.0f;
};

class Biquad
{
public:
    void setLowpass (double sr, double freq, double q);
    void setHighpass (double sr, double freq, double q);
    void setLowShelf (double sr, double freq, double gainDb);
    void setPeak (double sr, double freq, double q, double gainDb);
    void reset() { z1 = z2 = 0.0; }
    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

private:
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2);
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

// 2x oversampler for the saturation stage (half-band FIR up / down).
class Oversampler2x
{
public:
    Oversampler2x();
    void reset();
    // in -> two upsampled samples
    void up (float in, float& a, float& b);
    // two oversampled samples -> one output
    float down (float a, float b);
    static constexpr int latencySamples() { return taps / 2; } // in base-rate samples (approx.)

private:
    static constexpr int taps = 31;
    std::array<float, taps> h {};
    std::array<float, taps> upHist {};
    std::array<float, taps> downHist {};
    int upPos = 0, downPos = 0;
};

class Rng
{
public:
    explicit Rng (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
    uint32_t next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float uniform() { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // 0..1
    float bipolar() { return uniform() * 2.0f - 1.0f; }

private:
    uint32_t s;
};

// ---------- the engine ----------

class Engine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const Params& p) { target = p; }

    // In-place processing; numChannels 1 or 2.
    void process (float* const* channels, int numChannels, int numSamples);

    // Fixed latency of the wet path in samples (dry path is delayed to match).
    int getLatencySamples() const { return latency; }

private:
    struct Effective
    {
        float tape, satAmount, wobble, vinyl, crush, cutoffHz, headBumpDb, drive, width, mix, outGain, noiseLevel;
    };
    Effective computeEffective (const Params& p) const;
    void updateFilters (const Effective& e);
    float readDelay (int ch, float delaySamples) const;

    double sr = 44100.0;
    Params target;

    // smoothed controls
    OnePole sDrive, sWobble, sNoise, sMix, sOut, sWidth, sCrush, sCutLog, sBump, sSat;
    float lastCutoff = -1.0f, lastBump = -100.0f;
    int filterCountdown = 0;
    bool firstBlock = true;

    // per channel state
    struct Channel
    {
        Oversampler2x os;
        Biquad preEmph, deEmph, bump, lowpass, lowpass2, dcBlock;
        std::vector<float> delay;   // wobble delay line
        std::vector<float> dryDelay; // latency match for dry path
        float holdValue = 0.0f;
        float crackleEnv = 0.0f;
        float crackleSign = 1.0f;
        Biquad hissBand, crackleTone;
    };
    std::array<Channel, maxChannels> ch;
    int writePos = 0;
    int delaySize = 0;
    int dryPos = 0;
    int latency = 0;
    float baseDelaySamples = 0.0f;

    // modulation
    double wowPhase = 0.0, flutterPhase = 0.0, driftValue = 0.0, driftTarget = 0.0;
    int driftCounter = 0;
    float holdPhase = 0.0f;

    // noise
    Rng rng { 0xC0FFEEu };
    float envFollower = 0.0f;
    float envAttack = 0.0f, envRelease = 0.0f;
};

} // namespace dust
