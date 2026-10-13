#pragma once

#include "PluginProcessor.h"

class HaloLook final : public juce::LookAndFeel_V4
{
public:
    HaloLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int, int) override;
};

class HaloPanel final : public juce::Component, private juce::Timer
{
public:
    explicit HaloPanel (HaloProcessor&);
    ~HaloPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 820, baseH = 500;

    // used by the snapshot tool
    void setDemoLevels (float in, float wet) { lvIn = in; lvWet = wet; demo = true; }
    void advanceAnimation (float seconds) { phase += seconds; }

private:
    void timerCallback() override;
    void paintVisualiser (juce::Graphics&);
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    HaloProcessor& proc;
    HaloLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
        juce::Point<float> centre;
        bool big = false;
    };
    std::array<Knob, 11> knobs;

    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 700 };

    struct Particle { float angle, radius, speed, size, twinkle; };
    std::array<Particle, 56> particles {};
    float phase = 0.0f, lvIn = 0.0f, lvWet = 0.0f, freezeGlow = 0.0f;
    bool demo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaloPanel)
};

class HaloEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HaloEditor (HaloProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff070e13)); }
    HaloPanel& getPanel() { return panel; }

private:
    HaloPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaloEditor)
};
