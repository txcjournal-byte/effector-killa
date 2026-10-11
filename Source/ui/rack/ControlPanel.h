#pragma once

// Placeholder for the MOD rack unit (DESIGN-RACK.md section 12.1). The editor uses ModUnit.h
// automatically once it is merged (see PluginEditor.h); this blank panel can then be deleted.
// (The old top control panel was replaced by Toolbar + ControlUnit + PerformerUnit.)

#include "Hardware.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class ModFallback : public juce::Component
{
public:
    explicit ModFallback (EffectorKillaAudioProcessor&) { setOpaque (true); }
    void paint (juce::Graphics&) override;

private:
    hw::BackgroundCache background;
};
} // namespace ek::ui
