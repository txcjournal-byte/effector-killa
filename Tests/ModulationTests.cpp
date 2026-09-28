#include "TestHelpers.h"

using namespace ektest;

class ModulationTest : public juce::UnitTest
{
public:
    ModulationTest() : juce::UnitTest ("Modulation & macros", "modulation") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("AV2 SCRIBBLE follows the transport and stops with it");
        {
            ModState st;
            st.mode = AvMode::Scribble;
            st.scribbleBars = 1;
            st.path = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
            st.resamplePath();
            expectEquals ((int) st.path.size(), ModState::kPathPoints);
            ModulationEngine me;
            me.prepare (sr);
            ProcessContext ctx; ctx.sampleRate = sr; ctx.bpm = 120.0; ctx.playing = true; ctx.hostHasPosition = true;

            // position is a pure function of the song position
            ctx.ppq = 1.0; // a quarter of the loop -> second corner (1,0)
            auto o1 = me.process (st, st.pathMean(), ctx, 64);
            expectWithinAbsoluteError (o1.dotX, 1.0f, 0.02f);
            expectWithinAbsoluteError (o1.dotY, 0.0f, 0.02f);
            ctx.ppq = 5.0; // next bar, same place
            auto o2 = me.process (st, st.pathMean(), ctx, 64);
            expectWithinAbsoluteError (o2.dotX, o1.dotX, 1.0e-4f);
            expectWithinAbsoluteError (o2.offX, 0.5f * 1.5f, 0.03f);

            // host stopped -> hold
            ctx.playing = false;
            ctx.ppq = 7.3;
            auto o3 = me.process (st, st.pathMean(), ctx, 4800);
            auto o4 = me.process (st, st.pathMean(), ctx, 4800);
            expectWithinAbsoluteError (o3.dotX, o2.dotX, 1.0e-4f);
            expectWithinAbsoluteError (o4.dotY, o2.dotY, 1.0e-4f);
        }

        beginTest ("AV3 SCREENSAVER corner hit = glitch burst every 4 crossings");
        {
            ModState st;
            st.mode = AvMode::Screensaver;
            st.screensaverSpeed = 2; // 1 bar per crossing
            ModulationEngine me;
            me.prepare (sr);
            ProcessContext ctx; ctx.sampleRate = sr; ctx.bpm = 140.0; ctx.playing = true; ctx.hostHasPosition = true;
            const int block = 256;
            std::vector<double> hits;
            int lastHits = 0;
            for (int i = 0; i < 20000; ++i)
            {
                ctx.ppq = (double) (i + 1) * block * ctx.beatsPerSample();
                auto o = me.process (st, {}, ctx, block);
                if (me.getCornerHits() != lastHits) { hits.push_back (ctx.ppq); lastHits = me.getCornerHits(); expect (o.glitch > 0.99f); }
                expect (o.dotX >= 0.0f && o.dotX <= 1.0f && o.dotY >= 0.0f && o.dotY <= 1.0f);
            }
            expect (hits.size() > 3, "no corner hits");
            for (size_t k = 1; k < hits.size(); ++k)
                expectWithinAbsoluteError (hits[k] - hits[k - 1], 16.0, 0.1); // 4 crossings x 1 bar (4 beats)
            logMessage (juce::String ((int) hits.size()) + " corner hits");
        }

        beginTest ("macros: neutral at 0.5, preset maps, source limits");
        {
            ProgramState p;
            p.slots[0].setType (EffectType::AuraRoom);
            p.slots[1].setType (EffectType::ThroughTheWall);
            p.slots[0].p[7] = 0.1f; // reverb mix 0.1
            RackParams base, out;
            for (int s = 0; s < kNumSlots; ++s) base.slots[(size_t) s].p = p.slots[(size_t) s].p;
            std::array<float, kNumMacros> macros { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

            MacroEngine::compute (p.rackConfig(), base, macros, {}, Source::Melody, out);
            for (int s = 0; s < 2; ++s)
                for (int k = 0; k < kNumParams; ++k)
                    expectWithinAbsoluteError (out.slots[(size_t) s].p[(size_t) k], base.slots[(size_t) s].p[(size_t) k], 1.0e-6f);

            macros[Aura] = 1.0f;
            MacroEngine::compute (p.rackConfig(), base, macros, {}, Source::Melody, out);
            expect (out.slots[0].p[7] > 0.3f, "AURA should raise the reverb mix");
            MacroEngine::compute (p.rackConfig(), base, macros, {}, Source::Bass808, out);
            expect (out.slots[0].p[7] <= 0.15f + 1.0e-6f, "808: AURA must not push reverb mix above 0.15");

            macros[VillainArc] = 1.0f;
            MacroEngine::compute (p.rackConfig(), base, macros, {}, Source::Melody, out);
            expect (out.slots[1].p[1] < base.slots[1].p[1], "VILLAIN ARC should lower the cutoff");

            // preset map: 0 -> atZero, 0.5 -> base, 1 -> atOne
            std::vector<MacroMapping> maps { { VillainArc, 1, 1, 0.9f, 0.2f } };
            for (float m : { 0.0f, 0.5f, 1.0f })
            {
                macros[VillainArc] = m;
                MacroEngine::compute (p.rackConfig(), base, macros, maps, Source::Melody, out);
                const float expected = m == 0.0f ? 0.9f : (m == 1.0f ? 0.2f : base.slots[1].p[1]);
                expectWithinAbsoluteError (out.slots[1].p[1], expected, 1.0e-5f);
            }
        }
    }
};
static ModulationTest modulationTest;
