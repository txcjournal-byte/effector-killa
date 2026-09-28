// Menace – distortion (FUZZ / CLIP / FOLDBACK / RECTIFY) with pre high-pass, post low-pass,
// tone, low keep and mix. Oversampled.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeMenaceInfo()
{
    EffectInfo i;
    i.type = EffectType::Menace;
    i.id = "menace";
    i.name = "Menace";
    i.colour = juce::Colour (0xffe8b923);
    i.category = Category::Distortion;
    i.params = { pChoice ("TYPE", { "FUZZ", "CLIP", "FOLDBACK", "RECTIFY" }, 1),
                 pPercent ("DRIVE", 0.3f),
                 pPercent ("TONE", 0.5f),
                 pHz ("PRE-HP", 20.0f, 1000.0f, 20.0f, 150.0f),
                 pHz ("POST-LP", 1000.0f, 20000.0f, 20000.0f, 6000.0f),
                 pHz ("LOW KEEP", 20.0f, 250.0f, 20.0f, 80.0f),
                 pPercent ("MIX", 1.0f),
                 pUnused() };
    i.mainParam = 1;
    i.mixParam = 6;
    i.lowKeepParam = 5;
    i.oversampled = true;
    i.macro[VillainArc] = { 0, 0, -0.35f, 0, -0.3f, 0, 0, 0 };
    i.macro[CrashOut]   = { 0, 0.45f, 0.05f, 0, 0, 0, 0.1f, 0 };
    return i;
}

namespace
{
class Menace final : public Effect
{
public:
    Menace() : Effect (EffectType::Menace) {}

    enum Type { FUZZ = 0, CLIP, FOLDBACK, RECTIFY };

    void onPrepare() override
    {
        host.prepare (spec, true, true);
        const double osRate = spec.sampleRate * (1 << spec.osFactorLog2);
        drive.reset (osRate, 30.0f, 1.0f);
        for (auto& d : dc) d.prepare (spec.sampleRate);
        lastPreHp = lastPostLp = -1.0f;
        updateFilters();
    }

    void reset() override
    {
        host.reset();
        for (auto& f : preHp) f.reset();
        for (auto& f : postLp) f.reset();
        for (auto& f : postLp2) f.reset();
        for (auto& f : tone) f.reset();
        for (auto& d : dc) d.reset();
        drive.snap (drive.getTarget());
    }

    void applyParameters() override
    {
        type = choice (0);
        driveAmt = getReal (1);
        drive.setTarget (driveAmt);
        toneValue = getReal (2);
        preHpHz = getReal (3);
        postLpHz = getReal (4);
        lowKeep = getReal (5);
        mix = getReal (6);
        updateFilters();
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        host.process (L, R, n, lowKeep, mix, [this] (float* l, float* r, int nOS, int)
        {
            for (int i = 0; i < nOS; ++i)
            {
                const float d = drive.next();
                l[i] = postLp2[0].process (postLp[0].process (shape (preHp[0].process (l[i]), d)));
                r[i] = postLp2[1].process (postLp[1].process (shape (preHp[1].process (r[i]), d)));
            }
        });

        for (int i = 0; i < n; ++i)
        {
            L[i] = tone[0].process (dc[0].process (L[i]));
            R[i] = tone[1].process (dc[1].process (R[i]));
        }
    }

    int getLatencySamples() const noexcept override { return host.getLatency(); }

private:
    inline float shape (float x, float d) const noexcept
    {
        switch (type)
        {
            case FUZZ:
            {
                const float g = dbToGainFast (6.0f + d * 34.0f);
                const float a = fastTanh (g * x + 0.25f) - 0.2449187f; // tanh(0.25)
                return fastTanh (1.6f * a) * std::pow (g, -0.55f) * 1.4f;
            }
            case CLIP:
            {
                const float g = dbToGainFast (d * 30.0f);
                const float v = g * x;
                const float a = std::abs (v);
                // smooth hard clip: x / (1 + |x|^6)^(1/6)
                const float a2 = a * a, a6 = a2 * a2 * a2;
                const float y = v / std::pow (1.0f + a6, 1.0f / 6.0f);
                return y * std::pow (g, -0.65f);
            }
            case FOLDBACK:
            {
                const float g = 1.0f + d * 9.0f;
                return std::sin (juce::jlimit (-40.0f, 40.0f, g * x) * 1.5707963f) * (0.8f + 0.2f / g);
            }
            case RECTIFY:
            default:
            {
                const float g = dbToGainFast (d * 24.0f);
                const float t = fastTanh (g * x);
                const float rect = std::abs (t);
                return (t * (1.0f - d * 0.7f) + rect * (0.4f + d * 0.8f)) * std::pow (g, -0.45f);
            }
        }
    }

    static inline float dbToGainFast (float db) noexcept { return std::exp (db * 0.11512925f); }

    void updateFilters()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        const float osRate = s * (float) (1 << spec.osFactorLog2);
        if (preHpHz != lastPreHp)
        {
            Biquad b; b.setHighPass (preHpHz, 0.707f, osRate);
            for (auto& f : preHp) f.copyCoeffs (b);
            lastPreHp = preHpHz;
        }
        if (postLpHz != lastPostLp)
        {
            Biquad b1, b2;
            b1.setLowPass (std::min (postLpHz, s * 0.45f), 0.54f, osRate);
            b2.setLowPass (std::min (postLpHz, s * 0.45f), 1.31f, osRate);
            for (auto& f : postLp) f.copyCoeffs (b1);
            for (auto& f : postLp2) f.copyCoeffs (b2);
            lastPostLp = postLpHz;
        }
        Biquad t; t.setHighShelf (2200.0f, (toneValue - 0.5f) * 18.0f, s, 0.8f);
        for (auto& f : tone) f.copyCoeffs (t);
    }

    NonlinearHost host;
    int type = CLIP;
    float driveAmt = 0.3f, toneValue = 0.5f, preHpHz = 20.0f, postLpHz = 20000.0f, lowKeep = 20.0f, mix = 1.0f;
    float lastPreHp = -1.0f, lastPostLp = -1.0f;
    Smoothed drive;
    std::array<Biquad, 2> preHp, postLp, postLp2, tone;
    std::array<DcBlocker, 2> dc;
};
} // namespace

std::unique_ptr<Effect> createMenace() { return std::make_unique<Menace>(); }
} // namespace ek
