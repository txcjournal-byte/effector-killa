/*
    TrapWin BEND DEMON - dev tool: renders the editor to PNG files while a scripted melody plays
    through the engine (no audio device, no hosted synth). Used to check the UI headlessly (Xvfb)
    and in CI.  Usage:  BendDemonSnapshot <output-dir>
*/
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    /** Fake host transport: 140 bpm, always playing. */
    struct FakePlayHead final : public juce::AudioPlayHead
    {
        int64_t samples = 0;
        double sampleRate = 48000.0, bpm = 140.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setIsPlaying (true);
            p.setBpm (bpm);
            p.setTimeInSamples (samples);
            p.setPpqPosition ((double) samples / sampleRate * bpm / 60.0);
            p.setTimeSignature (TimeSignature { 4, 4 });
            return p;
        }
    };

    struct NoteEv { double beat; int note; double lenBeats; int vel; };
}

class SnapshotApp final : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return "BendDemonSnapshot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    void initialise (const juce::String& args) override
    {
        outDir = juce::File::getCurrentWorkingDirectory().getChildFile (args.unquoted().trim().isEmpty() ? "snapshots" : args.unquoted().trim());
        outDir.createDirectory();

        processor = std::make_unique<BendDemonProcessor>();
        processor->setPlayHead (&playHead);
        processor->setRateAndBufferSizeDetails (48000.0, 512);
        processor->prepareToPlay (48000.0, 512);

        auto& s = processor->getAPVTS();
        auto set = [&s] (const char* id, float v) { if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set (bd::ids::glideOn, 1); set (bd::ids::diveOn, 1); set (bd::ids::vibratoOn, 1);
        set (bd::ids::diveTime, 300); set (bd::ids::vibDelay, 150);

        // A trap-style melody: legato runs (glide), a held note (vibrato), short stabs with dives, a chord.
        const NoteEv m[] = {
            { 0.0, 60, 0.5, 110 }, { 0.5, 63, 0.5, 100 }, { 1.0, 67, 1.0, 120 }, { 2.0, 65, 0.5, 90 },
            { 2.5, 63, 1.5, 100 }, { 4.5, 48, 0.25, 127 }, { 5.0, 48, 0.25, 110 }, { 5.75, 51, 0.5, 120 },
            { 6.5, 70, 0.5, 100 }, { 7.0, 72, 1.0, 115 }, { 8.0, 55, 2.0, 90 }, { 8.0, 58, 2.0, 90 },
            { 8.0, 62, 2.0, 90 }, { 10.25, 67, 0.75, 110 }, { 11.0, 68, 0.5, 100 }, { 11.5, 70, 2.0, 120 },
            { 13.75, 43, 0.25, 127 }, { 14.25, 43, 0.5, 120 } };
        for (auto& e : m) melody.push_back (e);

        editor.reset (processor->createEditor());
        window = std::make_unique<juce::DocumentWindow> ("BEND DEMON snapshot", juce::Colours::black, 0);
        window->setUsingNativeTitleBar (false);
        window->setTitleBarHeight (0);
        window->setContentNonOwned (editor.get(), true);
        window->setVisible (true);

        startTime = juce::Time::getMillisecondCounterHiRes();
        startTimer (5);
    }

    void shutdown() override
    {
        stopTimer();
        window.reset();
        editor.reset();
        processor->releaseResources();
        processor.reset();
    }

    void systemRequestedQuit() override { quit(); }

private:
    void timerCallback() override
    {
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - startTime) * 0.001;
        const double sr = 48000.0;
        const int block = 512;

        // Run the engine in "real time".
        while ((double) playHead.samples < elapsed * sr)
        {
            juce::AudioBuffer<float> audio (2, block);
            juce::MidiBuffer midi;
            const double b0 = (double) playHead.samples / sr * playHead.bpm / 60.0;
            const double b1 = (double) (playHead.samples + block) / sr * playHead.bpm / 60.0;
            const double loop = 16.0;

            for (auto& e : melody)
            {
                for (double base = std::floor (b0 / loop) * loop; base <= b1; base += loop)
                {
                    const double on = base + e.beat, off = base + e.beat + e.lenBeats;
                    auto pos = [&] (double beat) { return juce::jlimit (0, block - 1, (int) ((beat - b0) * 60.0 / playHead.bpm * sr)); };
                    if (on >= b0 && on < b1)   midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel), pos (on));
                    if (off >= b0 && off < b1) midi.addEvent (juce::MidiMessage::noteOff (1, e.note), pos (off));
                }
            }

            processor->processBlock (audio, midi);
            playHead.samples += block;
        }

        auto shoot = [this] (const juce::String& name)
        {
            auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto f = outDir.getChildFile (name);
            f.deleteFile();
            juce::FileOutputStream os (f);
            png.writeImageToStream (img, os);
            std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        };

        if (stage == 0 && elapsed > 5.6)  { shoot ("bend-demon-flat.png"); ++stage; }
        if (stage == 1 && elapsed > 5.7)
        {
            processor->getUiState().setProperty ("dimension", true, nullptr);
            // toggle DIMENSION through the button so the editor animates it
            if (auto* b = findDimensionButton (editor.get())) b->triggerClick();
            ++stage;
        }
        if (stage == 2 && elapsed > 8.2)  { shoot ("bend-demon-dimension.png"); ++stage; }
        if (stage == 3 && elapsed > 8.3)
        {
            editor->setSize (960, 600);
            ++stage;
        }
        if (stage == 4 && elapsed > 9.0)  { shoot ("bend-demon-960x600.png"); ++stage; quit(); }
    }

    static juce::Button* findDimensionButton (juce::Component* c)
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (c))
            if (b->getButtonText() == "DIMENSION") return b;
        for (auto* child : c->getChildren())
            if (auto* f = findDimensionButton (child)) return f;
        return nullptr;
    }

    std::unique_ptr<BendDemonProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    std::unique_ptr<juce::DocumentWindow> window;
    FakePlayHead playHead;
    std::vector<NoteEv> melody;
    juce::File outDir;
    double startTime = 0.0;
    int stage = 0;
};

START_JUCE_APPLICATION (SnapshotApp)
