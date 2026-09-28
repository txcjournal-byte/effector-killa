// Brainrot – bitcrusher / lo-fi: bit depth, sample-rate reduction with jitter, noise, crackle, tone, mix.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeBrainrotInfo()
{
    EffectInfo i;
    i.type = EffectType::Brainrot;
    i.id = "brainrot";
    i.name = "Brainrot";
    i.colour = juce::Colour (0xffd94fa0);
    i.category = Category::LoFi;
    i.params = { pUnit ("BITS", Unit::Bits, 4.0f, 16.0f, 12.0f),
                 pHz ("RATE", 1000.0f, 44100.0f, 44100.0f, 11000.0f),
                 pPercent ("JITTER", 0.0f),
                 pPercent ("NOISE", 0.0f),
                 pPercent ("CRACKLE", 0.0f),
                 pPercent ("TONE", 0.6f),
                 pPercent ("MIX", 1.0f),
                 pUnused() };
    i.mainParam = 6;
    i.mixParam = 6;
    i.oversampled = true;
    i.macro[VillainArc] = { 0, 0, 0, 0, 0, -0.35f, 0, 0 };
    i.macro[CrashOut]   = { -0.4f, -0.2f, 0.1f, 0.1f, 0, 0, 0, 0 };
    i.macro[Drip]       = { 0, 0, 0.3f, 0, 0, 0, 0, 0 };
    return i;
}

namespace
{
class Brainrot final : public Effect
{
public:
    Brainrot() : Effect (EffectType::Brainrot) {}

    void onPrepare() override
    {
        host.prepare (spec, true, false);
        bitsSm.reset (spec.sampleRate, 20.0f, 12.0f);
        noiseSm.reset (spec.sampleRate, 30.0f, 0.0f);
        for (auto& c : crackleLp) c.setCutoff (3500.0f, sr());
        updateTone();
    }

    void reset() override
    {
        host.reset();
        held[0] = held[1] = 0.0f;
        holdPhase = 0.0f;
        for (auto& t : tone) t.reset();
        for (auto& t : tone2) t.reset();
        for (auto& c : crackleLp) c.reset();
        crackleEnv = 0.0f;
        bitsSm.snap (bitsSm.getTarget());
        noiseSm.snap (noiseSm.getTarget());
    }

    void applyParameters() override
    {
        bits = getReal (0);
        bitsSm.setTarget (bits);
        rateHz = getReal (1);
        jitter = getReal (2);
        noiseSm.setTarget (getReal (3));
        crackle = getReal (4);
        toneValue = getReal (5);
        mix = getReal (6);
        updateTone();
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        const float s = sr();
        // noise + crackle are added before crushing so they get the same lo-fi treatment
        for (int i = 0; i < n; ++i)
        {
            const float nz = noiseSm.next();
            if (nz > 0.0f)
            {
                const float amp = nz * nz * 0.03f;
                L[i] += amp * noise.next();
                R[i] += amp * noise.next();
            }
            if (crackle > 0.0f)
            {
                if (noise.next() * 0.5f + 0.5f < crackle * crackle * 0.0009f * (44100.0f / s))
                    crackleEnv = (0.3f + 0.7f * std::abs (noise.next())) * (0.04f + crackle * 0.12f) * (noise.next() > 0 ? 1.0f : -1.0f);
                const float c = crackleEnv;
                crackleEnv *= 0.55f;
                L[i] += crackleLp[0].lp (c);
                R[i] += crackleLp[1].lp (c * 0.8f);
            }
        }

        host.process (L, R, n, 0.0f, mix, [this] (float* l, float* r, int nOS, int factor)
        {
            const float osRate = sr() * (float) factor;
            const float baseStep = rateHz >= 43000.0f ? 1.0f : std::min (1.0f, rateHz / osRate);
            for (int i = 0; i < nOS; ++i)
            {
                const float b = bitsSm.next();
                const float levels = std::exp2 (b - 1.0f);
                const float inv = 1.0f / levels;

                float step = baseStep;
                if (jitter > 0.0f) step *= 1.0f + jitter * 0.9f * rnd.next();
                holdPhase += step;
                if (holdPhase >= 1.0f)
                {
                    holdPhase -= std::floor (holdPhase);
                    held[0] = std::round (juce::jlimit (-1.5f, 1.5f, l[i]) * levels) * inv;
                    held[1] = std::round (juce::jlimit (-1.5f, 1.5f, r[i]) * levels) * inv;
                }
                l[i] = held[0];
                r[i] = held[1];
            }
        });

        for (int i = 0; i < n; ++i)
        {
            L[i] = tone2[0].process (tone[0].process (L[i]));
            R[i] = tone2[1].process (tone[1].process (R[i]));
        }
    }

    int getLatencySamples() const noexcept override { return host.getLatency(); }

private:
    void updateTone()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        // tone: 0 = dark (1.5 kHz LP), 1 = open
        const float hz = 1500.0f * std::pow (std::min (20000.0f, s * 0.45f) / 1500.0f, toneValue);
        Biquad a, b;
        a.setLowPass (hz, 0.54f, s);
        b.setLowPass (hz, 1.31f, s);
        for (auto& t : tone) t.copyCoeffs (a);
        for (auto& t : tone2) t.copyCoeffs (b);
    }

    NonlinearHost host;
    float bits = 12.0f, rateHz = 44100.0f, jitter = 0.0f, crackle = 0.0f, toneValue = 0.6f, mix = 1.0f;
    Smoothed bitsSm, noiseSm;
    float held[2] {}, holdPhase = 0.0f, crackleEnv = 0.0f;
    std::array<Biquad, 2> tone, tone2;
    std::array<OnePole, 2> crackleLp;
    NoiseGen noise, rnd;
};
} // namespace

std::unique_ptr<Effect> createBrainrot() { return std::make_unique<Brainrot>(); }
} // namespace ek
