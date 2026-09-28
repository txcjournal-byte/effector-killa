#include "TestHelpers.h"

using namespace ektest;

// CPU benchmark: every factory preset through the full engine, 44.1 kHz / 256 / 2x oversampling.
// Run with:  EffectorKillaTests benchmark
class CpuBenchmark : public juce::UnitTest
{
public:
    CpuBenchmark() : juce::UnitTest ("CPU benchmark", "benchmark") {}

    void runTest() override
    {
        beginTest ("factory presets, % of one core");
        const double sr = 44100.0;
        const int block = 256, seconds = 5;
        const int blocks = (int) (sr * seconds / block);
        juce::AudioBuffer<float> source (2, blocks * block);
        TestSignals::generate (TestSignals::Kind::Bus, sr, source.getWritePointer (0), source.getWritePointer (1), source.getNumSamples(), 5);

        double worst = 0.0, sum = 0.0;
        juce::String worstName;
        int count = 0;
        for (int c = 0; c < 12; ++c)
            for (auto& p : factoryBank().getChannelPresets (c))
            {
                Engine e;
                e.setRackConfig (p.rackConfig());
                e.prepare (sr, block, 1);
                ProgramExtras ex; ex.source = p.source; ex.maps = p.maps;
                e.setExtras (ex);
                EngineInput in;
                for (int s = 0; s < kNumSlots; ++s)
                {
                    in.base.slots[(size_t) s].p = p.slots[(size_t) s].p;
                    in.base.slots[(size_t) s].mix = p.slots[(size_t) s].mix;
                }
                in.macros = p.macros;
                in.global.autoTracking = true;
                ProcessContext ctx; ctx.sampleRate = sr; ctx.playing = true; ctx.hostHasPosition = true;
                juce::AudioBuffer<float> buf (2, block);
                const auto t0 = juce::Time::getHighResolutionTicks();
                for (int b = 0; b < blocks; ++b)
                {
                    for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, source, ch, b * block, block);
                    e.process (buf, in, ctx);
                    ctx.ppq += block * ctx.beatsPerSample();
                }
                const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                const double pct = 100.0 * secs / seconds;
                sum += pct; ++count;
                if (pct > worst) { worst = pct; worstName = p.name; }
            }
        logMessage ("average " + juce::String (sum / count, 2) + " %, worst " + juce::String (worst, 2) + " % (" + worstName + ")");
        expect (sum / count < 8.0, "typical preset should stay below 8 % of a core");
    }
};
static CpuBenchmark cpuBenchmark;
