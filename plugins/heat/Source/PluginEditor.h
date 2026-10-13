#pragma once

#include "PluginProcessor.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

class HeatLook final : public juce::LookAndFeel_V4
{
public:
    HeatLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Illuminated AUTO GAIN button bound to a bool parameter; shows the live correction while on.
class AutoGainButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    AutoGainButton (juce::RangedAudioParameter&, HeatProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isOn() const { return on; }

private:
    HeatProcessor& proc;
    juce::ParameterAttachment attachment;
    bool on = false;
};

// Model selector drawn into the baked LCD window: < MODEL > ; click the name for the full list.
class ModelSelector final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModelSelector (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::Colour colour;

private:
    void step (int dir);
    juce::ParameterAttachment attachment;
    int model = 0;
};

// Live transfer curve (left) + spectrum with draggable crossovers and the LOW KEEP zone (right).
class HeatDisplay final : public juce::Component
{
public:
    explicit HeatDisplay (HeatProcessor&);
    void paint (juce::Graphics&) override;
    void tick();
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    int focusBand = 0;

    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;

private:
    float xForFreq (float f) const;
    float freqForX (float x) const;
    int crossoverAt (float x) const;
    void paintCurve (juce::Graphics&, const heatfx::Params&);
    void paintSpectrum (juce::Graphics&, const heatfx::Params&);

    HeatProcessor& proc;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, work;
    std::vector<float> inDb, outDb;            // per display column
    juce::Rectangle<float> curveArea, specArea;
    int dragging = -1, hover = -1;
};

class HeatPanel final : public juce::Component, private juce::Timer
{
public:
    explicit HeatPanel (HeatProcessor&);
    ~HeatPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    static constexpr int baseW = 860, baseH = 540;

private:
    void timerCallback() override;
    void refreshSelectors();
    void setFocusBand (int b);

    HeatProcessor& proc;
    HeatLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    struct BandUi
    {
        Knob drive, mix, level;
        std::unique_ptr<ModelSelector> model;
        juce::TextButton solo { "S" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloA;
    };
    std::array<BandUi, heatfx::maxBands> bandUi;
    Knob punch, dynamics, lowKeep, tiltPre, tiltPost, lowCut, highCut, mix, output;

    HeatDisplay display;
    std::array<juce::TextButton, 3> bandButtons;
    std::array<juce::TextButton, heatfx::numOsChoices> osButtons;
    std::unique_ptr<juce::ParameterAttachment> bandsAttachment, osAttachment;
    int bandsState = 0, osState = heatfx::defaultOs;

    AutoGainButton autoGain;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };
    int focusBand = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatPanel)
};

class HeatEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HeatEditor (HeatProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff120e0e)); }

private:
    HeatPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatEditor)
};
