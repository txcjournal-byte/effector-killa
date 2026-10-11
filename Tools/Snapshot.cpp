// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/rack/PerformerUnit.h"
#include "ui/rack/PresetBrowser.h"

// --performer: the PERFORMER unit at 2x and the preset browser on their own (performer.png, browser.png).
static int renderPerformerSnapshots (const juce::File& outDir)
{
    EffectorKillaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    int blockIndex = 0;
    auto render = [&] (float amp)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                buf.setSample (c, i, amp * std::sin ((float) (blockIndex * 512 + i) * 0.05f));
        ++blockIndex;
        proc.processBlock (buf, midi);
    };
    auto write = [&] (juce::Component& comp, const juce::String& name, float scale)
    {
        auto img = comp.createComponentSnapshot (comp.getLocalBounds(), true, scale);
        auto f = outDir.getChildFile (name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };

    proc.loadFactory (9, 2);   // CH10 Night Drive
    const float macroPos[] = { 0.62f, 0.35f, 0.78f, 0.5f, 0.24f };
    for (int m = 0; m < ek::kNumMacros; ++m)
        proc.setParamNorm (ek::ParamIDs::macro (m), macroPos[m]);
    proc.killLevel = ek::KillLevel::R;

    {
        ek::ui::PerformerUnit unit (proc);
        // ~1 s of a -17 dBFS RMS sine so the needles settle with their ballistics
        for (int frame = 0; frame < 34; ++frame)
        {
            render (0.2f);
            unit.tick();
        }
        write (unit, "performer", 2.0f);
    }
    {
        ek::ui::PresetBrowser browser (proc);
        browser.setSize (940, 540);   // the rack area it covers in the editor
        browser.open();
        write (browser, "browser", 1.0f);
    }
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]) == "--performer")
        {
            const juce::File dir (a + 1 < argc ? juce::String (argv[a + 1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
            dir.createDirectory();
            return renderPerformerSnapshots (dir);   // EffectorKillaSnapshot --performer [outDir]
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
