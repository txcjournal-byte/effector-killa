/*
    TrapWin BEND DEMON
    EditToolbar.h - the tool row under the top bar (2.0, "brutal" = few, big controls):

      [PENCIL][LINE][SELECT][ERASER] | SNAP  KEY  SCALE [LOCK]  MAGNET |   [UNDO][REDO] [MORE]
      [drag MIDI] [LANE]
    Everything secondary (edit commands, CAPTURE, mono export, clear ...) lives in MORE.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LineEditor.h"
#include "GlowKnob.h"

class BendDemonProcessor;

namespace bd
{

class VoiceField;

/** Small glass button with a vector icon (or short text), optional toggle glow. */
class IconButton final : public juce::Button
{
public:
    enum class Icon { pencil, line, select, eraser, undo, redo, play, stop, record, drag, lane, dock, gear, none };

    IconButton (const juce::String& name, Icon, juce::Colour accent = juce::Colour (0xff3ad7f2));
    void setIcon (Icon i)                   { icon = i; repaint(); }
    void setLabel (const juce::String& s)   { label = s; repaint(); }
    void setPulse (float p)                 { pulse = p; repaint(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;

    std::function<void (const juce::MouseEvent&)> onDragOut;   // MIDI drag handles
    void mouseDown (const juce::MouseEvent& e) override       { dragStarted = false; juce::Button::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent&) override;
    /** True once after a drag-out (so the click that follows does not trigger the button). */
    bool consumeDragged() noexcept                            { const bool d = dragStarted; dragStarted = false; return d; }

private:
    juce::Path makeIcon (juce::Rectangle<float>) const;
    Icon icon;
    juce::Colour accent;
    juce::String label;
    float pulse = 0.0f;
    bool dragStarted = false;
};

/** Horizontal glowing value pill (MAGNET). */
class PillSlider final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PillSlider (const juce::String& caption);
    void setValue (float v, bool notify);
    float getValue() const noexcept         { return value; }
    std::function<void (float)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::String caption;
    float value = 0.6f, dragStartValue = 0.0f;
};

//==============================================================================
class EditToolbar final : public juce::Component
{
public:
    EditToolbar (BendDemonProcessor&, LineEditController&, VoiceField&);

    void resized() override;
    void paint (juce::Graphics&) override;

    /** Frame clock: refreshes enable / toggle states. */
    void tick (float timeSeconds);

    void setLaneOpen (bool);
    void setEditingEnabled (bool);     // LIVE mode: drawing tools off, key / scale stay
    std::function<void (bool)> onLaneToggled;
    std::function<void (const juce::String&)> onMessage;

    double getSnapBeats() const;
    void   setSnapIndex (int index);
    int    getSnapIndex() const              { return snapBox.getSelectedItemIndex(); }
    PillSlider& getMagnet() noexcept         { return magnet; }

    static int numSnapChoices();
    static double snapBeatsFor (int index);

private:
    void showMoreMenu();
    void dragMidi (bool mono, const juce::MouseEvent&);
    void saveMidi (bool mono);
    void savePattern();
    void openFile();
    juce::File writeMidiFile (bool mono) const;

    BendDemonProcessor& processor;
    LineEditController& controller;
    VoiceField& field;

    IconButton pencil { "Pencil", IconButton::Icon::pencil }, lineTool { "Line", IconButton::Icon::line },
               select { "Select", IconButton::Icon::select }, eraser { "Eraser", IconButton::Icon::eraser },
               undo { "Undo", IconButton::Icon::undo }, redo { "Redo", IconButton::Icon::redo },
               dragMidiButton { "MIDI", IconButton::Icon::drag },
               lane { "Lane", IconButton::Icon::lane };
    juce::TextButton moreMenu { "MORE" };
    juce::ComboBox snapBox;
    PillSlider magnet { "MAGNET" };
    std::unique_ptr<ChoiceBox> keyBox, scaleBox;
    std::unique_ptr<PillToggle> scaleLock;

    juce::Rectangle<int> snapCaption;
    std::vector<juce::Rectangle<int>> separators;
    float time = 0.0f;
};

} // namespace bd
