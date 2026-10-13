// TrapWin HEAT - multiband saturation / distortion engine (plain C++, no JUCE).
//
// Signal flow (per channel):
//   in -> LOW KEEP split (LR4) -> low part: delayed, untouched
//                              -> high part: tilt (pre) -> punch (pre-compression)
//        -> oversample (1/2/4/8x, linear-phase half-band FIR cascade)
//        -> 1/2/3 band LR4 split (phase-compensated, flat sum)
//        -> per band: model shaper (drive follows envelope with DYNAMICS), band mix, level, solo
//        -> sum -> downsample -> DC block -> tilt (post) -> low cut / high cut -> auto-gain
//   wet = low part + processed high part;  out = mix(dry delayed, wet) * output
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace heatfx
{

enum class Model : int { tube = 0, tape, transformer, transistor, fuzz, rectify, foldback, warmCrush, count };
static constexpr int numModels = (int) Model::count;
static constexpr const char* modelNames[numModels] = { "TUBE", "TAPE", "TRANSFORMER", "TRANSISTOR", "FUZZ", "RECTIFY", "FOLDBACK", "WARM CRUSH" };

static constexpr int maxBands = 3;
static constexpr int numOsChoices = 4;                 // 1x, 2x, 4x, 8x
static constexpr const char* osNames[numOsChoices] = { "1x", "2x", "4x", "8x" };
static constexpr int defaultOs = 2;                    // 4x
static constexpr float maxDriveDb = 36.0f;             // drive 100 % = +36 dB into the shaper
static constexpr float lowKeepOffHz = 20.0f;           // LOW KEEP at / below this = OFF
static constexpr float lowCutOffHz = 20.0f, highCutOffHz = 20000.0f;

struct BandParams
{
    int model = 0;
    float drive = 0.3f;      // 0..1
    float mix = 1.0f;        // 0..1
    float levelDb = 0.0f;    // -24..+12
    bool solo = false;
};

struct Params
{
    int bands = 1;                       // 1..3
    float xoverLoHz = 180.0f;            // band A | B
    float xoverHiHz = 2500.0f;           // band B | C (3 bands)
    std::array<BandParams, maxBands> band {};
    int oversampling = defaultOs;        // index into osNames
    float tiltPreDb = 0.0f;              // -6..+6 (dB at the spectrum ends, pivot 650 Hz)
    float tiltPostDb = 0.0f;
    float lowCutHz = lowCutOffHz;        // wet low cut (OFF at 20 Hz)
    float highCutHz = highCutOffHz;      // wet high cut (OFF at 20 kHz)
    bool autoGain = true;
    float punch = 0.0f;                  // 0..1 pre-compression
    float dynamics = 0.0f;               // -1..+1 drive follows the envelope
    float mix = 1.0f;                    // global dry / wet
    float outputDb = 0.0f;               // -24..+12
    float lowKeepHz = lowKeepOffHz;      // sub below this stays untouched (OFF at 20 Hz)
};

// ---- memoryless core transfer curve of a model (also used by the editor's transfer display).
// x = input sample (before drive), drive 0..1. Includes the drive gain, the level compensation and
// the clean blend at very low drive; excludes the frequency-dependent parts (tape / crush smoothing).
float transferCurve (int model, float drive, float x);
// raw shaper core: u = driven sample, a = drive 0..1, lowPart = low-frequency component of u,
// hiK / hiOut = transformer high-band drive reduction / output scale (see transformerHiK / transformerHiOut)
float shapeCore (int model, float u, float a, float lowPart, float hiK, float hiOut);
float transformerHiK (float drive);
float transformerHiOut (float drive);
inline float driveGain (float driveDb);
float compensation (float g);
float cleanBlend (float drive);

int factorForChoice (int osChoice);
int latencyForChoice (int osChoice);
int padForChoice (int osChoice);   // extra delay (highest-rate samples) that makes the latency an integer

// ---------------------------------------------------------------------------------------------

// Simper / Zavalishin state-variable filter (modulation friendly).
struct SvfCoefs
{
    float g = 0, k = 1.41421356f, a1 = 0, a2 = 0, a3 = 0;
    void set (double fc, double fs, double q);
};

struct SvfState
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    inline void tick (const SvfCoefs& c, float v0, float& lp, float& bp)
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1;
    }
};

// Linkwitz-Riley 4th order split: lp4 + hp4 = 2nd order Butterworth allpass
struct LR4
{
    SvfState l1, l2, h1, h2;
    void reset() { l1.reset(); l2.reset(); h1.reset(); h2.reset(); }
    inline void split (const SvfCoefs& c, float x, float& lo, float& hi)
    {
        float lp, bp;
        l1.tick (c, x, lp, bp);
        float lp2, bp2;
        l2.tick (c, lp, lp2, bp2);
        lo = lp2;
        float a, b;
        h1.tick (c, x, a, b);
        const float hp1 = x - c.k * b - a;
        h2.tick (c, hp1, a, b);
        hi = hp1 - c.k * b - a;
    }
};

// 2nd order allpass matching the LR4 sum (phase compensation of the lower band)
struct AP2
{
    SvfState s;
    void reset() { s.reset(); }
    inline float process (const SvfCoefs& c, float x)
    {
        float lp, bp;
        s.tick (c, x, lp, bp);
        return x - 2.0f * c.k * bp;
    }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    void setLowShelf (double fs, double f, double gainDb, double slope);
    void setHighShelf (double fs, double f, double gainDb, double slope);
};

struct BiquadState
{
    float z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (const Biquad& q, float x)
    {
        const float y = q.b0 * x + z1;
        z1 = q.b1 * x - q.a1 * y + z2;
        z2 = q.b2 * x - q.a2 * y;
        return y;
    }
};

// Linear-phase half-band FIR stage (2x up + 2x down), polyphase: only the even taps + the centre tap are computed.
class HalfbandStage
{
public:
    void design (int numTaps, double kaiserBeta);   // numTaps = 4k + 3
    void reset();
    int centre() const { return c; }
    inline void up (float x, float& y0, float& y1)
    {
        upPos = (upPos == 0 ? m : upPos) - 1;
        upBuf[(size_t) upPos] = upBuf[(size_t) (upPos + m)] = x;
        const float* h = he.data();
        const float* b = upBuf.data() + upPos;
        float s = 0.0f;
        for (int j = 0; j < m; ++j) s += h[j] * b[j];
        y0 = 2.0f * s;
        y1 = b[dUp];
    }
    inline float down (float v0, float v1)
    {
        evPos = (evPos == 0 ? m : evPos) - 1;
        evBuf[(size_t) evPos] = evBuf[(size_t) (evPos + m)] = v0;
        odPos = (odPos == 0 ? m : odPos) - 1;
        odBuf[(size_t) odPos] = odBuf[(size_t) (odPos + m)] = v1;
        const float* h = he.data();
        const float* b = evBuf.data() + evPos;
        float s = 0.0f;
        for (int j = 0; j < m; ++j) s += h[j] * b[j];
        return s + 0.5f * odBuf[(size_t) (odPos + dDown)];
    }

private:
    std::vector<float> he, upBuf, evBuf, odBuf;
    int m = 1, c = 1, dUp = 0, dDown = 1, upPos = 0, evPos = 0, odPos = 0;
};

class Engine
{
public:
    Engine();
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { target = p; }
    void process (float* const* ch, int numCh, int n);

    // latency of the oversampling setting currently running (changes after a switch fade)
    int getLatencySamples() const { return latencyForChoice (activeOs); }
    int getActiveOs() const { return activeOs; }

    // meters (audio thread writes, UI reads)
    float getBandInputPeak (int b) const { return bandPeak[(size_t) b].load (std::memory_order_relaxed); }
    float getAutoGainDb() const { return agDbMeter.load (std::memory_order_relaxed); }
    float getPunchGrDb() const { return punchGrMeter.load (std::memory_order_relaxed); }
    float getDriveModDb() const { return dynModMeter.load (std::memory_order_relaxed); }

private:
    static constexpr int chunk = 32;
    static constexpr int maxCh = 2;
    static constexpr int delaySize = 256;

    struct Smooth
    {
        float cur = 0, prev = 0;
        void reset (float v) { cur = prev = v; }
        void step (float t, float coef)
        {
            prev = cur;
            cur += (t - cur) * coef;
            if (std::abs (t - cur) < 1.0e-5f) cur = t;
        }
        float at (float frac) const { return prev + (cur - prev) * frac; }
    };

    struct BandState
    {
        mutable float tapeLp = 0, xfLp = 0, crushLp = 0, crushLp2 = 0;
        void reset() { tapeLp = xfLp = crushLp = crushLp2 = 0; }
    };

    struct Channel
    {
        LR4 keepSplit;
        BiquadState tiltPreLo, tiltPreHi, tiltPostLo, tiltPostHi;
        std::array<HalfbandStage, 3> os;
        LR4 split1, split2;
        AP2 ap2;
        std::array<BandState, maxBands> band;
        float dcX = 0, dcY = 0;
        SvfState lowCut, highCut;
        std::array<float, delaySize> dryDelay {}, lowDelay {};
        std::array<float, 8> pad {};
        int padPos = 0;
        void reset();
    };

    void applyStructure();
    void updateCoefficients();
    float runModel (int model, BandState& st, float u, float a, int b) const;

    double sr = 44100.0;
    Params target;
    std::array<Channel, maxCh> chans;

    // structural state (switched behind a short fade)
    int activeOs = defaultOs, activeBands = 1;
    float fadeGain = 1.0f;
    int fadeDir = 0;                         // -1 fading out, +1 fading in
    int holdSamples = 0;                     // muted settle time after an oversampling switch
    float fadeStep = 1.0f / 128.0f;
    int delayPos = 0;

    // smoothed params
    Smooth sXlo, sXhi, sTiltPre, sTiltPost, sLowCut, sHighCut, sLowCutOn, sHighCutOn, sPunch, sDyn, sMix, sOut, sKeep;
    std::array<Smooth, maxBands> sDrive, sBandMix, sBandLevel, sSolo;
    std::array<int, maxBands> curModel {}, prevModel {};
    std::array<float, maxBands> modelXf {};   // 1 = fully current model

    // coefficient caches
    SvfCoefs keepC, x1C, x2C, lowCutC, highCutC;
    Biquad preLo, preHi, postLo, postHi;
    float lastTiltPre = 1.0e9f, lastTiltPost = 1.0e9f;
    float tapeA[maxBands] {}, crushA[maxBands] {}, xfK[maxBands] { 1, 1, 1 }, xfO[maxBands] { 1, 1, 1 }, xfA = 0;

    // detectors
    float punchEnv = 0, dynEnv = 0, punchGrDb = 0;
    double msIn = 0, msOut = 0;
    float agDb = 0, agPrev = 0;
    float dcR = 0.9995f;

    // scratch (no allocation in process)
    std::array<std::array<float, chunk>, maxCh> hi {}, hiRaw {}, low {}, proc {};
    std::array<float, chunk> fadeArr {};
    std::array<float, chunk> punchGain {}, driveMod {};

    std::array<std::atomic<float>, maxBands> bandPeak;
    std::atomic<float> agDbMeter { 0 }, punchGrMeter { 0 }, dynModMeter { 0 };
};

inline float driveGain (float driveDb) { return std::pow (10.0f, driveDb / 20.0f); }

} // namespace heatfx
