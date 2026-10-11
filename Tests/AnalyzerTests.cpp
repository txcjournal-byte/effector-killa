// SPECTRAL VIEW (SlotTap FIFO + SpectrumAnalyzer) and LEVEL-MATCHED POWER (per-unit audition).
#include "TestHelpers.h"
#include "ui/rack/SpectrumOverlay.h"
#include "PluginProcessor.h"
#include <thread>

using namespace ektest;

class AnalyzerTest : public juce::UnitTest
{
public:
    AnalyzerTest() : juce::UnitTest ("Analyzer / level-matched power", "analyzer") {}

    static RackConfig config (std::initializer_list<EffectType> types)
    {
        RackConfig c;
        int i = 0;
        for (auto t : types) c.slots[(size_t) i++].type = t;
        return c;
    }

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

    // Tone Up as a (roughly) broadband +6 dB gain: all bands +6 dB, low-cut off.
    static void toneUpPlus6 (RackParams& p, int slot)
    {
        const auto& info = effectInfo (EffectType::ToneUp);
        auto& sp = p.slots[(size_t) slot].p;
        sp[0] = info.params[0].fromReal (10.0f);
        for (int k : { 1, 2, 4, 6 }) sp[(size_t) k] = info.params[(size_t) k].fromReal (6.0f);
    }

    void runTest() override
    {
        testFifo();
        testAnalyzerPeak();
        testTapThroughRack();
        testAuditionLevel();
        testAuditionLatency();
        testAuditionClicks();
        testBitIdentical();
        testProcessorApi();
    }

    // ------------------------------------------------------------------------------------------
    void testFifo()
    {
        beginTest ("SpectrumFifo: lock-free SPSC, ordered, nothing lost or corrupted");
        SpectrumFifo fifo;
        constexpr int total = 400000;
        std::atomic<bool> producerDone { false };
        std::thread producer ([&]
        {
            std::vector<float> b (700), a (700);
            juce::Random rnd (3);
            int next = 0;
            while (next < total)
            {
                const int n = std::min (total - next, 1 + rnd.nextInt (699));
                for (int i = 0; i < n; ++i) { b[(size_t) i] = (float) (next + i); a[(size_t) i] = -(float) (next + i); }
                int done = 0;
                while (done < n) // the real audio thread drops instead; the test retries to check ordering
                {
                    const int w = fifo.push (b.data() + done, b.data() + done, a.data() + done, a.data() + done, n - done);
                    done += w;
                    if (w == 0) std::this_thread::yield();
                }
                next += n;
            }
            producerDone = true;
        });

        std::vector<float> rb (1000), ra (1000);
        int expected = 0;
        bool ordered = true;
        while (expected < total)
        {
            const int got = fifo.pop (rb.data(), ra.data(), 1 + (expected % 997));
            for (int i = 0; i < got; ++i)
            {
                if (rb[(size_t) i] != (float) expected || ra[(size_t) i] != -(float) expected) ordered = false;
                ++expected;
            }
            if (got == 0)
            {
                if (producerDone && fifo.getNumReady() == 0) break;
                std::this_thread::yield();
            }
        }
        producer.join();
        expect (ordered, "FIFO samples out of order or corrupted");
        expectEquals (expected, total);

        // full FIFO: push drops (never blocks) and reports the partial count
        SpectrumFifo full;
        std::vector<float> big ((size_t) SpectrumFifo::kCapacity + 100, 0.25f);
        const int w = full.push (big.data(), big.data(), big.data(), big.data(), (int) big.size());
        expect (w < (int) big.size() && w >= SpectrumFifo::kCapacity - 1, "overfull push must be clamped, got " + juce::String (w));
        expectEquals (full.push (big.data(), big.data(), big.data(), big.data(), 10), 0);
        full.discardAll();
        expectEquals (full.getNumReady(), 0);
    }

    // ------------------------------------------------------------------------------------------
    void testAnalyzerPeak()
    {
        beginTest ("SpectrumAnalyzer: sine peak at the right bin and level");
        const double sr = 48000.0;
        SpectrumAnalyzer an;
        an.setSampleRate (sr);
        const int bin = 100; // 2343.75 Hz
        const double f = bin * sr / SpectrumAnalyzer::fftSize;
        std::vector<float> b (4096), a (4096);
        for (int i = 0; i < 4096; ++i)
        {
            const float s = (float) std::sin (juce::MathConstants<double>::twoPi * f * i / sr);
            b[(size_t) i] = 0.5f * s;  // -6 dB
            a[(size_t) i] = 0.25f * s; // -12 dB
        }
        an.push (b.data(), a.data(), 4096);
        expect (an.analyse());
        expectEquals (an.getPeakBin (SpectrumAnalyzer::Before), bin);
        expectEquals (an.getPeakBin (SpectrumAnalyzer::After), bin);
        expectWithinAbsoluteError (an.getBinDb (SpectrumAnalyzer::Before, bin), -6.02f, 0.3f);
        expectWithinAbsoluteError (an.getBinDb (SpectrumAnalyzer::After, bin), -12.04f, 0.3f);
        // far away from the tone the Hann window keeps leakage low
        expect (an.getBinDb (SpectrumAnalyzer::Before, bin + 40) < -60.0f, "leakage too high");

        // log-frequency display points: the loudest point is the one closest to the tone
        int bestP = 0;
        for (int p = 1; p < SpectrumAnalyzer::numPoints; ++p)
            if (an.getPointDb (0, p) > an.getPointDb (0, bestP)) bestP = p;
        expect (std::abs (std::log2 (an.getPointHz (bestP) / (float) f)) < 0.1f,
                "display peak at " + juce::String (an.getPointHz (bestP)) + " Hz");

        // a different tone after reset, at a non-integer bin
        an.reset();
        expect (! an.hasData());
        const double f2 = 440.0;
        for (int i = 0; i < 4096; ++i)
            b[(size_t) i] = a[(size_t) i] = 0.8f * (float) std::sin (juce::MathConstants<double>::twoPi * f2 * i / sr);
        an.push (b.data(), a.data(), 4096);
        an.analyse();
        const int pk = an.getPeakBin (0);
        expect (std::abs (an.binToHz (pk) - (float) f2) <= (float) (sr / SpectrumAnalyzer::fftSize),
                "440 Hz peak found at " + juce::String (an.binToHz (pk)));

        // smoothing: a sudden drop decays instead of jumping
        std::fill (b.begin(), b.end(), 0.0f);
        std::fill (a.begin(), a.end(), 0.0f);
        an.push (b.data(), a.data(), 4096);
        an.analyse();
        expect (an.getBinDb (0, pk) > -40.0f, "release smoothing missing");
        for (int k = 0; k < 60; ++k) { an.push (b.data(), a.data(), 1024); an.analyse(); }
        expect (an.getBinDb (0, pk) < -80.0f, "spectrum must decay to the floor in silence");
        for (int p = 0; p < SpectrumAnalyzer::numPoints; ++p)
            expect (std::isfinite (an.getPointDb (0, p)) && std::isfinite (an.getPointDb (1, p)));
    }

    // ------------------------------------------------------------------------------------------
    void testTapThroughRack()
    {
        beginTest ("SlotTap: before/after of the selected slot, zero traffic when off");
        const double sr = 48000.0;
        auto cfg = config ({ EffectType::WideBody, EffectType::ToneUp });
        Rack rack;
        rack.setConfig (cfg);
        rack.prepare (sr, 1);
        auto params = defaults (cfg);
        const auto& info = effectInfo (EffectType::ToneUp);
        params.slots[1].p[6] = info.params[6].fromReal (12.0f); // HIGH +12 dB (8 kHz shelf)
        ProcessContext ctx; ctx.sampleRate = sr;
        auto& tap = rack.getSlotTap();

        std::vector<float> L (512), R (512);
        double ph = 0.0;
        const double f = 512 * sr / SpectrumAnalyzer::fftSize; // 12 kHz, exactly bin 512
        auto run = [&] (int blocks)
        {
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    ph += f / sr;
                    L[(size_t) i] = R[(size_t) i] = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * ph);
                }
                rack.process (L.data(), R.data(), 512, params, ctx);
            }
        };

        run (20);
        expectEquals (tap.getFifo().getNumReady(), 0); // off: nothing copied

        SpectrumOverlay overlay (tap);
        overlay.setSlot (1);
        expectEquals (tap.getAnalyzedSlot(), 1);
        run (20);
        expect (tap.getFifo().getNumReady() >= 4096, "tap must deliver samples of slot 2");
        expect (overlay.update(), "analysis expected");
        auto& an = overlay.getAnalyzer();
        expectEquals (an.getPeakBin (SpectrumAnalyzer::Before), 512);
        expectEquals (an.getPeakBin (SpectrumAnalyzer::After), 512);
        const float diff = an.getBinDb (SpectrumAnalyzer::After, 512) - an.getBinDb (SpectrumAnalyzer::Before, 512);
        logMessage ("Tone Up HIGH +12 dB at 12 kHz: after - before = " + juce::String (diff, 2) + " dB");
        expect (diff > 9.0f && diff < 13.0f, "after - before should show the shelf gain, got " + juce::String (diff));
        expectWithinAbsoluteError (an.getBinDb (SpectrumAnalyzer::Before, 512), -20.0f, 0.5f);

        // paint into an image (smoke test: no crash, something drawn)
        juce::Image img (juce::Image::ARGB, 300, 120, true);
        {
            juce::Graphics g (img);
            overlay.paint (g, { 0.0f, 0.0f, 300.0f, 120.0f });
        }
        int lit = 0;
        for (int y = 0; y < 120; ++y) for (int x = 0; x < 300; ++x) if (img.getPixelAt (x, y).getAlpha() > 0) ++lit;
        expect (lit > 300, "overlay painted nothing");

        overlay.setSlot (-1);
        expectEquals (tap.getAnalyzedSlot(), -1);
        run (5);
        expectEquals (tap.getFifo().getNumReady(), 0);
    }

    // ------------------------------------------------------------------------------------------
    struct AuditionRun
    {
        float withDb = 0, auditionDb = 0, bypassDb = 0, gainDb = 0;
        bool finite = true;
    };

    AuditionRun runAudition (double sr, int osLog2, TestSignals::Kind kind)
    {
        AuditionRun r;
        const int total = (int) (7.0 * sr), onAt = (int) (3.5 * sr);
        std::vector<float> inL ((size_t) total), inR ((size_t) total);
        TestSignals::generate (kind, sr, inL.data(), inR.data(), total, 11);

        auto cfg = config ({ EffectType::ToneUp });
        auto render = [&] (bool paused, bool audition, LoudnessMeter& m1, LoudnessMeter& m2)
        {
            Rack rack;
            rack.setConfig (cfg);
            rack.prepare (sr, osLog2);
            auto params = defaults (cfg);
            toneUpPlus6 (params, 0);
            params.slots[0].pause = paused;
            ProcessContext ctx; ctx.sampleRate = sr;
            rack.getSlotTap().setAnalyzedSlot (0); // hovered first, as in the UI
            std::vector<float> L (256), R (256);
            for (int pos = 0; pos < total; pos += 256)
            {
                const int n = std::min (256, total - pos);
                if (audition && pos >= onAt) rack.getSlotTap().setAuditionSlot (0);
                std::copy (inL.begin() + pos, inL.begin() + pos + n, L.begin());
                std::copy (inR.begin() + pos, inR.begin() + pos + n, R.begin());
                rack.process (L.data(), R.data(), n, params, ctx);
                if (! allFinite (L.data(), n) || ! allFinite (R.data(), n)) r.finite = false;
                if (pos >= (int) (1.0 * sr) && pos < (int) (3.4 * sr)) m1.process (L.data(), R.data(), n);
                if (pos >= (int) (4.5 * sr)) m2.process (L.data(), R.data(), n);
            }
            r.gainDb = rack.getSlotTap().getAuditionGainDb();
        };
        LoudnessMeter withM, audM, bypM, dummy;
        for (auto* m : { &withM, &audM, &bypM, &dummy }) m->prepare (sr);
        render (false, true, withM, audM);
        const float g = r.gainDb;
        render (true, false, dummy, bypM);
        r.gainDb = g;
        r.withDb = withM.getLoudnessDb();
        r.auditionDb = audM.getLoudnessDb();
        r.bypassDb = bypM.getLoudnessDb();
        return r;
    }

    void testAuditionLevel()
    {
        beginTest ("LEVEL-MATCHED POWER: without-unit level matches the with-unit level (+-1 dB)");
        for (double sr : { 44100.0, 96000.0 })
            for (auto kind : { TestSignals::Kind::PinkNoise, TestSignals::Kind::Vocal, TestSignals::Kind::DrumLoop })
            {
                const auto r = runAudition (sr, 1, kind);
                logMessage (juce::String (sr) + " Hz kind " + juce::String ((int) kind) + ": with " + juce::String (r.withDb, 2)
                            + " dB, bypass " + juce::String (r.bypassDb, 2) + " dB, audition " + juce::String (r.auditionDb, 2)
                            + " dB (applied " + juce::String (r.gainDb, 2) + " dB)");
                expect (r.finite, "NaN / runaway output");
                expect (r.withDb - r.bypassDb > 3.0f, "Tone Up +6 should be clearly louder than bypass");
                expectWithinAbsoluteError (r.auditionDb, r.withDb, 1.0f);
                expect (r.gainDb > 3.0f && r.gainDb <= SlotTap::maxMatchDb, "applied gain readout " + juce::String (r.gainDb));
            }

        beginTest ("LEVEL-MATCHED POWER: silence and extreme input stay finite, gain clamped");
        {
            const double sr = 48000.0;
            auto cfg = config ({ EffectType::ToneUp, EffectType::Menace });
            Rack rack;
            rack.setConfig (cfg);
            rack.prepare (sr, 1);
            auto params = defaults (cfg);
            toneUpPlus6 (params, 0);
            params.slots[1].p[1] = 1.0f; // full drive
            ProcessContext ctx; ctx.sampleRate = sr;
            rack.getSlotTap().setAuditionSlot (1);
            std::vector<float> L (512), R (512);
            juce::Random rnd (5);
            bool finite = true;
            float maxGain = 0.0f;
            for (int b = 0; b < 400; ++b)
            {
                const float amp = b < 100 ? 0.0f : (b < 200 ? 1.0e-6f : (b < 300 ? 30.0f : 0.5f));
                for (int i = 0; i < 512; ++i) { L[(size_t) i] = amp * (rnd.nextFloat() * 2 - 1); R[(size_t) i] = amp * (rnd.nextFloat() * 2 - 1); }
                rack.process (L.data(), R.data(), 512, params, ctx);
                finite = finite && allFinite (L.data(), 512, 1.0e4f) && allFinite (R.data(), 512, 1.0e4f);
                maxGain = std::max (maxGain, std::abs (rack.getSlotTap().getAuditionGainDb()));
                if (b == 99) expectWithinAbsoluteError (rack.getSlotTap().getAuditionGainDb(), 0.0f, 0.01f); // silence: hold 0 dB
            }
            expect (finite, "NaN / Inf during audition");
            expect (maxGain <= SlotTap::maxMatchDb + 0.01f, "gain must stay within +-12 dB");
        }
    }

    // ------------------------------------------------------------------------------------------
    void testAuditionLatency()
    {
        beginTest ("LEVEL-MATCHED POWER: timing does not jump (latency-compensated bypass)");
        for (int os = 0; os <= 2; ++os)
        {
            const double sr = 48000.0;
            auto cfg = config ({ EffectType::CassettePlug, EffectType::ToneUp, EffectType::RedLine });
            const int fixed = Rack::computeFixedLatency (sr, os);
            for (int slot : { -1, 0, 2 })
            {
                Rack rack;
                rack.setConfig (cfg);
                rack.prepare (sr, os);
                expectEquals (rack.getFixedLatency(), fixed);
                auto params = defaults (cfg);
                params.slots[0].p[1] = 0.0f; // no drive
                params.slots[2].p[0] = 1.0f; // ceiling 0 dB
                rack.getSlotTap().setAuditionSlot (slot);
                ProcessContext ctx; ctx.sampleRate = sr;
                const int n = 8192, at = 4000; // well after the 20 ms fade-in
                std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
                L[at] = R[at] = 0.05f;
                // a little constant hum so the meters have signal
                for (int i = 0; i < n; ++i) { L[(size_t) i] += 0.001f * (float) std::sin (0.01 * i); R[(size_t) i] = L[(size_t) i]; }
                for (int pos = 0; pos < n; pos += 256) rack.process (L.data() + pos, R.data() + pos, 256, params, ctx);
                int peak = at;
                for (int i = at; i < n; ++i) if (std::abs (L[(size_t) i]) > std::abs (L[(size_t) peak])) peak = i;
                expect (std::abs (peak - at - fixed) <= 2, "os " + juce::String (os) + " slot " + juce::String (slot)
                        + ": impulse at " + juce::String (peak - at) + ", expected " + juce::String (fixed));
                expectEquals (rack.getActiveLatency(), fixed);
            }
        }
    }

    // ------------------------------------------------------------------------------------------
    void testAuditionClicks()
    {
        beginTest ("LEVEL-MATCHED POWER: no clicks when toggling, switching units or swapping the chain");
        const double sr = 44100.0;
        auto cfg = config ({ EffectType::ToneUp, EffectType::ToneUp });
        Rack rack;
        rack.setConfig (cfg);
        rack.prepare (sr, 1);
        auto params = defaults (cfg);
        toneUpPlus6 (params, 0);
        toneUpPlus6 (params, 1);
        ProcessContext ctx; ctx.sampleRate = sr;
        std::vector<float> L (256), R (256);
        double phase = 0.0;
        float prev = 0.0f, maxStep = 0.0f, steadyStep = 0.0f;
        bool finite = true;
        const float amp = 0.3f;
        for (int block = 0; block < 900; ++block)
        {
            const int phaseOfCycle = block % 60;
            if (phaseOfCycle == 10) rack.getSlotTap().setAuditionSlot ((block / 60) % 3 == 2 ? 1 : 0);
            if (phaseOfCycle == 30 && (block / 60) % 3 == 1) rack.getSlotTap().setAuditionSlot (1); // direct unit switch
            if (phaseOfCycle == 40) rack.getSlotTap().setAuditionSlot (-1);
            if (block == 315) rack.setConfig (config ({ EffectType::ToneUp, EffectType::ToneUp, EffectType::WideBody })); // swap mid-audition
            for (int i = 0; i < 256; ++i)
            {
                phase += 110.0 / sr;
                L[(size_t) i] = R[(size_t) i] = amp * (float) std::sin (juce::MathConstants<double>::twoPi * phase);
            }
            rack.process (L.data(), R.data(), 256, params, ctx);
            finite = finite && allFinite (L.data(), 256) && allFinite (R.data(), 256);
            const bool steady = phaseOfCycle < 8 && block > 60; // with-unit sound, nothing switching
            for (int i = 0; i < 256; ++i)
            {
                const float step = std::abs (L[(size_t) i] - prev);
                if (block > 4) maxStep = std::max (maxStep, step);
                if (steady) steadyStep = std::max (steadyStep, step);
                prev = L[(size_t) i];
            }
            rack.collectGarbage();
        }
        // level-matched: the audition is as loud as the units, so a smooth 20 ms crossfade never
        // produces a bigger step than the steady with-unit signal itself
        logMessage ("max step " + juce::String (maxStep, 5) + ", steady " + juce::String (steadyStep, 5));
        expect (finite);
        expect (maxStep <= steadyStep * 1.15f + 0.002f, "click while toggling the audition: " + juce::String (maxStep));
    }

    // ------------------------------------------------------------------------------------------
    void testBitIdentical()
    {
        beginTest ("Off = bit-identical (analyzer on / audition released)");
        const double sr = 48000.0;
        auto cfg = config ({ EffectType::ToneUp });
        auto params = defaults (cfg);
        toneUpPlus6 (params, 0);
        const int total = 120 * 256;
        std::vector<float> inL ((size_t) total), inR ((size_t) total);
        TestSignals::generate (TestSignals::Kind::PinkNoise, sr, inL.data(), inR.data(), total, 2);

        auto render = [&] (int mode, std::vector<float>& outL, std::vector<float>& outR)
        {
            Rack rack;
            rack.setConfig (cfg);
            rack.prepare (sr, 1);
            ProcessContext ctx; ctx.sampleRate = sr;
            outL = inL; outR = inR;
            if (mode >= 1) rack.getSlotTap().setAnalyzedSlot (0);
            for (int b = 0; b < 120; ++b)
            {
                if (mode == 2 && b == 20) rack.getSlotTap().setAuditionSlot (0);
                if (mode == 2 && b == 50) rack.getSlotTap().setAuditionSlot (-1);
                rack.process (outL.data() + b * 256, outR.data() + b * 256, 256, params, ctx);
            }
        };
        std::vector<float> refL, refR, anL, anR, auL, auR;
        render (0, refL, refR);
        render (1, anL, anR);
        render (2, auL, auR);
        expect (refL == anL && refR == anR, "analyzer tap changed the audio");
        bool before = true, after = true, during = false;
        for (int i = 0; i < total; ++i)
        {
            const bool same = refL[(size_t) i] == auL[(size_t) i] && refR[(size_t) i] == auR[(size_t) i];
            if (i < 20 * 256) before = before && same;
            else if (i >= 70 * 256) after = after && same;
            else if (! same) during = true;
        }
        expect (before, "audition changed the audio before it was pressed");
        expect (after, "audio not bit-identical after the audition was released");
        expect (during, "audition had no effect while held");
    }

    // ------------------------------------------------------------------------------------------
    void testProcessorApi()
    {
        beginTest ("Processor API: setAnalyzedSlot / setLevelMatchedAudition keep latency and default output");
        auto renderProc = [] (bool useApi, std::vector<float>& out, int& latency)
        {
            EffectorKillaAudioProcessor proc;
            proc.prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            out.clear();
            for (int b = 0; b < 60; ++b)
            {
                if (useApi && b == 10) { proc.setAnalyzedSlot (0); proc.setLevelMatchedAudition (0); }
                if (useApi && b == 20) proc.setLevelMatchedAudition (-1);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (c, i, 0.3f * std::sin ((float) (b * 512 + i) * 0.031f));
                proc.processBlock (buf, midi);
                for (int i = 0; i < 512; ++i) out.push_back (buf.getSample (0, i));
            }
            latency = proc.getLatencySamples();
            if (useApi) proc.setAnalyzedSlot (-1);
        };
        std::vector<float> a, b;
        int latA = 0, latB = 0;
        renderProc (false, a, latA);
        renderProc (true, b, latB);
        expectEquals (latB, latA);
        expect (allFinite (b.data(), (int) b.size()));
        bool tailSame = true;
        for (size_t i = 40 * 512; i < a.size(); ++i) tailSame = tailSame && a[i] == b[i];
        logMessage ("default program after the audition is released: " + juce::String (tailSame ? "bit-identical" : "differs (unit tails)"));
    }
};

static AnalyzerTest analyzerTest;
