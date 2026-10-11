#pragma once

// Hardware controls that sit on the effect faceplates of the rack units (DESIGN-RACK.md section 6):
//   RackControl  - one parameter: rotary knob, stepped rotary selector, LED button row or toggle.
//                  Label above, value window below, tinted from faceplate::palette(type).
//   SideButton   - 26 x 26 square button of the side strip (menu / power / solo / lock) with a vector icon.
//   UnitLcd      - small black-glass programme display with a down arrow (unit presets).
//   MsSwitch     - ST / MID / SIDE push button in the corner of the plate.

#include "Widgets.h"
#include "Faceplates.h"

namespace ek::ui
{
// ----------------------------------------------------------------------------------------------
class RackControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { Rotary, Selector, LedRow, Toggle };

    // param: 0..7 = P1..P8 (spec != nullptr), 8 = unit DRY/WET (spec == nullptr)
    RackControl (int slot, int param, const ek::ParamSpec* spec, juce::RangedAudioParameter* hostParam,
                 const ek::faceplate::Palette& palette);
    ~RackControl() override;

    static Kind kindFor (const ek::ParamSpec* spec);
    static int preferredWidth (Kind k, const ek::ParamSpec* spec);

    const int slot, param;
    const Kind kind;

    std::function<void()> onGestureStart;                          // push undo
    std::function<bool (juce::Point<int> screenPos)> onRightClick;  // modulation / macro learn hook; true = handled
    std::function<void (juce::Graphics&, juce::Rectangle<float> face)> paintOverlay;
    std::function<float()> getEffectiveValue;                      // modulated value (normalised), optional

    // The knob face (or LED block) in local coordinates - the place modulation rings go.
    juce::Rectangle<float> getFaceBounds() const;
    float getNormValue() const noexcept { return value; }

    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::String valueText() const;
    int numSteps() const;
    int stepIndex() const;
    float stepToNorm (int step) const;
    void setFromUser (float norm, bool asCompleteGesture);
    void showChoiceMenu();
    juce::Rectangle<float> ledRowItem (int i) const;

    void paintRotary (juce::Graphics&, bool selector);
    void paintLedRow (juce::Graphics&);
    void paintToggle (juce::Graphics&);
    void paintLabel (juce::Graphics&);
    void paintValue (juce::Graphics&, const juce::String&);

    const ek::ParamSpec* spec;
    juce::RangedAudioParameter* hostParam;
    ek::faceplate::Palette pal;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float value = 0.0f, defaultValue = 0.0f, dragValue = 0.0f, shownEffective = -1.0f;
    juce::Point<float> lastDrag;
    bool dragging = false, moved = false, gestureOpen = false;
    juce::String label;
};

// ----------------------------------------------------------------------------------------------
class SideButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Icon { Menu, Power, Solo, Lock };
    SideButton (Icon icon, juce::Colour litColour);
    ~SideButton() override;

    std::function<void()> onClick;
    // Press-and-hold (optional): when set, holding the button longer than holdMs calls onHoldStart,
    // releasing it calls onHoldEnd and suppresses onClick (LEVEL-MATCHED POWER uses this).
    std::function<void()> onHoldStart, onHoldEnd;
    int holdMs = 250;
    bool isHeld() const noexcept { return held; }
    std::function<bool()> isLit;
    std::function<bool()> isEnabledFn;
    bool blinkWhenLit = false;

    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float> r, juce::Colour c);

private:
    void timerCallback() override;

    Icon icon;
    juce::Colour colour;
    bool down = false, held = false, lastLit = false, lastBlink = false, lastEnabled = true;
    bool litNow() const;
};

// ----------------------------------------------------------------------------------------------
class UnitLcd : public juce::Component, public juce::SettableTooltipClient
{
public:
    UnitLcd();
    std::function<void()> onClick;
    void setText (const juce::String& t, bool animate);
    void setReduceMotion (bool r) noexcept { reduceMotion = r; }
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String text;
    double scanStart = -1.0;
    bool reduceMotion = false;
};

// ----------------------------------------------------------------------------------------------
class MsSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    MsSwitch();
    std::function<int()> getMode;   // 0 ST, 1 MID, 2 SIDE
    std::function<void()> onClick;
    ek::faceplate::Palette pal;
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    int shown = -1;
};

// Shared look: a hardware rotary knob (shadow, ribbed skirt, black body, chrome cap, pointer).
void drawRackKnob (juce::Graphics&, juce::Point<float> centre, float radius, float angle, const ek::faceplate::Palette&, bool hot);

} // namespace ek::ui
