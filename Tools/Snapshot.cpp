// Renders the editor to PNG files - checks the UI without a DAW.
//   EffectorKillaSnapshot <outDir>
//   EffectorKillaSnapshot --units <outDir>   (rack units + gear list only, Tools/SnapshotUnits.cpp)
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

int runUnitsSnapshot (const juce::File& outDir);

// --faceplates: contact sheet of the colour-group faceplates
#include "ui/Theme.h"
#include "ui/rack/Faceplates.h"

// Renders the Cassette Plug..Slap faceplates at 2x with a fake live State (+ mock knobs on top,
// the way RackView places them) and their 264x41 miniatures into one contact sheet.
static int renderFaceplatesColor (const juce::File& outDir)
{
    using namespace ek;
    using namespace juce;
    const EffectType types[] = { EffectType::CassettePlug, EffectType::Menace, EffectType::Brainrot, EffectType::ThroughTheWall,
                                 EffectType::ToneUp, EffectType::Squeeze, EffectType::Overcooked, EffectType::Slap };
    constexpr float sc = 2.0f, W = 898.0f, H = 140.0f, gap = 18.0f, mW = 264.0f, mH = 41.0f;
    const int miniCols = 3, miniRows = 4;
    const float sheetW = 24.0f + W + 24.0f;
    const float sheetH = 40.0f + 8.0f * (H + gap) + 30.0f + (float) miniRows * (mH + 12.0f) + 20.0f;
    Image img (Image::ARGB, (int) (sheetW * sc), (int) (sheetH * sc), true);
    Graphics g (img);
    g.addTransform (AffineTransform::scale (sc));
    g.fillAll (Colour (0xff07090d));
    g.setColour (theme::silverDim);
    g.setFont (theme::monoLabel (11.0f));
    g.drawText ("FACEPLATES - COLOR GROUP (2x, fake live state, mock knobs)", Rectangle<float> (24.0f, 10.0f, W, 20.0f), Justification::centredLeft);

    auto fakeState = [&] (EffectType t, int variant)
    {
        faceplate::State s;
        const auto& info = effectInfo (t);
        for (int i = 0; i < kNumParams; ++i) s.params[(size_t) i] = info.params[(size_t) i].defaultNorm();
        auto setReal = [&] (int i, float v) { s.params[(size_t) i] = info.params[(size_t) i].fromReal (v); };
        s.timeSec = 1.37 + variant * 0.61;
        s.beats = 5.3 + variant * 1.1;
        s.inLevel = 0.4f; s.outLevel = 0.5f;
        switch (t)
        {
            case EffectType::CassettePlug:   setReal (1, 0.55f); setReal (4, 0.4f); break;
            case EffectType::Menace:         setReal (0, 0.0f); setReal (1, variant == 0 ? 0.72f : 0.45f); break;
            case EffectType::Brainrot:       setReal (0, variant == 0 ? 8.0f : 6.0f); setReal (1, 11000.0f); break;
            case EffectType::ThroughTheWall: setReal (0, variant == 0 ? 1.0f : 3.0f); setReal (1, 1800.0f); setReal (2, 0.6f); break;
            case EffectType::ToneUp:         setReal (0, 40.0f); setReal (1, 3.0f); setReal (2, -2.5f); setReal (3, 300.0f);
                                             setReal (4, 4.0f); setReal (5, 3000.0f); setReal (6, 2.5f); setReal (7, 0.5f); break;
            case EffectType::Squeeze:        s.gainReductionDb = variant == 0 ? 6.5f : 3.0f; setReal (1, 4.0f); break;
            case EffectType::Overcooked:     s.bands = { 0.85f, 0.55f, 0.3f }; setReal (0, 0.6f); break;
            case EffectType::Slap:           setReal (0, 0.6f); setReal (1, -0.3f); s.transientHit = variant == 0; break;
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
        const float cw = jmin (78.0f, ka.getWidth() / (float) jmax (1, n));
        int k = 0;
        for (int i = 0; i < kNumParams && k < n; ++i)
        {
            if (! info.params[(size_t) i].used) continue;
            const auto cell = Rectangle<float> (ka.getX() + cw * (float) k, ka.getY(), cw, ka.getHeight());
            g.setColour (pal.ink.withAlpha (0.8f));
            g.setFont (theme::monoLabel (8.5f));
            g.drawText (info.params[(size_t) i].name, cell.withHeight (14.0f), Justification::centred);
            theme::drawKnob (g, cell.getCentre().translated (0.0f, -2.0f), 19.0f, -1.0f + 0.3f * (float) k);
            g.setColour (pal.ink.withAlpha (0.8f));
            g.setFont (theme::monoLabel (8.5f));
            g.drawText (info.params[(size_t) i].format (info.params[(size_t) i].toReal (s.params[(size_t) i])),
                        cell.withTop (cell.getBottom() - 16.0f), Justification::centred);
            ++k;
        }
        y += H + gap;
    }

    g.setColour (theme::silverDim);
    g.setFont (theme::monoLabel (11.0f));
    g.drawText ("MINIATURES (264 x 41)  -  last row: powered off, solo-dimmed, reduce motion", Rectangle<float> (24.0f, y, W, 20.0f), Justification::centredLeft);
    y += 26.0f;
    for (int i = 0; i < 8; ++i)
    {
        const int c = i % miniCols, r = i / miniCols;
        const Rectangle<float> plate (24.0f + (float) c * (mW + 26.0f), y + (float) r * (mH + 12.0f), mW, mH);
        faceplate::paintFaceplate (g, types[i], plate, fakeState (types[i], 1), true);
    }
    for (int i = 0; i < 3; ++i)
    {
        const auto t = types[i == 0 ? 1 : (i == 1 ? 5 : 0)];
        const Rectangle<float> plate (24.0f + (float) i * (mW + 26.0f), y + 3.0f * (mH + 12.0f), mW, mH);
        auto s = fakeState (t, 1);
        if (i == 0) s.powered = false;
        if (i == 1) s.soloDimmed = true;
        if (i == 2) s.reduceMotion = true;
        faceplate::paintFaceplate (g, t, plate, s, true);
    }

    auto f = outDir.getChildFile ("faceplates_color.png");
    f.deleteFile();
    FileOutputStream os (f);
    PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}

// --faceplates-space: contact sheet of the space-group faceplates
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

// --mod: the MOD unit on its own (mod_unit.png, mod_unit_2x.png, mod_ring_demo.png)
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
    auto env = proc.getModSources().env;
    env.sensitivity = 0.15f; env.releaseMs = 220.0f;
    proc.setEnvFollower (env);
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
    // Special snapshot modes: EffectorKillaSnapshot --<mode> [outDir]
    struct Mode { const char* flag; int (*fn) (const juce::File&); };
    const Mode modes[] = {
        { "--frame", renderFrameSnapshots },
        { "--performer", renderPerformerSnapshots },
        { "--units", runUnitsSnapshot },
        { "--faceplates", renderFaceplatesColor },
        { "--faceplates-space", renderFaceplatesSpace },
        { "--mod", snapshotModUnit },
    };
    // the output dir is the first argument that is not a flag (before or after the flag)
    juce::String dirArg;
    for (int a = 1; a < argc; ++a)
        if (! juce::String (argv[a]).startsWith ("--")) { dirArg = argv[a]; break; }
    for (int a = 1; a < argc; ++a)
        for (auto& m : modes)
            if (juce::String (argv[a]) == m.flag)
            {
                const juce::File dir (dirArg.isNotEmpty() ? dirArg : juce::File::getCurrentWorkingDirectory().getFullPathName());
                dir.createDirectory();
                return m.fn (dir);
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
