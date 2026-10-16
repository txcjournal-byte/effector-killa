#include "VoiceField.h"
#include "Palette.h"
#include "Fonts.h"
#include "LookAndFeel.h"
#include <cmath>

namespace bd
{

namespace
{
    constexpr size_t kMaxStrokes      = 600;
    constexpr size_t kMaxPtsPerStroke = 4000;
    constexpr float  kBloomSeconds    = 0.9f;
}

VoiceField::VoiceField (VisualFifo& f) : fifo (f)
{
    std::fill (std::begin (activeStroke), std::end (activeStroke), -1);
    strokes.reserve (kMaxStrokes);
    setOpaque (true);
    setTooltip ("VOICE FIELD - click to change how many bars are shown");
}

//==============================================================================
// Data

double VoiceField::beatOf (int64_t t) const noexcept
{
    // Map an engine-clock sample time to beats using the current view anchor.
    return view.beatNow + ((double) t - view.nowClock) / view.sampleRate * view.bpm / 60.0;
}

void VoiceField::updateView()
{
    const auto tv = fifo.getTransportNow();
    const bool pageMode = tv.hasPpq && tv.playing;

    if (pageMode != view.pageMode)
    {
        // Switching between "host transport" and "free running" time bases: start a fresh field.
        strokes.clear();
        std::fill (std::begin (activeStroke), std::end (activeStroke), -1);
    }

    view.pageMode   = pageMode;
    view.nowClock   = tv.clock;
    view.sampleRate = tv.sampleRate;
    view.bpm        = tv.bpm;
    view.beatsPerBar = juce::jmax (1, (int) std::lround (tv.num * 4.0 / (double) tv.den));
    view.viewBeats  = (double) numBars * (double) tv.num * 4.0 / (double) tv.den;

    if (pageMode)
    {
        view.beatNow      = tv.ppq;
        view.viewStart    = std::floor (tv.ppq / view.viewBeats) * view.viewBeats;
        view.playheadBeat = tv.ppq;
    }
    else
    {
        view.beatNow      = tv.clock / tv.sampleRate * tv.bpm / 60.0;
        view.viewStart    = view.beatNow - view.viewBeats;
        view.playheadBeat = view.beatNow;
    }
}

void VoiceField::consume (const VisualEvent& e)
{
    const int ch = juce::jlimit (0, 16, (int) e.channel);
    const double beat = beatOf (e.time);

    auto endStroke = [this] (int channel, double b)
    {
        const int idx = activeStroke[channel];
        if (idx >= 0 && idx < (int) strokes.size())
            strokes[(size_t) idx].endBeat = b;
        activeStroke[channel] = -1;
    };

    auto current = [this, ch]() -> Stroke*
    {
        const int idx = activeStroke[ch];
        return (idx >= 0 && idx < (int) strokes.size()) ? &strokes[(size_t) idx] : nullptr;
    };

    switch (e.type)
    {
        case VisualEvent::noteOn:
        {
            endStroke (ch, beat);
            if (strokes.size() >= kMaxStrokes)
                prune();
            if (strokes.size() >= kMaxStrokes)
                strokes.erase (strokes.begin());

            Stroke s;
            s.channel   = ch;
            s.note      = e.note;
            s.velocity  = e.velocity;
            s.startBeat = beat;
            s.createdAt = time;
            s.seed      = juce::Random::getSystemRandom().nextFloat();
            s.pts.reserve (256);
            s.pts.push_back ({ beat, e.pitch, e.pressure, e.timbre });
            s.nodes.push_back ({ beat, e.pitch, 0 });
            strokes.push_back (std::move (s));

            // indices may have shifted after an erase
            std::fill (std::begin (activeStroke), std::end (activeStroke), -1);
            for (int i = 0; i < (int) strokes.size(); ++i)
                if (strokes[(size_t) i].endBeat < 0.0)
                    activeStroke[strokes[(size_t) i].channel] = i;
            break;
        }

        case VisualEvent::update:
            if (auto* s = current())
                if (s->pts.size() < kMaxPtsPerStroke)
                    s->pts.push_back ({ beat, e.pitch, e.pressure, e.timbre });
            break;

        case VisualEvent::glide:
            if (auto* s = current())
            {
                const float from = s->pts.empty() ? e.pitch : s->pts.back().pitch;
                s->nodes.push_back ({ beat, from, 1 });
                s->createdAt = time;   // re-bloom: a new key was played on this voice
                s->nodes.push_back ({ beat, e.pitch, 3 });   // ghost target marker
            }
            break;

        case VisualEvent::keyUp:
            if (auto* s = current())
                s->nodes.push_back ({ beat, s->pts.empty() ? e.pitch : s->pts.back().pitch, 2 });
            break;

        case VisualEvent::noteOff:
            if (auto* s = current())
                s->pts.push_back ({ beat, e.pitch, 0.0f, e.timbre });
            endStroke (ch, beat);
            break;

        case VisualEvent::allOff:
            for (int c = 0; c <= 16; ++c)
                endStroke (c, beat);
            break;

        default: break;
    }
}

void VoiceField::prune()
{
    // Remove strokes that ended before the visible window (plus one page of ghosts in page mode),
    // and strokes "from the future" after the host jumped backwards (loop / rewind).
    const double oldest = view.viewStart - (view.pageMode ? view.viewBeats : 0.25 * view.viewBeats);
    const double newest = view.beatNow + 0.5;

    std::vector<Stroke> kept;
    kept.reserve (strokes.size());

    for (auto& s : strokes)
    {
        const bool active = s.endBeat < 0.0;
        const bool tooOld = ! active && s.endBeat < oldest;
        const bool future = s.startBeat > newest;
        if (! tooOld && ! future)
            kept.push_back (std::move (s));
    }

    strokes.swap (kept);
    std::fill (std::begin (activeStroke), std::end (activeStroke), -1);
    for (int i = 0; i < (int) strokes.size(); ++i)
        if (strokes[(size_t) i].endBeat < 0.0)
            activeStroke[strokes[(size_t) i].channel] = i;
}

void VoiceField::tick (float t)
{
    time = t;
    updateView();
    fifo.drain ([this] (const VisualEvent& e) { consume (e); });
    prune();

    // DIMENSION transition
    dimension += (dimensionTarget - dimension) * 0.12f;
    if (std::abs (dimension - dimensionTarget) < 0.001f)
        dimension = dimensionTarget;
    updateProjectionFit();

    // auto-fit pitch range to what is visible (min 2 octaves), smoothed
    float lo = 1000.0f, hi = -1000.0f;
    for (auto& s : strokes)
        for (auto& p : s.pts)
        {
            lo = juce::jmin (lo, p.pitch);
            hi = juce::jmax (hi, p.pitch);
        }

    if (lo > hi) { lo = 48.0f; hi = 72.0f; }
    const float mid = 0.5f * (lo + hi);
    const float span = juce::jmax (24.0f, hi - lo + 10.0f);
    pitchLoSmoothed += (mid - span * 0.5f - pitchLoSmoothed) * 0.06f;
    pitchHiSmoothed += (mid + span * 0.5f - pitchHiSmoothed) * 0.06f;
    view.pitchLo = pitchLoSmoothed;
    view.pitchHi = pitchHiSmoothed;

    repaint();
}

//==============================================================================
// Projection

float VoiceField::uOf (double beat, float& alpha) const noexcept
{
    float u = (float) ((beat - view.viewStart) / view.viewBeats);
    alpha = 1.0f;

    if (view.pageMode)
    {
        const float pu = (float) ((view.playheadBeat - view.viewStart) / view.viewBeats);
        if (u < 0.0f)
        {
            // previous page: shown as a fading ghost ahead of the playhead (radar sweep)
            u += 1.0f;
            const float ahead = (u - pu) / 0.25f;
            alpha = 0.35f * juce::jlimit (0.0f, 1.0f, ahead);
            if (u < pu) alpha = 0.0f;
        }
    }
    else
    {
        alpha = juce::jlimit (0.0f, 1.0f, u * 5.0f);   // fade out at the left edge
    }

    return u;
}

juce::Point<float> VoiceField::projectRaw (float u, float v, float z) const noexcept
{
    // Flat: plain orthographic. DIMENSION: rotate the field around X and Y, then perspective-divide.
    const float d = dimension;
    const float X = u - 0.5f;
    const float Y = v - 0.5f;
    const float Z = (z - 0.5f) * 0.9f * d;

    const float ax = -0.55f * d;           // tilt the top back
    const float ay =  0.30f * d;           // turn to the side
    const float x1 =  X * std::cos (ay) + Z * std::sin (ay);
    const float z1 = -X * std::sin (ay) + Z * std::cos (ay);
    const float y2 =  Y * std::cos (ax) - z1 * std::sin (ax);
    const float z2 =  Y * std::sin (ax) + z1 * std::cos (ax);

    const float camera = 2.2f;
    const float s = camera / (camera + z2 + 0.0001f);
    return { x1 * s, -y2 * s };            // y down, like the screen
}

void VoiceField::updateProjectionFit()
{
    // Fit the projected unit cube (all 8 corners) into the field rectangle.
    float x0 = 1.0e9f, y0 = 1.0e9f, x1 = -1.0e9f, y1 = -1.0e9f;
    for (int i = 0; i < 8; ++i)
    {
        const auto p = projectRaw ((float) (i & 1), (float) ((i >> 1) & 1), (float) ((i >> 2) & 1));
        x0 = juce::jmin (x0, p.x); x1 = juce::jmax (x1, p.x);
        y0 = juce::jmin (y0, p.y); y1 = juce::jmax (y1, p.y);
    }
    rawBounds = { x0, y0, juce::jmax (0.001f, x1 - x0), juce::jmax (0.001f, y1 - y0) };
}

juce::Point<float> VoiceField::project (float u, float v, float z) const noexcept
{
    const auto p = projectRaw (u, v, z);
    return { field.getX() + (p.x - rawBounds.getX()) / rawBounds.getWidth()  * field.getWidth(),
             field.getY() + (p.y - rawBounds.getY()) / rawBounds.getHeight() * field.getHeight() };
}

float VoiceField::depthScale (float v, float z) const noexcept
{
    if (dimension <= 0.001f) return 1.0f;
    const auto a = project (0.5f, v, z), b = project (0.5f, v + 0.01f, z);
    return juce::jlimit (0.3f, 1.6f, a.getDistanceFrom (b) / (0.01f * field.getHeight()));
}

//==============================================================================
// Painting

void VoiceField::resized()
{
    field = getLocalBounds().toFloat().reduced (getWidth() * 0.035f, getHeight() * 0.07f);
    updateProjectionFit();
    renderBackgroundImage();
}

void VoiceField::renderBackgroundImage()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;

    bgImage = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g (bgImage);
    drawBackground (g);
}

void VoiceField::drawBackground (juce::Graphics& g)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();

    ColourGradient radial (pal::ground1, b.getCentreX(), b.getCentreY() * 0.9f,
                           pal::ground0, b.getX(), b.getY(), true);
    g.setGradientFill (radial);
    g.fillAll();

    // faint coloured nebula washes
    ColourGradient c1 (pal::cyan.withAlpha (0.06f), b.getWidth() * 0.2f, b.getHeight() * 0.8f,
                       Colours::transparentBlack, b.getWidth() * 0.55f, b.getHeight() * 0.2f, true);
    g.setGradientFill (c1);
    g.fillAll();
    ColourGradient c2 (pal::magenta.withAlpha (0.05f), b.getWidth() * 0.85f, b.getHeight() * 0.25f,
                       Colours::transparentBlack, b.getWidth() * 0.45f, b.getHeight() * 0.9f, true);
    g.setGradientFill (c2);
    g.fillAll();

    // star dust
    Random rng (42);
    for (int i = 0; i < 160; ++i)
    {
        const float x = rng.nextFloat() * b.getWidth(), y = rng.nextFloat() * b.getHeight();
        const float r = 0.4f + rng.nextFloat() * 0.9f;
        g.setColour (pal::text.withAlpha (0.04f + rng.nextFloat() * 0.10f));
        g.fillEllipse (x, y, r, r);
    }

    // vignette
    ColourGradient vig (Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                        Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillAll();
}

void VoiceField::drawGrid (juce::Graphics& g)
{
    using namespace juce;

    auto line = [&] (Point<float> a, Point<float> b, Colour c, float w)
    {
        g.setColour (c);
        g.drawLine ({ a, b }, w);
    };

    // floor / back plane outline when tilted
    if (dimension > 0.01f)
    {
        Path plane;
        plane.startNewSubPath (project (0, 0, 0));
        plane.lineTo (project (1, 0, 0));
        plane.lineTo (project (1, 0, 1));
        plane.lineTo (project (0, 0, 1));
        plane.closeSubPath();
        g.setColour (pal::violet.withAlpha (0.05f * dimension));
        g.fillPath (plane);
        g.setColour (pal::violet.withAlpha (0.18f * dimension));
        g.strokePath (plane, PathStrokeType (1.0f));
    }

    // beat + bar lines
    const int totalBeats = (int) std::round (view.viewBeats);
    const int bpb = jmax (1, view.beatsPerBar);
    for (int i = 0; i <= totalBeats; ++i)
    {
        const float u = (float) i / (float) jmax (1, totalBeats);
        const bool bar = (i % bpb) == 0;
        line (project (u, 0, 0), project (u, 1, 0), (bar ? pal::violet : pal::textDim).withAlpha (bar ? 0.20f : 0.06f), bar ? 1.0f : 0.7f);

        if (dimension > 0.01f && bar)
            line (project (u, 0, 0), project (u, 0, 1), pal::violet.withAlpha (0.12f * dimension), 0.8f);

        if (bar && i < totalBeats)
        {
            g.setColour (pal::textFaint);
            g.setFont (fonts::mono (10.0f));
            const auto p = project (u, 1, 0);
            const int barNo = (int) std::floor (view.viewStart / view.viewBeats * numBars + 0.0001) + i / bpb + 1;
            g.drawText (view.pageMode ? String (barNo) : String (), Rectangle<float> (p.x + 4, p.y - 2, 40, 12), Justification::left);
        }
    }

    // octave lines (C notes)
    const int firstC = (int) std::ceil (view.pitchLo / 12.0f) * 12;
    for (int n = firstC; n <= (int) view.pitchHi; n += 12)
    {
        const float v = vOf ((float) n);
        line (project (0, v, 0), project (1, v, 0), pal::cyan.withAlpha (0.07f), 0.8f);
        g.setColour (pal::textFaint);
        g.setFont (fonts::mono (10.0f));
        const auto p = project (0, v, 0);
        g.drawText ("C" + String (n / 12 - 1), Rectangle<float> (p.x - 30, p.y - 6, 26, 12), Justification::right);
    }
}

void VoiceField::drawStroke (juce::Graphics& g, const Stroke& s)
{
    using namespace juce;
    if (s.pts.empty()) return;

    const float z = zOf (s.channel);
    const float uiScale = jmax (0.6f, field.getHeight() / 480.0f);
    const bool active = s.endBeat < 0.0;

    // Build screen-space polylines, splitting where the page wraps (u jumps)
    struct SP { Point<float> p; float half; float timbre; float alpha; float u; };
    std::vector<SP> pts;
    pts.reserve (s.pts.size() + 1);

    auto addPoint = [&] (double beat, float pitch, float pressure, float timbre)
    {
        float alpha;
        const float u = uOf (beat, alpha);
        if (u < -0.02f || u > 1.02f) return;
        const float v = vOf (pitch);
        const auto p = project (u, v, z);
        const float half = (1.2f + pressure * 9.0f) * uiScale * depthScale (v, z);
        pts.push_back ({ p, half, timbre, alpha, u });
    };

    float lastX = -1.0e9f;
    for (size_t i = 0; i < s.pts.size(); ++i)
    {
        const auto& p = s.pts[i];
        float a;
        const float u = uOf (p.beat, a);
        const float x = field.getX() + u * field.getWidth();
        if (i + 1 < s.pts.size() && std::abs (x - lastX) < 1.5f && dimension < 0.01f) continue;   // decimate
        lastX = x;
        addPoint (p.beat, p.pitch, p.pressure, p.timbre);
    }

    if (active)   // extend the head to "now"
    {
        const auto& last = s.pts.back();
        addPoint (view.playheadBeat, last.pitch, last.pressure, last.timbre);
    }

    if (pts.size() < 2)
    {
        if (pts.size() == 1)
        {
            g.setColour (pal::lime.withAlpha (pts[0].alpha));
            g.fillEllipse (Rectangle<float> (5, 5).withCentre (pts[0].p));
        }
        return;
    }

    // split into continuous segments
    size_t segStart = 0;
    for (size_t i = 1; i <= pts.size(); ++i)
    {
        const bool breakHere = i == pts.size() || std::abs (pts[i].u - pts[i - 1].u) > 0.5f;
        if (! breakHere) continue;

        const size_t a = segStart, b = i;   // [a, b)
        segStart = i;
        if (b - a < 2) continue;

        float alpha = 0.0f;
        for (size_t k = a; k < b; ++k) alpha = jmax (alpha, pts[k].alpha);
        if (alpha <= 0.01f) continue;

        // ribbon polygon
        Path ribbon, core;
        auto normalAt = [&] (size_t k)
        {
            const auto p0 = pts[k > a ? k - 1 : k].p, p1 = pts[k + 1 < b ? k + 1 : k].p;
            auto d = p1 - p0;
            const float len = jmax (0.001f, d.getDistanceFromOrigin());
            return Point<float> (-d.y / len, d.x / len);
        };

        for (size_t k = a; k < b; ++k)
        {
            const auto n = normalAt (k) * pts[k].half;
            if (k == a) ribbon.startNewSubPath (pts[k].p + n); else ribbon.lineTo (pts[k].p + n);
            if (k == a) core.startNewSubPath (pts[k].p);       else core.lineTo (pts[k].p);
        }
        for (size_t k = b; k-- > a;)
            ribbon.lineTo (pts[k].p - normalAt (k) * pts[k].half);
        ribbon.closeSubPath();

        // colour: timbre + position in time
        const auto& first = pts[a];
        const auto& last  = pts[b - 1];
        const float tStart = jlimit (0.0f, 1.0f, 0.55f * first.timbre + 0.45f * first.u);
        const float tEnd   = jlimit (0.0f, 1.0f, 0.55f * last.timbre  + 0.45f * last.u);
        const auto cA = pal::ribbon (tStart), cB = pal::ribbon (tEnd);

        ColourGradient fill (cA.withAlpha (0.20f * alpha), first.p, cB.withAlpha (0.30f * alpha), last.p, false);
        g.setGradientFill (fill);
        g.fillPath (ribbon);

        ColourGradient edge (cA.withAlpha (0.35f * alpha), first.p, cB.withAlpha (0.45f * alpha), last.p, false);
        g.setGradientFill (edge);
        g.strokePath (ribbon, PathStrokeType (0.8f));

        // glow + bright core line (actual pitch incl. bends)
        for (int layer = 3; layer >= 1; --layer)
        {
            ColourGradient glow (cA.withAlpha (0.07f * alpha), first.p, cB.withAlpha (0.09f * alpha), last.p, false);
            g.setGradientFill (glow);
            g.strokePath (core, PathStrokeType (1.5f + (float) layer * 3.2f * uiScale, PathStrokeType::curved, PathStrokeType::rounded));
        }

        ColourGradient coreCol (cA.brighter (0.6f).withAlpha (0.95f * alpha), first.p,
                                cB.brighter (0.6f).withAlpha (0.95f * alpha), last.p, false);
        g.setGradientFill (coreCol);
        g.strokePath (core, PathStrokeType (1.4f * uiScale, PathStrokeType::curved, PathStrokeType::rounded));

        // sparks travelling along live voices
        if (active && b == pts.size())
        {
            const float len = core.getLength();
            if (len > 10.0f)
                for (int k = 0; k < 4; ++k)
                {
                    const float f = std::fmod (time * 0.35f + s.seed + (float) k * 0.25f, 1.0f);
                    const auto p = core.getPointAlongPath (len * f);
                    const float tw = 0.5f + 0.5f * std::sin (time * 9.0f + (float) k * 2.1f + s.seed * 10.0f);
                    g.setColour (Colours::white.withAlpha (0.18f * tw * alpha));
                    g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (p));
                    g.setColour (pal::text.withAlpha ((0.5f + 0.5f * tw) * alpha));
                    g.fillEllipse (Rectangle<float> (2.6f, 2.6f).withCentre (p));
                }
        }

        // head
        if (active && b == pts.size())
        {
            const auto p = pts[b - 1].p;
            const float pulse = 0.5f + 0.5f * std::sin (time * 6.0f + s.seed * 6.0f);
            g.setColour (cB.withAlpha (0.25f));
            g.fillEllipse (Rectangle<float> (18.0f + 6.0f * pulse, 18.0f + 6.0f * pulse).withCentre (p));
            g.setColour (Colours::white);
            g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (p));
        }
    }

    // nodes
    for (auto& n : s.nodes)
    {
        float alpha;
        const float u = uOf (n.beat, alpha);
        if (u < 0.0f || u > 1.0f || alpha <= 0.01f) continue;
        const auto p = project (u, vOf (n.pitch), z);

        switch (n.kind)
        {
            case 0:   // onset
                g.setColour (pal::lime.withAlpha (0.22f * alpha));
                g.fillEllipse (Rectangle<float> (16, 16).withCentre (p));
                g.setColour (pal::lime.withAlpha (alpha));
                g.fillEllipse (Rectangle<float> (6.5f, 6.5f).withCentre (p));
                break;
            case 1:   // glide start / bend point
                g.setColour (pal::amber.withAlpha (0.25f * alpha));
                g.fillEllipse (Rectangle<float> (14, 14).withCentre (p));
                g.setColour (pal::amber.withAlpha (alpha));
                g.fillEllipse (Rectangle<float> (5.5f, 5.5f).withCentre (p));
                break;
            case 2:   // key up (dive / release starts)
                g.setColour (pal::violet.withAlpha (0.8f * alpha));
                g.drawEllipse (Rectangle<float> (7, 7).withCentre (p), 1.3f);
                break;
            case 3:   // glide target ghost
                g.setColour (pal::amber.withAlpha (0.35f * alpha));
                g.drawEllipse (Rectangle<float> (8, 8).withCentre (p), 1.0f);
                break;
            default: break;
        }
    }

    // onset bloom / ripple
    const float age = time - s.createdAt;
    if (age >= 0.0f && age < kBloomSeconds)
    {
        const float k = age / kBloomSeconds;
        const auto& origin = s.nodes.empty() ? Node { s.startBeat, (float) s.note, 0 } : s.nodes.back();
        float alpha;
        const float u = uOf (origin.beat, alpha);
        const auto p = project (u, vOf (origin.pitch), z);
        const float r = (8.0f + 70.0f * applyCurve (Curve::ease, k)) * uiScale;
        g.setColour (pal::lime.withAlpha ((1.0f - k) * 0.55f));
        g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), 2.0f * (1.0f - k) + 0.5f);
        g.setColour (pal::cyan.withAlpha ((1.0f - k) * 0.10f));
        g.fillEllipse (Rectangle<float> (r * 1.2f, r * 1.2f).withCentre (p));
    }
}

void VoiceField::drawPlayhead (juce::Graphics& g)
{
    using namespace juce;
    float alpha;
    const float u = jlimit (0.0f, 1.0f, uOf (view.playheadBeat, alpha));

    const auto bottom = project (u, -0.02f, 0.0f), top = project (u, 1.02f, 0.0f);
    const float w = field.getWidth() * 0.05f;

    // trailing light (behind the beam)
    {
        Path trail;
        const float u0 = jmax (0.0f, u - 0.06f);
        trail.startNewSubPath (project (u0, -0.02f, 0));
        trail.lineTo (bottom);
        trail.lineTo (top);
        trail.lineTo (project (u0, 1.02f, 0));
        trail.closeSubPath();
        ColourGradient grad (pal::cyan.withAlpha (0.0f), project (u0, 0.5f, 0), pal::cyan.withAlpha (0.10f), project (u, 0.5f, 0), false);
        g.setGradientFill (grad);
        g.fillPath (trail);
    }

    // beam plane through depth when tilted
    if (dimension > 0.01f)
    {
        Path plane;
        plane.startNewSubPath (bottom);
        plane.lineTo (top);
        plane.lineTo (project (u, 1.02f, 1.0f));
        plane.lineTo (project (u, -0.02f, 1.0f));
        plane.closeSubPath();
        g.setColour (pal::cyan.withAlpha (0.06f * dimension));
        g.fillPath (plane);
    }

    // soft beam
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (pal::cyan.withAlpha (0.035f));
        g.drawLine ({ bottom, top }, w * (float) i / 4.0f);
    }

    ColourGradient core (pal::cyan.withAlpha (0.0f), top, pal::text.withAlpha (0.95f), bottom, false);
    core.addColour (0.5, pal::cyan.withAlpha (0.9f));
    g.setGradientFill (core);
    g.drawLine ({ bottom, top }, 1.6f);

    // base flare
    g.setColour (pal::cyan.withAlpha (0.25f));
    g.fillEllipse (Rectangle<float> (w * 0.9f, 8.0f).withCentre (bottom));
}

void VoiceField::paint (juce::Graphics& g)
{
    if (bgImage.isValid())
        g.drawImageAt (bgImage, 0, 0);
    else
        g.fillAll (pal::ground0);

    drawGrid (g);

    // draw far voices first in DIMENSION mode
    std::vector<const Stroke*> order;
    order.reserve (strokes.size());
    for (auto& s : strokes) order.push_back (&s);
    if (dimension > 0.01f)
        std::stable_sort (order.begin(), order.end(), [] (auto* a, auto* b) { return a->channel > b->channel; });

    for (auto* s : order)
        drawStroke (g, *s);

    drawPlayhead (g);

    // HUD
    g.setColour (pal::textDim);
    g.setFont (fonts::mono (11.0f, true));
    g.drawText ("VOICE FIELD", getLocalBounds().reduced (14, 8), juce::Justification::topLeft);
    g.setFont (fonts::mono (11.0f));
    g.setColour (pal::textFaint);
    g.drawText (juce::String (numBars) + (numBars == 1 ? " BAR" : " BARS") + juce::String (juce::CharPointer_UTF8 (view.pageMode ? "  \xc2\xb7  SYNCED" : "  \xc2\xb7  FREE")),
                getLocalBounds().reduced (14, 8), juce::Justification::topRight);

    if (strokes.empty() && hint.isNotEmpty())
    {
        g.setColour (pal::textDim.withAlpha (0.8f));
        g.setFont (fonts::mono (13.0f));
        g.drawFittedText (hint, getLocalBounds().reduced (40), juce::Justification::centred, 3);
    }
}

void VoiceField::mouseDown (const juce::MouseEvent&)
{
    static constexpr int options[] = { 1, 2, 4, 8 };
    int next = 4;
    for (size_t i = 0; i < 4; ++i)
        if (options[i] == numBars) { next = options[(i + 1) % 4]; break; }
    setBars (next);
    prune();
}

} // namespace bd
