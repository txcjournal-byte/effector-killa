#include "TestHelpers.h"

using namespace ektest;

// Live per-unit telemetry for the rack UI (engine/UnitTaps.h): levels, gain reduction, activity, scope.
class UnitTapsTest : public juce::UnitTest
{
public:
    UnitTapsTest() : juce::UnitTest ("UnitTaps", "rack") {}

    void runTest() override
    {
        RackConfig cfg;
        cfg.slots[0].type = EffectType::Squeeze;
        cfg.slots[1].type = EffectType::Overcooked;
        cfg.slots[2].type = EffectType::Slap;
        cfg.slots[3].type = EffectType::WideBody;

        auto paramsFor = [&] (float squeezeThresholdNorm)
        {
            RackParams p;
            for (int s = 0; s < kNumSlots; ++s)
            {
                const auto& info = effectInfo (cfg.slots[(size_t) s].type);
                for (int k = 0; k < kNumParams; ++k) p.slots[(size_t) s].p[(size_t) k] = info.params[(size_t) k].defaultNorm();
            }
            p.slots[0].p[0] = squeezeThresholdNorm;  // threshold
            p.slots[0].p[1] = 1.0f;                  // ratio 20:1
            p.slots[2].p[0] = 1.0f;                  // Slap attack
            return p;
        };

        auto run = [&] (Rack& rack, const RackParams& params, float amp, bool bursts)
        {
            ProcessContext ctx; ctx.sampleRate = 48000.0;
            std::vector<float> L (256), R (256);
            int pos = 0;
            for (int b = 0; b < 200; ++b)
            {
                for (int i = 0; i < 256; ++i, ++pos)
                {
                    const float env = bursts ? std::exp (-(float) (pos % 9600) / 600.0f) : 1.0f;
                    L[(size_t) i] = amp * env * std::sin ((float) pos * 0.05f);
                    R[(size_t) i] = amp * env * std::sin ((float) pos * 0.07f);
                }
                rack.process (L.data(), R.data(), 256, params, ctx);
                expect (allFinite (L.data(), 256) && allFinite (R.data(), 256));
            }
        };

        beginTest ("levels, gain reduction, activity and scope reach the UI");
        {
            Rack rack;
            rack.setConfig (cfg);
            rack.prepare (48000.0, 1);
            run (rack, paramsFor (0.2f), 0.7f, true);
            auto& taps = rack.getTaps();
            expect (taps.takeInPeak (0) > 0.3f, "slot input level");
            expect (taps.takeOutPeak (0) > 0.0f, "slot output level");
            expect (taps.takeInPeak (0) == 0.0f, "peaks reset after reading");
            expect (taps.takeGainReduction (0) > 1.0f, "Squeeze reports gain reduction");
            float bands = 0.0f;
            for (int b = 0; b < 3; ++b) bands = std::max (bands, taps.takeActivity (1, b));
            expect (bands > 0.2f, "Overcooked band activity");
            expect (taps.takeActivity (2, 0) > 0.3f, "Slap transient activity");
            std::array<float, UnitTaps::scopeSize> sl {}, sr {};
            taps.copyScope (3, sl.data(), sr.data());
            float m = 0.0f;
            for (auto v : sl) m = std::max (m, std::abs (v));
            expect (m > 0.0f && allFinite (sl.data(), UnitTaps::scopeSize) && allFinite (sr.data(), UnitTaps::scopeSize), "scope ring filled");
            expectEquals (taps.takeInPeak (5), 0.0f);   // empty slot
            expectEquals (taps.takeInPeak (-1), 0.0f);  // out of range is safe
        }

        beginTest ("telemetry does not change the audio");
        {
            Rack a, b;
            for (auto* r : { &a, &b }) { r->setConfig (cfg); r->prepare (48000.0, 1); }
            const auto params = paramsFor (0.4f);
            ProcessContext ctx; ctx.sampleRate = 48000.0;
            std::vector<float> L1 (256), R1 (256), L2 (256), R2 (256);
            float maxDiff = 0.0f;
            for (int blk = 0; blk < 50; ++blk)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float x = 0.5f * std::sin ((float) (blk * 256 + i) * 0.03f);
                    L1[(size_t) i] = L2[(size_t) i] = x;
                    R1[(size_t) i] = R2[(size_t) i] = -x;
                }
                a.process (L1.data(), R1.data(), 256, params, ctx);
                b.process (L2.data(), R2.data(), 256, params, ctx);
                (void) b.getTaps().takeOutPeak (0);   // a reader on one of them only
                for (int i = 0; i < 256; ++i) maxDiff = std::max (maxDiff, std::abs (L1[(size_t) i] - L2[(size_t) i]));
            }
            expectEquals (maxDiff, 0.0f);
        }
    }
};
static UnitTapsTest unitTapsTest;
