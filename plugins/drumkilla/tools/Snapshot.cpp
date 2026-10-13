// Renders the editor to a PNG (store images). Usage: DrumKillaSnapshot out.png [presetIndex] [scale]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cmath>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    K808Processor proc;
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2) proc.presets.load (juce::String (argv[2]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (argc > 3) { const int w = juce::String (argv[3]).getIntValue(); ed->setSize (w, w * 2 / 3); }

    // feed a few 808 hits so the scope, meters and jaw show a live signal
    {
        const double sr = 48000.0;
        const int block = 512, total = (int) (sr * 2.2), hitEvery = (int) (sr * 0.43);
        juce::AudioBuffer<float> buf (juce::jmax (proc.getTotalNumInputChannels(), proc.getTotalNumOutputChannels()), block);
        juce::MidiBuffer midi;
        double phase = 0.0;
        for (int pos = 0; pos < total; pos += block)
        {
            buf.clear();
            for (int i = 0; i < block; ++i)
            {
                const int t = (pos + i) % hitEvery;
                const double ts = t / sr;
                const double f = 48.0 + 110.0 * std::exp (-ts * 40.0);
                phase += juce::MathConstants<double>::twoPi * f / sr;
                const float v = (float) (0.85 * std::sin (phase) * std::exp (-ts * 4.5));
                for (int ch = 0; ch < juce::jmin (2, buf.getNumChannels()); ++ch)
                    buf.setSample (ch, i, v);
            }
            proc.processBlock (buf, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
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
