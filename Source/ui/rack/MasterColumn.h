#pragma once

// MASTER column (Docs/TRAPWIN-STYLE.md "Layout"): one TrapWin card on the right of the window,
// 224 x 812 at 1x. Replaces the rack-era CONTROL unit + PERFORMER unit and keeps all of their
// functions:
//   TEMPO    big glowing Plex Mono BPM (drag / wheel / double-click to type), TAP + beat dot, HOST / FREE
//   LEVELS   IN, OUT, RECYCLE (amber warning arc), MIX - flat TrapWin knobs
//   KILL     big red-glow KILL pill, PG / R / UNRATED pills, BOOTLEG
//   MACROS   VILLAIN ARC, CRASH OUT, AURA, DRIP, KNOCK - flat knobs in their colours, the arc follows the
//            live (modulated) value, a white tick marks the knob itself
//   METERS   slim vertical LED bars IN / OUT (ice blue -> white -> red, peak hold, OVER dot)
//   TOOLBOX  PHONE CHECK (pulses while on), AUTO GAIN, SIZE SMALL / FULL / AUTO
// Self-contained: owns a 30 Hz timer and talks to the processor directly.

#include "Hardware.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class MasterColumn : public juce::Component, private juce::Timer
{
public:
    explicit MasterColumn (EffectorKillaAudioProcessor& p);
    ~MasterColumn() override;

    static constexpr int kWidth = 224;
    static constexpr int kHeight = 812;

    // Fired once for every KILL (this key, the space bar, anything that calls proc.kill()),
    // so the editor can flash the chain.
    std::function<void()> onKillFlash;
    std::function<void (float scale)> onSetScale;   // SIZE pills: 0 = auto, 0.8 = SMALL, 1.0 = FULL

    // Reduce motion: meters jump, no KILL / BOOTLEG flash, PHONE CHECK does not pulse.
    void setReduceMotion (bool shouldReduce);
    bool getReduceMotion() const noexcept { return reduceMotion; }

    // Advances meters / LEDs by one 30 Hz frame. The internal timer calls it; offline renderers
    // (Tools/Snapshot.cpp) can call it directly.
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override;

    // Macro knob under a screen position (0..4), -1 = none. Used to drop MOD jacks onto macros.
    int macroAt (juce::Point<int> screenPos) const;
    // Bounds of a macro cell in screen coordinates (cable target highlight).
    juce::Rectangle<int> macroScreenBounds (int macro) const;

    // section rectangles (local coordinates)
    static juce::Rectangle<int> tempoArea()   { return { 14, 50, 196, 108 }; }
    static juce::Rectangle<int> levelsArea()  { return { 14, 166, 196, 92 }; }
    static juce::Rectangle<int> killArea()    { return { 14, 266, 196, 116 }; }
    static juce::Rectangle<int> macrosArea()  { return { 14, 390, 196, 164 }; }
    static juce::Rectangle<int> metersArea()  { return { 14, 562, 196, 140 }; }
    static juce::Rectangle<int> toolboxArea() { return { 14, 710, 196, 90 }; }

    class BpmDisplay;
    class BeatDot;
    class LevelKnob;
    class KillKey;
    class MacroCell;
    class LedMeter;

private:
    void timerCallback() override { tick(); }
    void fireKillFlash();
    void paintStatic (juce::Graphics&);

    EffectorKillaAudioProcessor& proc;
    hw::BackgroundCache background;

    std::unique_ptr<BpmDisplay> bpm;
    std::unique_ptr<BeatDot> beat;
    hw::HwButton tapBtn, hostBtn, freeBtn;
    juce::OwnedArray<LevelKnob> levels;
    std::unique_ptr<KillKey> killKey;
    hw::HwButton pgBtn, rBtn, unratedBtn, bootlegBtn;
    juce::OwnedArray<MacroCell> macros;
    std::unique_ptr<LedMeter> meterIn, meterOut;
    hw::HwButton phoneBtn, autoGainBtn, smallBtn, fullBtn, autoBtn;

    bool reduceMotion = false;
    int lastKillEvents = 0;
    float bootlegLit = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterColumn)
};

using PerformerUnit = MasterColumn;   // old name (editor wiring / snapshot tool)
} // namespace ek::ui
