// Squeeze – feed-forward stereo-linked compressor with soft knee, makeup and parallel mix.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeSqueezeInfo()
{
    EffectInfo i;
    i.type = EffectType::Squeeze;
    i.id = "squeeze";
    i.name = "Squeeze";
    i.colour = juce::Colour (0xff8fbf3a);
    i.category = Category::Dynamics;
    i.params = { pDb ("THRESHOLD", -60.0f, 0.0f, -18.0f, -18.0f),
                 pUnit ("RATIO", Unit::Ratio, 1.0f, 20.0f, 3.0f, 4.0f),
                 pMs ("ATTACK", 0.1f, 100.0f, 10.0f, 10.0f),
                 pMs ("RELEASE", 10.0f, 1000.0f, 120.0f, 150.0f),
                 pDb ("KNEE", 0.0f, 12.0f, 6.0f),
                 pDb ("MAKEUP", 0.0f, 24.0f, 0.0f, 6.0f),
                 pPercent ("MIX", 1.0f),
                 pUnused() };
    i.mainParam = 1;
    i.mixParam = 6;
    i.macro[CrashOut] = { -0.12f, 0.1f, 0, 0, 0, 0.08f, 0, 0 };
    i.macro[Knock]    = { -0.08f, 0.2f, 0.12f, 0, 0, 0.12f, 0, 0 };
    return i;
}

namespace
{
class Squeeze final : public Effect
{
public:
    Squeeze() : Effect (EffectType::Squeeze) {}

    void onPrepare() override
    {
        threshold.reset (spec.sampleRate, 30.0f, -18.0f);
        ratio.reset (spec.sampleRate, 30.0f, 3.0f);
        knee.reset (spec.sampleRate, 30.0f, 6.0f);
        makeup.reset (spec.sampleRate, 30.0f, 0.0f);
        mix.reset (spec.sampleRate, 30.0f, 1.0f);
        updateTimes();
    }

    void reset() override
    {
        grDb = 0.0f;
        for (auto* s : { &threshold, &ratio, &knee, &makeup, &mix }) s->snap (s->getTarget());
    }

    void applyParameters() override
    {
        threshold.setTarget (getReal (0));
        ratio.setTarget (getReal (1));
        attackMs = getReal (2);
        releaseMs = getReal (3);
        knee.setTarget (getReal (4));
        makeup.setTarget (getReal (5));
        mix.setTarget (getReal (6));
        updateTimes();
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float th = threshold.next(), ra = ratio.next(), kn = knee.next();
            const float mk = makeup.next(), m = mix.next();
            const float peak = std::max (std::abs (L[i]), std::abs (R[i]));
            const float xDb = gainToDb (peak + 1.0e-9f);

            // soft-knee static curve (target gain reduction, <= 0 dB)
            const float over = xDb - th;
            float target;
            if (2.0f * over < -kn)               target = 0.0f;
            else if (2.0f * std::abs (over) <= kn && kn > 0.0f)
            {
                const float t = over + kn * 0.5f;
                target = (1.0f / ra - 1.0f) * t * t / (2.0f * kn);
            }
            else                                   target = (1.0f / ra - 1.0f) * over;

            // attack when reduction increases, release when it recovers
            grDb = target < grDb ? target + (grDb - target) * attCoeff
                                 : target + (grDb - target) * relCoeff;
            grDb = sanitize (grDb);

            const float g = dbToGain (grDb + mk);
            L[i] = L[i] * (1.0f - m) + L[i] * g * m;
            R[i] = R[i] * (1.0f - m) + R[i] * g * m;
        }
    }

    float getGainReductionDb() const noexcept { return grDb; }

private:
    void updateTimes()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        attCoeff = std::exp (-1.0f / (attackMs * 0.001f * s));
        relCoeff = std::exp (-1.0f / (releaseMs * 0.001f * s));
    }

    Smoothed threshold, ratio, knee, makeup, mix;
    float attackMs = 10.0f, releaseMs = 120.0f, attCoeff = 0.0f, relCoeff = 0.0f;
    float grDb = 0.0f;
};
} // namespace

std::unique_ptr<Effect> createSqueeze() { return std::make_unique<Squeeze>(); }
} // namespace ek
