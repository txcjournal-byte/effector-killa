#include "GlowKnob.h"
#include "Palette.h"
#include "Fonts.h"

namespace bd
{

GlowKnob::GlowKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                    const juce::String& cap, juce::Colour acc)
    : caption (cap), accent (acc)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityModeParameters (1.0, 1, 0.08, true);
    slider.setDoubleClickReturnValue (true, 0.0);   // replaced by the attachment's default
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    slider.setTooltip (caption);
}

void GlowKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (juce::jmax (12, getHeight() / 6));
    slider.setBounds (r);
}

void GlowKnob::paint (juce::Graphics& g)
{
    const int capH = juce::jmax (12, getHeight() / 6);
    auto r = getLocalBounds().removeFromBottom (capH);
    g.setColour (slider.isEnabled() ? pal::textDim : pal::textFaint);
    g.setFont (fonts::mono (juce::jlimit (8.5f, 11.5f, (float) capH * 0.8f), true));
    g.drawFittedText (caption.toUpperCase(), r, juce::Justification::centredTop, 1, 0.7f);
}

//==============================================================================
ChoiceBox::ChoiceBox (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& cap)
    : caption (cap)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        combo.addItemList (p->choices, 1);

    addAndMakeVisible (combo);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, combo);
}

void ChoiceBox::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (juce::jmax (11, getHeight() / 3));
    combo.setBounds (r);
}

void ChoiceBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromTop (juce::jmax (11, getHeight() / 3));
    g.setColour (pal::textDim);
    g.setFont (fonts::mono (juce::jlimit (8.5f, 11.0f, (float) r.getHeight() * 0.8f), true));
    g.drawFittedText (caption.toUpperCase(), r, juce::Justification::centredLeft, 1);
}

//==============================================================================
PillToggle::PillToggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& buttonText)
    : juce::Button (buttonText)
{
    setClickingTogglesState (true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
}

void PillToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;
    const bool on = getToggleState();

    if (on)
    {
        g.setColour (pal::lime.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (1.0f), corner);
        g.setColour (pal::lime);
        g.drawRoundedRectangle (r, corner, 1.2f);
    }
    else
    {
        g.setColour (pal::panel);
        g.fillRoundedRectangle (r, corner);
        g.setColour (over ? pal::cyan.withAlpha (0.6f) : pal::hairline.withAlpha (0.5f));
        g.drawRoundedRectangle (r, corner, 1.0f);
    }

    g.setColour (on ? pal::lime : pal::textDim);
    g.setFont (fonts::mono (juce::jlimit (8.5f, 11.5f, r.getHeight() * 0.55f), true));
    g.drawFittedText (getButtonText().toUpperCase(), r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

} // namespace bd
