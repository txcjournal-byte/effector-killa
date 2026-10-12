#include "PluginEditor.h"
#include "ui/rack/CardStyle.h"
#include "ui/rack/CableStyle.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "1.0.0"   // snapshot tool (not a plugin target)
#endif

using namespace ek;
namespace hw = ek::ui::hw;

namespace
{
// tick() is optional on the standalone units
template <typename T> auto tickIfPossible (T& t, int) -> decltype (t.tick(), void()) { t.tick(); }
template <typename T> void tickIfPossible (T&, long) {}

// ModUnit collapse API (isExpanded / onExpandedChanged). Older ModUnits without it are always expanded.
template <typename T> auto modIsExpanded (const T& t, int) -> decltype (t.isExpanded(), bool()) { return t.isExpanded(); }
template <typename T> bool modIsExpanded (const T&, long) { return true; }
template <typename T> auto hookModExpanded (T& t, std::function<void()> f, int) -> decltype (t.onExpandedChanged = f, void())
{
    t.onExpandedChanged = std::move (f);
}
template <typename T> void hookModExpanded (T&, std::function<void()>, long) {}

// GearList::onDragStarted (agent C). Without it dragOperationStarted() below still catches the drag.
template <typename T> auto hookDragStarted (T& t, std::function<void()> f, int) -> decltype (t.onDragStarted = f, void())
{
    t.onDragStarted = std::move (f);
}
template <typename T> void hookDragStarted (T&, std::function<void()>, long) {}

constexpr double kDrawerSeconds = 0.16;

int countComponents (const juce::Component& c)
{
    int n = 1;
    for (auto* ch : c.getChildren()) n += countComponents (*ch);
    return n;
}

void releaseKeyboardRecursive (juce::Component& c)
{
    // text fields keep their normal focus behaviour (they give it back on Enter / Esc / click outside)
    if (auto* te = dynamic_cast<juce::TextEditor*> (&c))
    {
        // fields without their own handlers: Enter / Esc end typing and hand the keyboard back
        juce::Component::SafePointer<juce::TextEditor> safe (te);
        auto release = [safe] { if (safe != nullptr) safe->giveAwayKeyboardFocus(); };
        if (! te->onReturnKey) te->onReturnKey = release;
        if (! te->onEscapeKey) te->onEscapeKey = release;
        return;
    }
    c.setWantsKeyboardFocus (false);
    c.setMouseClickGrabsKeyboardFocus (false);
    for (auto* ch : c.getChildren()) releaseKeyboardRecursive (*ch);
}
} // namespace

// ============================================================================================
void EffectorKillaAudioProcessorEditor::Frame::paint (juce::Graphics& g)
{
    cache.draw (g, getLocalBounds(), [this] (juce::Graphics& bg)
    {
        const float W = (float) getWidth(), H = (float) getHeight();
        bg.fillAll (hw::col::bg);

        // very subtle deep-blue radial glow (trapwin.com background): a wide one from the top centre,
        // a fainter one rising from the lower right
        {
            juce::ColourGradient top (juce::Colour (0xff4676c4).withAlpha (0.16f), W * 0.5f, -H * 0.18f,
                                      juce::Colour (0xff4676c4).withAlpha (0.0f), W * 0.5f + W * 0.62f, -H * 0.18f, true);
            top.addColour (0.45, juce::Colour (0xff4676c4).withAlpha (0.06f));
            bg.setGradientFill (top);
            bg.fillRect (getLocalBounds().toFloat());
            juce::ColourGradient low (juce::Colour (0xff2b4d8f).withAlpha (0.10f), W * 0.86f, H * 1.05f,
                                      juce::Colour (0xff2b4d8f).withAlpha (0.0f), W * 0.86f - W * 0.45f, H * 1.05f, true);
            bg.setGradientFill (low);
            bg.fillRect (getLocalBounds().toFloat());
        }

        // sparse sparkles + star dust (deterministic, so snapshots are stable)
        juce::Random rnd (0x7a11);
        for (int i = 0; i < 90; ++i)
        {
            const juce::Point<float> p (rnd.nextFloat() * W, rnd.nextFloat() * H);
            bg.setColour (juce::Colour (0xffc9d8f5).withAlpha (0.06f + 0.16f * rnd.nextFloat()));
            const float d = 0.8f + rnd.nextFloat() * 1.0f;
            bg.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
        }
        for (int i = 0; i < 26; ++i)
        {
            const juce::Point<float> p (rnd.nextFloat() * W, rnd.nextFloat() * H);
            const float big = rnd.nextFloat();
            const float r = big > 0.85f ? 5.0f + 3.0f * rnd.nextFloat() : 2.0f + 2.2f * rnd.nextFloat();
            ek::card::paintSparkle (bg, p, r, hw::col::ice.withAlpha (big > 0.85f ? 0.55f : 0.22f + 0.2f * rnd.nextFloat()),
                                    big > 0.85f ? 0.7f : 0.0f);
        }
    });
}

// ============================================================================================
void EffectorKillaAudioProcessorEditor::CableOverlay::paint (juce::Graphics& g)
{
    if (source < 0) return;
    ek::ui::cable::paintOverlay (g, ek::ui::modSourceColour (source), from, to, target);   // ui/rack/CableStyle
}

// ============================================================================================
void EffectorKillaAudioProcessorEditor::KillFlash::trigger()
{
    if (hw::reduceMotion()) return;
    startMs = juce::Time::getMillisecondCounterHiRes();
    tick();
}

void EffectorKillaAudioProcessorEditor::KillFlash::tick()
{
    const double t = (juce::Time::getMillisecondCounterHiRes() - startMs) / 220.0;
    const float a = t >= 1.0 || t < 0.0 ? 0.0f : (float) (1.0 - t) * (float) (1.0 - t);
    if (std::abs (a - shown) < 0.002f) return;
    shown = a;
    setVisible (a > 0.0f);
    repaint();
}

void EffectorKillaAudioProcessorEditor::KillFlash::paint (juce::Graphics& g)
{
    if (shown <= 0.0f) return;
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (hw::col::ice.withAlpha (0.12f * shown));
    g.fillRoundedRectangle (r, 20.0f);
    g.setColour (hw::col::ice.withAlpha (0.6f * shown));
    g.drawRoundedRectangle (r.reduced (1.0f), 19.0f, 1.5f);
}

// ============================================================================================
void EffectorKillaAudioProcessorEditor::DrawerBackdrop::paint (juce::Graphics& g)
{
    if (amount <= 0.0f) return;
    // dim MOD + chain, then a soft shadow to the right of the drawer
    g.setColour (juce::Colour (0xff020308).withAlpha (0.62f * amount));
    g.fillRoundedRectangle (dimArea.toFloat(), 18.0f);
    const auto d = drawer.toFloat();
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.55f * amount), d.getRight(), 0.0f,
                             juce::Colours::black.withAlpha (0.0f), d.getRight() + 26.0f, 0.0f, false);
    g.setGradientFill (sh);
    g.fillRect (juce::Rectangle<float> (d.getRight() - 4.0f, d.getY() + 10.0f, 30.0f, d.getHeight() - 20.0f));
}

// ============================================================================================
EffectorKillaAudioProcessorEditor::EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), toolbar (p), master (p), modUnit (p),
      rackView (p), gearList (p), browser (p)
{
    setOpaque (true);
    addAndMakeVisible (content);
    content.addAndMakeVisible (frame);
    content.addAndMakeVisible (toolbar);
    content.addAndMakeVisible (master);
    content.addAndMakeVisible (modUnit);

    rackViewport.setViewedComponent (&rackView, false);
    rackViewport.setScrollBarsShown (true, false);
    rackViewport.setScrollBarThickness (8);
    rackViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    content.addAndMakeVisible (rackViewport);
    content.addChildComponent (killFlash);
    content.addChildComponent (backdrop);
    content.addChildComponent (gearList);
    content.addChildComponent (browser);
    content.addAndMakeVisible (cableOverlay);   // topmost

    toolbar.onBrowse = [this] { showBrowser (! browser.isVisible()); };
    toolbar.isBrowserShown = [this] { return browser.isVisible(); };
    toolbar.onSave = [this] { showSaveDialog(); };
    toolbar.onSettings = [this] { showSettingsMenu(); };
    toolbar.onInfo = [this] { showInfoMenu(); };
    toolbar.onToggleLibrary = [this] { setLibraryShown (! libraryOpen); };
    toolbar.isLibraryShown = [this] { return libraryOpen; };
    backdrop.onClick = [this] { setLibraryShown (false); };
    master.onKillFlash = [this] { killFlash.trigger(); };
    master.setReduceMotion (hw::reduceMotion());
    master.onSetScale = [this] (float s) { setUiScale (s); };
    browser.onClose = [this] { showBrowser (false); };
    hookModExpanded (modUnit, [this] { layoutBody(); }, 0);
    auto say = [this] (const juce::String& m) { toolbar.showMessage (m); };
    rackView.onMessage = say;
    // MOD patching + knob right-click (modulation list, MACRO LEARN) + modulation rings
    modUnit.onHandleDrag = [this] (int src, juce::Point<int> screen) { modHandleDragged (src, screen, false); };
    modUnit.onHandleDrop = [this] (int src, juce::Point<int> screen) { modHandleDragged (src, screen, true); };
    rackView.onKnobRightClick = [this] (int slot, int param, juce::Point<int>) { showKnobMenu (slot, param); };
    rackView.paintKnobOverlay = [this] (juce::Graphics& g, juce::Rectangle<float> face, int slot, int param)
    {
        paintKnobOverlay (g, face, slot, param);
    };
    gearList.onMessage = say;
    hookDragStarted (gearList, [this]
    {
        if (! libraryOpen || draggingFromDrawer) return;
        draggingFromDrawer = true;
        gearList.setInterceptsMouseClicks (false, false);
        backdrop.setInterceptsMouseClicks (false, false);
        tickDrawer();
    }, 0);

    getLookAndFeel().setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf20a0f18));
    getLookAndFeel().setColour (juce::TooltipWindow::textColourId, hw::col::print);
    getLookAndFeel().setColour (juce::TooltipWindow::outlineColourId, hw::col::ice.withAlpha (0.35f));
    getLookAndFeel().setColour (juce::ScrollBar::thumbColourId, hw::col::ice.withAlpha (0.3f));
    getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0a0f18));
    getLookAndFeel().setColour (juce::PopupMenu::textColourId, hw::col::print);
    getLookAndFeel().setColour (juce::PopupMenu::headerTextColourId, hw::col::ice);
    getLookAndFeel().setColour (juce::PopupMenu::highlightedBackgroundColourId, hw::col::ice.withAlpha (0.22f));
    getLookAndFeel().setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    // the plugin never takes the computer keyboard: no shortcuts, clicks do not grab the focus
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    releaseKeyboard();
    addMouseListener (this, true);   // click outside an active text field = give the keyboard back
    processor.addChangeListener (this);

    setResizable (true, true);
    updateConstrainer();
    layoutBody();
    // a size saved with the project wins; otherwise the size the user picked last (any project)
    float startScale = processor.uiScale;
    if (startScale <= 0.0f && hw::userUiScale() >= 0.0f) startScale = hw::userUiScale();
    // a saved size that does not fit this screen (e.g. saved by an older, larger UI) falls back to Auto
    if (startScale > 0.0f && ! fitsScreen (startScale)) startScale = 0.0f;
    applyScale (startScale);
    startTimerHz (30);
}

EffectorKillaAudioProcessorEditor::~EffectorKillaAudioProcessorEditor()
{
    // The host may close the window and unload the library without running its message loop again
    // (FL Studio on exit). Stop and detach everything first, then close every window that lives
    // outside this component tree, so nothing of the plugin is left on the desktop or in the queue.
    stopTimer();
    processor.removeChangeListener (this);
    removeMouseListener (this);

    // popup menus are top-level windows (some are not attached to a component); their callbacks
    // hold SafePointers and become no-ops
    juce::PopupMenu::dismissAllActiveMenus();
    prompt.reset();      // a modal prompt: cancelled, its callback sees the editor gone
    aboutBox.close();
    tooltips.hideTip();

    // a text field with the keyboard focus would run its focus-lost callbacks during member destruction
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent(); focused != nullptr && isParentOf (focused))
        focused->giveAwayKeyboardFocus();
}

void EffectorKillaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (hw::col::bg);
}

void EffectorKillaAudioProcessorEditor::releaseKeyboard()
{
    releaseKeyboardRecursive (content);
    lastChildCount = countComponents (content);
}

bool EffectorKillaAudioProcessorEditor::fitsScreen (float s) const
{
    const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto area = d != nullptr ? d->userArea : juce::Rectangle<int> (1920, 1040);
    return (float) kHeight * s <= (float) area.getHeight() * 0.85f && (float) kWidth * s <= (float) area.getWidth() * 0.95f;
}

float EffectorKillaAudioProcessorEditor::autoScale() const
{
    const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto area = d != nullptr ? d->userArea : juce::Rectangle<int> (1920, 1040);
    // largest size option whose height fits ~75 % of the screen (and whose width fits the screen)
    float best = sizeOptions[0];
    for (float s : sizeOptions)
        if ((float) kHeight * s <= (float) area.getHeight() * 0.75f && (float) kWidth * s <= (float) area.getWidth() * 0.95f)
            best = s;
    return best;
}

void EffectorKillaAudioProcessorEditor::updateConstrainer()
{
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) kWidth / (double) kHeight);
        c->setSizeLimits (juce::roundToInt (kWidth * 0.6), juce::roundToInt (kHeight * 0.6), kWidth * 2, kHeight * 2);
    }
}

void EffectorKillaAudioProcessorEditor::setUiScale (float s)
{
    applyScale (s);
    hw::setUserUiScale (processor.uiScale);   // remembered for new instances too
}

void EffectorKillaAudioProcessorEditor::applyScale (float s)
{
    processor.uiScale = s;
    const float scale = s <= 0.0f ? autoScale() : juce::jlimit (0.6f, 2.0f, s);
    settingSize = true;
    setSize (juce::roundToInt ((float) kWidth * scale), juce::roundToInt ((float) kHeight * scale));
    settingSize = false;
}

void EffectorKillaAudioProcessorEditor::resized()
{
    const float scale = currentScale();
    if (scale <= 0.0f) return;
    content.setBounds (0, 0, kWidth, kHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    // Remember a new size only when the user drags the corner. Hosts (FL Studio) restore their own
    // remembered window size on open - often from an older, larger version of the UI - and that must
    // not overwrite the user's size choice.
    if (! settingSize && isShowing() && juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        processor.uiScale = scale;
}

bool EffectorKillaAudioProcessorEditor::modExpanded() const
{
    return modIsExpanded (modUnit, 0);
}

void EffectorKillaAudioProcessorEditor::layoutBody()
{
    const bool expanded = modExpanded();
    lastModHeight = expanded ? kModExpanded : kModCollapsed;
    content.setBounds (0, 0, kWidth, kHeight);
    frame.setBounds (0, 0, kWidth, kHeight);
    frame.invalidate();
    toolbar.setBounds (toolbarArea());
    toolbar.setCentreX (centreArea().getCentreX());
    master.setBounds (masterArea());
    modUnit.setBounds (modUnitArea (expanded));

    const auto rack = rackArea (expanded);
    rackViewport.setBounds (rack);
    killFlash.setBounds (rack);
    rackView.setSize (rack.getWidth() - rackViewport.getScrollBarThickness(), juce::jmax (rack.getHeight(), rackView.getIdealHeight()));

    // FX library drawer (over MOD + chain) and its backdrop
    backdrop.setBounds (0, 0, kWidth, kHeight);
    backdrop.dimArea = centreArea();
    tickDrawer();
    // the browser covers the centre column (MOD card + chain); MASTER stays visible
    browser.setBounds (browserArea());
    cableOverlay.setBounds (0, 0, kWidth, kHeight);
    cableOverlay.toFront (false);
}

// ---- FX library drawer --------------------------------------------------------------------------
void EffectorKillaAudioProcessorEditor::setLibraryShown (bool shown, bool animate)
{
    if (shown && browser.isVisible()) showBrowser (false);
    if (shown == libraryOpen && ! (shown && ! gearList.isVisible())) return;
    libraryOpen = shown;
    draggingFromDrawer = false;
    if (! animate || hw::reduceMotion()) drawerPos = shown ? 1.0f : 0.0f;
    drawerAnimStart = juce::Time::getMillisecondCounterHiRes() * 0.001 - (shown ? drawerPos : 1.0f - drawerPos) * kDrawerSeconds;
    if (shown)
    {
        backdrop.setVisible (true);
        gearList.setVisible (true);
        backdrop.toFront (false);
        gearList.toFront (false);
        cableOverlay.toFront (false);
    }
    tickDrawer();
    toolbar.tick();
}

void EffectorKillaAudioProcessorEditor::tickDrawer()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float target = libraryOpen ? 1.0f : 0.0f;
    if (drawerPos != target)
    {
        const float t = (float) juce::jlimit (0.0, 1.0, (now - drawerAnimStart) / kDrawerSeconds);
        drawerPos = libraryOpen ? t : 1.0f - t;
        if (hw::reduceMotion()) drawerPos = target;
    }
    // ease out; a short slide (28 px) plus a fade - subtle
    const float e = 1.0f - (1.0f - drawerPos) * (1.0f - drawerPos);
    const auto area = drawerArea();
    gearList.setBounds (area.translated (juce::roundToInt (-28.0f * (1.0f - e)), 0));
    gearList.setAlpha (draggingFromDrawer ? 0.0f : juce::jlimit (0.0f, 1.0f, e * 1.15f));
    const float dim = draggingFromDrawer ? 0.0f : e;
    if (std::abs (backdrop.amount - dim) > 0.001f || backdrop.drawer != gearList.getBounds())
    {
        backdrop.amount = dim;
        backdrop.drawer = gearList.getBounds();
        backdrop.repaint();
    }
    if (drawerPos <= 0.0f && ! libraryOpen)
    {
        gearList.setVisible (false);
        backdrop.setVisible (false);
    }
}

void EffectorKillaAudioProcessorEditor::dragOperationStarted (const juce::DragAndDropTarget::SourceDetails& d)
{
    // dragging an effect out of the drawer: fade it out and let the chain under it take the drop
    if (libraryOpen && d.sourceComponent != nullptr && gearList.isParentOf (d.sourceComponent.get()))
    {
        draggingFromDrawer = true;
        gearList.setInterceptsMouseClicks (false, false);
        backdrop.setInterceptsMouseClicks (false, false);
        tickDrawer();
    }
}

void EffectorKillaAudioProcessorEditor::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    if (! draggingFromDrawer) return;
    draggingFromDrawer = false;
    gearList.setInterceptsMouseClicks (true, true);
    backdrop.setInterceptsMouseClicks (true, true);
    libraryOpen = false;
    drawerPos = 0.0f;
    tickDrawer();
    toolbar.tick();
}

void EffectorKillaAudioProcessorEditor::showBrowser (bool shown)
{
    if (shown == browser.isVisible()) return;
    if (shown && libraryOpen) setLibraryShown (false, false);
    browser.setVisible (shown);
    if (shown)
    {
        browser.toFront (false);
        cableOverlay.toFront (false);
        browser.open();
        // opening the browser must not take the keyboard (FL space = play): the search field only
        // gets the focus when the user clicks into it
        if (auto* f = juce::Component::getCurrentlyFocusedComponent(); f != nullptr && browser.isParentOf (f))
            f->giveAwayKeyboardFocus();
    }
}

void EffectorKillaAudioProcessorEditor::tickAll()
{
    // structure changes normally arrive through the change message; checking here too keeps the
    // rack right even when the host delays messages (cheap: compares the slot types only)
    rackView.refresh (false);
    if ((modExpanded() ? kModExpanded : kModCollapsed) != lastModHeight) layoutBody();
    const int h = rackViewport.getHeight();
    const int want = juce::jmax (h, rackView.getIdealHeight());
    if (rackView.getHeight() != want) rackView.setSize (rackView.getWidth(), want);
    toolbar.tick();
    tickIfPossible (modUnit, 0);   // (MasterColumn runs its own timer)
    rackView.tick();
    killFlash.tick();
    if (gearList.isVisible()) tickDrawer();
    // units / chips are created on the fly: keep them away from the keyboard too
    if (countComponents (content) != lastChildCount) releaseKeyboard();
}

void EffectorKillaAudioProcessorEditor::setReduceMotion (bool on)
{
    hw::setReduceMotion (on);
    master.setReduceMotion (on);
}

void EffectorKillaAudioProcessorEditor::timerCallback()
{
    tickAll();
}

void EffectorKillaAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rackView.refresh();
    rackView.setSize (rackView.getWidth(), juce::jmax (rackViewport.getHeight(), rackView.getIdealHeight()));
    toolbar.refresh();
}

void EffectorKillaAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // a click anywhere outside the focused text field ends typing and gives the keyboard back to the host
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if (focused == nullptr || dynamic_cast<juce::TextEditor*> (focused) == nullptr) return;
    if (! isParentOf (focused)) return;
    if (e.eventComponent == focused || focused->isParentOf (e.eventComponent)) return;
    focused->giveAwayKeyboardFocus();
}

bool EffectorKillaAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    // No plugin keyboard shortcuts: keys go to the host (FL Studio space = play / stop etc.).
    // Esc that a text field did not use only closes an overlay.
    if (k.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (browser.isVisible()) { showBrowser (false); return true; }
        if (libraryOpen) { setLibraryShown (false); return true; }
    }
    return false;
}

void EffectorKillaAudioProcessorEditor::showSaveDialog()
{
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    showNamePrompt ("SAVE PRESET", "Preset name:",
                    processor.getMeta().factory ? processor.getMeta().name + " (mine)" : processor.getMeta().name,
                    [safe] (const juce::String& name)
                    {
                        if (safe == nullptr) return;
                        const bool ok = safe->processor.saveUserPreset (name);
                        safe->toolbar.showMessage (ok ? "SAVED" : "SAVE FAILED");
                    });
}

void EffectorKillaAudioProcessorEditor::showNamePrompt (const juce::String& title, const juce::String& message,
                                                        const juce::String& initialName,
                                                        std::function<void (const juce::String&)> onSave)
{
    // one prompt at a time; owned by the editor (not deleteWhenDismissed), so closing the window closes it
    prompt = std::make_unique<juce::AlertWindow> (title, message, juce::MessageBoxIconType::NoIcon);
    prompt->addTextEditor ("name", initialName);
    prompt->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    prompt->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    juce::Component::SafePointer<juce::AlertWindow> window (prompt.get());
    prompt->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window, onSave] (int res)
    {
        // the dialog is gone: nothing in the plugin keeps the keyboard
        juce::Component::unfocusAllComponents();
        if (safe == nullptr || window == nullptr) return;   // editor closed (the prompt died with it)
        const auto name = window->getTextEditorContents ("name").trim();
        // delete the window after this callback returns (it is still referenced by the modal manager)
        juce::MessageManager::callAsync ([safe, window]
        {
            if (safe != nullptr && window != nullptr && safe->prompt.get() == window.getComponent()) safe->prompt.reset();
        });
        if (res == 1 && name.isNotEmpty() && onSave) onSave (name);
    }), false);
}

void EffectorKillaAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("TRAPWIN EFFECTOR 1.0 - Every effect. One win.");
    juce::PopupMenu size;
    size.addItem (100, "Auto (fits the screen)", true, processor.uiScale <= 0.0f);
    for (float opt : sizeOptions)
    {
        const int pct = juce::roundToInt (opt * 100.0f);
        size.addItem (100 + pct, juce::String (pct) + " %", true,
                      processor.uiScale > 0.0f && std::abs (processor.uiScale * 100.0f - (float) pct) < 0.5f);
    }
    m.addSubMenu ("Window size  (or drag the corner)", size);
    m.addItem (2, "FX library", true, libraryOpen);
    m.addItem (5, "Reduce motion", true, hw::reduceMotion());
    juce::PopupMenu os;
    for (int i = 0; i < 3; ++i) os.addItem (300 + i, juce::String (1 << i) + "x", true, processor.getOversampling() == i);
    m.addSubMenu ("Oversampling (saturation / distortion / crush / clipper)", os);
    m.addSeparator();
    m.addItem (4, "Open user preset folder");
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 100) safe->setUiScale (0.0f);
        else if (r > 100 && r <= 250) safe->setUiScale ((float) (r - 100) / 100.0f);
        else if (r == 2) safe->setLibraryShown (! safe->libraryOpen);
        else if (r == 5) safe->setReduceMotion (! hw::reduceMotion());
        else if (r >= 300 && r < 303) safe->processor.setOversampling (r - 300);
        else if (r == 4)
        {
            auto dir = PresetBank::getUserPresetDir();
            dir.createDirectory();
            dir.startAsProcess();
        }
    });
}

void EffectorKillaAudioProcessorEditor::showInfoMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("TRAPWIN EFFECTOR " + juce::String (JucePlugin_VersionString));
    m.addItem (1, "About TrapWin Effector");
    m.addItem (2, "Open the manual (trapwin.com)");
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1)
            safe->aboutBox = juce::AlertWindow::showScopedAsync (juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::NoIcon)
                .withTitle ("TrapWin Effector")
                .withMessage ("Version " + juce::String (JucePlugin_VersionString) + "\nEvery effect. One win.\n\n"
                              "15 effects in one chain, KILL, macros, MOD, A / B and level-matched compare.\n"
                              "(c) TrapWin - trapwin.com")
                .withButton ("OK")
                .withAssociatedComponent (safe.getComponent()), nullptr);
        else if (r == 2)
            juce::URL ("https://trapwin.com").launchInDefaultBrowser();
    });
}

// ============================================================================================
// MOD patching / knob menu / modulation rings
// ============================================================================================
void EffectorKillaAudioProcessorEditor::modHandleDragged (int source, juce::Point<int> screen, bool dropped)
{
    ek::ui::RackView::KnobHit hit;
    if (rackViewport.isShowing() && rackViewport.getScreenBounds().contains (screen))
        hit = rackView.knobAt (screen);
    const int macro = hit.slot < 0 ? master.macroAt (screen) : -1;

    if (! dropped)
    {
        cableOverlay.source = source;
        cableOverlay.from = content.getLocalPoint (nullptr, modUnit.getHandleScreenPosition (source)).toFloat();
        cableOverlay.to = content.getLocalPoint (nullptr, screen).toFloat();
        cableOverlay.target = hit.slot >= 0 ? content.getLocalArea (&rackView, hit.bounds)
                            : macro >= 0 ? content.getLocalArea (nullptr, master.macroScreenBounds (macro)).reduced (8, 4)
                                         : juce::Rectangle<int>();
        cableOverlay.repaint();
        return;
    }

    cableOverlay.source = -1;
    cableOverlay.target = {};
    cableOverlay.repaint();
    const juce::String src (ek::modSourceName (source));
    if (hit.slot >= 0)
    {
        const int idx = processor.addModAssignment (source, hit.slot, hit.param, 0.5f);
        toolbar.showMessage (idx >= 0 ? src + " -> S" + juce::String (hit.slot + 1) + (hit.param < kNumParams ? " P" + juce::String (hit.param + 1) : juce::String (" DRY/WET"))
                                      : juce::String ("MODULATION FULL (16 MAX)"));
        rackView.repaintKnobs();
    }
    else if (macro >= 0)
    {
        const int idx = processor.addMacroModAssignment (source, macro, 0.5f);
        toolbar.showMessage (idx >= 0 ? src + " -> " + MacroEngine::macroName (macro) : juce::String ("MODULATION FULL (16 MAX)"));
    }
}

void EffectorKillaAudioProcessorEditor::paintKnobOverlay (juce::Graphics& g, juce::Rectangle<float> face, int slot, int param)
{
    const auto idx = processor.findModAssignments (slot, param);
    if (! idx.empty())
    {
        const auto all = processor.getModAssignments();
        const float base = processor.getSlotParam (slot, param);
        const float cur = processor.getEffectiveSlotParam (slot, param);
        int ring = 0;
        for (int i : idx)
        {
            if (i < 0 || i >= (int) all.size()) continue;
            const auto& a = all[(size_t) i];
            const bool bipolar = a.source == (int) ModSourceId::Lfo1 || a.source == (int) ModSourceId::Lfo2;
            ek::ui::paintModRing (g, face, base, a.depth, ek::ui::modSourceColour (a.source), bipolar, cur, ring++);
        }
    }
    // MACRO LEARN: a small dot per learned macro assignment
    int k = 0;
    for (const auto& mm : processor.getMacroAssignmentsFor (slot, param))
        if (mm.learned)
        {
            g.setColour (hw::col::ice);
            g.fillEllipse (face.getX() + 1.0f + 6.0f * (float) k++, face.getY() + 1.0f, 4.0f, 4.0f);
        }
}

void EffectorKillaAudioProcessorEditor::showKnobMenu (int slot, int param)
{
    const auto type = processor.getSlotType (slot);
    if (type == EffectType::None) return;
    const auto& info = effectInfo (type);
    const juce::String pname = param < kNumParams ? info.params[(size_t) param].name : juce::String ("DRY/WET");
    const float defaultValue = param < kNumParams ? info.params[(size_t) param].defaultNorm() : 1.0f;
    static constexpr float depths[] = { 1.0f, 0.5f, 0.25f, -0.25f, -0.5f, -1.0f };

    juce::PopupMenu m;
    m.addSectionHeader ("S" + juce::String (slot + 1) + "  " + info.name.toUpperCase() + "  -  " + pname.toUpperCase());
    m.addItem (1, "Reset to default");

    m.addSeparator();
    m.addSectionHeader ("MODULATION");
    const auto all = processor.getModAssignments();
    const auto idx = processor.findModAssignments (slot, param);
    if (idx.empty()) m.addItem (2, "(drag a MOD jack onto this knob)", false);
    for (int i : idx)
    {
        if (i < 0 || i >= (int) all.size()) continue;
        const auto& a = all[(size_t) i];
        juce::PopupMenu sub;
        for (int d = 0; d < 6; ++d)
            sub.addItem (2000 + i * 10 + d, "Depth " + juce::String (juce::roundToInt (depths[d] * 100.0f)) + " %", true,
                         std::abs (a.depth - depths[d]) < 0.001f);
        sub.addSeparator();
        sub.addItem (1000 + i, "Remove");
        m.addSubMenu (juce::String (ek::modSourceName (a.source)) + "   depth " + juce::String (juce::roundToInt (a.depth * 100.0f)) + " %", sub);
    }
    juce::PopupMenu add;
    for (int src = 0; src < kNumModSources; ++src) add.addItem (3000 + src, ek::modSourceName (src));
    m.addSubMenu ("Add modulation (depth 50 %)", add);
    if (! idx.empty()) m.addItem (3, "Remove all modulation of this knob");

    m.addSeparator();
    m.addSectionHeader ("MACRO LEARN");
    const auto maps = processor.getMacroAssignmentsFor (slot, param);
    for (int mc = 0; mc < kNumMacros; ++mc)
    {
        const bool learned = std::any_of (maps.begin(), maps.end(), [mc] (const MacroMapping& x) { return x.macro == mc && x.learned; });
        juce::PopupMenu sub;
        sub.addItem (4000 + mc * 10 + 0, "Range: full (macro 0 = 0 %, 100 = 100 %)");
        sub.addItem (4000 + mc * 10 + 1, "Range: inverted (macro 0 = 100 %, 100 = 0 %)");
        sub.addItem (4000 + mc * 10 + 2, "Range: subtle (knob +- 25 %)");
        m.addSubMenu (juce::String ("Assign to ") + MacroEngine::macroName (mc) + (learned ? "   (assigned)" : ""), sub);
        if (learned) m.addItem (5000 + mc, juce::String ("Remove from ") + MacroEngine::macroName (mc));
    }

    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, slot, param, defaultValue] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& p = safe->processor;
        if (r == 1) { p.pushUndo(); p.setSlotParam (slot, param, defaultValue); }
        else if (r == 3) p.removeModAssignmentsFor (slot, param);
        else if (r >= 1000 && r < 2000) p.removeModAssignment (r - 1000);
        else if (r >= 2000 && r < 3000) { p.pushUndo(); p.setModDepth ((r - 2000) / 10, depths[(r - 2000) % 10]); }
        else if (r >= 3000 && r < 4000) p.addModAssignment (r - 3000, slot, param, 0.5f);
        else if (r >= 4000 && r < 5000)
        {
            const int mc = (r - 4000) / 10, kind = (r - 4000) % 10;
            const float v = p.getSlotParam (slot, param);
            float lo = 0.0f, hi = 1.0f;
            if (kind == 1) { lo = 1.0f; hi = 0.0f; }
            else if (kind == 2) { lo = juce::jlimit (0.0f, 1.0f, v - 0.25f); hi = juce::jlimit (0.0f, 1.0f, v + 0.25f); }
            p.assignMacro (mc, slot, param, lo, hi);
        }
        else if (r >= 5000) p.removeMacroAssignment (r - 5000, slot, param);
        safe->rackView.repaintKnobs();
    });
}
