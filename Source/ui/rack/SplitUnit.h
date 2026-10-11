#pragma once

// SPLIT / MERGE hardware strip (932 x 64) for the rack.
//
// Shows and edits the rack's single split (EffectorKillaAudioProcessor split API):
//   [ SPLIT > A | B plate ] [ 5 mode LED buttons ] [ XOVER ] [ BALANCE ] [ LVL A ] [ LVL B ] [ MERGE plate ] [ remove ]
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

    static constexpr int width = 932, height = 64;

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
    void setMode (ek::SplitMode m);
    juce::Rectangle<float> platePane() const;
    juce::Rectangle<float> mergePane() const;

    EffectorKillaAudioProcessor& proc;
    std::array<std::unique_ptr<ModeButton>, ek::kNumSplitModes> modeButtons;
    LabeledKnob xover { "XOVER" }, balance { "A / B" }, levelA { "LVL A" }, levelB { "LVL B" };
    TwButton removeBtn { "X", juce::Colour (0xffff2e3e), 11.0f };
    ek::SplitMode shownMode = ek::SplitMode::Parallel;
    float shownGainA = -1.0f, shownGainB = -1.0f;
};

} // namespace ek::ui
