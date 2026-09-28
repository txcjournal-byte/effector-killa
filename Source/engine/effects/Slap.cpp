// Slap – transient shaper (attack / sustain) with optional soft clip on the output.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeSlapInfo()
{
    EffectInfo i;
    i.type = EffectType::Slap;
    i.id = "slap";
    i.name = "Slap";
    i.colour = juce::Colour (0xfff2e14c);
    i.category = Category::Dynamics;
    i.params = { pBipolar ("ATTACK", 0.0f),
                 pBipolar ("SUSTAIN", 0.0f),
                 pPercent ("CLIP OUT", 0.0f),
                 pPercent ("MIX", 1.0f),
                 pUnused(), pUnused(), pUnused(), pUnused() };
    i.mainParam = 0;
    i.mixParam = 3;
    i.macro[Knock]    = { 0.35f, -0.1f, 0.2f, 0, 0, 0, 0, 0 };
    i.macro[CrashOut] = { 0, 0, 0.3f, 0, 0, 0, 0, 0 };
    i.macro[Aura]     = { 0, 0.15f, 0, 0, 0, 0, 0, 0 };
    return i;
}

namespace
{
class Slap final : public Effect
{
public:
    Slap() : Effect (EffectType::Slap) {}

    void onPrepare() override
    {
        const float s = sr();
        fast.set (0.3f, 25.0f, s);
        slow.set (18.0f, 200.0f, s);
        susFast.set (3.0f, 35.0f, s);
        susSlow.set (3.0f, 420.0f, s);
        gainSm.set (0.2f, 8.0f, s);
        for (auto* x : { &attack, &sustain, &clip, &mix }) x->reset (spec.sampleRate, 30.0f, x->getTarget());
    }

    void reset() override
    {
        fast.reset(); slow.reset(); susFast.reset(); susSlow.reset();
        gainState = 0.0f;
        for (auto* x : { &attack, &sustain, &clip, &mix }) x->snap (x->getTarget());
    }

    void applyParameters() override
    {
        attack.setTarget (getReal (0));
        sustain.setTarget (getReal (1));
        clip.setTarget (getReal (2));
        mix.setTarget (getReal (3));
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = attack.next(), su = sustain.next(), c = clip.next(), m = mix.next();
            const float x = 0.5f * (std::abs (L[i]) + std::abs (R[i]));
            const float f = fast.process (x), s = slow.process (x);
            const float sf = susFast.process (x), ss = susSlow.process (x);

            const float atkDiff = juce::jlimit (0.0f, 24.0f, gainToDb (f + 1.0e-6f) - gainToDb (s + 1.0e-6f));
            const float susDiff = juce::jlimit (0.0f, 24.0f, gainToDb (ss + 1.0e-6f) - gainToDb (sf + 1.0e-6f));
            float gDb = a * atkDiff * 0.9f + su * susDiff * 0.8f;
            gDb = juce::jlimit (-18.0f, 18.0f, gDb);
            // light smoothing of the gain curve to avoid crackle
            gainState = gDb + (gainState - gDb) * 0.6f;
            const float g = dbToGain (gainState);

            float l = L[i] * g, r = R[i] * g;
            if (c > 0.001f)
            {
                l = softClip (l, c);
                r = softClip (r, c);
            }
            L[i] = L[i] + (l - L[i]) * m;
            R[i] = R[i] + (r - R[i]) * m;
        }
    }

private:
    static inline float softClip (float x, float amount) noexcept
    {
        const float t = 1.0f - amount * 0.75f; // knee start
        const float ax = std::abs (x);
        if (ax <= t) return x;
        const float y = t + (1.0f - t) * fastTanh ((ax - t) / (1.0f - t));
        return x < 0.0f ? -y : y;
    }

    EnvelopeFollower fast, slow, susFast, susSlow, gainSm;
    Smoothed attack, sustain, clip, mix;
    float gainState = 0.0f;
};
} // namespace

std::unique_ptr<Effect> createSlap() { return std::make_unique<Slap>(); }
} // namespace ek
