#pragma once

// Generic cabinet controls: rotary knob, lit push button, 3-position slide switch, toggle switch.
// All of them live in cabinet space (see Layout.h) and paint themselves opaque over the background.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Theme.h"

namespace ek::ui
{
// ----------------------------------------------------------------------------------------------
// Rotary knob. Drag up/down = value, Shift = fine, double-click = default, wheel = step.
// Either bound to a host parameter or driven through get/set callbacks.
class Knob : public juce::Component
{
public:
    enum class Style { Macro, Small };

    Knob (Style s = Style::Macro);
    ~Knob() override;

    void bindTo (juce::RangedAudioParameter& param);
    std::function<void()> onGestureStart, onGestureEnd;
    std::function<void (float norm)> onUserChange;   // any change made by the user (for the OSD)
    std::function<float()> getDisplayValue;          // optional: value to draw (e.g. modulated macro)

    void setNormValue (float v, juce::NotificationType n);
    float getNormValue() const noexcept { return value; }
    void setDefault (float d) noexcept { defaultValue = d; }
    void setHot (bool h) { if (hot != h) { hot = h; repaint(); } }
    void tick(); // repaints when the displayed (modulated) value moved

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void userSet (float v);
    Style style;
    float value = 0.5f, defaultValue = 0.5f, dragStartValue = 0.0f, lastDisplayed = -1.0f;
    juce::Point<float> lastDragPos;
    bool hot = false, dragging = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::RangedAudioParameter* parameter = nullptr;
};

// ----------------------------------------------------------------------------------------------
class LitButton : public juce::Component
{
public:
    LitButton (juce::String text, juce::Colour colour, float fontHeight = 17.0f);

    std::function<void()> onClick;
    std::function<void (bool down)> onPress;   // momentary press / release
    std::function<bool()> isLit;
    bool blinkWhenLit = false;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();

private:
    juce::String text;
    juce::Colour colour;
    float fontHeight;
    bool down = false, lastLit = false, lastBlink = false;
};

// ----------------------------------------------------------------------------------------------
// 3-position slide switch (PG / R / UNRATED, AV1 / AV2 / AV3).
class ThreeWaySwitch : public juce::Component
{
public:
    // positions are x coordinates in local space
    ThreeWaySwitch (std::array<float, 3> positionsX, juce::Rectangle<float> track, float thumbW, bool litThumb);

    std::function<void (int)> onChange;
    std::function<int()> getPosition;
    std::array<juce::Point<float>, 3> leds {};  // optional LEDs (local coords)
    bool showLeds = false;
    std::array<juce::Rectangle<float>, 3> clickZones {};

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();

private:
    int nearest (float x) const;
    std::array<float, 3> pos;
    juce::Rectangle<float> track;
    float thumbW;
    bool litThumb;
    float dragX = -1.0f;
    int shown = -1;
};

// ----------------------------------------------------------------------------------------------
// Round-thumb toggle (BASIC CABLE / PREMIUM CABLE).
class CableToggle : public juce::Component
{
public:
    CableToggle (juce::Rectangle<float> track, juce::Point<float> led);
    std::function<bool()> isOn;
    std::function<void (bool)> onChange;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

private:
    juce::Rectangle<float> track;
    juce::Point<float> led;
    bool shown = false;
};

// ----------------------------------------------------------------------------------------------
// Small arrow push button (PROGRAM up / down).
class ArrowButton : public juce::Component
{
public:
    explicit ArrowButton (bool up);
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool up, down = false;
};

} // namespace ek::ui
