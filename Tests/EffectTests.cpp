#include "TestHelpers.h"

using namespace ektest;

// Every effect: stability, no NaN/Inf with extreme parameters, several sample rates and block sizes.
class EffectStabilityTest : public juce::UnitTest
{
public:
    EffectStabilityTest() : juce::UnitTest ("Effect stability", "effects") {}

    void runTest() override
    {
        const double rates[] = { 44100.0, 48000.0, 96000.0 };
        const int blocks[] = { 1, 17, 256, kMaxChunk };
        Rng rng (42);

        for (auto type : allEffectTypes())
        {
            const auto& info = effectInfo (type);
            beginTest (info.name);
            for (double sr : rates)
            {
                for (int os = 0; os <= 2; os += (sr > 50000.0 ? 2 : 1))
                {
                    auto fx = createEffect (type);
                    PrepareSpec spec;
                    spec.sampleRate = sr;
                    spec.osFactorLog2 = os;
                    fx->prepare (spec);
                    expectEquals (fx->getLatencySamples(), fx->getLatencySamples());

                    std::vector<float> L ((size_t) kMaxChunk), R ((size_t) kMaxChunk);
                    NoiseGen noise;
                    ProcessContext ctx;
                    ctx.sampleRate = sr;
                    bool ok = true;
                    double phase = 0.0;

                    // parameter scenarios: all min, all max, default, 6 random
                    for (int scenario = 0; scenario < 9; ++scenario)
                    {
                        std::array<float, kNumParams> p {};
                        for (int k = 0; k < kNumParams; ++k)
                        {
                            if (scenario == 0) p[(size_t) k] = 0.0f;
                            else if (scenario == 1) p[(size_t) k] = 1.0f;
                            else if (scenario == 2) p[(size_t) k] = info.params[(size_t) k].defaultNorm();
                            else p[(size_t) k] = rng.uniform();
                        }
                        fx->setParameters (p.data());

                        for (int b = 0; b < 4 && ok; ++b)
                        {
                            const int n = blocks[b];
                            for (int rep = 0; rep < std::max (1, 2048 / n) && ok; ++rep)
                            {
                                for (int i = 0; i < n; ++i)
                                {
                                    phase += 110.0 / sr;
                                    const float s = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                                    // loud: sine + noise up to +12 dBFS, with silent gaps
                                    const float gate = (rep % 7 == 3) ? 0.0f : 1.0f;
                                    L[(size_t) i] = gate * (2.0f * s + 1.5f * noise.next());
                                    R[(size_t) i] = gate * (2.0f * s - 1.5f * noise.next());
                                }
                                fx->process (L.data(), R.data(), n, ctx);
                                ctx.ppq += n * ctx.beatsPerSample();
                                ok = allFinite (L.data(), n, 200.0f) && allFinite (R.data(), n, 200.0f);
                            }
                        }
                    }
                    expect (ok, info.name + " unstable at " + juce::String (sr) + " Hz, os " + juce::String (os));

                    // silence after reset stays (almost) silent – no DC / self-oscillation runaway
                    fx->reset();
                    std::array<float, kNumParams> def {};
                    for (int k = 0; k < kNumParams; ++k) def[(size_t) k] = info.params[(size_t) k].defaultNorm();
                    fx->setParameters (def.data());
                    double maxAbs = 0.0;
                    for (int rep = 0; rep < 40; ++rep)
                    {
                        std::fill (L.begin(), L.end(), 0.0f);
                        std::fill (R.begin(), R.end(), 0.0f);
                        fx->process (L.data(), R.data(), 256, ctx);
                        for (int i = 0; i < 256; ++i) maxAbs = std::max (maxAbs, (double) std::abs (L[(size_t) i]));
                    }
                    expect (maxAbs < 0.05, info.name + " not silent on silence: " + juce::String (maxAbs));
                }
            }
        }
    }
};
static EffectStabilityTest effectStabilityTest;

// LOW KEEP: content below the crossover passes (almost) unchanged even with heavy drive.
class LowKeepTest : public juce::UnitTest
{
public:
    LowKeepTest() : juce::UnitTest ("Low keep", "effects") {}

    void runTest() override
    {
        struct Case { EffectType type; std::vector<std::pair<const char*, float>> params; };
        const std::vector<Case> cases {
            { EffectType::CassettePlug, { { "DRIVE", 1.0f }, { "LOW KEEP", 250.0f }, { "BIAS", 0.6f } } },
            { EffectType::Menace, { { "DRIVE", 1.0f }, { "LOW KEEP", 250.0f }, { "TYPE", 0.0f } } },
            { EffectType::Doubles, { { "DEPTH", 1.0f }, { "LOW KEEP", 300.0f }, { "MIX", 1.0f } } },
        };
        const double sr = 48000.0;
        for (auto& c : cases)
        {
            const auto& info = effectInfo (c.type);
            beginTest (info.name);
            auto fx = createEffect (c.type);
            PrepareSpec spec; spec.sampleRate = sr; spec.osFactorLog2 = 1;
            fx->prepare (spec);
            std::array<float, kNumParams> p {};
            for (int k = 0; k < kNumParams; ++k) p[(size_t) k] = info.params[(size_t) k].defaultNorm();
            for (auto& kv : c.params)
                for (int k = 0; k < kNumParams; ++k)
                    if (info.params[(size_t) k].name == kv.first)
                        p[(size_t) k] = info.params[(size_t) k].isDiscrete() ? info.params[(size_t) k].fromReal (kv.second)
                                                                             : info.params[(size_t) k].fromReal (kv.second);
            fx->setParameters (p.data());

            const int n = (int) sr * 2;
            std::vector<float> L ((size_t) n), R ((size_t) n), in ((size_t) n);
            for (int i = 0; i < n; ++i) in[(size_t) i] = L[(size_t) i] = R[(size_t) i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 40.0 * i / sr);
            ProcessContext ctx; ctx.sampleRate = sr;
            for (int pos = 0; pos < n; pos += 256)
                fx->process (L.data() + pos, R.data() + pos, std::min (256, n - pos), ctx);

            const int start = n / 2, len = n / 2;
            const double inMag = toneMagnitude (in.data() + start, len, 40.0, sr);
            const double outMag = toneMagnitude (L.data() + start, len, 40.0, sr);
            const double h2 = toneMagnitude (L.data() + start, len, 80.0, sr);
            const double h3 = toneMagnitude (L.data() + start, len, 120.0, sr);
            const double gainDb = 20.0 * std::log10 (outMag / inMag);
            const double distDb = 20.0 * std::log10 (std::max (h2, h3) / outMag + 1.0e-12);
            logMessage (info.name + ": 40 Hz gain " + juce::String (gainDb, 3) + " dB, harmonics " + juce::String (distDb, 1) + " dB");
            expect (std::abs (gainDb) < 0.3, "low band level changed: " + juce::String (gainDb) + " dB");
            expect (distDb < -40.0, "low band distorted: " + juce::String (distDb) + " dB");
        }
    }
};
static LowKeepTest lowKeepTest;
