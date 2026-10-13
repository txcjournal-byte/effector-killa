// TrapWin WIDE – stereo imager engine.
//
// Signal flow (zero latency):
//   L/R → M/S
//     side: phase-compensated Linkwitz-Riley (LR4) 3-band split × per-band width; the mid runs through the same
//           all-pass chain, so with equal widths the image is exactly preserved (flat magnitude, no comb)
//         + STEREOIZE: decorrelated copy of the mid (all-pass cascade) added to the side – mono-compatible by design,
//           because +D goes to L and -D goes to R, the mono sum stays exactly 2·M
//     M/S balance
//   → Haas: one channel delayed 0–30 ms (smoothed, both channels have a line so switching sides glides)
//   → MONO BELOW: phase-coherent mono-maker (mid through the LR4 all-pass, side through the LR4 high-pass)
//   → output gain
//
// When every imaging control is neutral the processed path is crossfaded out (30 ms, linear) and the engine outputs
// the input untouched, so width 100 % (with everything else at default) is bit-exact. The output gain sits after
// that blend and is skipped at exactly 0 dB.
// Meters: correlation (exponentially averaged, ~300 ms) and a vectorscope ring buffer.
// Plain C++17, no allocation after prepare(), no JUCE.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <vector>

namespace widefx
{

using std::size_t;

enum class HaasSide { left = 0, right = 1 };

struct Params
{
    float lowXHz = 200.0f;     // low / mid crossover   (40 .. 1000 Hz)
    float highXHz = 3000.0f;   // mid / high crossover  (1 .. 12 kHz)
    float widthLow = 1.0f;     // 0 .. 2  (0 = mono, 1 = unchanged, 2 = double side)
    float widthMid = 1.0f;
    float widthHigh = 1.0f;
    float monoBelowHz = 0.0f;  // <= 20 = off, else 20 .. 500 Hz
    float haasMs = 0.0f;       // 0 .. 30 ms
    HaasSide haasSide = HaasSide::right;
    float balance = 0.0f;      // -1 (mid only) .. 0 .. +1 (side only)
    float stereoize = 0.0f;    // 0 .. 1
    float outputDb = 0.0f;     // -24 .. +12
};

constexpr float monoOffHz = 20.0f;
constexpr float maxHaasMs = 30.0f;

// TPT state-variable filter
struct SVF
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1.41421356f;
    float ic1 = 0, ic2 = 0;
    void set (double fc, double sr, double q);
    void reset() { ic1 = ic2 = 0; }
    void flush() { if (ic1 > -1e-15f && ic1 < 1e-15f) ic1 = 0; if (ic2 > -1e-15f && ic2 < 1e-15f) ic2 = 0; }
    struct Out { float lp, bp, hp; };
    inline Out tick (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }
    inline float lp (float v) { return tick (v).lp; }
    inline float hp (float v) { return tick (v).hp; }
    inline float ap (float v) { const auto o = tick (v); return v - 2.0f * k * o.bp; }
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { target = p; }
    void process (float* const* ch, int numCh, int n);
    int getLatencySamples() const { return 0; }

    // metering
    float getCorrelation() const { return meterCorr.load (std::memory_order_relaxed); }
    float getBandSideDb (int b) const { return meterBand[(size_t) b].load (std::memory_order_relaxed); }
    float getPeak (int c) const { return meterPeak[(size_t) c].load (std::memory_order_relaxed); }

    // vectorscope: most recent output samples (L, R); the UI copies the last n points
    static constexpr int scopeSize = 4096;
    int copyScope (float* l, float* r, int n) const;

    // pure helper used by the meter (exposed for tests)
    static float correlationOf (double sumLR, double sumLL, double sumRR);

private:
    void updateFilters();
    std::array<SVF*, 15> allFilters() { return { &monoApM, &aLp1, &aLp2, &aHp1, &aHp2, &bLp1, &bLp2, &bHp1, &bHp2, &lowApB, &midApA, &midApB, &decorHp, &monoHp1, &monoHp2 }; }
    static bool isNeutral (const Params& p);   // imaging controls only (output gain is separate)

    double sr = 44100.0;
    Params target;

    // smoothed controls
    float wS[3] = { 1, 1, 1 };
    float gmS = 1.0f, gsS = 1.0f, stS = 0.0f, outS = 1.0f, monoFade = 0.0f;
    float xf = 0.0f, xfStep = 0.0f;  // processed-path amount (0 = bit-exact dry)
    float dS[2] = { 0, 0 };          // Haas delay per channel, in samples
    float lowXCur = -1, highXCur = -1, monoCur = -1;
    float lowXS = 200.0f, highXS = 3000.0f, monoS = 120.0f;
    float pCoef = 0.0f, fCoef = 0.0f;

    // filters
    // side band split: LR4 at the low crossover (a) and the high crossover (b), all-pass compensated
    SVF aLp1, aLp2, aHp1, aHp2, bLp1, bLp2, bHp1, bHp2, lowApB;
    SVF midApA, midApB;                    // the same all-pass chain for the mid
    std::array<SVF, 6> decor;              // stereoize all-pass cascade
    SVF decorHp;                           // keeps the decorrelated copy out of the lows
    SVF monoApM, monoHp1, monoHp2;         // mono-below: LR4 all-pass for mid, LR4 high-pass for side

    // Haas delay lines
    std::array<std::vector<float>, 2> line;
    int lineMask = 0, writePos = 0;

    // meters
    double eLR = 0, eLL = 0, eRR = 0;
    float corrCoef = 0.0f;
    std::array<double, 3> bandPow {};
    std::atomic<float> meterCorr { 1.0f };
    std::array<std::atomic<float>, 3> meterBand {};
    std::array<std::atomic<float>, 2> meterPeak {};
    std::array<std::atomic<float>, scopeSize> scopeL {}, scopeR {};
    std::atomic<int> scopeWrite { 0 };
    int scopeDecim = 1, scopeCount = 0;
};

} // namespace widefx
