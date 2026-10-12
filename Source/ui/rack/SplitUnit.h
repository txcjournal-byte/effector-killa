#pragma once

// SPLIT / MERGE strip at the fork of the chain - a slim TrapWin card (all text >= 11 px, knobs 30 px).
//
// Shows and edits the chain's single split (EffectorKillaAudioProcessor split API):
//   wide   (>= wideMinWidth, 64 px high):  [ chrome SPLIT + A/B routing ] [ mode pill ] [ XOVER ] [ A/B ] [ LVL A ] [ LVL B ] [ X ]
//   narrow (< wideMinWidth, 124 px high):  [ SPLIT + routing ........ X ]
//                                          [ mode pill (full width)      ]
//                                          [ XOVER  A/B  LVL A  LVL B    ]
// Use heightForWidth() for the bounds; `height` is the wide height.
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

    static constexpr int width = 752, height = 64;   // width = default only; the strip adapts to any width
    static constexpr int narrowHeight = 124, wideMinWidth = 600;
    static int heightForWidth (int w) noexcept { return w >= wideMinWidth ? height : narrowHeight; }

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
    bool isWide() const noexcept { return getWidth() >= wideMinWidth; }
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
