#pragma once

#include "PluginProcessor.h"
#include "ui/rack/Hardware.h"
#include "ui/rack/Toolbar.h"
#include "ui/rack/MasterColumn.h"
#include "ui/rack/RackView.h"
#include "ui/rack/GearList.h"
#include "ui/rack/PresetBrowser.h"

#include "ui/rack/ModUnit.h"
#include "ui/UiResources.h"
#include "ui/Prompt.h"
using EkModUnit = ek::ui::ModUnit;

// FEATURE WIRING (keep when restyling the editor):
//   * MOD patching: modUnit.onHandleDrag -> CableOverlay (cable + target highlight);
//     modUnit.onHandleDrop -> rackView.knobAt (screenPos) -> proc.addModAssignment (source, slot, param, 0.5)
//     or master.macroAt (screenPos) -> proc.addMacroModAssignment (source, macro, 0.5)
//     (master = the MASTER column, MasterColumn.h; the old PerformerUnit name is an alias of it).
//   * Knob right-click: rackView.onKnobRightClick -> showKnobMenu(): reset, modulation list (depth /
//     remove / add / remove all) and MACRO LEARN (assignMacro with full / inverted / subtle range,
//     removeMacroAssignment).
//   * Knob overlay: rackView.paintKnobOverlay -> paintKnobOverlay(): paintModRing per assignment and a
//     dot for learned macro assignments.
//   * SPLIT, SPECTRAL VIEW and LEVEL-MATCHED POWER live in RackView (see RackView.h).

// Window (Docs/TRAPWIN-STYLE.md), one layout, base size 1000 x 680:
//   whole window = #04060A with a deep-blue radial glow and sparse sparkles (cached), no frame;
//   HEADER 0,0,1000,48 (logo, preset pill, A/B, undo/redo, settings, BYPASS);
//   MOD card 16,56,752,(36 collapsed | 104 expanded), CHAIN viewport x 16 w 752 from 8 px below MOD
//   down to y 664 (RackView); MASTER column 784,56,200,608 (MasterColumn);
//   FX LIBRARY = drawer overlay 16,56,232,608 over MOD + chain (header library button; click outside
//   closes, it hides itself while an effect is dragged out of it); PRESET BROWSER overlay 16,56,752,608.
// Everything is laid out in base coordinates and scaled (75 / 100 / 125 / 150 % / Auto / drag the corner).
// KEYBOARD: the editor never takes the computer keyboard (no shortcuts) - only an active text field
// does, and it gives the focus back on Enter / Esc / click outside, so DAW transport keys pass through.
// TEARDOWN (the host may unload the library right after the editor is deleted): nothing of the UI
// may outlive it - no open popup menu, prompt or message box, no timer, listener or pending callback
// that reaches a dead object, no JUCE object in a static (shared resources live in ui::UiResources,
// held by `uiResources`, freed with the last editor). See ~EffectorKillaAudioProcessorEditor().
class EffectorKillaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
                                          public ek::ui::PromptOwner,
                                          private juce::Timer,
                                          private juce::ChangeListener
{
public:
    explicit EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor&);
    ~EffectorKillaAudioProcessorEditor() override;

    static constexpr int kWidth = 1000;
    static constexpr int kHeight = 680;
    static constexpr int kModCollapsed = 36, kModExpanded = 104;   // ModUnit::collapsedHeight / expandedHeight
    static constexpr int kChainBottom = 664;

    // layout in base coordinates
    static juce::Rectangle<int> toolbarArea()               { return { 0, 0, kWidth, 48 }; }
    static juce::Rectangle<int> modUnitArea (bool expanded) { return { 16, 56, 752, expanded ? kModExpanded : kModCollapsed }; }
    static juce::Rectangle<int> rackArea (bool modExpanded)
    {
        const int y = modUnitArea (modExpanded).getBottom() + 8;
        return { 16, y, 752, kChainBottom - y };
    }
    static juce::Rectangle<int> masterArea()   { return { 784, 56, 200, 608 }; }
    static juce::Rectangle<int> drawerArea()   { return { 16, 56, 232, 608 }; }
    static juce::Rectangle<int> centreArea()   { return { 16, 56, 752, 608 }; }   // MOD + chain
    static juce::Rectangle<int> browserArea()  { return centreArea(); }
    static constexpr float sizeOptions[] = { 0.75f, 1.0f, 1.25f, 1.5f };          // + Auto

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;   // (listener on all children) click outside a text field = give the keyboard back

    void setUiScale (float s);          // 0 = auto (largest size option that fits ~75 % of the screen height)
    void setLibraryShown (bool shown, bool animate = true);   // FX library drawer
    bool isLibraryShown() const noexcept { return libraryOpen; }
    void setGearShown (bool shown) { setLibraryShown (shown, false); }   // old name (snapshot tool)
    void showBrowser (bool shown);
    bool isBrowserShown() const { return browser.isVisible(); }
    void setReduceMotion (bool on);     // settings menu "Reduce motion" (persisted per user)
    bool getReduceMotion() const { return ek::ui::hw::reduceMotion(); }
    void tickAll();                      // also used by the snapshot tool
    ek::ui::RackView& getRackView() noexcept { return rackView; }
    ek::ui::Toolbar& getToolbar() noexcept { return toolbar; }
    ek::ui::MasterColumn& getMaster() noexcept { return master; }      // has its own 30 Hz timer
    ek::ui::MasterColumn& getPerformer() noexcept { return master; }   // old name

    // drag from the library drawer into the chain: the drawer gets out of the way
    void dragOperationStarted (const juce::DragAndDropTarget::SourceDetails&) override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;

    // PromptOwner: modal name prompt owned by the editor (closed with it)
    void showNamePrompt (const juce::String& title, const juce::String& message, const juce::String& initialName,
                         std::function<void (const juce::String&)> onSave) override;

private:
    // window background: #04060A, deep-blue radial glow, sparse sparkles (cached)
    class Frame : public juce::Component
    {
    public:
        Frame() { setOpaque (true); setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        void invalidate() { cache.invalidate(); repaint(); }
    private:
        ek::ui::hw::BackgroundCache cache;
    };

    // patch cable while a MOD jack is dragged (content coordinates)
    class CableOverlay : public juce::Component
    {
    public:
        CableOverlay() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        int source = -1;
        juce::Point<float> from, to;
        juce::Rectangle<int> target;
    };

    // short ice-blue flash over the chain after a KILL
    class KillFlash : public juce::Component
    {
    public:
        KillFlash() { setInterceptsMouseClicks (false, false); }
        void trigger();
        void tick();
        void paint (juce::Graphics&) override;
    private:
        double startMs = -1.0e9;
        float shown = 0.0f;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showSettingsMenu();
    void showKnobMenu (int slot, int param);
    void paintKnobOverlay (juce::Graphics&, juce::Rectangle<float> face, int slot, int param);
    void modHandleDragged (int source, juce::Point<int> screenPos, bool dropped);
    void showInfoMenu();
    void showSaveDialog();
    void layoutBody();
    void updateConstrainer();
    bool fitsScreen (float scale) const;
    float autoScale() const;
    void applyScale (float s);   // setUiScale without remembering it as the user's choice
    float currentScale() const { return (float) getHeight() / (float) kHeight; }
    bool modExpanded() const;
    void tickDrawer();
    void releaseKeyboard();   // every non-text component: no keyboard focus, clicks do not grab it

    // dims MOD + chain behind the library drawer; a click on it closes the drawer
    class DrawerBackdrop : public juce::Component
    {
    public:
        std::function<void()> onClick;
        float amount = 0.0f;                 // 0..1 (fade)
        juce::Rectangle<int> dimArea, drawer;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };

    // first member: destroyed last, after every component that paints with the shared fonts / images
    ek::ui::UiResourceHolder uiResources;

    EffectorKillaAudioProcessor& processor;
    juce::Component content;
    Frame frame;
    ek::ui::Toolbar toolbar;
    ek::ui::MasterColumn master;
    EkModUnit modUnit;
    juce::Viewport rackViewport;
    KillFlash killFlash;
    ek::ui::RackView rackView;
    DrawerBackdrop backdrop;
    ek::ui::GearList gearList;
    ek::ui::PresetBrowser browser;
    CableOverlay cableOverlay;
    juce::TooltipWindow tooltips { this, 650 };
    std::unique_ptr<juce::AlertWindow> prompt;   // SAVE PRESET / SAVE UNIT PRESET (owned: closed with the editor)
    juce::ScopedMessageBox aboutBox;             // About (closed with the editor)
    bool settingSize = false;
    bool libraryOpen = false, draggingFromDrawer = false;
    float drawerPos = 0.0f;              // 0 = closed .. 1 = open (animated)
    double drawerAnimStart = 0.0;
    int lastChildCount = -1, lastModHeight = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessorEditor)
};
