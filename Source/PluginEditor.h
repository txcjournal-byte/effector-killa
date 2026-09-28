#pragma once

#include "PluginProcessor.h"
#include "ui/cabinet/Cabinet.h"
#include "ui/tv/TvGl.h"

class EffectorKillaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::ChangeListener
{
public:
    explicit EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor&);
    ~EffectorKillaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void setUiScale (float s);
    void setOpenGl (bool on);
    ek::ui::CabinetView& getCabinet() noexcept { return cabinet; }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showMainMenu();
    void mouseDown (const juce::MouseEvent&) override;
    bool focusGrabbed = false;

    EffectorKillaAudioProcessor& processor;
    ek::ui::CabinetView cabinet;
    std::unique_ptr<ek::ui::TvGl> gl;
    float uiScale = 1.0f;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessorEditor)
};
