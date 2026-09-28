// Through The Wall – multimode filter (LP12 / LP24 / HP / BP / NOTCH / COMB) with drive,
// tempo-synced LFO, envelope follower and band-pass width in octaves.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeThroughTheWallInfo()
{
    EffectInfo i;
    i.type = EffectType::ThroughTheWall;
    i.id = "through_the_wall";
    i.name = "Through The Wall";
    i.colour = juce::Colour (0xff2f6fd6);
    i.category = Category::Filter;
    i.params = { pChoice ("TYPE", { "LP12", "LP24", "HP", "BP", "NOTCH", "COMB" }, 1),
                 pHz ("CUTOFF", 20.0f, 20000.0f, 2000.0f, 1000.0f),
                 pPercent ("RESO", 0.15f),
                 pPercent ("DRIVE", 0.0f),
                 pSync ("LFO RATE", "1/4"),
                 pPercent ("LFO DEPTH", 0.0f),
                 pBipolar ("ENV", 0.0f),
                 pUnit ("BP WIDTH", Unit::Octaves, 0.2f, 6.0f, 1.0f, 1.5f) };
    i.mainParam = 1;
    i.macro[VillainArc] = { 0, -0.35f, 0, 0, 0, 0, 0, 0 };
    i.macro[CrashOut]   = { 0, 0, 0.1f, 0.4f, 0, 0, 0, 0 };
    i.macro[Drip]       = { 0, 0, 0, 0, 0, 0.4f, 0, 0 };
    return i;
}

namespace
{
class ThroughTheWall final : public Effect
{
public:
    ThroughTheWall() : Effect (EffectType::ThroughTheWall) {}

    enum Type { LP12 = 0, LP24, HP, BP, NOTCH, COMB };

    void onPrepare() override
    {
        for (auto& c : comb) c.prepare ((int) std::ceil (sr() / 20.0f) + 8);
        cutoffOct.reset (spec.sampleRate, 25.0f, std::log2 (2000.0f));
        reso.reset (spec.sampleRate, 25.0f, 0.15f);
        drive.reset (spec.sampleRate, 25.0f, 0.0f);
        lfoDepth.reset (spec.sampleRate, 40.0f, 0.0f);
        envAmt.reset (spec.sampleRate, 40.0f, 0.0f);
        width.reset (spec.sampleRate, 25.0f, 1.0f);
        env.set (4.0f, 160.0f, sr());
        for (auto& d : dc) d.prepare (spec.sampleRate);
    }

    void reset() override
    {
        for (auto& f : f1) f.reset();
        for (auto& f : f2) f.reset();
        for (auto& c : comb) c.reset();
        for (auto& d : dc) d.reset();
        env.reset();
        combLp[0] = combLp[1] = 0.0f;
        for (auto* s : { &cutoffOct, &reso, &drive, &lfoDepth, &envAmt, &width }) s->snap (s->getTarget());
        counter = 0;
    }

    void applyParameters() override
    {
        const int newType = choice (0);
        if (newType != type)
        {
            type = newType;
            for (auto& f : f1) f.reset();
            for (auto& f : f2) f.reset();
        }
        cutoffOct.setTarget (std::log2 (getReal (1)));
        reso.setTarget (getReal (2));
        drive.setTarget (getReal (3));
        lfoBeats = syncBeats (getReal (4));
        lfoDepth.setTarget (getReal (5));
        envAmt.setTarget (getReal (6));
        width.setTarget (getReal (7));
    }

    void process (float* L, float* R, int n, const ProcessContext& ctx) override
    {
        lfo.advanceBlock (ctx, lfoBeats, n);
        const float s = sr();

        for (int i = 0; i < n; ++i)
        {
            const float lfoVal = std::sin (kTwoPi * (float) lfo.tick());
            const float e = env.process (0.5f * (L[i] + R[i]));
            const float oct = cutoffOct.next();
            const float q = reso.next();
            const float drv = drive.next();
            const float depth = lfoDepth.next();
            const float ea = envAmt.next();
            const float bw = width.next();

            if (counter-- <= 0)
            {
                counter = 15;
                float modOct = oct + depth * 3.0f * lfoVal + ea * 5.0f * std::min (1.0f, std::sqrt (e) * 1.4f);
                const float hz = juce::jlimit (20.0f, std::min (20000.0f, s * 0.45f), std::exp2 (modOct));
                updateFilters (hz, q, bw);
                currentHz = hz;
            }

            float l = L[i], r = R[i];
            if (drv > 0.001f)
            {
                const float g = 1.0f + drv * 10.0f;
                const float comp = 1.0f / std::sqrt (g);
                l = fastTanh (l * g) * comp;
                r = fastTanh (r * g) * comp;
            }

            L[i] = filter (0, l, q);
            R[i] = filter (1, r, q);
        }
    }

private:
    void updateFilters (float hz, float q, float bwOct)
    {
        const float s = sr();
        switch (type)
        {
            case LP12: case HP:
                for (auto& f : f1) f.set (hz, 0.707f + q * q * 14.0f, s);
                break;
            case LP24:
                for (auto& f : f1) f.set (hz, 0.54f, s);
                for (auto& f : f2) f.set (hz, 1.31f + q * q * 12.0f, s);
                break;
            case BP:
            {
                const float p = std::exp2 (bwOct);
                const float bwQ = std::sqrt (p) / (p - 1.0f);
                for (auto& f : f1) f.set (hz, bwQ * (1.0f + q * 3.0f), s);
                break;
            }
            case NOTCH:
                for (auto& f : f1) f.set (hz, 0.4f + q * 6.0f, s);
                break;
            case COMB:
            default:
                combDelay = juce::jlimit (2.0f, (float) comb[0].capacity() - 4.0f, s / hz);
                break;
        }
    }

    inline float filter (int ch, float x, float q) noexcept
    {
        auto& a = f1[(size_t) ch];
        switch (type)
        {
            case LP12:  return a.lp (x);
            case LP24:  return f2[(size_t) ch].lp (a.lp (x));
            case HP:    return a.hp (x);
            case BP:    return a.bpNorm (x);
            case NOTCH: return a.notch (x);
            case COMB:
            default:
            {
                auto& d = comb[(size_t) ch];
                const float fb = 0.2f + q * 0.75f;
                float delayed = d.readLinear (combDelay);
                combLp[(size_t) ch] += (delayed - combLp[(size_t) ch]) * 0.7f;
                delayed = combLp[(size_t) ch];
                const float w = x + sanitize (fastTanh (delayed * fb));
                d.push (w);
                return dc[(size_t) ch].process (0.5f * (x + delayed)) * 1.1f;
            }
        }
    }

    int type = LP24;
    float lfoBeats = 1.0f, currentHz = 2000.0f, combDelay = 20.0f;
    int counter = 0;
    Smoothed cutoffOct, reso, drive, lfoDepth, envAmt, width;
    EnvelopeFollower env;
    SyncPhase lfo;
    std::array<Svf, 2> f1, f2;
    std::array<DelayLine, 2> comb;
    std::array<DcBlocker, 2> dc;
    std::array<float, 2> combLp {};
};
} // namespace

std::unique_ptr<Effect> createThroughTheWall() { return std::make_unique<ThroughTheWall>(); }
} // namespace ek
