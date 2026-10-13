#include "Widgets.h"

using namespace juce;
using namespace Look;

namespace UI
{

//==============================================================================
Knob::Knob (APVTS& state, const String& paramID, const String& labelText, const String& tooltip, bool bipolar)
    : label (labelText)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (250);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);

    if (auto* p = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    slider.onValueChange = [this] { repaint(); };
}

void Knob::setKnobArea (Rectangle<int> knobBounds, float labelHeight)
{
    labelH = labelHeight;
    const auto boxH = (int) (labelHeight * 1.05f);
    setBounds (knobBounds.getX() - 20, knobBounds.getY() - (int) labelHeight - 8,
               knobBounds.getWidth() + 40, knobBounds.getHeight() + (int) labelHeight + boxH + 16);
}

void Knob::resized()
{
    const auto boxH = (int) (labelH * 1.05f);
    slider.setBounds (20, (int) labelH + 8, getWidth() - 40, getHeight() - (int) labelH - boxH - 16);
}

void Knob::paint (Graphics& g)
{
    const auto alpha = isEnabled() ? 1.0f : 0.4f;
    drawLabel (g, label, Rectangle<float> (0.0f, 0.0f, (float) getWidth(), labelH), labelH, labelColour.withMultipliedAlpha (alpha));

    const auto boxH = labelH * 1.05f;
    const auto text = slider.getTextFromValue (slider.getValue());
    auto font = fonts->mono (boxH * 0.72f);
    const auto textW = GlyphArrangement::getStringWidth (font, text);
    const auto boxW = jmax (boxH * 2.4f, textW + boxH * 0.8f);
    const Rectangle<float> box ((float) getWidth() * 0.5f - boxW * 0.5f, (float) getHeight() - boxH - 2.0f, boxW, boxH);

    g.setColour (Palette::boxFill.withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (box, 2.0f);
    g.setColour (Palette::boxEdge);
    g.drawRoundedRectangle (box.reduced (0.5f), 2.0f, 1.0f);
    g.setColour (Palette::text.withMultipliedAlpha (alpha));
    g.setFont (font);
    g.drawText (text, box, Justification::centred, false);
}

//==============================================================================
LedToggle::LedToggle (APVTS& state, const String& paramID, const String& buttonText, const String& tooltip)
    : Button (buttonText)
{
    setClickingTogglesState (true);
    setTooltip (tooltip);
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, *this);
}

void LedToggle::paintButton (Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto on = getToggleState();

    g.setGradientFill (ColourGradient (Colour (0xff1e1d1b), 0.0f, r.getY(), Colour (0xff0a0a0a), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 3.0f, 1.5f);
    g.setColour (Colours::white.withAlpha (over ? 0.35f : 0.18f));
    g.drawRoundedRectangle (r.reduced (3.0f), 2.0f, 1.0f);

    const auto ledSize = jmin (r.getHeight() * (big ? 0.34f : 0.42f), 18.0f);
    const auto ledCentre = big ? Point<float> (r.getCentreX(), r.getY() + r.getHeight() * 0.33f)
                               : Point<float> (r.getRight() - r.getHeight() * 0.5f, r.getCentreY());
    const auto led = Rectangle<float> (ledSize, ledSize).withCentre (ledCentre);

    g.setColour (Colour (0xff050505));
    g.fillRoundedRectangle (led.expanded (3.0f), 2.5f);
    if (on)
    {
        g.setGradientFill (ColourGradient (Palette::redGlow.withAlpha (0.45f), ledCentre,
                                           Palette::redGlow.withAlpha (0.0f), ledCentre.translated (ledSize * 2.0f, 0.0f), true));
        g.fillEllipse (Rectangle<float> (ledSize * 4.0f, ledSize * 4.0f).withCentre (ledCentre));
        g.setGradientFill (ColourGradient (Colour (0xffffe0d6), ledCentre, Colour (0xffd0140c),
                                           ledCentre.translated (ledSize * 0.62f, 0.0f), true));
    }
    else
    {
        g.setGradientFill (ColourGradient (Colour (0xff4a0e0a), ledCentre, Colour (0xff170403),
                                           ledCentre.translated (ledSize * 0.62f, 0.0f), true));
    }
    g.fillRoundedRectangle (led, 2.0f);

    g.setColour (Palette::text);
    if (big)
    {
        g.setFont (fonts->bold (jmin (22.0f, r.getHeight() * 0.4f)));
        g.drawFittedText (getButtonText(), r.withTrimmedTop (r.getHeight() * 0.52f).reduced (4.0f, 1.0f).toNearestInt(),
                          Justification::centred, 1, 0.7f);
    }
    else
    {
        g.setFont (fonts->bold (jmin (34.0f, r.getHeight() * 0.56f)));
        g.drawText (getButtonText(), r.withTrimmedRight (r.getHeight()).withTrimmedLeft (8.0f), Justification::centredLeft, false);
    }
}

//==============================================================================
ChoiceSelector::ChoiceSelector (APVTS& state, const String& paramID, const String& tooltip)
{
    auto* param = state.getParameter (paramID);
    jassert (param != nullptr);
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (param))
        names = choice->choices;

    setTooltip (tooltip);
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment = std::make_unique<ParameterAttachment> (*param, [this] (float v) { selected = roundToInt (v); repaint(); }, nullptr);
    attachment->sendInitialUpdate();
}

Rectangle<float> ChoiceSelector::segment (int i) const
{
    const auto w = (float) getWidth() / (float) jmax (1, names.size());
    return { (float) i * w + 1.0f, 0.0f, w - 2.0f, (float) getHeight() };
}

void ChoiceSelector::paint (Graphics& g)
{
    g.setColour (Colour (0xff050505));
    g.fillRect (getLocalBounds());

    for (int i = 0; i < names.size(); ++i)
    {
        const auto r = segment (i);
        const auto sel = i == selected;

        if (sel)
            g.setGradientFill (ColourGradient (Colour (0xffa3170f), r.getCentreX(), r.getCentreY(), Colour (0xff4d0604), r.getX(), r.getY(), true));
        else
            g.setGradientFill (ColourGradient (Colour (0xff171716), 0.0f, r.getY(), Colour (0xff0a0a0a), 0.0f, r.getBottom(), false));
        g.fillRect (r);

        g.setColour (sel ? Colour (0xffff4b3c) : Colour (0xff8b877f).withAlpha (0.45f));
        g.drawRect (r.reduced (3.0f), sel ? 1.6f : 1.1f);
        g.setColour (sel ? Colour (0xffffe5de) : Colour (0xffd9d4ca));
        g.setFont (fonts->sans (jmin (26.0f, r.getHeight() * 0.46f)));
        g.drawFittedText (names[i], r.reduced (4.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
    }
}

void ChoiceSelector::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < names.size(); ++i)
        if (segment (i).contains (e.position))
            attachment->setValueAsCompleteGesture ((float) i);
}

//==============================================================================
void FlatButton::paintButton (Graphics& g, bool over, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto enabled = isEnabled();

    if (active)
        g.setGradientFill (ColourGradient (Colour (0xffa3170f), r.getCentreX(), r.getCentreY(), Colour (0xff4d0604), r.getX(), r.getY(), true));
    else
        g.setGradientFill (ColourGradient (Colour (down ? 0xff0a0a0a : 0xff1b1a19), 0.0f, r.getY(), Colour (0xff0a0a0a), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (Colours::black);
    g.drawRect (r, 1.0f);
    g.setColour (active ? Colour (0xffff4b3c) : Colour (0xff8b877f).withAlpha (over ? 0.8f : 0.45f));
    g.drawRect (r.reduced (3.0f), active ? 1.6f : 1.1f);

    g.setColour ((active ? Colour (0xffffe5de) : Palette::text).withMultipliedAlpha (enabled ? 1.0f : 0.4f));
    g.setFont (fonts->sans (textHeight));
    g.drawFittedText (getButtonText(), r.reduced (6.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);
}

//==============================================================================
namespace
{
    // dB -> 0..1 position (non-linear, more resolution at the top)
    float meterPos (float db)
    {
        static constexpr float dbs[] = { 3.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f, -60.0f };
        static constexpr float ps[]  = { 1.0f, 0.975f, 0.77f, 0.56f, 0.34f, 0.12f, 0.0f };
        if (db >= dbs[0]) return 1.0f;
        for (int i = 1; i < 7; ++i)
            if (db >= dbs[i])
                return jmap (db, dbs[i], dbs[i - 1], ps[i], ps[i - 1]);
        return 0.0f;
    }
}

void Meter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0b0b));
    g.fillRect (r);

    const auto labelH = 30.0f;
    const auto scaleW = 34.0f;
    auto bars = r.reduced (8.0f).withTrimmedBottom (labelH).withTrimmedRight (scaleW);
    const auto barW = (bars.getWidth() - 10.0f) * 0.5f;

    for (int b = 0; b < 2; ++b)
    {
        const auto x = bars.getX() + (float) b * (barW + 10.0f);
        const auto level = meterPos (levels[b]);

        for (float y = bars.getBottom() - 4.0f; y >= bars.getY(); y -= 5.0f)
        {
            const auto pos = (bars.getBottom() - y) / bars.getHeight();
            const auto lit = pos <= level;
            if (! lit)                                g.setColour (Colour (0xff2a2927));
            else if (b == 1 && pos > meterPos (-12.0f)) g.setColour (Palette::red);
            else                                       g.setColour (Colour (0xffd8d2c6));
            g.fillRect (x, y, barW, 3.2f);
        }

        g.setColour (Palette::text);
        g.setFont (fonts->bold (24.0f));
        g.drawText (b == 0 ? "IN" : "OUT", Rectangle<float> (x - 6.0f, bars.getBottom() + 4.0f, barW + 12.0f, labelH),
                    Justification::centred, false);
    }

    g.setFont (fonts->mono (17.0f));
    g.setColour (Palette::textDim);
    for (auto db : { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const auto y = bars.getBottom() - meterPos (db) * bars.getHeight();
        g.drawText (approximatelyEqual (db, 0.0f) ? "0" : String ((int) db), Rectangle<float> (bars.getRight() + 2.0f, y - 8.0f, scaleW + 4.0f, 16.0f),
                    Justification::centredLeft, false);
    }
}

} // namespace UI
