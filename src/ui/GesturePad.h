/*
    TrapWin BEND DEMON
    GesturePad.h - large glowing gesture toggle with a mini curve icon (SLIDE, DIVE, ...).
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace bd
{

class GesturePad final : public juce::Button
{
public:
    enum class Icon { glide, chordGlide, vibrato, dive, rise, wobble, shake, trill };

    GesturePad (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                const juce::String& title, Icon, juce::Colour accent);

    void paintButton (juce::Graphics&, bool over, bool down) override;

    /** Animation phase from the editor's frame clock (makes active pads breathe). */
    void setPhase (float p) { if (getToggleState()) { phase = p; repaint(); } }

    /** Pads can be "selected" (shows their knobs in the dock) independently of on/off. */
    void setSelected (bool s) { if (selected != s) { selected = s; repaint(); } }
    bool isSelected() const noexcept { return selected; }

    std::function<void()> onSelect;

    /** 2.0: a click only selects the tile (shows its settings); the on/off state is shown as a dot. */
    void setSelectOnly (bool b)         { selectOnly = b; setClickingTogglesState (! b); }

    void mouseDown (const juce::MouseEvent&) override;

    /** The mini curve of a gesture, fitted into `r` (also used by the APPLY buttons). */
    static juce::Path iconPath (Icon, juce::Rectangle<float> r);

private:
    juce::Path makeIcon (juce::Rectangle<float> r) const { return iconPath (icon, r); }
    bool selectOnly = false;

    Icon icon;
    juce::Colour accent;
    float phase = 0.0f;
    bool selected = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

} // namespace bd
