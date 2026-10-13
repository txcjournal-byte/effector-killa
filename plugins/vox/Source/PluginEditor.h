#pragma once

#include "PluginProcessor.h"

class VoxLook final : public juce::LookAndFeel_V4
{
public:
    VoxLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

// One-octave keyboard that picks the KEY and lights the notes of the scale
class KeyPicker final : public juce::Component
{
public:
    explicit KeyPicker (VoxProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int keyAt (juce::Point<float>) const;
    juce::Rectangle<float> keyRect (int pc) const;
    VoxProcessor& proc;
    juce::ParameterAttachment attachment;
};

class VoxPanel final : public juce::Component, private juce::Timer
{
public:
    explicit VoxPanel (VoxProcessor&);
    ~VoxPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 820, baseH = 500;

private:
    void timerCallback() override;
    void refreshTuneButtons();

    VoxProcessor& proc;
    VoxLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    std::array<Knob, 5> knobs; // pitch, formant, retune, mix, output

    std::array<juce::TextButton, 3> tuneButtons;
    std::unique_ptr<juce::ParameterAttachment> tuneAttachment;
    KeyPicker keys;
    juce::ComboBox scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    juce::ToggleButton midiToggle { "MIDI CONTROL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiAttachment;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    float shownDetected = 0.0f, shownTarget = 0.0f, shownCents = 0.0f;
    int tuneMode = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxPanel)
};

class VoxEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VoxEditor (VoxProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff120d19)); }

private:
    VoxPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxEditor)
};
