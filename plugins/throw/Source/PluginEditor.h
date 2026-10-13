#pragma once

#include "PluginProcessor.h"

class ThrowLook final : public juce::LookAndFeel_V4
{
public:
    ThrowLook();
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

// Big illuminated button bound to a bool parameter.
// THROW: a short click latches, holding it (> 300 ms) makes it momentary. FREEZE: plain latch.
class GlowButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Style { throwButton, freezeButton };
    GlowButton (juce::RangedAudioParameter&, Style);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
    bool armed = true;   // THROW: false when the send is not in throw-only mode (drawn dimmer)

private:
    Style style;
    juce::ParameterAttachment attachment;
    bool on = false, latched = false, ignoreUp = false;
    juce::uint32 downTime = 0;
};

// Echo-tap visualiser: taps as decaying bars (L up, R down) that follow time / feedback / mode
class TapDisplay final : public juce::Component
{
public:
    explicit TapDisplay (ThrowProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    ThrowProcessor& proc;
    double cursorMs = 0.0;
    float activity = 0.0f;
    juce::uint32 lastTick = 0;
};

class ThrowPanel final : public juce::Component, private juce::Timer
{
public:
    explicit ThrowPanel (ThrowProcessor&);
    ~ThrowPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 820, baseH = 500;

private:
    void timerCallback() override;
    void refreshSelectors();

    ThrowProcessor& proc;
    ThrowLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    Knob timeKnob, divKnob, fbKnob;
    std::array<Knob, 8> knobs; // drive, wobble, locut, hicut, duck, width, mix, output

    TapDisplay taps;
    std::array<juce::TextButton, 2> syncButtons;
    std::array<juce::TextButton, 3> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> syncAttachment, modeAttachment;
    int syncState = 1, modeState = 0;

    GlowButton throwButton, freezeButton;
    juce::ToggleButton throwOnlyToggle { "THROW-ONLY SEND" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> throwOnlyAttachment;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThrowPanel)
};

class ThrowEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ThrowEditor (ThrowProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff141416)); }

private:
    ThrowPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThrowEditor)
};
