// Renders the editor to a PNG (for store images).
// Usage: SubKillaSnapshot out.png [presetIndex] [scale] [slide 0/1]
// Plays note 36 (and, with slide = 1, an overlapping note 43 mid-glide) so the scope, keyboard and readouts are live.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: SubKillaSnapshot out.png [preset] [scale] [slide]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    SubKillaProcessor proc;
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    const bool slide = argc > 4 ? juce::String (argv[4]).getIntValue() != 0 : true;
    proc.prepareToPlay (48000.0, 480);

    juce::AudioBuffer<float> buf (2, 480);
    auto run = [&] (int blocks, juce::MidiBuffer midi)
    {
        for (int b = 0; b < blocks; ++b)
        {
            proc.processBlock (buf, midi);
            midi.clear();
        }
    };
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 120), 0);
    run (30, m);                                   // 300 ms of note 36
    if (slide)
    {
        juce::MidiBuffer m2;
        m2.addEvent (juce::MidiMessage::noteOn (1, 43, (juce::uint8) 110), 0);
        run (9, m2);                                // 90 ms into the slide
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (SubPanel::baseW * scale), (int) (SubPanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
