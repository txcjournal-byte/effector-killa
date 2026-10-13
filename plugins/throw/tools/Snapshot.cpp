// Renders the editor to a PNG (for store images).
// Usage: ThrowSnapshot out.png [presetIndex] [scale] [throw 0/1] [bpm]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: ThrowSnapshot out.png [preset] [scale] [throw] [bpm]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    ThrowProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    if (argc > 4 && juce::String (argv[4]).getIntValue() != 0)
        if (auto* t = proc.apvts.getParameter (IDs::throwOn)) t->setValueNotifyingHost (1.0f);
    proc.uiBpm = argc > 5 ? juce::String (argv[5]).getDoubleValue() : 140.0;
    proc.uiHostTempo = true;
    // let the display show a little activity
    proc.uiWet = 0.25f; proc.uiInput = 0.2f; proc.uiDuck = 0.6f;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (ThrowPanel::baseW * scale), (int) (ThrowPanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
