#pragma once

// Gear (DESIGN-RACK.md section 7): the right-hand column (280 px) with a miniature of every effect's
// faceplate (rendered by faceplate::paintFaceplate (..., miniature = true), ~264 x 41), grouped
// COLOR / FILTER & EQ / DYNAMICS / MODULATION / SPACE / STEREO, with a search field.
//   hover        = the miniature lights up + tooltip with the effect description
//   double-click = add at the end of the rack
//   drag         = into the rack (edge = insert, centre = replace, under the last unit = add)
//   a unit dragged back here = remove (the column turns red: "DROP TO REMOVE")

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

    static constexpr int preferredWidth = 280;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void setFilter (const juce::String& text);   // same as typing into the search field
    std::function<void (const juce::String&)> onMessage;

    class Content;

private:
    EffectorKillaAudioProcessor& proc;
    std::unique_ptr<Content> content;
    juce::Viewport viewport;
    juce::TextEditor search;
    bool removeHover = false;
};

} // namespace ek::ui
