#pragma once

#include "PluginProcessor.h"

class PumpLook final : public juce::LookAndFeel_V4
{
public:
    PumpLook();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// Draggable gain curve: drag points, double-click empty space to add, double-click a point to delete,
// drag a segment up/down to bend it (tension).
class CurveEditor final : public juce::Component, private juce::Timer
{
public:
    explicit CurveEditor (PumpProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> plot() const;
    juce::Point<float> toScreen (float x, float y) const;
    int hitPoint (juce::Point<float>) const;
    int hitSegment (juce::Point<float>) const;
    void push();

    PumpProcessor& proc;
    std::vector<pump::Point> pts;
    int dragPoint = -1, dragSegment = -1, hoverPoint = -1;
    float dragStartTension = 0.0f, dragStartY = 0.0f;
    float phase = 0.0f, gain = 1.0f, flash = 0.0f;
    int lastTriggers = 0;
    std::array<float, 200> history {};
    int histPos = 0;
};

class PumpPanel final : public juce::Component, private juce::Timer
{
public:
    explicit PumpPanel (PumpProcessor&);
    ~PumpPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 820, baseH = 500;

private:
    void timerCallback() override;
    PumpProcessor& proc;
    PumpLook look;
    CurveEditor curve;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label l;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    std::array<Knob, 6> knobs; // depth, smooth, crossover, threshold, mix, output

    struct Choice
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> a;
    };
    std::array<Choice, 3> choices; // rate, trigger, band
    juce::ComboBox shapeBox;
    juce::Image background;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PumpPanel)
};

class PumpEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PumpEditor (PumpProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0c0a)); }

private:
    PumpPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PumpEditor)
};
