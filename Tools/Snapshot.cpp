// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>
//   EffectorKillaSnapshot --faceplates-space <outDir>   (contact sheet of the space-group faceplates)
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/Theme.h"
#include "ui/rack/Faceplates.h"

// Renders the Doubles..Red Line faceplates at 2x with a fake live State (+ mock knobs on top,
// the way RackView places them) and their 264x41 miniatures into one PNG.
static int renderFaceplatesSpace (const juce::File& outDir)
{
    using namespace ek;
    using namespace juce;
    const EffectType types[] = { EffectType::Doubles, EffectType::Swirl, EffectType::AdLibThrow, EffectType::AuraRoom,
                                 EffectType::WideBody, EffectType::Chopped, EffectType::RedLine };
    constexpr float sc = 2.0f, W = 898.0f, H = 140.0f, gap = 18.0f, mW = 264.0f, mH = 41.0f;
    const int miniCols = 3, miniRows = 3;
    const float sheetW = 24.0f + W + 24.0f;
    const float sheetH = 40.0f + 7.0f * (H + gap) + 30.0f + (float) miniRows * (mH + 12.0f) + 20.0f;
    Image img (Image::ARGB, (int) (sheetW * sc), (int) (sheetH * sc), true);
    Graphics g (img);
    g.addTransform (AffineTransform::scale (sc));
    g.fillAll (Colour (0xff07090d));
    g.setColour (theme::silverDim);
    g.setFont (theme::monoLabel (11.0f));
    g.drawText ("FACEPLATES - SPACE GROUP (2x, fake live state, mock knobs)", Rectangle<float> (24.0f, 10.0f, W, 20.0f), Justification::centredLeft);

    // fake stereo scope data for Wide Body
    std::vector<float> sl (512), sr (512);
    for (int i = 0; i < 512; ++i)
    {
        const float t = (float) i * 0.07f;
        const float m = 0.55f * std::sin (t) + 0.2f * std::sin (t * 2.7f + 0.4f);
        const float s = 0.28f * std::sin (t * 1.9f + 1.1f);
        sl[(size_t) i] = m - s; sr[(size_t) i] = m + s;
    }

    auto fakeState = [&] (EffectType t, int variant)
    {
        faceplate::State s;
        const auto& info = effectInfo (t);
        for (int i = 0; i < kNumParams; ++i) s.params[(size_t) i] = info.params[(size_t) i].defaultNorm();
        auto setReal = [&] (int i, float v) { s.params[(size_t) i] = info.params[(size_t) i].fromReal (v); };
        s.timeSec = 1.37 + variant * 0.61;
        s.beats = 5.3 + variant * 1.1;
        s.bpm = 140.0;
        s.inLevel = 0.4f; s.outLevel = 0.35f;
        switch (t)
        {
            case EffectType::Doubles:    setReal (2, variant == 0 ? 3.0f : 4.0f); break;
            case EffectType::Swirl:      setReal (2, 8.0f); setReal (0, 0.45f); break;
            case EffectType::AdLibThrow: setReal (0, 6.0f /* 1/4D */); setReal (3, 1.0f); setReal (2, 0.55f); break;
            case EffectType::AuraRoom:   setReal (2, 4.5f); setReal (3, 25.0f); break;
            case EffectType::WideBody:   setReal (0, 1.4f); setReal (1, 6.0f); setReal (2, 120.0f);
                                         if (variant == 0) { s.scopeL = sl.data(); s.scopeR = sr.data(); s.scopeN = 512; } break;
            case EffectType::Chopped:    setReal (1, 3.0f); break;
            case EffectType::RedLine:    s.gainReductionDb = 4.2f; setReal (1, 6.0f); break;
            default: break;
        }
        return s;
    };

    float y = 40.0f;
    for (auto t : types)
    {
        const Rectangle<float> plate (24.0f, y, W, H);
        const auto s = fakeState (t, 0);
        faceplate::paintFaceplate (g, t, plate, s, false);
        // mock knob row, as RackView would place it (label above, value below)
        const auto ka = faceplate::knobArea (t, plate);
        const auto pal = faceplate::palette (t);
        const auto& info = effectInfo (t);
        int used = 0;
        for (auto& p : info.params) used += p.used ? 1 : 0;
        const int n = used + (info.mixParam < 0 ? 1 : 0);
        const float cw = juce::jmin (78.0f, ka.getWidth() / (float) juce::jmax (1, n));
        int k = 0;
        for (int i = 0; i < kNumParams && k < n; ++i)
        {
            if (! info.params[(size_t) i].used) continue;
            const auto cell = Rectangle<float> (ka.getX() + cw * (float) k, ka.getY(), cw, ka.getHeight());
            g.setColour (pal.ink.withAlpha (0.8f));
            g.setFont (theme::monoLabel (8.5f));
            g.drawText (info.params[(size_t) i].name, cell.withHeight (14.0f), Justification::centred);
            theme::drawKnob (g, cell.getCentre().translated (0.0f, -2.0f), 19.0f, -1.0f + 0.3f * (float) k);
            g.drawText (info.params[(size_t) i].format (info.params[(size_t) i].toReal (s.params[(size_t) i])),
                        cell.withTop (cell.getBottom() - 16.0f), Justification::centred);
            ++k;
        }
        y += H + gap;
    }

    g.setColour (theme::silverDim);
    g.setFont (theme::monoLabel (11.0f));
    g.drawText ("MINIATURES (264 x 41)", Rectangle<float> (24.0f, y, W, 20.0f), Justification::centredLeft);
    y += 26.0f;
    for (int i = 0; i < 7; ++i)
    {
        const int c = i % miniCols, r = i / miniCols;
        const Rectangle<float> plate (24.0f + (float) c * (mW + 26.0f), y + (float) r * (mH + 12.0f), mW, mH);
        faceplate::paintFaceplate (g, types[i], plate, fakeState (types[i], 1), true);
    }
    // one extra miniature: powered off
    {
        const Rectangle<float> plate (24.0f + 2.0f * (mW + 26.0f), y + 2.0f * (mH + 12.0f), mW, mH);
        auto s = fakeState (EffectType::RedLine, 1);
        s.powered = false;
        faceplate::paintFaceplate (g, EffectType::RedLine, plate, s, true);
    }

    auto f = outDir.getChildFile ("faceplates_space.png");
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--faceplates-space")
    {
        const juce::File dir (argc > 2 ? juce::String (argv[2]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
        dir.createDirectory();
        return renderFaceplatesSpace (dir);
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
