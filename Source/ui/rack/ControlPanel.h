#pragma once

// Top of the window: logo, preset section (browse / prev / next / save / favourite / A-B / undo),
// tempo (HOST or MANUAL, tap, drag or type the BPM), levels (input, recycle, mix, output + meters),
// KILL with its rating and BOOTLEG; under it the macro strip with the global switches.

#include "Widgets.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class BpmDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BpmDisplay (EffectorKillaAudioProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    bool manual() const;
    void setBpm (double bpm);
    EffectorKillaAudioProcessor& proc;
    double dragStartBpm = 120.0;
    juce::String shown;
    std::unique_ptr<juce::TextEditor> editor;
};

class PresetDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetDisplay (EffectorKillaAudioProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void refresh() { repaint(); }

private:
    EffectorKillaAudioProcessor& proc;
};

class ControlPanel : public juce::Component
{
public:
    explicit ControlPanel (EffectorKillaAudioProcessor& p);

    void tick();
    void refresh();
    void showMessage (const juce::String& m);
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onBrowse, onSave, onToggleGear, onSettings;
    std::function<bool()> isGearShown, isBrowserShown;

    static constexpr int headerHeight = 140;
    static constexpr int stripHeight = 74;

private:
    EffectorKillaAudioProcessor& proc;

    PresetDisplay presetDisplay;
    TwButton prevBtn, nextBtn, browseBtn, saveBtn, favBtn, aBtn, bBtn, undoBtn, redoBtn;
    TwButton hostBtn, manualBtn, tapBtn;
    BpmDisplay bpm;
    LabeledKnob inKnob, recycleKnob, mixKnob, outKnob;
    PeakMeter inMeter, outMeter;
    TwButton killBtn, pgBtn, rBtn, unratedBtn, bootlegBtn;
    juce::OwnedArray<LabeledKnob> macros;
    TwButton autoGainBtn, phoneBtn, compareBtn, gearBtn, settingsBtn;

    juce::String message;
    juce::uint32 messageUntil = 0;
    juce::uint32 compareDownAt = 0;
    bool compareWasOn = false;
};

} // namespace ek::ui
