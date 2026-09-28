// Aura Room – 8-line feedback delay network reverb (ROOM / PLATE / HALL / DARK)
// with input diffusion, modulated lines, damping, pre-delay, width and ducking.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeAuraRoomInfo()
{
    EffectInfo i;
    i.type = EffectType::AuraRoom;
    i.id = "aura_room";
    i.name = "Aura Room";
    i.colour = juce::Colour (0xffef7d22);
    i.category = Category::Space;
    i.params = { pChoice ("TYPE", { "ROOM", "PLATE", "HALL", "DARK" }, 2),
                 pPercent ("SIZE", 0.5f),
                 pUnit ("DECAY", Unit::Seconds, 0.2f, 20.0f, 2.0f, 2.5f),
                 pMs ("PRE-DELAY", 0.0f, 200.0f, 10.0f, 40.0f),
                 pPercent ("DAMPING", 0.4f),
                 pPercent ("WIDTH", 1.0f),
                 pPercent ("DUCKING", 0.0f),
                 pPercent ("MIX", 0.25f) };
    i.mainParam = 7;
    i.mixParam = 7;
    i.macro[VillainArc] = { 0, 0, 0, 0, 0.45f, 0, 0, 0 };
    i.macro[Aura]       = { 0, 0.15f, 0.2f, 0, 0, 0.1f, 0, 0.3f };
    return i;
}

namespace
{
constexpr int kLines = 8;

class AuraRoom final : public Effect
{
public:
    AuraRoom() : Effect (EffectType::AuraRoom) {}

    void onPrepare() override
    {
        const float s = sr();
        for (auto& d : lines) d.prepare ((int) std::ceil (0.25f * s) + 16);
        for (auto& p : preDelay) p.prepare ((int) std::ceil (0.21f * s) + 4);
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k)
                diffusers[(size_t) c][(size_t) k].prepare ((int) std::ceil (0.02f * s) + 4);
        lengthScale.reset (spec.sampleRate, 250.0f, 1.0f);
        preDelayMs.reset (spec.sampleRate, 100.0f, 10.0f);
        mix.reset (spec.sampleRate, 30.0f, 0.25f);
        width.reset (spec.sampleRate, 30.0f, 1.0f);
        duckEnv.set (8.0f, 320.0f, s);
        for (auto& d : dc) d.prepare (spec.sampleRate);
    }

    void reset() override
    {
        for (auto& d : lines) d.reset();
        for (auto& p : preDelay) p.reset();
        for (auto& c : diffusers) for (auto& d : c) d.reset();
        for (auto& d : damp) d.reset();
        for (auto& d : dc) d.reset();
        for (auto& t : toneL) t.reset();
        duckEnv.reset();
        for (auto* s : { &lengthScale, &preDelayMs, &mix, &width }) s->snap (s->getTarget());
        modPhase = 0.0f;
    }

    void applyParameters() override
    {
        type = choice (0);
        const float size = getReal (1);
        decay = getReal (2);
        preDelayMs.setTarget (getReal (3));
        damping = getReal (4);
        width.setTarget (getReal (5));
        ducking = getReal (6);
        mix.setTarget (getReal (7));

        static constexpr float typeScale[] = { 0.32f, 0.55f, 1.0f, 1.05f };
        lengthScale.setTarget (typeScale[type] * (0.3f + 1.15f * size));

        static constexpr float diff[] = { 0.55f, 0.75f, 0.68f, 0.62f };
        diffusion = diff[type];

        // damping: loop low-pass cutoff
        const float baseHz = type == 3 ? 5500.0f : (type == 1 ? 16000.0f : 12000.0f);
        const float minHz = type == 3 ? 700.0f : 1500.0f;
        dampHz = baseHz * std::pow (minHz / baseHz, damping);
        for (auto& d : damp) d.setCutoff (dampHz, sr());

        Biquad t;
        if (type == 3) t.setLowPass (3200.0f, 0.6f, sr());
        else           t.setIdentity();
        for (auto& x : toneL) x.copyCoeffs (t);
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        static constexpr float baseMs[kLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 59.3f, 67.1f, 73.3f };
        static constexpr float diffMs[4] = { 4.771f, 3.595f, 12.73f, 9.307f };
        static constexpr float outL[kLines] = { 1, -1, 1, -1, 1, 1, -1, -1 };
        static constexpr float outR[kLines] = { 1, 1, -1, -1, -1, 1, 1, -1 };

        const float s = sr();
        const float msToSamp = s * 0.001f;
        const float modDepthSamples = (type == 0 ? 0.08f : 0.35f) * msToSamp;
        const float modInc = 0.43f / s;
        const float t60 = std::max (0.1f, decay * (type == 0 ? 0.7f : 1.0f));

        // per-block feedback gains for the current length scale
        std::array<float, kLines> gains {};
        const float scaleNow = lengthScale.get();
        for (int k = 0; k < kLines; ++k)
        {
            const float lenSec = baseMs[k] * scaleNow * 0.001f;
            gains[(size_t) k] = std::pow (10.0f, -3.0f * lenSec / t60);
        }

        for (int i = 0; i < n; ++i)
        {
            const float dryL = L[i], dryR = R[i];
            const float scale = lengthScale.next();
            const float pd = std::max (1.0f, preDelayMs.next() * msToSamp);

            preDelay[0].push (dryL);
            preDelay[1].push (dryR);
            float inL = preDelay[0].readLinear (pd);
            float inR = preDelay[1].readLinear (pd);

            // input diffusion (Schroeder allpasses)
            for (int k = 0; k < 4; ++k)
            {
                const float dl = diffMs[k] * msToSamp * (0.6f + 0.4f * scale);
                inL = allpass (diffusers[0][(size_t) k], inL, dl, diffusion);
                inR = allpass (diffusers[1][(size_t) k], inR, dl * 1.037f, diffusion);
            }

            modPhase += modInc; if (modPhase >= 1.0f) modPhase -= 1.0f;
            const float m1 = std::sin (kTwoPi * modPhase);
            const float m2 = std::cos (kTwoPi * modPhase * 1.0f);

            std::array<float, kLines> y {};
            float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < kLines; ++k)
            {
                float len = baseMs[k] * scale * msToSamp;
                if (k < 4) len += modDepthSamples * (k & 1 ? m1 : m2) * (1.0f + 0.3f * (float) k);
                float v = lines[(size_t) k].readLinear (std::max (2.0f, len));
                v = damp[(size_t) k].lp (v) * gains[(size_t) k];
                y[(size_t) k] = v;
                wl += outL[k] * v;
                wr += outR[k] * v;
            }

            hadamard (y);
            for (int k = 0; k < kLines; ++k)
            {
                const float in = (k < 4 ? inL : inR) * ((k & 1) ? -0.5f : 0.5f);
                lines[(size_t) k].push (sanitize (y[(size_t) k] + in));
            }

            wl = toneL[0].process (dc[0].process (wl * 0.42f));
            wr = toneL[1].process (dc[1].process (wr * 0.42f));

            // width (M/S on the wet signal)
            const float w = width.next();
            const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * w;
            wl = mid + side; wr = mid - side;

            // ducking by the dry input
            const float e = duckEnv.process (0.5f * (std::abs (dryL) + std::abs (dryR)));
            const float duck = 1.0f - ducking * std::min (1.0f, e * 4.0f);

            const float m = mix.next();
            // send-style mix law: dry stays at unity up to 50 %
            const float dg = std::min (1.0f, 2.0f * (1.0f - m)), wg = std::min (1.0f, 2.0f * m);
            L[i] = dryL * dg + wl * duck * wg;
            R[i] = dryR * dg + wr * duck * wg;
        }
    }

private:
    static inline float allpass (DelayLine& d, float x, float delay, float g) noexcept
    {
        const float delayed = d.readLinear (std::max (1.0f, delay));
        const float v = x + g * delayed;
        d.push (sanitize (v));
        return delayed - g * v;
    }

    static inline void hadamard (std::array<float, kLines>& x) noexcept
    {
        for (int h = 1; h < kLines; h *= 2)
            for (int i = 0; i < kLines; i += 2 * h)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[(size_t) j], b = x[(size_t) (j + h)];
                    x[(size_t) j] = a + b;
                    x[(size_t) (j + h)] = a - b;
                }
        constexpr float norm = 0.35355339f; // 1/sqrt(8)
        for (auto& v : x) v *= norm;
    }

    int type = 2;
    float decay = 2.0f, damping = 0.4f, ducking = 0.0f, diffusion = 0.7f, dampHz = 8000.0f;
    Smoothed lengthScale, preDelayMs, mix, width;
    std::array<DelayLine, kLines> lines;
    std::array<OnePole, kLines> damp;
    std::array<DelayLine, 2> preDelay;
    std::array<std::array<DelayLine, 4>, 2> diffusers;
    std::array<DcBlocker, 2> dc;
    std::array<Biquad, 2> toneL;
    EnvelopeFollower duckEnv;
    float modPhase = 0.0f;
};
} // namespace

std::unique_ptr<Effect> createAuraRoom() { return std::make_unique<AuraRoom>(); }
} // namespace ek
