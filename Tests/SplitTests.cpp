// SPLIT / MERGE: parallel branches inside the rack (Source/engine/Split.*).
#include "TestHelpers.h"
#include "PluginProcessor.h"

using namespace ektest;

class SplitTest : public juce::UnitTest
{
public:
    SplitTest() : juce::UnitTest ("Split / merge", "split") {}

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

    // Runs L/R through a Chain in 256-sample chunks.
    static void runChain (Chain& chain, std::vector<float>& L, std::vector<float>& R, const RackParams& params, double sr)
    {
        ProcessContext ctx;
        ctx.sampleRate = sr;
        const int n = (int) L.size();
        for (int pos = 0; pos < n; pos += 256)
        {
            const int len = std::min (256, n - pos);
            chain.process (L.data() + pos, R.data() + pos, len, &params, ctx);
        }
    }

    static void noise (std::vector<float>& L, std::vector<float>& R, int n, uint64_t seed, float amp = 0.5f)
    {
        Rng r;
        r.setSeed (seed);
        L.resize ((size_t) n);
        R.resize ((size_t) n);
        for (int i = 0; i < n; ++i) { L[(size_t) i] = amp * r.bipolar(); R[(size_t) i] = amp * r.bipolar(); }
    }

    // Largest |split(in) - serial(in)| for an empty split (both branches empty). The reference is the
    // empty serial rack, i.e. the input delayed by exactly the rack's own latency compensation.
    double emptySplitError (SplitMode mode, float balance, double sr, const std::vector<float>& inL, const std::vector<float>& inR)
    {
        PrepareSpec spec; spec.sampleRate = sr; spec.osFactorLog2 = 1;
        const int fixed = Rack::computeFixedLatency (sr, 1);
        RackConfig serialCfg, cfg;
        cfg.split.at = 0;
        cfg.split.mode = mode;
        Chain serial (serialCfg, spec, fixed), chain (cfg, spec, fixed);
        expectEquals (chain.getTotalLatency(), fixed);
        auto params = defaults (cfg);
        params.split.balance = balance;
        auto L = inL, R = inR, refL = inL, refR = inR;
        runChain (serial, refL, refR, defaults (serialCfg), sr);
        runChain (chain, L, R, params, sr);
        double err = 0.0, refEnergy = 0.0;
        for (size_t i = 0; i < L.size(); ++i)
        {
            err = std::max (err, (double) std::abs (L[i] - refL[i]));
            err = std::max (err, (double) std::abs (R[i] - refR[i]));
            refEnergy += std::abs (refL[i]);
        }
        expect (refEnergy > 1.0, "reference must carry signal");
        return err;
    }

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("empty branches sum back to the input");
        {
            std::vector<float> L, R;
            noise (L, R, 48000, 11);
            // drum-like bursts for the transient detector
            for (int i = 0; i < 48000; ++i)
            {
                const float env = std::exp (-(float) (i % 6000) / 600.0f);
                L[(size_t) i] *= env; R[(size_t) i] *= env;
            }
            expect (emptySplitError (SplitMode::Parallel, 0.5f, sr, L, R) == 0.0, "PARALLEL 0.5 must be exact unity");
            expect (emptySplitError (SplitMode::LeftRight, 0.5f, sr, L, R) == 0.0, "L/R must be exact");
            const double ms = emptySplitError (SplitMode::MidSide, 0.5f, sr, L, R);
            expect (ms < 1.0e-6, "M/S error " + juce::String (ms));
            const double ts = emptySplitError (SplitMode::TransientSustain, 0.5f, sr, L, R);
            expect (ts < 1.0e-6, "TRANSIENT/SUSTAIN error " + juce::String (ts));
            logMessage ("M/S max error " + juce::String (ms) + ", TRANS/SUS " + juce::String (ts));
        }

        beginTest ("LOW/HIGH is flat within 0.1 dB (Linkwitz-Riley sum)");
        {
            for (float xover : { 80.0f, 250.0f, 1000.0f, 4000.0f, 12000.0f })
            {
                double worst = 0.0;
                for (double f : { 30.0, 60.0, 120.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 12000.0, 16000.0 })
                {
                    RackConfig cfg;
                    cfg.split.at = 0;
                    cfg.split.mode = SplitMode::LowHigh;
                    PrepareSpec spec; spec.sampleRate = sr;
                    Chain chain (cfg, spec, Rack::computeFixedLatency (sr, 1));
                    auto params = defaults (cfg);
                    params.split.freqHz = xover;
                    const int n = 48000;
                    std::vector<float> L ((size_t) n), R ((size_t) n);
                    for (int i = 0; i < n; ++i)
                        L[(size_t) i] = R[(size_t) i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * f * i / sr);
                    runChain (chain, L, R, params, sr);
                    const int skip = 24000;
                    const double mag = toneMagnitude (L.data() + skip, n - skip, f, sr);
                    const double db = 20.0 * std::log10 (mag / 0.5);
                    worst = std::max (worst, std::abs (db));
                    expect (std::abs (db) < 0.1, "xover " + juce::String (xover) + " Hz, tone " + juce::String (f) + " Hz: " + juce::String (db, 3) + " dB");
                    expect (allFinite (L.data(), n) && allFinite (R.data(), n));
                }
                logMessage ("crossover " + juce::String (xover) + " Hz: worst deviation " + juce::String (worst, 4) + " dB");
            }
        }

        beginTest ("no split = bit-identical output (old presets)");
        {
            const std::pair<int, int> presets[] = { { 0, 0 }, { 2, 0 }, { 4, 1 }, { 9, 2 }, { 11, 0 } };
            for (auto [ch, idx] : presets)
            {
                const auto* prog = factoryBank().getFactory (ch, idx);
                if (prog == nullptr) { expect (false, "missing factory preset"); continue; }
                expect (! prog->split.isActive());
                const auto json = prog->toJson();
                expect (! json.contains ("\"split\"") && ! json.contains ("\"branch\""), "old preset JSON must not change");

                const int n = 24000;
                std::vector<float> L0 ((size_t) n), R0 ((size_t) n);
                TestSignals::generate (TestSignals::forSource (prog->source), sr, L0.data(), R0.data(), n, 3);

                auto render = [&] (const ProgramState& p)
                {
                    auto L = L0, R = R0;
                    renderProgram (p, sr, 1, L.data(), R.data(), n);
                    return std::make_pair (L, R);
                };
                const auto ref = render (*prog);
                const auto reloaded = render (ProgramState::fromJson (json));
                expect (ref == reloaded, "JSON round trip changed the sound");

                // A split behind the last unit (PARALLEL 0.5) and a split in front with every unit in A at
                // balance 0 are both transparent: the result must be bit-identical, too.
                auto behind = *prog;
                behind.split.at = kNumSlots;
                expect (render (behind) == ref, "split at the end must be transparent");

                auto allA = *prog;
                allA.split.at = 0;
                allA.split.params.balance = 0.0f;
                expect (render (allA) == ref, "split with every unit in branch A (balance A) must be transparent");
            }
        }

        beginTest ("fixed latency with unequal branches, SOLO and SCREENING");
        {
            for (int os = 0; os <= 2; ++os)
            {
                const int fixed = Rack::computeFixedLatency (sr, os);
                RackConfig cfg;
                cfg.slots[0].type = EffectType::CassettePlug;  // in front of the split
                cfg.slots[1].type = EffectType::Menace;        // A
                cfg.slots[2].type = EffectType::RedLine;       // A (lookahead)
                cfg.slots[3].type = EffectType::ToneUp;        // B (no latency)
                cfg.split.at = 1;
                cfg.split.branch[3] = 1;
                for (int mode = 0; mode < kNumSplitModes; ++mode)
                {
                    cfg.split.mode = (SplitMode) mode;
                    for (int solo : { -1, 0, 1, 3 })
                        for (int screening : { -1, 0, 2, 3 })
                        {
                            auto c = cfg;
                            c.solo = solo;
                            c.screening = solo >= 0 ? -1 : screening;
                            PrepareSpec spec; spec.sampleRate = sr; spec.osFactorLog2 = os;
                            Chain chain (c, spec, fixed);
                            expectEquals (chain.getTotalLatency(), fixed);
                        }
                }

                // impulse through near-linear units: both branches arrive together, at the fixed latency
                cfg.split.mode = SplitMode::Parallel;
                Rack rack;
                rack.setConfig (cfg);
                rack.prepare (sr, os);
                auto params = defaults (cfg);
                params.slots[0].p[1] = 0.0f; // Cassette Plug: no drive
                params.slots[1].p[1] = 0.0f; // Menace: no drive
                params.slots[2].p[0] = 1.0f; // Red Line: ceiling 0 dB
                const int n = 8192;
                std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
                L[100] = R[100] = 0.05f;
                ProcessContext ctx; ctx.sampleRate = sr;
                for (int pos = 0; pos < n; pos += 256) rack.process (L.data() + pos, R.data() + pos, 256, params, ctx);
                int peak = 0;
                for (int i = 0; i < n; ++i) if (std::abs (L[(size_t) i]) > std::abs (L[(size_t) peak])) peak = i;
                expect (std::abs (peak - 100 - fixed) <= 2, "os " + juce::String (1 << os) + ": impulse at " + juce::String (peak - 100) + ", expected " + juce::String (fixed));
                // the two branch impulses add up (0.5 + 0.5): nothing smeared into a second peak
                expect (std::abs (L[(size_t) peak]) > 0.03f, "branches not aligned: peak " + juce::String (L[(size_t) peak]));
            }
        }

        beginTest ("no NaN / Inf with full branches, extreme and invalid values");
        {
            RackConfig cfg;
            const EffectType types[] = { EffectType::Squeeze, EffectType::Menace, EffectType::AuraRoom, EffectType::AdLibThrow,
                                         EffectType::Brainrot, EffectType::WideBody, EffectType::Chopped, EffectType::RedLine };
            for (int i = 0; i < kNumSlots; ++i) cfg.slots[(size_t) i].type = types[i];
            cfg.split.at = 2;
            cfg.split.branch = { 0, 0, 0, 1, 0, 1, 1, 0 };
            for (int mode = 0; mode < kNumSplitModes; ++mode)
            {
                cfg.split.mode = (SplitMode) mode;
                PrepareSpec spec; spec.sampleRate = 44100.0; spec.osFactorLog2 = 1;
                Chain chain (cfg, spec, Rack::computeFixedLatency (44100.0, 1));
                auto params = defaults (cfg);
                for (auto& s : params.slots) for (auto& v : s.p) v = 1.0f;
                std::vector<float> L, R;
                noise (L, R, 44100, 99 + (uint64_t) mode, 2.0f);
                ProcessContext ctx; ctx.sampleRate = 44100.0;
                const float freqs[] = { 1.0f, 40.0f, 12000.0f, 1.0e9f, std::numeric_limits<float>::quiet_NaN() };
                int k = 0;
                for (int pos = 0; pos < 44100; pos += 441, ++k)
                {
                    params.split.freqHz = freqs[k % 5];
                    params.split.balance = (k % 3) == 0 ? -3.0f : ((k % 3) == 1 ? 7.0f : std::numeric_limits<float>::infinity());
                    params.split.levelADb = (k % 2) ? 100.0f : -1000.0f;
                    params.split.levelBDb = std::numeric_limits<float>::quiet_NaN();
                    chain.process (L.data() + pos, R.data() + pos, 441, &params, ctx);
                }
                expect (allFinite (L.data(), 44100, 1.0e4f) && allFinite (R.data(), 44100, 1.0e4f), splitModeName ((SplitMode) mode));
            }
            const auto s = SplitParams { std::numeric_limits<float>::quiet_NaN(), 9.0f, -99.0f, 99.0f }.sanitised();
            expect (std::isfinite (s.freqHz) && s.balance == 1.0f && s.levelADb == SplitParams::minLevelDb && s.levelBDb == SplitParams::maxLevelDb);
        }

        beginTest ("no clicks when the split is toggled or its knobs jump");
        {
            const double fs = 44100.0;
            RackConfig serial;
            serial.slots[0].type = EffectType::ToneUp;
            serial.slots[1].type = EffectType::CassettePlug;
            serial.slots[2].type = EffectType::Squeeze;
            RackConfig split = serial;
            split.split.at = 1;
            split.split.branch[2] = 1;
            const SplitMode modes[] = { SplitMode::LowHigh, SplitMode::MidSide, SplitMode::TransientSustain, SplitMode::Parallel, SplitMode::LeftRight };

            Rack rack;
            rack.setConfig (serial);
            rack.prepare (fs, 1);
            auto params = defaults (serial);
            ProcessContext ctx; ctx.sampleRate = fs;
            std::vector<float> L (256), R (256);
            double phase = 0.0;
            float prev = 0.0f, maxJump = 0.0f, steadyJump = 0.0f;
            for (int block = 0; block < 600; ++block)
            {
                if (block % 40 == 20)
                {
                    const int k = block / 40;
                    if (k % 2 == 0) rack.setConfig (serial);
                    else { split.split.mode = modes[(k / 2) % 5]; rack.setConfig (split); }
                }
                // knob jumps (smoothed inside the splitter)
                if (block % 40 == 35)
                {
                    params.split.freqHz = params.split.freqHz > 1000.0f ? 120.0f : 6000.0f;
                    params.split.balance = params.split.balance > 0.5f ? 0.2f : 0.8f;
                }
                for (int i = 0; i < 256; ++i)
                {
                    phase += 220.0 / fs;
                    L[(size_t) i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
                    R[(size_t) i] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * phase * 1.01);
                }
                rack.process (L.data(), R.data(), 256, params, ctx);
                const bool changing = (block % 40) >= 20;
                for (int i = 0; i < 256; ++i)
                {
                    const float jump = std::abs (L[(size_t) i] - prev);
                    if (block > 4) (changing ? maxJump : steadyJump) = std::max (changing ? maxJump : steadyJump, jump);
                    prev = L[(size_t) i];
                }
                expect (allFinite (L.data(), 256) && allFinite (R.data(), 256));
                rack.collectGarbage();
            }
            logMessage ("steady max step " + juce::String (steadyJump, 4) + ", while toggling " + juce::String (maxJump, 4));
            expect (maxJump <= steadyJump * 1.25f + 0.005f, "click while toggling the split: " + juce::String (maxJump));
        }

        beginTest ("preset / project state round trip");
        {
            auto prog = *factoryBank().getFactory (9, 2);
            prog.split.at = 1;
            prog.split.mode = SplitMode::LowHigh;
            prog.split.params.freqHz = 180.0f;
            prog.split.params.balance = 0.35f;
            prog.split.params.levelADb = -3.0f;
            prog.split.params.levelBDb = 2.5f;
            prog.slots[2].branch = 1;
            const auto json = prog.toJson();
            expect (json.contains ("\"split\"") && json.contains ("\"version\""));
            const auto back = ProgramState::fromJson (json);
            expect (back.sameSound (prog));
            expect (back.split == prog.split);
            expectEquals (back.slots[2].branch, 1);
            expectEquals ((int) back.rackConfig().split.branch[2], 1);
            expect (back.rackConfig() == prog.rackConfig());

            // corrupt / future blocks never break loading
            auto weird = ProgramState::fromJson (json.replace ("\"LOWHIGH\"", "\"WHATEVER\"").replace ("\"at\": 1", "\"at\": 42"));
            expect (! weird.split.isActive());

            // processor state (host project)
            EffectorKillaAudioProcessor a;
            a.prepareToPlay (44100.0, 256);
            a.applyProgram (prog, false);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);
            EffectorKillaAudioProcessor b;
            b.prepareToPlay (44100.0, 256);
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (b.hasSplit() && b.getSplit() == prog.split);
            expect (b.captureProgram().sameSound (prog));
            expect (b.engine.getLatency() == a.engine.getLatency());

            // a project without split loads without one
            EffectorKillaAudioProcessor c;
            c.prepareToPlay (44100.0, 256);
            c.getStateInformation (mb);
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (! b.hasSplit());
        }

        beginTest ("processor API: insert / move / remove");
        {
            EffectorKillaAudioProcessor p;
            p.prepareToPlay (44100.0, 256);
            p.loadFactory (9, 2, false); // Night Drive: 4 units
            const int latency = p.engine.getLatency();
            const auto types = [&] { std::vector<EffectType> v; for (int i = 0; i < kNumSlots; ++i) v.push_back (p.getSlotType (i)); return v; };
            const auto before = types();

            p.insertSplit (2);
            expect (p.hasSplit() && p.getSplitPosition() == 2);
            expectEquals (p.getSlotBranch (0), -1);
            expectEquals (p.getSlotBranch (2), 0);
            expect (p.getBranchSlots (0) == std::vector<int> { 2, 3 });
            expect (p.getBranchSlots (1).empty());

            p.setSlotBranch (3, 1);
            expect (p.getBranchSlots (1) == std::vector<int> { 3 });
            expect (types() == before, "moving between branches keeps the slot order");

            // a unit in front of the split moves behind it
            p.setSlotBranch (0, 1);
            expectEquals (p.getSplitPosition(), 1);
            expect (p.getBranchSlots (-1) == std::vector<int> { 0 });
            expect (p.getSlotType (3) == before[0]);
            expectEquals (p.getSlotBranch (3), 1);

            // insert into B, remove in front of the split (split follows)
            const int s = p.insertEffectInBranch (1, EffectType::WideBody);
            expect (s >= p.getSplitPosition() && p.getSlotBranch (s) == 1);
            p.removeEffect (0);
            expectEquals (p.getSplitPosition(), 0);

            p.setSplitMode (SplitMode::MidSide);
            p.setSplitFreq (99999.0f);
            p.setSplitBalance (0.25f);
            p.setSplitLevel (1, -6.0f);
            expect (p.getSplit().mode == SplitMode::MidSide);
            expectEquals (p.getSplit().params.freqHz, SplitParams::maxFreq);
            expectEquals (p.getSplit().params.levelBDb, -6.0f);
            expect (p.captureProgram().split == p.getSplit());

            // undo walks back through the structural steps
            p.undo();
            expect (p.getSplit().mode == SplitMode::Parallel);
            p.removeSplit();
            expect (! p.hasSplit());
            for (int i = 0; i < kNumSlots; ++i) expectEquals (p.getSlotBranch (i), -1);
            p.undo();
            expect (p.hasSplit());

            // latency reported to the host never changes
            expectEquals (p.engine.getLatency(), latency);

            // and the audio path runs (no NaN) while switching
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            for (int k = 0; k < 40; ++k)
            {
                for (int i = 0; i < 512; ++i) buf.setSample (0, i, 0.3f * std::sin (0.05f * (float) (i + k * 512))), buf.setSample (1, i, buf.getSample (0, i));
                if (k == 10) p.setSplitMode (SplitMode::LowHigh);
                if (k == 20) p.removeSplit();
                if (k == 30) p.insertSplit (0);
                p.processBlock (buf, midi);
                expect (allFinite (buf.getReadPointer (0), 512) && allFinite (buf.getReadPointer (1), 512));
                p.engine.collectGarbage();
            }
        }
    }
};
static SplitTest splitTest;
