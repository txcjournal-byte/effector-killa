#pragma once

#include "PluginProcessor.h"

class WideLook final : public juce::LookAndFeel_V4
{
public:
    WideLook();
    juce::Image knob;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Lissajous vectorscope of the output (M up, S sideways), auto-scaled
class Vectorscope final : public juce::Component
{
public:
    explicit Vectorscope (WideProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    WideProcessor& proc;
    static constexpr int numPts = 1536;
    std::array<float, numPts> l {}, r {};
    float scale = 4.0f;
};

// Frequency strip: three side bands with their width, mono-below zone, draggable crossovers
class BandView final : public juce::Component
{
public:
    explicit BandView (WideProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    std::array<float, 3> energy { -90.0f, -90.0f, -90.0f };

private:
    float xFor (float hz) const;
    float hzFor (float x) const;
    int hitTarget (float x) const;          // 0..2 band, 3 low x-over, 4 high x-over
    juce::RangedAudioParameter* paramFor (int target) const;

    WideProcessor& proc;
    int dragging = -1, hover = -1;
    float dragStartValue = 0.0f;
};

class WidePanel final : public juce::Component, private juce::Timer
{
public:
    explicit WidePanel (WideProcessor&);
    ~WidePanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 860, baseH = 540;

private:
    void timerCallback() override;

    WideProcessor& proc;
    WideLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    std::array<Knob, 10> knobs; // wlow, lowx, wmid, highx, whigh, mono, haas, balance, stereoize, output

    Vectorscope scope;
    BandView bands;
    std::array<juce::TextButton, 2> sideButtons;
    std::unique_ptr<juce::ParameterAttachment> sideAttachment;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    float corr = 1.0f, peakDb[2] = { -60.0f, -60.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WidePanel)
};

class WideEditor final : public juce::AudioProcessorEditor
{
public:
    explicit WideEditor (WideProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff131318)); }

private:
    WidePanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WideEditor)
};
