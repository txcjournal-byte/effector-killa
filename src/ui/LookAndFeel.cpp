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
    setColour (ComboBox::arrowColourId,             pal::textDim);
    setColour (PopupMenu::backgroundColourId,       pal::ground1);
    setColour (PopupMenu::textColourId,             pal::text);
    setColour (PopupMenu::highlightedBackgroundColourId, pal::cyan.withAlpha (0.14f));
    setColour (PopupMenu::highlightedTextColourId,  pal::text);
    setColour (PopupMenu::headerTextColourId,       pal::textDim);
    setColour (TextButton::buttonColourId,          pal::panel);
    setColour (TextButton::textColourOffId,         pal::text);
    setColour (TextButton::textColourOnId,          pal::text);
    setColour (TextEditor::backgroundColourId,      pal::panel);
    setColour (TextEditor::textColourId,            pal::text);
    setColour (TextEditor::highlightColourId,       pal::cyan.withAlpha (0.3f));
    setColour (TextEditor::outlineColourId,         pal::hairline);
    setColour (TextEditor::focusedOutlineColourId,  pal::cyan.withAlpha (0.6f));
    setColour (CaretComponent::caretColourId,       pal::cyan);
    setColour (ListBox::backgroundColourId,         Colours::transparentBlack);
    setColour (ListBox::textColourId,               pal::text);
    setColour (ScrollBar::thumbColourId,            pal::hairline);
    setColour (AlertWindow::backgroundColourId,     pal::ground1);
    setColour (AlertWindow::textColourId,           pal::text);
    setColour (AlertWindow::outlineColourId,        pal::hairline);
    setColour (DocumentWindow::textColourId,        pal::text);
    setColour (ToggleButton::textColourId,          pal::text);
    setColour (ToggleButton::tickColourId,          pal::cyan);
    setColour (ProgressBar::foregroundColourId,     pal::cyan);
    setColour (ProgressBar::backgroundColourId,     pal::panel);
}

void BendDemonLookAndFeel::strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c,
                                       float coreWidth, float glowWidth, int layers)
{
    for (int i = layers; i >= 1; --i)
    {
        const float t = (float) i / (float) layers;
        g.setColour (c.withMultipliedAlpha (0.07f * (1.0f - t) + 0.025f));
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

    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float size   = jmin (bounds.getWidth(), bounds.getHeight());
    const auto  centre = bounds.getCentre();
    const float radius = size * 0.5f;
    const float ringW  = jmax (2.0f, radius * 0.09f);
    const float arcR   = radius - ringW;
    const float angle  = a0 + pos * (a1 - a0);
    const auto  accent = s.findColour (Slider::rotarySliderFillColourId) == Colour() ? pal::cyan
                                                                                     : s.findColour (Slider::rotarySliderFillColourId);

    g.setColour (pal::panel);
    g.fillEllipse (Rectangle<float> (arcR * 1.5f, arcR * 1.5f).withCentre (centre));
    g.setColour (s.isMouseOverOrDragging() ? pal::textFaint : pal::hairline);
    g.drawEllipse (Rectangle<float> (arcR * 1.5f, arcR * 1.5f).withCentre (centre), 1.0f);

    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (pal::hairline);
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, angle, true);
        g.setColour (s.isEnabled() ? accent : pal::textDim);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const auto tip = centre.getPointOnCircumference (arcR * 0.72f, angle);
    g.setColour (pal::text);
    g.drawLine ({ centre.getPointOnCircumference (arcR * 0.35f, angle), tip }, jmax (1.5f, ringW * 0.8f));
}

void BendDemonLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                             juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace juce;

    if (style != Slider::LinearBar)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0.0f, 0.0f, style, s);
        return;
    }

    // Bar slider: [caption ......... value], the fill shows the value. Used for every parameter
    // row in the inspector.
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0.5f);
    const float corner = pal::metric::corner;
    const bool hot = s.isMouseOverOrDragging() && s.isEnabled();

    g.setColour (hot ? pal::raised : pal::panel);
    g.fillRoundedRectangle (r, corner);

    const float norm = (float) s.valueToProportionOfLength (s.getValue());
    auto fill = r.reduced (1.0f).withWidth (jmax (0.0f, (r.getWidth() - 2.0f) * norm));
    const auto accent = s.findColour (Slider::trackColourId) == Colour() ? pal::cyan : s.findColour (Slider::trackColourId);
    g.setColour (accent.withAlpha (s.isEnabled() ? 0.20f : 0.08f));
    g.fillRoundedRectangle (fill, corner - 1.0f);
    if (norm > 0.0f)
    {
        g.setColour (accent.withAlpha (s.isEnabled() ? 0.9f : 0.3f));
        g.fillRect (Rectangle<float> (fill.getRight() - 1.0f, fill.getY() + 2.0f, 2.0f, fill.getHeight() - 4.0f));
    }

    g.setColour (hot ? pal::textFaint : pal::hairline);
    g.drawRoundedRectangle (r, corner, 1.0f);

    const float fs = jlimit (10.0f, 13.5f, r.getHeight() * 0.46f);
    auto text = r.reduced (8.0f, 0.0f);
    g.setFont (fonts::ui (fs));
    g.setColour (s.isEnabled() ? pal::textDim : pal::textFaint);
    g.drawFittedText (s.getName(), text.toNearestInt(), Justification::centredLeft, 1, 0.8f);
    g.setColour (s.isEnabled() ? pal::text : pal::textFaint);
    g.setFont (fonts::mono (fs, true));
    g.drawFittedText (s.getTextFromValue (s.getValue()), text.toNearestInt(), Justification::centredRight, 1, 0.8f);
}

//==============================================================================
void BendDemonLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    const bool hot = box.isMouseOver (true) || down;

    g.setColour (hot ? pal::raised : pal::panel);
    g.fillRoundedRectangle (r, pal::metric::corner);
    g.setColour (hot ? pal::textFaint : pal::hairline);
    g.drawRoundedRectangle (r, pal::metric::corner, 1.0f);

    const float cx = (float) w - 10.0f, cy = (float) h * 0.5f;
    Path chev;
    chev.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chev.lineTo (cx, cy + 2.0f);
    chev.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (hot ? pal::text : pal::textDim);
    g.strokePath (chev, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

juce::Font BendDemonLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return fonts::ui (juce::jlimit (10.0f, 13.5f, (float) box.getHeight() * 0.46f));
}

void BendDemonLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 18, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setMinimumHorizontalScale (0.75f);
}

void BendDemonLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (pal::ground1);
    g.setColour (pal::hairline);
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

    auto r = area.reduced (4, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (pal::raised);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
    }

    if (isTicked)
    {
        g.setColour (pal::cyan);
        g.fillRoundedRectangle (Rectangle<float> (3.0f, 12.0f).withCentre ({ (float) r.getX() + 8.0f, (float) r.getCentreY() }), 1.5f);
    }

    g.setColour (textColour != nullptr ? *textColour : (isActive ? pal::text : pal::textFaint));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, r.withTrimmedLeft (18).withTrimmedRight (hasSubMenu ? 16 : 6), Justification::centredLeft, 1);

    if (hasSubMenu)
    {
        const float x = (float) r.getRight() - 9.0f, y = (float) r.getCentreY();
        Path p; p.startNewSubPath (x - 2, y - 4); p.lineTo (x + 2, y); p.lineTo (x - 2, y + 4);
        g.setColour (pal::textDim);
        g.strokePath (p, PathStrokeType (1.4f));
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
    g.setColour (pal::textDim);
    g.setFont (fonts::mono (11.0f, true));
    g.drawFittedText (text.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft, 1);
}

//==============================================================================
void BendDemonLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = pal::metric::corner;
    const bool on = b.getToggleState();
    const bool enabled = b.isEnabled();

    Colour fill = pal::panel;
    if (on)                     fill = pal::cyan.withAlpha (0.16f);
    if (enabled && over)        fill = on ? pal::cyan.withAlpha (0.22f) : pal::raised;
    if (enabled && down)        fill = pal::ground1;

    g.setColour (pal::panel);
    g.fillRoundedRectangle (r, corner);
    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);

    g.setColour (on ? pal::cyan.withAlpha (0.75f) : (enabled && over ? pal::textFaint : pal::hairline));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void BendDemonLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (! b.isEnabled() ? pal::textFaint : (b.getToggleState() ? pal::text : pal::text.withAlpha (0.9f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.75f);
}

juce::Font BendDemonLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return fonts::ui (juce::jlimit (10.0f, 13.5f, (float) h * 0.44f), true);
}

void BendDemonLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat();
    const float d = jmin (14.0f, r.getHeight() - 6.0f);
    auto box = Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 2.0f, r.getCentreY() });

    g.setColour (b.getToggleState() ? pal::cyan.withAlpha (0.2f) : pal::panel);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (b.getToggleState() ? pal::cyan : (over ? pal::textFaint : pal::hairline));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        g.setColour (pal::cyan);
        g.fillRoundedRectangle (box.reduced (3.5f), 1.5f);
    }

    g.setColour (b.isEnabled() ? pal::text : pal::textDim);
    g.setFont (fonts::mono (jlimit (10.0f, 12.5f, r.getHeight() * 0.5f)));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (d + 8.0f).toNearestInt(), Justification::centredLeft, 1);
}

//==============================================================================
juce::Rectangle<int> BendDemonLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (fonts::mono (12.0f), text, 0.0f, 0.0f);
    const int w = juce::jmin (560, (int) ga.getBoundingBox (0, -1, true).getWidth() + 22);
    const int h = 26;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8)  : pos.y + 14, w, h).constrainedWithin (parent);
}

void BendDemonLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (pal::raised);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (pal::hairline);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (pal::text);
    g.setFont (fonts::mono (12.0f));
    g.drawFittedText (text, r.reduced (10.0f, 2.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
}

juce::Font BendDemonLookAndFeel::getLabelFont (juce::Label& l)
{
    return fonts::mono (juce::jlimit (10.0f, 13.0f, (float) l.getHeight() * 0.6f));
}

void BendDemonLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                          int thumbStart, int thumbSize, bool over, bool)
{
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> ((float) x + 3, (float) thumbStart, (float) w - 6, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, (float) y + 3, (float) thumbSize, (float) h - 6);
    g.setColour (over ? pal::textFaint : pal::hairline);
    g.fillRoundedRectangle (thumb, 3.0f);
}

void BendDemonLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (pal::panel);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
}

void BendDemonLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? pal::cyan.withAlpha (0.6f) : pal::hairline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 4.0f, 1.0f);
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
