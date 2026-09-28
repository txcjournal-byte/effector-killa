// Renders the editor (software TV) to PNG files – used to check the UI layout without a DAW.
//   EffectorKillaSnapshot <outDir>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    EffectorKillaAudioProcessor proc;
    proc.softwareTv = true;
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

    auto shot = [&] (const juce::String& name, std::function<void (EffectorKillaAudioProcessorEditor&)> setup)
    {
        auto* ed = dynamic_cast<EffectorKillaAudioProcessorEditor*> (proc.createEditor());
        if (ed == nullptr) return;
        if (setup) setup (*ed);
        for (int i = 0; i < 90; ++i)
        {
            render (2);
            ed->getCabinet().tick (1.0 / 60.0);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        delete ed;
    };

    shot ("01_default", nullptr);
    shot ("02_detail", [&] (EffectorKillaAudioProcessorEditor& e) { e.getCabinet().selectSlot (1); });
    shot ("03_ch08_av3", [&] (EffectorKillaAudioProcessorEditor&)
    {
        proc.loadFactory (7, 0);
        auto m = proc.getMeta().mod; m.mode = ek::AvMode::Screensaver; proc.setModState (m, false);
        proc.setWriteProtect (0, true);
        proc.setSlotParam (2, 9, 1.0f);
    });
    shot ("04_scale150", [&] (EffectorKillaAudioProcessorEditor& e) { proc.loadFactory (2, 0); e.setUiScale (1.5f); });
    shot ("05_basic_pocket", [&] (EffectorKillaAudioProcessorEditor&)
    {
        proc.premiumCable = false;
        proc.setParamNorm (ek::ParamIDs::pocketTv, 1.0f);
        proc.loadFactory (4, 2);
    });
    return 0;
}
