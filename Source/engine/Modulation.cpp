#include "Modulation.h"

namespace ek
{
void ModulationEngine::prepare (double sr) { sampleRate = sr; reset(); }

void ModulationEngine::reset()
{
    freeBeats = 0.0;
    lastScribbleBeats = 0.0;
    glitchEnv = 0.0f;
}

double ModulationEngine::beatsPerBar (const ProcessContext& ctx) noexcept
{
    const int num = ctx.timeSigNumerator > 0 ? ctx.timeSigNumerator : 4;
    const int den = ctx.timeSigDenominator > 0 ? ctx.timeSigDenominator : 4;
    return (double) num * 4.0 / (double) den;
}

juce::Point<float> ModulationEngine::pathPosition (const std::vector<juce::Point<float>>& path, double phase01) noexcept
{
    if (path.empty()) return { 0.5f, 0.5f };
    if (path.size() == 1) return path.front();
    const double pos = phase01 * (double) path.size();
    const int i0 = juce::jlimit (0, (int) path.size() - 1, (int) pos);
    const int i1 = (i0 + 1) % (int) path.size(); // closed loop
    const float t = (float) (pos - (double) i0);
    return path[(size_t) i0] + (path[(size_t) i1] - path[(size_t) i0]) * t;
}

static inline double triangle (double u) noexcept
{
    const double m = std::fmod (u, 2.0);
    const double w = m < 0.0 ? m + 2.0 : m;
    return w < 1.0 ? w : 2.0 - w;
}

juce::Point<float> ModulationEngine::screensaverPosition (double beats, double crossingBeats) noexcept
{
    const double c = std::max (0.01, crossingBeats);
    // X crosses the screen every c beats, Y every 0.8 c: they meet in a corner every 4 crossings.
    return { (float) triangle (beats / c), (float) triangle (beats / (0.8 * c) + 0.5) };
}

ModOutput ModulationEngine::process (const ModState& st, juce::Point<float> mean, const ProcessContext& ctx, int n) noexcept
{
    ModOutput out;
    const double dBeats = (double) n * ctx.beatsPerSample();
    const bool transport = ctx.playing && ctx.hostHasPosition;

    // internal clock: follows the host while playing, free-runs otherwise. [b0, b1] = this chunk.
    const double b0 = transport ? ctx.ppq : freeBeats;
    const double b1 = b0 + dBeats;
    freeBeats = b1;

    glitchEnv = std::max (0.0f, glitchEnv - (float) dBeats); // decays over one beat

    switch (st.mode)
    {
        case AvMode::Remote:
            break;

        case AvMode::Scribble:
        {
            if (st.path.size() < 2) break;
            double beats;
            if (transport) beats = ctx.ppq;
            else if (! ctx.hostHasPosition) beats = lastScribbleBeats + dBeats; // standalone: run
            else beats = lastScribbleBeats;                                       // host stopped: hold
            lastScribbleBeats = beats;
            const double loopBeats = (double) st.scribbleBars * beatsPerBar (ctx);
            double phase = std::fmod (beats / loopBeats, 1.0);
            if (phase < 0.0) phase += 1.0;
            const auto p = pathPosition (st.path, phase);
            out.active = true;
            out.dotX = p.x; out.dotY = p.y;
            out.offX = (p.x - mean.x) * st.depth * 1.5f;
            out.offY = (p.y - mean.y) * st.depth * 1.5f;
            break;
        }

        case AvMode::Screensaver:
        {
            const double crossing = (double) screensaverBarsForIndex (st.screensaverSpeed) * beatsPerBar (ctx);
            const auto p = screensaverPosition (b1, crossing);

            // corner detection: X hits an edge inside this chunk while Y is (almost) at an edge too
            const double ux0 = b0 / crossing, ux1 = b1 / crossing;
            const auto crossIndex = (juce::int64) std::floor (ux1);
            if (std::floor (ux1) != std::floor (ux0) && ux1 > 0.0 && crossIndex != lastCrossing)
            {
                lastCrossing = crossIndex;
                const double tEdge = std::floor (ux1) * crossing;
                const double uy = tEdge / (0.8 * crossing) + 0.5;
                const double dist = std::abs (uy - std::round (uy));
                if (dist < 0.035)
                {
                    glitchEnv = 1.0f;
                    ++cornerHits;
                }
            }
            out.active = true;
            out.dotX = p.x; out.dotY = p.y;
            out.offX = (p.x - 0.5f) * st.depth;
            out.offY = (p.y - 0.5f) * st.depth;
            break;
        }
    }
    out.glitch = glitchEnv;
    return out;
}

} // namespace ek
