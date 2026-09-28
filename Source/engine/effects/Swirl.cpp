// Swirl – phaser with 2–12 all-pass stages, feedback, free or tempo-synced LFO.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeSwirlInfo()
{
    EffectInfo i;
    i.type = EffectType::Swirl;
    i.id = "swirl";
    i.name = "Swirl";
    i.colour = juce::Colour (0xff9b59d0);
    i.category = Category::Modulation;
    i.params = { pHz ("RATE", 0.02f, 8.0f, 0.3f, 0.6f),
                 pToggle ("SYNC", false),
                 pUnit ("STAGES", Unit::Stages, 2.0f, 12.0f, 6.0f),
                 pPercent ("FEEDBACK", 0.4f),
                 pPercent ("DEPTH", 0.7f),
                 pPercent ("MIX", 0.5f),
                 pSync ("SYNC RATE", "1 BAR"),
                 pUnused() };
    i.mainParam = 5;
    i.mixParam = 5;
    i.macro[Drip] = { 0.25f, 0, 0, 0.1f, 0.25f, 0.1f, 0, 0 };
    i.macro[VillainArc] = { 0, 0, 0, 0.1f, 0, 0, 0, 0 };
    return i;
}

namespace
{
class Swirl final : public Effect
{
public:
    Swirl() : Effect (EffectType::Swirl) {}

    void onPrepare() override
    {
        for (auto* s : { &rate, &feedback, &depth, &mix }) s->reset (spec.sampleRate, 40.0f, s->getTarget());
        for (auto& d : dc) d.prepare (spec.sampleRate);
    }

    void reset() override
    {
        for (auto& ch : ap) ch.fill (0.0f);
        fb[0] = fb[1] = 0.0f;
        freePhase = 0.0;
        for (auto& d : dc) d.reset();
        for (auto* s : { &rate, &feedback, &depth, &mix }) s->snap (s->getTarget());
    }

    void applyParameters() override
    {
        rate.setTarget (getReal (0));
        synced = getReal (1) > 0.5f;
        // keep an even number of stages
        stages = juce::jlimit (2, 12, (choice (2) / 2) * 2);
        feedback.setTarget (getReal (3));
        depth.setTarget (getReal (4));
        mix.setTarget (getReal (5));
        syncBeatsValue = syncBeats (getReal (6));
    }

    void process (float* L, float* R, int n, const ProcessContext& ctx) override
    {
        const float s = sr();
        if (synced) sync.advanceBlock (ctx, syncBeatsValue, n);

        for (int i = 0; i < n; ++i)
        {
            const float rt = rate.next(), f = feedback.next() * 0.85f, d = depth.next(), m = mix.next();
            double ph;
            if (synced) ph = sync.tick();
            else { freePhase += rt / s; if (freePhase >= 1.0) freePhase -= 1.0; ph = freePhase; }

            const float in[2] = { L[i], R[i] };
            float out[2];
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * (float) ph + (c == 0 ? 0.0f : 1.5707963f));
                // sweep 200 Hz .. 200 Hz * 2^(depth*5)
                const float hz = 180.0f * std::exp2 (lfo * d * 5.5f);
                const float t = std::tan (kPi * std::min (hz, s * 0.45f) / s);
                const float a = (t - 1.0f) / (t + 1.0f);

                float x = in[c] + fb[c] * f;
                for (int k = 0; k < stages; ++k)
                {
                    float& z = ap[(size_t) c][(size_t) k];
                    const float y = a * x + z;
                    z = sanitize (x - a * y);
                    x = y;
                }
                fb[c] = sanitize (fastTanh (x));
                out[c] = dc[(size_t) c].process (x);
            }
            L[i] = in[0] + (out[0] - in[0]) * m;
            R[i] = in[1] + (out[1] - in[1]) * m;
        }
    }

private:
    Smoothed rate, feedback, depth, mix;
    std::array<std::array<float, 12>, 2> ap {};
    std::array<DcBlocker, 2> dc;
    float fb[2] {};
    int stages = 6;
    bool synced = false;
    float syncBeatsValue = 4.0f;
    double freePhase = 0.0;
    SyncPhase sync;
};
} // namespace

std::unique_ptr<Effect> createSwirl() { return std::make_unique<Swirl>(); }
} // namespace ek
