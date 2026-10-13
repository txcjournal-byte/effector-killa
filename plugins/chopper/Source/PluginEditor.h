#pragma once

#include "PluginProcessor.h"

class ChopperLook final : public juce::LookAndFeel_V4
{
public:
    ChopperLook();
    juce::Image knob;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// 16-step grid: click cycles a step's effect, right-click / drag paints the palette effect.
// Also draws the readout strip (step, effect, rate, tempo) and the playhead LEDs.
class StepGrid final : public juce::Component
{
public:
    StepGrid (ChopperProcessor&, std::function<chopper::Fx()> paletteFx);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void tick();

private:
    int cellAt (juce::Point<float>) const;
    juce::Rectangle<float> cellRect (int i) const;   // local coordinates
    void paintCell (int i, chopper::Fx fx);

    ChopperProcessor& proc;
    std::function<chopper::Fx()> palette;
    int downCell = -1, lastPainted = -1, hover = -1;
    bool painting = false;
    int shownStep = -1, shownVersion = -1;
    float flash[chopper::numSteps] {};
};

// One effect swatch of the paint palette
class FxButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    FxButton (chopper::Fx, std::function<void (chopper::Fx)> onPick);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    bool selected = false;
    const chopper::Fx fx;

private:
    std::function<void (chopper::Fx)> pick;
};

// Big illuminated HOLD button: momentary while pressed, right-click latches
class HoldButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HoldButton (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isOn() const { return on; }
    bool latched = false;

private:
    juce::ParameterAttachment attachment;
    bool on = false, momentary = false;
};

class ChopperPanel final : public juce::Component, private juce::Timer
{
public:
    explicit ChopperPanel (ChopperProcessor&);
    ~ChopperPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 820, baseH = 500;

private:
    void timerCallback() override;
    void selectFx (chopper::Fx);
    void refreshRate();
    void randomise();

    ChopperProcessor& proc;
    ChopperLook look;
    juce::Image background;
    chopper::Fx paletteFx = chopper::Fx::repeat;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    std::array<Knob, 4> knobs; // size, swing, mix, output

    StepGrid grid;
    std::vector<std::unique_ptr<FxButton>> fxButtons;
    std::array<juce::TextButton, chopper::numRates> rateButtons;
    std::unique_ptr<juce::ParameterAttachment> rateAttachment;
    int rateState = 1;

    HoldButton holdButton;
    juce::TextButton randomButton { "RANDOM" }, clearButton { "CLEAR" };
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopperPanel)
};

class ChopperEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ChopperEditor (ChopperProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff131318)); }

private:
    ChopperPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChopperEditor)
};
