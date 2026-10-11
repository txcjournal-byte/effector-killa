// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>
//   EffectorKillaSnapshot --faceplates <outDir>   (rack faceplates of the colour group at 2x + contact sheet)
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/rack/Faceplates.h"

static void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

// Renders each colour-group faceplate (full size + miniature) at 2x with a fake live State.
static int renderFaceplates (const juce::File& outDir)
{
    using ek::EffectType;
    namespace fp = ek::faceplate;
    const EffectType types[] = { EffectType::CassettePlug, EffectType::Menace, EffectType::Brainrot, EffectType::ThroughTheWall,
                                 EffectType::ToneUp, EffectType::Squeeze, EffectType::Overcooked, EffectType::Slap };
    auto fakeState = [] (EffectType t)
    {
        fp::State s;
        s.timeSec = 1.37;
        s.outLevel = 0.6f;
        for (int i = 0; i < 8; ++i) s.params[(size_t) i] = ek::effectInfo (t).params[(size_t) i].defaultNorm();
        switch (t)
        {
            case EffectType::CassettePlug:   s.params[1] = 0.55f; s.params[4] = 0.4f; break;
            case EffectType::Menace:         s.params[0] = 0.0f; s.params[1] = 0.72f; break;
            case EffectType::Brainrot:       s.params[0] = 4.0f / 12.0f; s.params[1] = 0.45f; break;
            case EffectType::ThroughTheWall: s.params[0] = 0.2f; s.params[1] = 0.55f; s.params[2] = 0.6f; break;
            case EffectType::ToneUp:         s.params = { 0.3f, 0.65f, 0.4f, 0.4f, 0.62f, 0.5f, 0.72f, 0.55f }; break;
            case EffectType::Squeeze:        s.gainReductionDb = 6.5f; break;
            case EffectType::Overcooked:     s.bands = { 0.85f, 0.55f, 0.3f }; break;
            case EffectType::Slap:           s.params[0] = 0.8f; s.params[1] = 0.35f; s.transientHit = true; break;
            default: break;
        }
        return s;
    };

    const float sc = 2.0f;
    const int plateW = 898, plateH = 140, miniW = 264, miniH = 41, pad = 14;
    juce::Image sheet (juce::Image::RGB, (int) ((plateW + miniW + pad * 3) * sc), (int) ((pad + 8 * (plateH + pad)) * sc), true);
    juce::Graphics sg (sheet);
    sg.fillAll (juce::Colour (0xff0c0d10));
    sg.addTransform (juce::AffineTransform::scale (sc));

    int row = 0;
    for (auto t : types)
    {
        const auto s = fakeState (t);
        for (int mini = 0; mini < 2; ++mini)
        {
            const int w = mini ? miniW : plateW, h = mini ? miniH : plateH;
            juce::Image img (juce::Image::ARGB, (int) (w * sc), (int) (h * sc), true);
            {
                juce::Graphics g (img);
                g.addTransform (juce::AffineTransform::scale (sc));
                fp::paintFaceplate (g, t, { 0.0f, 0.0f, (float) w, (float) h }, s, mini != 0);
            }
            writePng (img, outDir.getChildFile ("fp_" + ek::effectInfo (t).id + (mini ? "_mini" : "") + ".png"));
        }
        const float y = (float) (pad + row * (plateH + pad));
        fp::paintFaceplate (sg, t, { (float) pad, y, (float) plateW, (float) plateH }, s, false);
        fp::paintFaceplate (sg, t, { (float) (pad * 2 + plateW), y + (plateH - miniH) * 0.5f, (float) miniW, (float) miniH }, s, true);
        ++row;
    }
    writePng (sheet, outDir.getChildFile ("faceplates_color.png"));
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--faceplates")
    {
        const juce::File dir (argc > 2 ? juce::String (argv[2]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
        dir.createDirectory();
        return renderFaceplates (dir);
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
