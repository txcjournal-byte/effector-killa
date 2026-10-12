// Plugin lifecycle: load / unload must be fast and must never hang (FL Studio froze on exit with the
// editor open). Covers processor create / destroy, prepare / release re-entrancy, audio running while the
// message thread reconfigures, and pending async messages after destruction.
//
// The editor part (EK_LIFECYCLE_GUI) is compiled into the separate EffectorKillaLifecycle tool, which
// links the UI and runs headless under xvfb:  xvfb-run -a ./EffectorKillaLifecycle
#include "PluginProcessor.h"
#include <thread>
#include <atomic>

#if EK_LIFECYCLE_GUI
 #include "PluginEditor.h"
 #include "ui/SharedResources.h"
 #if JUCE_LINUX
  #include <dlfcn.h>
 #endif
#endif

namespace
{
double msSince (double t0) { return juce::Time::getMillisecondCounterHiRes() - t0; }

void pumpMessages (int ms)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
   #else
    juce::ignoreUnused (ms);
   #endif
}

void fillNoise (juce::AudioBuffer<float>& b, juce::Random& r)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, (r.nextFloat() * 2.0f - 1.0f) * 0.3f);
}

bool finite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

#if EK_LIFECYCLE_GUI
// JUCE installs its tolerant X error handler only in JUCEApplication apps; a console test under xvfb
// (no window manager) would otherwise be killed by Xlib's default handler on a harmless BadAtom.
void installQuietXErrorHandler()
{
   #if JUCE_LINUX
    juce::Desktop::getInstance().getDisplays();   // makes JUCE load libX11
    if (auto* lib = dlopen ("libX11.so.6", RTLD_NOW | RTLD_NOLOAD))
    {
        using Handler = int (*) (void*, void*);
        using SetHandler = Handler (*) (Handler);
        if (auto set = (SetHandler) dlsym (lib, "XSetErrorHandler"))
            set ([] (void*, void*) { return 0; });
    }
   #endif
}
#endif

#if JUCE_DEBUG
constexpr double kTimeScale = 6.0;    // Debug builds are much slower: only catch gross regressions
#else
constexpr double kTimeScale = 1.0;
#endif
} // namespace

class LifecycleTest : public juce::UnitTest
{
public:
    LifecycleTest() : juce::UnitTest ("Plugin lifecycle (load / unload)", "lifecycle") {}

    void runTest() override
    {
        processorCreateDestroy();
        prepareReleaseReentrancy();
        audioThreadWhileReconfiguring();
        pendingMessagesAfterDestruction();
       #if EK_LIFECYCLE_GUI
        installQuietXErrorHandler();
        editorCreateDestroy();
        editorWithOpenOverlays();
       #endif
    }

private:
    void processorCreateDestroy()
    {
        beginTest ("processor create / destroy 50x (timing)");
        double first = 0.0, total = 0.0, worst = 0.0, worstDtor = 0.0;
        for (int i = 0; i < 50; ++i)
        {
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            const double ctor = msSince (t0);
            if (i == 0) first = ctor;
            else { total += ctor; worst = juce::jmax (worst, ctor); }

            if (i % 10 == 0)   // some instances also play before they die
            {
                p->setPlayConfigDetails (2, 2, 48000.0, 256);
                p->prepareToPlay (48000.0, 256);
                juce::AudioBuffer<float> b (2, 256);
                juce::MidiBuffer m;
                juce::Random r (i);
                for (int k = 0; k < 8; ++k) { fillNoise (b, r); p->processBlock (b, m); }
                expect (finite (b));
                p->releaseResources();
            }
            const double t1 = juce::Time::getMillisecondCounterHiRes();
            p.reset();
            worstDtor = juce::jmax (worstDtor, msSince (t1));
        }
        const double avg = total / 49.0;
        logMessage (juce::String::formatted ("  processor ctor: first %.2f ms, avg %.2f ms, worst %.2f ms; dtor worst %.2f ms",
                                             first, avg, worst, worstDtor));
        expect (first < 120.0 * kTimeScale, "first processor ctor took " + juce::String (first, 1) + " ms");
        expect (avg < 30.0 * kTimeScale, "processor ctor avg " + juce::String (avg, 1) + " ms (target < 30 ms)");
        expect (worstDtor < 30.0 * kTimeScale, "processor dtor worst " + juce::String (worstDtor, 1) + " ms");
        pumpMessages (60);   // let deferred timer work of dead instances (if any) try to run
    }

    void prepareReleaseReentrancy()
    {
        beginTest ("prepareToPlay / releaseResources re-entrancy");
        EffectorKillaAudioProcessor p;
        juce::MidiBuffer m;
        juce::Random r (7);

        p.releaseResources();                         // release before any prepare
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int bs : { 1, 64, 512, 4096 })
            {
                p.setPlayConfigDetails (2, 2, sr, bs);
                p.prepareToPlay (sr, bs);
                p.prepareToPlay (sr, bs);             // twice without release
                juce::AudioBuffer<float> b (2, bs);
                fillNoise (b, r);
                p.processBlock (b, m);
                expect (finite (b));
                p.releaseResources();
                p.releaseResources();                 // twice
            }
        // processBlock after releaseResources (some hosts do this briefly) must not crash
        juce::AudioBuffer<float> b (2, 128);
        fillNoise (b, r);
        p.processBlock (b, m);
        expect (finite (b));
        const int latency = p.getLatencySamples();
        p.prepareToPlay (192000.0, 4096);
        p.prepareToPlay (44100.0, 32);
        expect (p.getLatencySamples() > 0 && latency > 0);
    }

    void audioThreadWhileReconfiguring()
    {
        beginTest ("audio thread running while the message thread reconfigures and destroys");
        for (int round = 0; round < 4; ++round)
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            p->setPlayConfigDetails (2, 2, 48000.0, 256);
            p->prepareToPlay (48000.0, 256);

            std::atomic<bool> run { true }, sawNaN { false };
            std::atomic<int> blocks { 0 };
            auto* proc = p.get();
            // Plugin wrappers (VST3 / AU / standalone) call processBlock under the callback lock.
            std::thread audio ([&]
            {
                juce::AudioBuffer<float> b (2, 256);
                juce::MidiBuffer m;
                juce::Random r (round + 1);
                while (run.load())
                {
                    fillNoise (b, r);
                    {
                        const juce::ScopedLock sl (proc->getCallbackLock());
                        proc->processBlock (b, m);
                    }
                    if (! finite (b)) sawNaN = true;
                    ++blocks;
                    std::this_thread::yield();
                }
            });

            juce::MemoryBlock state;
            p->getStateInformation (state);
            for (int k = 0; k < 12; ++k)
            {
                p->loadFactory (k % 12, k % 8);
                if (k % 3 == 0) { p->killLevel = ek::KillLevel::R; p->kill(); }
                if (k % 4 == 1) p->prepareToPlay (k % 2 ? 44100.0 : 48000.0, 256);   // host re-prepares
                if (k % 4 == 2) p->releaseResources();
                if (k == 6) p->setStateInformation (state.getData(), (int) state.getSize());
                pumpMessages (5);
            }
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            run = false;
            audio.join();
            const double stopMs = msSince (t0);
            const double t1 = juce::Time::getMillisecondCounterHiRes();
            p.reset();   // destroyed right after the audio thread stops
            const double dtorMs = msSince (t1);
            expect (! sawNaN.load());
            expect (blocks.load() > 0);
            expect (stopMs < 100.0 * kTimeScale && dtorMs < 50.0 * kTimeScale,
                    "stop " + juce::String (stopMs, 1) + " ms, dtor " + juce::String (dtorMs, 1) + " ms");
        }
    }

    void pendingMessagesAfterDestruction()
    {
        beginTest ("pending change messages / timers after processor destruction");
        for (int i = 0; i < 10; ++i)
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            p->loadFactory (i % 12, 0);   // posts a change message (async)
            p->stepPreset (1);
            p.reset();                    // destroyed before the message is delivered
        }
        pumpMessages (100);
        expect (true);
    }

   #if EK_LIFECYCLE_GUI
    void editorCreateDestroy()
    {
        beginTest ("processor + editor create / destroy 20x (timing)");
        double firstOpen = 0.0, totalOpen = 0.0, worstOpen = 0.0, worstClose = 0.0, totalCtor = 0.0;
        for (int i = 0; i < 20; ++i)
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
            expect (ed != nullptr);
            if (i > 0) totalCtor += msSince (t0);
            ed->addToDesktop (0);
            ed->setVisible (true);
            {
                // first paint is part of "opening"
                juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
                juce::Graphics g (img);
                ed->paintEntireComponent (g, true);
            }
            const double open = msSince (t0);
            if (i == 0) firstOpen = open;
            else { totalOpen += open; worstOpen = juce::jmax (worstOpen, open); }
            expectEquals (ek::ui::UiResources::getNumLiveInstances(), 1);
            pumpMessages (40);   // timers tick, async repaints

            const double t1 = juce::Time::getMillisecondCounterHiRes();
            p->editorBeingDeleted (ed.get());
            ed.reset();
            p.reset();
            worstClose = juce::jmax (worstClose, msSince (t1));
            expectEquals (ek::ui::UiResources::getNumLiveInstances(), 0, "UI resources must die with the last editor");
        }
        const double avg = totalOpen / 19.0;
        logMessage (juce::String::formatted ("  editor open (ctor + first paint): first %.1f ms, avg %.1f ms, worst %.1f ms; "
                                             "close (editor + processor) worst %.1f ms; editor ctor alone avg %.1f ms",
                                             firstOpen, avg, worstOpen, worstClose, totalCtor / 19.0));
        expect (avg < 150.0 * kTimeScale, "editor open avg " + juce::String (avg, 1) + " ms (target < 150 ms)");
        expect (worstClose < 100.0 * kTimeScale, "close worst " + juce::String (worstClose, 1) + " ms");

        beginTest ("editor open with warm shared resources (another editor already open)");
        {
            ek::ui::UiResourceHolder keep;   // stands in for an editor that stays open
            {
                const double tf = juce::Time::getMillisecondCounterHiRes();
                for (auto* n : { "osd_ttf", "vfd_ttf", "hand_ttf", "display_ttf" }) ek::ui::embeddedTypeface (n);
                logMessage (juce::String::formatted ("  embedded fonts load: %.1f ms", msSince (tf)));
            }
            double total = 0.0, second = 0.0;
            for (int i = 0; i < 6; ++i)
            {
                EffectorKillaAudioProcessor p;
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditorIfNeeded());
                juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
                {
                    juce::Graphics g (img);
                    ed->paintEntireComponent (g, true);
                }
                if (i > 0) total += msSince (t0);
                const double t1 = juce::Time::getMillisecondCounterHiRes();
                {
                    juce::Graphics g (img);
                    ed->paintEntireComponent (g, true);
                }
                second += msSince (t1);
                p.editorBeingDeleted (ed.get());
            }
            logMessage (juce::String::formatted ("  editor open, warm resources: avg %.1f ms; a repaint of the whole window: avg %.1f ms",
                                                 total / 5.0, second / 6.0));
            expectEquals (ek::ui::UiResources::getNumLiveInstances(), 1);
        }
        expectEquals (ek::ui::UiResources::getNumLiveInstances(), 0);

        beginTest ("two editors share the UI resources; released with the last one");
        {
            EffectorKillaAudioProcessor a, b;
            std::unique_ptr<juce::AudioProcessorEditor> ea (a.createEditorIfNeeded()), eb (b.createEditorIfNeeded());
            expectEquals (ek::ui::UiResources::getNumLiveInstances(), 1);
            a.editorBeingDeleted (ea.get()); ea.reset();
            expectEquals (ek::ui::UiResources::getNumLiveInstances(), 1);
            b.editorBeingDeleted (eb.get()); eb.reset();
            expectEquals (ek::ui::UiResources::getNumLiveInstances(), 0);
        }
    }

    void editorWithOpenOverlays()
    {
        auto* mcm = juce::ModalComponentManager::getInstance();

        beginTest ("editor destroyed while a popup menu is open");
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            std::unique_ptr<EffectorKillaAudioProcessorEditor> ed (dynamic_cast<EffectorKillaAudioProcessorEditor*> (p->createEditorIfNeeded()));
            ed->addToDesktop (0);
            ed->setVisible (true);
            pumpMessages (20);
            if (ed->getToolbar().onSettings) ed->getToolbar().onSettings();   // settings menu (no target component)
            pumpMessages (50);
            expect (mcm->getNumModalComponents() >= 1, "the menu should be open");
            p->editorBeingDeleted (ed.get());
            ed.reset();
            pumpMessages (50);
            expectEquals (mcm->getNumModalComponents(), 0, "menu must close with the editor");
            p.reset();
            pumpMessages (50);
        }

        beginTest ("editor destroyed while the SAVE dialog is open, then the processor");
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
            ed->addToDesktop (0);
            ed->setVisible (true);
            pumpMessages (20);
            ed->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0));   // Ctrl+S = save dialog
            pumpMessages (50);
            expect (mcm->getNumModalComponents() >= 1, "the dialog should be open");
            p->editorBeingDeleted (ed.get());
            ed.reset();
            p.reset();            // processor gone too: the dialog callback must not touch it
            pumpMessages (80);
            expectEquals (mcm->getNumModalComponents(), 0, "dialog must close with the editor");
            expectEquals (ek::ui::UiResources::getNumLiveInstances(), 0);
        }

        beginTest ("editor destroyed with pending async UI messages");
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
            ed->addToDesktop (0);
            ed->setVisible (true);
            for (int i = 0; i < 5; ++i) { p->stepPreset (1); p->killLevel = ek::KillLevel::Unrated; p->kill(); }   // change messages queued
            ed->repaint();
            p->editorBeingDeleted (ed.get());
            ed.reset();
            pumpMessages (60);
            p.reset();
            pumpMessages (30);
            expect (true);
        }
    }
   #endif
};

static LifecycleTest lifecycleTest;
