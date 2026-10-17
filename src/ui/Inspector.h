/*
    TrapWin BEND DEMON
    Inspector.h - the right-hand panel of the 2.0 editor (replaces the 1.x gesture dock).

      NOTE    the selected notes (lines): velocity, smooth / straight, vibrato, transpose,
              APPLY SHAPE (Slide, Dive, Wobble ... with a mini curve each), edit actions
      SHAPES  how each shape behaves (amount, time ...) - used by APPLY - and whether it is also
              applied live to the piano-roll notes
      SOUND   built-in sounds or a loaded plugin, volume
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "GesturePad.h"
#include "GlowKnob.h"
#include "../engine/Pattern.h"

class BendDemonProcessor;

namespace bd
{

class LineEditController;

/** APPLY button: mini curve + plain name. */
class ShapeButton final : public juce::Button
{
public:
    ShapeButton (const juce::String& name, GesturePad::Icon i) : juce::Button (name), icon (i) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    GesturePad::Icon icon;
};

class Inspector final : public juce::Component
{
public:
    enum class Tab { note = 0, shapes, sound };

    Inspector (BendDemonProcessor&, LineEditController&);
    ~Inspector() override;

    void setTab (Tab);
    Tab getTab() const noexcept      { return tab; }
    std::function<void()> onTabChanged;
    std::function<void()> onLoadSynth;

    void tick (float t);
    void resized() override;
    void paint (juce::Graphics&) override;

    void showShape (int index);      // 0..7 shapes, 8 = live feel (expression)

private:
    juce::Slider& makeSlider (const juce::String& name);
    juce::Slider& makeParamSlider (const char* paramId, const juce::String& caption);
    juce::Component* makeChoice (const char* paramId, const juce::String& caption);
    juce::Component* makePill (const char* paramId, const juce::String& caption);
    void layoutNote (juce::Rectangle<int>);
    void layoutShapes (juce::Rectangle<int>);
    void layoutSound (juce::Rectangle<int>);
    void refreshNoteValues();
    void editSelected (std::function<void (PatternLine&)>, const juce::String& name, bool live);
    void updateVisibility();

    BendDemonProcessor& processor;
    LineEditController& controller;
    Tab tab = Tab::note;
    int shape = 0;
    float scale = 1.0f;

    juce::OwnedArray<juce::TextButton> tabButtons;

    // NOTE
    std::vector<std::unique_ptr<juce::Slider>> noteSliders;
    juce::Slider* velocity = nullptr, *releaseVel = nullptr, *vibDepth = nullptr, *vibFade = nullptr;
    juce::ComboBox lfoShape, lfoSpeed;
    juce::TextButton polarity { "BIPOLAR" };
    juce::TextButton smoothCurve { "SMOOTH" }, straightCurve { "STRAIGHT" };
    juce::OwnedArray<juce::TextButton> transposeButtons, actionButtons;
    juce::OwnedArray<ShapeButton> applyButtons;
    juce::Rectangle<int> noteHeader, vibCaption, transposeCaption, applyCaption, actionCaption;
    uint32_t lastVersion = 0;
    bool sliderDragging = false;

    // SHAPES
    std::vector<std::unique_ptr<GesturePad>> tiles;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::vector<juce::Component*>> shapeItems;    // per shape (9)
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::Rectangle<int> tilesCaption, shapeTitle, shapeHint;

    // SOUND
    juce::OwnedArray<juce::TextButton> soundButtons;
    juce::TextButton loadSynth { "LOAD PLUGIN..." }, useHosted { "USE" }, editHosted { "EDIT" };
    juce::Slider* volume = nullptr;
    juce::Rectangle<int> internalCaption, hostedCaption, outputCaption, hostedNameArea, settingsHint;
    juce::String hostedName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Inspector)
};

} // namespace bd
