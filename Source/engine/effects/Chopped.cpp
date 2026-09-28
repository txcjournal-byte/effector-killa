// Chopped – rhythmic gate: 16 step patterns, tempo pump (sidechain-like) or a noise gate.
#include "AllEffects.h"

namespace ek
{
EffectInfo makeChoppedInfo()
{
    EffectInfo i;
    i.type = EffectType::Chopped;
    i.id = "chopped";
    i.name = "Chopped";
    i.colour = juce::Colour (0xff3bbf5c);
    i.category = Category::Gate;
    i.params = { pChoice ("MODE", { "PATTERN", "PUMP", "NOISE GATE" }, 0),
                 pUnit ("PATTERN", Unit::Pattern, 0.0f, 15.0f, 0.0f),
                 pSync ("RATE", "1/16"),
                 pPercent ("DEPTH", 1.0f),
                 pPercent ("SMOOTH", 0.3f),
                 pDb ("THRESHOLD", -80.0f, 0.0f, -45.0f, -30.0f),
                 pPercent ("MIX", 1.0f),
                 pUnused() };
    i.mainParam = 3;
    i.mixParam = 6;
    i.macro[Drip]  = { 0, 0, 0, 0.3f, 0, 0, 0, 0 };
    i.macro[Knock] = { 0, 0, 0, 0.1f, -0.15f, 0, 0, 0 };
    return i;
}

// 16-step patterns, bit 15 = first step.
static constexpr uint16_t kChopPatterns[16] = {
    0b1010101010101010, 0b1110111011101110, 0b1011011010110110, 0b1100110011001100,
    0b1111000011110000, 0b1001001001001010, 0b1110110111101101, 0b1000100010001000,
    0b1101101101101101, 0b1111111011111010, 0b1011101110111010, 0b1100101011001010,
    0b1110001110001110, 0b1010111010101111, 0b1111101111111010, 0b1001101110011011 };

uint16_t choppedPattern (int index) { return kChopPatterns[juce::jlimit (0, 15, index)]; }

namespace
{
class Chopped final : public Effect
{
public:
    Chopped() : Effect (EffectType::Chopped) {}

    void onPrepare() override
    {
        depth.reset (spec.sampleRate, 30.0f, 1.0f);
        mix.reset (spec.sampleRate, 30.0f, 1.0f);
        detector.set (1.0f, 60.0f, sr());
        updateSmooth();
    }

    void reset() override
    {
        gain = 1.0f;
        gateOpen = false;
        holdCounter = 0;
        detector.reset();
        depth.snap (depth.getTarget());
        mix.snap (mix.getTarget());
    }

    void applyParameters() override
    {
        mode = choice (0);
        pattern = choppedPattern (choice (1));
        rateBeats = syncBeats (getReal (2));
        depth.setTarget (getReal (3));
        smooth = getReal (4);
        thresholdDb = getReal (5);
        mix.setTarget (getReal (6));
        updateSmooth();
    }

    void process (float* L, float* R, int n, const ProcessContext& ctx) override
    {
        const double period = mode == 0 ? rateBeats * 16.0 : rateBeats;
        phase.advanceBlock (ctx, period, n);

        for (int i = 0; i < n; ++i)
        {
            const float d = depth.next(), m = mix.next();
            const double ph = phase.tick();
            float target = 1.0f;

            if (mode == 0) // PATTERN
            {
                const int step = juce::jlimit (0, 15, (int) (ph * 16.0));
                const bool on = (pattern >> (15 - step)) & 1;
                target = on ? 1.0f : 1.0f - d;
            }
            else if (mode == 1) // PUMP – duck at the start of every period, recover smoothly
            {
                const float p = (float) ph;
                const float shape = 1.0f - std::exp (-p * (4.0f + (1.0f - smooth) * 8.0f));
                const float norm = 1.0f - std::exp (-(4.0f + (1.0f - smooth) * 8.0f));
                target = (1.0f - d) + d * (shape / norm);
            }
            else // NOISE GATE
            {
                const float lvl = gainToDb (detector.process (0.5f * (std::abs (L[i]) + std::abs (R[i]))) + 1.0e-9f);
                if (lvl > thresholdDb) { gateOpen = true; holdCounter = holdSamples; }
                else if (lvl < thresholdDb - 4.0f)
                {
                    if (holdCounter > 0) --holdCounter;
                    else gateOpen = false;
                }
                target = gateOpen ? 1.0f : 1.0f - d;
            }

            const float coeff = mode == 1 ? pumpCoeff : (target > gain ? attackCoeff : releaseCoeff);
            gain = target + (gain - target) * coeff;
            const float g = 1.0f + (gain - 1.0f) * m;
            L[i] *= g;
            R[i] *= g;
        }
    }

private:
    void updateSmooth()
    {
        const float s = sr();
        if (s <= 0.0f) return;
        const float ms = mode == 2 ? 1.0f : 0.5f + smooth * 25.0f;
        const float relMs = mode == 2 ? 20.0f + smooth * 480.0f : ms;
        attackCoeff = std::exp (-1.0f / (ms * 0.001f * s));
        releaseCoeff = std::exp (-1.0f / (relMs * 0.001f * s));
        pumpCoeff = std::exp (-1.0f / (1.5f * 0.001f * s));
        holdSamples = (int) (0.012f * s);
    }

    int mode = 0;
    uint16_t pattern = kChopPatterns[0];
    float rateBeats = 0.25f, smooth = 0.3f, thresholdDb = -45.0f;
    Smoothed depth, mix;
    SyncPhase phase;
    EnvelopeFollower detector;
    float gain = 1.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f, pumpCoeff = 0.0f;
    bool gateOpen = false;
    int holdCounter = 0, holdSamples = 500;
};
} // namespace

std::unique_ptr<Effect> createChopped() { return std::make_unique<Chopped>(); }
} // namespace ek
