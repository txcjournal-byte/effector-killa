// Red Line – output clipper / limiter: input drive, ceiling, SOFT <-> HARD character and a
// 1.5 ms lookahead limiter stage. Latency is fixed (lookahead is always reserved when allowed).
#include "AllEffects.h"
#include "../Rack.h"

namespace ek
{
EffectInfo makeRedLineInfo()
{
    EffectInfo i;
    i.type = EffectType::RedLine;
    i.id = "red_line";
    i.name = "Red Line";
    i.colour = juce::Colour (0xff3a7bd5);
    i.category = Category::Clipper;
    i.params = { pDb ("CEILING", -12.0f, 0.0f, -0.3f, -3.0f),
                 pDb ("DRIVE", 0.0f, 24.0f, 0.0f, 6.0f),
                 pPercent ("CHARACTER", 0.5f),
                 pToggle ("LOOKAHEAD", true),
                 pUnused(), pUnused(), pUnused(), pUnused() };
    i.mainParam = 1;
    i.oversampled = true;
    i.macro[CrashOut] = { 0, 0.3f, 0.2f, 0, 0, 0, 0, 0 };
    i.macro[Knock]    = { 0, 0.2f, 0, 0, 0, 0, 0, 0 };
    return i;
}

namespace
{
class RedLine final : public Effect
{
public:
    RedLine() : Effect (EffectType::RedLine) {}

    void onPrepare() override
    {
        host.prepare (spec, true, false);
        lookahead = spec.allowLookahead ? Rack::redLineLookaheadSamples (spec.sampleRate) : 0;
        laDelay.prepare (lookahead);
        window.assign ((size_t) lookahead + 2, 1.0f);
        dequeIdx.assign ((size_t) lookahead + 2, 0);
        dequeVal.assign ((size_t) lookahead + 2, 1.0f);
        const double osRate = spec.sampleRate * (1 << spec.osFactorLog2);
        drive.reset (spec.sampleRate, 30.0f, 1.0f);
        ceiling.reset (spec.sampleRate, 30.0f, 1.0f);
        ceilingOs.reset (osRate, 30.0f, 1.0f);
        character.reset (osRate, 30.0f, 0.5f);
        releaseCoeff = std::exp (-1.0f / (0.08f * sr()));
        attackCoeff = lookahead > 0 ? std::exp (-1.0f / std::max (1.0f, (float) lookahead / 4.0f)) : 0.0f;
    }

    void reset() override
    {
        host.reset();
        laDelay.reset();
        std::fill (window.begin(), window.end(), 1.0f);
        dqHead = dqTail = 0;
        sampleIndex = 0;
        limGain = 1.0f;
        for (auto* s : { &drive, &ceiling, &ceilingOs, &character }) s->snap (s->getTarget());
    }

    void applyParameters() override
    {
        const float c = dbToGain (getReal (0));
        ceiling.setTarget (c);
        ceilingOs.setTarget (c);
        drive.setTarget (dbToGain (getReal (1)));
        character.setTarget (getReal (2));
        charValue = getReal (2);
        lookaheadOn = getReal (3) > 0.5f;
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        // 1) drive + lookahead limiter (base rate). The delay is always applied (fixed latency).
        for (int i = 0; i < n; ++i)
        {
            const float g = drive.next();
            const float c = ceiling.next();
            float l = L[i] * g, r = R[i] * g;

            if (lookahead > 0)
            {
                // limiter threshold rises with HARD character so the clipper does more of the work
                const float th = c * dbToGainCheap (charValue * 6.0f);
                const float peak = std::max (std::abs (l), std::abs (r));
                const float need = (lookaheadOn && peak > th) ? th / peak : 1.0f;
                const float target = pushMin (need);
                limGain = target < limGain ? target + (limGain - target) * attackCoeff
                                           : target + (limGain - target) * releaseCoeff;
                l = laDelay.process (0, l) * limGain;
                r = laDelay.process (1, r) * limGain;
            }
            L[i] = l; R[i] = r;
        }

        // 2) clipper (oversampled)
        host.process (L, R, n, 0.0f, 1.0f, [this] (float* l, float* r, int nOS, int)
        {
            for (int i = 0; i < nOS; ++i)
            {
                const float c = ceilingOs.next();
                const float ch = character.next();
                l[i] = clip (l[i], c, ch);
                r[i] = clip (r[i], c, ch);
            }
        });

        // 3) final safety clamp at base rate (removes oversampling filter overshoot)
        for (int i = 0; i < n; ++i)
        {
            const float c = ceiling.get();
            L[i] = juce::jlimit (-c, c, sanitize (L[i]));
            R[i] = juce::jlimit (-c, c, sanitize (R[i]));
        }
    }

    int getLatencySamples() const noexcept override { return host.getLatency() + lookahead; }

private:
    static inline float dbToGainCheap (float db) noexcept { return std::exp (db * 0.11512925f); }

    // SOFT: knee from -6 dB below the ceiling; HARD: straight clip at the ceiling.
    static inline float clip (float x, float c, float character) noexcept
    {
        const float knee = c * (0.5f + 0.5f * character); // knee start
        const float ax = std::abs (x);
        if (ax <= knee) return x;
        const float range = c - knee;
        float y;
        if (range < 1.0e-5f) y = c;
        else y = knee + range * fastTanh ((ax - knee) / range);
        y = std::min (y, c);
        return x < 0.0f ? -y : y;
    }

    // Sliding-window minimum over the lookahead window (monotonic deque, O(1) amortised).
    float pushMin (float v) noexcept
    {
        const int size = (int) dequeVal.size();
        const int64_t idx = sampleIndex++;
        while (dqTail != dqHead && dequeVal[(size_t) ((dqTail - 1 + size) % size)] >= v)
            dqTail = (dqTail - 1 + size) % size;
        dequeVal[(size_t) dqTail] = v;
        dequeIdx[(size_t) dqTail] = idx;
        dqTail = (dqTail + 1) % size;
        while (dequeIdx[(size_t) dqHead] <= idx - lookahead)
            dqHead = (dqHead + 1) % size;
        return dequeVal[(size_t) dqHead];
    }

    NonlinearHost host;
    int lookahead = 0;
    bool lookaheadOn = true;
    float charValue = 0.5f;
    LatencyDelay laDelay;
    std::vector<float> window, dequeVal;
    std::vector<int64_t> dequeIdx;
    int dqHead = 0, dqTail = 0;
    int64_t sampleIndex = 0;
    float limGain = 1.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f;
    Smoothed drive, ceiling, ceilingOs, character;
};
} // namespace

std::unique_ptr<Effect> createRedLine() { return std::make_unique<RedLine>(); }
} // namespace ek
