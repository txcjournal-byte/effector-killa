#include "PluginEditor.h"

EffectorKillaAudioProcessorEditor::EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1100, 650);
}

void EffectorKillaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a1a10));
    g.setColour (juce::Colours::white);
    g.setFont (32.0f);
    g.drawText ("EFFECTOR KILLA", getLocalBounds(), juce::Justification::centred);
}
