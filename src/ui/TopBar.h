/*
    TrapWin BEND DEMON
    TopBar.h - 2.0 slim top bar:

      TRAPWIN | BEND DEMON  [DRAW|LIVE]  SOUND [Internal . Glide Lead v]  LOOP [4 bars]
      [> PLAY  140 BPM]            [<][preset v][>]  [3D]  [gear: MPE / MIDI / panic / size]
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "EditToolbar.h"

class BendDemonProcessor;

namespace bd
{

class TopBar final : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    explicit TopBar (BendDemonProcessor&);
    ~TopBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called by the editor's frame clock. */
    void tick (float timeSeconds);

    /** Refresh the synth slot / preset / mode texts. */
    void refresh();

    std::function<void()> onLoadSynth;        // open the synth browser
    std::function<void()> onDimensionToggled;
    std::function<void (int bars)> onBarsChanged;
    std::function<void()> onWindowSizeMenu;
    juce::TextButton& getDimensionButton() noexcept { return dimensionButton; }

private:
    void drawWordmark (juce::Graphics&, juce::Rectangle<float>);
    void showPresetMenu();
    void showSynthMenu();
    void showSettingsMenu();

    BendDemonProcessor& processor;

    juce::TextButton modeDraw { "DRAW" }, modeLive { "LIVE" };
    juce::TextButton synthButton, presetButton, prevPreset, nextPreset;
    juce::TextButton dimensionButton;   // owned here, shown by the editor in the canvas corner
    IconButton settingsButton { "Settings", IconButton::Icon::gear };
    juce::ComboBox barsBox;
    IconButton playButton { "Play", IconButton::Icon::play };
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    juce::Rectangle<float> wordmarkArea, soundCaption, barsCaption, statusArea;
    float time = 0.0f;
    int lastVoices = -1, lastBars = -1;
    juce::String lastSlot, lastTempo;
};

} // namespace bd
