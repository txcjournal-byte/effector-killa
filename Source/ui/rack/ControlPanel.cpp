#include "ControlPanel.h"
#include "../../engine/Macro.h"

namespace ek::ui
{
using namespace juce;

namespace
{
const Colour kPerformerBlue { 0xff0e1626 };
}

// ============================================================================================
PerformerFallback::PerformerFallback (EffectorKillaAudioProcessor& p)
    : proc (p),
      killBtn ("KILL", theme::killRed, 22.0f), pgBtn ("PG", theme::accent, 9.5f), rBtn ("R", theme::accent, 9.5f),
      unratedBtn ("UNRATED", theme::accent, 8.5f), bootlegBtn ("BOOTLEG", theme::accent, 9.5f)
{
    setOpaque (true);
    killBtn.solid = true;
    killBtn.onClick = [this] { proc.kill(); };
    killBtn.setTooltip ("KILL: a new, musical, level-matched chain (space bar). Locked units stay.");
    auto rating = [this] (TwButton& b, KillLevel l, const String& tip)
    {
        b.isLit = [this, l] { return proc.killLevel == l; };
        b.onClick = [this, l] { proc.killLevel = l; };
        b.setTooltip (tip);
    };
    rating (pgBtn, KillLevel::PG, "PG: same units, small changes");
    rating (rBtn, KillLevel::R, "R: new chain of 3 - 5 units");
    rating (unratedBtn, KillLevel::Unrated, "UNRATED: 4 - 7 units, hotter settings");
    bootlegBtn.onClick = [this] { proc.bootleg(); };
    bootlegBtn.setTooltip ("BOOTLEG: subtle variation of every setting, same units");

    for (int m = 0; m < kNumMacros; ++m)
    {
        auto* k = macros.add (new LabeledKnob (MacroEngine::macroName (m), Knob::Style::Macro));
        if (auto* rp = proc.getParam (ParamIDs::macro (m))) k->knob.bindTo (*rp);
        k->knob.getDisplayValue = [this, m] { return proc.engine.meters.macroValue[(size_t) m].load(); };
        k->valueText = [this, m] { return String (roundToInt (proc.engine.meters.macroValue[(size_t) m].load() * 100.0f)); };
        k->knob.onGestureStart = [this, m] { proc.beginUiGesture (m); };
        k->knob.onGestureEnd = [this, m] { proc.endUiGesture (m); };
        static const char* tips[] = { "VILLAIN ARC: darker - filters down, darker EQ and reverb",
                                      "CRASH OUT: dirt - more drive, fewer bits, harder clipping",
                                      "AURA: space - more reverb / delay, longer, wider",
                                      "DRIP: motion - faster, deeper modulation and wobble",
                                      "KNOCK: punch - transients, compression, clipper drive" };
        k->setTooltip (tips[m]);
        addAndMakeVisible (k);
    }
    inMeter.getPeak = [this] { return proc.engine.meters.inLevel.load(); };
    outMeter.getPeak = [this] { return proc.engine.meters.outPeak.load(); };
    for (Component* c : std::initializer_list<Component*> { &killBtn, &pgBtn, &rBtn, &unratedBtn, &bootlegBtn, &inMeter, &outMeter })
        addAndMakeVisible (c);
}

void PerformerFallback::tick()
{
    for (auto* b : { &pgBtn, &rBtn, &unratedBtn }) b->tick();
    for (auto* k : macros) k->tick();
    inMeter.tick();
    outMeter.tick();
}

void PerformerFallback::resized()
{
    killBtn.setBounds (34, 8, 132, 50);
    pgBtn.setBounds (34, 62, 36, 18);
    rBtn.setBounds (72, 62, 28, 18);
    unratedBtn.setBounds (102, 62, 64, 18);
    bootlegBtn.setBounds (34, 82, 132, 16);
    int x = 196;
    for (auto* k : macros) { k->setBounds (x, 6, 104, 92); x += 112; }
    inMeter.setBounds (getWidth() - 70, 14, 10, 76);
    outMeter.setBounds (getWidth() - 50, 14, 10, 76);
}

void PerformerFallback::paint (Graphics& g)
{
    background.draw (g, getLocalBounds(), [this] (Graphics& bg)
    {
        bg.fillAll (hw::col::shaft);
        hw::drawRackPanel (bg, getLocalBounds().toFloat(), kPerformerBlue, 22.0f, 21);
        bg.setColour (hw::col::ice.withAlpha (0.6f));
        bg.fillRect (22.0f, 2.0f, (float) getWidth() - 44.0f, 1.0f);
        hw::drawPrint (bg, "IN", { (float) getWidth() - 76.0f, 90.0f, 22.0f, 10.0f }, Justification::centred, 7.5f);
        hw::drawPrint (bg, "OUT", { (float) getWidth() - 56.0f, 90.0f, 22.0f, 10.0f }, Justification::centred, 7.5f);
    });
}

// ============================================================================================
void ModFallback::paint (Graphics& g)
{
    background.draw (g, getLocalBounds(), [this] (Graphics& bg)
    {
        bg.fillAll (hw::col::shaft);
        auto inner = hw::drawRackPanel (bg, getLocalBounds().toFloat(), Colour (0xff121419), 22.0f, 31);
        hw::drawPrint (bg, "MOD", inner.reduced (14.0f, 0.0f), Justification::centredLeft, 12.0f, hw::col::print);
        hw::drawPrint (bg, String::fromUTF8 ("LFO 1  \xc2\xb7  LFO 2  \xc2\xb7  ENVELOPE  \xc2\xb7  STEP SEQ"), inner, Justification::centred, 9.0f);
    });
}
} // namespace ek::ui
