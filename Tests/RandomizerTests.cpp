#include "TestHelpers.h"

using namespace ektest;

class RandomizerTest : public juce::UnitTest
{
public:
    RandomizerTest() : juce::UnitTest ("Randomizer", "randomizer") {}

    void runTest() override
    {
        const KillLevel levels[] = { KillLevel::PG, KillLevel::R, KillLevel::Unrated };
        const char* levelNames[] = { "PG", "R", "UNRATED" };

        beginTest ("rules hold for 10 000 seeds x all sources x all levels");
        {
            int failures = 0;
            juce::String firstFailure;
            for (int src = 0; src < (int) Source::Count; ++src)
            {
                for (int li = 0; li < 3; ++li)
                {
                    for (uint64_t seed = 1; seed <= 10000; ++seed)
                    {
                        ProgramState start;
                        start.source = (Source) src;
                        if (levels[li] == KillLevel::PG) // PG varies an existing (valid) chain
                            start = Randomizer::kill (start, KillLevel::R, (Source) src, seed * 7919u);
                        const auto out = Randomizer::kill (start, levels[li], (Source) src, seed);
                        const auto err = Randomizer::checkRules (out, (Source) src, levels[li]);
                        if (err.isNotEmpty())
                        {
                            if (failures++ == 0)
                                firstFailure = sourceName ((Source) src) + " " + levelNames[li] + " seed " + juce::String ((juce::int64) seed) + ": " + err;
                        }
                        if (levels[li] == KillLevel::PG)
                        {
                            int changed = 0;
                            for (int s = 0; s < kNumSlots; ++s)
                                changed += start.slots[(size_t) s].type != out.slots[(size_t) s].type ? 1 : 0;
                            if (changed > 1 && failures++ == 0) firstFailure = "PG changed more than one slot";
                        }
                    }
                }
            }
            expectEquals (failures, 0, firstFailure);
        }

        beginTest ("deterministic by seed");
        for (int src = 0; src < (int) Source::Count; ++src)
            for (int li = 0; li < 3; ++li)
                for (uint64_t seed : { 1ull, 99ull, 123456789ull })
                {
                    ProgramState start;
                    start.source = (Source) src;
                    auto a = Randomizer::kill (start, levels[li], (Source) src, seed);
                    auto b = Randomizer::kill (start, levels[li], (Source) src, seed);
                    expect (a.sameSound (b));
                    auto c = Randomizer::bootleg (a, seed + 1);
                    auto d = Randomizer::bootleg (a, seed + 1);
                    expect (c.sameSound (d));
                }

        beginTest ("BOOTLEG never changes effect types");
        for (uint64_t seed = 1; seed < 500; ++seed)
        {
            const auto& presets = factoryBank().getChannelPresets ((int) (seed % 12));
            const auto& p = presets[(size_t) (seed % presets.size())];
            auto b = Randomizer::bootleg (p, seed);
            bool same = true, changed = false;
            for (int s = 0; s < kNumSlots; ++s)
            {
                same &= b.slots[(size_t) s].type == p.slots[(size_t) s].type;
                for (int k = 0; k < kNumParams; ++k)
                {
                    const float d = std::abs (b.slots[(size_t) s].p[(size_t) k] - p.slots[(size_t) s].p[(size_t) k]);
                    changed |= d > 1.0e-6f;
                    // +-20 % of the value (min step 0.15 * 20 %) -> never more than 0.2 normalised
                    if (d > 0.2001f) same = false;
                }
            }
            expect (same, "bootleg changed an effect or moved a parameter too far");
            expect (changed, "bootleg changed nothing");
        }

        beginTest ("WRITE PROTECT slots survive KILL");
        for (uint64_t seed = 1; seed < 2000; ++seed)
        {
            ProgramState start;
            start.slots[1].setType (EffectType::CassettePlug);
            start.slots[1].p[1] = 0.77f;
            start.slots[1].writeProtect = true;
            start.slots[5].writeProtect = true; // locked empty slot stays empty
            const auto level = levels[seed % 3];
            auto out = Randomizer::kill (start, level, (Source) (seed % 6), seed);
            expect (out.slots[1].type == EffectType::CassettePlug && std::abs (out.slots[1].p[1] - 0.77f) < 1.0e-6f && out.slots[1].writeProtect);
            expect (out.slots[5].isEmpty() && out.slots[5].writeProtect);
        }

        beginTest ("KILL result is loudness matched (+-1 dB)");
        {
            const double sr = 44100.0;
            int checked = 0;
            for (int src = 0; src < (int) Source::Count; ++src)
                for (uint64_t seed = 1; seed <= 6; ++seed)
                    for (int li = 1; li < 3; ++li)
                    {
                        ProgramState start;
                        start.source = (Source) src;
                        auto p = Randomizer::kill (start, levels[li], (Source) src, seed * 31 + (uint64_t) src);
                        p.trimDb = estimateProgramTrimDb (p, sr);
                        float inDb = 0, outDb = 0;
                        bool nan = false;
                        measureProgramLoudness (p, sr, 1.5f, inDb, outDb, true, &nan);
                        expect (! nan, "NaN in KILL render");
                        expect (std::abs (inDb - outDb) <= 1.0f, sourceName ((Source) src) + " seed " + juce::String ((int) seed)
                                                                   + ": in " + juce::String (inDb, 2) + " out " + juce::String (outDb, 2));
                        ++checked;
                    }
            logMessage ("loudness-checked " + juce::String (checked) + " KILL chains");
        }
    }
};
static RandomizerTest randomizerTest;
