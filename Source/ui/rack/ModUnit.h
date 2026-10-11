#pragma once

// MOD unit (932 x 96): the fixed hardware unit under the performer strip with the four modulation
// sources of the engine (LFO 1, LFO 2, ENV follower, 16-step SEQ). Each section has small knobs,
// a mini display and a patch-jack "handle".
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
// Colour of a modulation source (LFO 1 ice blue, LFO 2 violet, ENV amber, SEQ green).
juce::Colour modSourceColour (int source);

// Depth ring around a rotary knob. knobBounds = the knob's square, baseNorm = the knob value (0..1),
// depth = assignment depth (-1..+1). Bipolar sources (LFOs) draw base +- depth, unipolar (ENV / SEQ)
// base .. base + depth. currentNorm >= 0 also draws a dot at the live modulated value.
// ringIndex > 0 draws further rings outside (several assignments on one knob).
void paintModRing (juce::Graphics& g, juce::Rectangle<float> knobBounds, float baseNorm, float depth, juce::Colour colour,
                   bool bipolar = true, float currentNorm = -1.0f, int ringIndex = 0);

// Patch cable between two points (sagging, with plugs), e.g. while a handle is dragged.
void paintPatchCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, juce::Colour colour);

class ModUnit : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModUnit (EffectorKillaAudioProcessor& p);
    ~ModUnit() override;

    static constexpr int kWidth = 932, kHeight = 96;

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
    struct Section { int source; juce::Rectangle<int> area, display, jack; juce::String title; };

    void paintChassis (juce::Graphics&);
    void paintSection (juce::Graphics&, const Section&);
    void paintDisplay (juce::Graphics&, const Section&);
    void paintJack (juce::Graphics&, const Section&);
    int jackAt (juce::Point<int> p) const;
    int seqStepAt (juce::Point<int> p) const;
    void setSeqFromMouse (juce::Point<int> p);
    void showAssignmentMenu (int source);
    juce::String countText (int source) const;

    EffectorKillaAudioProcessor& proc;
    std::array<Section, kNumModSources> sections;
    juce::Rectangle<int> syncButtons[2];
    std::vector<std::unique_ptr<MiniKnob>> knobs;
    std::array<float, 64> envHistory {};
    int envPos = 0;
    int dragSource = -1, hoverJack = -1, seqDragStep = -1;
    float lastSeqValue = -1.0f;
    std::array<float, kNumModSources> shownValue {};
    int shownStep = -1;
};

} // namespace ek::ui
