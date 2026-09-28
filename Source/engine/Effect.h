#pragma once

// Common interface of every Effector Killa rack effect.
//
// Each effect exposes exactly 8 normalised host parameters (P1..P8). The effect maps them onto its
// own real ranges through ParamSpec. Unused P slots are marked inactive. The effect never allocates
// in process(); memory is set up in prepare().

#include "dsp/DspCommon.h"
#include "dsp/Oversampler.h"
#include <functional>
#include <memory>

namespace ek
{
constexpr int kNumSlots = 8;
constexpr int kNumParams = 8;
constexpr int kNumMacros = 5;
constexpr int kMaxChunk = 512; // the rack always processes in chunks of at most this many samples

enum class EffectType : int
{
    None = 0,
    CassettePlug,
    Menace,
    Brainrot,
    ThroughTheWall,
    ToneUp,
    Squeeze,
    Overcooked,
    Slap,
    Doubles,
    Swirl,
    AdLibThrow,
    AuraRoom,
    WideBody,
    Chopped,
    RedLine,
    Count
};
constexpr int kNumEffectTypes = (int) EffectType::Count - 1;

enum Macro : int { VillainArc = 0, CrashOut, Aura, Drip, Knock };

enum class Unit { None, Percent, Hz, Db, Ms, Seconds, Ratio, Bits, Octaves, Choice, Toggle, Sync, Voices, Stages, Pattern };

struct ParamSpec
{
    juce::String name;             // label shown in the UI (short)
    float min = 0.0f, max = 1.0f;  // real range
    float def = 0.0f;              // real default
    float centre = 0.0f;           // skew centre (0 = linear)
    Unit unit = Unit::None;
    juce::StringArray choices;     // for Choice / Sync
    bool used = true;

    bool isDiscrete() const noexcept
    {
        return unit == Unit::Choice || unit == Unit::Toggle || unit == Unit::Sync || unit == Unit::Voices
            || unit == Unit::Stages || unit == Unit::Bits || unit == Unit::Pattern;
    }
    int numSteps() const noexcept { return (int) std::lround (max - min) + 1; }

    float toReal (float norm) const noexcept;
    float fromReal (float real) const noexcept;
    float defaultNorm() const noexcept { return fromReal (def); }
    juce::String format (float real) const;
};

enum class Category { Saturation, Distortion, LoFi, Filter, Eq, Dynamics, Modulation, Space, Stereo, Gate, Clipper };

struct EffectInfo
{
    EffectType type = EffectType::None;
    juce::String id;      // stable serialisation id
    juce::String name;    // UI name
    juce::Colour colour;  // cassette dot colour
    Category category = Category::Filter;
    std::array<ParamSpec, kNumParams> params;
    int mainParam = 0;    // "amount" used by preset shorthand ("Cassette Plug 0.3")
    int mixParam = -1;    // internal mix parameter, if any
    int lowKeepParam = -1;
    bool oversampled = false;
    // Default macro mapping: amount per macro per parameter (normalised offset at full macro swing).
    std::array<std::array<float, kNumParams>, kNumMacros> macro {};
};

struct PrepareSpec
{
    double sampleRate = 44100.0;
    int maxBlock = kMaxChunk;
    int osFactorLog2 = 1; // 0 = 1x, 1 = 2x, 2 = 4x
    bool allowLookahead = true; // only one Red Line per chain may use its lookahead
};

struct ProcessContext
{
    double sampleRate = 44100.0;
    double bpm = 140.0;
    double ppq = 0.0;          // position (quarter notes) at the start of the chunk
    bool playing = false;
    bool hostHasPosition = false;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    double beatsPerSample() const noexcept { return bpm / (60.0 * sampleRate); }
    double samplesPerBeat() const noexcept { return 60.0 * sampleRate / bpm; }
};

class Effect
{
public:
    explicit Effect (EffectType t);
    virtual ~Effect() = default;

    const EffectInfo& info() const noexcept { return *infoPtr; }
    EffectType getType() const noexcept { return infoPtr->type; }

    void prepare (const PrepareSpec& s)
    {
        spec = s;
        onPrepare();
        applyParameters();
        reset();
    }

    virtual void reset() = 0;

    // Called once per chunk with the effective normalised parameter values.
    void setParameters (const float* normalised) noexcept
    {
        bool changed = false;
        for (int i = 0; i < kNumParams; ++i)
            if (norm[(size_t) i] != normalised[i]) { norm[(size_t) i] = normalised[i]; changed = true; }
        if (changed || first) { first = false; applyParameters(); }
    }

    // In-place stereo processing, n <= spec.maxBlock.
    virtual void process (float* L, float* R, int n, const ProcessContext& ctx) = 0;

    // Fixed latency after prepare() (never changes with parameters).
    virtual int getLatencySamples() const noexcept { return 0; }

    float getNorm (int i) const noexcept { return norm[(size_t) i]; }
    float getReal (int i) const noexcept { return infoPtr->params[(size_t) i].toReal (norm[(size_t) i]); }

protected:
    virtual void onPrepare() = 0;
    virtual void applyParameters() = 0; // read getReal() into internal targets

    float sr() const noexcept { return (float) spec.sampleRate; }
    int choice (int i) const noexcept { return (int) std::lround (getReal (i)); }

    PrepareSpec spec;

private:
    const EffectInfo* infoPtr;
    std::array<float, kNumParams> norm {};
    bool first = true;
};

// ---------------------------------------------------------------------------
// Registry – adding an effect = one new class + one registration entry.
// ---------------------------------------------------------------------------
const EffectInfo& effectInfo (EffectType t);
std::unique_ptr<Effect> createEffect (EffectType t);
EffectType effectTypeFromId (const juce::String& idOrName); // accepts id or UI name
juce::Array<EffectType> allEffectTypes();

// ---------------------------------------------------------------------------
// Helper for nonlinear effects: optional Linkwitz-Riley "low keep" split, oversampling
// of the high band, latency-aligned dry/wet mix. Used by Cassette Plug, Menace, Brainrot, Red Line.
// ---------------------------------------------------------------------------
class NonlinearHost
{
public:
    void prepare (const PrepareSpec& s, bool useOversampling, bool useLowKeep);
    void reset();
    int getLatency() const noexcept { return os.getLatency(); }

    // fn (float* L, float* R, int nOS, int factor) processes the (oversampled) band in place.
    template <typename Fn>
    void process (float* L, float* R, int n, float lowKeepHz, float mixTarget, Fn&& fn)
    {
        mix.setTarget (mixTarget);
        const bool split = lowKeep && lowKeepHz > 0.0f;
        if (split) { xL.setCutoff (lowKeepHz); xR.setCutoff (lowKeepHz); }

        for (int i = 0; i < n; ++i)
        {
            float lo, hi;
            if (split)
            {
                xL.process (L[i], lo, hi); lowL[(size_t) i] = lo; L[i] = hi;
                xR.process (R[i], lo, hi); lowR[(size_t) i] = lo; R[i] = hi;
            }
            else
            {
                lowL[(size_t) i] = lowR[(size_t) i] = 0.0f;
            }
            dryL[(size_t) i] = L[i];
            dryR[(size_t) i] = R[i];
        }

        os.process (L, R, n, fn);

        for (int i = 0; i < n; ++i)
        {
            const float m = mix.next();
            const float dl = dryDelay.process (0, dryL[(size_t) i]);
            const float dr = dryDelay.process (1, dryR[(size_t) i]);
            const float ll = lowDelay.process (0, lowL[(size_t) i]);
            const float lr = lowDelay.process (1, lowR[(size_t) i]);
            L[i] = ll + dl + (L[i] - dl) * m;
            R[i] = lr + dr + (R[i] - dr) * m;
        }
    }

private:
    Oversampler os;
    bool lowKeep = false;
    LR4Crossover xL, xR;
    LatencyDelay dryDelay, lowDelay;
    std::vector<float> dryL, dryR, lowL, lowR;
    Smoothed mix;
};

} // namespace ek
