#include "SnapEngine.h"

#include <algorithm>
#include <cmath>

namespace snapfx
{

namespace
{
inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float sanitize (float v) { return std::isfinite (v) ? v : 0.0f; }
inline float coefFor (double seconds, double sr) { return (float) (1.0 - std::exp (-1.0 / (seconds * sr))); }
constexpr float dbPerLn = 8.6858896f; // 20 / ln(10)

// smooth clipper: linear below 75 % of the ceiling, tanh knee up to the ceiling
inline float softClip (float x, float ceil)
{
    const float a = std::abs (x);
    const float t = ceil * 0.75f;
    if (a <= t) return x;
    const float r = ceil - t;
    const float y = t + r * std::tanh ((a - t) / r);
    return x < 0.0f ? -y : y;
}
} // namespace

void Engine::SVF::set (float cutoff, float sr)
{
    const float fc = clampf (cutoff, 10.0f, sr * 0.45f);
    g = std::tan (3.14159265f * fc / sr);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void Engine::prepare (double sr, int /*maxBlock*/)
{
    sampleRate = sr > 1000.0 ? sr : 48000.0;
    lookaheadSamples = std::max (1, (int) std::lround (lookaheadSeconds * sampleRate));
    delaySize = lookaheadSamples + 1;
    for (auto& d : delay) d.assign ((size_t) delaySize * 2, 0.0f);   // [x, band] interleaved per channel
    const int rampLen = std::max (1, (int) (0.02 * sampleRate));
    for (auto* r : { &rAttack, &rSustain, &rMix, &rOut, &rCeil }) r->setLength (rampLen);
    rLook.setLength (std::max (1, (int) (0.01 * sampleRate)));
    splitCoef = coefFor (0.02, sampleRate / 16.0);
    gainCoef = coefFor (0.00025, sampleRate);
    gainCoefLa = coefFor (0.0006, sampleRate);
    for (int k = 0; k < 4; ++k)
        gainTap[k] = std::min (lookaheadSamples, (int) std::lround (0.0005 * k * sampleRate));
    gainSize = lookaheadSamples + 1;
    gainLine.assign ((size_t) gainSize, 1.0f);
    histHop = std::max (1, (int) std::lround (sampleRate * 0.005));
    reset();
}

void Engine::reset()
{
    for (auto& s : split) s.clear();
    for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
    delayPos = 0;
    std::fill (gainLine.begin(), gainLine.end(), 1.0f);
    gainPos = 0;
    eFast = eSlow = eHold = eShort = 0.0f;
    gainSm = 1.0f;
    gainDbNow = 0.0f;
    first = true;
    lastSpeed = -1.0f;
    splitSet = -1.0f;
    histCount = 0;
    accIn = accOut = accGain = 0.0f;
    clipPeak = 0.0f;
    for (int i = 0; i < historySize; ++i)
    {
        histIn[(size_t) i].store (0.0f, std::memory_order_relaxed);
        histOut[(size_t) i].store (0.0f, std::memory_order_relaxed);
        histGain[(size_t) i].store (0.0f, std::memory_order_relaxed);
    }
}

void Engine::updateTimes()
{
    const float sp = clampf (sanitize (params.speed), 0.0f, 1.0f);
    if (sp == lastSpeed) return;
    lastSpeed = sp;
    const double scale = std::pow (2.0, (0.5 - (double) sp) * 2.6);
    aFast = coefFor (0.00015 * scale, sampleRate);
    aSlow = coefFor (0.012 * scale, sampleRate);
    rRel = coefFor (0.080 * scale, sampleRate);
    rShort = coefFor (0.030 * scale, sampleRate);
    rHold = coefFor (0.300 * scale, sampleRate);
}

void Engine::process (float* const* ch, int numCh, int n)
{
    if (numCh <= 0 || n <= 0) return;
    numCh = std::min (numCh, 2);
    const Params& p = params;

    const float tAttack = clampf (sanitize (p.attack), -1.0f, 1.0f);
    const float tSustain = clampf (sanitize (p.sustain), -1.0f, 1.0f);
    const float tMix = clampf (sanitize (p.mix), 0.0f, 1.0f);
    const float tOut = std::pow (10.0f, clampf (sanitize (p.outputDb), -60.0f, 24.0f) / 20.0f);
    const float tCeil = std::pow (10.0f, clampf (sanitize (p.ceilingDb), -24.0f, 0.0f) / 20.0f);
    const float tLook = p.lookahead ? 1.0f : 0.0f;
    const float tSplit = clampf (sanitize (p.splitHz), 20.0f, 20000.0f);
    const Band band = p.band;
    updateTimes();

    if (first)
    {
        rAttack.snap (tAttack); rSustain.snap (tSustain); rMix.snap (tMix); rOut.snap (tOut);
        rCeil.snap (tCeil); rLook.snap (tLook);
        splitSm = tSplit;
        first = false;
    }

    float blockClip = 0.0f;
    const int L = lookaheadSamples;

    for (int i = 0; i < n; ++i)
    {
        const float A = rAttack.next (tAttack);
        const float S = rSustain.next (tSustain);
        const float mix = rMix.next (tMix);
        const float outG = rOut.next (tOut);
        const float ceil = rCeil.next (tCeil);
        const float la = rLook.next (tLook);

        if (coefCounter-- <= 0)
        {
            coefCounter = 15;
            splitSm += splitCoef * (tSplit - splitSm);
            if (std::abs (splitSm - splitSet) > 0.01f * splitSet || splitSet < 0.0f)
            {
                splitSet = splitSm;
                for (auto& s : split) s.set (splitSet, (float) sampleRate);
            }
        }

        // ---- split + detector (undelayed)
        float x[2], b[2];
        for (int c = 0; c < numCh; ++c)
        {
            x[c] = sanitize (ch[c][i]);
            float lp, hp;
            split[(size_t) c].tick (x[c], lp, hp);
            b[c] = band == Band::full ? x[c] : (band == Band::above ? hp : lp);
        }
        if (numCh == 1) { x[1] = x[0]; b[1] = b[0]; }
        const float det = std::max (std::abs (b[0]), std::abs (b[1])) + 1.0e-6f;

        eFast += (det > eFast ? aFast : rRel) * (det - eFast);
        eSlow += (det > eSlow ? aSlow : rRel) * (det - eSlow);
        eHold += (det > eHold ? aFast : rHold) * (det - eHold);
        eShort += (det > eShort ? aFast : rShort) * (det - eShort);

        float gTarget = 1.0f;
        float gDb = 0.0f;
        const bool neutral = (A == 0.0f && S == 0.0f);
        if (! neutral)
        {
            const float diffA = std::max (0.0f, dbPerLn * std::log (eFast / std::max (eSlow, 1.0e-9f)));
            const float diffS = std::max (0.0f, dbPerLn * std::log (eHold / std::max (eShort, 1.0e-9f)));
            gDb = A * (A > 0.0f ? 0.85f : 1.1f) * diffA + S * (S > 0.0f ? 0.7f : 1.4f) * diffS;
            {
                // leave the noise floor / silence alone (no pumping of the floor, no gain riding in silence)
                const float levelDb = dbPerLn * std::log (eShort);
                gDb *= clampf ((levelDb + 75.0f) / 15.0f, 0.0f, 1.0f);
            }
            gDb = clampf (gDb, maxCutDb, maxBoostDb);
            gTarget = std::exp (gDb / dbPerLn);
        }
        // lookahead: the applied gain is the most extreme target over the look window [0, 1.5 ms] ahead of the
        // (delayed) audio, so the curve starts early AND keeps its full depth
        gainLine[(size_t) gainPos] = gTarget;
        {
            float gDel = 1.0f, best = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                int r = gainPos - (L - gainTap[k]);
                if (r < 0) r += gainSize;
                const float gv = gainLine[(size_t) r];
                const float m = std::abs (std::log (std::max (gv, 1.0e-6f)));
                if (k == 0 || m > best) { best = m; gDel = gv; }
            }
            gainPos = gainPos + 1 >= gainSize ? 0 : gainPos + 1;
            if (la == 1.0f) gTarget = gDel;
            else if (la != 0.0f) gTarget += la * (gDel - gTarget);
        }
        gainSm += (la > 0.0f ? gainCoefLa : gainCoef) * (gTarget - gainSm);
        if (neutral && std::abs (gainSm - 1.0f) < 1.0e-6f) gainSm = 1.0f;

        // ---- lookahead delay (x and band per channel, interleaved)
        float xd[2], bd[2];
        const int rd = delayPos - L < 0 ? delayPos - L + delaySize : delayPos - L;
        for (int c = 0; c < 2; ++c)
        {
            auto& d = delay[(size_t) c];
            d[(size_t) delayPos * 2] = x[c];
            d[(size_t) delayPos * 2 + 1] = b[c];
            if (la == 0.0f) { xd[c] = x[c]; bd[c] = b[c]; }
            else
            {
                const float dx = d[(size_t) rd * 2], db = d[(size_t) rd * 2 + 1];
                if (la == 1.0f) { xd[c] = dx; bd[c] = db; }
                else { xd[c] = x[c] + la * (dx - x[c]); bd[c] = b[c] + la * (db - b[c]); }
            }
        }
        delayPos = delayPos + 1 >= delaySize ? 0 : delayPos + 1;

        float peakIn = 0.0f, peakOut = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            float s = xd[c];
            if (gainSm != 1.0f) s += (gainSm - 1.0f) * bd[c];
            if (p.clip)
            {
                const float cl = softClip (s, ceil);
                blockClip = std::max (blockClip, std::abs (s) - std::abs (cl));
                s = cl;
            }
            float y = mix == 1.0f ? s : (mix == 0.0f ? xd[c] : xd[c] + mix * (s - xd[c]));
            if (outG != 1.0f) y *= outG;
            ch[c][i] = y;
            peakIn = std::max (peakIn, std::abs (xd[c]));
            peakOut = std::max (peakOut, std::abs (y));
        }

        // ---- display history
        accIn = std::max (accIn, peakIn);
        accOut = std::max (accOut, peakOut);
        const float gd = gainSm == 1.0f ? 0.0f : dbPerLn * std::log (std::max (gainSm, 1.0e-6f));
        if (std::abs (gd) > std::abs (accGain)) accGain = gd;
        if (++histCount >= histHop)
        {
            const int w = (histWrite.load (std::memory_order_relaxed) + 1) % historySize;
            histIn[(size_t) w].store (accIn, std::memory_order_relaxed);
            histOut[(size_t) w].store (accOut, std::memory_order_relaxed);
            histGain[(size_t) w].store (accGain, std::memory_order_relaxed);
            histWrite.store (w, std::memory_order_release);
            histCount = 0;
            accIn = accOut = accGain = 0.0f;
        }
    }

    gainDbNow = gainSm == 1.0f ? 0.0f : dbPerLn * std::log (std::max (gainSm, 1.0e-6f));
    clipPeak = std::max (blockClip, clipPeak * 0.85f);
}

} // namespace snapfx
