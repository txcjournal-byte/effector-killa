#pragma once

#include "PluginProcessor.h"

class SubLook final : public juce::LookAndFeel_V4
{
public:
    SubLook();
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

// Four illuminated glass buttons bound to the saturation-type choice parameter
class SatSelector final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SatSelector (juce::RangedAudioParameter&, SubKillaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int getSelected() const { return selected; }
    static juce::Rectangle<float> cell (int i) { return { 74.0f * (float) i, 0.0f, 68.0f, 36.0f }; }

private:
    SubKillaProcessor& proc;
    juce::ParameterAttachment attachment;
    int selected = 0;
};

// Live output oscilloscope (zero-crossing triggered) with readouts
class Scope final : public juce::Component
{
public:
    explicit Scope (SubKillaProcessor&);
    void paint (juce::Graphics&) override;

private:
    SubKillaProcessor& proc;
    std::array<float, 2048> snap {};
};

// Clickable 4-octave keyboard (C1..B4); shows held notes and the current (gliding) pitch
class MiniKeyboard final : public juce::Component
{
public:
    explicit MiniKeyboard (SubKillaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    static constexpr int lowNote = 24, numWhite = 28;

private:
    int noteAt (juce::Point<float>) const;
    juce::Rectangle<float> keyRect (int note) const;
    float centreX (float note) const;
    static bool isBlack (int note) { const int k = note % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
    float velocityAt (juce::Point<float>) const;

    SubKillaProcessor& proc;
    int mouseNote = -1;
    static constexpr float railH = 9.0f;
};

class SubPanel final : public juce::Component, private juce::Timer
{
public:
    explicit SubPanel (SubKillaProcessor&);
    ~SubPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 820, baseH = 540;

private:
    void timerCallback() override;
    void refreshModeButtons();

    SubKillaProcessor& proc;
    SubLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    Knob glideKnob;
    std::array<Knob, 6> bodyKnobs;   // punch, punch time, click, sub, tune, fine
    std::array<Knob, 11> deckKnobs;  // cutoff, reso, env, drive, mix, attack, decay, sustain, release, width, output

    Scope scope;
    SatSelector satSelector;
    MiniKeyboard keyboard;
    std::array<juce::TextButton, 3> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    int modeState = 1;
    juce::ToggleButton limiterToggle { "LIMITER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttachment;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubPanel)
};

class SubKillaEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SubKillaEditor (SubKillaProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0e0f11)); }

private:
    SubPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubKillaEditor)
};
