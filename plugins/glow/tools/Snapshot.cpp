// Renders the editor to a PNG (for store images).
// Usage: GlowSnapshot out.png [presetIndex] [scale] [glow 0..1]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: GlowSnapshot out.png [preset] [scale] [glow]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    GlowProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    if (argc > 4)
        if (auto* g = proc.apvts.getParameter (IDs::glow)) g->setValueNotifyingHost (juce::String (argv[4]).getFloatValue());
    // let the meters show a little activity
    proc.uiIn = 0.42f; proc.uiOut = 0.45f; proc.uiGr = 1.8f; proc.uiAg = -1.6f; proc.uiHarm = 0.55f;
    proc.uiLift[0] = 3.1f; proc.uiLift[1] = 1.4f; proc.uiLift[2] = 4.6f;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (GlowPanel::baseW * scale), (int) (GlowPanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
