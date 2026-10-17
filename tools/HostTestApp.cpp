/*
    TrapWin BEND DEMON - dev tool: end-to-end hosting test.

    1. Creates a BendDemonProcessor (the real plugin processor).
    2. Loads a VST3 instrument (the MPE Probe) through the real HostedPluginManager.
    3. Plays 60 -> 64 legato with GLIDE on, renders audio, and measures the output pitch:
         - silence             => MPE configuration (RPN 6) not received by the synth
         - ~261.6 Hz at start  => note-on on a member channel works
         - ~329.6 Hz at end    => per-note pitch bend reached the hosted synth (glide)
    4. 2.0: plays a drawn pattern (line 60 -> 67) on the PREVIEW clock through the hosted synth
       and measures both pitches (sequencer -> MPE -> hosted plugin end to end).
    5. Saves BEND DEMON's state, creates a second processor, restores it and checks that the
       hosted plugin, its state and the pattern come back.

    Usage: BendDemonHostTest <path-to-plugin.vst3>      exit code 0 = pass
*/
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace
{
    double estimateHz (const juce::AudioBuffer<float>& b, int start, int len, double sr)
    {
        const float* d = b.getReadPointer (0);
        int crossings = 0, first = -1, last = -1;
        for (int i = start + 1; i < start + len; ++i)
            if (d[i - 1] < 0.0f && d[i] >= 0.0f)
            {
                if (first < 0) first = i;
                last = i;
                ++crossings;
            }
        return crossings > 1 ? (crossings - 1) * sr / (double) (last - first) : 0.0;
    }

    float rms (const juce::AudioBuffer<float>& b, int start, int len) { return b.getRMSLevel (0, start, len); }
}

class HostTestApp final : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "BendDemonHostTest"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise (const juce::String& args) override
    {
        pluginFile = juce::File (args.unquoted().trim());
        if (! pluginFile.exists()) { fail ("plugin not found: " + pluginFile.getFullPathName()); return; }

        a = std::make_unique<BendDemonProcessor>();
        a->setRateAndBufferSizeDetails (sr, block);
        a->prepareToPlay (sr, block);

        // Describe the plugin file (VST3 format, in-process scan of a single file)
        juce::OwnedArray<juce::PluginDescription> found;
        for (auto* fmt : a->getHost().getFormatManager().getFormats())
            if (fmt->fileMightContainThisPluginType (pluginFile.getFullPathName()))
                fmt->findAllTypesForFile (found, pluginFile.getFullPathName());

        if (found.isEmpty()) { fail ("no plugin types found in file"); return; }
        std::printf ("found: %s (%s), instrument=%d\n", found[0]->name.toRawUTF8(), found[0]->pluginFormatName.toRawUTF8(), (int) found[0]->isInstrument);

        a->getHost().loadPlugin (*found[0], {}, [this] (juce::String err)
        {
            if (err.isNotEmpty()) { fail ("load failed: " + err); return; }
            std::printf ("loaded into BEND DEMON: %s\n", a->getHost().getPluginName().toRawUTF8());
            runAudioTest();
        });

        startTimer (20000);   // watchdog
    }

    void shutdown() override { a.reset(); b.reset(); }
    void timerCallback() override { fail ("timeout"); }

private:
    void runAudioTest()
    {
        auto& s = a->getAPVTS();
        auto set = [&s] (const char* id, float v) { if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set (bd::ids::glideOn, 1); set (bd::ids::glideTime, 60);
        set (bd::ids::vibratoOn, 0); set (bd::ids::diveOn, 0);

        const int total = (int) (sr * 1.0);
        juce::AudioBuffer<float> out (2, total);
        out.clear();

        for (int pos = 0; pos < total; pos += block)
        {
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            auto at = [&] (double sec, const juce::MidiMessage& m)
            {
                const int t = (int) (sec * sr);
                if (t >= pos && t < pos + block) midi.addEvent (m, t - pos);
            };
            at (0.05, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
            at (0.40, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100));
            at (0.41, juce::MidiMessage::noteOff (1, 60));

            a->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, buf, ch, 0, juce::jmin (block, total - pos));
        }

        const double f1 = estimateHz (out, (int) (0.15 * sr), (int) (0.2 * sr), sr);
        const double f2 = estimateHz (out, (int) (0.65 * sr), (int) (0.3 * sr), sr);
        const float  r1 = rms (out, (int) (0.15 * sr), (int) (0.2 * sr));
        std::printf ("rms=%.4f  pitch before glide=%.2f Hz (expect 261.63)  after glide=%.2f Hz (expect 329.63)\n", r1, f1, f2);

        bool ok = r1 > 0.01f && std::abs (f1 - 261.63) < 3.0 && std::abs (f2 - 329.63) < 3.0;
        if (! ok) { fail ("audio check failed"); return; }

        // ---- 2.0: the line sequencer through the hosted synth (preview clock, 140 bpm)
        {
            bd::Pattern p;
            p.bars = 2;
            bd::PatternLine l;
            l.start = 0.0; l.length = 6.0; l.smooth = false;
            l.pitch = { { 0.0, 60.0f }, { 0.5, 60.0f }, { 1.0, 67.0f } };
            p.add (l);
            // release the held note of the glide test and let it ring out
            for (int i = 0; i < (int) (0.3 * sr) / block; ++i)
            {
                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer m;
                if (i == 0) m.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
                a->processBlock (buf, m);
            }

            a->getPatternStore().replaceAll (p, true);
            a->setDrawMode (true);
            a->setPreviewPlaying (true);

            const int len = (int) (sr * 1.6);
            juce::AudioBuffer<float> seqOut (2, len);
            seqOut.clear();
            for (int pos = 0; pos < len; pos += block)
            {
                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer none;
                a->processBlock (buf, none);
                for (int ch = 0; ch < 2; ++ch)
                    seqOut.copyFrom (ch, pos, buf, ch, 0, juce::jmin (block, len - pos));
            }
            a->setPreviewPlaying (false);

            // 140 bpm: beat 0.5 = 0.214 s, beat 1.0 = 0.429 s
            const double s1 = estimateHz (seqOut, (int) (0.03 * sr), (int) (0.15 * sr), sr);
            const double s2 = estimateHz (seqOut, (int) (0.70 * sr), (int) (0.6 * sr), sr);
            std::printf ("pattern: pitch at start=%.2f Hz (expect 261.63)  after the glide=%.2f Hz (expect 392.00)\n", s1, s2);
            if (std::abs (s1 - 261.63) > 3.0 || std::abs (s2 - 392.0) > 4.0) { fail ("pattern playback check failed"); return; }
        }

        // State round trip
        juce::MemoryBlock state;
        a->getStateInformation (state);
        b = std::make_unique<BendDemonProcessor>();
        b->setRateAndBufferSizeDetails (sr, block);
        b->prepareToPlay (sr, block);
        b->setStateInformation (state.getData(), (int) state.getSize());

        juce::Timer::callAfterDelay (1500, [this]
        {
            const bool restored = b->getHost().hasPlugin() && b->getHost().getPluginName() == a->getHost().getPluginName();
            std::printf ("state restore: hosted plugin restored = %d (%s)\n", (int) restored, b->getHost().getPluginName().toRawUTF8());
            if (! restored) { fail ("state restore failed"); return; }
            const bool patternBack = b->getPatternStore().get() == a->getPatternStore().get();
            std::printf ("state restore: pattern restored = %d (%d lines)\n", (int) patternBack, (int) b->getPatternStore().get().lines.size());
            if (! patternBack) { fail ("pattern state restore failed"); return; }
            std::printf ("HOST TEST PASSED\n");
            stopTimer();
            setApplicationReturnValue (0);
            quit();
        });
    }

    void fail (const juce::String& why)
    {
        std::printf ("HOST TEST FAILED: %s\n", why.toRawUTF8());
        stopTimer();
        setApplicationReturnValue (1);
        quit();
    }

    std::unique_ptr<BendDemonProcessor> a, b;
    juce::File pluginFile;
    const double sr = 48000.0;
    const int block = 256;
};

START_JUCE_APPLICATION (HostTestApp)
