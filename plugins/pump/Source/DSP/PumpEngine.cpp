#include "PumpEngine.h"

#include <algorithm>
#include <cmath>

namespace pump
{

namespace
{
constexpr double pi = 3.14159265358979323846;
inline float clamp01 (float v) { return std::min (1.0f, std::max (0.0f, v)); }

float shape (float t, float tension)
{
    tension = std::max (-1.0f, std::min (1.0f, tension));
    if (tension > 0.0f)
        return std::pow (t, 1.0f + 3.0f * tension);
    if (tension < 0.0f)
        return 1.0f - std::pow (1.0f - t, 1.0f - 3.0f * tension);
    return t;
}

struct PresetDef
{
    const char* name;
    std::vector<Point> pts;
};

const std::vector<PresetDef>& presetDefs()
{
    static const std::vector<PresetDef> defs {
        { "Classic Duck",   { { 0.00f, 0.00f, -0.55f }, { 0.50f, 1.00f, 0.0f } } },
        { "808 Space",      { { 0.00f, 0.15f, -0.35f }, { 0.35f, 1.00f, 0.0f } } },
        { "Hard Pump",      { { 0.00f, 0.00f, 0.50f }, { 0.75f, 1.00f, 0.0f } } },
        { "Gentle Breathe", { { 0.00f, 0.60f, -0.40f }, { 0.60f, 1.00f, 0.0f } } },
        { "Chop",           { { 0.00f, 1.00f, 0.0f }, { 0.40f, 1.00f, 0.0f }, { 0.45f, 0.00f, 0.0f }, { 0.95f, 0.00f, 0.0f } } },
        { "Stutter Gate",   { { 0.00f, 1.00f, 0.0f }, { 0.20f, 1.00f, 0.0f }, { 0.24f, 0.00f, 0.0f }, { 0.49f, 0.00f, 0.0f },
                              { 0.51f, 1.00f, 0.0f }, { 0.70f, 1.00f, 0.0f }, { 0.74f, 0.00f, 0.0f }, { 0.97f, 0.00f, 0.0f } } },
        { "Reverse Swell",  { { 0.00f, 0.00f, 0.60f }, { 0.96f, 1.00f, 0.0f } } },
        { "Wobble",         { { 0.00f, 1.00f, 0.45f }, { 0.25f, 0.55f, -0.45f }, { 0.50f, 0.15f, 0.45f }, { 0.75f, 0.55f, -0.45f }, { 0.999f, 1.00f, 0.0f } } },
    };
    return defs;
}
} // namespace

// ---------------- Curve ----------------

Curve::Curve() { points = presetDefs().front().pts; }

void Curve::setPoints (std::vector<Point> pts)
{
    for (auto& p : pts)
    {
        p.x = std::min (0.999f, std::max (0.0f, p.x));
        p.y = clamp01 (p.y);
        p.tension = std::max (-1.0f, std::min (1.0f, p.tension));
    }
    std::sort (pts.begin(), pts.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
    if (pts.empty())
        pts.push_back ({ 0.0f, 1.0f, 0.0f });
    pts.front().x = 0.0f;
    points = std::move (pts);
}

float Curve::evaluate (float x) const
{
    x = x - std::floor (x);
    const size_t n = points.size();
    for (size_t i = 0; i < n; ++i)
    {
        const Point& a = points[i];
        const bool last = (i + 1 == n);
        if (last)
            return a.y; // hold the last value until the cycle restarts (the jump is smoothed by SMOOTH)
        const float bx = points[i + 1].x;
        const float by = points[i + 1].y;
        if (x >= a.x && x < bx)
        {
            const float span = bx - a.x;
            const float t = span > 1e-6f ? (x - a.x) / span : 1.0f;
            return a.y + (by - a.y) * shape (t, a.tension);
        }
    }
    return points.back().y;
}

Curve Curve::preset (int index)
{
    const auto& d = presetDefs();
    Curve c;
    c.setPoints (d[(size_t) std::max (0, std::min ((int) d.size() - 1, index))].pts);
    return c;
}

const char* Curve::presetName (int index)
{
    const auto& d = presetDefs();
    return d[(size_t) std::max (0, std::min ((int) d.size() - 1, index))].name;
}

int Curve::numPresets() { return (int) presetDefs().size(); }

// ---------------- Biquad ----------------

void Biquad::setLowpass (double s, double f, double q)
{
    const double w = 2.0 * pi * f / s, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1 + al;
    b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
}

void Biquad::setHighpass (double s, double f, double q)
{
    const double w = 2.0 * pi * f / s, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1 + al;
    b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
}

// ---------------- Engine ----------------

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    reset();
}

void Engine::reset()
{
    phase = 0.0;
    oneShotRunning = false;
    numNotes = 0;
    detEnv = detFloor = 0.0f;
    holdoff = 0;
    gainSmoothed = lastGain = 1.0f;
    mixSm = clamp01 (params.mix);
    outSm = std::pow (10.0f, params.outputDb / 20.0f);
    for (auto& s : split)
        for (auto* b : { &s.lp1, &s.lp2, &s.hp1, &s.hp2 })
            b->reset();
    lastCrossover = -1.0f;
    updateFilters();
}

void Engine::setParams (const Params& p)
{
    params = p;
    params.rate = std::max (0, std::min ((int) rateBeats.size() - 1, params.rate));
    const double ms = std::max (0.05, (double) params.smoothMs);
    smoothCoef = (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr)));
    updateFilters();
}

void Engine::updateFilters()
{
    const float f = std::max (40.0f, std::min (1000.0f, params.crossoverHz));
    if (std::abs (f - lastCrossover) < 0.01f)
        return;
    for (auto& s : split)
    {
        s.lp1.setLowpass (sr, f, 0.70710678);
        s.lp2.setLowpass (sr, f, 0.70710678);
        s.hp1.setHighpass (sr, f, 0.70710678);
        s.hp2.setHighpass (sr, f, 0.70710678);
    }
    lastCrossover = f;
}

void Engine::process (float* const* main, int numChannels, const float* const* sidechain, int numScChannels,
                      int numSamples, const HostTime& time)
{
    numChannels = std::min (numChannels, maxChannels);
    triggeredFlag = false;
    if (numChannels <= 0 || numSamples <= 0)
    {
        numNotes = 0;
        return;
    }

    const double cycleBeats = rateBeats[(size_t) params.rate];
    const double bpm = time.bpm > 1.0 ? time.bpm : 120.0;
    const double inc = bpm / 60.0 / sr / cycleBeats; // cycles per sample
    const float targetMix = clamp01 (params.mix);
    const float targetOut = std::pow (10.0f, std::max (-24.0f, std::min (12.0f, params.outputDb)) / 20.0f);
    const float paramCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
    const float detAttack = (float) (1.0 - std::exp (-1.0 / (0.0005 * sr)));
    const float detRelease = (float) (1.0 - std::exp (-1.0 / (0.030 * sr)));
    const int holdoffSamples = (int) (0.05 * sr);
    const bool split3 = params.band != Band::full;

    std::sort (noteOffsets.begin(), noteOffsets.begin() + numNotes);
    int nextNote = 0;

    for (int n = 0; n < numSamples; ++n)
    {
        // ---------- phase ----------
        bool trig = false;
        if (params.trigger == Trigger::midi)
        {
            while (nextNote < numNotes && noteOffsets[(size_t) nextNote] <= n)
            {
                trig = true;
                ++nextNote;
            }
        }
        else if (params.trigger == Trigger::audio)
        {
            float peak = 0.0f;
            if (sidechain != nullptr && numScChannels > 0)
                for (int c = 0; c < numScChannels; ++c) peak = std::max (peak, std::abs (sidechain[c][n]));
            else
                for (int c = 0; c < numChannels; ++c) peak = std::max (peak, std::abs (main[c][n]));
            detEnv += (peak > detEnv ? detAttack : detRelease) * (peak - detEnv);
            if (holdoff > 0)
                --holdoff;
            const float thr = std::max (0.001f, params.threshold);
            if (detFloor < 0.5f && detEnv > thr && holdoff == 0)
            {
                trig = true;
                detFloor = 1.0f; // armed -> fired
                holdoff = holdoffSamples;
            }
            else if (detEnv < thr * 0.6f)
                detFloor = 0.0f; // re-arm
        }

        float curveVal = 1.0f;
        if (params.trigger == Trigger::sync)
        {
            if (time.playing)
            {
                const double beats = time.ppq + (double) n * bpm / 60.0 / sr;
                phase = beats / cycleBeats;
                phase -= std::floor (phase);
            }
            else
            {
                phase += inc;
                if (phase >= 1.0) phase -= std::floor (phase);
            }
            curveVal = curve.evaluate ((float) phase);
        }
        else
        {
            if (trig)
            {
                phase = 0.0;
                oneShotRunning = true;
                triggeredFlag = true;
            }
            if (oneShotRunning)
            {
                // one pass through the curve, then hold at the last point's value (no wrap back)
                const auto& pts = curve.getPoints();
                const float lastX = pts.back().x;
                curveVal = (phase < lastX) ? curve.evaluate ((float) phase) : pts.back().y;
                phase += inc;
                if (phase >= 1.0)
                {
                    phase = 1.0;
                    oneShotRunning = false;
                }
            }
            else
            {
                curveVal = curve.getPoints().back().y;
            }
        }

        // ---------- gain ----------
        const float target = 1.0f - clamp01 (params.depth) * (1.0f - clamp01 (curveVal));
        gainSmoothed += smoothCoef * (target - gainSmoothed);
        const float g = gainSmoothed;
        mixSm += paramCoef * (targetMix - mixSm);
        outSm += paramCoef * (targetOut - outSm);

        for (int c = 0; c < numChannels; ++c)
        {
            const float x = main[c][n];
            float dry, wet;
            if (split3)
            {
                auto& s = split[(size_t) c];
                const float lo = s.lp2.process (s.lp1.process (x));
                const float hi = s.hp2.process (s.hp1.process (x));
                dry = lo + hi; // Linkwitz-Riley: flat magnitude, same phase as wet
                wet = (params.band == Band::low) ? lo * g + hi : lo + hi * g;
            }
            else
            {
                dry = x;
                wet = x * g;
            }
            float o = (dry + (wet - dry) * mixSm) * outSm;
            if (! std::isfinite (o))
                o = 0.0f;
            main[c][n] = o;
        }
        lastGain = g;
    }
    numNotes = 0;
}

} // namespace pump
