#pragma once

// Small building blocks of the rack UI: cards, push buttons with tooltips, labelled knobs, meters.

#include "../Controls.h"

namespace ek::ui
{
// TrapWin card (dark navy, thin ice-blue line).
void drawCard (juce::Graphics& g, juce::Rectangle<float> r, float corner = 10.0f);
void drawSmallLabel (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r,
                     juce::Justification j = juce::Justification::centred, juce::Colour c = juce::Colour (0xffa9b6cc), float size = 10.0f);

// ----------------------------------------------------------------------------------------------
// Push / toggle button. isLit() drives the lit state; momentary press via onPress.
class TwButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    TwButton (juce::String text, juce::Colour litColour, float fontHeight = 11.0f);

    std::function<void()> onClick;
    std::function<void (bool down)> onPress;
    std::function<bool()> isLit;
    std::function<bool()> isEnabledFn;
    bool blinkWhenLit = false;
    bool solid = false;          // always filled with its colour (KILL)

    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void tick();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String text;
    juce::Colour colour;
    float fontHeight;
    bool down = false, lastLit = false, lastBlink = false, lastEnabled = true;
};

// ----------------------------------------------------------------------------------------------
// Knob with its name above and the real value under it ("4.2 dB").
class LabeledKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LabeledKnob (juce::String name, Knob::Style style = Knob::Style::Small);

    Knob knob;
    std::function<juce::String()> valueText;
    void setName (const juce::String& n) { if (n != name) { name = n; repaint(); } }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void tick();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String name, shownValue;
    juce::Colour accent { 0xff9fc3ff };
};

// ----------------------------------------------------------------------------------------------
// Vertical peak meter (0..1 linear peak in, painted in dB).
class PeakMeter : public juce::Component
{
public:
    std::function<float()> getPeak;
    void tick();
    void paint (juce::Graphics&) override;

private:
    float shown = 0.0f, hold = 0.0f;
    int holdFrames = 0;
};

} // namespace ek::ui
