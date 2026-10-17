#include "Pattern.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bd
{

//==============================================================================
namespace curves
{

namespace
{
    /** Index k so that c[k].t <= t < c[k+1].t (c.size() >= 2, t inside the range). */
    size_t segmentFor (const Curve1D& c, double t) noexcept
    {
        auto it = std::upper_bound (c.begin(), c.end(), t, [] (double x, const CurvePoint& p) { return x < p.t; });
        const auto idx = (size_t) std::distance (c.begin(), it);
        return juce::jlimit ((size_t) 0, c.size() - 2, idx == 0 ? (size_t) 0 : idx - 1);
    }

    double slope (const Curve1D& c, size_t k) noexcept
    {
        const double h = c[k + 1].t - c[k].t;
        return h > 1.0e-12 ? ((double) c[k + 1].v - (double) c[k].v) / h : 0.0;
    }

    /** Steffen (1990) tangent: monotone, local, no overshoot. Ends are flat (ease in / out). */
    double tangent (const Curve1D& c, size_t i) noexcept
    {
        if (i == 0 || i + 1 >= c.size())
            return 0.0;

        const double h0 = c[i].t - c[i - 1].t, h1 = c[i + 1].t - c[i].t;
        const double d0 = slope (c, i - 1), d1 = slope (c, i);

        if (d0 * d1 <= 0.0 || h0 + h1 <= 1.0e-12)
            return 0.0;

        const double p = (d0 * h1 + d1 * h0) / (h0 + h1);
        const double sgn = d0 > 0.0 ? 1.0 : -1.0;
        return 2.0 * sgn * std::min ({ std::abs (d0), std::abs (d1), 0.5 * std::abs (p) });
    }
}

float evalLinear (const Curve1D& c, double t) noexcept
{
    if (c.empty())            return 0.0f;
    if (t <= c.front().t)     return c.front().v;
    if (t >= c.back().t)      return c.back().v;

    const size_t k = segmentFor (c, t);
    const double h = c[k + 1].t - c[k].t;
    if (h <= 1.0e-12) return c[k + 1].v;
    const double s = (t - c[k].t) / h;
    return (float) ((double) c[k].v + ((double) c[k + 1].v - (double) c[k].v) * s);
}

float evalMonotone (const Curve1D& c, double t) noexcept
{
    if (c.empty())            return 0.0f;
    if (t <= c.front().t)     return c.front().v;
    if (t >= c.back().t)      return c.back().v;

    const size_t k = segmentFor (c, t);
    const double h = c[k + 1].t - c[k].t;
    if (h <= 1.0e-12) return c[k + 1].v;

    const double s  = (t - c[k].t) / h;
    const double s2 = s * s, s3 = s2 * s;
    const double m0 = tangent (c, k), m1 = tangent (c, k + 1);

    const double y = (2.0 * s3 - 3.0 * s2 + 1.0) * c[k].v
                   + (s3 - 2.0 * s2 + s) * h * m0
                   + (-2.0 * s3 + 3.0 * s2) * c[k + 1].v
                   + (s3 - s2) * h * m1;
    return (float) y;
}

void normalise (Curve1D& c, double minDt)
{
    std::stable_sort (c.begin(), c.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.t < b.t; });

    Curve1D out;
    out.reserve (c.size());
    for (auto& p : c)
    {
        if (! std::isfinite (p.t) || ! std::isfinite (p.v))
            continue;

        if (! out.empty() && p.t - out.back().t < minDt)
            out.back() = p;
        else
            out.push_back (p);
    }
    c.swap (out);
}

Curve1D simplify (const Curve1D& c, float tolerance)
{
    if (c.size() <= 2)
        return c;

    std::vector<bool> keep (c.size(), false);
    keep.front() = keep.back() = true;

    std::vector<std::pair<size_t, size_t>> stack;
    stack.emplace_back (0, c.size() - 1);

    while (! stack.empty())
    {
        const auto [a, b] = stack.back();
        stack.pop_back();
        if (b <= a + 1) continue;

        // vertical (value) error against the straight segment a..b: what matters for a curve that
        // is evaluated as a function of time
        const double dt = c[b].t - c[a].t;
        double worst = -1.0;
        size_t worstIdx = a;
        for (size_t i = a + 1; i < b; ++i)
        {
            const double f = dt > 1.0e-12 ? (c[i].t - c[a].t) / dt : 0.0;
            const double lin = (double) c[a].v + ((double) c[b].v - (double) c[a].v) * f;
            const double d = std::abs ((double) c[i].v - lin);
            if (d > worst) { worst = d; worstIdx = i; }
        }

        if (worst > (double) tolerance)
        {
            keep[worstIdx] = true;
            stack.emplace_back (a, worstIdx);
            stack.emplace_back (worstIdx, b);
        }
    }

    Curve1D out;
    for (size_t i = 0; i < c.size(); ++i)
        if (keep[i]) out.push_back (c[i]);
    return out;
}

} // namespace curves

//==============================================================================
float PatternLine::curvePitchAt (double tRel) const noexcept
{
    if (pitch.empty()) return 60.0f;
    return smooth ? curves::evalMonotone (pitch, tRel) : curves::evalLinear (pitch, tRel);
}

float PatternLine::pitchAt (double tRel) const noexcept
{
    float p = curvePitchAt (tRel);

    if (vibDepth > 0.0001f && tRel > 0.0)
    {
        const double fade = vibFade > 0.0001f ? juce::jlimit (0.0, 1.0, tRel / (double) vibFade) : 1.0;
        p += (float) (std::sin (juce::MathConstants<double>::twoPi * (double) vibRate * tRel) * fade) * vibDepth;
    }

    return p;
}

float PatternLine::pressureAt (double tRel) const noexcept
{
    return pressure.empty() ? defaultPressure : juce::jlimit (0.0f, 1.0f, curves::evalLinear (pressure, tRel));
}

float PatternLine::timbreAt (double tRel) const noexcept
{
    return timbre.empty() ? defaultTimbre : juce::jlimit (0.0f, 1.0f, curves::evalLinear (timbre, tRel));
}

void PatternLine::pitchRange (float& lo, float& hi) const noexcept
{
    lo = 1000.0f; hi = -1000.0f;
    for (auto& p : pitch) { lo = std::min (lo, p.v); hi = std::max (hi, p.v); }
    if (lo > hi) lo = hi = 60.0f;
    lo -= vibDepth; hi += vibDepth;
}

void PatternLine::normalise()
{
    if (! std::isfinite (start))  start = 0.0;
    if (! std::isfinite (length)) length = 1.0;
    start  = std::max (0.0, start);
    length = std::max (1.0 / 256.0, length);
    velocity = juce::jlimit (0.01f, 1.0f, std::isfinite (velocity) ? velocity : 0.8f);
    vibDepth = juce::jlimit (0.0f, 12.0f, vibDepth);
    vibRate  = juce::jlimit (0.05f, 32.0f, vibRate);
    vibFade  = juce::jlimit (0.0f, 64.0f, vibFade);

    curves::normalise (pitch);
    curves::normalise (pressure);
    curves::normalise (timbre);

    for (auto& p : pitch)    p.v = juce::jlimit (0.0f, 127.0f, p.v);
    for (auto& p : pressure) p.v = juce::jlimit (0.0f, 1.0f, p.v);
    for (auto& p : timbre)   p.v = juce::jlimit (0.0f, 1.0f, p.v);

    if (pitch.empty())
        pitch.push_back ({ 0.0, 60.0f });

    // A point at the very start (the note-on pitch is defined there).
    if (pitch.front().t > 0.0)
        pitch.insert (pitch.begin(), { 0.0, curvePitchAt (0.0) });
    for (auto& p : pitch) p.t = std::max (0.0, p.t);
}

bool PatternLine::operator== (const PatternLine& o) const noexcept
{
    using juce::exactlyEqual;
    return id == o.id && exactlyEqual (start, o.start) && exactlyEqual (length, o.length) && smooth == o.smooth
        && exactlyEqual (velocity, o.velocity) && pitch == o.pitch && pressure == o.pressure && timbre == o.timbre
        && exactlyEqual (vibDepth, o.vibDepth) && exactlyEqual (vibRate, o.vibRate) && exactlyEqual (vibFade, o.vibFade);
}

//==============================================================================
uint32_t Pattern::add (PatternLine l)
{
    l.id = nextId++;
    if (nextId == 0) nextId = 1;
    lines.push_back (std::move (l));
    return lines.back().id;
}

PatternLine* Pattern::find (uint32_t lineId) noexcept
{
    for (auto& l : lines) if (l.id == lineId) return &l;
    return nullptr;
}

const PatternLine* Pattern::find (uint32_t lineId) const noexcept
{
    for (auto& l : lines) if (l.id == lineId) return &l;
    return nullptr;
}

bool Pattern::remove (uint32_t lineId)
{
    const auto before = lines.size();
    lines.erase (std::remove_if (lines.begin(), lines.end(), [lineId] (const PatternLine& l) { return l.id == lineId; }), lines.end());
    return lines.size() != before;
}

void Pattern::normalise()
{
    bars = juce::jlimit (minBars, maxBars, bars);
    beatsPerBar = juce::jlimit (1, 16, beatsPerBar);

    uint32_t maxId = 0;
    std::vector<uint32_t> seen;
    for (auto& l : lines)
    {
        l.normalise();
        if (l.id == 0 || std::find (seen.begin(), seen.end(), l.id) != seen.end())
            l.id = 0;   // repaired below
        else
            seen.push_back (l.id);
        maxId = std::max (maxId, l.id);
    }

    nextId = std::max (nextId, maxId + 1);
    for (auto& l : lines)
        if (l.id == 0) l.id = nextId++;

    std::stable_sort (lines.begin(), lines.end(), [] (const PatternLine& a, const PatternLine& b) { return a.start < b.start; });
}

//==============================================================================
namespace
{
    juce::String curveToString (const Curve1D& c)
    {
        juce::String s;
        s.preallocateBytes (c.size() * 16);
        for (auto& p : c)
        {
            // %.17g / %.9g round-trip doubles / floats exactly
            char buf[64];
            std::snprintf (buf, sizeof (buf), "%s%.17g,%.9g", s.isEmpty() ? "" : " ", p.t, (double) p.v);
            s << buf;
        }
        return s;
    }

    Curve1D curveFromString (const juce::String& s)
    {
        Curve1D c;
        for (auto& tok : juce::StringArray::fromTokens (s, " ", ""))
        {
            const int comma = tok.indexOfChar (',');
            if (comma <= 0) continue;
            c.push_back ({ tok.substring (0, comma).getDoubleValue(), tok.substring (comma + 1).getFloatValue() });
        }
        return c;
    }
}

std::unique_ptr<juce::XmlElement> Pattern::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> (xmlTag);
    xml->setAttribute ("bars", bars);
    xml->setAttribute ("beatsPerBar", beatsPerBar);
    xml->setAttribute ("nextId", (int) nextId);

    for (auto& l : lines)
    {
        auto* e = xml->createNewChildElement ("LINE");
        e->setAttribute ("id", (int) l.id);
        e->setAttribute ("start", l.start);
        e->setAttribute ("length", l.length);
        e->setAttribute ("smooth", l.smooth ? 1 : 0);
        e->setAttribute ("velocity", (double) l.velocity);
        if (l.vibDepth > 0.0f)
        {
            e->setAttribute ("vibDepth", (double) l.vibDepth);
            e->setAttribute ("vibRate",  (double) l.vibRate);
            e->setAttribute ("vibFade",  (double) l.vibFade);
        }
        e->setAttribute ("pitch", curveToString (l.pitch));
        if (! l.pressure.empty()) e->setAttribute ("pressure", curveToString (l.pressure));
        if (! l.timbre.empty())   e->setAttribute ("timbre",   curveToString (l.timbre));
    }

    return xml;
}

Pattern Pattern::fromXml (const juce::XmlElement& xml)
{
    Pattern p;
    p.bars        = xml.getIntAttribute ("bars", defaultBars);
    p.beatsPerBar = xml.getIntAttribute ("beatsPerBar", 4);
    p.nextId      = (uint32_t) juce::jmax (1, xml.getIntAttribute ("nextId", 1));

    for (auto* e : xml.getChildWithTagNameIterator ("LINE"))
    {
        PatternLine l;
        l.id       = (uint32_t) juce::jmax (0, e->getIntAttribute ("id", 0));
        l.start    = e->getDoubleAttribute ("start", 0.0);
        l.length   = e->getDoubleAttribute ("length", 1.0);
        l.smooth   = e->getIntAttribute ("smooth", 1) != 0;
        l.velocity = (float) e->getDoubleAttribute ("velocity", 0.8);
        l.vibDepth = (float) e->getDoubleAttribute ("vibDepth", 0.0);
        l.vibRate  = (float) e->getDoubleAttribute ("vibRate", 2.5);
        l.vibFade  = (float) e->getDoubleAttribute ("vibFade", 0.5);
        l.pitch    = curveFromString (e->getStringAttribute ("pitch"));
        l.pressure = curveFromString (e->getStringAttribute ("pressure"));
        l.timbre   = curveFromString (e->getStringAttribute ("timbre"));
        p.lines.push_back (std::move (l));
    }

    p.normalise();
    return p;
}

bool Pattern::operator== (const Pattern& o) const noexcept
{
    return bars == o.bars && beatsPerBar == o.beatsPerBar && lines == o.lines;
}

} // namespace bd
