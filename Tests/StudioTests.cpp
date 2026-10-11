// Rack workflow features: SOLO (one unit only), POWER-off keeps tails, RECYCLE feedback.
#include "TestHelpers.h"

using namespace ektest;

class StudioTest : public juce::UnitTest
{
public:
    StudioTest() : juce::UnitTest ("Rack studio features", "rack") {}

    static RackParams defaults (const RackConfig& cfg)
    {
        RackParams p;
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto& info = effectInfo (cfg.slots[(size_t) s].type);
            for (int k = 0; k < kNumParams; ++k) p.slots[(size_t) s].p[(size_t) k] = info.params[(size_t) k].defaultNorm();
        }
        return p;
    }

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("SOLO plays one unit only");
        {
            // slot 0: Wide Body width 0 (kills a pure side signal), slot 1: Tone Up (flat)
            RackConfig cfg;
            cfg.slots[0].type = EffectType::WideBody;
            cfg.slots[1].type = EffectType::ToneUp;
            auto params = defaults (cfg);
            params.slots[0].p[0] = 0.0f;
            auto sideRms = [&] (int solo)
            {
                cfg.solo = solo;
                Rack rack;
                rack.setConfig (cfg);
                rack.prepare (sr, 1);
                const int n = 24000;
                std::vector<float> L ((size_t) n), R ((size_t) n);
                for (int i = 0; i < n; ++i) { L[(size_t) i] = 0.2f * std::sin (0.05f * (float) i); R[(size_t) i] = -L[(size_t) i]; }
                ProcessContext ctx; ctx.sampleRate = sr;
                for (int pos = 0; pos < n; pos += 256) rack.process (L.data() + pos, R.data() + pos, std::min (256, n - pos), params, ctx);
                return rms (L.data() + n / 2, n / 2);
            };
            expect (sideRms (-1) < 0.01, "without solo the width-0 unit is heard");
            expect (sideRms (1) > 0.12, "solo on Tone Up must skip the Wide Body unit");
            expect (sideRms (0) < 0.01, "solo on Wide Body must still apply it");
        }

        beginTest ("POWER off keeps the delay tail");
        {
            RackConfig cfg;
            cfg.slots[0].type = EffectType::AdLibThrow;
            auto params = defaults (cfg);
            params.slots[0].mix = 1.0f;
            params.slots[0].pause = true;
            Rack rack;
            rack.setConfig (cfg);
            rack.prepare (sr, 1);
            ProcessContext ctx; ctx.sampleRate = sr;
            const int n = (int) sr; // 1 s blocks
            std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
            for (int i = 0; i < 2000; ++i) L[(size_t) i] = R[(size_t) i] = 0.3f * std::sin (0.1f * (float) i);
            for (int pos = 0; pos < n; pos += 256) rack.process (L.data() + pos, R.data() + pos, std::min (256, n - pos), params, ctx);
            // powered back on with silence at the input: the echoes that built up while off must play
            params.slots[0].pause = false;
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            for (int pos = 0; pos < n; pos += 256) rack.process (L.data() + pos, R.data() + pos, std::min (256, n - pos), params, ctx);
            expect (rms (L.data(), n) > 1.0e-4, "tail must ring on after POWER on");
            expect (allFinite (L.data(), n), "no NaN");
        }

        beginTest ("RECYCLE stays bounded");
        {
            for (auto t : { EffectType::AdLibThrow, EffectType::AuraRoom, EffectType::Menace, EffectType::ToneUp })
            {
                Engine e;
                RackConfig cfg;
                cfg.slots[0].type = t;
                cfg.slots[1].type = EffectType::CassettePlug;
                e.setRackConfig (cfg);
                e.prepare (sr, 512, 1);
                EngineInput in;
                in.base = defaults (cfg);
                for (auto& s : in.base.slots) s.mix = 1.0f;
                in.global.recycle = 1.0f;
                ProcessContext ctx; ctx.sampleRate = sr;
                juce::AudioBuffer<float> buf (2, 333); // odd host block size on purpose
                juce::Random rnd (7);
                float peak = 0.0f;
                bool finite = true;
                for (int b = 0; b < 1500; ++b) // ~10 s
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < buf.getNumSamples(); ++i)
                            buf.setSample (c, i, b < 150 ? 0.5f * (rnd.nextFloat() * 2.0f - 1.0f) : 0.0f);
                    e.process (buf, in, ctx);
                    for (int c = 0; c < 2; ++c)
                    {
                        finite = finite && allFinite (buf.getReadPointer (c), buf.getNumSamples(), 1.0e6f);
                        peak = std::max (peak, buf.getMagnitude (c, 0, buf.getNumSamples()));
                    }
                }
                expect (finite, "RECYCLE: no NaN/Inf for " + effectInfo (t).name);
                expect (peak < 8.0f, "RECYCLE must not run away for " + effectInfo (t).name + " (peak " + juce::String (peak) + ")");
            }
        }
    }
};
static StudioTest studioTest;
