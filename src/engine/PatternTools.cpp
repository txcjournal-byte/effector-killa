#include "PatternTools.h"
#include "PatternPlayer.h"
#include "MpeOutput.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace bd::tools
{

namespace
{
    bool contains (const IdList& ids, uint32_t id)
    {
        return std::find (ids.begin(), ids.end(), id) != ids.end();
    }

    double msToBeats (double ms, double bpm) { return ms * 0.001 * bpm / 60.0; }

    /** Re-bases a curve after the line start moved by `shift` beats and its length became `len`. */
    void rebaseCurve (Curve1D& c, double shift, double len, bool isLinear, bool keepIfEmpty)
    {
        if (c.empty()) { juce::ignoreUnused (keepIfEmpty); return; }

        auto eval = [&] (double t) { return isLinear ? curves::evalLinear (c, t) : curves::evalMonotone (c, t); };
        const float atStart = eval (shift);
        const float atEnd   = eval (shift + len);

        Curve1D out;
        out.push_back ({ 0.0, atStart });
        for (auto& p : c)
        {
            const double t = p.t - shift;
            if (t > 1.0e-6 && t < len - 1.0e-6)
                out.push_back ({ t, p.v });
        }
        // only pin the end when we actually cut something off
        if (! c.empty() && c.back().t - shift > len + 1.0e-6)
            out.push_back ({ len, atEnd });
        c.swap (out);
    }

    /** New pitch curve = old curve + offset(t), sampled at the old points plus `times`. */
    template <typename Fn>
    void superimpose (PatternLine& l, std::vector<double> times, Fn&& offset)
    {
        for (auto& p : l.pitch) times.push_back (p.t);
        times.push_back (0.0);
        times.push_back (l.length);
        std::sort (times.begin(), times.end());
        times.erase (std::unique (times.begin(), times.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-6; }), times.end());

        Curve1D out;
        out.reserve (times.size());
        for (auto t : times)
            if (t >= 0.0 && t <= l.length + 1.0e-9)
                out.push_back ({ t, l.curvePitchAt (t) + offset (t) });
        l.pitch.swap (out);
    }

    uint32_t rngNext (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float rngBipolar (uint32_t& s) { return (float) (rngNext (s) & 0xffffff) / (float) 0x7fffff - 1.0f; }
}

//==============================================================================
PatternLine makeNote (double start, double length, float pitch, float velocity)
{
    PatternLine l;
    l.start = start;
    l.length = std::max (1.0 / 256.0, length);
    l.velocity = velocity;
    l.pitch = { { 0.0, pitch } };
    return l;
}

void retime (PatternLine& l, double newStart, double newEnd)
{
    newEnd = std::max (newEnd, newStart + 1.0 / 256.0);
    const double shift = newStart - l.start;
    const double len = newEnd - newStart;

    rebaseCurve (l.pitch, shift, len, ! l.smooth, true);
    rebaseCurve (l.pressure, shift, len, true, false);
    rebaseCurve (l.timbre, shift, len, true, false);

    l.start = newStart;
    l.length = len;
    l.normalise();
}

bool transpose (Pattern& p, const IdList& ids, float semis)
{
    bool changed = false;
    for (auto& l : p.lines)
        if (contains (ids, l.id))
        {
            for (auto& pt : l.pitch) pt.v = juce::jlimit (0.0f, 127.0f, pt.v + semis);
            changed = true;
        }
    return changed;
}

bool quantizeStarts (Pattern& p, const IdList& ids, double grid)
{
    if (grid <= 0.0) return false;
    bool changed = false;
    for (auto& l : p.lines)
        if (contains (ids, l.id))
        {
            const double q = std::max (0.0, std::round (l.start / grid) * grid);
            if (std::abs (q - l.start) > 1.0e-9) { l.start = q; changed = true; }
        }
    return changed;
}

bool smooth (Pattern& p, const IdList& ids, float amount)
{
    bool changed = false;
    amount = juce::jlimit (0.05f, 1.0f, amount);

    for (auto& l : p.lines)
    {
        if (! contains (ids, l.id) || l.pitch.size() < 3)
        {
            if (contains (ids, l.id) && ! l.smooth) { l.smooth = true; changed = true; }
            continue;
        }

        // dense resample -> moving average (window grows with amount) -> simplify
        const double step = 1.0 / 64.0;
        const int n = juce::jmax (2, (int) std::ceil (l.length / step) + 1);
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = l.curvePitchAt (std::min (l.length, (double) i * step));

        const int half = juce::jmax (1, (int) std::lround (amount * 8.0f));   // up to 1/8 beat each side
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            double acc = 0.0, wsum = 0.0;
            for (int k = -half; k <= half; ++k)
            {
                const int j = juce::jlimit (0, n - 1, i + k);
                const double w = 1.0 - std::abs ((double) k) / (double) (half + 1);
                acc += w * v[(size_t) j];
                wsum += w;
            }
            s[(size_t) i] = (float) (acc / wsum);
        }
        s.front() = v.front();
        s.back()  = v.back();

        Curve1D dense;
        for (int i = 0; i < n; ++i)
            dense.push_back ({ std::min (l.length, (double) i * step), s[(size_t) i] });

        l.pitch = curves::simplify (dense, 0.03f);
        l.smooth = true;
        l.normalise();
        changed = true;
    }
    return changed;
}

void magnetize (Curve1D& c, const ScaleLock& scale, float strength)
{
    strength = juce::jlimit (0.0f, 1.0f, strength);
    if (strength <= 0.0f || c.empty()) return;

    const Curve1D orig = c;
    for (size_t i = 0; i < c.size(); ++i)
    {
        // local slope (semitones per beat) from the neighbours
        double slope = 0.0;
        if (orig.size() > 1)
        {
            const size_t a = i > 0 ? i - 1 : i, b = i + 1 < orig.size() ? i + 1 : i;
            const double dt = orig[b].t - orig[a].t;
            if (dt > 1.0e-9) slope = std::abs ((double) orig[b].v - (double) orig[a].v) / dt;
            // a point that holds on one side counts as a landing point
            if (i > 0 && i + 1 < orig.size())
            {
                const double s1 = std::abs ((double) orig[i].v - (double) orig[i - 1].v) / std::max (1.0e-9, orig[i].t - orig[i - 1].t);
                const double s2 = std::abs ((double) orig[i + 1].v - (double) orig[i].v) / std::max (1.0e-9, orig[i + 1].t - orig[i].t);
                slope = std::min (s1, s2);
            }
            else
            {
                slope = 0.0;   // first / last point always land
            }
        }

        const double k = 6.0;   // semitones per beat where the pull is halved
        const float weight = (float) (1.0 / (1.0 + (slope / k) * (slope / k)));
        const float target = scale.snapPitch (orig[i].v);
        c[i].v = orig[i].v + (target - orig[i].v) * strength * weight;
    }
}

IdList duplicate (Pattern& p, const IdList& ids, double offset, float semis)
{
    std::vector<PatternLine> copies;
    for (auto& l : p.lines)
        if (contains (ids, l.id))
        {
            auto c = l;
            c.start = std::max (0.0, c.start + offset);
            for (auto& pt : c.pitch) pt.v = juce::jlimit (0.0f, 127.0f, pt.v + semis);
            copies.push_back (std::move (c));
        }

    IdList out;
    for (auto& c : copies)
        out.push_back (p.add (std::move (c)));
    return out;
}

uint32_t split (Pattern& p, uint32_t id, double at)
{
    auto* l = p.find (id);
    if (l == nullptr || at <= 1.0e-4 || at >= l->length - 1.0e-4)
        return 0;

    auto second = *l;
    const double absAt = l->start + at;
    retime (second, absAt, l->end());
    retime (*l, l->start, absAt);
    return p.add (std::move (second));
}

//==============================================================================
const char* gestureName (Gesture g)
{
    switch (g)
    {
        case Gesture::glide:      return "Glide";
        case Gesture::chordGlide: return "Chord Glide";
        case Gesture::vibrato:    return "Vibrato";
        case Gesture::dive:       return "808 Dive";
        case Gesture::rise:       return "Rise";
        case Gesture::wobble:     return "Wobble";
        case Gesture::shake:      return "Shake";
        case Gesture::trill:      return "Trill";
        case Gesture::numGestures: break;
    }
    return "";
}

namespace
{
    /** Appends `b` to `a` (a legato chain): a holds until b starts, then glides to b's pitch over
        `glideBeats` with the given curve shape. b's own curve continues after the glide. */
    void mergeInto (PatternLine& a, const PatternLine& b, double glideBeats, Curve shape)
    {
        const double join = std::max (a.start + 1.0 / 64.0, b.start);     // a is cut where b starts
        const double aLen = join - a.start;
        const double bOff = b.start - a.start;                             // b's t=0 inside a
        const double g = juce::jlimit (1.0 / 128.0, std::max (1.0 / 128.0, b.length * 0.9), glideBeats);

        const float fromPitch = a.curvePitchAt (std::min (aLen, a.length));
        Curve1D pts;
        for (auto& pt : a.pitch) if (pt.t < aLen - 1.0e-6) pts.push_back (pt);
        pts.push_back ({ aLen, fromPitch });

        // glide shape: sample the 1.x curve so the monotone spline follows it
        const float toPitch = b.curvePitchAt (g);
        const int steps = shape == Curve::linear ? 1 : 4;
        for (int i = 1; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            pts.push_back ({ aLen + g * x, fromPitch + (toPitch - fromPitch) * applyCurve (shape, x) });
        }

        for (auto& pt : b.pitch)
            if (pt.t > g + 1.0e-6)
                pts.push_back ({ bOff + pt.t, pt.v });

        // expression: concatenate (a until the join, then b)
        auto concat = [&] (const Curve1D& ca, const Curve1D& cb, float defA, float defB)
        {
            Curve1D out;
            if (ca.empty() && cb.empty()) return out;
            if (ca.empty()) out.push_back ({ 0.0, defA });
            for (auto& pt : ca) if (pt.t < aLen) out.push_back (pt);
            out.push_back ({ aLen, ca.empty() ? defA : curves::evalLinear (ca, aLen) });
            if (cb.empty()) out.push_back ({ bOff + 1.0 / 64.0, defB });
            for (auto& pt : cb) out.push_back ({ bOff + pt.t + 1.0e-4, pt.v });
            return out;
        };

        a.pressure = concat (a.pressure, b.pressure, PatternLine::defaultPressure, PatternLine::defaultPressure);
        a.timbre   = concat (a.timbre,   b.timbre,   PatternLine::defaultTimbre,   PatternLine::defaultTimbre);
        a.pitch    = pts;
        a.smooth   = true;
        a.length   = std::max (aLen + 1.0 / 64.0, b.end() - a.start);
        a.vibDepth = std::max (a.vibDepth, b.vibDepth);
        a.normalise();
    }

    IdList bakeGlides (Pattern& p, const IdList& ids, bool chords, const EngineSettings& s, double bpm)
    {
        std::vector<const PatternLine*> sel;
        for (auto& l : p.lines) if (contains (ids, l.id)) sel.push_back (&l);
        std::sort (sel.begin(), sel.end(), [] (auto* a, auto* b)
                   { return a->start < b->start || (juce::exactlyEqual (a->start, b->start) && a->startPitch() < b->startPitch()); });

        // group into "chords" (single lines when not chord-gliding)
        std::vector<std::vector<const PatternLine*>> groups;
        const double chordTol = chords ? std::max (1.0 / 16.0, msToBeats (s.chordWindowMs, bpm)) : -1.0;
        for (auto* l : sel)
        {
            if (! groups.empty() && chords && l->start - groups.back().front()->start <= chordTol)
                groups.back().push_back (l);
            else
                groups.push_back ({ l });
        }

        // chains of line ids to merge
        std::vector<std::vector<uint32_t>> chains;
        std::vector<int> prevChains;   // chains ending in the previous group
        for (auto& grp : groups)
        {
            std::vector<int> nowChains;
            std::vector<bool> used (prevChains.size(), false);
            const float range = (float) s.bendRange;

            // greedy nearest-pitch matching (like the live chord glide)
            std::vector<bool> done (grp.size(), false);
            for (;;)
            {
                int bi = -1, bc = -1; float best = 1.0e9f;
                for (size_t i = 0; i < grp.size(); ++i)
                {
                    if (done[i]) continue;
                    for (size_t c = 0; c < prevChains.size(); ++c)
                    {
                        if (used[c]) continue;
                        const auto* last = p.find (chains[(size_t) prevChains[c]].back());
                        const float d = std::abs (last->curvePitchAt (last->length) - grp[i]->startPitch());
                        if (d < best && d <= range) { best = d; bi = (int) i; bc = (int) c; }
                    }
                }
                if (bi < 0) break;
                done[(size_t) bi] = used[(size_t) bc] = true;
                chains[(size_t) prevChains[(size_t) bc]].push_back (grp[(size_t) bi]->id);
                nowChains.push_back (prevChains[(size_t) bc]);
            }

            for (size_t i = 0; i < grp.size(); ++i)
                if (! done[i])
                {
                    chains.push_back ({ grp[i]->id });
                    nowChains.push_back ((int) chains.size() - 1);
                }

            prevChains = nowChains;
        }

        const double g = msToBeats (s.glideTimeMs, bpm);
        IdList result;
        for (auto& chain : chains)
        {
            auto* head = p.find (chain.front());
            for (size_t i = 1; i < chain.size(); ++i)
            {
                if (const auto* next = p.find (chain[i]))
                {
                    const auto copy = *next;
                    mergeInto (*head, copy, g, s.glideCurve);
                    p.remove (copy.id);
                    head = p.find (chain.front());   // pointer may move after remove
                }
            }
            result.push_back (chain.front());
        }
        return result;
    }
}

IdList bakeGesture (Pattern& p, const IdList& ids, Gesture g, const EngineSettings& s, double bpm)
{
    bpm = bpm > 1.0 ? bpm : 120.0;
    ScaleLock scale;
    scale.set (s.scaleKey, s.scaleLockOn ? s.scaleType : (int) ScaleLock::chromatic);

    if (g == Gesture::glide || g == Gesture::chordGlide)
    {
        auto r = bakeGlides (p, ids, g == Gesture::chordGlide, s, bpm);
        p.normalise();
        return r;
    }

    for (auto& l : p.lines)
    {
        if (! contains (ids, l.id)) continue;

        switch (g)
        {
            case Gesture::vibrato:
            {
                l.vibDepth = s.vibDepthCents * 0.01f;
                l.vibRate  = (float) (s.vibSync ? 1.0 / syncDivisionBeats (s.vibDiv) : (double) s.vibRateHz * 60.0 / bpm);
                l.vibFade  = (float) msToBeats (s.vibDelayMs + 350.0, bpm);
                break;
            }

            case Gesture::dive:
            case Gesture::rise:
            {
                // like 1.x: the voice keeps sounding after the note and bends during the tail
                const float dir = g == Gesture::dive ? -1.0f : 1.0f;
                const double len = s.diveSync ? syncDivisionBeats (s.diveDiv) : msToBeats (s.diveTimeMs, bpm);
                const double oldLen = l.length;
                const float from = l.curvePitchAt (oldLen);
                float target = from + dir * s.diveSemis;
                if (s.scaleLockOn) target = scale.snapOffsetTarget ((int) std::lround (from), dir * s.diveSemis);

                Curve1D pts;
                for (auto& pt : l.pitch) if (pt.t < oldLen - 1.0e-6) pts.push_back (pt);
                pts.push_back ({ oldLen, from });
                const int steps = s.diveCurve == Curve::linear ? 1 : 5;
                for (int i = 1; i <= steps; ++i)
                {
                    const float x = (float) i / (float) steps;
                    pts.push_back ({ oldLen + len * x, from + (target - from) * applyCurve (s.diveCurve, x) });
                }
                l.pitch = pts;
                l.length = oldLen + len;
                l.smooth = true;
                break;
            }

            case Gesture::wobble:
            {
                const double period = syncDivisionBeats (s.wobbleDiv);
                std::vector<double> t;
                for (double x = 0.0; x <= l.length; x += period / 8.0) t.push_back (x);
                superimpose (l, t, [&] (double x) { return (float) std::sin (juce::MathConstants<double>::twoPi * x / period) * s.wobbleSemis; });
                l.smooth = true;
                break;
            }

            case Gesture::shake:
            {
                const double step = std::max (1.0 / 128.0, (1.0 / std::max (1.0f, s.shakeRateHz)) * bpm / 60.0);
                uint32_t rng = l.id * 2654435761u + 1u;
                std::vector<double> t;
                std::vector<float> vals;
                for (double x = 0.0; x <= l.length; x += step) { t.push_back (x); vals.push_back (x <= 0.0 ? 0.0f : rngBipolar (rng)); }
                const auto depth = s.shakeCents * 0.01f;
                superimpose (l, t, [&] (double x)
                {
                    const auto i = (size_t) juce::jlimit (0, (int) vals.size() - 1, (int) std::floor (x / step));
                    const auto j = std::min (vals.size() - 1, i + 1);
                    const float f = (float) (x / step - (double) i);
                    return (vals[i] + (vals[j] - vals[i]) * f) * depth;
                });
                l.smooth = true;
                break;
            }

            case Gesture::trill:
            {
                const double period = syncDivisionBeats (s.trillDiv);
                const int base = (int) std::lround (l.startPitch());
                const int upper = s.scaleLockOn ? scale.stepDegrees (base, 1) : base + 2;
                const float off = (float) (upper - base);
                const double ramp = std::min (period * 0.08, 1.0 / 64.0);
                std::vector<double> t;
                for (double x = period; x < l.length; x += period) { t.push_back (x - ramp); t.push_back (x); }
                for (double x = 0.0; x < l.length; x += period / 4.0) t.push_back (x);
                superimpose (l, t, [&] (double x)
                {
                    const auto step = (int64_t) std::floor ((x + 1.0e-9) / period);
                    const double into = x - (double) step * period;
                    const bool up = (step & 1) == 1;
                    if (into < -1.0e-9) return 0.0f;
                    return up ? off : 0.0f;
                });
                l.smooth = false;
                break;
            }

            case Gesture::glide:
            case Gesture::chordGlide:
            case Gesture::numGestures:
                break;
        }

        l.normalise();
    }

    p.normalise();
    return ids;
}

//==============================================================================
std::vector<PatternLine> linesFromCapture (const std::vector<CapturedEvent>& evs, double startPpq, double len,
                                           int bendRange, bool includeChannel1)
{
    struct Open
    {
        bool active = false;
        int note = 60;
        float velocity = 0.8f;
        double start = 0.0;
        Curve1D pitch, pressure, timbre;
    };

    std::array<Open, 17> open {};
    std::array<float, 17> bend {}, pres {}, timb {};
    pres.fill (PatternLine::defaultPressure);
    timb.fill (PatternLine::defaultTimbre);

    std::vector<PatternLine> out;
    const double endPpq = startPpq + len;
    const float range = (float) juce::jmax (1, bendRange);

    auto close = [&] (int ch, double at)
    {
        auto& o = open[(size_t) ch];
        if (! o.active) return;
        o.active = false;

        const double s = std::max (o.start, startPpq);
        const double e = std::min (at, endPpq);
        if (e - s < 1.0 / 256.0) return;

        PatternLine l;
        l.start = s - startPpq;
        l.length = e - s;
        l.velocity = o.velocity;
        l.smooth = false;

        auto clip = [&] (const Curve1D& c, float tol)
        {
            Curve1D r;
            if (c.empty()) return r;
            const double shift = s - o.start;
            r.push_back ({ 0.0, curves::evalLinear (c, shift) });
            for (auto& p : c)
            {
                const double t = p.t - shift;
                if (t > 1.0e-6 && t < l.length - 1.0e-6) r.push_back ({ t, p.v });
            }
            r.push_back ({ l.length, curves::evalLinear (c, shift + l.length) });
            return curves::simplify (r, tol);
        };

        l.pitch = clip (o.pitch, 0.03f);
        // a flat curve becomes a single point (cleaner to edit)
        bool flat = true;
        for (auto& p : l.pitch) flat = flat && std::abs (p.v - l.pitch.front().v) < 0.01f;
        if (flat && ! l.pitch.empty()) l.pitch.resize (1);

        l.pressure = clip (o.pressure, 0.02f);
        l.timbre   = clip (o.timbre, 0.02f);
        auto flatten = [] (Curve1D& c, float def)
        {
            bool f = true;
            for (auto& p : c) f = f && std::abs (p.v - def) < 0.01f;
            if (f) c.clear();
        };
        flatten (l.pressure, PatternLine::defaultPressure);
        flatten (l.timbre, PatternLine::defaultTimbre);

        l.normalise();
        out.push_back (std::move (l));
    };

    for (auto& e : evs)
    {
        const int ch = juce::jlimit (1, 16, (int) e.channel);
        if (ch == 1 && ! includeChannel1) continue;
        auto& o = open[(size_t) ch];
        const double t = e.ppq;

        switch (e.type)
        {
            case CapturedEvent::noteOn:
                close (ch, t);
                o = Open{};
                o.active = true;
                o.note = e.note;
                o.velocity = juce::jlimit (0.01f, 1.0f, (float) e.velocity / 127.0f);
                o.start = t;
                o.pitch.push_back ({ 0.0, (float) e.note + bend[(size_t) ch] });
                o.pressure.push_back ({ 0.0, pres[(size_t) ch] });
                o.timbre.push_back ({ 0.0, timb[(size_t) ch] });
                break;

            case CapturedEvent::noteOff:
                if (o.active && o.note == e.note) close (ch, t);
                break;

            case CapturedEvent::bend:
                bend[(size_t) ch] = MpeOutput::pitchWheelToSemitones (e.value, range);
                if (o.active) o.pitch.push_back ({ t - o.start, (float) o.note + bend[(size_t) ch] });
                break;

            case CapturedEvent::pressure:
                pres[(size_t) ch] = (float) e.value / 127.0f;
                if (o.active) o.pressure.push_back ({ t - o.start, pres[(size_t) ch] });
                break;

            case CapturedEvent::timbre:
                timb[(size_t) ch] = (float) e.value / 127.0f;
                if (o.active) o.timbre.push_back ({ t - o.start, timb[(size_t) ch] });
                break;

            default: break;
        }
    }

    for (int ch = 1; ch <= 16; ++ch)
        close (ch, endPpq);

    std::sort (out.begin(), out.end(), [] (const PatternLine& a, const PatternLine& b) { return a.start < b.start; });
    return out;
}

//==============================================================================
juce::MidiFile exportMidi (const Pattern& pattern, bool mono, int bendRange, double bpm)
{
    constexpr int tpq = 960;
    bpm = bpm > 1.0 ? bpm : 120.0;

    juce::MidiMessageSequence seq;
    // NB: addEvent's second argument is ADDED to the message's own timestamp
    auto add = [&seq] (juce::MidiMessage m, double tick) { m.setTimeStamp (tick); seq.addEvent (m); };
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (juce::jmax (1, pattern.beatsPerBar), 4), 0.0);
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, mono ? "BEND DEMON (mono)" : "BEND DEMON (MPE)"), 0.0);

    // setup: MPE zone (or bend range on channel 1)
    {
        juce::MidiBuffer setup;
        if (mono)
            MpeOutput::addRpn (setup, 0, 100, 1, 0, bendRange);
        else
            MpeOutput::addZoneConfiguration (setup, 0, 100, 15, bendRange);

        for (const auto m : setup)
            add (m.getMessage(), 0.0);
    }

    // Render through the real player: one "sample" = one tick.
    PatternPlayer player;
    player.prepare ((double) tpq * bpm / 60.0);
    player.setControlInterval (tpq / 96);        // 96 controller updates per beat
    PatternPlayer::Config cfg;
    cfg.mono = mono;
    cfg.bendRange = bendRange;
    player.setConfig (cfg);
    player.setPattern (&pattern);

    const auto total = (int) std::lround (pattern.lengthBeats() * tpq);
    const int block = tpq;
    juce::MidiBuffer buf;

    for (int pos = 0; pos < total; pos += block)
    {
        const int n = juce::jmin (block, total - pos);
        buf.clear();
        PatternPlayer::Clock clk { true, (double) pos / tpq, bpm };
        player.process (buf, n, clk);

        for (const auto m : buf)
            add (m.getMessage(), (double) (pos + m.samplePosition));
    }

    buf.clear();
    player.releaseAll (buf, 0);
    for (const auto m : buf)
        if (m.getMessage().isNoteOff())
            add (m.getMessage(), (double) total);

    seq.addEvent (juce::MidiMessage::endOfTrack(), (double) total);
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);
    file.addTrack (seq);
    return file;
}

} // namespace bd::tools
