#include "TestHelpers.h"

using namespace ektest;

class RackTest : public juce::UnitTest
{
public:
    RackTest() : juce::UnitTest ("Rack", "rack") {}

    static RackConfig config (std::initializer_list<EffectType> types)
    {
        RackConfig c;
        int i = 0;
        for (auto t : types) c.slots[(size_t) i++].type = t;
        return c;
    }

    // impulse through a (near-linear) rack: the peak must sit at the fixed latency
    int impulsePeak (const RackConfig& cfg, int os, double sr)
    {
        Rack rack;
        rack.setConfig (cfg);
        rack.prepare (sr, os);
        RackParams params;
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto t = cfg.slots[(size_t) s].type;
            const auto& info = effectInfo (t);
            for (int k = 0; k < kNumParams; ++k) params.slots[(size_t) s].p[(size_t) k] = info.params[(size_t) k].defaultNorm();
            if (t == EffectType::CassettePlug || t == EffectType::Menace)
                params.slots[(size_t) s].p[1] = 0.0f; // no drive
            if (t == EffectType::RedLine)
                params.slots[(size_t) s].p[0] = 1.0f; // ceiling 0 dB
        }
        const int n = 4096;
        std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
        L[100] = R[100] = 0.05f;
        ProcessContext ctx; ctx.sampleRate = sr;
        for (int pos = 0; pos < n; pos += 256) rack.process (L.data() + pos, R.data() + pos, 256, params, ctx);
        int peak = 0;
        for (int i = 0; i < n; ++i) if (std::abs (L[(size_t) i]) > std::abs (L[(size_t) peak])) peak = i;
        return peak - 100;
    }

    void runTest() override
    {
        beginTest ("fixed latency independent of rack content");
        for (int os = 0; os <= 2; ++os)
        {
            const double sr = 48000.0;
            const int fixed = Rack::computeFixedLatency (sr, os);
            const RackConfig configs[] = {
                config ({}),
                config ({ EffectType::ToneUp }),
                config ({ EffectType::CassettePlug, EffectType::Menace, EffectType::RedLine }),
                config ({ EffectType::RedLine, EffectType::RedLine, EffectType::CassettePlug, EffectType::Menace,
                          EffectType::RedLine, EffectType::CassettePlug, EffectType::Menace, EffectType::RedLine }),
            };
            for (auto& c : configs)
            {
                PrepareSpec spec; spec.sampleRate = sr; spec.osFactorLog2 = os;
                Chain chain (c, spec, fixed);
                expectEquals (chain.getTotalLatency(), fixed);
                const int peak = impulsePeak (c, os, sr);
                expect (std::abs (peak - fixed) <= 2, "impulse at " + juce::String (peak) + ", expected " + juce::String (fixed));
            }
            logMessage ("os " + juce::String (1 << os) + "x: fixed latency " + juce::String (fixed) + " samples");
        }

        beginTest ("crossfade chain swap without clicks");
        {
            const double sr = 44100.0;
            Rack rack;
            rack.setConfig (config ({ EffectType::ToneUp }));
            rack.prepare (sr, 1);
            RackParams params;
            for (auto& s : params.slots) for (auto& v : s.p) v = 0.5f;
            ProcessContext ctx; ctx.sampleRate = sr;
            std::vector<float> L (256), R (256);
            double phase = 0.0;
            float prev = 0.0f, maxJump = 0.0f, steadyJump = 0.0f;
            const EffectType seq[] = { EffectType::CassettePlug, EffectType::Squeeze, EffectType::WideBody, EffectType::None, EffectType::Swirl };
            for (int block = 0; block < 400; ++block)
            {
                if (block % 40 == 20)
                {
                    rack.setConfig (config ({ seq[(block / 40) % 5], EffectType::ToneUp }));
                }
                for (int i = 0; i < 256; ++i)
                {
                    phase += 220.0 / sr;
                    L[(size_t) i] = R[(size_t) i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                }
                rack.process (L.data(), R.data(), 256, params, ctx);
                const bool fadingBlock = (block % 40) >= 20 && (block % 40) < 30;
                for (int i = 0; i < 256; ++i)
                {
                    const float jump = std::abs (L[(size_t) i] - prev);
                    if (block > 4) (fadingBlock ? maxJump : steadyJump) = std::max (fadingBlock ? maxJump : steadyJump, jump);
                    prev = L[(size_t) i];
                }
                rack.collectGarbage();
            }
            // a swap must never produce a bigger step than the processed signals themselves
            logMessage ("steady max step " + juce::String (steadyJump, 4) + ", during swaps " + juce::String (maxJump, 4));
            expect (maxJump <= steadyJump * 1.1f + 0.005f, "click during swap: " + juce::String (maxJump));
        }

        beginTest ("PAUSE / MIX / SCREENING / M-S");
        {
            const double sr = 44100.0;
            auto cfg = config ({ EffectType::WideBody });
            auto monoErr = [&] (std::function<void (RackParams&)> setup, MSMode ms, int screening, RackConfig c)
            {
                c.slots[0].ms = ms;
                c.screening = screening;
                Rack rack;
                rack.setConfig (c);
                rack.prepare (sr, 0);
                RackParams params;
                for (int s = 0; s < kNumSlots; ++s)
                {
                    const auto& info = effectInfo (c.slots[(size_t) s].type);
                    for (int k = 0; k < kNumParams; ++k) params.slots[(size_t) s].p[(size_t) k] = info.params[(size_t) k].defaultNorm();
                }
                setup (params);
                ProcessContext ctx; ctx.sampleRate = sr;
                std::vector<float> L (256), R (256);
                double phase = 0.0;
                double sumL = 0, sumR = 0, sumD = 0;
                for (int blk = 0; blk < 60; ++blk)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        phase += 1000.0 / sr;
                        L[(size_t) i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                        R[(size_t) i] = -L[(size_t) i];
                    }
                    rack.process (L.data(), R.data(), 256, params, ctx);
                    if (blk >= 40)
                        for (int i = 0; i < 256; ++i)
                        {
                            sumL += L[(size_t) i] * L[(size_t) i];
                            sumR += R[(size_t) i] * R[(size_t) i];
                            sumD += (L[(size_t) i] - R[(size_t) i]) * (L[(size_t) i] - R[(size_t) i]);
                        }
                }
                const double n = 20 * 256;
                return std::array<double, 3> { std::sqrt (sumL / n), std::sqrt (sumR / n), std::sqrt (sumD / n) };
            };
            const double inRms = 0.3 / std::sqrt (2.0);

            // width 0 -> mono: side cancelled completely
            auto mono = monoErr ([] (RackParams& p) { p.slots[0].p[0] = 0.0f; }, MSMode::Stereo, -1, cfg);
            expect (mono[0] < 0.01 && mono[2] < 0.01, "width 0 should cancel a pure side signal");
            // paused -> untouched
            auto paused = monoErr ([] (RackParams& p) { p.slots[0].p[0] = 0.0f; p.slots[0].pause = true; }, MSMode::Stereo, -1, cfg);
            expectWithinAbsoluteError (paused[0], inRms, 0.005);
            // mix 0.5 -> half level
            auto half = monoErr ([] (RackParams& p) { p.slots[0].p[0] = 0.0f; p.slots[0].mix = 0.5f; }, MSMode::Stereo, -1, cfg);
            expectWithinAbsoluteError (half[0], inRms * 0.5, 0.005);
            // M/S: Through The Wall LP at 20 Hz removes whatever component it is applied to
            auto lp = config ({ EffectType::ThroughTheWall });
            auto kill = [] (RackParams& p) { p.slots[0].p[0] = effectInfo (EffectType::ThroughTheWall).params[0].fromReal (1.0f); p.slots[0].p[1] = 0.0f; };
            auto midOnly = monoErr (kill, MSMode::Mid, -1, lp);
            expectWithinAbsoluteError (midOnly[0], inRms, 0.01);      // pure side input survives
            auto sideOnly = monoErr (kill, MSMode::Side, -1, lp);
            expect (sideOnly[0] < 0.01, "SIDE mode: the side component must be filtered away");
            // SCREENING slot 0 (Tone Up flat): slot 1 (width 0) is not heard
            auto sc = config ({ EffectType::ToneUp, EffectType::WideBody });
            auto screened = monoErr ([] (RackParams& p) { p.slots[1].p[0] = 0.0f; }, MSMode::Stereo, 0, sc);
            expectWithinAbsoluteError (screened[0], inRms, 0.005);
            auto notScreened = monoErr ([] (RackParams& p) { p.slots[1].p[0] = 0.0f; }, MSMode::Stereo, -1, sc);
            expect (notScreened[0] < 0.01, "without screening slot 2 must be heard");
        }
    }
};
static RackTest rackTest;
