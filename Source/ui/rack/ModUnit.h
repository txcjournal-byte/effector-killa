#pragma once

// MOD card (TrapWin style, Docs/TRAPWIN-STYLE.md): any width >= kMinWidth (designed for 752). Two modes:
//   collapsed (default, collapsedHeight = 36): a slim strip - chrome "MOD", four source chips
//       (LFO 1 / LFO 2 / ENV / SEQ, each with a live dot and its draggable glowing jack) and an "EDIT" pill.
//   expanded (expandedHeight = 104): title column (MOD, ACTIVE, CLOSE pill) + the four source sections, each
//       with its chip + jack on top, a glowing glass screen and flat 30 px knobs (all text >= 11 px).
// The editor owns the height: it listens to onExpandedChanged and gives the unit collapsedHeight or
// expandedHeight (the layout follows isExpanded(), not the bounds). Clicking the EDIT / CLOSE pill or a chip
// (outside its jack) toggles the mode.
//
// Patching: grab a jack and drag it onto any knob in the rack. ModUnit does not know the rack, it only
// reports the gesture; the editor / RackView hit-tests the knob and calls
//   proc.addModAssignment (source, slot, param, depth)   (or addMacroModAssignment for a macro knob)
//
//   onHandleDrag (source, screenPos)   while the jack is dragged (every mouse move)
//   onHandleDrop (source, screenPos)   when it is released (also when released over the unit itself)
//
// Right-click a jack = list / remove the assignments of that source.
// Helpers for the rack knobs: paintModRing() draws the coloured depth ring, paintPatchCable() a cable
// (e.g. from getHandleScreenPosition() to the mouse while dragging, in an overlay component).

#include "Widgets.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
// Colour of a modulation source (LFO 1 #9FC3FF, LFO 2 #B07CFF, ENV #5FE0B0, SEQ #FFD23F).
juce::Colour modSourceColour (int source);

// Depth ring around a rotary knob. knobBounds = the knob's square, baseNorm = the knob value (0..1),
// depth = assignment depth (-1..+1). Bipolar sources (LFOs) draw base +- depth, unipolar (ENV / SEQ)
// base .. base + depth. currentNorm >= 0 also draws a dot at the live modulated value.
// ringIndex > 0 draws further rings outside (several assignments on one knob).
void paintModRing (juce::Graphics& g, juce::Rectangle<float> knobBounds, float baseNorm, float depth, juce::Colour colour,
                   bool bipolar = true, float currentNorm = -1.0f, int ringIndex = 0);

// Patch cable between two points: a thin glowing bezier in the source colour with glowing end dots.
void paintPatchCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, juce::Colour colour);

class ModUnit : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModUnit (EffectorKillaAudioProcessor& p);
    ~ModUnit() override;

    static constexpr int collapsedHeight = 36, expandedHeight = 104;
    static constexpr int kWidth = 752, kMinWidth = 640;
    static constexpr int kHeight = expandedHeight;   // legacy name

    bool isExpanded() const noexcept { return expanded; }
    void setExpanded (bool shouldBeExpanded);         // calls onExpandedChanged when it changes
    std::function<void()> onExpandedChanged;
    int getCurrentHeight() const noexcept { return expanded ? expandedHeight : collapsedHeight; }

    std::function<void (int source, juce::Point<int> screenPos)> onHandleDrag;
    std::function<void (int source, juce::Point<int> screenPos)> onHandleDrop;

    void tick();      // call at ~30 Hz (animated displays, LEDs)
    void refresh();   // program changed (preset, undo ...)

    int getDraggingSource() const noexcept { return dragSource; }      // -1 = none
    juce::Point<int> getHandleScreenPosition (int source) const;
    juce::Rectangle<int> getHandleBounds (int source) const;           // local

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    class MiniKnob;

private:
    struct Section { int source; juce::Rectangle<int> area, chip, display, jack; juce::String title; };

    void paintTitle (juce::Graphics&);
    void paintChip (juce::Graphics&, const Section&);
    void paintEditPill (juce::Graphics&);
    void paintSection (juce::Graphics&, const Section&);
    void paintDisplay (juce::Graphics&, const Section&);
    void paintJack (juce::Graphics&, const Section&);
    int jackAt (juce::Point<int> p) const;
    int seqStepAt (juce::Point<int> p) const;
    void setSeqFromMouse (juce::Point<int> p);
    void showAssignmentMenu (int source);
    juce::String countText (int source) const;
    juce::Rectangle<float> seqPlot() const;

    EffectorKillaAudioProcessor& proc;
    std::array<Section, kNumModSources> sections;
    juce::Rectangle<int> syncButtons[2];
    juce::Rectangle<int> titleArea, editPill;
    bool expanded = false, editHover = false;
    std::vector<std::unique_ptr<MiniKnob>> knobs;
    std::array<float, 64> envHistory {};
    int envPos = 0;
    int dragSource = -1, hoverJack = -1, seqDragStep = -1;
    float lastSeqValue = -1.0f;
    std::array<float, kNumModSources> shownValue {};
    int shownStep = -1;
};

} // namespace ek::ui
