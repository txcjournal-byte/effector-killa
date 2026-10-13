// Renders the editor to a PNG (for store images). Feeds a short drum loop through the processor first
// so the envelope display shows real data.
// Usage: SweepSnapshot out.png [presetIndex] [scale] [seconds of audio]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <random>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: SweepSnapshot out.png [preset] [scale]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    SweepProcessor proc;
    const double sr = 48000.0;
    proc.setPlayConfigDetails (2, 2, sr, 512);
    proc.prepareToPlay (sr, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;

    // kick / snare / hat pattern at 140 BPM
    std::mt19937 rng (5);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    const double step = 60.0 / 140.0 / 2.0;   // eighth notes
    size_t pos = 0;
    for (int b = 0; b < (int) (sr * (argc > 4 ? juce::String (argv[4]).getDoubleValue() : 3.0) / 512); ++b)
    {
        for (int i = 0; i < 512; ++i, ++pos)
        {
            const double t = (double) pos / sr;
            const int k = (int) (t / step);
            const double tt = t - k * step;
            const int slot = k % 8;
            double v = 0.0;
            if (slot == 0 || slot == 5) v = 0.9 * std::exp (-tt / 0.18) * std::sin (2.0 * juce::MathConstants<double>::pi * (55.0 + 90.0 * std::exp (-tt / 0.03)) * tt);
            else if (slot == 2 || slot == 6) v = 0.6 * std::exp (-tt / 0.09) * (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 190.0 * tt) + 0.7 * u (rng));
            else v = 0.25 * std::exp (-tt / 0.02) * u (rng);
            v = 0.55 * v + 0.25 * (2.0 * std::fmod (t * 55.0, 1.0) - 1.0);   // + saw bass
            buf.setSample (0, i, (float) v);
            buf.setSample (1, i, (float) v * 0.95f);
        }
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (SweepPanel::baseW * scale), (int) (SweepPanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
