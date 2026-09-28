// Overcooked – 3-band upward + downward compressor (120 Hz / 2.5 kHz Linkwitz-Riley split).
#include "AllEffects.h"

namespace ek
{
EffectInfo makeOvercookedInfo()
{
    EffectInfo i;
    i.type = EffectType::Overcooked;
    i.id = "overcooked";
    i.name = "Overcooked";
    i.colour = juce::Colour (0xffe8452c);
    i.category = Category::Dynamics;
    i.params = { pPercent ("DEPTH", 0.4f),
                 pPercent ("TIME", 0.5f),
                 pPercent ("UPWARD", 0.5f),
                 pPercent ("DOWNWARD", 0.5f),
                 pDb ("LOW", -12.0f, 12.0f, 0.0f),
                 pDb ("MID", -12.0f, 12.0f, 0.0f),
                 pDb ("HIGH", -12.0f, 12.0f, 0.0f),
                 pPercent ("MIX", 1.0f) };
    i.mainParam = 0;
    i.mixParam = 7;
    i.macro[CrashOut] = { 0.35f, 0, 0.1f, 0.1f, 0, 0, 0, 0 };
    i.macro[Knock]    = { 0.15f, -0.1f, 0, 0.15f, 0, 0, 0, 0 };
    i.macro[VillainArc] = { 0, 0, 0, 0, 0, 0, -0.12f, 0 };
    return i;
}

namespace
{
class Overcooked final : public Effect
{
public:
    Overcooked() : Effect (EffectType::Overcooked) {}

    void onPrepare() override
    {
        for (auto* x : { &xLow, &xHigh, &xApLow })
            for (auto& c : *x) c.prepare (spec.sampleRate);
        for (auto& c : xLow) c.setCutoff (120.0f, true);
        for (auto& c : xHigh) c.setCutoff (2500.0f, true);
        for (auto& c : xApLow) c.setCutoff (2500.0f, true);
        for (auto* s : { &depth, &upward, &downward, &mix }) s->reset (spec.sampleRate, 40.0f, s->getTarget());
        for (auto& g : bandGain) g.reset (spec.sampleRate, 40.0f, 1.0f);
        updateTimes();
    }

    void reset() override
    {
        for (auto* x : { &xLow, &xHigh, &xApLow }) for (auto& c : *x) c.reset();
        env.fill (0.0f);
        gainDb.fill (0.0f);
        for (auto* s : { &depth, &upward, &downward, &mix }) s->snap (s->getTarget());
        for (auto& g : bandGain) g.snap (g.getTarget());
    }

    void applyParameters() override
    {
        depth.setTarget (getReal (0));
        time = getReal (1);
        upward.setTarget (getReal (2));
        downward.setTarget (getReal (3));
        for (int b = 0; b < 3; ++b) bandGain[(size_t) b].setTarget (dbToGain (getReal (4 + b)));
        mix.setTarget (getReal (7));
        updateTimes();
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        static constexpr float downTh[3] = { -18.0f, -20.0f, -24.0f };
        static constexpr float upTh[3]   = { -32.0f, -34.0f, -38.0f };

        for (int i = 0; i < n; ++i)
        {
            const float d = depth.next(), up = upward.next(), dn = downward.next(), m = mix.next();
            const float g0 = bandGain[0].next(), g1 = bandGain[1].next(), g2 = bandGain[2].next();
            const float bandOut[3] = { g0, g1, g2 };

            std::array<std::array<float, 3>, 2> bands {};
            const float in[2] = { L[i], R[i] };
            for (int c = 0; c < 2; ++c)
            {
                float lo, rest, mid, hi, a, b;
                xLow[(size_t) c].process (in[c], lo, rest);
                xHigh[(size_t) c].process (rest, mid, hi);
                xApLow[(size_t) c].process (lo, a, b); // phase-match the low band
                bands[(size_t) c] = { a + b, mid, hi };
            }

            const float rd = 1.0f + 40.0f * dn * d;
            const float ru = 1.0f + 3.5f * up * d;
            float outL = 0.0f, outR = 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                const float lvl = std::max (std::abs (bands[0][(size_t) b]), std::abs (bands[1][(size_t) b]));
                float& e = env[(size_t) b];
                e = lvl > e ? lvl + (e - lvl) * att[(size_t) b] : lvl + (e - lvl) * rel[(size_t) b];
                e = sanitize (e);
                const float lDb = gainToDb (e + 1.0e-9f);

                float g = 0.0f;
                if (lDb > downTh[b]) g -= (lDb - downTh[b]) * (1.0f - 1.0f / rd);
                if (lDb < upTh[b])
                {
                    const float gate = juce::jlimit (0.0f, 1.0f, (lDb + 72.0f) / 12.0f);
                    g += std::min (18.0f * d, (upTh[b] - lDb) * (1.0f - 1.0f / ru)) * gate;
                }
                g += 3.0f * d; // gentle makeup
                gainDb[(size_t) b] = g;
                const float lin = dbToGain (g) * bandOut[b];
                outL += bands[0][(size_t) b] * lin;
                outR += bands[1][(size_t) b] * lin;
            }

            L[i] = in[0] + (outL - in[0]) * m;
            R[i] = in[1] + (outR - in[1]) * m;
        }
    }

private:
    void updateTimes()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        static constexpr float attBase[3] = { 12.0f, 5.0f, 2.0f };
        static constexpr float relBase[3] = { 180.0f, 110.0f, 70.0f };
        const float scale = std::pow (4.0f, time * 2.0f - 1.0f); // 0.25x .. 4x
        for (int b = 0; b < 3; ++b)
        {
            att[(size_t) b] = std::exp (-1.0f / (attBase[b] * scale * 0.001f * s));
            rel[(size_t) b] = std::exp (-1.0f / (relBase[b] * scale * 0.001f * s));
        }
    }

    std::array<LR4Crossover, 2> xLow, xHigh, xApLow;
    Smoothed depth, upward, downward, mix;
    std::array<Smoothed, 3> bandGain;
    std::array<float, 3> env {}, gainDb {}, att {}, rel {};
    float time = 0.5f;
};
} // namespace

std::unique_ptr<Effect> createOvercooked() { return std::make_unique<Overcooked>(); }
} // namespace ek
