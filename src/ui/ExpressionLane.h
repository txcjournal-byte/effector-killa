/*
    TrapWin BEND DEMON
    ExpressionLane.h - the collapsible lane under the DRAW canvas (2.0): draw the PRESSURE or
    TIMBRE (CC74) curve, or set the VELOCITY, of the selected lines (all lines under the mouse when
    nothing is selected). Shares the canvas' time axis. Right-click resets.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/Pattern.h"

namespace bd
{

class VoiceField;
class PatternStore;

class ExpressionLane final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    enum class Mode { pressure = 0, timbre, velocity, release };

    ExpressionLane (VoiceField&, PatternStore&);

    void setMode (Mode m)                   { mode = m; repaint(); }
    Mode getMode() const noexcept           { return mode; }
    std::function<void()> onModeChanged;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    juce::Rectangle<float> tabArea (int index) const;
    float valueToY (float v) const;
    float yToValue (float y) const;
    std::vector<uint32_t> targets (double b0, double b1) const;
    void applyStroke();

    VoiceField& field;
    PatternStore& store;
    Mode mode = Mode::pressure;

    bool drawing = false;
    Pattern base;
    std::vector<uint32_t> baseTargets;
    std::vector<CurvePoint> stroke;    // absolute beat, value
};

} // namespace bd
