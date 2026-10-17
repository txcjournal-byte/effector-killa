/*
    TrapWin BEND DEMON
    StatusBar.h - 2.0: the one-line help at the bottom (what the thing under the mouse does, in
    plain words) and the notice banner (e.g. "this synth has no MPE").
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Palette.h"
#include "Fonts.h"

namespace bd
{

class StatusBar final : public juce::Component
{
public:
    void setTexts (const juce::String& left, const juce::String& right)
    {
        if (left == hint && right == info) return;
        hint = left; info = right;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float s = (float) getWidth() / 1040.0f;
        g.fillAll (pal::ground0);
        g.setColour (pal::hairline);
        g.fillRect (0, 0, getWidth(), 1);
        auto r = getLocalBounds().reduced (juce::roundToInt (12 * s), 0).withTrimmedRight (16);   // resize corner
        g.setFont (fonts::ui (juce::jmax (10.0f, 12.5f * s)));
        g.setColour (pal::textDim);
        const int infoW = juce::roundToInt (330 * s);
        g.drawFittedText (info, r.removeFromRight (infoW), juce::Justification::centredRight, 1, 0.8f);
        g.setColour (pal::text.withAlpha (0.85f));
        g.drawFittedText (hint, r.withTrimmedRight (juce::roundToInt (12 * s)), juce::Justification::centredLeft, 1, 0.75f);
    }

private:
    juce::String hint, info;
};

/** A calm notice strip with up to two actions. */
class NoticeBanner final : public juce::Component
{
public:
    NoticeBanner()
    {
        for (auto* b : { &action, &dismiss })
        {
            b->setWantsKeyboardFocus (false);
            b->setMouseClickGrabsKeyboardFocus (false);
            addAndMakeVisible (b);
        }
        dismiss.setButtonText ("OK");
    }

    void show (const juce::String& message, const juce::String& actionText, std::function<void()> onAction, std::function<void()> onDismiss)
    {
        text = message;
        action.setButtonText (actionText);
        action.setVisible (actionText.isNotEmpty());
        action.onClick = std::move (onAction);
        dismiss.onClick = std::move (onDismiss);
        setVisible (true);
        resized();
        repaint();
    }

    void resized() override
    {
        const float s = (float) getWidth() / 760.0f;
        auto r = getLocalBounds().reduced (juce::roundToInt (8 * juce::jmin (1.4f, s)));
        dismiss.setBounds (r.removeFromRight (juce::roundToInt (56 * juce::jmin (1.4f, s))));
        r.removeFromRight (6);
        if (action.isVisible())
            action.setBounds (r.removeFromRight (juce::roundToInt (170 * juce::jmin (1.4f, s))));
        textArea = r.withTrimmedLeft (juce::roundToInt (26 * juce::jmin (1.4f, s)));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (pal::raised);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (pal::amber.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 14.0f, r.getCentreY() }));
        g.setColour (pal::text);
        g.setFont (fonts::ui (juce::jlimit (10.0f, 13.5f, (float) getHeight() * 0.32f)));
        g.drawFittedText (text, textArea, juce::Justification::centredLeft, 2, 0.85f);
    }

private:
    juce::String text;
    juce::TextButton action, dismiss;
    juce::Rectangle<int> textArea;
};

} // namespace bd
