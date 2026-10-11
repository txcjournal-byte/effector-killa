#pragma once

#include "PluginProcessor.h"
#include "ui/rack/ControlPanel.h"
#include "ui/rack/RackView.h"
#include "ui/rack/GearList.h"
#include "ui/rack/PresetBrowser.h"

// Window: control panel on top, the rack (units top to bottom) and the gear list on the right.
// Everything is laid out at kWidth x kHeight and scaled (Auto / 80 - 150 % / drag the corner).
class EffectorKillaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
                                          private juce::Timer,
                                          private juce::ChangeListener
{
public:
    explicit EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor&);
    ~EffectorKillaAudioProcessorEditor() override;

    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 820;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void setUiScale (float s);          // 0 = auto
    void setGearShown (bool shown);
    void showBrowser (bool shown);
    bool isBrowserShown() const { return browser.isVisible(); }
    void tickAll();                      // also used by the snapshot tool
    ek::ui::RackView& getRackView() noexcept { return rackView; }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showSettingsMenu();
    void showSaveDialog();
    void layoutBody();
    float autoScale() const;
    void mouseDown (const juce::MouseEvent&) override;

    EffectorKillaAudioProcessor& processor;
    juce::Component content;
    ek::ui::ControlPanel controls;
    juce::Viewport rackViewport;
    ek::ui::RackView rackView;
    ek::ui::GearList gearList;
    ek::ui::PresetBrowser browser;
    juce::TooltipWindow tooltips { this, 650 };
    bool focusGrabbed = false, settingSize = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessorEditor)
};
