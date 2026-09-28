// Cassette Plug – tape / tube saturation with bias (even harmonics), tone, wow, hiss and low keep.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeCassettePlugInfo()
{
    EffectInfo i;
    i.type = EffectType::CassettePlug;
    i.id = "cassette_plug";
    i.name = "Cassette Plug";
    i.colour = juce::Colour (0xffd8342c);
    i.category = Category::Saturation;
    i.params = { pChoice ("MODE", { "TAPE", "TUBE" }, 0),
                 pPercent ("DRIVE", 0.3f),
                 pPercent ("BIAS", 0.1f),
                 pPercent ("TONE", 0.5f),
                 pPercent ("WOW", 0.0f),
                 pPercent ("HISS", 0.0f),
                 pHz ("LOW KEEP", 20.0f, 250.0f, 20.0f, 80.0f),
                 pPercent ("MIX", 1.0f) };
    i.mainParam = 1;
    i.mixParam = 7;
    i.lowKeepParam = 6;
    i.oversampled = true;
    i.macro[VillainArc] = { 0, 0, 0, -0.45f, 0, 0, 0, 0 };
    i.macro[CrashOut]   = { 0, 0.45f, 0.15f, 0, 0, 0, 0, 0 };
    i.macro[Drip]       = { 0, 0, 0, 0, 0.4f, 0, 0, 0 };
    return i;
}

namespace
{
class CassettePlug final : public Effect
{
public:
    CassettePlug() : Effect (EffectType::CassettePlug) {}

    void onPrepare() override
    {
        host.prepare (spec, true, true);
        const float s = sr();
        const int maxWow = (int) std::ceil (0.004f * s * 4.0f) + 8;
        for (auto& d : wowDelay) d.prepare (maxWow);
        const double osRate = spec.sampleRate * (1 << spec.osFactorLog2);
        drive.reset (osRate, 30.0f, 1.0f);
        bias.reset (osRate, 30.0f, 0.0f);
        wowDepth.reset (osRate, 50.0f, 0.0f);
        hissLevel.reset (spec.sampleRate, 50.0f, 0.0f);
        for (auto& d : dc) d.prepare (spec.sampleRate);
        hissHp.setHighPass (1800.0f, 0.707f, s);
        hissHp2 = hissHp;
        headBump.setPeak (90.0f, 0.9f, 1.5f, s);
        for (auto& b : bump) b.copyCoeffs (headBump);
        updateTone();
    }

    void reset() override
    {
        host.reset();
        for (auto& d : wowDelay) d.reset();
        for (auto& d : dc) d.reset();
        for (auto& b : bump) b.reset();
        for (auto& t : tone) t.reset();
        for (auto& p : preEmph) p.reset();
        for (auto& p : deEmph) p.reset();
        hissHp.reset(); hissHp2.reset();
        drive.snap (drive.getTarget());
        bias.snap (bias.getTarget());
        wowDepth.snap (wowDepth.getTarget());
        hissLevel.snap (hissLevel.getTarget());
        wowPhase = flutterPhase = 0.0f;
    }

    void applyParameters() override
    {
        tube = choice (0) == 1;
        const float d = getReal (1);
        drive.setTarget (dbToGain (d * 26.0f));
        bias.setTarget (getReal (2));
        toneValue = getReal (3);
        wowDepth.setTarget (getReal (4));
        hissLevel.setTarget (getReal (5));
        lowKeep = getReal (6);
        mix = getReal (7);
        updateTone();
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        // 1) wow & flutter + saturation of the band above LOW KEEP (oversampled, zero-latency pitch mod)
        host.process (L, R, n, lowKeep, mix, [this] (float* l, float* r, int nOS, int factor)
        {
            const float osRate = sr() * (float) factor;
            const float wowInc = 0.55f / osRate, flutInc = 6.5f / osRate;
            for (int i = 0; i < nOS; ++i)
            {
                const float depth = wowDepth.next();
                wowDelay[0].push (l[i]); wowDelay[1].push (r[i]);
                if (depth > 0.0f)
                {
                    wowPhase += wowInc; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
                    flutterPhase += flutInc; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
                    const float mod = 0.5f + 0.5f * std::sin (kTwoPi * wowPhase)
                                    + 0.06f * std::sin (kTwoPi * flutterPhase);
                    const float delay = 2.0f + depth * depth * 0.0028f * osRate * juce::jlimit (0.0f, 1.2f, mod);
                    l[i] = wowDelay[0].readCubic (delay);
                    r[i] = wowDelay[1].readCubic (delay);
                }
                const float g = drive.next();
                const float b = bias.next() * 0.35f;
                l[i] = saturate (0, l[i], g, b);
                r[i] = saturate (1, r[i], g, b);
            }
        });

        // 3) DC removal, head bump, tone, hiss (base rate)
        for (int i = 0; i < n; ++i)
        {
            float l = dc[0].process (L[i]);
            float r = dc[1].process (R[i]);
            if (! tube) { l = bump[0].process (l); r = bump[1].process (r); }
            l = tone[0].process (l);
            r = tone[1].process (r);
            const float h = hissLevel.next();
            if (h > 0.0f)
            {
                const float amp = h * h * 0.018f;
                l += amp * hissHp.process (noise.next());
                r += amp * hissHp2.process (noise.next());
            }
            L[i] = l; R[i] = r;
        }
    }

    int getLatencySamples() const noexcept override { return host.getLatency(); }

private:
    inline float saturate (int ch, float x, float g, float b) noexcept
    {
        // Tape: pre-emphasis before the curve, de-emphasis after (highs saturate earlier).
        if (! tube)
        {
            x = preEmph[(size_t) ch].process (x);
            const float y = fastTanh (g * x + b) - fastTanh (b);
            return deEmph[(size_t) ch].process (y) / std::sqrt (g);
        }
        // Tube: asymmetric transfer (softer negative half) + bias -> even harmonics.
        const float v = g * x + b;
        const float y = v >= 0.0f ? fastTanh (v) : fastTanh (0.72f * v) * 1.25f;
        const float y0 = b >= 0.0f ? fastTanh (b) : fastTanh (0.72f * b) * 1.25f;
        return (y - y0) / std::sqrt (g);
    }

    void updateTone()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        const float osRate = s * (float) (1 << spec.osFactorLog2);
        Biquad pe, de;
        pe.setHighShelf (3000.0f, 5.0f, osRate);
        de.setHighShelf (3000.0f, -5.0f, osRate);
        for (auto& p : preEmph) p.copyCoeffs (pe);
        for (auto& d : deEmph) d.copyCoeffs (de);
        Biquad t;
        t.setHighShelf (2500.0f, (toneValue - 0.5f) * 16.0f, s, 0.8f);
        for (auto& x : tone) x.copyCoeffs (t);
    }

    NonlinearHost host;
    bool tube = false;
    float toneValue = 0.5f, lowKeep = 20.0f, mix = 1.0f;
    Smoothed drive, bias, wowDepth, hissLevel;
    std::array<DelayLine, 2> wowDelay;
    std::array<DcBlocker, 2> dc;
    std::array<Biquad, 2> bump, tone, preEmph, deEmph;
    Biquad headBump, hissHp, hissHp2;
    NoiseGen noise;
    float wowPhase = 0.0f, flutterPhase = 0.0f;
};
} // namespace

std::unique_ptr<Effect> createCassettePlug() { return std::make_unique<CassettePlug>(); }
} // namespace ek
