#pragma once

// SPLIT / MERGE strip (any width, 64 px high) at the fork of the chain - a slim TrapWin card.
//
// Shows and edits the chain's single split (EffectorKillaAudioProcessor split API):
//   [ chrome SPLIT + A/B routing ] [ mode segmented pill ] [ XOVER ] [ A/B ] [ LVL A ] [ LVL B ] [ chrome MERGE ] [ X ]
// The component is standalone: the RackView places it where the split sits and lays out the branch units
// in two half-width columns underneath (A left, B right). It reads everything from the processor, so
// call refresh() after any program change and tick() from the UI timer.

#include "Widgets.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class SplitUnit : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SplitUnit (EffectorKillaAudioProcessor& p);
    ~SplitUnit() override;

    static constexpr int width = 852, height = 64;   // width = default only; the strip adapts to any width

    void refresh(); // re-read mode / knob values from the processor (after preset load, undo ...)
    void tick();    // UI timer: LEDs, value texts

    // Optional hook for the rack (e.g. start dragging the split to a new position from the plate).
    std::function<void (const juce::MouseEvent&)> onPlateDrag;
    // Called after the remove button removed the split (the rack can relayout).
    std::function<void()> onRemoved;

    // value <-> knob mappings (shared with tests / the rack)
    static float freqToNorm (float hz) noexcept;
    static float normToFreq (float norm) noexcept;
    static float levelToNorm (float db) noexcept;
    static float normToLevel (float norm) noexcept;
    static juce::String formatFreq (float hz);
    static juce::String formatBalance (ek::SplitMode mode, float balance);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    class ModeButton;
    class FlatKnob;
    class ClosePill;
    void setMode (ek::SplitMode m);
    juce::Rectangle<float> platePane() const;
    juce::Rectangle<float> mergePane() const;
    std::array<FlatKnob*, 4> allKnobs() const;

    EffectorKillaAudioProcessor& proc;
    std::array<std::unique_ptr<ModeButton>, ek::kNumSplitModes> modeButtons;
    std::unique_ptr<FlatKnob> xover, balance, levelA, levelB;
    std::unique_ptr<ClosePill> removeBtn;
    juce::Rectangle<float> modeArea;
    ek::SplitMode shownMode = ek::SplitMode::Parallel;
    float shownGainA = -1.0f, shownGainB = -1.0f;
};

} // namespace ek::ui
