/*
    TrapWin BEND DEMON
    VoiceFieldDraw.cpp - DRAW mode of the Voice Field: the pattern canvas (2.0).
*/
#include "VoiceField.h"
#include "LineEditor.h"
#include "Palette.h"
#include "Fonts.h"
#include "LookAndFeel.h"
#include "../PatternStore.h"
#include <array>
#include <cmath>

namespace bd
{

namespace
{
    bool isBlackKey (int note) noexcept
    {
        const int n = ((note % 12) + 12) % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    juce::String noteName (int note)
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1);
    }

}

//==============================================================================
// view + mapping

float VoiceField::beatToX (double beat) const noexcept
{
    return field.getX() + (float) ((beat - pview.start) / juce::jmax (1.0e-6, pview.beats)) * field.getWidth();
}

double VoiceField::xToBeat (float x) const noexcept
{
    return pview.start + (double) ((x - field.getX()) / juce::jmax (1.0f, field.getWidth())) * pview.beats;
}

float VoiceField::pitchToY (float pitch) const noexcept
{
    return field.getBottom() - (pitch - pview.lo) / juce::jmax (1.0f, pview.hi - pview.lo) * field.getHeight();
}

float VoiceField::yToPitch (float y) const noexcept
{
    return pview.lo + (field.getBottom() - y) / juce::jmax (1.0f, field.getHeight()) * (pview.hi - pview.lo);
}

void VoiceField::setPatternView (PatternView v)
{
    pview = v;
    clampPatternView();
}

void VoiceField::clampPatternView()
{
    const double L = store != nullptr ? store->get().lengthBeats() : 16.0;
    pview.beats = juce::jlimit (0.5, std::max (L, 1.0), pview.beats);
    pview.start = juce::jlimit (0.0, std::max (0.0, L - pview.beats), pview.start);

    float span = juce::jlimit (6.0f, 96.0f, pview.hi - pview.lo);
    pview.lo = juce::jlimit (0.0f, 127.0f - span, pview.lo);
    pview.hi = pview.lo + span;
}

void VoiceField::fitToPattern()
{
    if (store == nullptr) return;
    const auto& p = store->get();
    pview.start = 0.0;
    pview.beats = p.lengthBeats();

    float lo = 1000.0f, hi = -1000.0f;
    for (auto& l : p.lines)
    {
        float a, b;
        l.pitchRange (a, b);
        lo = std::min (lo, a); hi = std::max (hi, b);
    }
    if (lo > hi) { lo = 55.0f; hi = 79.0f; }
    const float mid = 0.5f * (lo + hi);
    const float span = juce::jmax (20.0f, hi - lo + 8.0f);
    pview.lo = mid - span * 0.5f;
    pview.hi = mid + span * 0.5f;
    clampPatternView();
}

void VoiceField::scrollPitch (float semis)
{
    pview.lo += semis;
    pview.hi += semis;
    clampPatternView();
}

void VoiceField::scrollTime (double beats)
{
    pview.start += beats;
    clampPatternView();
}

void VoiceField::zoomTime (float factor, float anchorX)
{
    const double anchor = xToBeat (anchorX);
    const double frac = (double) ((anchorX - field.getX()) / juce::jmax (1.0f, field.getWidth()));
    pview.beats *= (double) factor;
    clampPatternView();
    pview.start = anchor - frac * pview.beats;
    clampPatternView();
}

void VoiceField::zoomPitch (float factor, float anchorY)
{
    const float anchor = yToPitch (anchorY);
    const float frac = (field.getBottom() - anchorY) / juce::jmax (1.0f, field.getHeight());
    const float span = juce::jlimit (6.0f, 96.0f, (pview.hi - pview.lo) * factor);
    pview.lo = anchor - frac * span;
    pview.hi = pview.lo + span;
    clampPatternView();
}

double VoiceField::playheadInPattern() const noexcept
{
    if (! seqRunning || store == nullptr) return -1.0;
    const double L = store->get().lengthBeats();
    if (L <= 0.0) return -1.0;
    double x = std::fmod (seqPpq, L);
    if (x < 0.0) x += L;
    return x;
}

void VoiceField::updateDrawView()
{
    clampPatternView();
    view.viewStart  = pview.start;
    view.viewBeats  = pview.beats;
    view.pitchLo    = pview.lo;
    view.pitchHi    = pview.hi;
    view.beatsPerBar = store != nullptr ? store->get().beatsPerBar : 4;
    view.playheadBeat = playheadInPattern();
    view.pageMode = false;
}

//==============================================================================
// painting

void VoiceField::paintDrawMode (juce::Graphics& g)
{
    using namespace juce;
    const float s = getUiScale();

    drawPatternGrid (g);
    drawPatternLines (g);

    if (view.playheadBeat >= 0.0 && view.playheadBeat >= pview.start && view.playheadBeat <= pview.start + pview.beats)
    {
        g.saveState();
        if (dimension < 0.01f) g.reduceClipRegion (field.toNearestInt().expanded (0, 2));
        drawPlayhead (g);
        g.restoreState();
    }
    drawActiveHeads (g);

    if (canEdit() && controller != nullptr)
        controller->paintOverlay (g);

    if (dimension < 0.5f)
        drawKeysAndRuler (g);

    // HUD
    auto hud = getLocalBounds().toFloat().reduced (10.0f * s, 6.0f * s);
    g.setFont (fonts::ui (jmax (10.0f, 12.0f * s)));
    g.setColour (pal::textDim);
    const int bars = store != nullptr ? store->get().bars : 4;
    juce::ignoreUnused (bars, hud);

    if (dimensionTarget > 0.5f)
    {
        g.setColour (pal::violet.withAlpha (0.8f));
        g.setFont (fonts::ui (jmax (10.0f, 12.5f * s), true));
        g.drawText (String (CharPointer_UTF8 ("3D VIEW  \xc2\xb7  turn 3D off to edit")),
                    getLocalBounds().reduced ((int) (14 * s), (int) (6 * s)).withTrimmedLeft ((int) (52 * s)), Justification::topLeft);
    }

    const bool empty = store == nullptr || store->get().lines.empty();
    if (empty && hint.isNotEmpty())
    {
        g.setColour (pal::text.withAlpha (0.85f));
        g.setFont (fonts::ui (jmax (11.0f, 15.0f * s)));
        g.drawFittedText (hint, field.toNearestInt().reduced (30), Justification::centred, 4);
    }

    if (toast.isNotEmpty() && time < toastUntil)
    {
        const float a = jlimit (0.0f, 1.0f, (toastUntil - time) * 2.0f);
        g.setFont (fonts::ui (jmax (11.0f, 13.0f * s), true));
        const float w = jmin (field.getWidth() - 20.0f, GlyphArrangement::getStringWidth (g.getCurrentFont(), toast) + 30.0f);
        auto r = Rectangle<float> (w, 32.0f * s).withCentre ({ field.getCentreX(), field.getBottom() - 24.0f * s });
        g.setColour (pal::raised.withAlpha (0.97f * a));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (pal::hairline.withAlpha (a));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (pal::text.withAlpha (a));
        g.drawFittedText (toast, r.reduced (10.0f, 2.0f).toNearestInt(), Justification::centred, 1, 0.7f);
    }
}

void VoiceField::drawPatternGrid (juce::Graphics& g)
{
    using namespace juce;
    const float s = getUiScale();
    const double L = store != nullptr ? store->get().lengthBeats() : 16.0;
    const int bpb = jmax (1, view.beatsPerBar);
    const float d = dimension;

    auto line = [&] (Point<float> a, Point<float> b, Colour c, float w)
    {
        g.setColour (c);
        g.drawLine ({ a, b }, w);
    };
    auto uOfBeat = [&] (double b) { return (float) ((b - pview.start) / pview.beats); };

    if (d < 0.01f)
    {
        g.saveState();
        g.reduceClipRegion (field.toNearestInt());

        // semitone rows: like a piano roll (black-key rows darker), scale tones tinted
        const float rowH = semitoneHeight();
        for (int n = (int) std::floor (pview.lo) - 1; n <= (int) std::ceil (pview.hi) + 1; ++n)
        {
            const float y0 = std::round (pitchToY ((float) n + 0.5f)), y1 = std::round (pitchToY ((float) n - 0.5f));
            auto row = Rectangle<float> (field.getX(), y0, field.getWidth(), y1 - y0);
            g.setColour (isBlackKey (n) ? Colour (0xff0d0f14) : Colour (0xff13161d));
            g.fillRect (row);
            if (scaleLockOn && scaleMask[(size_t) (((n % 12) + 12) % 12)])
            {
                g.setColour (pal::cyan.withAlpha (0.045f));
                g.fillRect (row);
            }
            if (rowH > 6.0f || n % 12 == 0)
            {
                g.setColour (n % 12 == 0 ? pal::gridStrong : pal::gridLine);
                g.fillRect (Rectangle<float> (field.getX(), y1 - 1.0f, field.getWidth(), 1.0f));
            }
        }

        // grid / beat / bar lines (crisp 1 px)
        const double pxPerBeat = field.getWidth() / pview.beats;
        double step = gridBeats > 0.0 ? gridBeats : 1.0;
        while (step * pxPerBeat < 8.0) step *= 2.0;
        const double first = std::floor (pview.start / step) * step;
        for (double b = first; b <= pview.start + pview.beats + 1.0e-6; b += step)
        {
            const float x = std::round (beatToX (b));
            const bool isBar  = std::abs (std::fmod (b + 1.0e-6, (double) bpb)) < 2.0e-6;
            const bool isBeat = std::abs (b - std::round (b)) < 1.0e-6;
            g.setColour (isBar ? Colour (0xff343b4b) : (isBeat ? pal::gridStrong : pal::gridLine));
            g.fillRect (Rectangle<float> (x, field.getY(), 1.0f, field.getHeight()));
        }

        // beyond the end of the loop
        if (L < pview.start + pview.beats)
        {
            const float x = std::round (beatToX (L));
            g.setColour (Colours::black.withAlpha (0.55f));
            g.fillRect (Rectangle<float> (x, field.getY(), field.getRight() - x, field.getHeight()));
            g.setColour (pal::magenta.withAlpha (0.6f));
            g.fillRect (Rectangle<float> (x, field.getY(), 1.0f, field.getHeight()));
        }

        g.restoreState();
        return;
    }

    // ---- DIMENSION: the same field tilted (bars + octaves on the floor / back plane)
    Path plane;
    plane.startNewSubPath (project (0, 0, 0));
    plane.lineTo (project (1, 0, 0));
    plane.lineTo (project (1, 0, 1));
    plane.lineTo (project (0, 0, 1));
    plane.closeSubPath();
    g.setColour (pal::violet.withAlpha (0.05f * d));
    g.fillPath (plane);
    g.setColour (pal::violet.withAlpha (0.18f * d));
    g.strokePath (plane, PathStrokeType (1.0f));

    for (double b = std::ceil (pview.start); b <= pview.start + pview.beats + 1.0e-6; b += 1.0)
    {
        const float u = uOfBeat (b);
        const bool bar = std::abs (std::fmod (b + 1.0e-6, (double) bpb)) < 2.0e-6;
        line (project (u, 0, 0), project (u, 1, 0), (bar ? pal::violet : pal::textDim).withAlpha (bar ? 0.22f : 0.06f), bar ? 1.0f : 0.7f);
        if (bar)
        {
            line (project (u, 0, 0), project (u, 0, 1), pal::violet.withAlpha (0.12f * d), 0.8f);
            g.setColour (pal::textFaint);
            g.setFont (fonts::ui (jmax (10.0f, 11.5f * s)));
            const auto p = project (u, 1, 0);
            g.drawText (String ((int) std::lround (b / bpb) + 1), Rectangle<float> (p.x + 4, p.y - 2, 40, 12), Justification::left);
        }
    }

    for (int n = (int) std::ceil (pview.lo / 12.0f) * 12; n <= (int) pview.hi; n += 12)
    {
        const float v = vOf ((float) n);
        line (project (0, v, 0), project (1, v, 0), pal::cyan.withAlpha (0.07f), 0.8f);
        g.setColour (pal::textFaint);
        g.setFont (fonts::ui (jmax (10.0f, 11.5f * s)));
        const auto p = project (0, v, 0);
        g.drawText (noteName (n), Rectangle<float> (p.x - 32, p.y - 6, 28, 12), Justification::right);
    }
}

void VoiceField::drawLine (juce::Graphics& g, const PatternLine& l, int index, bool selected, bool playing)
{
    using namespace juce;
    const double L = store != nullptr ? store->get().lengthBeats() : 16.0;
    const float uiScale = jmax (0.6f, field.getHeight() / 480.0f);
    const float z = dimension > 0.001f ? (float) (index % 15) / 14.0f : 0.0f;

    const float u0 = (float) ((l.start - pview.start) / pview.beats);
    const float u1 = (float) ((l.end() - pview.start) / pview.beats);
    if (u1 < -0.02f || u0 > 1.02f) return;

    const float pxWidth = (u1 - u0) * field.getWidth();
    const int n = jlimit (6, 700, (int) (pxWidth / 2.5f));

    std::vector<SP> pts;
    pts.reserve ((size_t) n + 1);
    const float alpha = 1.0f;
    for (int i = 0; i <= n; ++i)
    {
        const double t = l.length * (double) i / (double) n;
        const double beat = l.start + t;
        const float u = (float) ((beat - pview.start) / pview.beats);
        if (u < -0.05f || u > 1.05f) continue;
        const float v = vOf (l.pitchAt (t));
        const float pressure = l.pressureAt (t);
        const float half = (1.0f + pressure * 8.0f) * (0.75f + 0.35f * l.velocity) * uiScale * depthScale (v, z);
        pts.push_back ({ project (u, v, z), half, l.timbreAt (t), alpha, (float) (beat / jmax (1.0, L)) });
    }

    if (pts.size() < 2) return;

    const float emphasis = selected ? 1.6f : (playing ? 1.3f : 1.0f);
    drawRibbon (g, pts, 0, pts.size(), false, (float) (l.id % 97) / 97.0f, emphasis, false);

    if (selected)
    {
        Path core;
        core.startNewSubPath (pts.front().p);
        for (size_t i = 1; i < pts.size(); ++i) core.lineTo (pts[i].p);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.strokePath (core, PathStrokeType (1.3f * uiScale, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // the nodes of the line (where its pitch changes direction) - like a piano-roll polyline
    if (dimension < 0.01f && l.pitch.size() > 1)
    {
        float lastX = -1.0e9f;
        for (size_t i = 1; i < l.pitch.size(); ++i)
        {
            const Point<float> q (beatToX (l.start + l.pitch[i].t), pitchToY (l.pitch[i].v));
            if (q.x - lastX < 7.0f * uiScale) continue;
            lastX = q.x;
            g.setColour (pal::ground0);
            g.fillEllipse (Rectangle<float> (6.0f * uiScale, 6.0f * uiScale).withCentre (q));
            g.setColour (pal::text.withAlpha (selected ? 0.0f : 0.75f));
            g.drawEllipse (Rectangle<float> (6.0f * uiScale, 6.0f * uiScale).withCentre (q), 1.0f);
        }
    }

    // onset node + end cap
    const auto a = pts.front().p, b = pts.back().p;
    g.setColour (pal::lime.withAlpha (0.22f));
    g.fillEllipse (Rectangle<float> (13.0f * uiScale, 13.0f * uiScale).withCentre (a));
    g.setColour (pal::lime);
    g.fillEllipse (Rectangle<float> (5.5f * uiScale, 5.5f * uiScale).withCentre (a));
    g.setColour (pal::violet.withAlpha (0.8f));
    g.drawEllipse (Rectangle<float> (5.5f * uiScale, 5.5f * uiScale).withCentre (b), 1.1f);
}

void VoiceField::drawPatternLines (juce::Graphics& g)
{
    if (store == nullptr) return;
    const auto& p = store->get();

    // The ribbons are cached; the cache is rebuilt when the pattern, the selection, the view or
    // the size changes (every frame only while DIMENSION animates).
    const juce::String key = juce::String ((int) store->getVersion()) + "|" + juce::String (pview.start, 4) + "|" + juce::String (pview.beats, 4)
                           + "|" + juce::String (pview.lo, 3) + "|" + juce::String (pview.hi, 3) + "|" + juce::String (dimension, 4)
                           + "|" + juce::String (getWidth()) + "x" + juce::String (getHeight());

    if (key != patternLayerKey || ! patternLayer.isValid())
    {
        patternLayerKey = key;
        if (getWidth() <= 0 || getHeight() <= 0) return;
        patternLayer = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics lg (patternLayer);
        if (dimension < 0.01f) lg.reduceClipRegion (field.toNearestInt().expanded (2));

        std::vector<int> order ((size_t) p.lines.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = (int) i;
        if (dimension > 0.01f)
            std::stable_sort (order.begin(), order.end(), [] (int a, int b) { return (a % 15) > (b % 15); });
        else
            std::stable_sort (order.begin(), order.end(), [&] (int a, int b)   // selected on top
            {
                return (int) store->isSelected (p.lines[(size_t) a].id) < (int) store->isSelected (p.lines[(size_t) b].id);
            });

        for (int i : order)
        {
            const auto& l = p.lines[(size_t) i];
            drawLine (lg, l, i, store->isSelected (l.id), false);
        }
    }

    g.drawImageAt (patternLayer, 0, 0);
}

void VoiceField::drawActiveHeads (juce::Graphics& g)
{
    using namespace juce;
    if (store == nullptr || view.playheadBeat < 0.0) return;
    const float uiScale = jmax (0.6f, field.getHeight() / 480.0f);
    const auto& p = store->get();
    const double ph = view.playheadBeat;

    for (size_t i = 0; i < p.lines.size(); ++i)
    {
        const auto& l = p.lines[i];
        if (ph < l.start || ph >= l.end()) continue;
        const float u = (float) ((ph - pview.start) / pview.beats);
        if (u < 0.0f || u > 1.0f) continue;
        const double t = ph - l.start;
        const float z = dimension > 0.001f ? (float) (i % 15) / 14.0f : 0.0f;
        const auto q = project (u, vOf (l.pitchAt (t)), z);
        if (dimension < 0.01f && ! field.contains (q)) continue;

        const float pulse = 0.5f + 0.5f * std::sin (time * 7.0f + (float) i);
        const float pres = l.pressureAt (t);
        const auto c = pal::ribbon (jlimit (0.0f, 1.0f, 0.55f * l.timbreAt (t) + 0.45f * (float) (ph / jmax (1.0, p.lengthBeats()))));
        g.setColour (c.withAlpha (0.18f + 0.12f * pulse));
        g.fillEllipse (Rectangle<float> ((22.0f + 10.0f * pres + 6.0f * pulse) * uiScale, (22.0f + 10.0f * pres + 6.0f * pulse) * uiScale).withCentre (q));
        g.setColour (c.brighter (0.5f).withAlpha (0.6f));
        g.fillEllipse (Rectangle<float> (9.0f * uiScale, 9.0f * uiScale).withCentre (q));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (4.5f * uiScale, 4.5f * uiScale).withCentre (q));
    }
}

void VoiceField::drawKeysAndRuler (juce::Graphics& g)
{
    using namespace juce;
    const float s = getUiScale();
    const float alpha = 1.0f - jlimit (0.0f, 1.0f, dimension * 2.0f);
    if (alpha <= 0.01f) return;

    // --- piano keys
    auto keys = Rectangle<float> (0.0f, field.getY(), field.getX() - 3.0f * s, field.getHeight());
    g.saveState();
    g.reduceClipRegion (keys.toNearestInt());
    g.setColour (Colour (0xff0d0f14).withAlpha (alpha));
    g.fillRect (keys);

    // keys that are sounding right now (lines under the playhead)
    std::array<bool, 128> sounding {};
    if (store != nullptr && view.playheadBeat >= 0.0)
        for (auto& l : store->get().lines)
            if (view.playheadBeat >= l.start && view.playheadBeat < l.end())
                sounding[(size_t) jlimit (0, 127, (int) std::lround (l.pitchAt (view.playheadBeat - l.start)))] = true;

    const float rowH = semitoneHeight();
    for (int n = (int) std::floor (pview.lo) - 1; n <= (int) std::ceil (pview.hi) + 1; ++n)
    {
        const float y0 = pitchToY ((float) n + 0.5f), y1 = pitchToY ((float) n - 0.5f);
        auto r = Rectangle<float> (keys.getX(), std::round (y0) + 1.0f, keys.getWidth() - 3.0f, std::round (y1) - std::round (y0) - 1.0f);
        const bool black = isBlackKey (n);
        const bool on = n >= 0 && n < 128 && sounding[(size_t) n];
        g.setColour ((on ? pal::cyan : (black ? Colour (0xff1a1e26) : Colour (0xffc9ced8))).withAlpha (alpha));
        g.fillRect (r.withTrimmedRight (black ? r.getWidth() * 0.38f : 0.0f));

        if (scaleLockOn && scaleMask[(size_t) (((n % 12) + 12) % 12)])
        {
            g.setColour (pal::cyan.withAlpha (alpha));
            g.fillRect (Rectangle<float> (r.getRight() + 1.0f, r.getY(), 2.0f, r.getHeight()));
        }

        if (n % 12 == 0 || (rowH > 15.0f && ! black))
        {
            g.setColour (Colour (0xff1d212a).withAlpha (alpha));
            g.setFont (fonts::ui (jlimit (9.0f, 12.0f, jmin (rowH * 0.85f, 11.5f * s)), n % 12 == 0));
            g.drawText (noteName (n), r.reduced (3.0f * s, 0.0f), Justification::centredRight);
        }
    }
    g.restoreState();

    // --- bar ruler
    auto ruler = Rectangle<float> (field.getX(), 0.0f, field.getWidth(), field.getY() - 2.0f);
    g.setColour (pal::ground1.withAlpha (alpha));
    g.fillRect (ruler.withLeft (0.0f));

    const double L = store != nullptr ? store->get().lengthBeats() : 16.0;
    const int bpb = jmax (1, view.beatsPerBar);
    g.saveState();
    g.reduceClipRegion (ruler.toNearestInt());
    for (double b = std::ceil (pview.start); b <= pview.start + pview.beats + 1.0e-6; b += 1.0)
    {
        const float x = beatToX (b);
        const bool bar = std::abs (std::fmod (b + 1.0e-6, (double) bpb)) < 2.0e-6;
        g.setColour ((bar ? pal::textDim : pal::textFaint).withAlpha (alpha));
        const float h = bar ? ruler.getHeight() * 0.55f : ruler.getHeight() * 0.25f;
        g.fillRect (Rectangle<float> (x - 0.5f, ruler.getBottom() - h, 1.0f, h));
        if (bar && b < L - 1.0e-6)
        {
            g.setColour (pal::text.withAlpha (0.9f * alpha));
            g.setFont (fonts::ui (jlimit (10.0f, 13.0f, 12.5f * s), true));
            g.drawText (String ((int) std::lround (b / bpb) + 1), Rectangle<float> (x + 4.0f * s, ruler.getY(), 30.0f * s, ruler.getHeight()),
                        Justification::centredLeft);
        }
    }

    // pattern end marker + playhead triangle
    {
        const float x = beatToX (L);
        g.setColour (pal::magenta.withAlpha (0.8f * alpha));
        Path tri;
        tri.addTriangle (x, ruler.getBottom(), x - 5.0f * s, ruler.getY() + 2.0f, x, ruler.getY() + 2.0f);
        g.fillPath (tri);
    }
    if (view.playheadBeat >= 0.0)
    {
        const float x = beatToX (view.playheadBeat);
        Path tri;
        tri.addTriangle (x - 5.0f * s, ruler.getY() + 2.0f, x + 5.0f * s, ruler.getY() + 2.0f, x, ruler.getBottom());
        g.setColour (pal::cyan.withAlpha (alpha));
        g.fillPath (tri);
    }
    g.restoreState();

    g.setColour (pal::hairline.withAlpha (alpha));
    g.fillRect (Rectangle<float> (0.0f, std::round (ruler.getBottom()), (float) getWidth(), 1.0f));
}

} // namespace bd
