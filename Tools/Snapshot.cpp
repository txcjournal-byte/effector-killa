// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>
#include "PluginProcessor.h"
#include "PluginEditor.h"

// --frame: window frame, toolbar and control unit (DESIGN-RACK.md sections 2 - 4):
// frame_default.png (1280 x 900), frame_nogear.png (980 x 900), frame_small_browser.png (80 %),
// frame_control_2x.png (toolbar + control unit at 2x for close inspection).
static int renderFrameSnapshots (const juce::File& outDir)
{
    EffectorKillaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    int blockIndex = 0;
    auto render = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b, ++blockIndex)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (c, i, 0.25f * std::sin ((float) (blockIndex * 512 + i) * 0.05f));
            proc.processBlock (buf, midi);
        }
    };
    render (50);

    auto write = [&] (const juce::Image& img, const juce::String& name)
    {
        auto f = outDir.getChildFile (name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    auto shot = [&] (const juce::String& name, std::function<void (EffectorKillaAudioProcessorEditor&)> setup,
                     std::function<juce::Rectangle<int> (EffectorKillaAudioProcessorEditor&)> area = {}, float imageScale = 1.0f)
    {
        proc.uiScale = 1.0f;
        std::unique_ptr<EffectorKillaAudioProcessorEditor> ed (dynamic_cast<EffectorKillaAudioProcessorEditor*> (proc.createEditor()));
        if (ed == nullptr) return;
        ed->setUiScale (1.0f);
        if (setup) setup (*ed);
        for (int i = 0; i < 34; ++i) { render (2); ed->tickAll(); ed->getPerformer().tick(); }
        write (ed->createComponentSnapshot (area ? area (*ed) : ed->getLocalBounds(), true, imageScale), name);
    };

    proc.loadFactory (9, 2);   // CH10 Night Drive
    shot ("frame_default", [&] (EffectorKillaAudioProcessorEditor& e) { e.setGearShown (true); });
    shot ("frame_nogear", [&] (EffectorKillaAudioProcessorEditor& e)
    {
        e.setGearShown (false);
        proc.setParamNorm (ek::ParamIDs::recycle, 0.6f);
        proc.setParamNorm (ek::ParamIDs::tempoMode, 1.0f);
        proc.setParamNorm (ek::ParamIDs::pocketTv, 1.0f);
        proc.setParamNorm (ek::ParamIDs::offAir, 1.0f);
    });
    shot ("frame_control_2x", [&] (EffectorKillaAudioProcessorEditor& e)
    {
        e.setGearShown (true);
        proc.setParamNorm (ek::ParamIDs::recycle, 0.6f);
        proc.setParamNorm (ek::ParamIDs::tempoMode, 1.0f);
        proc.setParamNorm (ek::ParamIDs::pocketTv, 0.0f);
        proc.setParamNorm (ek::ParamIDs::offAir, 0.0f);
    }, [] (EffectorKillaAudioProcessorEditor&) { return juce::Rectangle<int> (0, 0, 980, 236); }, 2.0f);
    shot ("frame_small_browser", [&] (EffectorKillaAudioProcessorEditor& e)
    {
        e.setGearShown (true);
        proc.setParamNorm (ek::ParamIDs::recycle, 0.0f);
        proc.setParamNorm (ek::ParamIDs::tempoMode, 0.0f);
        e.setUiScale (0.8f);
        e.showBrowser (true);
    });
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]) == "--frame")
        {
            const juce::File dir (a + 1 < argc ? juce::String (argv[a + 1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
            dir.createDirectory();
            return renderFrameSnapshots (dir);   // EffectorKillaSnapshot --frame [outDir]
        }
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    EffectorKillaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
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
        for (int i = 0; i < 20; ++i) { render (2); ed->tickAll(); ed->getPerformer().tick(); }
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
