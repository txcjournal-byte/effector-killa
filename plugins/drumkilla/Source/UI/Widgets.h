#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Look.h"

namespace UI
{
    using APVTS = juce::AudioProcessorValueTreeState;

    //==========================================================================
    // Rotary knob bound to a parameter: label above, value box below.
    // Ctrl/Cmd + drag = fine adjustment, double click = default value.
    class Knob : public juce::Component
    {
    public:
        Knob (APVTS&, const juce::String& paramID, const juce::String& label, const juce::String& tooltip,
              bool bipolar = false);

        void setKnobArea (juce::Rectangle<int> knobBoundsInParent, float labelHeight = 30.0f);
        void paint (juce::Graphics&) override;
        void resized() override;

        juce::Slider& getSlider() noexcept { return slider; }

        juce::Colour labelColour = Look::Palette::ink;   // light text when the knob sits on a dark panel
        void setDark() { labelColour = Look::Palette::text; slider.getProperties().set ("dark", true); }

    private:
        struct FineSlider : juce::Slider
        {
            void mouseDown (const juce::MouseEvent& e) override
            {
                const auto fine = e.mods.isCtrlDown() || e.mods.isCommandDown();
                setMouseDragSensitivity (fine ? 1600 : 250);
                Slider::mouseDown (e);
            }
        };

        FineSlider slider;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
        juce::String label;
        float labelH = 30.0f;
        juce::SharedResourcePointer<Look::Fonts> fonts;
    };

    //==========================================================================
    // Button with a red LED and a text label, bound to a bool parameter
    class LedToggle : public juce::Button
    {
    public:
        LedToggle (APVTS&, const juce::String& paramID, const juce::String& buttonText, const juce::String& tooltip);
        void paintButton (juce::Graphics&, bool over, bool down) override;
        bool big = false;

    private:
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
        juce::SharedResourcePointer<Look::Fonts> fonts;
    };

    //==========================================================================
    // Row of segment buttons for a choice parameter
    class ChoiceSelector : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ChoiceSelector (APVTS&, const juce::String& paramID, const juce::String& tooltip);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::Rectangle<float> segment (int i) const;
        juce::StringArray names;
        int selected = 0;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::SharedResourcePointer<Look::Fonts> fonts;
    };

    //==========================================================================
    // Simple clickable text button in the plugin style
    class FlatButton : public juce::Button
    {
    public:
        explicit FlatButton (const juce::String& buttonText) : Button (buttonText) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
        bool active = false;
        float textHeight = 24.0f;

    private:
        juce::SharedResourcePointer<Look::Fonts> fonts;
    };

    //==========================================================================
    // Vertical IN / OUT peak meters
    class Meter : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void setLevels (float inDb, float outDb) { levels[0] = inDb; levels[1] = outDb; repaint(); }

    private:
        float levels[2] { -100.0f, -100.0f };
        juce::SharedResourcePointer<Look::Fonts> fonts;
    };
}
