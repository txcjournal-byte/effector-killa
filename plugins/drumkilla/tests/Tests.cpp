// DSP unit tests for 808 KILLA. Run: K808Tests (exit code 0 = all passed)
#include "PluginProcessor.h"

using namespace juce;

namespace
{
    int failures = 0;

    void check (bool ok, const String& what)
    {
        if (! ok)
        {
            ++failures;
            std::cout << "  FAIL: " << what << std::endl;
        }
    }

    // 808-like test signal: pitched sine with a pitch drop and exponential decay, one note per 0.5 s
    float test808 (double t)
    {
        const auto local = std::fmod (t, 0.5);
        return (float) (0.8 * std::exp (-local * 4.0) * std::sin (2.0 * MathConstants<double>::pi * (45.0 * local + 3.0 * (1.0 - std::exp (-local * 40.0)))));
    }

    AudioBuffer<float> makeInput (double sampleRate, double seconds, int channels = 2)
    {
        AudioBuffer<float> b (channels, (int) (sampleRate * seconds));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto t = i / sampleRate;
            const auto v = t < seconds - 0.5 ? test808 (t) : 0.0f;
            for (int c = 0; c < channels; ++c)
                b.setSample (c, i, v * (c == 0 ? 1.0f : 0.9f));
        }
        return b;
    }

    AudioBuffer<float> run (K808Processor& p, const AudioBuffer<float>& input, double sampleRate, int blockSize,
                            const AudioBuffer<float>* sidechain = nullptr, int resetAtSample = -1)
    {
        const auto chans = input.getNumChannels();
        const auto total = chans + (sidechain != nullptr ? 2 : 0);
        p.setRateAndBufferSizeDetails (sampleRate, blockSize);
        p.prepareToPlay (sampleRate, blockSize);

        AudioBuffer<float> out (chans, input.getNumSamples());
        AudioBuffer<float> block (total, blockSize);
        MidiBuffer midi;

        for (int start = 0; start < input.getNumSamples(); start += blockSize)
        {
            const auto n = jmin (blockSize, input.getNumSamples() - start);
            AudioBuffer<float> view (block.getArrayOfWritePointers(), total, n);
            for (int c = 0; c < chans; ++c)
                view.copyFrom (c, 0, input, c, start, n);
            if (sidechain != nullptr)
                for (int c = 0; c < 2; ++c)
                    view.copyFrom (chans + c, 0, *sidechain, jmin (c, sidechain->getNumChannels() - 1), start, n);
            if (resetAtSample >= start && resetAtSample < start + blockSize)
                p.reset();                                   // what a host does when the transport stops
            p.processBlock (view, midi);
            for (int c = 0; c < chans; ++c)
                out.copyFrom (c, start, view, c, 0, n);
        }
        return out;
    }

    bool allFinite (const AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i)))
                    return false;
        return true;
    }

    void setParam (K808Processor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void enableSidechain (K808Processor& p, bool on)
    {
        auto layout = p.getBusesLayout();
        if (layout.inputBuses.size() > 1)
            layout.inputBuses.getReference (1) = on ? AudioChannelSet::stereo() : AudioChannelSet::disabled();
        p.setBusesLayout (layout);
    }
}

int main()
{
    ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    const auto limit = Decibels::decibelsToGain (-0.1f) + 1.0e-4f;

    auto rms = [] (const AudioBuffer<float>& b, int ch, int start, int len)
    {
        double sum = 0.0;
        for (int i = start; i < start + len; ++i)
            sum += (double) b.getSample (ch, i) * b.getSample (ch, i);
        return (float) std::sqrt (sum / jmax (1, len));
    };
    auto sine = [] (double rate, double seconds, double hz, float amp, float right = 1.0f)
    {
        AudioBuffer<float> b (2, (int) (rate * seconds));
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto v = amp * (float) std::sin (MathConstants<double>::twoPi * hz * i / rate);
            b.setSample (0, i, v);
            b.setSample (1, i, v * right);
        }
        return b;
    };

    // ---------------------------------------------------------------- presets x sample rates
    std::cout << "[1] every preset, sample rates 44.1-192 kHz, odd block sizes: finite, never above -0.1 dBFS" << std::endl;
    {
        K808Processor proc;
        enableSidechain (proc, false);
        const auto& list = proc.presets.getPresets();
        for (int i = 0; i < list.size(); ++i)
        {
            if (! list.getReference (i).factory) continue;
            proc.presets.load (i);
            for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            {
                auto input = makeInput (rate, 1.0);
                input.applyGain (2.0f);                            // hot input: the ceiling has to hold
                const auto out = run (proc, input, rate, rate > 100000.0 ? 1023 : 333);
                const auto peak = out.getMagnitude (0, out.getNumSamples());
                check (allFinite (out), list[i].name + " @" + String (rate) + ": NaN/inf");
                check (peak <= limit, list[i].name + " @" + String (rate) + ": peak " + String (Decibels::gainToDecibels (peak), 2) + " dBFS");
                check (peak > 0.01f, list[i].name + " @" + String (rate) + ": silent");
            }
            std::cout << "    ok  " << list[i].name << std::endl;
        }
    }

    // ---------------------------------------------------------------- saturation curves
    std::cout << "[2] saturation curves and soft clipper" << std::endl;
    {
        for (int mode = 0; mode < 3; ++mode)
            for (float x = -50.0f; x <= 50.0f; x += 0.37f)
            {
                const auto y = Engine::saturate (mode, x);
                check (std::isfinite (y) && std::abs (y) <= 1.0001f, "mode " + String (mode) + " at " + String (x) + " = " + String (y));
            }
        check (std::abs (Engine::saturate (0, 1.0e3f) - 1.0f) < 1.0e-2f, "tape does not approach 1");
        check (std::abs (Engine::saturate (1, 0.5f) - 0.5f / 1.5f) < 1.0e-6f, "tube positive half");
        check (std::abs (Engine::saturate (1, -0.5f) - std::tanh (-0.5f)) < 1.0e-6f, "tube negative half");
        check (std::abs (Engine::saturate (2, 1.5f) - 0.5f) < 1.0e-5f, "foldback reflects 1.5 to 0.5");
        for (float t : { 0.5f, 0.8f, 0.95f })
        {
            check (std::abs (Engine::softClip (t * 0.9f, t) - t * 0.9f) < 1.0e-6f, "knee: linear below the threshold");
            check (Engine::softClip (100.0f, t) <= 1.0f && Engine::softClip (100.0f, t) > 0.99f, "knee: approaches the ceiling");
        }
    }

    // ---------------------------------------------------------------- block size independence
    std::cout << "[3] block size independence (offline render = realtime)" << std::endl;
    {
        K808Processor a, b;
        enableSidechain (a, false);
        enableSidechain (b, false);
        a.presets.load (40);
        b.presets.load (40);
        const auto input = makeInput (sr, 2.0);
        const auto outA = run (a, input, sr, 64);
        const auto outB = run (b, input, sr, 512);
        float maxDiff = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 24000; i < input.getNumSamples(); ++i)
                maxDiff = jmax (maxDiff, std::abs (outA.getSample (c, i) - outB.getSample (c, i)));
        std::cout << "    max difference " << maxDiff << std::endl;
        check (maxDiff < 1.0e-3f, "64 vs 512 sample blocks differ by " + String (maxDiff));
    }

    // ---------------------------------------------------------------- bypass and latency
    std::cout << "[4] bypass = input delayed by the reported latency" << std::endl;
    {
        K808Processor p;
        enableSidechain (p, false);
        setParam (p, ParamIDs::bypass, 1.0f);
        const auto input = makeInput (sr, 1.0);
        const auto out = run (p, input, sr, 256);
        const auto lat = p.getLatencySamples();
        float maxDiff = 0.0f;
        for (int i = 4800; i < input.getNumSamples(); ++i)
            maxDiff = jmax (maxDiff, std::abs (out.getSample (0, i) - input.getSample (0, i - lat)));
        std::cout << "    latency " << lat << " samples (" << String (1000.0 * lat / sr, 2) << " ms), max difference " << maxDiff << std::endl;
        check (maxDiff < 1.0e-5f, "bypass is not transparent");
        check (lat < 400, "latency too high");
    }

    // ---------------------------------------------------------------- silence
    std::cout << "[5] silence after the notes stop" << std::endl;
    {
        K808Processor p;
        enableSidechain (p, false);
        p.presets.load (80);
        auto input = makeInput (sr, 3.0);
        input.clear (0, 48000 * 2, 48000);
        input.clear (1, 48000 * 2, 48000);
        const auto out = run (p, input, sr, 512);
        const auto tail = out.getMagnitude (0, (int) (sr * 2.5), (int) (sr * 0.5));
        std::cout << "    residual " << Decibels::gainToDecibels (tail) << " dB" << std::endl;
        check (tail < 1.0e-4f, "not silent after the notes stop");
    }

    // ---------------------------------------------------------------- sub mono
    std::cout << "[6] SUB MONO: the sub band is summed to mono, the upper band keeps its stereo" << std::endl;
    {
        auto run1 = [&] (double hz, bool mono)
        {
            K808Processor p;
            enableSidechain (p, false);
            setParam (p, ParamIDs::driveAmount, 0.0f);
            setParam (p, ParamIDs::midFocus, 0.0f);
            setParam (p, ParamIDs::clipDrive, 0.0f);
            setParam (p, ParamIDs::subMono, mono ? 1.0f : 0.0f);
            const auto out = run (p, sine (sr, 1.0, hz, 0.3f, -1.0f), sr, 512);   // L and R in opposite phase
            return rms (out, 0, 24000, 24000) + rms (out, 1, 24000, 24000) > 1.0e-6f
                       ? 20.0f * std::log10 ((rms (out, 0, 24000, 24000) + 1.0e-9f) / 0.2121f) : -200.0f;
        };
        const auto subMono = run1 (40.0, true), subStereo = run1 (40.0, false), highMono = run1 (1000.0, true);
        std::cout << "    40 Hz anti-phase: " << String (subMono, 1) << " dB mono, " << String (subStereo, 1)
                  << " dB stereo; 1 kHz anti-phase with mono on: " << String (highMono, 1) << " dB" << std::endl;
        check (subMono < -30.0f, "sub is not mono");
        check (subStereo > -3.0f, "sub mono off still removes the side");
        check (highMono > -3.0f, "the upper band lost its stereo");
    }

    // ---------------------------------------------------------------- 28 Hz cut
    std::cout << "[7] 28 Hz CUT: removes infrasound, keeps a C1 808" << std::endl;
    {
        auto level = [&] (double hz, bool cut)
        {
            K808Processor p;
            enableSidechain (p, false);
            setParam (p, ParamIDs::driveAmount, 0.0f);
            setParam (p, ParamIDs::clipDrive, 0.0f);
            setParam (p, ParamIDs::subCut, cut ? 1.0f : 0.0f);
            const auto out = run (p, sine (sr, 2.0, hz, 0.3f), sr, 512);
            return Decibels::gainToDecibels (rms (out, 0, 48000, 48000) / 0.2121f);
        };
        const auto at15 = level (15.0, true), at33 = level (32.7, true), at33off = level (32.7, false);
        std::cout << "    15 Hz " << String (at15, 1) << " dB, 32.7 Hz " << String (at33, 1) << " dB (off: " << String (at33off, 1) << " dB)" << std::endl;
        check (at15 < -20.0f, "15 Hz not cut");
        check (at33 > -6.0f, "C1 cut too much");
    }

    // ---------------------------------------------------------------- ducking only on the sub
    std::cout << "[8] kick ducking works on the sub band only" << std::endl;
    {
        AudioBuffer<float> kick (2, (int) (sr * 2.0));
        kick.clear();
        for (int i = 0; i < kick.getNumSamples(); ++i)
        {
            const auto t = std::fmod (i / sr, 0.5);
            const auto v = t < 0.08 ? (float) (0.9 * std::sin (MathConstants<double>::twoPi * 60.0 * t) * std::exp (-t * 30.0)) : 0.0f;
            kick.setSample (0, i, v);
            kick.setSample (1, i, v);
        }
        auto level = [&] (double hz, bool withKick)
        {
            K808Processor p;
            enableSidechain (p, true);
            setParam (p, ParamIDs::driveAmount, 0.0f);
            setParam (p, ParamIDs::midFocus, 0.0f);
            setParam (p, ParamIDs::clipDrive, 0.0f);
            setParam (p, ParamIDs::duckDepth, 100.0f);
            AudioBuffer<float> silence (2, kick.getNumSamples());
            silence.clear();
            const auto out = run (p, sine (sr, 2.0, hz, 0.3f), sr, 512, withKick ? &kick : &silence);
            // right where the kicks hit (first 30 ms of each 0.5 s), after the first kick
            double sum = 0.0; int count = 0;
            for (int k = 1; k < 4; ++k)
                for (int i = 0; i < (int) (sr * 0.03); ++i)
                {
                    const auto v = out.getSample (0, (int) (k * 0.5 * sr) + i);
                    sum += (double) v * v; ++count;
                }
            return Decibels::gainToDecibels ((float) std::sqrt (sum / count) / 0.2121f);
        };
        const auto sub = level (45.0, true) - level (45.0, false);
        const auto high = level (1500.0, true) - level (1500.0, false);
        std::cout << "    under the kick: sub " << String (sub, 1) << " dB, 1.5 kHz " << String (high, 1) << " dB" << std::endl;
        check (sub < -6.0f, "the sub does not duck");
        check (std::abs (high) < 0.5f, "the upper band ducks too");
    }

    // ---------------------------------------------------------------- phone preview
    std::cout << "[9] PHONE: band-pass 400 Hz - 3.5 kHz" << std::endl;
    {
        auto level = [&] (double hz)
        {
            K808Processor p;
            enableSidechain (p, false);
            setParam (p, ParamIDs::driveAmount, 0.0f);
            setParam (p, ParamIDs::midFocus, 0.0f);
            setParam (p, ParamIDs::clipDrive, 0.0f);
            setParam (p, ParamIDs::phonePreview, 1.0f);
            const auto out = run (p, sine (sr, 1.0, hz, 0.3f), sr, 512);
            return Decibels::gainToDecibels (rms (out, 0, 24000, 24000) / 0.2121f);
        };
        const auto low = level (50.0), mid = level (1200.0), top = level (12000.0);
        std::cout << "    50 Hz " << String (low, 1) << " dB, 1.2 kHz " << String (mid, 1) << " dB, 12 kHz " << String (top, 1) << " dB" << std::endl;
        check (low < -30.0f && top < -15.0f && mid > -3.0f, "phone band-pass wrong");
    }

    // ---------------------------------------------------------------- ceiling and output
    std::cout << "[10] clip drive +18 dB and output +6 dB never pass -0.1 dBFS" << std::endl;
    {
        K808Processor p;
        enableSidechain (p, false);
        setParam (p, ParamIDs::clipDrive, 18.0f);
        setParam (p, ParamIDs::driveAmount, 100.0f);
        setParam (p, ParamIDs::satMode, 2.0f);
        setParam (p, ParamIDs::outputGain, 6.0f);
        auto input = makeInput (sr, 1.0);
        input.applyGain (3.0f);
        const auto out = run (p, input, sr, 256);
        const auto peak = out.getMagnitude (0, out.getNumSamples());
        std::cout << "    peak " << String (Decibels::gainToDecibels (peak), 3) << " dBFS" << std::endl;
        check (peak <= limit, "the hard limit does not hold");
    }

    // ---------------------------------------------------------------- HIT
    std::cout << "[10b] HIT: the drive follows the hit, the body stays a clean sub" << std::endl;
    {
        auto bodyHarmonics = [&] (float hitValue)
        {
            K808Processor p;
            enableSidechain (p, false);
            setParam (p, ParamIDs::driveAmount, 60.0f);
            setParam (p, ParamIDs::hit, hitValue);
            // an 808 with some upper harmonics (like a kit 808): 45 Hz plus its 3rd and 5th harmonic, one note per 0.5 s
            AudioBuffer<float> in (2, (int) (sr * 2.0));
            for (int i = 0; i < in.getNumSamples(); ++i)
            {
                const auto t = std::fmod (i / sr, 0.5);
                const auto ph = MathConstants<double>::twoPi * 45.0 * t;
                const auto v = (float) ((std::sin (ph) + 0.3 * std::sin (3.0 * ph) + 0.15 * std::sin (5.0 * ph)) * 0.6 * std::exp (-t * 3.0));
                in.setSample (0, i, v);
                in.setSample (1, i, v);
            }
            const auto out = run (p, in, sr, 512);
            IIRFilter hp1, hp2;
            hp1.setCoefficients (IIRCoefficients::makeHighPass (sr, 250.0));
            hp2.setCoefficients (IIRCoefficients::makeHighPass (sr, 250.0));
            double all = 0.0, high = 0.0;
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                const auto v = out.getSample (0, i);
                const auto h = hp2.processSingleSampleRaw (hp1.processSingleSampleRaw (v));
                const auto inNote = std::fmod (i / sr, 0.5);
                if (inNote > 0.15 && inNote < 0.45) { all += v * v; high += h * h; }   // the body, not the hit
            }
            return (float) (10.0 * std::log10 ((high + 1.0e-12) / (all + 1.0e-12)));
        };
        const auto h0 = bodyHarmonics (0.0f), h100 = bodyHarmonics (100.0f);
        std::cout << "    harmonics in the body: HIT 0 % " << String (h0, 1) << " dB, HIT 100 % " << String (h100, 1) << " dB" << std::endl;
        check (h100 < h0 - 6.0f, "HIT does not keep the body clean");
    }

    // ---------------------------------------------------------------- state
    std::cout << "[11] state save / restore" << std::endl;
    {
        K808Processor a;
        a.presets.load (30);
        setParam (a, ParamIDs::crossoverFreq, 150.0f);
        MemoryBlock mb;
        a.getStateInformation (mb);
        K808Processor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        for (auto* id : { ParamIDs::crossoverFreq, ParamIDs::driveAmount, ParamIDs::satMode, ParamIDs::clipDrive, ParamIDs::clipKnee })
            check (std::abs (a.apvts.getRawParameterValue (id)->load() - b.apvts.getRawParameterValue (id)->load()) < 1.0e-4f,
                   String ("state lost ") + id);
    }

    // ---------------------------------------------------------------- CPU
    std::cout << "[12] 10 instances, CPU" << std::endl;
    {
        OwnedArray<K808Processor> procs;
        for (int i = 0; i < 10; ++i)
        {
            auto* p = procs.add (new K808Processor());
            enableSidechain (*p, false);
            p->presets.load (i * 9);
        }
        const auto input = makeInput (sr, 5.0);
        const auto t0 = Time::getMillisecondCounterHiRes();
        for (auto* p : procs)
            check (allFinite (run (*p, input, sr, 512)), "instance NaN");
        const auto seconds = (Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        const auto perInstance = seconds / 10.0 / 5.0 * 100.0;
        std::cout << "    one instance uses about " << String (perInstance, 2) << " % of one CPU core (4x oversampling)" << std::endl;
        check (perInstance < 10.0, "too much CPU");
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : String (failures) + " FAILURES") << std::endl;
    return failures == 0 ? 0 : 1;
}
