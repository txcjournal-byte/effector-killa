// TrapWin AURA VERB - multi-algorithm studio reverb engine (plain C++, no JUCE).
//
// Signal flow (per sample):
//   input -> pre-delay (ms or tempo synced) -> [ Tank A | Tank B ] (equal-power crossfade on mode change)
//         -> wet low-cut -> width (M/S) -> ducking -> dry/wet mix -> output gain
//
// A Tank holds one complete algorithm, configured per mode:
//   bandwidth LP -> early reflections (mode specific tap pattern, scaled by size)
//                -> build/attack (swelling multi-tap smear) -> input diffusion (4 allpasses per side)
//                -> late structure:
//                     ROOM / HALL / CHAMBER / 80s DIGITAL / SHIMMER / DARK SPACE: 8-line feedback delay network
//                       (own line lengths, in-loop allpasses, modulation style, interpolation, ER pattern each),
//                       two-band decay (bass multiplier) + HF damping, 8x8 Hadamard mixing.
//                       SHIMMER: octave + fifth pitch shifters inside the loop. 80s DIGITAL: linear interpolation,
//                       deep chorused lines, reduced bandwidth, word-length grain and an output chorus.
//                       DARK SPACE: very long lines, random-walk modulation, heavy darkening.
//                     PLATE: figure-of-eight allpass/delay tank with multi-tap stereo output.
//
// Real-time safe: all memory is allocated in prepare(); process() never allocates or locks.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace auraverb
{

enum class Mode : int { room = 0, hall, plate, chamber, digital80s, shimmer, darkSpace };
constexpr int numModes = 7;
extern const char* const modeNames[numModes];

constexpr int numDivisions = 11;
extern const char* const divisionNames[numDivisions];
extern const double divisionBeats[numDivisions];
constexpr int defaultDivision = 4;   // 1/16

struct Params
{
    Mode mode = Mode::hall;
    float mix = 0.30f;           // 0 .. 1
    float predelayMs = 20.0f;    // 0 .. 500 ms (free)
    bool predelaySync = false;
    int predelayDiv = defaultDivision;
    double bpm = 120.0;
    float decaySec = 2.5f;       // 0.1 .. 60 s (RT60)
    float size = 0.6f;           // 0 .. 1
    float earlyLate = 0.65f;     // 0 = early reflections only, 0.5 = both, 1 = late tail only
    float attack = 0.0f;         // 0 .. 1 (build-up / swell of the tail)
    float diffusion = 0.8f;      // 0 .. 1
    float modDepth = 0.35f;      // 0 .. 1
    float modRateHz = 0.6f;      // 0.05 .. 5 Hz
    float dampingHz = 8000.0f;   // 1 k .. 20 k (HF decay; 20 k = off)
    float lowCutHz = 60.0f;      // 20 .. 1000 Hz (wet high-pass; 20 = off)
    float bassMult = 1.0f;       // 0.25 .. 2.5 (low-band decay multiplier)
    float width = 1.0f;          // 0 .. 2
    float duck = 0.0f;           // 0 .. 1
    bool freeze = false;
    bool vintage = false;        // colour: Modern / Vintage
    float outputDb = 0.0f;       // -24 .. +12
};

double predelayMsFor (const Params& p);

// Early reflection pattern of a mode at a given size (times in ms, signed linear gains); used by the engine and the UI.
constexpr int maxEarlyTaps = 16;
int earlyPattern (Mode m, float size, float* timesMs, float* gains);

struct ModeSpec;

class Engine
{
public:
    static constexpr float minDecay = 0.1f, maxDecay = 60.0f;
    static constexpr float maxPredelayMs = 2000.0f;   // free range is 500 ms, synced values may go further
    static constexpr double modeFadeSeconds = 0.15;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void setParams (const Params& p) { target = p; }
    void process (float* const* channels, int numChannels, int numSamples);
    int getLatencySamples() const { return 0; }

    // meters (audio thread writes, read from anywhere; approximate)
    float getInputLevel() const { return inLevel; }
    float getWetLevel() const { return wetLevel; }
    float getDuckGain() const { return duckGainNow; }   // 1 = no ducking
    bool isCrossfading() const { return fading; }
    Mode getActiveMode() const { return tanks[active].mode; }

    struct Smooth
    {
        float v = 0.0f, t = 0.0f, c = 1.0f;
        void setTime (double sr, double seconds) { c = seconds <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (seconds * sr))); }
        void snap() { v = t; }
        inline float next()
        {
            v += c * (t - v);
            if (std::abs (t - v) < 1.0e-7f) v = t;
            return v;
        }
    };

    struct Delay
    {
        std::vector<float> buf;
        int mask = 0, w = 0;
        void init (int minLength);
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        inline void write (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        // read BEFORE write: delay d (>= 1) returns x[n - d]
        inline float read (float d) const
        {
            const float pos = (float) w - d;
            const float fl = std::floor (pos);
            const int k = (int) fl;
            const float f = pos - fl;
            const float a = buf[(size_t) (k & mask)], b = buf[(size_t) ((k + 1) & mask)];
            return a + f * (b - a);
        }
        inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
        inline float readCubic (float d) const;   // d >= 3
    };

    struct Allpass   // Schroeder allpass on a variable-length delay
    {
        Delay d;
        inline float process (float x, float len, float g)
        {
            const float dl = d.read (len);
            const float v = x - g * dl;
            d.write (v);
            return dl + g * v;
        }
        inline float processInt (float x, int len, float g)
        {
            const float dl = d.readInt (len);
            const float v = x - g * dl;
            d.write (v);
            return dl + g * v;
        }
    };

    struct Svf   // TPT state variable filter
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

    struct Shifter   // two-tap granular pitch shifter (Hann crossfade, |gain| <= 1)
    {
        Delay d;
        float phase = 0.0f, inc = 0.0f, win = 1000.0f;
        void setup (double sr, float ratio, float windowSec)
        {
            win = (float) (windowSec * sr);
            inc = (ratio - 1.0f) / win;   // ratio > 1: the read tap runs towards the write head
        }
        inline float process (float x);
    };

    struct Tank
    {
        static constexpr int numLines = 8;
        void allocate (double sr);
        void configure (Mode m);              // no allocation: clears state, sets the structure
        void setTargets (const Params& p);
        void snapSmoothers();
        inline void process (float inL, float inR, float& outL, float& outR);
        void clear();

        Mode mode = Mode::hall;
        bool plate = false;
        double sr = 48000.0;
        const ModeSpec* spec = nullptr;

        // front end
        Svf bwL, bwR;
        Smooth bwGs;
        Delay erL, erR;          // early reflections + build taps read from here
        int erCount = 0;
        float erMs[maxEarlyTaps] {}, erGain[maxEarlyTaps] {};
        Smooth erScale, buildSpan, attackS;
        std::array<Allpass, 2> erAp;
        int erApLen[2] {};
        std::array<Allpass, 4> diffL, diffR;
        int diffLenL[4] {}, diffLenR[4] {};
        Smooth diffCoef;

        // late structure
        std::array<Delay, numLines> lines;
        std::array<Allpass, numLines> loopAp;     // FDN: in-loop allpass; PLATE: 0/2 modulated APs, 1/3 decay APs
        float baseLen[numLines] {};               // samples at scale 1
        int loopApLen[numLines] {};
        float plateAp[4] {};                      // PLATE allpass lengths (samples at scale 1)
        float lpLow[numLines] {}, damp[numLines] {};
        Smooth gHigh[numLines], gLow[numLines];
        float lfoPhase[numLines] {}, lfoMul[numLines] {}, rndFrom[numLines] {}, rndTo[numLines] {};
        uint32_t rng = 0x1234567u;
        Smooth scale, modDepth, dampCoef, apCoef, inGain, shimAmt;
        float xoverCoef = 0.03f;
        float modRate = 0.6f;   // Hz
        float plateFbL = 0.0f, plateFbR = 0.0f;
        float denorm = 1.0e-20f;

        // shimmer
        Shifter shOct, shFifth;
        float shHp = 0.0f, shLp = 0.0f;

        // output colour
        Smooth earlyGain, lateGain, wetNorm;
        float bitsQ = 0.0f;            // 80s DIGITAL quantiser steps (0 = off)
        bool vintage = false;
        Delay chL, chR;                // 80s chorus
        float chPhase = 0.0f;
        Svf hfL, hfR;                  // 80s / vintage output band limit
        Smooth hfGs;
        bool hfOn = false;
    };

private:
    void updateControl();

    double sr = 48000.0;
    bool prepared = false;
    Params target;

    Delay preL, preR;
    Smooth preSamples;

    Tank tanks[2];
    int active = 0;
    bool fading = false;
    float fadePos = 0.0f, fadeInc = 0.0f;

    Svf hpL, hpR;
    float hpG = 0.01f;
    Smooth widthS, dryS, wetS, outS;

    float duckEnv = 0.0f, duckAtt = 0.0f, duckRel = 0.0f, duckUp = 0.0f;
    Smooth duckGain;
    float duckGainNow = 1.0f;

    int controlCounter = 0;
    float inLevel = 0.0f, wetLevel = 0.0f;
};

} // namespace auraverb
