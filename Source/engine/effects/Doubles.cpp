// Doubles – multi-voice chorus (1–4 voices) with stereo spread and "low keep" (mono, chorus-free lows).
#include "AllEffects.h"

namespace ek
{
EffectInfo makeDoublesInfo()
{
    EffectInfo i;
    i.type = EffectType::Doubles;
    i.id = "doubles";
    i.name = "Doubles";
    i.colour = juce::Colour (0xff4fc3e8);
    i.category = Category::Modulation;
    i.params = { pHz ("RATE", 0.05f, 5.0f, 0.6f, 0.8f),
                 pPercent ("DEPTH", 0.4f),
                 pUnit ("VOICES", Unit::Voices, 1.0f, 4.0f, 2.0f),
                 pPercent ("SPREAD", 0.8f),
                 pHz ("LOW KEEP", 20.0f, 300.0f, 20.0f, 120.0f),
                 pPercent ("MIX", 0.5f),
                 pUnused(), pUnused() };
    i.mainParam = 5;
    i.mixParam = 5;
    i.lowKeepParam = 4;
    i.macro[Drip] = { 0.2f, 0.35f, 0, 0, 0, 0.05f, 0, 0 };
    i.macro[Aura] = { 0, 0, 0, 0.15f, 0, 0.1f, 0, 0 };
    return i;
}

namespace
{
class Doubles final : public Effect
{
public:
    Doubles() : Effect (EffectType::Doubles) {}

    void onPrepare() override
    {
        for (auto& d : lines) d.prepare ((int) std::ceil (0.05f * sr()) + 8);
        for (auto& x : xo) x.prepare (spec.sampleRate);
        for (auto* s : { &rate, &depth, &spread, &mix }) s->reset (spec.sampleRate, 40.0f, s->getTarget());
    }

    void reset() override
    {
        for (auto& d : lines) d.reset();
        for (auto& x : xo) x.reset();
        phase = 0.0f;
        for (auto* s : { &rate, &depth, &spread, &mix }) s->snap (s->getTarget());
    }

    void applyParameters() override
    {
        rate.setTarget (getReal (0));
        depth.setTarget (getReal (1));
        voices = juce::jlimit (1, 4, choice (2));
        spread.setTarget (getReal (3));
        lowKeep = getReal (4);
        mix.setTarget (getReal (5));
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        const float s = sr();
        const float msToS = s * 0.001f;
        for (auto& x : xo) x.setCutoff (lowKeep);
        static constexpr float baseMs[4] = { 11.0f, 17.0f, 23.5f, 14.0f };
        static constexpr float pan[4] = { -1.0f, 1.0f, -0.4f, 0.4f };
        const float voiceNorm = 1.0f / std::sqrt ((float) voices);

        for (int i = 0; i < n; ++i)
        {
            const float rt = rate.next(), dp = depth.next(), sp = spread.next(), m = mix.next();
            phase += rt / s;
            if (phase >= 1.0f) phase -= 1.0f;

            float loL, hiL, loR, hiR;
            xo[0].process (L[i], loL, hiL);
            xo[1].process (R[i], loR, hiR);
            const float lowMono = 0.5f * (loL + loR);

            lines[0].push (hiL);
            lines[1].push (hiR);

            float wl = 0.0f, wr = 0.0f;
            for (int v = 0; v < voices; ++v)
            {
                const float ph = phase + (float) v / (float) voices + 0.13f * (float) v;
                const float lfoL = std::sin (kTwoPi * ph);
                const float lfoR = std::sin (kTwoPi * (ph + 0.25f * sp));
                const float dl = (baseMs[v] + dp * 6.0f * (0.5f + 0.5f * lfoL)) * msToS;
                const float dr = (baseMs[v] + dp * 6.0f * (0.5f + 0.5f * lfoR)) * msToS;
                const float vl = lines[0].readCubic (dl);
                const float vr = lines[1].readCubic (dr);
                // equal-power pan by spread
                const float p = pan[v] * sp;
                const float gl = std::sqrt (0.5f * (1.0f - p)), gr = std::sqrt (0.5f * (1.0f + p));
                const float mono = 0.5f * (vl + vr);
                wl += (mono * (1.0f - sp) + vl * sp) * gl * 1.4142f;
                wr += (mono * (1.0f - sp) + vr * sp) * gr * 1.4142f;
            }
            wl *= voiceNorm; wr *= voiceNorm;

            L[i] = lowMono + hiL + (wl - hiL) * m;
            R[i] = lowMono + hiR + (wr - hiR) * m;
        }
    }

private:
    std::array<DelayLine, 2> lines;
    std::array<LR4Crossover, 2> xo;
    Smoothed rate, depth, spread, mix;
    int voices = 2;
    float lowKeep = 20.0f, phase = 0.0f;
};
} // namespace

std::unique_ptr<Effect> createDoubles() { return std::make_unique<Doubles>(); }
} // namespace ek
