#include "Faceplates.h"
// Placeholder – replaced by the faceplate designs for this group.
namespace ek::faceplate
{
Palette paletteSpaceGroup (EffectType) { return { juce::Colour (0xff24262b), juce::Colour (0xff15161a), juce::Colours::white, juce::Colour (0xff9fc3ff), juce::Colour (0xff3dff6e) }; }
void paintSpaceGroup (juce::Graphics& g, EffectType t, juce::Rectangle<float> plate, const State&, bool)
{
    g.setColour (palette (t).plate); g.fillRoundedRectangle (plate, 3.0f);
}
} // namespace ek::faceplate
