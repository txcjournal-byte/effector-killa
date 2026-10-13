// Renders the editor to a PNG (for store images).
// Usage: AuraVerbSnapshot out.png [presetIndex] [scale] [freeze 0/1] [bpm]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: AuraVerbSnapshot out.png [preset] [scale] [freeze] [bpm]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    AuraVerbProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    if (argc > 4 && juce::String (argv[4]).getIntValue() != 0)
        if (auto* f = proc.apvts.getParameter (IDs::freeze)) f->setValueNotifyingHost (1.0f);
    proc.uiBpm = argc > 5 ? juce::String (argv[5]).getDoubleValue() : 140.0;
    proc.uiHostTempo = true;
    // let the display / meters show a little activity
    proc.uiWet = 0.22f; proc.uiIn = 0.35f; proc.uiDuck = 0.55f;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (AuraPanel::baseW * scale), (int) (AuraPanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
