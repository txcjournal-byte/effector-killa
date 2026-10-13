#pragma once

#include "PluginProcessor.h"

class GhostLook final : public juce::LookAndFeel_V4
{
public:
    GhostLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Big illuminated FREEZE button bound to a bool parameter (latch)
class FreezeButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FreezeButton (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }

private:
    juce::ParameterAttachment attachment;
    bool on = false;
};

// Echo visualiser: draws what the current mode does to a hit (taps, pitch steps, reversed grains, smear, patterns)
class EchoDisplay final : public juce::Component
{
public:
    explicit EchoDisplay (GhostDelayProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    GhostDelayProcessor& proc;
    double cursorMs = 0.0, clockMs = 0.0;
    float activity = 0.0f;
    juce::uint32 lastTick = 0;
};

class GhostPanel final : public juce::Component, private juce::Timer
{
public:
    explicit GhostPanel (GhostDelayProcessor&);
    ~GhostPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 860, baseH = 540;

private:
    void timerCallback() override;
    void refreshSelectors();

    GhostDelayProcessor& proc;
    GhostLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    Knob timeKnob, divKnob, fbKnob;
    std::array<Knob, 9> knobs; // ratio, locut, hicut, depth, rate, age, duck, mix, output

    EchoDisplay display;

    // a row of mutually exclusive buttons bound to one choice / bool parameter
    struct Selector
    {
        std::vector<std::unique_ptr<juce::TextButton>> buttons;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        int state = 0;
    };
    Selector modeSel, syncSel, pitchSel, patternSel;
    void setupSelector (Selector&, const char* paramId, const juce::StringArray& names, const juce::StringArray& tips, bool invertBool = false);

    FreezeButton freezeButton;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GhostPanel)
};

class GhostDelayEditor final : public juce::AudioProcessorEditor
{
public:
    explicit GhostDelayEditor (GhostDelayProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff131516)); }

private:
    GhostPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GhostDelayEditor)
};
