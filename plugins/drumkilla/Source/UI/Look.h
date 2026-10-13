#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Look
{
    namespace Palette
    {
        const juce::Colour red        { 0xffcc1c15 };
        const juce::Colour redBright  { 0xffff5a46 };
        const juce::Colour redGlow    { 0xffe0241c };
        const juce::Colour ink        { 0xff151412 };   // dark label text on plaster
        const juce::Colour track      { 0xff141311 };
        const juce::Colour boxFill    { 0xff080808 };
        const juce::Colour boxEdge    { 0xff2e2c29 };
        const juce::Colour text       { 0xffe8e3d8 };
        const juce::Colour textDim    { 0xff8f8a80 };
        const juce::Colour panelEdge  { 0xff1d1b19 };
    }

    // Shared fonts (loaded once from binary data)
    struct Fonts
    {
        Fonts();
        juce::Font mono (float height) const;
        juce::Font sans (float height) const;
        juce::Font bold (float height) const;

        juce::Typeface::Ptr monoFace, sansFace, boldFace;
    };

    void drawPanel (juce::Graphics&, juce::Rectangle<float> area, bool dark = false);
    void drawLabel (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area, float height,
                    juce::Colour colour = Palette::ink, juce::Justification just = juce::Justification::centred);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        juce::Font getPopupMenuFont() override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override;

        juce::Image knobFace;
        juce::SharedResourcePointer<Fonts> fonts;
    };
}
