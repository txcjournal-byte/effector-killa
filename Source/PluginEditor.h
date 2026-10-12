#pragma once

#include "PluginProcessor.h"
#include "ui/rack/Hardware.h"
#include "ui/rack/Toolbar.h"
#include "ui/rack/MasterColumn.h"
#include "ui/rack/RackView.h"
#include "ui/rack/GearList.h"
#include "ui/rack/PresetBrowser.h"
#include "ui/SharedResources.h"

#include "ui/rack/ModUnit.h"
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

// Window (Docs/TRAPWIN-STYLE.md), base size 1360 x 900 with the FX library, 1124 x 900 without:
//   whole window = #04060A with a deep-blue radial glow and sparse sparkles (cached), no frame;
//   HEADER 0,0,W,56 (logo, preset pill, A/B, undo/redo, settings, BYPASS);
//   FX LIBRARY 16,72,220,812 (GearList) - hidden = everything shifts left by 236 and the window narrows;
//   centre: MOD card 252,72,852,96 and the CHAIN viewport 252,180,852,704 (RackView);
//   MASTER column 1120,72,224,812 (MasterColumn).
// Everything is laid out in base coordinates and scaled (Auto / SMALL 80 % / FULL / % / drag the corner).
class EffectorKillaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
                                          private juce::Timer,
                                          private juce::ChangeListener
{
public:
    explicit EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor&);
    ~EffectorKillaAudioProcessorEditor() override;

    static constexpr int kWidth = 1360;        // with the FX library
    static constexpr int kNarrowWidth = 1124;  // library hidden
    static constexpr int kHeight = 900;
    static constexpr int kLibraryShift = 236;  // library width + gap

    // layout in base coordinates (x0 = 0 with the library, -236 without)
    static juce::Rectangle<int> toolbarArea (int w)  { return { 0, 0, w, 56 }; }
    static juce::Rectangle<int> gearArea()           { return { 16, 72, 220, 812 }; }
    static juce::Rectangle<int> modUnitArea (bool lib = true)  { return { lib ? 252 : 16, 72, 852, 96 }; }
    static juce::Rectangle<int> rackArea (bool lib = true)     { return { lib ? 252 : 16, 180, 852, 704 }; }
    static juce::Rectangle<int> masterArea (bool lib = true)   { return { lib ? 1120 : 884, 72, 224, 812 }; }
    static juce::Rectangle<int> browserArea (bool lib = true)  { return { lib ? 252 : 16, 72, 852, 812 }; }

    int getBaseWidth() const noexcept { return processor.showGear ? kWidth : kNarrowWidth; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void setUiScale (float s);          // 0 = auto
    void setGearShown (bool shown);     // also narrows / widens the window
    void showBrowser (bool shown);
    bool isBrowserShown() const { return browser.isVisible(); }
    void setReduceMotion (bool on);     // settings menu "Reduce motion" (persisted per user)
    bool getReduceMotion() const { return ek::ui::hw::reduceMotion(); }
    void tickAll();                      // also used by the snapshot tool
    ek::ui::RackView& getRackView() noexcept { return rackView; }
    ek::ui::Toolbar& getToolbar() noexcept { return toolbar; }
    ek::ui::MasterColumn& getMaster() noexcept { return master; }      // has its own 30 Hz timer
    ek::ui::MasterColumn& getPerformer() noexcept { return master; }   // old name

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
    float autoScale() const;
    float currentScale() const { return (float) getHeight() / (float) kHeight; }
    bool libraryShown() const noexcept { return processor.showGear; }
    void mouseDown (const juce::MouseEvent&) override;

    ek::ui::UiResourceHolder uiResources;   // fonts / image caches, released with the last editor (keep first)
    EffectorKillaAudioProcessor& processor;
    juce::Component content;
    Frame frame;
    ek::ui::Toolbar toolbar;
    ek::ui::MasterColumn master;
    EkModUnit modUnit;
    juce::Viewport rackViewport;
    KillFlash killFlash;
    ek::ui::RackView rackView;
    ek::ui::GearList gearList;
    ek::ui::PresetBrowser browser;
    CableOverlay cableOverlay;
    juce::TooltipWindow tooltips { this, 650 };
    bool focusGrabbed = false, settingSize = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessorEditor)
};
