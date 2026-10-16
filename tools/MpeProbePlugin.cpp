/*
    TrapWin BEND DEMON - dev tool: "MPE Probe" test instrument (VST3).

    A minimal MPE synth used to verify hosting end-to-end: it starts with NO MPE zone, so it only
    makes sound if BEND DEMON sends a valid MPE Configuration Message. Each note is a sine at
    MPEInstrument's per-note frequency (incl. per-note pitch bend), amplitude follows pressure.
    Not shipped.
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

class MpeProbeProcessor final : public juce::AudioProcessor
{
public:
    MpeProbeProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true))
    {
        instrument.setZoneLayout ({});   // no zone until an MCM arrives
    }

    void prepareToPlay (double sr, int) override { sampleRate = sr; std::fill (std::begin (phase), std::end (phase), 0.0); }
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        audio.clear();
        int pos = 0;
        for (const auto meta : midi)
        {
            render (audio, pos, meta.samplePosition - pos);
            pos = meta.samplePosition;
            if (std::getenv ("MPE_PROBE_TRACE") != nullptr)   // debug: print every MIDI event the probe receives
                std::fprintf (stderr, "[probe] %s\n", meta.getMessage().getDescription().toRawUTF8());
            instrument.processNextMidiEvent (meta.getMessage());
        }
        render (audio, pos, audio.getNumSamples() - pos);
    }

    void render (juce::AudioBuffer<float>& audio, int start, int num)
    {
        if (num <= 0) return;
        for (int i = 0; i < instrument.getNumPlayingNotes(); ++i)
        {
            const auto note = instrument.getNote (i);
            if (note.keyState == juce::MPENote::off) continue;
            const double f = note.getFrequencyInHertz();
            const float amp = 0.2f * (0.25f + 0.75f * note.pressure.asUnsignedFloat());
            auto& ph = phase[note.midiChannel & 15];
            for (int s = 0; s < num; ++s)
            {
                const float v = amp * (float) std::sin (ph);
                ph += juce::MathConstants<double>::twoPi * f / sampleRate;
                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                    audio.addSample (ch, start + s, v);
            }
            ph = std::fmod (ph, juce::MathConstants<double>::twoPi);
        }
    }

    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MPE Probe"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& mb) override { mb.append ("probe-state-42", 14); }
    void setStateInformation (const void* d, int n) override { lastState = juce::String::fromUTF8 ((const char*) d, n); }

    juce::String lastState;

private:
    juce::MPEInstrument instrument;
    double sampleRate = 48000.0;
    double phase[16] {};
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MpeProbeProcessor(); }
