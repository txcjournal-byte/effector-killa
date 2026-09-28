// Ad-Lib Throw – stereo / ping-pong delay with free or synced time, filtered feedback and ducking.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeAdLibThrowInfo()
{
    EffectInfo i;
    i.type = EffectType::AdLibThrow;
    i.id = "adlib_throw";
    i.name = "Ad-Lib Throw";
    i.colour = juce::Colour (0xff8e44ad);
    i.category = Category::Space;
    i.params = { pSync ("TIME", "1/4"),
                 pToggle ("SYNC", true),
                 pPercent ("FEEDBACK", 0.35f, 0.95f),
                 pToggle ("PING-PONG", false),
                 pHz ("LP", 1000.0f, 20000.0f, 9000.0f, 5000.0f),
                 pHz ("HP", 20.0f, 2000.0f, 150.0f, 250.0f),
                 pPercent ("DUCKING", 0.0f),
                 pPercent ("MIX", 0.2f) };
    i.mainParam = 7;
    i.mixParam = 7;
    i.macro[Aura]       = { 0, 0, 0.2f, 0, 0, 0, 0, 0.25f };
    i.macro[VillainArc] = { 0, 0, 0, 0, -0.35f, 0, 0, 0 };
    i.macro[Drip]       = { 0, 0, 0.1f, 0, 0, 0, 0, 0 };
    return i;
}

namespace
{
class AdLibThrow final : public Effect
{
public:
    AdLibThrow() : Effect (EffectType::AdLibThrow) {}

    // Free time (SYNC off) maps the TIME choice onto milliseconds with the same list at 120 BPM.
    void onPrepare() override
    {
        for (auto& d : lines) d.prepare ((int) std::ceil (4.1f * sr()) + 8);
        delaySamples.reset (spec.sampleRate, 120.0f, 0.5f * sr());
        for (auto* s : { &feedback, &ducking, &mix }) s->reset (spec.sampleRate, 40.0f, s->getTarget());
        duckEnv.set (5.0f, 250.0f, sr());
        updateFilters();
    }

    void reset() override
    {
        for (auto& d : lines) d.reset();
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        duckEnv.reset();
        delaySamples.snap (delaySamples.getTarget());
        for (auto* s : { &feedback, &ducking, &mix }) s->snap (s->getTarget());
    }

    void applyParameters() override
    {
        timeBeats = syncBeats (getReal (0));
        synced = getReal (1) > 0.5f;
        feedback.setTarget (getReal (2));
        pingPong = getReal (3) > 0.5f;
        lpHz = getReal (4);
        hpHz = getReal (5);
        ducking.setTarget (getReal (6));
        mix.setTarget (getReal (7));
        updateFilters();
    }

    void process (float* L, float* R, int n, const ProcessContext& ctx) override
    {
        const float s = sr();
        const double bpm = synced ? ctx.bpm : 120.0;
        const float target = juce::jlimit (1.0f, 4.0f * s, (float) (timeBeats * 60.0 / bpm * s));
        delaySamples.setTarget (target);

        for (int i = 0; i < n; ++i)
        {
            const float d = delaySamples.next();
            const float fbk = feedback.next(), du = ducking.next(), m = mix.next();
            const float dryL = L[i], dryR = R[i];

            float yl = lines[0].readCubic (std::max (2.0f, d));
            float yr = lines[1].readCubic (std::max (2.0f, d));

            float inL, inR;
            if (pingPong)
            {
                const float mono = 0.5f * (dryL + dryR);
                inL = mono + yr * fbk;  // cross feedback
                inR = yl * fbk;
            }
            else
            {
                inL = dryL + yl * fbk;
                inR = dryR + yr * fbk;
            }
            inL = hp[0].process (lp[0].process (inL));
            inR = hp[1].process (lp[1].process (inR));
            lines[0].push (sanitize (fastTanh (inL * 0.8f) * 1.25f));
            lines[1].push (sanitize (fastTanh (inR * 0.8f) * 1.25f));

            const float e = duckEnv.process (0.5f * (std::abs (dryL) + std::abs (dryR)));
            const float duck = 1.0f - du * std::min (1.0f, e * 4.0f);
            // send-style mix law: dry stays at unity up to 50 %
            const float dg = std::min (1.0f, 2.0f * (1.0f - m)), wg = std::min (1.0f, 2.0f * m);
            L[i] = dryL * dg + yl * duck * wg;
            R[i] = dryR * dg + yr * duck * wg;
        }
    }

private:
    void updateFilters()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        Biquad a, b;
        a.setLowPass (std::min (lpHz, s * 0.45f), 0.707f, s);
        b.setHighPass (hpHz, 0.707f, s);
        for (auto& f : lp) f.copyCoeffs (a);
        for (auto& f : hp) f.copyCoeffs (b);
    }

    std::array<DelayLine, 2> lines;
    std::array<Biquad, 2> lp, hp;
    Smoothed delaySamples, feedback, ducking, mix;
    EnvelopeFollower duckEnv;
    float timeBeats = 1.0f, lpHz = 9000.0f, hpHz = 150.0f;
    bool synced = true, pingPong = false;
};
} // namespace

std::unique_ptr<Effect> createAdLibThrow() { return std::make_unique<AdLibThrow>(); }
} // namespace ek
