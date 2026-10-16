/*
    TrapWin BEND DEMON
    TopBar.h - TrapWin wordmark, "BEND DEMON" shimmer title, synth slot, voice/MPE status, presets.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class BendDemonProcessor;

namespace bd
{

class TopBar final : public juce::Component
{
public:
    explicit TopBar (BendDemonProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called by the editor's frame clock. */
    void tick (float timeSeconds);

    /** Refresh the synth slot / preset texts. */
    void refresh();

    std::function<void()> onLoadSynth;        // open the synth browser
    std::function<void()> onDimensionToggled;
    juce::TextButton& getDimensionButton() noexcept { return dimensionButton; }

private:
    void drawWordmark (juce::Graphics&, juce::Rectangle<float>);
    void drawTitle (juce::Graphics&, juce::Rectangle<float>);
    void drawStatus (juce::Graphics&, juce::Rectangle<float>);
    void showPresetMenu();
    void showSynthMenu();

    BendDemonProcessor& processor;

    juce::TextButton synthButton, editorButton, presetButton, prevPreset, nextPreset, dimensionButton, panicButton;
    juce::Rectangle<float> wordmarkArea, titleArea, statusArea;
    float time = 0.0f;
    int lastVoices = -1;
};

} // namespace bd
