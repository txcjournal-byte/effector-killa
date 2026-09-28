#pragma once

// Video recorder: VFD preset display (click = preset list), REC, REWIND / FAST FWD (undo / redo),
// EJECT (empty the selected cassette) and the SIDE A / SIDE B switch.

#include "../../PluginProcessor.h"
#include "Layout.h"
#include "../Theme.h"

namespace ek::ui
{
class VideoRecorder : public juce::Component
{
public:
    explicit VideoRecorder (EffectorKillaAudioProcessor& p);

    std::function<void (const juce::String&)> onMessage;
    std::function<void()> onEject;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();

    void showPresetMenu();
    void showSaveDialog();

private:
    enum Part { None = -1, Display, Rec, Rewind, FastFwd, Eject, Side };
    Part partAt (juce::Point<float> p) const;
    juce::Rectangle<float> local (juce::Rectangle<float> r) const { return r.translated (-origin.x, -origin.y); }

    EffectorKillaAudioProcessor& proc;
    juce::Point<float> origin;
    Part down = None;
    juce::String shownName;
    int shownSide = -1;
    bool shownUndo = false, shownRedo = false;
};
} // namespace ek::ui
