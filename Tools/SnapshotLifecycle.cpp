// EffectorKillaSnapshot --lifecycle : headless plugin lifecycle check (run under `xvfb-run -a`).
//
// Mimics what a host does when it loads a plugin, opens / closes its window, removes it from a slot
// and quits: timings of processor and editor construction / destruction, and teardown with things
// still pending (open popup menus, a modal save prompt, async callbacks, a running audio thread).
// Prints a report and returns non-zero when something stayed alive or took too long.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/UiResources.h"
#include "ui/rack/UnitControls.h"
#include "ui/rack/GearList.h"
#include "ui/rack/PresetBrowser.h"
#include <thread>
#include <cstring>
#if JUCE_LINUX
 #include <dlfcn.h>
#endif

namespace juce::detail { bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages); }   // juce_events (Linux)

namespace
{
double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

void pump (int ms)
{
    // run the message loop for a while (popup windows, modal callbacks, callAsync)
    const double end = nowMs() + ms;
    while (nowMs() < end)
        if (! juce::detail::dispatchNextMessageOnSystemQueue (true)) juce::Thread::sleep (1);
}

int desktopWindows() { return juce::Desktop::getInstance().getNumComponents(); }
int modalCount() { return juce::ModalComponentManager::getInstance()->getNumModalComponents(); }

template <typename T>
T* findChild (juce::Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c)) return t;
        if (auto* t = findChild<T> (*c)) return t;
    }
    return nullptr;
}

EffectorKillaAudioProcessorEditor* openEditor (EffectorKillaAudioProcessor& p, bool onDesktop)
{
    auto* ed = dynamic_cast<EffectorKillaAudioProcessorEditor*> (p.createEditor());
    if (ed != nullptr && onDesktop)
    {
        ed->addToDesktop (0);
        ed->setVisible (true);
    }
    return ed;
}

struct Stats
{
    double sum = 0.0, worst = 0.0, first = -1.0;
    int n = 0;
    void add (double v) { if (first < 0.0) first = v; sum += v; worst = juce::jmax (worst, v); ++n; }
    juce::String text() const
    {
        return "first " + juce::String (first, 2) + " ms, avg " + juce::String (n > 0 ? sum / n : 0.0, 2)
               + " ms, worst " + juce::String (worst, 2) + " ms";
    }
};
} // namespace

// JUCE looks up the window manager atoms with "only if exists". A bare Xvfb (no window manager) has
// none of them, so mapping a top-level window failed with BadAtom. Intern them on a connection that
// stays open for the whole run (an X server forgets its atoms when its last client disconnects).
// Runs before JUCE opens its own display connection.
void prepareBareXServer()
{
   #if JUCE_LINUX
    void* x11 = dlopen ("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
    if (x11 == nullptr) return;
    using OpenDisplay = void* (*) (const char*);
    using InternAtom  = unsigned long (*) (void*, const char*, int);
    using Sync        = int (*) (void*, int);
    auto openDisplay = (OpenDisplay) dlsym (x11, "XOpenDisplay");
    auto internAtom  = (InternAtom)  dlsym (x11, "XInternAtom");
    auto sync        = (Sync)        dlsym (x11, "XSync");
    if (openDisplay == nullptr || internAtom == nullptr || sync == nullptr) return;
    void* display = openDisplay (nullptr);   // intentionally kept open until the process exits
    if (display == nullptr) return;
    const char* names[] = {
        "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "_NET_WM_PING", "WM_CHANGE_STATE", "WM_STATE",
        "_NET_WM_WINDOW_TYPE", "_NET_WM_STATE", "_NET_WM_STATE_HIDDEN", "_NET_WM_STATE_FULLSCREEN",
        "_NET_WM_MOVERESIZE", "_NET_FRAME_EXTENTS", "_NET_WORKAREA", "_MOTIF_WM_HINTS", "_WIN_HINTS",
        "KWM_WIN_DECORATION", "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE", "_NET_WM_ALLOWED_ACTIONS",
        "_NET_WM_WINDOW_TYPE_COMBO", "_NET_WM_WINDOW_TYPE_NORMAL", "_NET_WM_ACTION_RESIZE",
        "_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_MINIMIZE", "_NET_WM_ACTION_CLOSE",
        "_NET_WM_STATE_SKIP_TASKBAR", "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_MAXIMIZED_VERT",
        "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MODAL", "_NET_WM_STATE_STAYS_ON_TOP", "_NET_ACTIVE_WINDOW",
        "_NET_WM_PID", "_NET_WM_ICON", "_NET_WM_NAME", "_NET_WM_WINDOW_TYPE_DIALOG",
        "_NET_WM_WINDOW_TYPE_POPUP_MENU", "_NET_WM_WINDOW_TYPE_TOOLTIP", "UTF8_STRING" };
    for (auto* n : names) internAtom (display, n, 0);
    sync (display, 0);
   #endif
}

int runLifecycle (const juce::File&)
{
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    };

    const int baseWindows = desktopWindows();

    // ---- 1. processor only (plugin scan / project load without UI)
    {
        Stats ctor, dtor;
        for (int i = 0; i < 50; ++i)
        {
            const double a = nowMs();
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            const double b = nowMs();
            p.reset();
            ctor.add (b - a);
            dtor.add (nowMs() - b);
        }
        std::printf ("processor ctor: %s\nprocessor dtor: %s\n", ctor.text().toRawUTF8(), dtor.text().toRawUTF8());
    }

    // ---- 2. processor + editor (open the window, first paint, close, remove the plugin)
    {
        Stats procCtor, edCtor, firstPaint, edDtor, procDtor;
        for (int i = 0; i < 20; ++i)
        {
            double t = nowMs();
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            p->prepareToPlay (48000.0, 256);
            procCtor.add (nowMs() - t);

            t = nowMs();
            std::unique_ptr<EffectorKillaAudioProcessorEditor> ed (openEditor (*p, true));
            edCtor.add (nowMs() - t);

            t = nowMs();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);   // first frame
            firstPaint.add (nowMs() - t);
            pump (5);

            t = nowMs();
            ed.reset();
            edDtor.add (nowMs() - t);

            t = nowMs();
            p.reset();
            procDtor.add (nowMs() - t);
            pump (2);
        }
        std::printf ("processor ctor+prepare: %s\neditor ctor: %s\nfirst paint: %s\neditor dtor: %s\nprocessor dtor: %s\n",
                     procCtor.text().toRawUTF8(), edCtor.text().toRawUTF8(), firstPaint.text().toRawUTF8(),
                     edDtor.text().toRawUTF8(), procDtor.text().toRawUTF8());
        check (edCtor.worst < 150.0 || edCtor.n == 0, "editor opens in < 150 ms (worst " + juce::String (edCtor.worst, 1) + " ms)");
        check (ek::ui::UiResources::current() == nullptr, "UI resources released after the last editor closed");
        check (desktopWindows() == baseWindows, "no windows left on the desktop");
    }

    // ---- 3. close the window while popup menus / a modal prompt / async calls are pending, then quit
    {
        auto p = std::make_unique<EffectorKillaAudioProcessor>();
        p->prepareToPlay (48000.0, 256);
        std::unique_ptr<EffectorKillaAudioProcessorEditor> ed (openEditor (*p, true));
        pump (20);

        // settings menu (header) + a unit's menu (captures the processor) + the save prompt (modal)
        if (ed->getToolbar().onSettings) ed->getToolbar().onSettings();
        pump (20);
        if (auto* unit = findChild<ek::ui::UnitComponent> (ed->getRackView()))
            if (auto* btn = findChild<ek::ui::SideButton> (*unit); btn != nullptr && btn->onClick)
                btn->onClick();
        pump (20);
        if (ed->getToolbar().onSave) ed->getToolbar().onSave();
        pump (20);
        const int windowsOpen = desktopWindows();
        std::printf ("pending before close: %d desktop windows, %d modal\n", windowsOpen - baseWindows, modalCount());
        check (windowsOpen > baseWindows + 1, "popups / prompt were open");

        // an async call from the UI that outlives the window
        juce::Component::SafePointer<juce::Component> safe (ed.get());
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->repaint(); });

        const double t = nowMs();
        ed.reset();                // host closes the window
        const double closeMs = nowMs() - t;
        p.reset();                 // host removes the plugin / quits before the message loop runs again
        pump (60);                 // pending callbacks run now (must be no-ops)
        std::printf ("close with pending UI: %.2f ms\n", closeMs);
        check (closeMs < 100.0, "editor closes quickly with menus / prompt open");
        check (desktopWindows() == baseWindows, "menus and prompt closed with the editor ("
                                                  + juce::String (desktopWindows() - baseWindows) + " left)");
        check (modalCount() == 0, "no modal components left (" + juce::String (modalCount()) + ")");
        check (! juce::PopupMenu::dismissAllActiveMenus(), "no popup menu left");
        check (ek::ui::UiResources::current() == nullptr, "UI resources released");
    }

    // ---- 3b. close with a focused search field (global mouse listener), the FX LIBRARY drawer mid-
    //          animation and a tooltip on screen
    {
        for (int round = 0; round < 2; ++round)
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            std::unique_ptr<EffectorKillaAudioProcessorEditor> ed (openEditor (*p, true));
            pump (10);
            if (round == 0)
            {
                ed->showBrowser (true);
                pump (10);
                if (auto* browser = findChild<ek::ui::PresetBrowser> (*ed))
                    if (auto* field = findChild<juce::TextEditor> (*browser)) field->grabKeyboardFocus();
            }
            else
            {
                ed->setLibraryShown (true, true);   // drawer slides in (editor timer animates it)
                pump (10);
                if (auto* lib = findChild<ek::ui::GearList> (*ed))
                    if (auto* field = findChild<juce::TextEditor> (*lib)) field->grabKeyboardFocus();
                ed->setLibraryShown (false, true);  // ... and is closing when the window goes
            }
            if (auto* tip = findChild<juce::TooltipWindow> (*ed)) tip->displayTip ({ 100, 100 }, "tip");
            pump (5);
            const double t = nowMs();
            ed.reset();
            const double closeMs = nowMs() - t;
            p.reset();
            pump (30);
            check (closeMs < 100.0, juce::String (round == 0 ? "browser search focused" : "drawer animating")
                                        + ": editor closes (" + juce::String (closeMs, 2) + " ms)");
            check (desktopWindows() == baseWindows, "no windows left (" + juce::String (desktopWindows() - baseWindows) + ")");
            check (juce::Component::getCurrentlyFocusedComponent() == nullptr, "no focused component left");
        }
    }

    // ---- 4. close the editor and destroy the processor while the audio thread runs, then stop it
    {
        for (int round = 0; round < 5; ++round)
        {
            auto p = std::make_unique<EffectorKillaAudioProcessor>();
            p->prepareToPlay (48000.0, 256);
            std::unique_ptr<EffectorKillaAudioProcessorEditor> ed (openEditor (*p, round % 2 == 0));
            std::atomic<bool> run { true };
            std::thread audio ([&]
            {
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                while (run.load())
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 256; ++i) buf.setSample (c, i, 0.2f * std::sin ((float) i * 0.05f));
                    p->processBlock (buf, midi);
                }
            });
            for (int k = 0; k < 10; ++k) { ed->tickAll(); pump (3); if (k == 4) p->kill(); }
            const double t = nowMs();
            ed.reset();            // editor closes while audio is running
            const double closeMs = nowMs() - t;
            run.store (false);     // host stops processing ...
            audio.join();
            p->releaseResources();
            p.reset();             // ... and deletes the plugin
            pump (5);
            check (closeMs < 100.0, "editor closes while audio runs (" + juce::String (closeMs, 2) + " ms)");
        }
    }

    // ---- 5. two instances: closing one editor keeps the other's resources alive
    {
        auto p1 = std::make_unique<EffectorKillaAudioProcessor>();
        auto p2 = std::make_unique<EffectorKillaAudioProcessor>();
        std::unique_ptr<EffectorKillaAudioProcessorEditor> e1 (openEditor (*p1, false)), e2 (openEditor (*p2, false));
        (void) e1->createComponentSnapshot (e1->getLocalBounds());
        e1.reset();
        check (ek::ui::UiResources::current() != nullptr, "second editor keeps the shared resources");
        (void) e2->createComponentSnapshot (e2->getLocalBounds());
        e2.reset();
        check (ek::ui::UiResources::current() == nullptr, "freed with the last editor");
        p1.reset();
        p2.reset();
    }

    std::printf ("\n==== lifecycle: %d failure(s) ====\n", failures);
    return failures == 0 ? 0 : 1;
}
