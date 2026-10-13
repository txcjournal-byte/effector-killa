#pragma once

#include "PluginProcessor.h"

class AuraLook final : public juce::LookAndFeel_V4
{
public:
    AuraLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Big illuminated FREEZE button bound to the freeze parameter: click = latch, hold (> 300 ms) = momentary.
class FreezeButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FreezeButton (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isOn() const { return on; }

private:
    juce::ParameterAttachment attachment;
    bool on = false, ignoreUp = false;
    juce::uint32 downTime = 0;
};

// Space display: log-time energy picture of the reverb (dry hit, early reflections, build, tail, HF / LF decay)
class SpaceDisplay final : public juce::Component
{
public:
    explicit SpaceDisplay (AuraVerbProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    AuraVerbProcessor& proc;
    double cursorSec = 0.0;
    float activity = 0.0f;
    juce::uint32 lastTick = 0;
};

class AuraPanel final : public juce::Component, private juce::Timer
{
public:
    explicit AuraPanel (AuraVerbProcessor&);
    ~AuraPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 860, baseH = 540;

private:
    void timerCallback() override;
    void refreshSelectors();

    AuraVerbProcessor& proc;
    AuraLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    Knob decayKnob, mixKnob, preDivKnob;
    std::array<Knob, 7> row1;   // predelay, size, early/late, attack, diffusion, mod depth, mod rate
    std::array<Knob, 6> row2;   // damping, low cut, bass mult, width, duck, output

    SpaceDisplay display;
    std::array<juce::TextButton, auraverb::numModes> modeButtons;
    std::array<juce::TextButton, 2> syncButtons, colourButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment, syncAttachment, colourAttachment;
    int modeState = 1, syncState = 0, colourState = 0;

    FreezeButton freezeButton;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraPanel)
};

class AuraVerbEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AuraVerbEditor (AuraVerbProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b1020)); }

private:
    AuraPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraVerbEditor)
};
