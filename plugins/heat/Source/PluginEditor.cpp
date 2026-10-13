#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

#include <cmath>

namespace
{
const juce::Colour cText  { 0xffeee4de };
const juce::Colour cInk   { 0xff968480 };
const juce::Colour cCoral { 0xffff5a36 };
const juce::Colour cHot   { 0xffffd27a };
const juce::Colour cDeep  { 0xff1a1414 };

struct Fonts
{
    juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    juce::Typeface::Ptr heavy = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    static const Fonts& get() { static Fonts f; return f; }
};

juce::Font fnt (const juce::Typeface::Ptr& t, float h) { return juce::Font (juce::FontOptions (t).withHeight (h)); }

// ---- layout (base px) – must match tools/make_background.py
const juce::Rectangle<int> displayRect { 16, 92, 558, 210 };
constexpr int bandX[3] = { 16, 205, 394 };
constexpr int bandW = 180, bandTop = 312, bandBottom = 532;
constexpr int driveDx = 64, driveY = 442, driveR = 48;
constexpr int smallDx = 146, mixY = 414, levelY = 486, smallR = 22;
constexpr int topKnobX[3] = { 629, 715, 801 };
constexpr int topKnobY = 222, topKnobR = 28;
constexpr int toneX[4] = { 618, 682, 746, 810 };
constexpr int toneY = 362, toneR = 24;
constexpr int outX[2] = { 737, 809 };
constexpr int outY = 472, outR = 26;
const juce::Rectangle<int> autoRect { 598, 436, 78, 70 };

juce::Colour bandColour (int b, int numBands)
{
    if (numBands <= 1) return cCoral;
    static const juce::Colour cols[3] = { juce::Colour (0xffff4768), juce::Colour (0xffff8a3d), juce::Colour (0xffffc04a) };
    return cols[juce::jlimit (0, 2, b)];
}

const char* bandName (int b, int numBands)
{
    if (numBands <= 1) return "FULL";
    if (numBands == 2) return b == 0 ? "LOW" : "HIGH";
    static const char* n[3] = { "LOW", "MID", "HIGH" };
    return n[juce::jlimit (0, 2, b)];
}

juce::String hzText (float v)
{
    if (v >= 1000.0f) return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : (v >= 1950.0f ? 1 : 2)) + "k";
    return juce::String (juce::roundToInt (v));
}

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c)
{
    g.setColour (c.withAlpha (0.2f));
    for (auto [dx, dy] : { std::pair { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (dx, dy), j);
    g.setColour (c);
    g.drawText (t, r, j);
}

juce::Rectangle<int> knobBounds (int cx, int cy, int seatR)
{
    const int s = 2 * (seatR + 6);
    return { cx - s / 2, cy - s / 2, s, s };
}
} // namespace

// ================= Look =================

HeatLook::HeatLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cCoral);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff160806));
    setColour (juce::PopupMenu::headerTextColourId, cCoral);
    setColour (juce::TooltipWindow::backgroundColourId, cDeep);
    setColour (juce::TooltipWindow::textColourId, cText);
    setColour (juce::TooltipWindow::outlineColourId, cCoral.withAlpha (0.5f));
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void HeatLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float seatR = (float) juce::jmin (w, h) * 0.5f - 6.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool hot = (bool) s.getProperties().getWithDefault ("hot", false);
    const auto base = s.getProperties().contains ("colour") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["colour"]) : cCoral;
    const auto on = s.isEnabled() ? base : cInk.withAlpha (0.6f);
    const float angle = start + pos * (end - start);
    const float arcR = seatR - 5.0f;
    const float stroke = seatR > 30.0f ? 4.0f : 3.0f;

    // lit arc (with a white-hot tail on DRIVE)
    const float a0 = bipolar ? (start + end) * 0.5f : start;
    const float lo = juce::jmin (a0, angle), hi = juce::jmax (a0, angle);
    if (hi - lo > 0.001f)
    {
        const int segs = juce::jmax (2, (int) ((hi - lo) / 0.06f));
        for (int k = 0; k < segs; ++k)
        {
            const float f0 = lo + (hi - lo) * (float) k / (float) segs;
            const float f1 = lo + (hi - lo) * (float) (k + 1) / (float) segs;
            const float t = (f1 - start) / (end - start);
            auto col = on;
            if (hot && s.isEnabled() && t > 0.55f) col = on.interpolatedWith (cHot, juce::jmin (1.0f, (t - 0.55f) / 0.45f));
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, f0, f1 + 0.01f, true);
            g.setColour (col.withAlpha (0.22f));
            g.strokePath (arc, juce::PathStrokeType (stroke * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            g.setColour (col);
            g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
    }

    // photoreal knob (pre-rendered); only the pointer rotates
    const float body = seatR - 9.5f;
    const float imgHalf = body / 0.80f;
    g.drawImage (seatR > 30.0f ? knobBig : knobSmall, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c),
                 juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    const float pw = seatR > 30.0f ? 3.6f : 2.4f;
    juce::Path p;
    p.addRoundedRectangle (-pw * 0.5f, -topR * 0.94f, pw, topR * 0.48f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (on.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (seatR > 30.0f ? 4.0f : 2.5f), tr);
    g.setColour (on.brighter (0.3f));
    g.fillPath (p, tr);
}

void HeatLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (box.isMouseOver (true) ? juce::Colour (0xff6e544c) : juce::Colour (0xff4a3a34));
    g.fillRoundedRectangle (r, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e0b0b), 0, 0, juce::Colour (0xff1a1414), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cCoral);
    g.fillPath (arrow);
}

void HeatLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font HeatLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font HeatLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void HeatLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto hue = b.getProperties().contains ("colour") ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["colour"]) : cCoral;
    if (on)
    {
        g.setColour (hue.withAlpha (0.2f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? hue.brighter (0.35f) : juce::Colour (down ? 0xff241e1c : 0xff362e2b), 0, 0,
                                             on ? hue.darker (0.35f) : juce::Colour (0xff1b1615), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? hue.brighter (0.7f) : juce::Colour (over ? 0xff76605a : 0xff4c3d38));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (1, r.getX() + 4.0f, r.getRight() - 4.0f);
    }
}

void HeatLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colour (0xff1e0a06) : cText.withAlpha (b.isEnabled() ? 0.85f : 0.35f));
    g.setFont (fnt (Fonts::get().label, b.getHeight() < 22 ? 13.0f : 14.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// ================= AutoGainButton =================

AutoGainButton::AutoGainButton (juce::RangedAudioParameter& p, HeatProcessor& pr)
    : proc (pr), attachment (p, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void AutoGainButton::mouseDown (const juce::MouseEvent&) { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

void AutoGainButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto hue = cCoral;
    if (on)
        g.setGradientFill (juce::ColourGradient (hue.brighter (0.4f), r.getCentreX(), r.getY() + r.getHeight() * 0.3f,
                                                 hue.darker (0.6f), r.getCentreX(), r.getBottom() + r.getHeight() * 0.3f, true));
    else
        g.setGradientFill (juce::ColourGradient (hue.withMultipliedBrightness (0.22f).withMultipliedSaturation (0.6f), 0, 0,
                                                 hue.withMultipliedBrightness (0.08f).withMultipliedSaturation (0.5f), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 8.0f);
    juce::Path top;
    top.addRoundedRectangle (r.getX() + 3.0f, r.getY() + 2.0f, r.getWidth() - 6.0f, r.getHeight() * 0.42f, 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (on ? 0.36f : 0.09f), 0, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0, r.getY() + r.getHeight() * 0.42f, false));
    g.fillPath (top);
    g.setColour (on ? hue.brighter (0.7f).withAlpha (0.9f) : hue.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.2f);

    const auto ink = on ? juce::Colour (0xff200a06) : hue.withAlpha (0.6f);
    g.setColour (ink);
    g.setFont (fnt (Fonts::get().heavy, 14.0f));
    g.drawText ("AUTO", r.withHeight (r.getHeight() * 0.36f).translated (0.0f, 8.0f), juce::Justification::centred);
    g.drawText ("GAIN", r.withHeight (r.getHeight() * 0.36f).translated (0.0f, 24.0f), juce::Justification::centred);
    g.setFont (fnt (Fonts::get().mono, 12.5f));
    if (on)
    {
        const float db = proc.uiAutoGainDb.load();
        g.drawText ((db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB", r.withTrimmedTop (r.getHeight() - 20.0f), juce::Justification::centred);
    }
    else
        g.drawText ("OFF", r.withTrimmedTop (r.getHeight() - 20.0f), juce::Justification::centred);
}

// ================= ModelSelector =================

ModelSelector::ModelSelector (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v) { model = juce::jlimit (0, heatfx::numModels - 1, juce::roundToInt (v)); repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ModelSelector::step (int dir)
{
    attachment.setValueAsCompleteGesture ((float) ((model + dir + heatfx::numModels) % heatfx::numModels));
}

void ModelSelector::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    if (e.x < 26) { step (-1); return; }
    if (e.x > getWidth() - 26) { step (1); return; }
    juce::PopupMenu m;
    m.addSectionHeader ("MODEL");
    for (int i = 0; i < heatfx::numModels; ++i)
        m.addItem (i + 1, heatfx::modelNames[i], true, i == model);
    juce::Component::SafePointer<ModelSelector> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->attachment.setValueAsCompleteGesture ((float) (r - 1));
    });
}

void ModelSelector::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (isEnabled() && std::abs (w.deltaY) > 0.0f) step (w.deltaY > 0 ? -1 : 1);
}

void ModelSelector::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto col = isEnabled() ? colour : cInk;
    auto tri = [&] (float cx, bool left)
    {
        juce::Path p;
        const float cy = r.getCentreY();
        if (left) p.addTriangle (cx + 3.5f, cy - 5.0f, cx + 3.5f, cy + 5.0f, cx - 3.5f, cy);
        else p.addTriangle (cx - 3.5f, cy - 5.0f, cx - 3.5f, cy + 5.0f, cx + 3.5f, cy);
        g.setColour (col.withAlpha (0.75f));
        g.fillPath (p);
    };
    tri (12.0f, true);
    tri (r.getWidth() - 12.0f, false);
    g.setFont (fnt (Fonts::get().mono, 16.0f));
    glowText (g, heatfx::modelNames[model], r, juce::Justification::centred, col);
}

// ================= HeatDisplay =================

HeatDisplay::HeatDisplay (HeatProcessor& p) : proc (p)
{
    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) (fftSize - 1));
    work.resize ((size_t) fftSize * 2);
    // display coordinates (relative to displayRect) - match the baked grids
    curveArea = { 18.0f, 32.0f, 162.0f, 160.0f };
    specArea = { 206.0f, 32.0f, 334.0f, 148.0f };
    inDb.assign ((size_t) specArea.getWidth(), -120.0f);
    outDb.assign ((size_t) specArea.getWidth(), -120.0f);
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

float HeatDisplay::xForFreq (float f) const
{
    return specArea.getX() + specArea.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f);
}

float HeatDisplay::freqForX (float x) const
{
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - specArea.getX()) / specArea.getWidth()));
}

void HeatDisplay::tick()
{
    const double sr = proc.getSampleRateSafe();
    const int pos = proc.scopePos.load (std::memory_order_acquire);
    const int cols = (int) inDb.size();
    for (int which = 0; which < 2; ++which)
    {
        const auto& src = which == 0 ? proc.scopeIn : proc.scopeOut;
        for (int i = 0; i < fftSize; ++i)
            work[(size_t) i] = src[(size_t) ((pos - fftSize + i) & (HeatProcessor::scopeSize - 1))] * window[(size_t) i];
        std::fill (work.begin() + fftSize, work.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (work.data(), true);
        auto& dst = which == 0 ? inDb : outDb;
        const float binHz = (float) (sr / fftSize);
        for (int c = 0; c < cols; ++c)
        {
            const float f0 = freqForX (specArea.getX() + (float) c), f1 = freqForX (specArea.getX() + (float) c + 1.0f);
            const float b0 = f0 / binHz, b1 = f1 / binHz;
            float mag;
            if (b1 - b0 < 1.0f)
            {
                const float bc = 0.5f * (b0 + b1);
                const int k = juce::jlimit (0, fftSize / 2 - 2, (int) bc);
                const float t = bc - (float) k;
                mag = work[(size_t) k] * (1.0f - t) + work[(size_t) k + 1] * t;
            }
            else
            {
                mag = 0.0f;
                for (int k = (int) b0; k <= juce::jmin (fftSize / 2 - 1, (int) b1); ++k) mag = juce::jmax (mag, work[(size_t) k]);
            }
            const float fc = std::sqrt (f0 * f1);
            float db = 20.0f * std::log10 (mag * 4.0f / (float) fftSize + 1.0e-9f) + 3.0f * std::log2 (fc / 1000.0f);
            auto& v = dst[(size_t) c];
            v = db > v ? db : juce::jmax (db, v - 2.2f);
        }
    }
    repaint();
}

void HeatDisplay::paint (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    // header strip
    const int nb = p.bands;
    const int fb = juce::jlimit (0, nb - 1, focusBand);
    const auto fcol = bandColour (fb, nb);
    g.setFont (fnt (Fonts::get().label, 12.5f));
    g.setColour (cInk);
    g.drawText ("TRANSFER", juce::Rectangle<float> (14.0f, 5.0f, 70.0f, 16.0f), juce::Justification::centredLeft);
    glowText (g, juce::String (bandName (fb, nb)) + "  " + heatfx::modelNames[p.band[(size_t) fb].model],
              juce::Rectangle<float> (64.0f, 5.0f, 126.0f, 16.0f), juce::Justification::centredRight, fcol);
    g.setColour (cInk);
    g.drawText ("SPECTRUM", juce::Rectangle<float> (202.0f, 5.0f, 80.0f, 16.0f), juce::Justification::centredLeft);
    // legend
    g.setColour (cText.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> (276.0f, 12.0f, 12.0f, 2.0f));
    g.setColour (cInk);
    g.drawText ("IN", juce::Rectangle<float> (291.0f, 5.0f, 20.0f, 16.0f), juce::Justification::centredLeft);
    g.setColour (cCoral);
    g.fillRect (juce::Rectangle<float> (312.0f, 11.0f, 12.0f, 3.0f));
    g.setColour (cInk);
    g.drawText ("OUT", juce::Rectangle<float> (327.0f, 5.0f, 30.0f, 16.0f), juce::Justification::centredLeft);
    const int lat = heatfx::latencyForChoice (p.oversampling);
    glowText (g, juce::String (heatfx::osNames[p.oversampling]) + " OVERSAMPLING  ·  " + juce::String (lat) + " SMP",
              juce::Rectangle<float> (360.0f, 5.0f, 184.0f, 16.0f), juce::Justification::centredRight, cCoral.withAlpha (0.8f));

    paintCurve (g, p);
    paintSpectrum (g, p);
}

void HeatDisplay::paintCurve (juce::Graphics& g, const heatfx::Params& p)
{
    const auto a = curveArea;
    const int nb = p.bands;
    const int fb = juce::jlimit (0, nb - 1, focusBand);
    auto curvePath = [&] (int b, float& norm)
    {
        const auto& bp = p.band[(size_t) b];
        constexpr int N = 161;
        float ys[N];
        float mx = 1.0e-6f;
        for (int i = 0; i < N; ++i)
        {
            const float x = -1.0f + 2.0f * (float) i / (float) (N - 1);
            ys[i] = heatfx::transferCurve (bp.model, bp.drive, x);
            mx = juce::jmax (mx, std::abs (ys[i]));
        }
        norm = 0.9f / mx;
        juce::Path path;
        for (int i = 0; i < N; ++i)
        {
            const float px = a.getX() + a.getWidth() * (float) i / (float) (N - 1);
            const float py = a.getCentreY() - ys[i] * norm * a.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath (px, py);
            else path.lineTo (px, py);
        }
        return path;
    };

    g.saveState();
    g.reduceClipRegion (a.expanded (2.0f).toNearestInt());
    for (int b = 0; b < nb; ++b)
    {
        if (b == fb) continue;
        float norm;
        const auto path = curvePath (b, norm);
        g.setColour (bandColour (b, nb).withAlpha (0.22f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }
    float norm = 1.0f;
    const auto path = curvePath (fb, norm);
    const auto col = bandColour (fb, nb);
    // live input range of the focused band
    const float pk = juce::jlimit (0.0f, 1.0f, proc.uiBand[(size_t) fb].load());
    if (pk > 0.002f)
    {
        const float x0 = a.getCentreX() - pk * a.getWidth() * 0.5f, x1 = a.getCentreX() + pk * a.getWidth() * 0.5f;
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.0f), x0, 0, col.withAlpha (0.10f), a.getCentreX(), 0, false));
        g.fillRect (juce::Rectangle<float> (x0, a.getY(), a.getCentreX() - x0, a.getHeight()));
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.10f), a.getCentreX(), 0, col.withAlpha (0.0f), x1, 0, false));
        g.fillRect (juce::Rectangle<float> (a.getCentreX(), a.getY(), x1 - a.getCentreX(), a.getHeight()));
    }
    g.setColour (col.withAlpha (0.18f));
    g.strokePath (path, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col.withAlpha (0.35f));
    g.strokePath (path, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col.brighter (0.25f));
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (pk > 0.002f)
    {
        const auto& bp = p.band[(size_t) fb];
        for (float sx : { pk, -pk })
        {
            const float y = heatfx::transferCurve (bp.model, bp.drive, sx) * norm;
            const juce::Point<float> dot (a.getCentreX() + sx * a.getWidth() * 0.5f, a.getCentreY() - y * a.getHeight() * 0.5f);
            const float rad = sx > 0 ? 4.0f : 2.8f;
            g.setColour (cHot.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (rad * 4.0f, rad * 4.0f).withCentre (dot));
            g.setColour (sx > 0 ? cHot : cHot.withAlpha (0.6f));
            g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (dot));
        }
    }
    g.restoreState();
    // drive readout
    g.setFont (fnt (Fonts::get().mono, 11.5f));
    g.setColour (cInk);
    g.drawText ("IN", juce::Rectangle<float> (a.getRight() - 22.0f, a.getBottom() - 13.0f, 20.0f, 12.0f), juce::Justification::centredRight);
    g.drawText ("OUT", juce::Rectangle<float> (a.getX() + 2.0f, a.getY() + 1.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
}

void HeatDisplay::paintSpectrum (juce::Graphics& g, const heatfx::Params& p)
{
    const auto a = specArea;
    const int nb = p.bands;
    const float dbTop = 0.0f, dbBottom = -84.0f;
    auto yFor = [&] (float db) { return a.getY() + a.getHeight() * juce::jlimit (0.0f, 1.0f, (dbTop - db) / (dbTop - dbBottom)); };

    g.saveState();
    g.reduceClipRegion (a.toNearestInt());

    // band regions
    float edges[4] = { 20.0f, 20000.0f, 20000.0f, 20000.0f };
    if (nb >= 2) edges[1] = p.xoverLoHz;
    if (nb >= 3) edges[2] = std::max (p.xoverHiHz, p.xoverLoHz * 1.5f);
    edges[nb] = 20000.0f;
    for (int b = 0; b < nb; ++b)
    {
        const float x0 = xForFreq (edges[b]), x1 = xForFreq (edges[b + 1]);
        const auto col = bandColour (b, nb);
        const bool soloed = p.band[(size_t) b].solo;
        g.setGradientFill (juce::ColourGradient (col.withAlpha (soloed ? 0.16f : 0.07f), 0, a.getBottom(), col.withAlpha (0.0f), 0, a.getY(), false));
        g.fillRect (juce::Rectangle<float> (x0, a.getY(), x1 - x0, a.getHeight()));
        g.setFont (fnt (Fonts::get().label, 11.5f));
        g.setColour (col.withAlpha (0.85f));
        const float w = x1 - x0;
        if (w > 30.0f)
            g.drawText (juce::String (bandName (b, nb)) + (soloed ? "  SOLO" : ""), juce::Rectangle<float> (x0, a.getY() + 2.0f, w, 13.0f), juce::Justification::centred);
    }

    // LOW KEEP zone (untouched sub)
    if (p.lowKeepHz > heatfx::lowKeepOffHz + 0.5f)
    {
        const float xk = xForFreq (p.lowKeepHz);
        g.setColour (juce::Colour (0xff9fd8ff).withAlpha (0.06f));
        g.fillRect (juce::Rectangle<float> (a.getX(), a.getY(), xk - a.getX(), a.getHeight()));
        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (a.getX(), a.getY(), xk - a.getX(), a.getHeight()).toNearestInt());
        g.setColour (juce::Colour (0xff9fd8ff).withAlpha (0.14f));
        for (float hx = a.getX() - a.getHeight(); hx < xk; hx += 7.0f)
            g.drawLine (hx, a.getBottom(), hx + a.getHeight(), a.getY(), 1.0f);
        g.restoreState();
        g.setColour (juce::Colour (0xff9fd8ff).withAlpha (0.7f));
        g.drawLine (xk, a.getY(), xk, a.getBottom(), 1.0f);
        g.setFont (fnt (Fonts::get().label, 11.0f));
        g.drawText ("KEEP", juce::Rectangle<float> (a.getX(), a.getBottom() - 16.0f, juce::jmax (30.0f, xk - a.getX()), 13.0f), juce::Justification::centred);
    }

    // wet low / high cut: dim what is filtered away
    if (p.lowCutHz > heatfx::lowCutOffHz + 0.5f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (a.getX(), a.getY(), xForFreq (p.lowCutHz) - a.getX(), a.getHeight()));
    }
    if (p.highCutHz < heatfx::highCutOffHz - 5.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        const float xh = xForFreq (p.highCutHz);
        g.fillRect (juce::Rectangle<float> (xh, a.getY(), a.getRight() - xh, a.getHeight()));
    }

    // spectra
    const int cols = (int) outDb.size();
    juce::Path inP, outP, outFill;
    for (int c = 0; c < cols; ++c)
    {
        const float x = a.getX() + (float) c + 0.5f;
        const float yi = yFor (inDb[(size_t) c]), yo = yFor (outDb[(size_t) c]);
        if (c == 0) { inP.startNewSubPath (x, yi); outP.startNewSubPath (x, yo); outFill.startNewSubPath (x, a.getBottom()); }
        else { inP.lineTo (x, yi); outP.lineTo (x, yo); }
        outFill.lineTo (x, yo);
    }
    outFill.lineTo (a.getRight(), a.getBottom());
    outFill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (cHot.withAlpha (0.42f), 0, a.getY() + a.getHeight() * 0.15f, cCoral.withAlpha (0.04f), 0, a.getBottom(), false));
    g.fillPath (outFill);
    g.setColour (cText.withAlpha (0.30f));
    g.strokePath (inP, juce::PathStrokeType (1.0f));
    g.setColour (cCoral.withAlpha (0.35f));
    g.strokePath (outP, juce::PathStrokeType (3.5f));
    g.setColour (cCoral.brighter (0.3f));
    g.strokePath (outP, juce::PathStrokeType (1.4f));

    // crossover handles
    for (int k = 0; k < nb - 1; ++k)
    {
        const float f = edges[k + 1];
        const float x = xForFreq (f);
        const bool lit = dragging == k || hover == k;
        g.setColour (cText.withAlpha (lit ? 0.9f : 0.45f));
        const float dashes[] = { 4.0f, 3.0f };
        juce::Path line, dashed;
        line.startNewSubPath (x, a.getY() + 16.0f);
        line.lineTo (x, a.getBottom());
        juce::PathStrokeType (lit ? 1.6f : 1.0f).createDashedStroke (dashed, line, dashes, 2);
        g.fillPath (dashed);
        const juce::String t = hzText (f) + " Hz";
        g.setFont (fnt (Fonts::get().mono, 11.0f));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 10.0f;
        auto chip = juce::Rectangle<float> (tw, 14.0f).withCentre ({ juce::jlimit (a.getX() + tw * 0.5f, a.getRight() - tw * 0.5f, x), a.getY() + 24.0f });
        g.setColour (juce::Colour (0xff140d0c).withAlpha (0.92f));
        g.fillRoundedRectangle (chip, 3.0f);
        g.setColour (lit ? cHot : cText.withAlpha (0.7f));
        g.drawRoundedRectangle (chip, 3.0f, 1.0f);
        g.drawText (t, chip, juce::Justification::centred);
    }
    g.restoreState();
}

int HeatDisplay::crossoverAt (float x) const
{
    const auto p = proc.currentParams();
    int best = -1;
    float bd = 7.0f;
    for (int k = 0; k < p.bands - 1; ++k)
    {
        const float f = k == 0 ? p.xoverLoHz : std::max (p.xoverHiHz, p.xoverLoHz * 1.5f);
        const float dist = std::abs (xForFreq (f) - x);
        if (dist < bd) { bd = dist; best = k; }
    }
    return best;
}

void HeatDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = specArea.expanded (0, 4).contains (e.position) ? crossoverAt (e.position.x) : -1;
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void HeatDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = specArea.expanded (0, 4).contains (e.position) ? crossoverAt (e.position.x) : -1;
    if (dragging >= 0)
        if (auto* prm = proc.apvts.getParameter (dragging == 0 ? IDs::xoverLo : IDs::xoverHi))
            prm->beginChangeGesture();
}

void HeatDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    if (auto* prm = proc.apvts.getParameter (dragging == 0 ? IDs::xoverLo : IDs::xoverHi))
    {
        float f = freqForX (e.position.x);
        const auto p = proc.currentParams();
        if (dragging == 0 && p.bands == 3) f = std::min (f, p.xoverHiHz / 1.5f);
        if (dragging == 1) f = std::max (f, p.xoverLoHz * 1.5f);
        prm->setValueNotifyingHost (prm->convertTo0to1 (f));
    }
    repaint();
}

void HeatDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        if (auto* prm = proc.apvts.getParameter (dragging == 0 ? IDs::xoverLo : IDs::xoverHi))
            prm->endChangeGesture();
    dragging = -1;
    repaint();
}

void HeatDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int k = specArea.expanded (0, 4).contains (e.position) ? crossoverAt (e.position.x) : -1;
    if (k < 0) return;
    if (auto* prm = proc.apvts.getParameter (k == 0 ? IDs::xoverLo : IDs::xoverHi))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->getDefaultValue());
        prm->endChangeGesture();
    }
}

// ================= Panel =================

HeatPanel::HeatPanel (HeatProcessor& p)
    : proc (p), display (p), autoGain (*p.apvts.getParameter (IDs::autoGain), p)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    auto setupKnob = [this] (Knob& k, const juce::String& id, const char* tip)
    {
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setTooltip (tip);
        k.s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.s);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.s);
        if (auto* prm = proc.apvts.getParameter (id))
            k.s.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    };

    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        auto& u = bandUi[(size_t) b];
        setupKnob (u.drive, IDs::band (b, "drive"), "DRIVE: how hard this band hits the saturation model");
        setupKnob (u.mix, IDs::band (b, "mix"), "Band MIX: parallel blend of the clean and the saturated band");
        setupKnob (u.level, IDs::band (b, "level"), "Band LEVEL: output level of this band");
        u.drive.s.getProperties().set ("hot", true);
        u.model = std::make_unique<ModelSelector> (*proc.apvts.getParameter (IDs::band (b, "model")));
        u.model->setTooltip ("Saturation model of this band. Arrows / mouse wheel = step, click the name = list.");
        addAndMakeVisible (*u.model);
        u.solo.setClickingTogglesState (true);
        u.solo.setTooltip ("SOLO: listen to this band only");
        addAndMakeVisible (u.solo);
        u.soloA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::band (b, "solo"), u.solo);
    }
    setupKnob (punch, IDs::punch, "PUNCH: pre-compression before the saturation. Tames the sustain so transients hit harder.");
    setupKnob (dynamics, IDs::dynamics, "DYNAMICS: drive follows the envelope. + = louder parts crunch more, - = the tails saturate more.");
    setupKnob (lowKeep, IDs::lowKeep, "LOW KEEP: everything below this frequency bypasses the saturation (keeps 808 subs clean). OFF at minimum.");
    setupKnob (tiltPre, IDs::tiltPre, "TILT PRE: tilts the tone into the saturation (+ = brighter, more top-end grit)");
    setupKnob (tiltPost, IDs::tiltPost, "TILT POST: tilts the tone after the saturation");
    setupKnob (lowCut, IDs::lowCut, "LOW CUT of the saturated signal (OFF at minimum)");
    setupKnob (highCut, IDs::highCut, "HIGH CUT of the saturated signal: tames fizz (OFF at maximum)");
    setupKnob (mix, IDs::mix, "MIX: global dry / wet (dry is latency compensated)");
    setupKnob (output, IDs::output, "OUTPUT level");
    dynamics.s.getProperties().set ("bipolar", true);
    tiltPre.s.getProperties().set ("bipolar", true);
    tiltPost.s.getProperties().set ("bipolar", true);
    lowKeep.s.getProperties().set ("colour", (juce::int64) 0xff9fd8ff);

    addAndMakeVisible (display);

    bandsAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::bands), [this] (float v)
    {
        bandsState = juce::jlimit (0, 2, juce::roundToInt (v));
        refreshSelectors();
    }, nullptr);
    for (int i = 0; i < 3; ++i)
    {
        auto& b = bandButtons[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.onClick = [this, i] { bandsAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip ("Number of bands (Linkwitz-Riley crossovers, drag them in the spectrum)");
        addAndMakeVisible (b);
    }
    osAttachment = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (IDs::os), [this] (float v)
    {
        osState = juce::jlimit (0, heatfx::numOsChoices - 1, juce::roundToInt (v));
        refreshSelectors();
    }, nullptr);
    for (int i = 0; i < heatfx::numOsChoices; ++i)
    {
        auto& b = osButtons[(size_t) i];
        b.setButtonText (heatfx::osNames[i]);
        b.onClick = [this, i] { osAttachment->setValueAsCompleteGesture ((float) i); };
        b.setTooltip ("Oversampling: higher = less aliasing (cleaner highs), more CPU. Latency is reported to the host.");
        addAndMakeVisible (b);
    }
    bandsAttachment->sendInitialUpdate();
    osAttachment->sendInitialUpdate();

    autoGain.setTooltip ("AUTO GAIN: keeps the saturated signal at the input loudness, so you judge tone, not volume");
    addAndMakeVisible (autoGain);

    // presets
    const auto& presets = heatfx::factoryPresets();
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

    refreshSelectors();
    display.tick();
    startTimerHz (30);
}

HeatPanel::~HeatPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HeatPanel::setFocusBand (int b)
{
    focusBand = b;
    display.focusBand = b;
    display.repaint();
    repaint();
}

void HeatPanel::refreshSelectors()
{
    for (int i = 0; i < 3; ++i)
        bandButtons[(size_t) i].setToggleState (i == bandsState, juce::dontSendNotification);
    for (int i = 0; i < heatfx::numOsChoices; ++i)
        osButtons[(size_t) i].setToggleState (i == osState, juce::dontSendNotification);
    const int nb = bandsState + 1;
    if (focusBand >= nb) setFocusBand (nb - 1);
    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        auto& u = bandUi[(size_t) b];
        const bool active = b < nb;
        const auto col = bandColour (b, nb);
        for (auto* k : { &u.drive, &u.mix, &u.level })
        {
            k->s.setEnabled (active);
            k->s.setInterceptsMouseClicks (active, active);
            k->s.getProperties().set ("colour", (juce::int64) col.getARGB());
            k->s.repaint();
        }
        u.model->setEnabled (active);
        u.model->setInterceptsMouseClicks (active, active);
        u.model->colour = col;
        u.model->repaint();
        u.solo.setEnabled (active);
        u.solo.setInterceptsMouseClicks (active, active);
        u.solo.getProperties().set ("colour", (juce::int64) col.getARGB());
        u.solo.repaint();
    }
    repaint();
}

void HeatPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int b = 0; b < heatfx::maxBands; ++b)
        if (juce::Rectangle<int> (bandX[b], bandTop, bandW, bandBottom - bandTop).contains (e.getPosition()))
        {
            if (b > bandsState)
                bandsAttachment->setValueAsCompleteGesture ((float) b);   // clicking a sleeping band wakes it up
            setFocusBand (b);
            return;
        }
}

void HeatPanel::timerCallback()
{
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    display.tick();
    if (autoGain.isOn()) autoGain.repaint();
}

void HeatPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    if (autoGain.isOn())
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (cCoral.withAlpha (0.07f * (float) (4 - k)));
            g.fillRoundedRectangle (autoRect.toFloat().expanded ((float) k * 3.0f), 10.0f + (float) k * 3.0f);
        }
    // focused band frame
    const int nb = bandsState + 1;
    if (focusBand < nb)
    {
        const auto r = juce::Rectangle<float> ((float) bandX[focusBand], (float) bandTop, (float) bandW, (float) (bandBottom - bandTop)).reduced (1.5f);
        const auto col = bandColour (focusBand, nb);
        g.setColour (col.withAlpha (0.10f));
        g.drawRoundedRectangle (r.expanded (2.0f), 13.0f, 4.0f);
        g.setColour (col.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 11.0f, 1.2f);
    }
}

void HeatPanel::paintOverChildren (juce::Graphics& g)
{
    const auto p = proc.currentParams();
    const int nb = p.bands;
    auto valueOf = [] (Knob& k) { return k.s.getTextFromValue (k.s.getValue()); };

    // ---- band modules: title, range, values
    float edges[4] = { 20.0f, 20000.0f, 20000.0f, 20000.0f };
    if (nb >= 2) edges[1] = p.xoverLoHz;
    if (nb >= 3) edges[2] = std::max (p.xoverHiHz, p.xoverLoHz * 1.5f);
    edges[nb] = 20000.0f;
    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        auto& u = bandUi[(size_t) b];
        const float x0 = (float) bandX[b];
        const bool active = b < nb;
        const auto col = bandColour (b, nb);
        g.setFont (fnt (Fonts::get().heavy, 15.0f));
        if (active) glowText (g, bandName (b, nb), juce::Rectangle<float> (x0 + 14.0f, 318.0f, 70.0f, 22.0f), juce::Justification::centredLeft, col);
        else { g.setColour (cInk.withAlpha (0.5f)); g.drawText ("BAND " + juce::String (b + 1), juce::Rectangle<float> (x0 + 14.0f, 318.0f, 90.0f, 22.0f), juce::Justification::centredLeft); }
        if (active)
        {
            juce::String range;
            if (nb == 1) range = "20 - 20k Hz";
            else if (b == 0) range = "< " + hzText (edges[1]) + " Hz";
            else if (b == nb - 1) range = "> " + hzText (edges[b]) + " Hz";
            else range = hzText (edges[b]) + " - " + hzText (edges[b + 1]) + " Hz";
            g.setFont (fnt (Fonts::get().label, 12.5f));
            g.setColour (cInk);
            g.drawText (range, juce::Rectangle<float> (x0 + 60.0f, 318.0f, 80.0f, 22.0f), juce::Justification::centredRight);

            g.setFont (fnt (Fonts::get().mono, 17.0f));
            glowText (g, valueOf (u.drive), juce::Rectangle<float> (x0 + driveDx - 34.0f, 504.0f, 68.0f, 20.0f), juce::Justification::centred,
                      u.drive.s.getValue() > 0.7 ? col.interpolatedWith (cHot, 0.6f) : col);
            g.setFont (fnt (Fonts::get().mono, 11.5f));
            g.setColour (cText.withAlpha (0.85f));
            g.drawText (valueOf (u.mix), juce::Rectangle<float> (x0 + smallDx - 30.0f, 439.0f, 60.0f, 12.0f), juce::Justification::centred);
            g.drawText (valueOf (u.level), juce::Rectangle<float> (x0 + smallDx - 30.0f, 511.0f, 60.0f, 12.0f), juce::Justification::centred);
        }
        else
        {
            const auto r = juce::Rectangle<float> (x0, (float) bandTop, (float) bandW, (float) (bandBottom - bandTop)).reduced (1.0f);
            g.setColour (juce::Colour (0xff0c0909).withAlpha (0.72f));
            g.fillRoundedRectangle (r, 12.0f);
            g.setFont (fnt (Fonts::get().heavy, 15.0f));
            g.setColour (cInk.withAlpha (0.8f));
            g.drawText ("BAND OFF", r.withTrimmedBottom (20.0f), juce::Justification::centred);
            g.setFont (fnt (Fonts::get().label, 13.0f));
            g.setColour (cInk.withAlpha (0.65f));
            g.drawText ("click to add band " + juce::String (b + 1), r.withTrimmedTop (24.0f), juce::Justification::centred);
        }
    }

    // ---- global LCDs
    g.setFont (fnt (Fonts::get().mono, 15.0f));
    Knob* top[3] = { &punch, &dynamics, &lowKeep };
    for (int i = 0; i < 3; ++i)
    {
        const auto col = i == 2 ? juce::Colour (0xff9fd8ff) : cCoral;
        glowText (g, valueOf (*top[i]), juce::Rectangle<float> ((float) topKnobX[i] - 36.0f, 263.0f, 72.0f, 18.0f), juce::Justification::centred, col);
    }
    g.setFont (fnt (Fonts::get().mono, 11.5f));
    g.setColour (cText.withAlpha (0.85f));
    Knob* tone[4] = { &tiltPre, &tiltPost, &lowCut, &highCut };
    for (int i = 0; i < 4; ++i)
        g.drawText (valueOf (*tone[i]), juce::Rectangle<float> ((float) toneX[i] - 32.0f, 393.0f, 64.0f, 12.0f), juce::Justification::centred);
    Knob* outs[2] = { &mix, &output };
    for (int i = 0; i < 2; ++i)
        g.drawText (valueOf (*outs[i]), juce::Rectangle<float> ((float) outX[i] - 32.0f, 508.0f, 64.0f, 12.0f), juce::Justification::centred);
}

void HeatPanel::resized()
{
    display.setBounds (displayRect);
    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        auto& u = bandUi[(size_t) b];
        const int x0 = bandX[b];
        u.drive.s.setBounds (knobBounds (x0 + driveDx, driveY, driveR));
        u.mix.s.setBounds (knobBounds (x0 + smallDx, mixY, smallR));
        u.level.s.setBounds (knobBounds (x0 + smallDx, levelY, smallR));
        u.model->setBounds (x0 + 12, 348, bandW - 24, 22);
        u.solo.setBounds (x0 + bandW - 34, 319, 22, 20);
    }
    punch.s.setBounds (knobBounds (topKnobX[0], topKnobY, topKnobR));
    dynamics.s.setBounds (knobBounds (topKnobX[1], topKnobY, topKnobR));
    lowKeep.s.setBounds (knobBounds (topKnobX[2], topKnobY, topKnobR));
    tiltPre.s.setBounds (knobBounds (toneX[0], toneY, toneR));
    tiltPost.s.setBounds (knobBounds (toneX[1], toneY, toneR));
    lowCut.s.setBounds (knobBounds (toneX[2], toneY, toneR));
    highCut.s.setBounds (knobBounds (toneX[3], toneY, toneR));
    mix.s.setBounds (knobBounds (outX[0], outY, outR));
    output.s.setBounds (knobBounds (outX[1], outY, outR));
    for (int i = 0; i < 3; ++i)
        bandButtons[(size_t) i].setBounds (666 + 58 * i, 102, 54, 24);
    for (int i = 0; i < heatfx::numOsChoices; ++i)
        osButtons[(size_t) i].setBounds (666 + 43 * i, 136, 41, 24);
    autoGain.setBounds (autoRect);
    prevButton.setBounds (486, 28, 30, 30);
    presetBox.setBounds (520, 28, 280, 30);
    nextButton.setBounds (804, 28, 30, 30);
}

// ================= Editor =================

HeatEditor::HeatEditor (HeatProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (HeatPanel::baseW * 3 / 4, HeatPanel::baseH * 3 / 4, HeatPanel::baseW * 2, HeatPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) HeatPanel::baseW / HeatPanel::baseH);
    setSize (HeatPanel::baseW, HeatPanel::baseH);
}

void HeatEditor::resized()
{
    panel.setBounds (0, 0, HeatPanel::baseW, HeatPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / HeatPanel::baseW));
}
