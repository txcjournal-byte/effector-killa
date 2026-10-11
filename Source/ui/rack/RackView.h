#pragma once

// The rack: units stacked top to bottom (signal flows downwards), every unit shows all its
// controls. Drag & drop:
//   gear item  -> top / bottom edge of a unit = insert there (the rack moves down)
//              -> centre of a unit            = replace it
//              -> empty space under the units = add at the end
//   unit grip  -> another position            = move
//              -> back onto the gear list     = remove

#include "Widgets.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
namespace dnd
{
    juce::String fx (ek::EffectType t);
    juce::String unit (int slot);
    bool parseFx (const juce::var& d, ek::EffectType& t);
    bool parseUnit (const juce::var& d, int& slot);
}

class RackView;

class UnitComponent : public juce::Component
{
public:
    UnitComponent (EffectorKillaAudioProcessor& p, RackView& owner, int slot);

    int getSlot() const noexcept { return slot; }
    void tick();
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int height = 116;

private:
    void showPresetMenu();
    void showUnitMenu();
    juce::Rectangle<int> gripArea() const { return { 0, 0, 36, getHeight() }; }
    juce::String currentProgrammeName() const;

    EffectorKillaAudioProcessor& proc;
    RackView& owner;
    const int slot;
    const ek::EffectType type;
    juce::OwnedArray<LabeledKnob> knobs;
    std::vector<int> knobParam;     // parameter index per knob (8 = MIX)
    TwButton presetBtn, powerBtn, soloBtn, msBtn, lockBtn, removeBtn, menuBtn;
    bool dragStarted = false;
    juce::String shownProgramme;
    bool lastPaused = false, lastDimmed = false;
};

class RackView : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit RackView (EffectorKillaAudioProcessor& p);

    void refresh (bool repaintAll = true);   // rebuilds when the rack structure changed
    void tick();
    int getIdealHeight() const;
    bool isDimmed (int slot) const;   // another unit is soloed

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    // DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (const juce::String&)> onMessage;
    static void showAddMenu (EffectorKillaAudioProcessor& p, juce::Component* target, std::function<void (ek::EffectType)> picked);

private:
    struct DropTarget
    {
        enum Kind { None, Insert, Replace, Append } kind = None;
        int slot = -1;     // slot to insert before / replace
        float y = 0.0f;    // indicator position
    };
    DropTarget dropTargetFor (juce::Point<int> pos, bool fromUnit, int fromSlot) const;
    juce::String signature() const;
    juce::Rectangle<int> addZone() const;
    int lastOccupied() const;
    bool isFull() const;

    EffectorKillaAudioProcessor& proc;
    juce::OwnedArray<UnitComponent> units;
    juce::String builtFor;
    DropTarget drop;
};

} // namespace ek::ui
