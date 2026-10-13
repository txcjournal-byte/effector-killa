#pragma once

#include "PluginProcessor.h"

class SnapLook final : public juce::LookAndFeel_V4
{
public:
    SnapLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Illuminated latch button bound to a bool parameter
class GlowButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    GlowButton (juce::RangedAudioParameter&, juce::String title, juce::Colour hue);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
    juce::String subOn, subOff;
    bool warn = false;   // clipper working: sub label flashes

private:
    juce::ParameterAttachment attachment;
    juce::String title;
    juce::Colour hue;
    bool on = false;
};

// Scrolling envelope display: original (grey) vs shaped (accent) envelope + the applied gain curve
class SnapDisplay final : public juce::Component
{
public:
    explicit SnapDisplay (SnapProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    SnapProcessor& proc;
    static constexpr int points = 296;
    std::array<float, points> in {}, out {}, gain {};
    float activity = 0.0f, clipFlash = 0.0f;
};

class SnapPanel final : public juce::Component, private juce::Timer
{
public:
    explicit SnapPanel (SnapProcessor&);
    ~SnapPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 840, baseH = 520;

private:
    void timerCallback() override;
    void refreshSelectors();

    SnapProcessor& proc;
    SnapLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    Knob attackKnob, sustainKnob;
    std::array<Knob, 5> knobs; // split, speed, ceiling, mix, output

    SnapDisplay display;
    std::array<juce::TextButton, 3> bandButtons;
    std::unique_ptr<juce::ParameterAttachment> bandAttachment;
    int bandState = 0;

    GlowButton lookButton, clipButton;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapPanel)
};

class SnapEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SnapEditor (SnapProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff141416)); }

private:
    SnapPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapEditor)
};
