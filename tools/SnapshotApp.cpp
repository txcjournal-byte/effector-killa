/*
    TrapWin BEND DEMON - dev tool: renders the editor to PNG files (headless under Xvfb / CI).

    2.0: loads the factory pattern "Trap Glide Lead" and renders
        bend-demon-draw-1040.png        DRAW mode, flat, host playing (playhead + active notes)
        bend-demon-select-1040.png      a selection + the NOTE inspector + the expression lane
        bend-demon-shapes-1040.png      the SHAPES inspector tab
        bend-demon-dimension-1040.png   DIMENSION (3D view)
        bend-demon-live-1040.png        LIVE mode while a melody plays through the gestures
        bend-demon-banner-1040.png      the "no MPE" notice
        bend-demon-draw-800x500.png     the small size
        bend-demon-sound-800x500.png    SOUND tab at the small size
    Usage:  BendDemonSnapshot <output-dir>
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
    const juce::String getApplicationVersion() override { return "2.0"; }

    void initialise (const juce::String& args) override
    {
        outDir = juce::File::getCurrentWorkingDirectory().getChildFile (args.unquoted().trim().isEmpty() ? "snapshots" : args.unquoted().trim());
        outDir.createDirectory();

        processor = std::make_unique<BendDemonProcessor>();
        processor->setPlayHead (&playHead);
        processor->setRateAndBufferSizeDetails (48000.0, 512);
        processor->prepareToPlay (48000.0, 512);
        processor->getPresets().loadFactory (0);    // Trap Glide Lead (pattern + sound)

        auto& s = processor->getAPVTS();
        auto set = [&s] (const char* id, float v) { if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set (bd::ids::scaleLockOn, 1);

        const NoteEv m[] = {
            { 0.0, 60, 0.5, 110 }, { 0.5, 63, 0.5, 100 }, { 1.0, 67, 1.0, 120 }, { 2.0, 65, 0.5, 90 },
            { 2.5, 63, 1.5, 100 }, { 4.5, 48, 0.25, 127 }, { 5.0, 48, 0.25, 110 }, { 5.75, 51, 0.5, 120 },
            { 6.5, 70, 0.5, 100 }, { 7.0, 72, 1.0, 115 }, { 8.0, 55, 2.0, 90 }, { 8.0, 58, 2.0, 90 },
            { 8.0, 62, 2.0, 90 }, { 10.25, 67, 0.75, 110 }, { 11.0, 68, 0.5, 100 }, { 11.5, 70, 2.0, 120 },
            { 13.75, 43, 0.25, 127 }, { 14.25, 43, 0.5, 120 } };
        for (auto& e : m) melody.push_back (e);

        editor.reset (dynamic_cast<BendDemonEditor*> (processor->createEditor()));
        window = std::make_unique<juce::DocumentWindow> ("BEND DEMON snapshot", juce::Colours::black, 0);
        window->setUsingNativeTitleBar (false);
        window->setTitleBarHeight (0);
        window->setContentNonOwned (editor.get(), true);
        window->setVisible (true);
        editor->setSize (1040, 650);

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
    void runAudio (double elapsed)
    {
        const double sr = 48000.0;
        const int block = 512;
        while ((double) playHead.samples < elapsed * sr)
        {
            juce::AudioBuffer<float> audio (2, block);
            juce::MidiBuffer midi;
            if (playMelody)
            {
                const double b0 = (double) playHead.samples / sr * playHead.bpm / 60.0;
                const double b1 = (double) (playHead.samples + block) / sr * playHead.bpm / 60.0;
                const double loop = 16.0;
                for (auto& e : melody)
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
    }

    void shoot (const juce::String& name)
    {
        auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    }

    void selectSome (int from, int count)
    {
        auto& store = processor->getPatternStore();
        std::vector<uint32_t> ids;
        for (int i = from; i < from + count && i < (int) store.get().lines.size(); ++i)
            ids.push_back (store.get().lines[(size_t) i].id);
        store.setSelection (ids);
    }

    void timerCallback() override
    {
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - startTime) * 0.001;
        runAudio (elapsed);

        struct Step { double at; std::function<void()> fn; };
        const Step steps[] = {
            { 3.4,  [this] { shoot ("bend-demon-draw-1040.png"); } },
            { 3.5,  [this] { selectSome (2, 2); editor->getController().setTool (bd::LineEditController::Tool::select); } },
            { 4.2,  [this] { shoot ("bend-demon-select-1040.png"); } },
            { 4.3,  [this] { editor->getInspector().setTab (bd::Inspector::Tab::shapes); editor->getInspector().showShape (3); } },
            { 4.8,  [this] { shoot ("bend-demon-shapes-1040.png"); } },
            { 4.9,  [this] { editor->getInspector().setTab (bd::Inspector::Tab::note); processor->getPatternStore().clearSelection();
                             if (auto* b = findButton (editor.get(), "3D")) b->triggerClick(); } },
            { 7.2,  [this] { shoot ("bend-demon-dimension-1040.png"); } },
            { 7.3,  [this] { if (auto* b = findButton (editor.get(), "3D")) b->triggerClick();
                             playMelody = true; processor->setDrawMode (false); } },
            { 11.0, [this] { shoot ("bend-demon-live-1040.png"); } },
            { 11.1, [this] { playMelody = false; processor->setDrawMode (true); processor->showNonMpeNoticeForTest(); } },
            { 11.6, [this] { shoot ("bend-demon-banner-1040.png"); } },
            { 11.7, [this] { processor->dismissNonMpeNotice(); editor->setSize (800, 500); } },
            { 12.5, [this] { shoot ("bend-demon-draw-800x500.png"); } },
            { 12.6, [this] { editor->getInspector().setTab (bd::Inspector::Tab::sound); } },
            { 13.1, [this] { shoot ("bend-demon-sound-800x500.png"); quit(); } },
        };

        while (stage < (int) std::size (steps) && elapsed > steps[stage].at)
            steps[stage++].fn();
    }

    static juce::Button* findButton (juce::Component* c, const juce::String& text)
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (c))
            if (b->getButtonText() == text) return b;
        for (auto* child : c->getChildren())
            if (auto* f = findButton (child, text)) return f;
        return nullptr;
    }

    std::unique_ptr<BendDemonProcessor> processor;
    std::unique_ptr<BendDemonEditor> editor;
    std::unique_ptr<juce::DocumentWindow> window;
    FakePlayHead playHead;
    std::vector<NoteEv> melody;
    juce::File outDir;
    double startTime = 0.0;
    int stage = 0;
    bool playMelody = false;
};

START_JUCE_APPLICATION (SnapshotApp)
