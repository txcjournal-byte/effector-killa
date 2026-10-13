#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText { 0xffe8e6f0 };
const juce::Colour cInk  { 0xff8c889c };
const juce::Colour cLav  { 0xffb9a8ff };
const juce::Colour cIce  { 0xffe4ddff };
const juce::Colour cWarn { 0xffff5c7a };
const juce::Colour cDeep { 0xff17161d };

struct Fonts
{
    juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    static const Fonts& get() { static Fonts f; return f; }
};

juce::Font fnt (const juce::Typeface::Ptr& t, float h) { return juce::Font (juce::FontOptions (t).withHeight (h)); }

// ---- layout (base px) – must match tools/make_background.py
const juce::Rectangle<int> scopeInner { 26, 106, 268, 268 };
const juce::Rectangle<float> corrLcd { 20.0f, 392.0f, 280.0f, 60.0f };
const juce::Rectangle<float> peakLcd { 20.0f, 464.0f, 280.0f, 60.0f };
const juce::Rectangle<int> bandArea { 328, 112, 500, 126 };
constexpr float wrowY = 326.0f, deckY = 450.0f;
struct KnobSpot { float x, y, body, ring; int dots; float valueY; };
const KnobSpot spots[10] = {
    { 400.0f, wrowY, 28.0f, 36.0f, 21, wrowY + 47.0f },   // low width
    { 489.0f, wrowY, 18.0f, 25.0f, 17, wrowY + 37.0f },   // low x-over
    { 578.0f, wrowY, 28.0f, 36.0f, 21, wrowY + 47.0f },   // mid width
    { 667.0f, wrowY, 18.0f, 25.0f, 17, wrowY + 37.0f },   // high x-over
    { 756.0f, wrowY, 28.0f, 36.0f, 21, wrowY + 47.0f },   // high width
    { 372.0f, deckY, 22.0f, 29.0f, 21, deckY + 42.0f },   // mono below
    { 458.0f, deckY, 22.0f, 29.0f, 21, deckY + 42.0f },   // haas
    { 614.0f, deckY, 22.0f, 29.0f, 21, deckY + 42.0f },   // balance
    { 708.0f, deckY, 22.0f, 29.0f, 21, deckY + 42.0f },   // stereoize
    { 796.0f, deckY, 22.0f, 29.0f, 21, deckY + 42.0f },   // output
};
constexpr float fLo = 20.0f, fHi = 20000.0f;

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c)
{
    g.setColour (c.withAlpha (0.18f));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (dx, dy), j);
    g.setColour (c);
    g.drawText (t, r, j);
}

juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + "k" : juce::String (juce::roundToInt (v));
}

void setParam (juce::RangedAudioParameter* p, float plain)
{
    if (p != nullptr) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}
} // namespace

// ================= Look =================

WideLook::WideLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cLav);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff120e22));
    setColour (juce::PopupMenu::headerTextColourId, cLav);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cLav.withAlpha (0.5f));
    knob = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void WideLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float body = (float) (double) s.getProperties().getWithDefault ("body", 22.0);
    const float ring = (float) (double) s.getProperties().getWithDefault ("ring", 29.0);
    const int dots = (int) s.getProperties().getWithDefault ("dots", 21);
    const float centreAt = (float) (double) s.getProperties().getWithDefault ("centre", bipolar ? 0.5 : 0.0);
    const float angle = start + pos * (end - start);
    const auto on = s.isEnabled() ? cLav : cInk;

    const int last = dots - 1;
    const int lit = juce::roundToInt (pos * (float) last);
    const int mid = juce::roundToInt (centreAt * (float) last);
    for (int k = juce::jmin (mid, lit); k <= juce::jmax (mid, lit); ++k)
    {
        const float a = juce::degreesToRadians (225.0f - 270.0f * (float) k / (float) last);
        const float lx = c.x + std::cos (a) * ring, ly = c.y - std::sin (a) * ring;
        g.setColour (on.withAlpha (0.22f));
        g.fillEllipse (lx - 3.6f, ly - 3.6f, 7.2f, 7.2f);
        g.setColour (k == mid && centreAt > 0.0f ? cIce : on);
        g.fillEllipse (lx - 1.7f, ly - 1.7f, 3.4f, 3.4f);
    }

    const float imgHalf = body / 0.80f;
    g.drawImage (knob, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    const float pw = body > 25.0f ? 3.2f : 2.6f;
    juce::Path p;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.94f, pw, topR * 0.46f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (juce::Colour (0xff1a1830).withAlpha (0.8f));
    g.strokePath (p, juce::PathStrokeType (2.0f), tr);
    g.setColour (on.darker (0.15f));
    g.fillPath (p, tr);
}

void WideLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff5e5878) : juce::Colour (0xff403c54));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0c12), 0, 0, juce::Colour (0xff17161e), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cLav);
    g.fillPath (arrow);
}

void WideLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font WideLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font WideLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void WideLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cLav.withAlpha (0.2f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffd9ceff) : juce::Colour (down ? 0xff22202a : 0xff33303f), 0, 0,
                                             on ? juce::Colour (0xff8f78f0) : juce::Colour (0xff1a1922), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.5f);
    g.setColour (on ? juce::Colour (0xffefeaff) : juce::Colour (over ? 0xff655f80 : 0xff433f56));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
}

void WideLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colour (0xff15102a) : cText.withAlpha (0.85f));
    g.setFont (fnt (Fonts::get().label, 15.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ================= Vectorscope =================

Vectorscope::Vectorscope (WideProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

void Vectorscope::tick()
{
    proc.getEngine().copyScope (l.data(), r.data(), numPts);
    float pk = 0.0f;
    for (int i = 0; i < numPts; ++i) pk = std::max (pk, std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
    // auto-scale: fast to zoom out, slow to zoom in
    const float target = 0.92f / std::max (0.03f, pk * 0.7071f * 1.41421f);
    scale += (target < scale ? 0.5f : 0.06f) * (target - scale);
    repaint();
}

void Vectorscope::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre().translated (0.0f, -2.0f);
    const float R = 108.0f;
    for (int i = 0; i < numPts; ++i)
    {
        const float L = l[(size_t) i], Rv = r[(size_t) i];
        const float side = 0.5f * (L - Rv), mid = 0.5f * (L + Rv);
        // L-only lands up-left, R-only up-right (45 degree axes)
        const float px = c.x - side * scale * R * 1.41421f;
        const float py = c.y - mid * scale * R * 1.41421f;
        if (! b.contains (px, py)) continue;
        const float age = (float) i / (float) numPts;
        g.setColour ((age > 0.85f ? cIce : cLav).withAlpha (0.14f + 0.66f * age * age));
        g.fillRect (px - 1.0f, py - 1.0f, 2.0f, 2.0f);
    }
    g.setFont (fnt (Fonts::get().mono, 11.0f));
    g.setColour (cInk.withAlpha (0.8f));
    g.drawText ("x" + juce::String (scale, scale < 10.0f ? 1 : 0), b.withTrimmedTop (b.getHeight() - 18.0f).withTrimmedRight (8.0f), juce::Justification::centredRight);
}

// ================= BandView =================

BandView::BandView (WideProcessor& p) : proc (p) {}

float BandView::xFor (float hz) const
{
    return (float) getWidth() * std::log (hz / fLo) / std::log (fHi / fLo);
}

float BandView::hzFor (float x) const
{
    return fLo * std::pow (fHi / fLo, juce::jlimit (0.0f, 1.0f, x / (float) getWidth()));
}

int BandView::hitTarget (float x) const
{
    const auto p = proc.currentParams();
    const float xl = xFor (p.lowXHz), xh = xFor (std::max (p.highXHz, p.lowXHz * 2.0f));
    if (std::abs (x - xl) < 7.0f) return 3;
    if (std::abs (x - xh) < 7.0f) return 4;
    return x < xl ? 0 : (x < xh ? 1 : 2);
}

juce::RangedAudioParameter* BandView::paramFor (int t) const
{
    static const char* ids[] = { IDs::widthLow, IDs::widthMid, IDs::widthHigh, IDs::lowX, IDs::highX };
    return t >= 0 && t < 5 ? proc.apvts.getParameter (ids[t]) : nullptr;
}

void BandView::mouseMove (const juce::MouseEvent& e)
{
    const int t = hitTarget ((float) e.x);
    setMouseCursor (t >= 3 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
    if (t != hover) { hover = t; repaint(); }
}

void BandView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTarget ((float) e.x);
    if (auto* p = paramFor (dragging))
    {
        p->beginChangeGesture();
        dragStartValue = p->convertFrom0to1 (p->getValue());
    }
}

void BandView::mouseDrag (const juce::MouseEvent& e)
{
    auto* p = paramFor (dragging);
    if (p == nullptr) return;
    if (dragging >= 3)
        setParam (p, hzFor ((float) e.x));
    else
    {
        const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
        setParam (p, juce::jlimit (0.0f, 2.0f, dragStartValue - (float) e.getDistanceFromDragStartY() * fine / 55.0f));
    }
    repaint();
}

void BandView::mouseUp (const juce::MouseEvent&)
{
    if (auto* p = paramFor (dragging)) p->endChangeGesture();
    dragging = -1;
}

void BandView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int t = hitTarget ((float) e.x);
    if (auto* p = paramFor (t))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

void BandView::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const float H = (float) getHeight(), W = (float) getWidth();
    const float midY = H * 0.5f, maxHalf = H * 0.5f - 6.0f;
    const float xs[4] = { 0.0f, xFor (p.lowXHz), xFor (std::max (p.highXHz, p.lowXHz * 2.0f)), W };
    const float widths[3] = { p.widthLow, p.widthMid, p.widthHigh };
    const char* names[3] = { "LOW", "MID", "HIGH" };

    for (int b = 0; b < 3; ++b)
    {
        const float x0 = xs[b] + 3.0f, x1 = xs[b + 1] - 3.0f;
        if (x1 - x0 < 4.0f) continue;
        const float w = widths[b];
        const float half = juce::jmax (1.5f, maxHalf * w * 0.5f);
        const float glow = juce::jlimit (0.0f, 1.0f, (energy[(size_t) b] + 50.0f) / 40.0f);
        const auto col = w > 1.001f ? cLav : (w < 0.999f ? cLav.interpolatedWith (cInk, 0.35f) : cLav.withMultipliedSaturation (0.7f));
        const auto lens = juce::Rectangle<float> (x0, midY - half, x1 - x0, half * 2.0f);

        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.10f + 0.22f * glow + (hover == b ? 0.08f : 0.0f)), 0, midY,
                                                 col.withAlpha (0.03f), 0, midY - maxHalf, true));
        g.fillRoundedRectangle (lens, 6.0f);
        g.setColour (col.withAlpha (0.85f));
        g.drawRoundedRectangle (lens, 6.0f, 1.4f);
        // 100 % reference marks
        g.setColour (cIce.withAlpha (0.25f));
        const float ref = maxHalf * 0.5f;
        for (float yy : { midY - ref, midY + ref })
        {
            const float dashes[] = { 3.0f, 4.0f };
            juce::Path dl; dl.startNewSubPath (x0 + 4.0f, yy); dl.lineTo (x1 - 4.0f, yy);
            juce::Path dd; juce::PathStrokeType (1.0f).createDashedStroke (dd, dl, dashes, 2);
            g.fillPath (dd);
        }
        // caption
        if (x1 - x0 > 44.0f)
        {
            g.setFont (fnt (Fonts::get().label, 12.5f));
            g.setColour (cText.withAlpha (0.8f));
            g.drawText (names[b], juce::Rectangle<float> (x0 + 6.0f, 3.0f, x1 - x0 - 12.0f, 14.0f), juce::Justification::centredLeft);
            g.setFont (fnt (Fonts::get().mono, 12.0f));
            glowText (g, juce::String (juce::roundToInt (w * 100.0f)) + "%", juce::Rectangle<float> (x0 + 6.0f, 3.0f, x1 - x0 - 12.0f, 14.0f),
                      juce::Justification::centredRight, cLav);
        }
    }

    // mono-below zone
    if (p.monoBelowHz > widefx::monoOffHz)
    {
        const float xm = xFor (p.monoBelowHz);
        auto zone = juce::Rectangle<float> (0.0f, 0.0f, xm, H);
        g.setColour (cIce.withAlpha (0.08f));
        g.fillRect (zone);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (zone.toNearestInt());
        g.setColour (cIce.withAlpha (0.16f));
        for (float k = -H; k < xm + H; k += 7.0f)
            g.drawLine (k, H, k + H, 0.0f, 1.0f);
        g.setColour (cIce.withAlpha (0.9f));
        g.fillRect (xm - 1.0f, 0.0f, 2.0f, H);
        g.setFont (fnt (Fonts::get().label, 12.0f));
        if (xm > 34.0f)
            glowText (g, "MONO", juce::Rectangle<float> (6.0f, H - 18.0f, xm - 10.0f, 14.0f), juce::Justification::centredLeft, cIce);
    }

    // crossover handles
    for (int k = 1; k <= 2; ++k)
    {
        const float x = xs[k];
        const bool hot = hover == k + 2 || dragging == k + 2;
        g.setColour (cIce.withAlpha (hot ? 0.95f : 0.6f));
        g.fillRect (x - 0.75f, 0.0f, 1.5f, H);
        const auto pill = juce::Rectangle<float> (40.0f, 15.0f).withCentre ({ juce::jlimit (21.0f, W - 21.0f, x), H - 9.0f });
        g.setColour (hot ? cLav : juce::Colour (0xff2a2640));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (cIce.withAlpha (0.9f));
        g.drawRoundedRectangle (pill, 4.0f, 1.0f);
        g.setFont (fnt (Fonts::get().mono, 11.5f));
        g.setColour (hot ? juce::Colour (0xff15102a) : cIce);
        g.drawText (hzText (k == 1 ? p.lowXHz : std::max (p.highXHz, p.lowXHz * 2.0f)), pill, juce::Justification::centred);
    }
}

// ================= Panel =================

WidePanel::WidePanel (WideProcessor& p) : proc (p), scope (p), bands (p)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    const char* ids[] = { IDs::widthLow, IDs::lowX, IDs::widthMid, IDs::highX, IDs::widthHigh,
                          IDs::monoBelow, IDs::haas, IDs::balance, IDs::stereoize, IDs::output };
    const char* tips[] = { "Stereo width of the low band (0 % = mono, 100 % = unchanged, 200 % = twice the side)",
                           "Crossover between the low and mid band (also draggable in the display)",
                           "Stereo width of the mid band",
                           "Crossover between the mid and high band (also draggable in the display)",
                           "Stereo width of the high band - widen the top, keep the body solid",
                           "Everything below this frequency becomes mono (phase-coherent). Keep 808s and kicks centred. OFF at minimum.",
                           "Haas widening: delays one side by up to 30 ms. Pair it with MONO BELOW for a solid low end.",
                           "M/S balance: turn left for more centre (mid), right for more sides",
                           "STEREOIZE: makes mono sources wide with a decorrelated copy. 100 % mono-compatible.",
                           "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setTooltip (tips[i]);
        k.s.onValueChange = [this] { repaint(); };
        k.s.getProperties().set ("body", spots[i].body);
        k.s.getProperties().set ("ring", spots[i].ring);
        k.s.getProperties().set ("dots", spots[i].dots);
        addAndMakeVisible (k.s);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.s);
        if (auto* prm = proc.apvts.getParameter (ids[i]))
            k.s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    for (int i : { 0, 2, 4, 7 }) knobs[(size_t) i].s.getProperties().set ("bipolar", true);

    addAndMakeVisible (scope);
    addAndMakeVisible (bands);

    sideAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::haasSide), [this] (float v)
    {
        const int s = juce::roundToInt (v);
        sideButtons[0].setToggleState (s == 0, juce::dontSendNotification);
        sideButtons[1].setToggleState (s == 1, juce::dontSendNotification);
    }, nullptr);
    const char* sideNames[] = { "L", "R" };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = sideButtons[(size_t) i];
        b.setButtonText (sideNames[i]);
        b.setTooltip (i == 0 ? "Haas: delay the left channel" : "Haas: delay the right channel");
        b.onClick = [this, i] { sideAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    sideAttachment->sendInitialUpdate();

    // presets
    const auto& presets = widefx::factoryPresets();
    juce::String lastCat;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (lastCat != presets[(size_t) i].category)
        {
            lastCat = presets[(size_t) i].category;
            presetBox.addSectionHeading (lastCat.toUpperCase());
        }
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    }
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);
    auto step = [this] (int dir)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + n) % n);
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    // start the meters where the audio is (no slide-in on open)
    corr = proc.uiCorr.load();
    for (int c = 0; c < 2; ++c) peakDb[c] = juce::Decibels::gainToDecibels (proc.uiPeak[(size_t) c].load(), -60.0f);
    for (int b = 0; b < 3; ++b) bands.energy[(size_t) b] = proc.uiBand[(size_t) b].load();
    timerCallback();
    startTimerHz (30);
}

WidePanel::~WidePanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void WidePanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    corr += 0.25f * (proc.uiCorr.load() - corr);
    for (int c = 0; c < 2; ++c)
    {
        const float t = juce::Decibels::gainToDecibels (proc.uiPeak[(size_t) c].load(), -60.0f);
        peakDb[c] += (t > peakDb[c] ? 0.6f : 0.12f) * (t - peakDb[c]);
    }
    for (int b = 0; b < 3; ++b) bands.energy[(size_t) b] += 0.3f * (proc.uiBand[(size_t) b].load() - bands.energy[(size_t) b]);
    scope.tick();
    bands.repaint();
    repaint (corrLcd.toNearestInt().expanded (3));
    repaint (peakLcd.toNearestInt().expanded (3));
}

void WidePanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
}

void WidePanel::paintOverChildren (juce::Graphics& g)
{
    // ---- knob values
    g.setFont (fnt (Fonts::get().mono, 13.0f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        g.setColour (cText.withAlpha (0.9f));
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (86.0f, 14.0f).withCentre ({ spots[i].x, spots[i].valueY }),
                    juce::Justification::centred);
    }

    // ---- correlation LCD
    {
        auto r = corrLcd.reduced (12.0f, 8.0f);
        auto top = r.removeFromTop (18.0f);
        g.setFont (fnt (Fonts::get().label, 13.0f));
        g.setColour (cLav.withAlpha (0.8f));
        g.drawText ("CORRELATION", top, juce::Justification::centredLeft);
        const auto col = corr < 0.0f ? cWarn : cLav;
        g.setFont (fnt (Fonts::get().mono, 16.0f));
        glowText (g, (corr >= 0.0f ? "+" : "") + juce::String (corr, 2), top, juce::Justification::centredRight, col);
        if (corr < 0.0f)
        {
            g.setFont (fnt (Fonts::get().label, 12.0f));
            glowText (g, "PHASE!", top.withTrimmedLeft (90.0f).withTrimmedRight (60.0f), juce::Justification::centred, cWarn);
        }
        auto bar = r.removeFromTop (12.0f).translated (0.0f, 3.0f);
        const int segs = 40;
        const float sw = bar.getWidth() / (float) segs;
        const float v = (corr + 1.0f) * 0.5f;
        for (int k = 0; k < segs; ++k)
        {
            const float f0 = (float) k / (float) segs, f1 = (float) (k + 1) / (float) segs;
            const bool lit = (v >= 0.5f) ? (f0 >= 0.5f - 1e-4f && f0 < v) : (f1 <= 0.5f + 1e-4f && f1 > v);
            const auto seg = juce::Rectangle<float> (bar.getX() + (float) k * sw + 0.7f, bar.getY(), sw - 1.4f, bar.getHeight());
            const auto c = f1 <= 0.5f ? cWarn : cLav;
            if (lit)
            {
                g.setColour (c.withAlpha (0.25f));
                g.fillRect (seg.expanded (1.0f, 1.5f));
                g.setColour (c);
            }
            else
                g.setColour (c.withAlpha (0.09f));
            g.fillRect (seg);
        }
        g.setColour (cText.withAlpha (0.6f));
        g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 3.0f, 1.0f, bar.getHeight() + 6.0f);
        g.setFont (fnt (Fonts::get().mono, 10.5f));
        g.setColour (cInk);
        auto lbl = r.translated (0.0f, 4.0f);
        g.drawText ("-1", lbl, juce::Justification::centredLeft);
        g.drawText ("0", lbl, juce::Justification::centred);
        g.drawText ("+1", lbl, juce::Justification::centredRight);
    }

    // ---- output peak LCD
    {
        auto r = peakLcd.reduced (12.0f, 10.0f);
        const float rowH = r.getHeight() / 2.0f;
        for (int c = 0; c < 2; ++c)
        {
            auto row = juce::Rectangle<float> (r.getX(), r.getY() + rowH * (float) c, r.getWidth(), rowH);
            g.setFont (fnt (Fonts::get().label, 13.0f));
            g.setColour (cLav.withAlpha (0.8f));
            g.drawText (c == 0 ? "L" : "R", row.removeFromLeft (16.0f), juce::Justification::centredLeft);
            auto val = row.removeFromRight (50.0f);
            auto bar = row.reduced (2.0f, rowH * 0.28f);
            const int segs = 30;
            const float sw = bar.getWidth() / (float) segs;
            const float v = juce::jlimit (0.0f, 1.0f, (peakDb[c] + 48.0f) / 48.0f);
            for (int k = 0; k < segs; ++k)
            {
                const float f0 = (float) k / (float) segs;
                const auto seg = juce::Rectangle<float> (bar.getX() + (float) k * sw + 0.7f, bar.getY(), sw - 1.4f, bar.getHeight());
                const auto col = f0 >= 44.0f / 48.0f ? cWarn : cLav;
                g.setColour (f0 < v ? col : col.withAlpha (0.09f));
                g.fillRect (seg);
            }
            g.setFont (fnt (Fonts::get().mono, 12.5f));
            glowText (g, peakDb[c] <= -59.0f ? juce::String ("-inf") : juce::String (peakDb[c], 1), val, juce::Justification::centredRight,
                      peakDb[c] > -0.3f ? cWarn : cLav);
        }
    }
}

void WidePanel::resized()
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const float sz = spots[i].ring * 2.0f + 10.0f;
        knobs[i].s.setBounds (juce::Rectangle<float> (sz, sz).withCentre ({ spots[i].x, spots[i].y }).toNearestInt());
    }
    scope.setBounds (scopeInner);
    bands.setBounds (bandArea);
    sideButtons[0].setBounds (500, 424, 40, 24);
    sideButtons[1].setBounds (500, 452, 40, 24);

    prevButton.setBounds (510, 30, 30, 30);
    presetBox.setBounds (544, 30, 250, 30);
    nextButton.setBounds (798, 30, 30, 30);
}

// ================= Editor =================

WideEditor::WideEditor (WideProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (WidePanel::baseW * 3 / 4, WidePanel::baseH * 3 / 4, WidePanel::baseW * 2, WidePanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) WidePanel::baseW / WidePanel::baseH);
    setSize (WidePanel::baseW, WidePanel::baseH);
}

void WideEditor::resized()
{
    panel.setBounds (0, 0, WidePanel::baseW, WidePanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / WidePanel::baseW));
}
