// TrapWin SWEEP – tempo-synced filter modulation engine.
//
// Two multimode filters (LP12 / LP24 / HP12 / HP24 / BP / NOTCH / COMB / FORMANT) with cutoff, resonance and drive,
// routed serial (A -> B) or parallel (A + B). Cutoffs are modulated in octaves by three unipolar (0..1) sources:
//   LFO  – host-synced or free, shapes incl. a drawable 16-step pattern, stereo phase offset
//   ENV  – envelope follower (level in dB, normalised 0..1)
//   ADSR – triggered by MIDI notes or by detected transients
// modOct = (lfo * lfoDepth + env * envDepth + adsr * adsrDepth) * maxModOctaves, then per filter * its MOD amount.
// Resonance never reaches true self-oscillation (k >= kMin), the band state is soft-saturated and the wet
// signal passes a soft safety limiter, so nothing can run away.
//
// Plain C++17, no allocation after prepare(), testable headless; wrapped by JUCE in PluginProcessor.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace sweepfx
{

enum class FilterType { lp12 = 0, lp24, hp12, hp24, bp, notch, comb, formant };
constexpr int numFilterTypes = 8;
constexpr const char* filterTypeNames[numFilterTypes] = { "LP 12", "LP 24", "HP 12", "HP 24", "BAND", "NOTCH", "COMB", "FORMANT" };

enum class Shape { sine = 0, triangle, sawUp, sawDown, square, sampleHold, pattern };
constexpr int numShapes = 7;
constexpr const char* shapeNames[numShapes] = { "SINE", "TRI", "RAMP UP", "RAMP DN", "SQUARE", "S&H", "STEPS" };

enum class Routing { serial = 0, parallel };
enum class Trigger { midi = 0, transient };

constexpr int numDivisions = 16;
constexpr const char* divisionNames[numDivisions] = { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T",
                                                      "1/4", "1/4D", "1/2T", "1/2", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
constexpr double divisionBeats[numDivisions] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0,
                                                 1.0, 1.5, 4.0 / 3.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
constexpr int defaultDivision = 8;   // 1/4
constexpr int numSteps = 16;

constexpr float maxModOctaves = 6.0f;
constexpr float minCutoff = 20.0f, maxCutoff = 20000.0f;
constexpr float kMin = 0.035f;       // resonance floor of the SVF damping (Q ~ 28): high, but never a free-running oscillator

struct FilterParams
{
    bool on = true;
    FilterType type = FilterType::lp12;
    float cutoff = 1000.0f;   // Hz
    float reso = 0.2f;        // 0..1
    float drive = 0.0f;       // 0..1
    float mod = 1.0f;         // -1..1: how much of the modulation this filter follows (negative = inverted)
};

struct Params
{
    FilterParams a;
    FilterParams b { false, FilterType::hp12, 200.0f, 0.2f, 0.0f, 1.0f };   // second filter: off by default
    Routing routing = Routing::serial;

    // LFO
    bool sync = true;
    int division = defaultDivision;   // cycle length (pattern: step length)
    float rateHz = 2.0f;              // free rate: cycles per second (pattern: steps per second)
    Shape shape = Shape::sine;
    float lfoDepth = 0.5f;            // -1..1
    float stereo = 0.0f;              // 0..1 = 0..180 deg phase offset on the right channel
    float glide = 0.0f;               // 0..1 step smoothing (pattern / S&H)
    bool retrig = false;              // restart the LFO on every ADSR trigger
    std::array<float, numSteps> pattern { 1, 0.25f, 0.6f, 0.25f, 1, 0.25f, 0.6f, 0.25f, 1, 0.25f, 0.6f, 0.25f, 1, 0.25f, 0.8f, 0.4f };

    // envelope follower
    float envDepth = 0.0f;            // -1..1
    float envSpeed = 0.5f;            // 0 = slow, 1 = fast

    // ADSR
    Trigger trigger = Trigger::midi;
    float sensitivity = 0.5f;         // transient detector, 0..1
    float attackMs = 5.0f, decayMs = 250.0f, sustain = 0.0f, releaseMs = 200.0f;
    float adsrDepth = 0.0f;           // -1..1

    float mix = 1.0f;
    float outputDb = 0.0f;
};

struct HostTime
{
    bool playing = false;
    bool hasPpq = false;
    double ppq = 0.0;
    double bpm = 140.0;
};

// static helpers (also used by the editor's response display and the tests)
double cycleSeconds (const Params&, double bpm);                       // LFO cycle (pattern: full 16-step cycle)
float lfoShapeValue (Shape, double phase, const std::array<float, numSteps>& pattern, uint32_t shRandom);
float magnitudeDb (const FilterParams&, float cutoffHz, float freqHz, double sampleRate);
float responseDb (const Params&, float cutoffA, float cutoffB, float freqHz, double sampleRate);

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { params = p; }
    const Params& getParams() const { return params; }

    void noteOn();
    void noteOff();
    void allNotesOff();

    // channels: 1 or 2, in-place
    void process (float* const* channels, int numChannels, int numSamples, const HostTime& time);

    // live values (audio thread writes, UI reads)
    std::atomic<float> cutoffA { 1000.0f }, cutoffB { 1000.0f };   // modulated cutoffs (left channel)
    std::atomic<float> lfoOut { 0.0f }, envOut { 0.0f }, adsrOut { 0.0f };
    std::atomic<float> lfoPhase { 0.0f };
    std::atomic<int> triggerCount { 0 };
    double getBpm() const { return bpm; }

private:
    struct SVF
    {
        float ic1 = 0, ic2 = 0;
        void clear() { ic1 = ic2 = 0; }
        // returns lp, writes bp/hp. g = tan(pi fc / sr), k = damping
        inline void tick (float x, float g, float k, float& lp, float& bp, float& hp)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float v3 = x - ic2;
            const float v1 = a1 * (ic1 + g * v3);
            const float v2 = ic2 + g * v1;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            // guard: soft-saturate the states (inaudible at normal levels, stops any runaway at extreme resonance)
            if (ic1 > 6.0f || ic1 < -6.0f) ic1 = softLimit (ic1);
            if (ic2 > 6.0f || ic2 < -6.0f) ic2 = softLimit (ic2);
            if (ic1 > -1e-20f && ic1 < 1e-20f) ic1 = 0.0f;
            if (ic2 > -1e-20f && ic2 < 1e-20f) ic2 = 0.0f;
            lp = v2;
            bp = v1;
            hp = x - k * v1 - v2;
        }
        static float softLimit (float v);
    };

    struct FilterState
    {
        SVF s1, s2, f1, f2, f3;
        float combLp = 0.0f;
        std::vector<float> comb;
        int combPos = 0;
        void clear();
    };

    float runFilter (FilterState&, const FilterParams&, FilterType, float x, float cutoff, float reso, float drive);

    Params params;
    double sampleRate = 48000.0;
    double bpm = 140.0;
    bool first = true;

    // smoothed controls
    float smCutA = 1000.0f, smCutB = 1000.0f, smResA = 0, smResB = 0, smDrvA = 0, smDrvB = 0, smModA = 1, smModB = 1;
    float smLfoD = 0, smEnvD = 0, smAdsrD = 0, smMix = 1, smOut = 1, smBMix = 1, smPar = 0;
    float ctlCoef = 0.0f;
    float mixTarget = 1.0f;

    std::array<FilterState, 2> fa, fb;
    FilterType activeA = FilterType::lp12, activeB = FilterType::lp12;
    float typeGainA = 1.0f, typeGainB = 1.0f;
    int combSize = 0;

    // LFO
    double phase = 0.0, phaseOffset = 0.0;
    bool lastHostLocked = false;
    double lastPhL = 2.0, lastPhR = 2.0;
    uint32_t rngState = 0x12345678u, shValue = 0, shValueR = 0;
    std::array<float, 2> lfoSm { 0, 0 };
    float lfoSmoothCoef = 0.0f;

    // envelope follower
    float env = 0.0f;
    // transient detector
    float tFast = 0, tSlow = 0, tFastA = 0, tFastR = 0, tSlowA = 0, tSlowR = 0;
    int holdoff = 0, autoOff = -1;

    // ADSR
    enum class Stage { idle, attack, decay, sustain, release } stage = Stage::idle;
    float adsr = 0.0f;
    int heldNotes = 0;
    bool pendingTrigger = false, pendingRelease = false;
    void trigger();
    void release();
};

} // namespace sweepfx
