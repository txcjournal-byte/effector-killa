#include "TestHelpers.h"
#include "PluginProcessor.h"

using namespace ektest;

class PresetTest : public juce::UnitTest
{
public:
    PresetTest() : juce::UnitTest ("Presets & state", "presets") {}

    void runTest() override
    {
        auto& bank = factoryBank();

        beginTest ("96 factory presets load");
        expectEquals (bank.getNumChannels(), 12);
        expectEquals (bank.getNumFactoryPresets(), 96);
        for (int c = 0; c < bank.getNumChannels(); ++c)
        {
            const auto& list = bank.getChannelPresets (c);
            expectEquals ((int) list.size(), 8);
            for (auto& p : list)
            {
                int fxCount = 0;
                for (auto& s : p.slots)
                {
                    fxCount += s.isEmpty() ? 0 : 1;
                    for (auto v : s.p) expect (v >= 0.0f && v <= 1.0f);
                }
                expect (fxCount >= 1, p.name + " has no effects");
                expect (p.factory && p.channel == c);
            }
        }

        beginTest ("preset values are what section 11 says");
        {
            auto* dark = bank.getFactory (0, 1); // Dark Keys: Through The Wall LP 2.5k, VILLAIN ARC map 800..6k
            expect (dark != nullptr && dark->name == "Dark Keys");
            const auto& ttw = effectInfo (EffectType::ThroughTheWall);
            expectWithinAbsoluteError (ttw.params[1].toReal (dark->slots[1].p[1]), 2500.0f, 5.0f);
            expect (dark->maps.size() == 1 && dark->maps[0].macro == VillainArc);
            expectWithinAbsoluteError (ttw.params[1].toReal (dark->maps[0].atOne), 800.0f, 2.0f);
            auto* phone = bank.getFactory (1, 2); // Phone Ghost: BP 500-3k
            const float centre = ttw.params[1].toReal (phone->slots[0].p[1]);
            const float width = ttw.params[7].toReal (phone->slots[0].p[7]);
            expectWithinAbsoluteError (centre, std::sqrt (500.0f * 3000.0f), 3.0f);
            expectWithinAbsoluteError (width, std::log2 (6.0f), 0.01f);
            auto* bigBack = bank.getFactory (8, 6);
            expect (bigBack->source == Source::Bass808);
        }

        beginTest ("program JSON round trip");
        for (int c = 0; c < 12; ++c)
            for (auto& p : bank.getChannelPresets (c))
            {
                auto q = ProgramState::fromJson (p.toJson());
                expect (q.sameSound (p), p.name);
                expectEquals ((int) q.maps.size(), (int) p.maps.size());
            }

        beginTest ("processor state save -> load = same state");
        {
            EffectorKillaAudioProcessor a;
            a.prepareToPlay (48000.0, 512);
            a.loadFactory (3, 4);
            a.setSlotParam (2, 3, 0.123f);
            a.setWriteProtect (1, true);
            a.setParamNorm (ParamIDs::macro (Drip), 0.8f);
            ModState ms = a.getMeta().mod;
            ms.mode = AvMode::Screensaver;
            ms.path = { { 0.1f, 0.2f }, { 0.8f, 0.3f }, { 0.5f, 0.9f } };
            a.setModState (ms, false);
            a.setOversampling (2);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            EffectorKillaAudioProcessor b;
            b.prepareToPlay (48000.0, 512);
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            const auto pa = a.captureProgram(), pb = b.captureProgram();
            expect (pa.sameSound (pb));
            expect (pb.slots[1].writeProtect);
            expect (pb.mod.mode == AvMode::Screensaver && pb.mod.path.size() == pa.mod.path.size());
            expectEquals (b.getOversampling(), 2);
            expectEquals (b.getLatencySamples(), a.getLatencySamples());
        }

        beginTest ("latency never changes with presets / KILL");
        {
            EffectorKillaAudioProcessor p;
            p.prepareToPlay (44100.0, 256);
            const int lat = p.getLatencySamples();
            for (int c = 0; c < 12; ++c)
            {
                p.loadFactory (c, c % 8);
                expectEquals (p.getLatencySamples(), lat);
            }
            for (int i = 0; i < 20; ++i) { p.kill(); expectEquals (p.getLatencySamples(), lat); }
        }

        beginTest ("factory presets are loudness matched (except BUS)");
        {
            EffectorKillaAudioProcessor p;
            p.prepareToPlay (44100.0, 256);
            int worst = 0; float worstDiff = 0.0f;
            for (int c = 0; c < 12; ++c)
                for (int i = 0; i < 8; ++i)
                {
                    p.loadFactory (c, i, false);
                    const auto prog = p.captureProgram();
                    if (prog.source == Source::Bus) { expectEquals (prog.trimDb, 0.0f); continue; }
                    float inDb = 0, outDb = 0;
                    measureProgramLoudness (prog, 44100.0, 1.5f, inDb, outDb, true);
                    const float d = std::abs (inDb - outDb);
                    // presets needing more than the trim range may stay outside +-1 dB
                    if (std::abs (prog.trimDb) < 5.9f && d > worstDiff) { worstDiff = d; worst = c * 8 + i; }
                }
            logMessage ("worst factory loudness difference " + juce::String (worstDiff, 2) + " dB (preset " + juce::String (worst) + ")");
            expect (worstDiff <= 1.0f);
        }

        beginTest ("undo / redo (30+ steps) and A/B");
        {
            EffectorKillaAudioProcessor p;
            p.prepareToPlay (44100.0, 256);
            std::vector<juce::String> names;
            for (int i = 0; i < 35; ++i)
            {
                names.push_back (p.getMeta().name);
                p.loadFactory (i % 12, i % 8);
            }
            for (int i = 34; i >= 0; --i)
            {
                p.undo();
                expectEquals (p.getMeta().name, names[(size_t) i]);
            }
            p.redo();
            expectEquals (p.getMeta().name, names[1]);

            p.loadFactory (0, 0);
            const auto a = p.captureProgram();
            p.setSide (1);
            p.loadFactory (5, 5);
            const auto b = p.captureProgram();
            p.setSide (0);
            expect (p.captureProgram().sameSound (a));
            p.setSide (1);
            expect (p.captureProgram().sameSound (b));
        }

        beginTest ("user presets, slot presets, favourites");
        {
            auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ek_preset_test");
            dir.deleteRecursively();
            PresetBank b;
            b.setRootOverride (dir);
            auto p = *bank.getFactory (2, 0);
            expect (b.saveUserPreset (p, "My 808"));
            auto files = b.getUserPresetFiles();
            expectEquals (files.size(), 1);
            auto loaded = b.loadUserPreset (files[0]);
            expect (loaded.has_value() && loaded->sameSound (p) && loaded->name == "My 808");
            expect (b.saveSlotPreset (p.slots[0], "Warm Tape"));
            auto sp = b.loadSlotPreset (b.getSlotPresetFiles()[0]);
            expect (sp.has_value() && sp->type == p.slots[0].type);
            b.setFavourite ("factory:3/Svbterra", true);
            PresetBank c;
            c.setRootOverride (dir);
            expect (c.isFavourite ("factory:3/Svbterra"));
            dir.deleteRecursively();
        }
    }
};
static PresetTest presetTest;

// Render test: every preset on the four reference signals – no NaN, bounded output.
class RenderTest : public juce::UnitTest
{
public:
    RenderTest() : juce::UnitTest ("Render", "render") {}

    void runTest() override
    {
        beginTest ("all presets x sine 55 / pink / drums / vocal");
        const double sr = 44100.0;
        const int n = (int) (sr * 1.0);
        const TestSignals::Kind kinds[] = { TestSignals::Kind::Sine55, TestSignals::Kind::PinkNoise,
                                            TestSignals::Kind::DrumLoop, TestSignals::Kind::Vocal };
        std::vector<float> L ((size_t) n), R ((size_t) n);
        int renders = 0;
        for (int c = 0; c < 12; ++c)
            for (auto& p : factoryBank().getChannelPresets (c))
                for (auto k : kinds)
                {
                    TestSignals::generate (k, sr, L.data(), R.data(), n, 3);
                    renderProgram (p, sr, 1, L.data(), R.data(), n, 256);
                    const bool ok = allFinite (L.data(), n, 16.0f) && allFinite (R.data(), n, 16.0f);
                    expect (ok, p.name + " produced NaN/Inf or exploded");
                    ++renders;
                }
        logMessage (juce::String (renders) + " renders OK");

        beginTest ("engine: arbitrary host buffer sizes 1..4096");
        {
            Engine e;
            e.setRackConfig (factoryBank().getFactory (4, 0)->rackConfig());
            e.prepare (48000.0, 4096, 1);
            EngineInput in;
            const auto& p = *factoryBank().getFactory (4, 0);
            for (int s = 0; s < kNumSlots; ++s) { in.base.slots[(size_t) s].p = p.slots[(size_t) s].p; in.base.slots[(size_t) s].mix = 1.0f; }
            ProcessContext ctx; ctx.sampleRate = 48000.0;
            bool ok = true;
            for (int size : { 1, 7, 64, 511, 512, 513, 1024, 4096 })
            {
                juce::AudioBuffer<float> buf (2, size);
                for (int rep = 0; rep < 8; ++rep)
                {
                    TestSignals::generate (TestSignals::Kind::PinkNoise, 48000.0, buf.getWritePointer (0), buf.getWritePointer (1), size, (uint64_t) rep);
                    e.process (buf, in, ctx);
                    ok &= allFinite (buf.getReadPointer (0), size, 16.0f);
                }
            }
            expect (ok);
        }
    }
};
static RenderTest renderTest;
