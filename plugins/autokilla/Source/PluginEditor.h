#pragma once

#include "PluginProcessor.h"

class AKLook final : public juce::LookAndFeel_V4
{
public:
    AKLook();
    juce::Image knobBig, knobSmall;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// Scrolling pitch graph: detected (white) vs corrected (blue) over the last 4 s, semitone grid with the allowed notes lit
class PitchGraph final : public juce::Component
{
public:
    explicit PitchGraph (AutoKillaProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    AutoKillaProcessor& proc;
    static constexpr int numFrames = 400;
    std::array<akill::Engine::Frame, numFrames> frames {};
    int count = 0;
    float viewCentre = 57.0f;
};

// Big note name + cents meter
class NoteDisplay final : public juce::Component
{
public:
    explicit NoteDisplay (AutoKillaProcessor&);
    void paint (juce::Graphics&) override;
    void tick();

private:
    AutoKillaProcessor& proc;
    int shownNote = -1;
    float shownCents = 0.0f, inTune = 0.0f;
    juce::uint32 lastVoiced = 0;
};

// One-octave keyboard: lit keys = allowed notes. Clicking edits the custom scale.
class NoteKeyboard final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit NoteKeyboard (AutoKillaProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void tick();

private:
    int keyAt (juce::Point<float>) const;
    juce::Rectangle<float> keyRect (int pc) const;
    static bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    AutoKillaProcessor& proc;
    int mask = 0, target = -1, midiMask = 0, hover = -1;
};

class AutoKillaPanel final : public juce::Component, private juce::Timer
{
public:
    explicit AutoKillaPanel (AutoKillaProcessor&);
    ~AutoKillaPanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 860, baseH = 520;
    void refreshNow() { timerCallback(); }

private:
    void timerCallback() override;
    void refreshMode();

    AutoKillaProcessor& proc;
    AKLook look;
    juce::Image background;

    struct Knob
    {
        juce::Slider s { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a;
    };
    Knob retuneKnob;
    std::array<Knob, 8> knobs; // humanize, threshold, vibrato, vib rate, transpose, throat, mix, output

    PitchGraph graph;
    NoteDisplay noteDisplay;
    NoteKeyboard keyboard;

    std::array<juce::TextButton, 2> modeButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    int modeState = 1;

    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment, scaleAttachment;
    juce::TextButton autoKeyButton { "AUTO KEY" }, setKeyButton { "--" };
    juce::TextButton formantButton { "FORMANT" }, midiButton { "MIDI NOTES" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoKeyAttachment, formantAttachment, midiAttachment;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoKillaPanel)
};

class AutoKillaEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AutoKillaEditor (AutoKillaProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff121419)); }

private:
    AutoKillaPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoKillaEditor)
};
