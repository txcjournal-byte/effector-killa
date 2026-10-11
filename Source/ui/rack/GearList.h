#pragma once

// Gear list: every effect, grouped, ready to drag into the rack.
// Drag an item into the rack to add / insert / replace, double-click to add at the end,
// drag a unit from the rack back here to remove it.

#include "RackView.h"

namespace ek::ui
{
namespace gear
{
    struct Group
    {
        juce::String name;
        std::vector<ek::EffectType> types;
    };
    const std::vector<Group>& groups();
    juce::String describe (ek::EffectType t);
}

class GearList : public juce::Component, public juce::DragAndDropTarget, public juce::TooltipClient
{
public:
    explicit GearList (EffectorKillaAudioProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    juce::String getTooltipFor (juce::Point<int> p) const;
    juce::String getTooltip() override { return getTooltipFor (getMouseXYRelative()); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (const juce::String&)> onMessage;

private:
    struct Row
    {
        bool header = false;
        juce::String text;
        ek::EffectType type = ek::EffectType::None;
        juce::Rectangle<int> bounds;
    };
    void layoutRows();
    int rowAt (juce::Point<int> p) const;

    EffectorKillaAudioProcessor& proc;
    std::vector<Row> rows;
    int hover = -1, pressed = -1;
    bool dragging = false, removeHover = false;
};

} // namespace ek::ui
