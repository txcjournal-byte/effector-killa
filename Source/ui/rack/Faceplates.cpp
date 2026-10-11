#include "Faceplates.h"

namespace ek::faceplate
{
static bool isColorGroup (EffectType t) { return (int) t >= (int) EffectType::CassettePlug && (int) t <= (int) EffectType::Slap; }

juce::Rectangle<float> nameArea (juce::Rectangle<float> p)      { return { p.getX() + 22.0f, p.getY() + 12.0f, 220.0f, 52.0f }; }
juce::Rectangle<float> presetLcdArea (juce::Rectangle<float> p) { return { p.getRight() - 222.0f, p.getY() + 12.0f, 200.0f, 22.0f }; }
juce::Rectangle<float> liveArea (EffectType, juce::Rectangle<float> p) { return { p.getX() + 22.0f, p.getY() + 70.0f, 220.0f, 58.0f }; }
juce::Rectangle<float> knobArea (EffectType, juce::Rectangle<float> p) { return { p.getX() + 256.0f, p.getY() + 40.0f, p.getWidth() - 278.0f, 94.0f }; }

Palette palette (EffectType t)
{
    if (t == EffectType::None) return { juce::Colour (0xff1a1c21), juce::Colour (0xff101115), juce::Colours::white, juce::Colour (0xff9fc3ff), juce::Colour (0xff9fc3ff) };
    return isColorGroup (t) ? paletteColorGroup (t) : paletteSpaceGroup (t);
}

void paintFaceplate (juce::Graphics& g, EffectType t, juce::Rectangle<float> plate, const State& s, bool miniature)
{
    if (t == EffectType::None) return;
    if (isColorGroup (t)) paintColorGroup (g, t, plate, s, miniature);
    else                  paintSpaceGroup (g, t, plate, s, miniature);
    if (! s.powered || s.soloDimmed)
        g.setColour (juce::Colours::black.withAlpha (s.powered ? 0.45f : 0.6f)), g.fillRect (plate);
}
} // namespace ek::faceplate
