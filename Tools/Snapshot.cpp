// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>          the editor shots
//   EffectorKillaSnapshot <outDir> --mod    only the MOD unit (mod_unit.png, mod_unit_2x.png, mod_ring_demo.png)
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/rack/ModUnit.h"

static void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

// MOD unit on its own: a few patched sources, the sources running, plus knobs with modulation rings.
static int snapshotModUnit (const juce::File& outDir)
{
    EffectorKillaAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.loadFactory (9, 2, false);
    proc.addModAssignment (0, 1, 1, 0.35f);      // LFO 1 -> filter cutoff
    proc.addModAssignment (0, 3, 8, -0.2f);      // LFO 1 -> reverb MIX
    proc.addMacroModAssignment (1, ek::Drip, 0.3f);
    proc.addModAssignment (2, 0, 1, 0.5f);       // ENV -> drive
    proc.addModAssignment (3, 2, 8, 0.4f);       // SEQ -> delay MIX
    proc.addModAssignment (3, 1, 2, 0.25f);
    auto lfo = proc.getModSources().lfo[0];
    lfo.rateHz = 0.8f; lfo.phase = 0.1f;
    proc.setLfo (0, lfo);
    proc.setParamNorm (ek::ParamIDs::tempoMode, 1.0f);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    ek::ui::ModUnit unit (proc);
    unit.setBounds (0, 0, ek::ui::ModUnit::kWidth, ek::ui::ModUnit::kHeight);
    for (int b = 0; b < 120; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float t = (float) (b * 512 + i);
            const float env = std::exp (-std::fmod (t, 12000.0f) / 3000.0f); // plucks for the ENV display
            buf.setSample (0, i, 0.5f * env * std::sin (t * 0.05f));
            buf.setSample (1, i, 0.5f * env * std::sin (t * 0.051f));
        }
        proc.processBlock (buf, midi);
        if (b % 2 == 0) unit.tick();
    }
    unit.refresh();
    writePng (unit.createComponentSnapshot (unit.getLocalBounds(), true, 1.0f), outDir.getChildFile ("mod_unit.png"));
    writePng (unit.createComponentSnapshot (unit.getLocalBounds(), true, 2.0f), outDir.getChildFile ("mod_unit_2x.png"));

    // knob rings (what RackView draws with paintModRing) + a patch cable
    juce::Image demo (juce::Image::ARGB, 420, 110, true);
    {
        juce::Graphics g (demo);
        g.fillAll (juce::Colour (0xff12151b));
        const float bases[] = { 0.5f, 0.3f, 0.7f, 0.2f };
        const float depths[] = { 0.35f, 0.5f, -0.4f, 0.6f };
        for (int s = 0; s < 4; ++s)
        {
            const auto kb = juce::Rectangle<float> (40.0f, 40.0f).withCentre ({ 50.0f + 85.0f * (float) s, 50.0f });
            ek::ui::paintModRing (g, kb, bases[s], depths[s], ek::ui::modSourceColour (s), ek::modSourceIsBipolar (s),
                                  juce::jlimit (0.0f, 1.0f, bases[s] + depths[s] * 0.6f));
            ek::theme::drawKnob (g, kb.getCentre(), 20.0f, ek::theme::knobStart + (ek::theme::knobEnd - ek::theme::knobStart) * bases[s]);
        }
        ek::ui::paintPatchCable (g, { 360.0f, 20.0f }, { 400.0f, 90.0f }, ek::ui::modSourceColour (1));
    }
    writePng (demo, outDir.getChildFile ("mod_ring_demo.png"));
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    const bool modOnly = args.contains ("--mod");
    args.removeString ("--mod");
    const juce::File outDir (args.size() > 0 ? args[0] : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    if (modOnly) return snapshotModUnit (outDir);

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
