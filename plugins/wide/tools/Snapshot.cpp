// Renders the editor to a PNG (for store images), after running a little synthetic stereo audio through it.
// Usage: WideSnapshot out.png [presetIndex] [scale]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

#include <random>

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: WideSnapshot out.png [preset] [scale]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    WideProcessor proc;
    const double sr = 48000.0;
    proc.prepareToPlay (sr, 512);
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;

    // a chord pad with a slightly wide image + a little noise
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    std::mt19937 rng (3);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    double t = 0.0;
    const double freqs[] = { 110.0, 220.0, 277.18, 329.63, 440.0, 659.25 };
    for (int blk = 0; blk < 100; ++blk)
    {
        for (int i = 0; i < 512; ++i, t += 1.0 / sr)
        {
            float l = 0.0f, r = 0.0f;
            for (size_t k = 0; k < 6; ++k)
            {
                const double ph = 2.0 * juce::MathConstants<double>::pi * freqs[k] * t;
                l += 0.08f * (float) std::sin (ph);
                r += 0.08f * (float) std::sin (ph + 0.22 * (double) k);
            }
            const float n = 0.02f * nd (rng);
            buf.setSample (0, i, l + n + 0.006f * nd (rng));
            buf.setSample (1, i, r + n + 0.006f * nd (rng));
        }
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (WidePanel::baseW * scale), (int) (WidePanel::baseH * scale));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
