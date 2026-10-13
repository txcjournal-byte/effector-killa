#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

class DustLook final : public juce::LookAndFeel_V4
{
public:
    DustLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
};

// Everything is laid out at a fixed base size and scaled as a whole.
class DustPanel final : public juce::Component, private juce::Timer
{
public:
    explicit DustPanel (DustProcessor&);
    ~DustPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 780, baseH = 440;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void drawReel (juce::Graphics&, juce::Point<float> centre, float radius, float angle) const;

    DustProcessor& proc;
    DustLook look;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::array<Knob, 9> knobs; // age, tape, wobble, vinyl, crush, tone, width, mix, output

    juce::ToggleButton follow { "NOISE FOLLOW" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    juce::Image grain;
    float reelAngle = 0.0f;
    float meterIn = 0.0f, meterOut = 0.0f;
    float crackleFlash = 0.0f;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DustPanel)
};

class DustEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DustEditor (DustProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff15110e)); }

private:
    DustPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DustEditor)
};
