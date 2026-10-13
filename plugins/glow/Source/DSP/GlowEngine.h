// TrapWin GLOW – one-knob instant enhancer engine.
//
// Signal flow (stereo-linked detection, zero latency):
//   x ─┬─ complementary 3-band split (low = LP2, mid = LP2(x - low), high = rest; the bands sum to x exactly)
//      │    per band: upward compression (lifts quiet detail) + gentle downward + mode EQ + colour tilt + contrast
//      │    y = x + Σ (G_b - 1)·band_b
//      ├─ FAT: octave harmonics of the sub band (full-wave rectifier, band-limited)  – level independent
//      ├─ AIR: exciter (rectified top band, high-passed again)                        – level independent
//      ├─ saturation (asymmetric soft clip, unity small-signal gain, DC-blocked difference)
//      └─ push → auto-gain → linked peak limiter (instant attack) → crossfade with dry → output gain
//
// GLOW = 0 is bit-exact: the wet chain is blended as  dry + fade·(wet - dry)  with fade = 0 at GLOW 0,
// and the output gain is exactly 1.0 at 0 dB.
// Plain C++17, no allocation after prepare(), no JUCE: testable headless.
#pragma once

#include <array>
#include <atomic>
#include <cstddef>

using std::size_t;

namespace glowfx
{

enum class Mode { warm = 0, bright = 1, fat = 2, loud = 3 };
constexpr int numModes = 4;
constexpr std::array<const char*, numModes> modeNames { "Warm", "Bright", "Fat", "Loud" };

struct Params
{
    Mode mode = Mode::warm;
    float glow = 0.5f;      // 0..1   the big knob
    float contrast = 0.0f;  // -1..1  (-1 = dense / glued, 0 = neutral, +1 = punchy / open)
    float colour = 0.0f;    // -1..1  (dark .. bright)
    float outputDb = 0.0f;  // -24..+12
    bool autoGain = true;
};

// per-mode blend of the processors (amounts at GLOW = 100 %)
struct Voice
{
    float up[3];      // upward compression amount per band (0..1)
    float down[3];    // downward compression amount per band (0..1)
    float eqDb[3];    // static band tilt in dB
    float sat;        // saturation amount 0..1
    float satBias;    // asymmetry (even harmonics) 0..1
    float fat;        // low harmonic generator 0..1
    float exciter;    // 0..1
    float exFreq;     // exciter corner (Hz)
    float pushDb;     // drive into the limiter (dB)
    float limit;      // limiter weight 0..1
    float ceilingDb;  // limiter ceiling (dBFS)
};

Voice voiceFor (Mode m);

// TPT state-variable filter (Butterworth by default)
struct SVF
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1.41421356f;
    float ic1 = 0, ic2 = 0;
    void set (double fc, double sr, double q = 0.70710678);
    void reset() { ic1 = ic2 = 0; }
    void flush() { if (ic1 > -1e-15f && ic1 < 1e-15f) ic1 = 0; if (ic2 > -1e-15f && ic2 < 1e-15f) ic2 = 0; }
    inline float lp (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
    inline float hp (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v0 - k * v1 - v2;
    }
};

// approximate K-weighted mean-square meter (HP ~60 Hz + ~+4 dB shelf above ~1.5 kHz), one-pole averaging
struct KMeter
{
    void prepare (double sr, double tauSeconds = 0.4);
    void reset();
    inline float weight (int ch, float x)
    {
        const float h = pre[(size_t) ch].hp (x);
        return h + 0.585f * shelf[(size_t) ch].hp (h);
    }
    inline void push (float meanSquare) { ms += coef * (meanSquare - ms); }
    float ms = 0.0f;
    float coef = 0.0f;
    std::array<SVF, 2> pre, shelf;
};

constexpr int numBands = 3;
constexpr float lowSplitHz = 170.0f, highSplitHz = 3600.0f;
constexpr float agMaxDb = 12.0f;

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { target = p; }
    void process (float* const* ch, int numCh, int n);
    int getLatencySamples() const { return 0; }

    // metering (written by the audio thread, read by the UI)
    float getBandLiftDb (int b) const { return meterLift[(size_t) b].load (std::memory_order_relaxed); }
    float getAutoGainDb() const { return meterAg.load (std::memory_order_relaxed); }
    float getLimiterGrDb() const { return meterGr.load (std::memory_order_relaxed); }
    float getInputPeak() const { return meterIn.load (std::memory_order_relaxed); }
    float getOutputPeak() const { return meterOut.load (std::memory_order_relaxed); }
    float getHarmonics() const { return meterHarm.load (std::memory_order_relaxed); }

private:
    void updateControls (int n);
    void setExciter (float hz);

    double sr = 44100.0;
    Params target;

    // smoothed controls
    float glowS = 0.0f, outS = 1.0f, contrastS = 0.0f, colourS = 0.0f;
    Voice voiceS {};
    float ctlCoef = 0.0f;   // per-sub-block smoothing
    float exFreqCur = -1.0f;

    struct Chan
    {
        SVF lowSplit, midSplit;
        SVF fatLp1, fatLp2, fatHp, fatLpOut;
        SVF exHp1, exHp2, exHpOut;
        float dcX = 0, dcY = 0;
    };
    std::array<Chan, 2> chans;

    // linked detectors (power domain)
    std::array<float, numBands> bandEnv {};
    float bandAtk = 0, bandRel = 0;
    float fastEnv = 0, slowEnv = 0, fastAtk = 0, fastRel = 0, slowAtk = 0, slowRel = 0;
    float limEnv = 0, limRel = 0, inPkEnv = 0;
    float dcCoef = 0.999f;

    // auto-gain (closed loop on K-weighted loudness)
    KMeter inMeter, outMeter;
    float agDb = 0.0f, agStep = 0.0f, agReturn = 0.0f;

    // UI meters
    std::array<std::atomic<float>, numBands> meterLift {};
    std::atomic<float> meterAg { 0.0f }, meterGr { 0.0f }, meterIn { 0.0f }, meterOut { 0.0f }, meterHarm { 0.0f };
};

} // namespace glowfx
