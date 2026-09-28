#pragma once

// The shelf with 8 VHS cassettes = the 8 rack slots.
// Click = select (cassette slides out and lights up), drag = reorder, right-click = slot menu.

#include "../../PluginProcessor.h"
#include "Layout.h"
#include "../Theme.h"

namespace ek::ui
{
class CassetteShelf : public juce::Component
{
public:
    explicit CassetteShelf (EffectorKillaAudioProcessor& p);

    std::function<void (int slot)> onSelect;   // -1 = deselect
    std::function<void (const juce::String&)> onMessage;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void showSlotMenu (int slot);
    void showEffectMenu (int slot);
    void tick(); // animation (slide-out)

    static void drawCassette (juce::Graphics& g, juce::Rectangle<float> r, const SlotState& s, int slotIndex,
                              bool selected, bool screening, float glow);

private:
    int slotAt (juce::Point<float> p) const;
    int dropIndexFor (float x) const;
    juce::Rectangle<float> cassetteRect (int slot) const; // in local coordinates

    EffectorKillaAudioProcessor& proc;
    int pressed = -1, dragFrom = -1, hover = -1;
    float dragX = 0.0f, grabOffset = 0.0f;
    bool dragging = false;
    std::array<float, kNumSlots> slide {};   // 0..1 slide-out animation per slot
};
} // namespace ek::ui
