// Renders the editor to a PNG (for store images).
// Usage: HeatSnapshot out.png [presetIndex] [scale] [focusBand]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: HeatSnapshot out.png [preset] [scale]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    HeatProcessor proc;
    const double sr = 48000.0;
    proc.setPlayConfigDetails (2, 2, sr, 512);
    proc.prepareToPlay (sr, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;

    // feed a trap-ish test signal (808 + kick click + hats + a little noise) so the meters and spectrum are live
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (3);
    double ph = 0.0;
    float pink = 0.0f;
    for (int blk = 0; blk < 90; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            const int n = blk * 512 + i;
            const double t = std::fmod ((double) n / sr, 0.4286);
            const double f = 48.0 + 120.0 * std::exp (-t * 30.0);
            ph += 2.0 * juce::MathConstants<double>::pi * f / sr;
            const float env = (float) std::exp (-t * 2.2);
            float v = 0.55f * env * (float) std::sin (ph);
            pink = 0.97f * pink + 0.03f * (rng.nextFloat() * 2.0f - 1.0f);
            const double th = std::fmod ((double) n / sr, 0.1071);
            v += 0.10f * (float) std::exp (-th * 70.0) * (rng.nextFloat() * 2.0f - 1.0f) + 0.6f * pink;
            v += 0.06f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1244.5 * n / sr) * env;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v * 0.96f);
        }
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (HeatPanel::baseW * scale), (int) (HeatPanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
