#pragma once

// CONTROL UNIT - the first rack panel under the toolbar (932 x 168, DESIGN-RACK.md section 4):
// black brushed aluminium, rack ears with screws, the official TrapWin logo, chrome EFFECTOR + RACK,
// and four sections: TEMPO / SYNC, LEVELS, SIZE, TOOLBOX.

#include "Hardware.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
// Self-drawn 7-segment BPM display. Drag up / down (Shift = fine), wheel, double-click = type.
// Changing the tempo here switches to FREE (manual) tempo.
class SevenSegBpm : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SevenSegBpm (EffectorKillaAudioProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void setBpm (double bpm);
    EffectorKillaAudioProcessor& proc;
    double dragStartBpm = 120.0;
    juce::String shown;
    std::unique_ptr<juce::TextEditor> editor;
};

// Round LED that follows a predicate (beat LED).
class LedIndicator : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LedIndicator (juce::Colour c) : colour (c) {}
    std::function<float()> level;   // 0..1 brightness
    void tick();
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    float shown = -1.0f;
};

// Big level knob: name above, chicken-head knob, MIN / MAX under its ends, value under it.
class LevelKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    LevelKnob (juce::String name, juce::String minText, juce::String maxText);
    hw::HwKnob knob;
    std::function<juce::String()> valueText;
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String name, minText, maxText, shownValue;
};

class ControlUnit : public juce::Component
{
public:
    explicit ControlUnit (EffectorKillaAudioProcessor& p);

    static constexpr int width = 932;
    static constexpr int height = 168;

    std::function<void (float scale)> onSetScale;   // 0 = auto, 0.8 = SMALL, 1.0 = FULL
    std::function<void()> onToggleGear;
    std::function<bool()> isGearShown;

    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

    // section rectangles (local coords), also used by the snapshot checks
    static juce::Rectangle<float> tempoSection()   { return { 30.0f, 56.0f, 250.0f, 104.0f }; }
    static juce::Rectangle<float> levelsSection()  { return { 288.0f, 56.0f, 340.0f, 104.0f }; }
    static juce::Rectangle<float> sizeSection()    { return { 636.0f, 56.0f, 108.0f, 104.0f }; }
    static juce::Rectangle<float> toolboxSection() { return { 752.0f, 56.0f, 150.0f, 104.0f }; }

private:
    void paintStatic (juce::Graphics&);
    EffectorKillaAudioProcessor& proc;
    hw::BackgroundCache background;

    hw::HwButton tapBtn, hostBtn, freeBtn;
    SevenSegBpm bpm;
    LedIndicator beatLed;
    LevelKnob inKnob, outKnob, recycleKnob, mixKnob;
    hw::HwButton smallBtn, fullBtn, autoBtn;
    hw::HwButton gearBtn, autoGainBtn, phoneBtn;
};
} // namespace ek::ui
