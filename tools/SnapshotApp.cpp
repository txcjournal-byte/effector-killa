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
        bend-demon-min-640x400.png      the minimum size
    then drives the canvas with synthetic mouse / key events (UI CHECK lines, exit code 1 on failure).
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
            { 13.1, [this] { shoot ("bend-demon-sound-800x500.png"); editor->setSize (640, 400);
                             editor->getInspector().setTab (bd::Inspector::Tab::note); selectSome (0, 1); } },
            { 13.7, [this] { shoot ("bend-demon-min-640x400.png"); editor->setSize (1040, 650); } },
            { 13.9, [this] { runUiChecks(); quit(); } },
        };

        while (stage < (int) std::size (steps) && elapsed > steps[stage].at)
            steps[stage++].fn();
    }

    //==============================================================================
    // UI smoke test: drives the canvas with synthetic mouse / key events like a user would.
    juce::MouseEvent mouseAt (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos, juce::ModifierKeys mods, bool dragged)
    {
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, downPos, now, 1, dragged);
    }

    void drag (juce::Component& c, juce::Point<float> a, juce::Point<float> b, juce::ModifierKeys mods = {})
    {
        const auto down = mods.withFlags (juce::ModifierKeys::leftButtonModifier);
        c.mouseDown (mouseAt (c, a, a, down, false));
        for (int i = 1; i <= 12; ++i)
            c.mouseDrag (mouseAt (c, a + (b - a) * ((float) i / 12.0f), a, down, true));
        c.mouseUp (mouseAt (c, b, a, mods, a != b));
    }

    void check (bool ok, const char* what)
    {
        std::printf ("UI CHECK %s: %s\n", ok ? "ok  " : "FAIL", what);
        if (! ok) ++uiFailures;
    }

    void runUiChecks()
    {
        auto& vf = editor->getVoiceField();
        auto& ctl = editor->getController();
        auto& store = processor->getPatternStore();
        store.clearSelection();
        vf.setPatternView ({ 0.0, 16.0, 48.0f, 84.0f });
        const auto f = vf.getFieldBounds();
        auto at = [&] (double beat, float pitch) { return juce::Point<float> (vf.beatToX (beat), vf.pitchToY (pitch)); };
        const auto n0 = store.get().lines.size();

        ctl.setTool (bd::LineEditController::Tool::pencil);
        drag (vf, at (0.5, 50.0f), at (2.0, 54.3f));
        check (store.get().lines.size() == n0 + 1, "pencil drag draws a line");
        const auto* drawn = store.get().lines.empty() ? nullptr : &store.get().lines.back();
        for (auto& l : store.get().lines) if (std::abs (l.start - 0.5) < 1.0e-6) drawn = &l;
        check (drawn != nullptr && std::abs (drawn->curvePitchAt (drawn->length) - 54.3f) < 0.5f, "the drawn curve ends where the mouse ended");

        drag (vf, at (3.0, 49.0f), at (3.0, 49.0f));
        check (store.get().lines.size() == n0 + 2, "pencil click places a note");

        store.undo();
        check (store.get().lines.size() == n0 + 1, "undo removes it");
        store.redo();
        check (store.get().lines.size() == n0 + 2, "redo brings it back");

        ctl.setTool (bd::LineEditController::Tool::line);
        drag (vf, at (5.0, 50.0f), at (7.0, 55.0f));
        check (store.get().lines.size() == n0 + 3, "line tool draws a glide");
        bool glide = false;
        for (auto& l : store.get().lines)
            if (std::abs (l.start - 5.0) < 1.0e-6 && ! l.smooth && std::abs (l.curvePitchAt (l.length) - 55.0f) < 0.01f) glide = true;
        check (glide, "glide goes from A to B in straight segments");
        ctl.finishLine();

        ctl.setTool (bd::LineEditController::Tool::select);
        const auto before = store.get();
        drag (vf, at (3.125, 49.0f), at (4.125, 51.0f));
        bool moved = false;
        for (auto& l : store.get().lines)
            if (std::abs (l.start - 4.0) < 1.0e-6 && std::abs (l.startPitch() - 51.0f) < 0.01f) moved = true;
        check (moved, "select tool drags a note in time (grid) and pitch (semitones)");

        drag (vf, juce::Point<float> (f.getX() + 2.0f, f.getY() + 2.0f), juce::Point<float> (f.getRight() - 2.0f, f.getBottom() - 2.0f));
        check (store.getSelection().size() == store.get().lines.size(), "box select selects everything");

        const auto countBefore = store.get().lines.size();
        vf.grabKeyboardFocus();
        check (! vf.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)), "space bar is NOT consumed (goes to the host)");
        vf.keyPressed (juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 'd'));
        check (store.get().lines.size() == countBefore * 2, "Ctrl+D duplicates the selection");
        vf.keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 'z'));
        check (store.get().lines.size() == countBefore, "Ctrl+Z undoes it");

        ctl.setTool (bd::LineEditController::Tool::eraser);
        store.clearSelection();
        drag (vf, at (4.125, 51.0f), at (4.125, 51.0f));
        check (store.get().lines.size() == countBefore - 1, "eraser deletes the note under the mouse");

        std::printf (uiFailures == 0 ? "UI CHECKS PASSED\n" : "UI CHECKS FAILED (%d)\n", uiFailures);
        setApplicationReturnValue (uiFailures == 0 ? 0 : 1);
        juce::ignoreUnused (before);
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
    int uiFailures = 0;
};

START_JUCE_APPLICATION (SnapshotApp)
