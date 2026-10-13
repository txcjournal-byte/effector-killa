// Renders the editor to a PNG (for store images). Usage: HaloSnapshot out.png [presetIndex] [scale] [freeze 0/1]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: HaloSnapshot out.png [preset] [scale] [freeze]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    HaloProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    if (argc > 4)
        if (auto* f = proc.apvts.getParameter (IDs::freeze))
            f->setValueNotifyingHost (juce::String (argv[4]).getIntValue() != 0 ? 1.0f : 0.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (HaloPanel::baseW * scale), (int) (HaloPanel::baseH * scale));
    if (auto* he = dynamic_cast<HaloEditor*> (ed.get()))
    {
        auto& panel = he->getPanel();
        panel.setDemoLevels (0.55f, 0.8f);   // let the visualiser show some signal
        panel.advanceAnimation (1.7f);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
