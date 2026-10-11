#pragma once

// PERFORMER unit (DESIGN-RACK.md section 5): the second fixed rack unit under the control unit.
// 932 x 104 at 1x, dark blue anodised metal with an ice-blue line, screws and rack ears.
//   left   - big red backlit KILL key, PG / R / UNRATED rating (3 LEDs), BOOTLEG
//   middle - the 5 macros with coloured rings that follow the modulated macro value
//   right  - analogue VU meters IN / OUT (needle ballistics, warm-white backlit scale, OVER LEDs)
// Self-contained: owns a 30 Hz timer and talks to the processor directly.

#include "../Controls.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class PerformerUnit : public juce::Component, private juce::Timer
{
public:
    explicit PerformerUnit (EffectorKillaAudioProcessor& p);
    ~PerformerUnit() override;

    static constexpr int unitWidth = 932;
    static constexpr int unitHeight = 104;

    // Fired once for every KILL (this key, the space bar, anything that calls proc.kill()),
    // so the editor can flash the whole rack.
    std::function<void()> onKillFlash;

    // Reduce motion: needles jump to the level, no KILL / BOOTLEG flash animation.
    void setReduceMotion (bool shouldReduce);
    bool getReduceMotion() const noexcept { return reduceMotion; }

    // Advances meters / LEDs by one 30 Hz frame. The internal timer calls it; offline renderers
    // (Tools/Snapshot.cpp) can call it directly.
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override;

    class KillKey;
    class RatingSelector;
    class BootlegKey;
    class MacroCell;
    class VuMeter;

private:
    void timerCallback() override { tick(); }
    void fireKillFlash();

    EffectorKillaAudioProcessor& proc;
    std::unique_ptr<KillKey> killKey;
    std::unique_ptr<RatingSelector> rating;
    std::unique_ptr<BootlegKey> bootleg;
    juce::OwnedArray<MacroCell> macros;
    std::unique_ptr<VuMeter> vuIn, vuOut;

    bool reduceMotion = false;
    int lastKillEvents = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformerUnit)
};
} // namespace ek::ui
