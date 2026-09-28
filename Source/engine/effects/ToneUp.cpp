// Tone Up – 5-band tone EQ: low-cut, low shelf (100 Hz), low-mid bell, high-mid bell,
// high shelf (8 kHz) and a tilt around 1 kHz.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeToneUpInfo()
{
    EffectInfo i;
    i.type = EffectType::ToneUp;
    i.id = "tone_up";
    i.name = "Tone Up";
    i.colour = juce::Colour (0xff3fb8a8);
    i.category = Category::Eq;
    i.params = { pHz ("LOW-CUT", 10.0f, 500.0f, 10.0f, 80.0f),
                 pDb ("LOW", -12.0f, 12.0f, 0.0f),
                 pDb ("LOW-MID", -12.0f, 12.0f, 0.0f),
                 pHz ("LOW-MID FREQ", 60.0f, 1200.0f, 250.0f, 300.0f),
                 pDb ("HIGH-MID", -12.0f, 12.0f, 0.0f),
                 pHz ("HIGH-MID FREQ", 500.0f, 10000.0f, 2500.0f, 2500.0f),
                 pDb ("HIGH", -12.0f, 12.0f, 0.0f),
                 pDb ("TILT", -6.0f, 6.0f, 0.0f) };
    i.mainParam = 6;
    i.macro[VillainArc] = { 0, 0.05f, 0, 0, -0.1f, 0, -0.25f, -0.35f };
    i.macro[Aura]       = { 0, 0, 0, 0, 0, 0, 0.08f, 0 };
    i.macro[Knock]      = { 0, 0.1f, 0, 0, 0.06f, 0, 0, 0 };
    return i;
}

namespace
{
class ToneUp final : public Effect
{
public:
    ToneUp() : Effect (EffectType::ToneUp) {}

    void onPrepare() override
    {
        for (int k = 0; k < kNumParams; ++k)
            sm[(size_t) k].reset (spec.sampleRate, 30.0f, getReal (k));
        design();
    }

    void reset() override
    {
        for (auto& band : f) for (auto& b : band) b.reset();
        for (auto& s : sm) s.snap (s.getTarget());
        design();
    }

    void applyParameters() override
    {
        for (int k = 0; k < kNumParams; ++k)
            sm[(size_t) k].setTarget (getReal (k));
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        int pos = 0;
        while (pos < n)
        {
            const int len = std::min (32, n - pos);
            bool moving = false;
            for (auto& s : sm) if (s.isSmoothing()) { moving = true; s.skip (len); }
            if (moving) design();

            for (int i = pos; i < pos + len; ++i)
            {
                float l = L[i], r = R[i];
                for (int b = 0; b < kBands; ++b)
                {
                    if (! active[(size_t) b]) continue;
                    l = f[(size_t) b][0].process (l);
                    r = f[(size_t) b][1].process (r);
                }
                L[i] = l; R[i] = r;
            }
            pos += len;
        }
    }

private:
    static constexpr int kBands = 8;

    void design()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        auto v = [this] (int k) { return sm[(size_t) k].get(); };
        std::array<Biquad, kBands> d;
        const float lc = v (0);
        d[0].setHighPass (lc, 0.54f, s);
        d[1].setHighPass (lc, 1.31f, s);
        active[0] = active[1] = lc > 11.0f;
        d[2].setLowShelf (100.0f, v (1), s, 0.9f);           active[2] = std::abs (v (1)) > 0.01f;
        d[3].setPeak (v (3), 0.9f, v (2), s);                active[3] = std::abs (v (2)) > 0.01f;
        d[4].setPeak (v (5), 0.9f, v (4), s);                active[4] = std::abs (v (4)) > 0.01f;
        d[5].setHighShelf (8000.0f, v (6), s, 0.9f);         active[5] = std::abs (v (6)) > 0.01f;
        d[6].setLowShelf (1000.0f, -0.5f * v (7), s, 0.35f);  active[6] = std::abs (v (7)) > 0.01f;
        d[7].setHighShelf (1000.0f, 0.5f * v (7), s, 0.35f);  active[7] = active[6];
        for (int b = 0; b < kBands; ++b)
        {
            if (! wasActive[(size_t) b] && active[(size_t) b])
                for (auto& x : f[(size_t) b]) x.reset();
            wasActive[(size_t) b] = active[(size_t) b];
            for (auto& x : f[(size_t) b]) x.copyCoeffs (d[(size_t) b]);
        }
    }

    std::array<Smoothed, kNumParams> sm;
    std::array<std::array<Biquad, 2>, kBands> f;
    std::array<bool, kBands> active {}, wasActive {};
};
} // namespace

std::unique_ptr<Effect> createToneUp() { return std::make_unique<ToneUp>(); }
} // namespace ek
