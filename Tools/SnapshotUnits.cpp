// Renders the rack units and the gear list on their own (no editor) to PNG files - checks the
// hardware units, side strip, live data and gear miniatures without a DAW.
//   EffectorKillaSnapshot --units <outDir>
// Writes units_rack.png (8 units, one powered off, one locked), units_solo.png, units_gear.png.
#include "PluginProcessor.h"
#include "ui/rack/RackView.h"
#include "ui/rack/GearList.h"

namespace
{
void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}
} // namespace

int runUnitsSnapshot (const juce::File& outDir)
{
    EffectorKillaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random rnd (7);
    int pos = 0;
    auto render = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i, ++pos)
            {
                // a little "beat": sine bass + noise hits every 12000 samples, slightly different L / R
                const float env = std::exp (-(float) (pos % 12000) / 1800.0f);
                const float n = (rnd.nextFloat() * 2.0f - 1.0f) * env * 0.5f;
                const float s = 0.25f * std::sin ((float) pos * 0.012f);
                buf.setSample (0, i, s + n);
                buf.setSample (1, i, 0.8f * s + n * 0.6f + 0.1f * std::sin ((float) pos * 0.031f));
            }
            proc.processBlock (buf, midi);
        }
    };

    // ---- full rack: 8 units, one powered off, one locked -------------------------------------------
    proc.loadFactory (1, 0);   // CH02 Vox Dei (5 units)
    for (auto t : { ek::EffectType::Doubles, ek::EffectType::WideBody, ek::EffectType::RedLine })
        proc.insertEffect (ek::kNumSlots, t);
    proc.setSlotParam (3, 9, 1.0f);   // Ad-Lib Throw powered off (tails keep running)
    proc.setWriteProtect (1, true);   // Squeeze locked
    render (60);

    auto shoot = [&] (ek::ui::RackView& rack, const juce::String& name)
    {
        rack.refresh();
        rack.setSize (940, rack.getIdealHeight());
        for (int i = 0; i < 24; ++i) { render (2); rack.tick(); }
        writePng (rack.createComponentSnapshot (rack.getLocalBounds(), true, 1.0f), outDir.getChildFile (name));
    };

    {
        ek::ui::RackView rack (proc);
        shoot (rack, "units_rack.png");
    }

    // ---- SOLO: the third unit is soloed, the others dim -------------------------------------------
    {
        proc.setSolo (2);
        ek::ui::RackView rack (proc);
        shoot (rack, "units_solo.png");
        proc.setSolo (-1);
    }

    // ---- gear list -------------------------------------------------------------------------------
    {
        ek::ui::GearList gear (proc);
        gear.setSize (ek::ui::GearList::preferredWidth, 824);
        writePng (gear.createComponentSnapshot (gear.getLocalBounds(), true, 1.0f), outDir.getChildFile ("units_gear.png"));
    }

    // ---- empty rack (drop zone) ------------------------------------------------------------------
    {
        for (int s = ek::kNumSlots - 1; s >= 0; --s) { proc.setWriteProtect (s, false); proc.removeEffect (s); }
        ek::ui::RackView rack (proc);
        rack.setSize (940, 300);
        rack.tick();
        writePng (rack.createComponentSnapshot (rack.getLocalBounds(), true, 1.0f), outDir.getChildFile ("units_empty.png"));
    }
    return 0;
}
