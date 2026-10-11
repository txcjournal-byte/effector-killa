#pragma once

// Fallback panels used by the editor until the dedicated rack units exist:
//   PerformerFallback - KILL, PG / R / UNRATED, BOOTLEG, the 5 macros and IN / OUT meters
//                       (replaced by PerformerUnit, DESIGN-RACK.md section 5)
//   ModFallback       - empty MOD unit blank panel (replaced by ModUnit, section 12.1)
// The editor picks PerformerUnit.h / ModUnit.h automatically when they are present
// (see PluginEditor.h), so these classes can be deleted once both units are merged.

#include "Hardware.h"
#include "Widgets.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class PerformerFallback : public juce::Component
{
public:
    explicit PerformerFallback (EffectorKillaAudioProcessor& p);
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EffectorKillaAudioProcessor& proc;
    hw::BackgroundCache background;
    TwButton killBtn, pgBtn, rBtn, unratedBtn, bootlegBtn;
    juce::OwnedArray<LabeledKnob> macros;
    PeakMeter inMeter, outMeter;
};

class ModFallback : public juce::Component
{
public:
    explicit ModFallback (EffectorKillaAudioProcessor&) { setOpaque (true); }
    void tick() {}
    void paint (juce::Graphics&) override;

private:
    hw::BackgroundCache background;
};
} // namespace ek::ui
