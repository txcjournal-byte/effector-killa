#include "LookAndFeel.h"
#include "Palette.h"
#include "Fonts.h"

namespace bd
{

BendDemonLookAndFeel::BendDemonLookAndFeel()
{
    using namespace juce;

    setColour (ResizableWindow::backgroundColourId, pal::ground0);
    setColour (Label::textColourId,                 pal::text);
    setColour (Slider::textBoxTextColourId,         pal::text);
    setColour (Slider::textBoxOutlineColourId,      Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId,        pal::panel);
    setColour (ComboBox::textColourId,              pal::text);
    setColour (ComboBox::outlineColourId,           pal::hairline);
    setColour (ComboBox::arrowColourId,             pal::cyan);
    setColour (PopupMenu::backgroundColourId,       pal::panel);
    setColour (PopupMenu::textColourId,             pal::text);
    setColour (PopupMenu::highlightedBackgroundColourId, pal::violet.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId,  pal::text);
    setColour (PopupMenu::headerTextColourId,       pal::cyan);
    setColour (TextButton::buttonColourId,          pal::panel);
    setColour (TextButton::textColourOffId,         pal::text);
    setColour (TextButton::textColourOnId,          pal::ground0);
    setColour (TextEditor::backgroundColourId,      pal::panel);
    setColour (TextEditor::textColourId,            pal::text);
    setColour (TextEditor::highlightColourId,       pal::violet.withAlpha (0.4f));
    setColour (TextEditor::outlineColourId,         pal::hairline);
    setColour (TextEditor::focusedOutlineColourId,  pal::cyan.withAlpha (0.6f));
    setColour (CaretComponent::caretColourId,       pal::cyan);
    setColour (ListBox::backgroundColourId,         Colours::transparentBlack);
    setColour (ListBox::textColourId,               pal::text);
    setColour (ScrollBar::thumbColourId,            pal::violet.withAlpha (0.5f));
    setColour (AlertWindow::backgroundColourId,     pal::panel);
    setColour (AlertWindow::textColourId,           pal::text);
    setColour (AlertWindow::outlineColourId,        pal::violet);
    setColour (DocumentWindow::textColourId,        pal::text);
    setColour (ToggleButton::textColourId,          pal::text);
    setColour (ToggleButton::tickColourId,          pal::lime);
    setColour (ProgressBar::foregroundColourId,     pal::cyan);
    setColour (ProgressBar::backgroundColourId,     pal::panel);
}

void BendDemonLookAndFeel::strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c,
                                       float coreWidth, float glowWidth, int layers)
{
    for (int i = layers; i >= 1; --i)
    {
        const float t = (float) i / (float) layers;
        g.setColour (c.withMultipliedAlpha (0.10f * (1.0f - t) + 0.035f));
        g.strokePath (p, juce::PathStrokeType (coreWidth + glowWidth * t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (coreWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void BendDemonLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                             float a0, float a1, juce::Slider& s)
{
    using namespace juce;

    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size   = jmin (bounds.getWidth(), bounds.getHeight());
    const auto  centre = bounds.getCentre();
    const float radius = size * 0.5f;
    const float ringW  = jmax (2.5f, radius * 0.10f);
    const float arcR   = radius - ringW * 1.6f;
    const float angle  = a0 + pos * (a1 - a0);
    const bool  enabled = s.isEnabled();
    const auto  accent = s.findColour (Slider::rotarySliderFillColourId) == Colour()
                            ? pal::cyan : s.findColour (Slider::rotarySliderFillColourId);

    // body: dark glass disc with a faint inner gradient
    {
        ColourGradient body (pal::ground1.brighter (0.15f), centre.translated (-arcR * 0.4f, -arcR * 0.6f),
                             pal::ground0, centre.translated (arcR * 0.5f, arcR * 0.8f), true);
        g.setGradientFill (body);
        g.fillEllipse (Rectangle<float> (arcR * 1.6f, arcR * 1.6f).withCentre (centre));
        g.setColour (pal::hairline);
        g.drawEllipse (Rectangle<float> (arcR * 1.6f, arcR * 1.6f).withCentre (centre), 1.0f);
    }

    // track
    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (pal::textFaint.withMultipliedAlpha (0.45f));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

    // glowing value arc (gradient from cyan to the accent)
    if (pos > 0.001f)
    {
        Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, angle, true);

        const auto col = enabled ? accent : pal::textDim;
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (col.withAlpha (0.06f + 0.03f * (4 - i)));
            g.strokePath (arc, PathStrokeType (ringW + (float) i * ringW * 0.9f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        ColourGradient grad (pal::cyan, centre.translated (-arcR, arcR), col, centre.translated (arcR, -arcR), false);
        g.setGradientFill (grad);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // indicator node (lime dot at the arc tip)
    const auto tip = centre.getPointOnCircumference (arcR, angle);
    g.setColour (pal::lime.withAlpha (enabled ? 0.25f : 0.08f));
    g.fillEllipse (Rectangle<float> (ringW * 3.6f, ringW * 3.6f).withCentre (tip));
    g.setColour (enabled ? pal::lime : pal::textDim);
    g.fillEllipse (Rectangle<float> (ringW * 1.5f, ringW * 1.5f).withCentre (tip));

    // centre value text
    g.setColour (enabled ? pal::text : pal::textDim);
    g.setFont (fonts::mono (jlimit (9.0f, 15.0f, radius * 0.30f), true));
    g.drawFittedText (s.getTextFromValue (s.getValue()),
                      Rectangle<float> (arcR * 1.5f, arcR * 0.7f).withCentre (centre).toNearestInt(),
                      Justification::centred, 1, 0.8f);
}

//==============================================================================
void BendDemonLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    const float corner = jmin (8.0f, r.getHeight() * 0.35f);

    g.setColour (pal::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (r, corner);
    g.setColour (box.hasKeyboardFocus (true) || box.isMouseOver (true) ? pal::cyan.withAlpha (0.55f) : pal::hairline.withAlpha (0.35f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    // chevron
    const float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
    Path chev;
    chev.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chev.lineTo (cx, cy + 2.5f);
    chev.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (pal::cyan);
    g.strokePath (chev, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

juce::Font BendDemonLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return fonts::mono (juce::jlimit (10.0f, 14.0f, (float) box.getHeight() * 0.48f));
}

void BendDemonLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void BendDemonLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (pal::panel);
    g.setColour (pal::violet.withAlpha (0.35f));
    g.drawRect (0, 0, w, h, 1);
}

void BendDemonLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                              bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                              const juce::String& text, const juce::String& shortcut,
                                              const juce::Drawable* icon, const juce::Colour* textColour)
{
    using namespace juce;

    if (isSeparator)
    {
        g.setColour (pal::hairline);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.reduced (3, 1);
    if (isHighlighted && isActive)
    {
        ColourGradient grad (pal::cyan.withAlpha (0.22f), r.getTopLeft().toFloat(),
                             pal::magenta.withAlpha (0.12f), r.getTopRight().toFloat(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }

    if (isTicked)
    {
        g.setColour (pal::lime);
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) r.getX() + 9.0f, (float) r.getCentreY() }));
    }

    g.setColour (textColour != nullptr ? *textColour : (isActive ? pal::text : pal::textFaint));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, r.withTrimmedLeft (20).withTrimmedRight (hasSubMenu ? 16 : 6), Justification::centredLeft, 1);

    if (hasSubMenu)
    {
        const float x = (float) r.getRight() - 9.0f, y = (float) r.getCentreY();
        Path p; p.startNewSubPath (x - 2, y - 4); p.lineTo (x + 2, y); p.lineTo (x - 2, y + 4);
        g.setColour (pal::cyan);
        g.strokePath (p, PathStrokeType (1.5f));
    }

    if (shortcut.isNotEmpty())
    {
        g.setColour (pal::textDim);
        g.drawText (shortcut, r.withTrimmedRight (8), Justification::centredRight);
    }

    ignoreUnused (icon);
}

juce::Font BendDemonLookAndFeel::getPopupMenuFont()                       { return fonts::mono (13.0f); }

void BendDemonLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& text)
{
    g.setColour (pal::cyan);
    g.setFont (fonts::display (10.5f, 1));
    g.drawFittedText (text.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft, 1);
}

//==============================================================================
void BendDemonLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = jmin (9.0f, r.getHeight() * 0.4f);
    const bool on = b.getToggleState();

    if (on)
    {
        ColourGradient grad (pal::cyan, r.getTopLeft(), pal::magenta, r.getBottomRight(), false);
        grad.addColour (0.5, pal::violet);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, corner);
    }
    else
    {
        g.setColour (pal::panel.withAlpha (down ? 1.0f : 0.8f));
        g.fillRoundedRectangle (r, corner);
    }

    g.setColour (over ? pal::cyan.withAlpha (0.7f) : pal::hairline.withAlpha (0.4f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

juce::Font BendDemonLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return fonts::mono (juce::jlimit (10.0f, 14.0f, (float) h * 0.45f), true);
}

void BendDemonLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat();
    const float d = jmin (14.0f, r.getHeight() - 4.0f);
    auto dot = Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 2.0f, r.getCentreY() });

    g.setColour (pal::panel);
    g.fillEllipse (dot);
    g.setColour (over ? pal::cyan : pal::hairline.withAlpha (0.6f));
    g.drawEllipse (dot, 1.0f);

    if (b.getToggleState())
    {
        g.setColour (pal::lime.withAlpha (0.3f));
        g.fillEllipse (dot.expanded (3.0f));
        g.setColour (pal::lime);
        g.fillEllipse (dot.reduced (3.0f));
    }

    g.setColour (b.isEnabled() ? pal::text : pal::textDim);
    g.setFont (fonts::mono (jlimit (10.0f, 13.0f, r.getHeight() * 0.55f)));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (d + 8.0f).toNearestInt(), Justification::centredLeft, 1);
}

//==============================================================================
juce::Rectangle<int> BendDemonLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (fonts::mono (12.0f), text, 0.0f, 0.0f);
    const int w = juce::jmin (420, (int) ga.getBoundingBox (0, -1, true).getWidth() + 22);
    const int h = 26;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8)  : pos.y + 14, w, h).constrainedWithin (parent);
}

void BendDemonLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (pal::panel.withAlpha (0.96f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (pal::cyan.withAlpha (0.45f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (pal::text);
    g.setFont (fonts::mono (12.0f));
    g.drawFittedText (text, r.reduced (10.0f, 2.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
}

juce::Font BendDemonLookAndFeel::getLabelFont (juce::Label& l)
{
    return fonts::mono (juce::jlimit (9.0f, 14.0f, (float) l.getHeight() * 0.7f));
}

void BendDemonLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                          int thumbStart, int thumbSize, bool over, bool)
{
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> ((float) x + 2, (float) thumbStart, (float) w - 4, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, (float) y + 2, (float) thumbSize, (float) h - 4);
    g.setColour (pal::violet.withAlpha (over ? 0.7f : 0.4f));
    g.fillRoundedRectangle (thumb, 3.0f);
}

void BendDemonLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (pal::panel);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
}

void BendDemonLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? pal::cyan.withAlpha (0.6f) : pal::hairline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 6.0f, 1.0f);
}

void BendDemonLookAndFeel::drawDocumentWindowTitleBar (juce::DocumentWindow& win, juce::Graphics& g, int w, int h,
                                                       int, int, const juce::Image*, bool)
{
    g.fillAll (pal::ground0);
    g.setColour (pal::text);
    g.setFont (fonts::mono (13.0f, true));
    g.drawText (win.getName(), 10, 0, w - 20, h, juce::Justification::centredLeft);
}

} // namespace bd
