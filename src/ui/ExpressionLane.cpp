#include "ExpressionLane.h"
#include "VoiceField.h"
#include "Palette.h"
#include "Fonts.h"
#include "../PatternStore.h"
#include <cmath>

namespace bd
{

namespace
{
    const char* const tabNames[] = { "Pressure", "Brightness", "Velocity", "Release" };

    juce::Colour modeColour (ExpressionLane::Mode m)
    {
        switch (m)
        {
            case ExpressionLane::Mode::pressure: return pal::lime;
            case ExpressionLane::Mode::timbre:   return pal::amber;
            case ExpressionLane::Mode::velocity: return pal::cyan;
            case ExpressionLane::Mode::release:  return pal::violet;
        }
        return pal::cyan;
    }
}

ExpressionLane::ExpressionLane (VoiceField& f, PatternStore& s) : field (f), store (s)
{
    setTooltip ("LANE - draw PRESSURE (loudness) or BRIGHTNESS (CC74 slide) of the selected notes over time, set VELOCITY / RELEASE. Right-click = reset");
    setOpaque (true);
}

juce::Rectangle<float> ExpressionLane::plotArea() const
{
    const auto fb = field.getFieldBounds();
    const float top = tabArea (0).getBottom() + 4.0f;
    return { fb.getX(), top, fb.getWidth(), (float) getHeight() - top - 6.0f };
}

juce::Rectangle<float> ExpressionLane::tabArea (int i) const
{
    // a row of tabs at the top-left of the lane
    const float s = field.getUiScale();
    const float w = 96.0f * s, h = 24.0f * s;
    return { field.getFieldBounds().getX() + (w + 4.0f * s) * (float) i, 6.0f * s, w, h };
}

float ExpressionLane::valueToY (float v) const
{
    const auto r = plotArea();
    return r.getBottom() - juce::jlimit (0.0f, 1.0f, v) * r.getHeight();
}

float ExpressionLane::yToValue (float y) const
{
    const auto r = plotArea();
    return juce::jlimit (0.0f, 1.0f, (r.getBottom() - y) / juce::jmax (1.0f, r.getHeight()));
}

std::vector<uint32_t> ExpressionLane::targets (double b0, double b1) const
{
    std::vector<uint32_t> out;
    const auto& sel = store.getSelection();
    for (auto& l : store.get().lines)
    {
        const bool chosen = sel.empty() ? (l.end() >= b0 && l.start <= b1) : store.isSelected (l.id);
        if (chosen) out.push_back (l.id);
    }
    return out;
}

//==============================================================================
void ExpressionLane::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    const float s = field.getUiScale();

    g.fillAll (pal::ground1);
    g.setColour (pal::hairline);
    g.fillRect (b.withHeight (1.0f));
    // the keys column of the canvas continues here
    g.setColour (pal::ground0);
    g.fillRect (b.withWidth (field.getFieldBounds().getX() - 3.0f));

    // tabs
    for (int i = 0; i < 4; ++i)
    {
        const auto m = (Mode) i;
        const auto r = tabArea (i).reduced (1.0f, 1.0f);
        const bool on = m == mode;
        if (on)
        {
            g.setColour (modeColour (m).withAlpha (0.16f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (modeColour (m).withAlpha (0.8f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
        else
        {
            g.setColour (pal::panel);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (pal::hairline);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
        g.setColour (on ? pal::text : pal::textDim);
        g.setFont (fonts::ui (jlimit (10.0f, 13.5f, r.getHeight() * 0.52f), on));
        g.drawFittedText (tabNames[i], r.toNearestInt(), Justification::centred, 1, 0.7f);
    }

    const auto plot = plotArea();
    g.saveState();
    g.reduceClipRegion (plot.toNearestInt().expanded (0, 3));

    // guides + bars
    for (float v : { 0.0f, 0.5f, 1.0f })
    {
        g.setColour (pal::text.withAlpha (v > 0.4f && v < 0.6f ? 0.05f : 0.08f));
        g.fillRect (Rectangle<float> (plot.getX(), valueToY (v) - 0.5f, plot.getWidth(), 1.0f));
    }
    const auto pv = field.getPatternView();
    const int bpb = store.get().beatsPerBar;
    for (double beat = std::ceil (pv.start); beat <= pv.start + pv.beats; beat += 1.0)
    {
        const bool bar = std::abs (std::fmod (beat + 1.0e-6, (double) bpb)) < 2.0e-6;
        g.setColour ((bar ? pal::violet : pal::textDim).withAlpha (bar ? 0.22f : 0.06f));
        g.fillRect (Rectangle<float> (field.beatToX (beat) - 0.5f, plot.getY(), 1.0f, plot.getHeight()));
    }

    const auto col = modeColour (mode);
    const auto& sel = store.getSelection();

    for (auto& l : store.get().lines)
    {
        const bool target = sel.empty() || store.isSelected (l.id);
        const float a = target ? 1.0f : 0.25f;
        const float x0 = field.beatToX (l.start), x1 = field.beatToX (l.end());
        if (x1 < plot.getX() || x0 > plot.getRight()) continue;

        if (mode == Mode::velocity || mode == Mode::release)
        {
            const bool rel = mode == Mode::release;
            const float y = valueToY (rel ? l.releaseVelocity : l.velocity);
            const float x0v = rel ? x1 : x0;
            g.setColour (col.withAlpha (0.35f * a));
            g.fillRect (Rectangle<float> (x0v - 1.0f, y, 2.0f, plot.getBottom() - y));
            g.setColour (col.withAlpha (0.25f * a));
            g.fillEllipse (Rectangle<float> (11.0f * s, 11.0f * s).withCentre ({ x0v, y }));
            g.setColour (col.withAlpha (a));
            g.fillEllipse (Rectangle<float> (5.0f * s, 5.0f * s).withCentre ({ x0v, y }));
            continue;
        }

        Path p, fill;
        const int n = jlimit (2, 400, (int) ((x1 - x0) / 3.0f));
        for (int i = 0; i <= n; ++i)
        {
            const double t = l.length * (double) i / (double) n;
            const float v = mode == Mode::pressure ? l.pressureAt (t) : l.timbreAt (t);
            const Point<float> q (field.beatToX (l.start + t), valueToY (v));
            if (i == 0) { p.startNewSubPath (q); fill.startNewSubPath (q.x, plot.getBottom()); fill.lineTo (q); }
            else        { p.lineTo (q); fill.lineTo (q); }
        }
        fill.lineTo (x1, plot.getBottom());
        fill.closeSubPath();

        ColourGradient fg (col.withAlpha (0.18f * a), 0.0f, plot.getY(), col.withAlpha (0.02f * a), 0.0f, plot.getBottom(), false);
        g.setGradientFill (fg);
        g.fillPath (fill);
        g.setColour (col.withAlpha (0.25f * a));
        g.strokePath (p, PathStrokeType (4.0f * s, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col.withAlpha (0.9f * a));
        g.strokePath (p, PathStrokeType (1.3f * s, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // live stroke
    if (drawing && stroke.size() >= 2 && mode != Mode::velocity && mode != Mode::release)
    {
        Path p;
        for (size_t i = 0; i < stroke.size(); ++i)
        {
            const Point<float> q (field.beatToX (stroke[i].t), valueToY (stroke[i].v));
            if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        g.setColour (Colours::white.withAlpha (0.6f));
        g.strokePath (p, PathStrokeType (1.0f));
    }

    if (store.get().lines.empty())
    {
        g.setColour (pal::textFaint);
        g.setFont (fonts::ui (jmax (10.0f, 12.5f * s)));
        g.drawText ("Draw some notes first", plot.toNearestInt(), Justification::centred);
    }

    g.restoreState();
}

//==============================================================================
void ExpressionLane::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 4; ++i)
        if (tabArea (i).contains (e.position))
        {
            setMode ((Mode) i);
            if (onModeChanged) onModeChanged();
            return;
        }

    if (! plotArea().expanded (0.0f, 4.0f).contains (e.position))
        return;

    const double beat = field.xToBeat (e.position.x);

    if (e.mods.isPopupMenu())
    {
        auto p = store.get();
        bool any = false;
        for (auto id : targets (beat, beat))
            if (auto* l = p.find (id))
            {
                any = true;
                if (mode == Mode::pressure) l->pressure.clear();
                else if (mode == Mode::timbre) l->timbre.clear();
                else if (mode == Mode::release) l->releaseVelocity = 0.5f;
                else l->velocity = 0.8f;
            }
        if (any) store.perform (std::move (p), "Reset expression");
        return;
    }

    base = store.get();
    store.beginDrag();
    drawing = true;
    stroke.clear();
    stroke.push_back ({ beat, yToValue (e.position.y) });
    baseTargets = targets (beat, beat);
    applyStroke();
}

void ExpressionLane::mouseDrag (const juce::MouseEvent& e)
{
    if (! drawing) return;
    stroke.push_back ({ field.xToBeat (e.position.x), yToValue (e.position.y) });

    // when nothing is selected, every line the stroke crosses becomes a target
    if (store.getSelection().empty())
    {
        double lo = 1.0e9, hi = -1.0e9;
        for (auto& p : stroke) { lo = std::min (lo, p.t); hi = std::max (hi, p.t); }
        for (auto& l : base.lines)
            if (l.end() >= lo && l.start <= hi && std::find (baseTargets.begin(), baseTargets.end(), l.id) == baseTargets.end())
                baseTargets.push_back (l.id);
    }

    applyStroke();
    repaint();
}

void ExpressionLane::mouseUp (const juce::MouseEvent&)
{
    if (! drawing) return;
    drawing = false;
    store.commitDrag (mode == Mode::pressure ? "Draw pressure" : mode == Mode::timbre ? "Draw brightness"
                      : mode == Mode::release ? "Set release velocity" : "Set velocity");
    stroke.clear();
    repaint();
}

void ExpressionLane::applyStroke()
{
    if (stroke.empty()) return;

    auto sorted = stroke;
    std::stable_sort (sorted.begin(), sorted.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.t < b.t; });
    const double b0 = sorted.front().t, b1 = sorted.back().t;
    const float px = (float) (field.getPatternView().beats / juce::jmax (1.0f, field.getFieldBounds().getWidth()));

    auto p = base;
    for (auto id : baseTargets)
    {
        auto* l = p.find (id);
        if (l == nullptr) continue;

        if (mode == Mode::velocity || mode == Mode::release)
        {
            // the stroke sets the (release) velocity of every target line whose start (end) it
            // passes, or of the line under a plain click
            const bool rel = mode == Mode::release;
            const double anchor = rel ? l->end() : l->start;
            float& target = rel ? l->releaseVelocity : l->velocity;
            const float lo = rel ? 0.0f : 0.01f;
            const double tol = 6.0 * px;
            for (auto& sp : stroke)
                if (std::abs (sp.t - anchor) <= tol || (stroke.size() == 1 && sp.t >= l->start && sp.t <= l->end()))
                    target = juce::jlimit (lo, 1.0f, sp.v);
            if (b1 - b0 > tol && anchor >= b0 && anchor <= b1)
                target = juce::jlimit (lo, 1.0f, curves::evalLinear (sorted, anchor));
            continue;
        }

        const double r0 = std::max (l->start, b0) - l->start;
        const double r1 = std::min (l->end(), b1) - l->start;
        if (r1 < r0 - 1.0e-9) continue;

        auto& curve = mode == Mode::pressure ? l->pressure : l->timbre;
        const float def = mode == Mode::pressure ? PatternLine::defaultPressure : PatternLine::defaultTimbre;
        Curve1D orig = curve;
        if (orig.empty()) orig = { { 0.0, def }, { l->length, def } };

        Curve1D out;
        for (auto& pt : orig)
            if (pt.t < r0 - 1.0e-6 || pt.t > r1 + 1.0e-6) out.push_back (pt);

        if (sorted.size() == 1)
            out.push_back ({ juce::jlimit (0.0, l->length, r0), sorted.front().v });
        else
            for (auto& sp : sorted)
            {
                const double t = sp.t - l->start;
                if (t >= r0 - 1.0e-9 && t <= r1 + 1.0e-9) out.push_back ({ t, sp.v });
            }

        curves::normalise (out);
        curve = curves::simplify (out, 0.008f);
    }

    store.setLive (p);
}

} // namespace bd
