// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>
//   EffectorKillaSnapshot --analyzer <outDir>   SPECTRAL VIEW only (analyzer.png)
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "engine/TestSignals.h"
#include "ui/rack/SpectrumOverlay.h"

// SPECTRAL VIEW: pink noise through Tone Up, BEFORE / AFTER overlay over a dark unit-sized rect.
static void renderAnalyzer (EffectorKillaAudioProcessor& proc, const juce::File& outDir)
{
    for (int s = ek::kNumSlots - 1; s >= 0; --s) proc.removeEffect (s);
    proc.insertEffect (0, ek::EffectType::ToneUp);
    const auto& info = ek::effectInfo (ek::EffectType::ToneUp);
    proc.setSlotParam (0, 0, info.params[0].fromReal (120.0f));   // low-cut 120 Hz
    proc.setSlotParam (0, 2, info.params[2].fromReal (-6.0f));    // low-mid -6 dB
    proc.setSlotParam (0, 3, info.params[3].fromReal (400.0f));   //   @ 400 Hz
    proc.setSlotParam (0, 4, info.params[4].fromReal (5.0f));     // high-mid +5 dB
    proc.setSlotParam (0, 5, info.params[5].fromReal (3000.0f));  //   @ 3 kHz
    proc.setSlotParam (0, 6, info.params[6].fromReal (6.0f));     // high +6 dB

    ek::SpectrumOverlay overlay (proc.getSlotTap());
    overlay.setSlot (0);

    const double sr = 48000.0;
    const int block = 512, blocks = 300;
    std::vector<float> L ((size_t) (block * blocks)), R ((size_t) (block * blocks));
    ek::TestSignals::generate (ek::TestSignals::Kind::PinkNoise, sr, L.data(), R.data(), (int) L.size(), 3);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        buf.copyFrom (0, 0, L.data() + b * block, block, 2.0f);
        buf.copyFrom (1, 0, R.data() + b * block, block, 2.0f);
        proc.processBlock (buf, midi);
        overlay.update();
    }

    const int w = 932, h = 140, scale = 2;
    juce::Image img (juce::Image::ARGB, w * scale, h * scale, true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale ((float) scale));
        g.fillAll (juce::Colour (0xff07090d));
        const auto unit = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (6.0f);
        g.setColour (juce::Colour (0xff0e131c));
        g.fillRoundedRectangle (unit, 6.0f);
        g.setColour (juce::Colour (0x2e9fc3ff));
        g.drawRoundedRectangle (unit, 6.0f, 1.0f);
        overlay.paint (g, unit.reduced (10.0f, 12.0f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (ek::SpectrumOverlay::afterColour());
        g.drawText (juce::String::fromUTF8 ("AFTER \xc2\xb7 TONE UP"), unit.reduced (14.0f, 10.0f), juce::Justification::topRight);
        g.setColour (ek::SpectrumOverlay::beforeColour().withAlpha (0.7f));
        g.drawText ("BEFORE", unit.reduced (14.0f, 10.0f).withTrimmedRight (110.0f), juce::Justification::topRight);
    }
    juce::PNGImageFormat png;
    auto f = outDir.getChildFile ("analyzer.png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    bool analyzerOnly = false;
    juce::String dir;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--analyzer") analyzerOnly = true;
        else dir = a;
    }
    const juce::File outDir (dir.isNotEmpty() ? dir : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    EffectorKillaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (analyzerOnly)
    {
        renderAnalyzer (proc, outDir);
        return 0;
    }
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    auto render = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (c, i, 0.3f * std::sin ((float) (b * 512 + i) * 0.05f));
            proc.processBlock (buf, midi);
        }
    };
    render (50);

    auto shot = [&] (const juce::String& name, std::function<void (EffectorKillaAudioProcessorEditor&)> setup, float imageScale = 1.0f)
    {
        proc.uiScale = 1.0f;
        auto* ed = dynamic_cast<EffectorKillaAudioProcessorEditor*> (proc.createEditor());
        if (ed == nullptr) return;
        ed->setUiScale (1.0f);
        if (setup) setup (*ed);
        for (int i = 0; i < 20; ++i) { render (2); ed->tickAll(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, imageScale);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        delete ed;
    };

    // 2x render for the shop cover (Tools/make_cover.py)
    shot ("00_cover_ui", [&] (EffectorKillaAudioProcessorEditor&) { proc.loadFactory (9, 2); }, 2.0f);
    shot ("01_factory", [&] (EffectorKillaAudioProcessorEditor&) { proc.loadFactory (9, 2); });   // CH10 Night Drive, 4 units
    shot ("02_full_rack", [&] (EffectorKillaAudioProcessorEditor&)
    {
        proc.loadFactory (1, 0);
        for (auto t : { ek::EffectType::Doubles, ek::EffectType::WideBody, ek::EffectType::RedLine })
            proc.insertEffect (ek::kNumSlots, t);
        proc.setSlotParam (1, 9, 1.0f);       // one unit powered off
        proc.setWriteProtect (0, true);
    });
    shot ("03_browser", [&] (EffectorKillaAudioProcessorEditor& e) { proc.loadFactory (2, 3); e.showBrowser (true); });
    shot ("04_empty_no_gear", [&] (EffectorKillaAudioProcessorEditor& e)
    {
        for (int s = ek::kNumSlots - 1; s >= 0; --s) proc.removeEffect (s);
        e.setGearShown (false);
    });
    shot ("05_solo_recycle", [&] (EffectorKillaAudioProcessorEditor& e)
    {
        e.setGearShown (true);
        proc.loadFactory (4, 0);
        proc.setSolo (2);
        proc.setParamNorm (ek::ParamIDs::recycle, 0.35f);
        proc.setParamNorm (ek::ParamIDs::tempoMode, 1.0f);
    });
    return 0;
}
