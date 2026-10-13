#pragma once

#include "PluginProcessor.h"

class GlowLook final : public juce::LookAndFeel_V4
{
public:
    GlowLook();
    juce::Image knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// The huge illuminated GLOW knob: sunburst rays light up to the value and breathe with the output level.
class BigGlowKnob final : public juce::Slider
{
public:
    BigGlowKnob();
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    float activity = 0.0f;   // 0..1 output level, drives the bloom

private:
    juce::Image knob, rays;
};

// Illuminated push button. Mode buttons select one choice index, the auto-gain button toggles a bool.
class LitButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LitButton (juce::RangedAudioParameter&, int choiceIndex, juce::String title, juce::String sub);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
    juce::String sub;

private:
    juce::ParameterAttachment attachment;
    int index;          // >= 0: choice index, -1: toggle
    juce::String title;
    bool on = false;
};

class GlowPanel final : public juce::Component, private juce::Timer
{
public:
    explicit GlowPanel (GlowProcessor&);
    ~GlowPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 840, baseH = 520;

private:
    void timerCallback() override;

    GlowProcessor& proc;
    GlowLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    BigGlowKnob glowKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glowAttachment;
    std::array<Knob, 3> knobs; // contrast, colour, output

    std::vector<std::unique_ptr<LitButton>> modeButtons;
    std::unique_ptr<LitButton> agButton;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    float lift[3] = { 0, 0, 0 }, harm = 0, inDb = -60, outDb = -60, gr = 0, ag = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlowPanel)
};

class GlowEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GlowEditor (GlowProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff141413)); }

private:
    GlowPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlowEditor)
};
