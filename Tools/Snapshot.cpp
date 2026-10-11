// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>
//   EffectorKillaSnapshot <outDir> --split    only the SPLIT / MERGE strip (split_unit.png, 2x)
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/rack/SplitUnit.h"

// --split: the SPLIT / MERGE strip alone, with a LOW/HIGH split on a factory preset (2x scale).
static int snapshotSplitUnit (EffectorKillaAudioProcessor& proc, const juce::File& outDir, const std::function<void (int)>& render)
{
    proc.loadFactory (9, 2);
    proc.insertSplit (1);
    proc.setSlotBranch (3, 1);
    proc.setSplitMode (ek::SplitMode::LowHigh);
    proc.setSplitFreq (180.0f);
    proc.setSplitBalance (0.42f);
    proc.setSplitLevel (1, -3.0f);
    render (20);
    ek::ui::SplitUnit unit (proc);
    unit.setBounds (0, 0, ek::ui::SplitUnit::width, ek::ui::SplitUnit::height);
    unit.tick();
    auto img = unit.createComponentSnapshot (unit.getLocalBounds(), true, 2.0f);
    auto f = outDir.getChildFile ("split_unit.png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (! os.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, os)) return 1;
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::String outArg;
    bool splitOnly = false;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--split") splitOnly = true;
        else if (! a.startsWith ("--") && outArg.isEmpty()) outArg = a;
    }
    const juce::File outDir (outArg.isNotEmpty() ? outArg : juce::File::getCurrentWorkingDirectory().getFullPathName());
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

    if (splitOnly)
        return snapshotSplitUnit (proc, outDir, render);

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
