#pragma once

#include "PluginProcessor.h"

class SweepLook final : public juce::LookAndFeel_V4
{
public:
    SweepLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Animated filter response: live curve, sweep range, cutoff markers, modulation meters
class ResponseDisplay final : public juce::Component
{
public:
    explicit ResponseDisplay (SweepProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    SweepProcessor& proc;
    float shownA = 1000.0f, shownB = 1000.0f;
};

// Drawable 16-step pattern (also shows the LFO waveform and playhead)
class StepEditor final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepEditor (SweepProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void setFromMouse (juce::Point<float>, bool zero);
    int stepAt (float x) const;
    SweepProcessor& proc;
    std::array<juce::RangedAudioParameter*, sweepfx::numSteps> params {};
    std::array<bool, sweepfx::numSteps> inGesture {};
    int lastStep = -1;
};

class SweepPanel final : public juce::Component, private juce::Timer
{
public:
    explicit SweepPanel (SweepProcessor&);
    ~SweepPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 860, baseH = 540;

private:
    void timerCallback() override;
    void refreshSelectors();

    SweepProcessor& proc;
    SweepLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
        juce::Point<float> centre;
        bool big = false;
    };
    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* cutA = nullptr; Knob* cutB = nullptr; Knob* divKnob = nullptr; Knob* rateKnob = nullptr;
    std::vector<Knob*> bKnobs;
    Knob& addKnob (const char* id, juce::Point<float> centre, bool big, const char* tip);

    ResponseDisplay display;
    StepEditor steps;

    juce::ComboBox typeA, typeB;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAAtt, typeBAtt;
    juce::TextButton bOnButton { "ON" }, syncButton { "SYNC" }, retrigButton { "RETRIG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bOnAtt, syncAtt, retrigAtt;

    std::array<juce::TextButton, sweepfx::numShapes> shapeButtons;
    std::array<juce::TextButton, 2> routingButtons, triggerButtons;
    std::unique_ptr<juce::ParameterAttachment> shapeAtt, routingAtt, triggerAtt;
    int shapeState = 0, routingState = 0, triggerState = 0;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SweepPanel)
};

class SweepEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SweepEditor (SweepProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff131515)); }

private:
    SweepPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SweepEditor)
};
