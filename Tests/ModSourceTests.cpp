// MOD SOURCES (LFO 1 / LFO 2 / ENV / SEQ + assignments) and MACRO LEARN.
#include "TestHelpers.h"
#include "PluginProcessor.h"
#include <cstdlib>
#include <new>

// ---------------------------------------------------------------------------------------------
// Allocation counter: counts operator new calls made on a thread while counting is switched on.
// Used to prove that Engine::process never allocates with modulation running.
// ---------------------------------------------------------------------------------------------
namespace ektest
{
static thread_local bool countAllocs = false;
static std::atomic<int> allocCount { 0 };
struct AllocScope
{
    AllocScope() { allocCount = 0; countAllocs = true; }
    ~AllocScope() { countAllocs = false; }
};
} // namespace ektest

void* operator new (std::size_t n)
{
    if (ektest::countAllocs) ++ektest::allocCount;
    if (void* p = std::malloc (n > 0 ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n)
{
    if (ektest::countAllocs) ++ektest::allocCount;
    if (void* p = std::malloc (n > 0 ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace ektest;

namespace
{
RackParams baseFrom (const ProgramState& p)
{
    RackParams b;
    for (int s = 0; s < kNumSlots; ++s)
    {
        b.slots[(size_t) s].p = p.slots[(size_t) s].p;
        b.slots[(size_t) s].mix = p.slots[(size_t) s].mix;
        b.slots[(size_t) s].pause = p.slots[(size_t) s].pause;
    }
    return b;
}

ProgramExtras extrasFor (const ProgramState& p)
{
    ProgramExtras e;
    e.source = p.source;
    e.maps = p.maps;
    e.mod.mode = AvMode::Remote;
    e.modSources = p.modSources;
    return e;
}

// A rack with modulation everywhere: every source, every kind of target, full depth.
ModSourceState heavyMod()
{
    ModSourceState m;
    m.lfo[0] = { LfoShape::Square, false, 20.0f, 5, 0.25f };
    m.lfo[1] = { LfoShape::SampleHold, true, 1.0f, 11, 0.0f };
    m.env = { 0.1f, 5.0f, 1.0f };
    m.seq.smooth = 0.0f;
    m.seq.division = 11;
    int src = 0;
    for (int s = 0; s < 4; ++s)
        for (int p : { 0, 1, 2, 8 })
            m.add ({ (src++) % kNumModSources, s, p, (src % 2 == 0) ? 1.0f : -1.0f });
    jassert (m.numAssign == kMaxModAssignments);
    return m;
}

struct EngineRig
{
    Engine e;
    EngineInput in;
    ProcessContext ctx;
    ProgramState prog;

    EngineRig (const ProgramState& p, double sr, const ModSourceState& mod)
        : prog (p)
    {
        e.setRackConfig (p.rackConfig());
        e.prepare (sr, 4096, 1);
        auto ex = extrasFor (p);
        ex.modSources = mod;
        e.setExtras (ex);
        in.base = baseFrom (p);
        in.macros = p.macros;
        ctx.sampleRate = sr;
        ctx.bpm = 140.0;
    }

    void run (juce::AudioBuffer<float>& buf)
    {
        e.process (buf, in, ctx);
        ctx.ppq += buf.getNumSamples() * ctx.beatsPerSample();
    }
};
} // namespace

class ModSourceTest : public juce::UnitTest
{
public:
    ModSourceTest() : juce::UnitTest ("MOD sources & macro learn", "modsources") {}

    void runTest() override
    {
        const auto& night = *factoryBank().getFactory (9, 2); // CH10 Night Drive (4 units)

        beginTest ("default: no assignment = bit-identical to the no-mod path");
        {
            // B has non-default source settings and depth-0 assignments: nothing may change.
            ModSourceState b;
            b.lfo[0].shape = LfoShape::Saw;
            b.lfo[0].rateHz = 7.0f;
            b.env.sensitivity = 1.0f;
            b.add ({ 0, 0, 1, 0.0f });
            b.add ({ 3, -1, Aura, 0.0f });
            for (int block : { 64, 333, 512, 2048 })
            {
                EngineRig a (night, 48000.0, ModSourceState());
                EngineRig c (night, 48000.0, b);
                juce::AudioBuffer<float> ba (2, block), bc (2, block);
                bool identical = true;
                for (int rep = 0; rep < 40; ++rep)
                {
                    TestSignals::generate (TestSignals::Kind::Melody, 48000.0, ba.getWritePointer (0), ba.getWritePointer (1), block, (uint64_t) rep);
                    bc.makeCopyOf (ba);
                    a.run (ba);
                    c.run (bc);
                    for (int ch = 0; ch < 2; ++ch)
                        identical &= std::memcmp (ba.getReadPointer (ch), bc.getReadPointer (ch), sizeof (float) * (size_t) block) == 0;
                }
                expect (identical, "block " + juce::String (block));
            }

            // a default state is not written to presets (older builds read the same JSON)
            ProgramState p = night;
            expect (! p.toVar().hasProperty ("modSources"));
            expect (ProgramState::fromJson (p.toJson()).modSources == ModSourceState());
            p.modSources.add ({ 1, 2, 3, 0.5f });
            expect (p.toVar().hasProperty ("modSources"));
        }

        beginTest ("old presets / projects load with no modulation");
        {
            const auto oldJson = juce::String (R"({"version":1,"name":"Old","slots":[{"fx":"cassette_plug"}],"maps":[{"macro":0,"slot":0,"param":1,"zero":0.1,"one":0.9}]})");
            const auto p = ProgramState::fromJson (oldJson);
            expect (p.modSources == ModSourceState());
            expectEquals ((int) p.maps.size(), 1);
            expect (! p.maps[0].learned);
            // garbage in the modSources object never produces an invalid assignment
            const auto bad = juce::String (R"({"modSources":{"lfo":[{"hz":1e9,"shape":"???","phase":-4}],"assign":[{"src":"NOPE","slot":0,"param":1,"depth":1},{"src":0,"slot":12,"param":1,"depth":1},{"src":0,"slot":0,"param":99,"depth":1},{"src":"LFO 1","macro":9,"depth":1},{"src":"SEQ","slot":1,"param":8,"depth":"nan"}]}})");
            const auto q = ProgramState::fromJson (bad);
            expectEquals (q.modSources.numAssign, 1);
            expect (q.modSources.lfo[0].rateHz <= LfoSettings::maxHz && q.modSources.lfo[0].phase == 0.0f);
        }

        beginTest ("save / load round trip");
        {
            ModSourceState m;
            m.lfo[0] = { LfoShape::SampleHold, true, 3.3f, 7, 0.4f };
            m.lfo[1] = { LfoShape::Saw, false, 0.07f, 0, 0.9f };
            m.env = { 2.5f, 900.0f, 0.8f };
            for (int i = 0; i < kSeqSteps; ++i) m.seq.steps[(size_t) i] = (float) i / 15.0f;
            m.seq.division = 3;
            m.seq.smooth = 0.6f;
            m.add ({ 0, 3, 8, -0.75f });
            m.add ({ 2, -1, Knock, 0.5f });
            m.add ({ 3, 7, 0, 1.0f });
            const auto back = ModSourceState::fromVar (juce::JSON::parse (juce::JSON::toString (m.toVar())));
            expect (back == m);

            ProgramState p = night;
            p.modSources = m;
            p.maps.push_back ({ Drip, 1, 2, 0.2f, 0.8f, true });
            const auto q = ProgramState::fromJson (p.toJson());
            expect (q.modSources == m);
            expect (q.maps.back().learned && q.maps.back().macro == Drip);

            // plugin state (DAW project)
            EffectorKillaAudioProcessor a;
            a.prepareToPlay (48000.0, 512);
            a.loadFactory (9, 2, false);
            a.setModSources (m);
            expect (a.assignMacro (Aura, 0, 1, 0.1f, 0.9f));
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            EffectorKillaAudioProcessor b;
            b.prepareToPlay (48000.0, 512);
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (b.getModSources() == m);
            expectEquals ((int) b.getMacroAssignmentsFor (0, 1).size(), 1);
            expect (b.getMacroAssignmentsFor (0, 1).front().learned);
        }

        beginTest ("sources: shapes, sync, sequencer, envelope");
        {
            using E = ModSourceEngine;
            expectWithinAbsoluteError (E::lfoShape (LfoShape::Sine, 0.25, 0, 0), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (E::lfoShape (LfoShape::Triangle, 0.25, 0, 0), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (E::lfoShape (LfoShape::Triangle, 0.75, 0, 0), -1.0f, 1.0e-6f);
            expectWithinAbsoluteError (E::lfoShape (LfoShape::Saw, 0.0, 0, 0), -1.0f, 1.0e-6f);
            expectEquals (E::lfoShape (LfoShape::Square, 0.1, 0, 0), 1.0f);
            expectEquals (E::lfoShape (LfoShape::Square, 0.6, 0, 0), -1.0f);
            expectEquals (E::lfoShape (LfoShape::SampleHold, 0.1, 5, 0), E::lfoShape (LfoShape::SampleHold, 0.9, 5, 0));
            expect (E::lfoShape (LfoShape::SampleHold, 0.1, 5, 0) != E::lfoShape (LfoShape::SampleHold, 0.1, 6, 0));

            SeqSettings sq;
            for (int i = 0; i < kSeqSteps; ++i) sq.steps[(size_t) i] = (float) i / 16.0f;
            expectEquals (E::seqValue (sq, 3.5), 3.0f / 16.0f);
            expectEquals (E::seqValue (sq, 19.2), 3.0f / 16.0f);  // wraps after 16 steps
            sq.smooth = 0.5f;
            expect (E::seqValue (sq, 3.1) < 3.0f / 16.0f && E::seqValue (sq, 3.1) > 2.0f / 16.0f); // gliding
            expectEquals (E::seqValue (sq, 3.6), 3.0f / 16.0f);

            // synced LFO + SEQ are a pure function of the song position (transport running)
            ModSourceState st;
            st.lfo[0].sync = true;
            st.lfo[0].division = 5; // 1/4: one cycle per beat
            st.seq.division = 10;   // 1/16
            std::vector<float> zeros (4800, 0.0f);
            ModSourceEngine a, b;
            a.prepare (48000.0); b.prepare (48000.0);
            ProcessContext ctx; ctx.sampleRate = 48000.0; ctx.bpm = 120.0; ctx.playing = true; ctx.hostHasPosition = true;
            ctx.ppq = 11.0; a.advance (st, ctx, zeros.data(), zeros.data(), 1000);
            ctx.ppq = 0.0;  b.advance (st, ctx, zeros.data(), zeros.data(), 77);
            const double endPpq = 11.0 + 1000 * ctx.beatsPerSample();
            ctx.ppq = endPpq - 64 * ctx.beatsPerSample(); b.advance (st, ctx, zeros.data(), zeros.data(), 64);
            expectWithinAbsoluteError (a.getValue (0), b.getValue (0), 1.0e-5f);
            expectEquals (a.getSeqStep(), b.getSeqStep());
            expectEquals (a.getSeqStep(), (int) std::floor (endPpq * 4.0) % 16);

            // envelope follower: rises on signal, falls in silence, stays 0..1
            ModSourceEngine env;
            env.prepare (48000.0);
            std::vector<float> loud (4800, 0.5f);
            env.advance (st, ctx, loud.data(), loud.data(), 4800);
            const float up = env.getValue ((int) ModSourceId::Env);
            expect (up > 0.9f && up <= 1.0f, "env up " + juce::String (up));
            for (int i = 0; i < 20; ++i) env.advance (st, ctx, zeros.data(), zeros.data(), 4800);
            expect (env.getValue ((int) ModSourceId::Env) < 0.01f);
        }

        beginTest ("modulation reaches the effective parameters (slot, MIX, macro)");
        {
            ProgramState p = night;
            ModSourceState m;
            m.seq.steps.fill (1.0f); // SEQ = constant 1
            m.add ({ (int) ModSourceId::Seq, 1, 1, -0.3f });
            m.add ({ (int) ModSourceId::Seq, 0, 8, -0.5f });
            m.add ({ (int) ModSourceId::Seq, -1, Aura, 0.4f });
            EngineRig r (p, 48000.0, m);
            juce::AudioBuffer<float> buf (2, 512);
            for (int i = 0; i < 40; ++i) { buf.clear(); r.run (buf); }
            RackParams ref;
            auto mac = p.macros;
            mac[Aura] = clamp01 (mac[Aura] + 0.4f);
            MacroEngine::compute (p.rackConfig(), r.in.base, mac, p.maps, p.source, ref);
            expectWithinAbsoluteError (r.e.effectiveParams[1][1].load(), clamp01 (ref.slots[1].p[1] - 0.3f), 1.0e-3f);
            expectWithinAbsoluteError (r.e.effectiveMix[0].load(), clamp01 (ref.slots[0].mix - 0.5f), 1.0e-3f);
            expectWithinAbsoluteError (r.e.meters.macroValue[Aura].load(), clamp01 (p.macros[Aura] + 0.4f), 1.0e-3f);
            expect (r.e.modMeters.active.load());

            // host automating the macro: the macro target is left alone
            r.in.macroModSuspended[Aura] = true;
            for (int i = 0; i < 4; ++i) { buf.clear(); r.run (buf); }
            expectWithinAbsoluteError (r.e.meters.macroValue[Aura].load(), p.macros[Aura], 1.0e-6f);
        }

        beginTest ("no NaN / Inf, extremes, any buffer size and sample rate");
        {
            ProgramState p = *factoryBank().getFactory (4, 0);
            for (double sr : { 44100.0, 96000.0, 192000.0 })
            {
                EngineRig r (p, sr, heavyMod());
                juce::Random rnd (42);
                bool ok = true;
                for (int size : { 1, 7, 31, 32, 33, 64, 511, 512, 513, 1024, 4096 })
                {
                    juce::AudioBuffer<float> buf (2, size);
                    for (int rep = 0; rep < 6; ++rep)
                    {
                        TestSignals::generate (TestSignals::Kind::DrumLoop, sr, buf.getWritePointer (0), buf.getWritePointer (1), size, (uint64_t) rep);
                        if (rep == 3) buf.applyGain (60.0f); // absurdly hot input
                        r.ctx.bpm = rep == 4 ? 999.0 : (rep == 5 ? 20.0 : 140.0);
                        r.ctx.playing = rnd.nextBool();
                        r.ctx.hostHasPosition = r.ctx.playing;
                        r.run (buf);
                        ok &= allFinite (buf.getReadPointer (0), size, 64.0f) && allFinite (buf.getReadPointer (1), size, 64.0f);
                        for (int s = 0; s < kNumModSources; ++s)
                        {
                            const float v = r.e.modMeters.value[(size_t) s].load();
                            ok &= std::isfinite (v) && v >= -1.0f && v <= 1.0f;
                        }
                        for (int s = 0; s < kNumSlots; ++s)
                            for (int k = 0; k < kNumParams; ++k)
                            {
                                const float v = r.e.effectiveParams[(size_t) s][(size_t) k].load();
                                ok &= std::isfinite (v) && v >= 0.0f && v <= 1.0f;
                            }
                    }
                }
                expect (ok, "sr " + juce::String (sr));
            }

            // extreme settings on their own
            ModSourceState x = heavyMod();
            x.lfo[0].rateHz = 1.0e9f; x.lfo[1].phase = 7.0f; x.env.attackMs = -5.0f; x.env.releaseMs = 0.0f;
            x.seq.division = 999; x.seq.steps[0] = 1.0e30f;
            ModSourceEngine me;
            me.prepare (44100.0);
            std::vector<float> in (512);
            for (auto& v : in) v = std::numeric_limits<float>::infinity();
            ProcessContext ctx;
            bool ok = true;
            for (int i = 0; i < 200; ++i)
            {
                me.advance (x, ctx, in.data(), in.data(), 512);
                me.updateOffsets (x, 512);
                for (int s = 0; s < kNumModSources; ++s) ok &= std::isfinite (me.getValue (s)) && std::abs (me.getValue (s)) <= 1.0f;
                for (int s = 0; s < kNumSlots; ++s)
                    for (int k = 0; k < kNumModSlotTargets; ++k) ok &= std::isfinite (me.slotOffset (s, k));
            }
            expect (ok);
        }

        beginTest ("determinism");
        {
            ProgramState p = night;
            auto runOnce = [&] (int block)
            {
                EngineRig r (p, 48000.0, heavyMod());
                r.ctx.playing = r.ctx.hostHasPosition = true;
                std::vector<float> out;
                juce::AudioBuffer<float> buf (2, block);
                for (int i = 0; i < 48000 / block; ++i)
                {
                    TestSignals::generate (TestSignals::Kind::Vocal, 48000.0, buf.getWritePointer (0), buf.getWritePointer (1), block, (uint64_t) i);
                    r.run (buf);
                    out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + block);
                }
                return out;
            };
            expect (runOnce (256) == runOnce (256), "same input, same output");
        }

        beginTest ("no clicks: bounded parameter step between 32-sample blocks");
        {
            for (double sr : { 44100.0, 48000.0, 96000.0 })
            {
                ProgramState p = night;
                ModSourceState m;
                m.lfo[0] = { LfoShape::Square, false, 20.0f, 0, 0.0f }; // hardest case: full-depth square at 20 Hz
                m.add ({ 0, 1, 1, 1.0f });
                m.add ({ 3, 1, 8, -1.0f });                              // hard SEQ steps on MIX
                m.seq.division = 11;
                EngineRig r (p, sr, m);
                juce::AudioBuffer<float> buf (2, kModBlock);
                float prevP = -1.0f, prevMix = -1.0f, maxStep = 0.0f;
                for (int i = 0; i < (int) sr / kModBlock; ++i)
                {
                    buf.clear();
                    r.run (buf);
                    const float v = r.e.effectiveParams[1][1].load(), mx = r.e.effectiveMix[1].load();
                    if (prevP >= 0.0f) maxStep = std::max ({ maxStep, std::abs (v - prevP), std::abs (mx - prevMix) });
                    prevP = v; prevMix = mx;
                }
                // ~4 ms smoothing: at most ~1/4 of the full range per 32 samples, even for a hard square
                const float limit = 2.0f * (1.0f - std::exp (-(float) kModBlock / (ModSourceEngine::smoothMs * 0.001f * (float) sr))) + 1.0e-3f;
                expect (maxStep <= limit && maxStep > 0.0f, "max step " + juce::String (maxStep) + " limit " + juce::String (limit));
            }

            // adding / removing an assignment fades in / out, then the plain path takes over again
            ModSourceEngine me;
            me.prepare (48000.0);
            ModSourceState m;
            m.seq.steps.fill (1.0f);
            m.add ({ 3, 0, 0, 1.0f });
            std::vector<float> z (kModBlock, 0.0f);
            ProcessContext ctx;
            float prev = 0.0f, maxStep = 0.0f;
            for (int i = 0; i < 200; ++i)
            {
                if (i == 100) m.remove (0);
                me.advance (m, ctx, z.data(), z.data(), kModBlock);
                me.updateOffsets (m, kModBlock);
                maxStep = std::max (maxStep, std::abs (me.slotOffset (0, 0) - prev));
                prev = me.slotOffset (0, 0);
                if (i == 99) expectWithinAbsoluteError (prev, 1.0f, 1.0e-3f);
            }
            expect (maxStep < 0.16f, "fade step " + juce::String (maxStep));
            expectEquals (me.slotOffset (0, 0), 0.0f);
            expect (! me.needsProcessing (m));
        }

        beginTest ("real-time safety: no allocation in Engine::process with modulation running");
        {
            ProgramState p = night;
            EngineRig r (p, 48000.0, heavyMod());
            juce::AudioBuffer<float> buf (2, 1024);
            for (int i = 0; i < 4; ++i) { TestSignals::generate (TestSignals::Kind::PinkNoise, 48000.0, buf.getWritePointer (0), buf.getWritePointer (1), 1024, 1); r.run (buf); }
            int allocs = 0;
            {
                AllocScope scope;
                for (int i = 0; i < 50; ++i) r.run (buf);
                allocs = allocCount.load();
            }
            expectEquals (allocs, 0);
            expect (r.e.modMeters.active.load());
        }

        beginTest ("processor API: assignments, slot moves, KILL, undo");
        {
            EffectorKillaAudioProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            proc.loadFactory (9, 2, false); // 4 units
            expectEquals (proc.addModAssignment (0, 1, 1, 0.5f), 0);
            expectEquals (proc.addModAssignment (0, 1, 1, -0.25f), 0);          // same source + target = new depth
            expectEquals (proc.getModAssignments()[0].depth, -0.25f);
            expectEquals (proc.addMacroModAssignment (2, Aura, 0.3f), 1);
            expectEquals (proc.addModAssignment (9, 1, 1, 0.5f), -1);          // bad source
            expectEquals (proc.addModAssignment (0, 1, 9, 0.5f), -1);          // bad param
            proc.setModDepth (1, 7.0f);
            expectEquals (proc.getModAssignments()[1].depth, 1.0f);           // clamped
            expectEquals ((int) proc.findModAssignments (-1, Aura).size(), 1);

            // follows its unit when units move, dies with the unit
            proc.moveSlot (1, 3);
            expectEquals (proc.getModAssignments()[0].slot, 3);
            proc.removeEffect (3);
            expectEquals ((int) proc.findModAssignments (3, 1).size(), 0);
            expectEquals ((int) proc.getModAssignments().size(), 1);           // the macro one stays

            proc.undo();
            expectEquals ((int) proc.getModAssignments().size(), 2);

            // the table is limited to 16
            proc.clearModAssignments();
            int ok = 0;
            for (int i = 0; i < 20; ++i) ok += proc.addModAssignment (i % 4, i % 4, i % 9, 0.1f) >= 0 ? 1 : 0;
            expectEquals (ok, kMaxModAssignments);

            // KILL keeps nothing on units that changed type
            proc.kill();
            for (auto& a : proc.getModAssignments())
                expect (a.isMacro() || ! proc.getSlotMeta (a.slot).isEmpty());
        }

        beginTest ("MACRO LEARN");
        {
            EffectorKillaAudioProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            proc.loadFactory (9, 2, false); // Cassette Plug, Through The Wall, Ad-Lib Throw, Aura Room
            expect (! proc.assignMacro (Drip, 7, 1, 0.0f, 1.0f));             // empty slot
            expect (proc.assignMacro (Drip, 0, 3, 0.1f, 0.9f));
            expect (proc.assignMacro (Drip, 0, 3, 0.2f, 0.7f));               // replaces
            const auto list = proc.getMacroAssignmentsFor (0, 3);
            expectEquals ((int) list.size(), 1);
            expectEquals (list[0].atZero, 0.2f);
            expectEquals (list[0].atOne, 0.7f);

            // engine: macro at 1 -> max, macro at 0 -> min; the macro's built-in mapping keeps working
            const auto p = proc.captureProgram();
            RackParams base = baseFrom (p), out, outDefault;
            std::array<float, kNumMacros> mac { 0.5f, 0.5f, 0.5f, 1.0f, 0.5f };
            MacroEngine::compute (p.rackConfig(), base, mac, p.maps, p.source, out);
            expectWithinAbsoluteError (out.slots[0].p[3], 0.7f, 1.0e-6f);
            mac[Drip] = 0.0f;
            MacroEngine::compute (p.rackConfig(), base, mac, p.maps, p.source, out);
            expectWithinAbsoluteError (out.slots[0].p[3], 0.2f, 1.0e-6f);
            std::vector<MacroMapping> noLearn;
            for (auto& m : p.maps) if (! m.learned) noLearn.push_back (m);
            MacroEngine::compute (p.rackConfig(), base, mac, noLearn, p.source, outDefault);
            for (int s = 1; s < kNumSlots; ++s)
                for (int k = 0; k < kNumParams; ++k)
                    expectWithinAbsoluteError (out.slots[(size_t) s].p[(size_t) k], outDefault.slots[(size_t) s].p[(size_t) k], 1.0e-6f);

            // saved in presets
            const auto q = ProgramState::fromJson (p.toJson());
            expectEquals ((int) q.maps.size(), (int) p.maps.size());
            proc.removeMacroAssignment (Drip, 0, 3);
            expectEquals ((int) proc.getMacroAssignmentsFor (0, 3).size(), 0);
            proc.undo();
            expectEquals ((int) proc.getMacroAssignmentsFor (0, 3).size(), 1);
        }
    }
};
static ModSourceTest modSourceTest;
