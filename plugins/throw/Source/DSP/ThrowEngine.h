// TrapWin THROW – vocal throw / tape-style delay engine.
//
// Two circular delay lines (one per side). Every sample:
//   y   = read (line, delay + wow/flutter)            (4-point Hermite, fractional)
//   w   = loop (send * input + feedback * y)          (HP/LP filters, tape saturation, guard clipper)
//   buf = mix (w, y, freeze)                          (freeze = feedback 1, no new input, no colour)
// The delay time is glided (one-pole) towards its target, so time changes bend the pitch like tape.
// Ping-Pong feeds the mono input into the left line and cross-feeds the lines.
// The wet signal is ducked by an envelope follower on the dry input, widened (M/S) and mixed.
//
// Plain C++17, no allocation after prepare(), so it can be tested headless; wrapped by JUCE in PluginProcessor.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace throwfx
{

enum class Mode { stereo = 0, pingPong = 1, mono = 2 };

constexpr int numDivisions = 17;
constexpr std::array<const char*, numDivisions> divisionNames {
    "1/32T", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/1T", "1/2D", "1/1"
};
// length in quarter notes
constexpr std::array<double, numDivisions> divisionBeats {
    1.0 / 12.0, 0.125, 1.0 / 6.0, 0.1875, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 8.0 / 3.0, 3.0, 4.0
};
constexpr int defaultDivision = 10; // 1/4

constexpr double maxDelaySeconds = 4.0;
constexpr float minFreeMs = 10.0f, maxFreeMs = 2000.0f;
constexpr float maxFeedback = 1.1f;

struct Params
{
    bool sync = true;
    int division = defaultDivision;
    float timeMs = 350.0f;       // used when sync == false
    double bpm = 120.0;          // host tempo
    Mode mode = Mode::stereo;
    float feedback = 0.35f;      // 0 .. 1.1 (above 1 = self-oscillation, guarded)
    float drive = 0.0f;          // tape saturation 0..1
    float wobble = 0.0f;         // wow & flutter 0..1
    float lowCutHz = 20.0f;      // 20 = off
    float highCutHz = 20000.0f;  // 20000 = off
    float duck = 0.0f;           // 0..1
    bool throwOnly = false;      // input reaches the delay only while throwOn
    bool throwOn = false;
    bool freeze = false;
    float width = 1.0f;          // 0..2 (wet only)
    float mix = 0.5f;            // 0 = dry, 0.5 = dry + full wet, 1 = wet only
    float outputDb = 0.0f;
};

// delay time in milliseconds for the given params (clamped to the buffer)
double delayMsFor (const Params& p);

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { params = p; }
    const Params& getParams() const { return params; }

    // channels: 1 or 2; in-place
    void process (float* const* channels, int numChannels, int numSamples);

    int getLatencySamples() const { return 0; }
    float getWetLevel() const { return wetLevel; }     // peak of the wet signal in the last block
    float getDuckGain() const { return duckGainNow; }  // 1 = no ducking
    float getSendGate() const { return sendGate; }
    double getCurrentDelayMs() const { return delaySmp / sampleRate * 1000.0; }

private:
    struct SVF
    {
        float g = 0, k = 1.41421356f, a1 = 0, a2 = 0, a3 = 0;
        float ic1 = 0, ic2 = 0;
        void set (float cutoff, float sr);
        void clear() { ic1 = ic2 = 0; }
        // returns lowpass, hp = x - k*bp - lp
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

    struct Smooth
    {
        float v = 0, coef = 0;
        void setTime (double seconds, double sr);
        inline float next (float target) { v += coef * (target - v); return v; }
    };

    float readLine (int line, double delay) const;
    float loopProcess (int line, float x);
    void updateCoefficients();

    Params params;
    double sampleRate = 48000.0;
    std::array<std::vector<float>, 2> lines;
    int bufSize = 0, bufMask = 0, writePos = 0;

    double delaySmp = 0.0, glideCoef = 0.0;
    bool first = true;

    Smooth sFeedback, sDrive, sWobble, sLowCut, sHighCut, sDuck, sWidth, sDry, sWet, sOut, sSend, sFreeze, sDuckGain;
    float sendGate = 0.0f;

    std::array<SVF, 2> hp, lp;
    std::array<float, 2> tapeLp {};
    float tapeCoef = 0.0f, hpBlend = 0.0f, lpBlend = 0.0f;
    int coefCounter = 0;

    double wowPhase = 0.0, flutterPhase = 0.0;
    float drift = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
    uint32_t rng = 0x1234567u;

    float env = 0.0f, envAtt = 0.0f, envRel = 0.0f;
    float wetLevel = 0.0f, duckGainNow = 1.0f;
};

} // namespace throwfx
