// Renders the CHAIN view (RackView cards) and the FX library on their own (no editor) to PNG files -
// checks the TrapWin cards, signal line, drop states, live data and the library chips without a DAW.
//   EffectorKillaSnapshot --units <outDir>
// Writes:
//   chain.png        852 wide: 6 cards, one powered off, one locked, mod rings + a learned macro, hover spectrum
//   chain_wide.png   1088 wide (no FX library): the same chain
//   chain_split.png  852 wide: SPLIT fork / compact branch cards / MERGE
//   chain_states.png drop states: insert line, REPLACE, lifted card + gap, remove hint, full chain
//   units_solo.png   SOLO (the other cards dim)
//   units_gear.png   the FX library list, units_empty.png the empty chain
#include "PluginProcessor.h"
#include "ui/rack/RackView.h"
#include "ui/rack/GearList.h"
#include "ui/rack/ModUnit.h"
#include "ui/rack/CardStyle.h"

namespace
{
void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

// the editor's knob overlay (PluginEditor::paintKnobOverlay): mod rings + learned macro dots
void installOverlay (ek::ui::RackView& rack, EffectorKillaAudioProcessor& proc)
{
    rack.paintKnobOverlay = [&proc] (juce::Graphics& g, juce::Rectangle<float> face, int slot, int param)
    {
        const auto idx = proc.findModAssignments (slot, param);
        const auto all = proc.getModAssignments();
        const float base = proc.getSlotParam (slot, param);
        const float cur = proc.getEffectiveSlotParam (slot, param);
        int ring = 0;
        for (int i : idx)
        {
            if (i < 0 || i >= (int) all.size()) continue;
            const auto& a = all[(size_t) i];
            const bool bipolar = a.source == 0 || a.source == 1;
            ek::ui::paintModRing (g, face, base, a.depth, ek::ui::modSourceColour (a.source), bipolar, cur, ring++);
        }
        int k = 0;
        for (const auto& mm : proc.getMacroAssignmentsFor (slot, param))
            if (mm.learned)
            {
                g.setColour (juce::Colour (0xff9fc3ff));
                g.fillEllipse (face.getX() + 1.0f + 6.0f * (float) k++, face.getY() + 1.0f, 4.0f, 4.0f);
            }
    };
}

juce::Image stack (const std::vector<std::pair<juce::String, juce::Image>>& parts, int width)
{
    int h = 0;
    for (auto& p : parts) h += p.second.getHeight() + 30;
    juce::Image out (juce::Image::RGB, width, h + 10, true);
    juce::Graphics g (out);
    g.fillAll (ek::card::bg);
    int y = 0;
    for (auto& p : parts)
    {
        g.setColour (ek::card::muted);
        g.setFont (ek::card::monoFont (10.0f));
        g.drawText (p.first, 12, y + 8, width - 24, 16, juce::Justification::centredLeft);
        y += 30;
        g.drawImageAt (p.second, 0, y);
        y += p.second.getHeight();
    }
    return out;
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

    auto settle = [&] (ek::ui::RackView& rack, int width)
    {
        rack.refresh();
        rack.setSize (width, rack.getIdealHeight());
        for (int i = 0; i < 24; ++i) { render (2); rack.tick(); }
    };
    auto snap = [&] (ek::ui::RackView& rack) { return rack.createComponentSnapshot (rack.getLocalBounds(), true, 1.0f); };

    // ---- the chain: 6 cards, one powered off, one locked, mod rings + learned macro -----------------
    proc.loadFactory (1, 0);   // CH02 Vox Dei (5 units)
    proc.insertEffect (ek::kNumSlots, ek::EffectType::WideBody);
    proc.setSlotParam (3, 9, 1.0f);   // Ad-Lib Throw powered off (tails keep running)
    proc.setWriteProtect (1, true);   // Squeeze locked
    proc.addModAssignment (0, 0, 1, 0.45f);    // LFO 1 -> Tone Up P2
    proc.addModAssignment (2, 0, 1, -0.3f);    // ENV   -> Tone Up P2 (second ring)
    proc.addModAssignment (1, 4, 1, 0.35f);    // LFO 2 -> Aura Room P2
    proc.assignMacro (0, 2, 1, 0.1f, 0.8f);    // learned VILLAIN ARC -> Cassette Plug P2
    render (60);

    {
        ek::ui::RackView rack (proc);
        installOverlay (rack, proc);
        rack.setHoveredSlot (2);   // SPECTRAL VIEW behind the knobs of the hovered card
        settle (rack, 852);
        writePng (snap (rack), outDir.getChildFile ("chain.png"));
        writePng (snap (rack), outDir.getChildFile ("units_rack.png"));
        settle (rack, 1088);
        writePng (snap (rack), outDir.getChildFile ("chain_wide.png"));
        rack.setHoveredSlot (-1);

        // drop states
        rack.setSize (852, rack.getIdealHeight());
        std::vector<std::pair<juce::String, juce::Image>> parts;
        auto crop = [&] (int y0, int h) { return snap (rack).getClippedImage ({ 0, y0, 852, h }); };
        rack.previewDropState (1, 2);
        parts.push_back ({ "INSERT (FX chip between two cards)", crop (130, 200) });
        rack.previewDropState (2, 2);
        parts.push_back ({ "REPLACE (FX chip on a card)", crop (300, 170) });
        rack.previewDropState (3, 1, 3);
        {
            auto img = snap (rack);
            juce::Graphics g (img);
            // the floating lifted card (normally drawn by the drag container under the mouse)
            for (int i = 0; i < rack.getNumChildComponents(); ++i)
                if (auto* u = dynamic_cast<ek::ui::UnitComponent*> (rack.getChildComponent (i)); u != nullptr && u->getSlot() == 1)
                {
                    u->setLifted (false);
                    auto lift = u->createLiftImage (22);
                    u->setLifted (true);
                    g.drawImageAt (lift, u->getX() + 40 - 22, 230 - 22);
                }
            parts.push_back ({ "DRAG: lifted card + gap where it lands", img.getClippedImage ({ 0, 0, 852, 640 }) });
        }
        rack.previewDropState (4, 1);
        parts.push_back ({ "DRAGGED OUT OF THE CHAIN", crop (0, 330) });
        rack.previewDropState (0, -1);
        writePng (stack (parts, 852), outDir.getChildFile ("chain_states.png"));
    }

    // ---- SOLO: the third card is soloed, the others dim -------------------------------------------
    {
        proc.setSolo (2);
        ek::ui::RackView rack (proc);
        installOverlay (rack, proc);
        settle (rack, 852);
        writePng (snap (rack), outDir.getChildFile ("units_solo.png"));
        proc.setSolo (-1);
    }

    // ---- full chain (8 cards) ------------------------------------------------------------------------
    {
        for (auto t : { ek::EffectType::Swirl, ek::EffectType::RedLine })
            proc.insertEffect (ek::kNumSlots, t);
        ek::ui::RackView rack (proc);
        settle (rack, 852);
        auto img = snap (rack);
        writePng (img.getClippedImage ({ 0, img.getHeight() - 380, 852, 380 }), outDir.getChildFile ("chain_full.png"));
    }

    // ---- FX library ---------------------------------------------------------------------------------
    {
        ek::ui::GearList gear (proc);
        gear.setSize (ek::ui::GearList::preferredWidth, 824);
        writePng (gear.createComponentSnapshot (gear.getLocalBounds(), true, 1.0f), outDir.getChildFile ("units_gear.png"));
    }

    // ---- SPLIT: serial cards, fork into A | B (compact cards), merge --------------------------------
    {
        for (int s = ek::kNumSlots - 1; s >= 0; --s) { proc.setWriteProtect (s, false); proc.removeEffect (s); }
        proc.clearModAssignments();
        proc.loadFactory (9, 2);   // CH10 Night Drive: 4 units
        proc.insertSplit (2);
        const auto a = proc.getBranchSlots (0);
        if (! a.empty()) proc.setSlotBranch (a.back(), 1);
        proc.insertEffectInBranch (1, ek::EffectType::Doubles);
        proc.insertEffectInBranch (0, ek::EffectType::Slap);
        proc.addModAssignment (0, 0, 1, 0.4f);
        render (40);
        ek::ui::RackView rack (proc);
        installOverlay (rack, proc);
        settle (rack, 852);
        writePng (snap (rack), outDir.getChildFile ("chain_split.png"));
    }

    // ---- empty chain (drop zone) --------------------------------------------------------------------
    {
        proc.removeSplit();
        for (int s = ek::kNumSlots - 1; s >= 0; --s) { proc.setWriteProtect (s, false); proc.removeEffect (s); }
        ek::ui::RackView rack (proc);
        rack.setSize (852, 300);
        rack.tick();
        writePng (rack.createComponentSnapshot (rack.getLocalBounds(), true, 1.0f), outDir.getChildFile ("units_empty.png"));
    }
    return 0;
}
