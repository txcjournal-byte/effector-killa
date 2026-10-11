#pragma once

// Controls of the TrapWin effect cards in the CHAIN view (Docs/TRAPWIN-STYLE.md):
//   RackControl  - one parameter: flat TrapWin knob (accent value arc with glow, white pointer dot),
//                  stepped knob, vertical segmented pill (2..4 choices) or pill toggle.
//                  Label above, value below, IBM Plex Mono; colours from faceplate::palette(type).
//                  Mod rings: the face bounds handed to paintOverlay hug the value arc, so rings drawn
//                  at face.width / 2 + 3 sit just outside it. A mouse-down on a ring (hitRing) drags
//                  that assignment's depth instead of the value (onRingDrag).
//   SideButton   - small round icon pill (menu / power / solo / lock / remove) in the card's top-right row.
//   UnitLcd      - dark preset pill: accent dot, programme name (Plex Mono), chevron.
//   MsSwitch     - ST / MID / SIDE as a 3-segment mini pill (click a segment).

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

    // Ring dragging (optional): hitRing (mouse position relative to the knob centre, face radius)
    // returns an id (e.g. a mod assignment index / a macro range handle) or -1; onRingDrag (id, delta)
    // applies a change (delta in normalised units), onRingDragEnd (id) finishes it.
    std::function<int (juce::Point<float> fromCentre, float faceRadius)> hitRing;
    std::function<void (int id)> onRingDragStart;
    std::function<void (int id, float deltaDepth)> onRingDrag;
    std::function<void (int id)> onRingDragEnd;

    // The knob face (or segment block) in local coordinates - the place modulation rings go.
    juce::Rectangle<float> getFaceBounds() const;
    float getNormValue() const noexcept { return value; }

    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    juce::String valueText() const;
    int numSteps() const;
    int stepIndex() const;
    float stepToNorm (int step) const;
    void setFromUser (float norm, bool asCompleteGesture);
    void showChoiceMenu();
    juce::Rectangle<float> ledRowItem (int i) const;
    float labelHeight() const;
    float valueHeight() const;

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
    int ringDrag = -1;
    bool ringHover = false;
    juce::String label;
};

// ----------------------------------------------------------------------------------------------
class SideButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Icon { Menu, Power, Solo, Lock, Remove };
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
    bool dangerHover = false;    // hover tints red (remove)

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
    juce::Colour accent { 0xff9fc3ff };
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
    std::function<int()> getMode;          // 0 ST, 1 MID, 2 SIDE
    std::function<void()> onClick;         // legacy: cycle (used when onSelect is not set)
    std::function<void (int mode)> onSelect;
    ek::faceplate::Palette pal;
    void tick();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int segmentAt (juce::Point<float> p) const;
    int shown = -1, hover = -1;
};

} // namespace ek::ui
