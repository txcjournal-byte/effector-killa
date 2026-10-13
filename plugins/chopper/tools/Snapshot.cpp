// Renders the editor to a PNG (for store images).
// Usage: ChopperSnapshot out.png [presetIndex] [scale] [hold 0/1] [playStep 0-15] [bpm]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: ChopperSnapshot out.png [preset] [scale] [hold] [step] [bpm]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    if (juce::String (argv[1]) == "--state-test")
    {
        // plugin-level state round trip: pattern + parameters survive save / restore
        ChopperProcessor a, b;
        a.setCurrentProgram (29);
        a.setStep (0, chopper::Fx::halfSpeed);
        if (auto* s = a.apvts.getParameter (IDs::swing)) s->setValueNotifyingHost (0.37f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool ok = a.getPattern() == b.getPattern() && b.getStep (0) == chopper::Fx::halfSpeed
                        && std::abs (b.apvts.getRawParameterValue (IDs::swing)->load() - a.apvts.getRawParameterValue (IDs::swing)->load()) < 1e-4f
                        && (int) b.apvts.getRawParameterValue (IDs::rate)->load() == (int) a.apvts.getRawParameterValue (IDs::rate)->load()
                        && b.getCurrentProgram() == 29;
        std::printf ("state test: %s (%s)\n", ok ? "PASS" : "FAIL", chopper::patternToString (b.getPattern()).c_str());
        return ok ? 0 : 1;
    }
    ChopperProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    if (argc > 4 && juce::String (argv[4]).getIntValue() != 0)
        if (auto* t = proc.apvts.getParameter (IDs::hold)) t->setValueNotifyingHost (1.0f);
    const int step = argc > 5 ? juce::String (argv[5]).getIntValue() : 4;
    proc.uiStep = step;
    proc.uiFx = (int) proc.getStep (step);
    proc.uiPhase = 0.45;
    proc.uiBpm = argc > 6 ? juce::String (argv[6]).getDoubleValue() : 140.0;
    proc.uiHostPlaying = true;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (ChopperPanel::baseW * scale), (int) (ChopperPanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
