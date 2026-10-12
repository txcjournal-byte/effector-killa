#pragma once

// FX LIBRARY drawer (Docs/TRAPWIN-STYLE.md), designed for 232 x 608: an overlay that slides in from the left
// (the editor shows / hides it). One TrapWin card with the chrome title, a search pill and a miniature chip of
// every effect card - faceplate::paintFaceplate (..., miniature = true) rendered at the real row size
// (rowHeight = 32, ~204 px wide; 2x for sharpness) - grouped COLOR / FILTER & EQ / DYNAMICS / MODULATION / SPACE /
// STEREO. When the rows do not fit, the list scrolls smoothly (mouse wheel is animated).
//   hover        = the miniature lights up + tooltip with the effect description
//   double-click = add at the end of the rack
//   drag         = into the rack (edge = insert, centre = replace, under the last unit = add)
//   a unit dragged back here = remove (the card glows red: "DROP TO REMOVE")

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
    juce::String categoryName (ek::EffectType t);   // SATURATION, DISTORTION ...

    // Cached miniature of an effect's faceplate at the given size (rendered at 2x for sharpness).
    juce::Image miniature (ek::EffectType t, int w, int h);
}

class GearList : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit GearList (EffectorKillaAudioProcessor& p);
    ~GearList() override;

    static constexpr int preferredWidth = 232, preferredHeight = 608;
    static constexpr int rowHeight = 32;   // miniature chip height (width = drawer width - 2 x 14 - scroll bar)

    // Called right after a chip drag has started (the drag image already follows the mouse), so the editor can
    // hide the drawer and uncover the chain. Hiding the GearList here is safe: the drag keeps running.
    std::function<void()> onDragStarted;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void setFilter (const juce::String& text);   // same as typing into the search field
    std::function<void (const juce::String&)> onMessage;

    // Snapshot tool only: force the drop-to-remove look / a hovered chip.
    void setSnapshotState (bool showRemove, ek::EffectType hoverType);
    void scrollForSnapshot (float proportion);   // 0 = top, 1 = end (no animation)

    class Content;

private:
    void releaseSearchFocus();

    EffectorKillaAudioProcessor& proc;
    std::unique_ptr<Content> content;
    juce::Viewport viewport;
    juce::TextEditor search;
    std::unique_ptr<juce::MouseListener> clickOutside;   // releases the search focus on clicks elsewhere
    bool removeHover = false;
};

} // namespace ek::ui
