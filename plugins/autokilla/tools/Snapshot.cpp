// Renders the editor to a PNG (for store images). A synthetic sung phrase is run through the plugin first so the
// pitch graph, note display and auto key have something to show.
// Usage: AutoKillaSnapshot out.png [presetIndex] [scale] [autoKey 0/1] [mode 0=live 1=mix]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

static void sing (AutoKillaProcessor& proc, double sr)
{
    // C minor-ish phrase, sung a little off with scoops and a natural vibrato
    const int notes[] = { 60, 63, 67, 65, 63, 62, 60, 67, 68, 67, 65, 63, 62, 63 };
    const double beats[] = { 1, 0.5, 1.5, 0.5, 0.5, 1, 1, 1, 0.5, 0.5, 0.5, 0.5, 1, 2 };
    std::vector<double> f0;
    for (int r = 0; r < 2; ++r)
        for (size_t i = 0; i < std::size (notes); ++i)
        {
            const size_t len = (size_t) (beats[i] * 0.5 * sr);
            const double target = 440.0 * std::pow (2.0, (notes[i] - 69) / 12.0);
            const double offC = ((int) (i * 37 % 7) - 3) * 9.0;
            for (size_t k = 0; k < len; ++k)
            {
                const double t = (double) k / sr;
                const double scoop = -70.0 * std::exp (-t / 0.05);
                const double vib = 22.0 * std::sin (2 * juce::MathConstants<double>::pi * 5.3 * t) * juce::jlimit (0.0, 1.0, (t - 0.25) / 0.3);
                f0.push_back (k + (size_t) (0.02 * sr) < len ? target * std::pow (2.0, (offC + scoop + vib) / 1200.0) : 0.0);
            }
        }
    std::vector<float> x (f0.size());
    double ph = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        if (f0[i] <= 0) { x[i] = 0; continue; }
        ph += f0[i] / sr; if (ph > 1) ph -= 1;
        double v = 0;
        for (int h = 1; h < 30; ++h) v += std::sin (2 * juce::MathConstants<double>::pi * h * ph) / h * (h < 6 ? 1.0 : 0.5);
        x[i] = (float) (0.15 * v);
    }
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos + 512 <= x.size() * 96 / 100; pos += 512)
    {
        for (int c = 0; c < 2; ++c) buf.copyFrom (c, 0, x.data() + pos, 512);
        proc.processBlock (buf, midi);
    }
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: AutoKillaSnapshot out.png [preset] [scale] [autoKey] [mode]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    AutoKillaProcessor proc;
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 2.0f;
    auto set = [&] (const char* id, float plain) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (plain)); };
    set (IDs::key, 0.0f);
    set (IDs::scale, 1.0f);
    if (argc > 4) set (IDs::autoKey, (float) juce::String (argv[4]).getIntValue());
    if (argc > 5) set (IDs::mode, (float) juce::String (argv[5]).getIntValue());
    proc.prepareToPlay (48000.0, 512);
    sing (proc, 48000.0);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (AutoKillaPanel::baseW * scale), (int) (AutoKillaPanel::baseH * scale));
    // timers do not run here: pump the display refresh by hand
    if (auto* panel = dynamic_cast<AutoKillaPanel*> (ed->getChildComponent (0)))
        for (int i = 0; i < 150; ++i) panel->refreshNow();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s %dx%d\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
