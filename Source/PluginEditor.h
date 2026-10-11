#pragma once

#include "PluginProcessor.h"
#include "ui/rack/Hardware.h"
#include "ui/rack/Toolbar.h"
#include "ui/rack/ControlUnit.h"
#include "ui/rack/ControlPanel.h"
#include "ui/rack/RackView.h"
#include "ui/rack/GearList.h"
#include "ui/rack/PresetBrowser.h"

#include "ui/rack/PerformerUnit.h"

// The MOD unit is built as a standalone component. Until it is merged the editor shows a blank
// placeholder panel (ControlPanel.h) in its place.
#if __has_include("ui/rack/ModUnit.h")
 #include "ui/rack/ModUnit.h"
 using EkModUnit = ek::ui::ModUnit;
#else
 using EkModUnit = ek::ui::ModFallback;
#endif

// Window (DESIGN-RACK.md section 2), base size 1280 x 900 with the gear list, 980 x 900 without:
//   toolbar (40 px) on top, walnut frame, dark rack shaft with rails, control unit, performer unit,
//   MOD unit, the rack viewport and the gear list on the right.
// Everything is laid out in base coordinates and scaled (Auto / SMALL 80 % / FULL / % / drag the corner).
class EffectorKillaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
                                          private juce::Timer,
                                          private juce::ChangeListener
{
public:
    explicit EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor&);
    ~EffectorKillaAudioProcessorEditor() override;

    static constexpr int kWidth = 1280;        // with the gear list
    static constexpr int kNarrowWidth = 980;   // SHOW GEAR off
    static constexpr int kHeight = 900;

    // layout in base coordinates
    static juce::Rectangle<int> toolbarArea (int w)  { return { 0, 0, w, 40 }; }
    static juce::Rectangle<int> shaftArea()          { return { 20, 58, 940, 824 }; }
    static juce::Rectangle<int> controlUnitArea()    { return { 24, 62, 932, 168 }; }
    static juce::Rectangle<int> performerArea()      { return { 24, 234, 932, 104 }; }
    static juce::Rectangle<int> modUnitArea()        { return { 24, 342, 932, 96 }; }
    static juce::Rectangle<int> rackArea()           { return { 20, 442, 940, 440 }; }
    static juce::Rectangle<int> gearArea()           { return { 980, 58, 280, 824 }; }

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
    ek::ui::ControlUnit& getControlUnit() noexcept { return controlUnit; }
    ek::ui::PerformerUnit& getPerformer() noexcept { return performer; }  // has its own 30 Hz timer

private:
    // walnut frame + rack shaft with rails (cached)
    class Frame : public juce::Component
    {
    public:
        explicit Frame (EffectorKillaAudioProcessor& p) : proc (p) { setOpaque (true); setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        void invalidate() { cache.invalidate(); repaint(); }
    private:
        EffectorKillaAudioProcessor& proc;
        ek::ui::hw::BackgroundCache cache;
    };

    // short ice-blue flash over the rack after a KILL (DESIGN-RACK.md section 9)
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
    void showInfoMenu();
    void showSaveDialog();
    void layoutBody();
    void updateConstrainer();
    float autoScale() const;
    float currentScale() const { return (float) getHeight() / (float) kHeight; }
    void mouseDown (const juce::MouseEvent&) override;

    EffectorKillaAudioProcessor& processor;
    juce::Component content;
    Frame frame;
    ek::ui::Toolbar toolbar;
    ek::ui::ControlUnit controlUnit;
    ek::ui::PerformerUnit performer;
    EkModUnit modUnit;
    juce::Viewport rackViewport;
    KillFlash killFlash;
    ek::ui::RackView rackView;
    ek::ui::GearList gearList;
    ek::ui::PresetBrowser browser;
    juce::TooltipWindow tooltips { this, 650 };
    bool focusGrabbed = false, settingSize = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessorEditor)
};
