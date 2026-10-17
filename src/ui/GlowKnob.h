/*
    TrapWin BEND DEMON
    GlowKnob.h - rotary knob (glowing arc ring, drawn by the LookAndFeel) + caption, bound to an APVTS parameter.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace bd
{

class GlowKnob final : public juce::Component
{
public:
    GlowKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
              const juce::String& caption, juce::Colour accent);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider& getSlider() noexcept { return slider; }

private:
    juce::Slider slider;
    juce::String caption;
    juce::Colour accent;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Compact labelled combo box bound to a choice parameter. */
class ChoiceBox final : public juce::Component
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::ComboBox& getCombo() noexcept { return combo; }
    void setCaptionFraction (float f)   { captionFraction = f; resized(); repaint(); }

private:
    juce::ComboBox combo;
    float captionFraction = 0.45f;
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
/** Small pill toggle (e.g. "SYNC", "QUANTIZE") bound to a bool parameter. */
class PillToggle final : public juce::Button
{
public:
    PillToggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text);

    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

} // namespace bd
