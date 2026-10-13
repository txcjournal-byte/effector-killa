// TrapWin GHOST DELAY – multi-mode creative delay engine.
//
// One pair of long circular delay lines is shared by seven mode paths. Each mode decides how the lines are
// read (taps, grains, modulation), what goes back into the loop and how the loop is coloured:
//
//   TAPE     wow / flutter / drift on the read head, asymmetric tape saturation + head-gap HF loss, hiss
//   BBD      triangle-LFO chorus read, clock-dependent anti-alias low-pass (longer = darker), compander grit
//   DIGITAL  pristine Hermite reads; AGE = bit depth + sample-rate reduction inside the loop
//   GHOST    pitch shifter (+12 / +7 / -12) in the feedback path: every repeat climbs / falls further
//   REVERSE  tempo-locked grains (grain = delay time) played backwards, aligned to the host bar grid
//   DIFFUSE  four modulated allpasses per side in the loop: every repeat smears further into ambience
//   RHYTHM   mono-in multi-tap patterns (dotted / triplet / swing), panned taps, loop from the last tap
//
// Shared: synced (1/64 .. 2 bars, dotted / triplet) or free time, L/R ratio, feedback up to 110 % with a guard
// clipper (bounded self-oscillation), HP / LP inside the loop, modulation depth / rate, AGE, ducking, freeze,
// mix and output. Delay time changes glide like tape (pitch bend). Mode changes fade the wet signal out / in.
//
// Plain C++17, no allocation after prepare(), testable headless; wrapped by JUCE in PluginProcessor.
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace ghostdelay
{

enum class Mode { tape = 0, bbd = 1, digital = 2, ghost = 3, reverse = 4, diffuse = 5, rhythm = 6 };
constexpr int numModes = 7;
constexpr std::array<const char*, numModes> modeNames { "TAPE", "BBD ANALOG", "DIGITAL", "GHOST", "REVERSE", "DIFFUSE", "RHYTHM" };

constexpr int numGhostPitches = 3;
constexpr std::array<const char*, numGhostPitches> ghostPitchNames { "+12", "+7", "-12" };
constexpr std::array<double, numGhostPitches> ghostPitchRatios { 2.0, 1.4983070768766815, 0.5 };

constexpr int numPatterns = 3;
constexpr std::array<const char*, numPatterns> patternNames { "DOTTED", "TRIPLET", "SWING" };

constexpr int numDivisions = 23;
constexpr std::array<const char*, numDivisions> divisionNames {
    "1/64T", "1/64", "1/32T", "1/64D", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8", "1/4T",
    "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/1T", "1/2D", "1 BAR", "2/1T", "1 BAR D", "2 BARS"
};
// length in quarter notes
constexpr std::array<double, numDivisions> divisionBeats {
    1.0 / 24.0, 1.0 / 16.0, 1.0 / 12.0, 3.0 / 32.0, 0.125, 1.0 / 6.0, 0.1875, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0,
    0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 8.0 / 3.0, 3.0, 4.0, 16.0 / 3.0, 6.0, 8.0
};
constexpr int defaultDivision = 13; // 1/4

constexpr double maxDelaySeconds = 8.0;
constexpr float minFreeMs = 2.0f, maxFreeMs = 4000.0f;
constexpr float maxFeedback = 1.1f;
constexpr float minModRate = 0.05f, maxModRate = 10.0f;

struct Params
{
    Mode mode = Mode::tape;
    bool sync = true;
    int division = defaultDivision;
    float timeMs = 375.0f;       // used when sync == false
    double bpm = 120.0;          // host tempo
    double ppq = -1.0;           // host position in quarter notes (< 0 = unknown); locks REVERSE grains to the grid
    bool playing = false;
    float lrRatio = 0.0f;        // -1..1: < 0 shortens the left time, > 0 the right (down to half)
    float feedback = 0.4f;       // 0 .. 1.1 (above 1 = self-oscillation, guarded)
    float lowCutHz = 20.0f;      // in the loop, 20 = off
    float highCutHz = 20000.0f;  // in the loop, 20000 = off
    float modDepth = 0.0f;       // 0..1
    float modRate = 0.8f;        // Hz
    float age = 0.0f;            // 0..1, mode-specific wear
    float duck = 0.0f;           // 0..1
    bool freeze = false;
    int ghostPitch = 0;          // index into ghostPitchRatios
    int pattern = 0;             // RHYTHM pattern
    float mix = 0.5f;            // 0 = dry, 0.5 = dry + full wet, 1 = wet only
    float outputDb = 0.0f;
};

// base delay time in milliseconds (before the L/R ratio)
double delayMsFor (const Params& p);
// time factor per side from the L/R ratio (1 = base time, 0.5 = half)
void ratioFactors (float lrRatio, double& left, double& right);

struct RhythmPattern
{
    int numTaps;
    std::array<double, 4> times;   // in units of the base time
    std::array<float, 4> gains;
    std::array<float, 4> pans;     // -1 = left, +1 = right
    double loop;                   // feedback is taken from here (base-time units)
};
const RhythmPattern& rhythmPattern (int index);

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
    float getWetLevel() const { return wetLevel; }
    float getDuckGain() const { return duckGainNow; }
    double getCurrentDelayMs() const { return delaySmp[0] / sampleRate * 1000.0; }
    Mode getActiveMode() const { return activeMode; }
    double getReversePhase() const { return chans[0].revPhase; }

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

    struct Smooth
    {
        float v = 0, coef = 0;
        void setTime (double seconds, double sr);
        inline float next (float target)
        {
            v += coef * (target - v);
            if (v - target < 1e-6f && target - v < 1e-6f) v = target; // lands exactly (bit-exact mix 0)
            return v;
        }
    };

    struct Ring
    {
        std::vector<float> buf;
        int mask = 0, wp = 0;
        void allocate (int minSize);
        void clear();
        inline void push (float x) { buf[(size_t) wp] = x; wp = (wp + 1) & mask; }
        float read (double delay) const; // Hermite, delay in samples behind the next write
    };

    // Two-head granular pitch shifter with waveform-aligned splices (WSOLA-style): when a head restarts, its new
    // read position is chosen (within one search span) to be in phase with the head that is still playing, so
    // the crossfade never cancels and a steady tone keeps a clean spectral line at exactly ratio * f.
    struct PitchShifter
    {
        Ring ring;
        int grain = 2400, search = 480, corrLen = 384, step = 2;
        std::array<double, 2> delay { 0.0, 0.0 }, age { 0.0, 0.5 };
        bool started = false;
        void configure (double sampleRate);
        float process (float x, double ratio);
        double latency (double ratio) const { return 2.0 + 0.5 * search + 0.5 * std::abs (1.0 - ratio) * grain; }
        void clear() { ring.clear(); started = false; age = { 0.0, 0.5 }; }

    private:
        double startDelay (double ratio) const { return 2.0 + (ratio > 1.0 ? (ratio - 1.0) * grain : 0.0); }
        double alignedStart (double ratio, double otherDelay) const;
    };

    struct Allpass
    {
        Ring ring;
        float process (float x, double delay, float g);
    };

    struct Chan
    {
        SVF hp, lp, bbdLp, ghostLp;
        float tapeLp = 0.0f, dcX = 0.0f, dcY = 0.0f, held = 0.0f;
        double shPhase = 1.0, revPhase = 0.0;
        PitchShifter ps;
        std::array<Allpass, 4> ap;
        void clearState();
    };

    float loopColour (int c, float x, Mode m);
    void updateCoefficients();
    void resetModeState();
    inline float readMain (int c, double delay) const { return lines[(size_t) c].read (std::min (maxRead, std::max (1.0, delay))); }
    inline float noise() { rng = rng * 1664525u + 1013904223u; return (float) ((rng >> 8) & 0xffff) / 32768.0f - 1.0f; }

    Params params;
    double sampleRate = 48000.0;
    std::array<Ring, 2> lines;
    std::array<Chan, 2> chans;
    double maxRead = 1000.0;

    std::array<double, 2> delaySmp { 0.0, 0.0 };
    double glideCoef = 0.0;
    bool first = true;

    Mode activeMode = Mode::tape;
    float modeGain = 1.0f, modeStep = 0.0f;

    Smooth sFeedback, sLowCut, sHighCut, sDepth, sAge, sDuck, sDry, sWet, sOut, sSend, sFreeze, sDuckGain;
    float hpBlend = 0.0f, lpBlend = 0.0f, tapeCoef = 0.0f, bbdCoef = 0.0f, dcCoef = 0.0f;
    int coefCounter = 0;

    double lfoPhase = 0.0, flutterPhase = 0.0;
    float drift = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
    uint32_t rng = 0x2545F491u;

    float env = 0.0f, envAtt = 0.0f, envRel = 0.0f, wetEnv = 0.0f;
    float wetLevel = 0.0f, duckGainNow = 1.0f;
};

} // namespace ghostdelay
