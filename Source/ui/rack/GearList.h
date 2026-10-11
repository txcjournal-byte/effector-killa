#pragma once

// FX LIBRARY (Docs/TRAPWIN-STYLE.md): the left sidebar (220 px) - one TrapWin card with the chrome title,
// a search pill and a miniature chip of every effect card (faceplate::paintFaceplate (..., miniature = true),
// designed at 264 x 41 and scaled to the column, aspect kept), grouped
// COLOR / FILTER & EQ / DYNAMICS / MODULATION / SPACE / STEREO.
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

    static constexpr int preferredWidth = 220;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void setFilter (const juce::String& text);   // same as typing into the search field
    std::function<void (const juce::String&)> onMessage;

    // Snapshot tool only: force the drop-to-remove look / a hovered chip.
    void setSnapshotState (bool showRemove, ek::EffectType hoverType);

    class Content;

private:
    EffectorKillaAudioProcessor& proc;
    std::unique_ptr<Content> content;
    juce::Viewport viewport;
    juce::TextEditor search;
    bool removeHover = false;
};

} // namespace ek::ui
