#include "WideEngine.h"

#include <algorithm>
#include <cmath>

namespace widefx
{

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr int subBlock = 32;
inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// stereoize all-pass cascade: corner frequencies (Hz) and Qs, spread so L/R swing several times across the spectrum
constexpr float decorHz[6] = { 330.0f, 760.0f, 1650.0f, 3300.0f, 6400.0f, 11500.0f };
constexpr float decorQ[6] = { 0.55f, 0.7f, 0.6f, 0.75f, 0.6f, 0.7f };
} // namespace

void SVF::set (double fc, double rate, double q)
{
    fc = std::min (fc, rate * 0.45);
    const double g = std::tan (PI * fc / rate);
    k = (float) (1.0 / q);
    const double a = 1.0 / (1.0 + g * (g + 1.0 / q));
    a1 = (float) a;
    a2 = (float) (g * a);
    a3 = (float) (g * g * a);
}

bool Engine::isNeutral (const Params& p)
{
    constexpr float eps = 1.0e-4f;   // host automation may deliver 0.99999994 for 100 %
    return std::abs (p.widthLow - 1.0f) < eps && std::abs (p.widthMid - 1.0f) < eps && std::abs (p.widthHigh - 1.0f) < eps
        && std::abs (p.balance) < eps && p.stereoize < eps && p.haasMs < 1.0e-3f && p.monoBelowHz <= monoOffHz;
}

float Engine::correlationOf (double sumLR, double sumLL, double sumRR)
{
    const double den = std::sqrt (sumLL * sumRR);
    if (den < 1.0e-14) return 0.0f;
    return (float) std::max (-1.0, std::min (1.0, sumLR / den));
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    pCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
    xfStep = (float) (1.0 / (0.03 * sr));
    fCoef = (float) (1.0 - std::exp (-(double) subBlock / (0.03 * sr)));
    corrCoef = (float) (1.0 - std::exp (-1.0 / (0.3 * sr)));
    int size = 1;
    while (size < (int) std::ceil (sr * (maxHaasMs + 2.0) / 1000.0) + 4) size <<= 1;
    for (auto& l : line) l.assign ((size_t) size, 0.0f);
    lineMask = size - 1;
    scopeDecim = std::max (1, (int) std::lround (sr / 48000.0));
    for (size_t i = 0; i < decor.size(); ++i) decor[i].set (decorHz[i], sr, decorQ[i]);
    decorHp.set (220.0, sr, 0.7071);
    reset();
}

void Engine::reset()
{
    for (auto* f : allFilters()) f->reset();
    for (auto& f : decor) f.reset();
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0f);
    writePos = 0;
    // jump to the targets
    wS[0] = target.widthLow; wS[1] = target.widthMid; wS[2] = target.widthHigh;
    const float bal = clampf (target.balance, -1.0f, 1.0f);
    gmS = bal > 0.0f ? 1.0f - bal : 1.0f;
    gsS = bal < 0.0f ? 1.0f + bal : 1.0f;
    stS = clampf (target.stereoize, 0.0f, 1.0f);
    outS = std::abs (target.outputDb) < 1.0e-6f ? 1.0f : dbToGain (target.outputDb);
    monoFade = target.monoBelowHz > monoOffHz ? 1.0f : 0.0f;
    const float hs = (float) ((double) clampf (target.haasMs, 0.0f, maxHaasMs) * sr / 1000.0);
    dS[0] = target.haasSide == HaasSide::left ? hs : 0.0f;
    dS[1] = target.haasSide == HaasSide::right ? hs : 0.0f;
    lowXS = clampf (target.lowXHz, 40.0f, 1000.0f);
    highXS = clampf (target.highXHz, 1000.0f, 12000.0f);
    monoS = clampf (target.monoBelowHz > monoOffHz ? target.monoBelowHz : 120.0f, 20.0f, 500.0f);
    lowXCur = highXCur = monoCur = -1.0f;
    xf = isNeutral (target) ? 0.0f : 1.0f;
    updateFilters();
    eLR = eLL = eRR = 0.0;
    bandPow.fill (0.0);
    scopeCount = 0;
}

void Engine::updateFilters()
{
    // keep the crossovers at least an octave apart
    const float hx = std::max (highXS, lowXS * 2.0f);
    constexpr double bw = 0.70710678;
    if (std::abs (lowXS - lowXCur) > lowXCur * 0.002f)
    {
        lowXCur = lowXS;
        for (auto* f : { &aLp1, &aLp2, &aHp1, &aHp2, &midApA }) f->set (lowXS, sr, bw);
    }
    if (std::abs (hx - highXCur) > highXCur * 0.002f)
    {
        highXCur = hx;
        for (auto* f : { &bLp1, &bLp2, &bHp1, &bHp2, &lowApB, &midApB }) f->set (hx, sr, bw);
    }
    if (std::abs (monoS - monoCur) > monoCur * 0.002f)
    {
        monoCur = monoS;
        monoApM.set (monoS, sr, 0.70710678);
        monoHp1.set (monoS, sr, 0.70710678);
        monoHp2.set (monoS, sr, 0.70710678);
    }
}

int Engine::copyScope (float* l, float* r, int n) const
{
    n = std::min (n, scopeSize);
    const int w = scopeWrite.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
    {
        const int idx = (w - n + i + scopeSize * 2) % scopeSize;
        l[i] = scopeL[(size_t) idx].load (std::memory_order_relaxed);
        r[i] = scopeR[(size_t) idx].load (std::memory_order_relaxed);
    }
    return n;
}

void Engine::process (float* const* ch, int numCh, int n)
{
    if (numCh <= 0 || n <= 0) return;
    numCh = std::min (numCh, 2);
    float* L = ch[0];
    float* R = numCh > 1 ? ch[1] : ch[0];

    const float outT = std::abs (target.outputDb) < 1.0e-6f ? 1.0f : dbToGain (clampf (target.outputDb, -60.0f, 24.0f));

    // ---- mono bus: nothing to image, only the output gain
    if (numCh == 1)
    {
        for (int i = 0; i < n; ++i)
        {
            outS += pCoef * (outT - outS);
            if (std::abs (outT - outS) < 1.0e-6f) outS = outT;
            if (outS != 1.0f) L[i] *= outS;
        }
        meterCorr.store (1.0f, std::memory_order_relaxed);
        return;
    }

    const float wT[3] = { clampf (target.widthLow, 0.0f, 2.0f), clampf (target.widthMid, 0.0f, 2.0f), clampf (target.widthHigh, 0.0f, 2.0f) };
    const float bal = clampf (target.balance, -1.0f, 1.0f);
    const float gmT = bal > 0.0f ? 1.0f - bal : 1.0f;
    const float gsT = bal < 0.0f ? 1.0f + bal : 1.0f;
    const float stT = clampf (target.stereoize, 0.0f, 1.0f);
    const bool monoOn = target.monoBelowHz > monoOffHz;
    const float monoT = monoOn ? 1.0f : 0.0f;
    const float hs = (float) ((double) clampf (target.haasMs, 0.0f, maxHaasMs) * sr / 1000.0);
    const float dT[2] = { target.haasSide == HaasSide::left ? hs : 0.0f, target.haasSide == HaasSide::right ? hs : 0.0f };
    const float hCoef = (float) (1.0 - std::exp (-1.0 / (0.06 * sr)));

    const float xfT = isNeutral (target) ? 0.0f : 1.0f;

    auto smooth = [] (float& v, float t, float c, float eps)
    {
        v += c * (t - v);
        if (std::abs (t - v) < eps) v = t;
    };

    float peak[2] = { 0, 0 };
    double bp[3] = { 0, 0, 0 };

    for (int start = 0; start < n; start += subBlock)
    {
        const int len = std::min (subBlock, n - start);
        // crossovers / mono-below frequency glide in the log domain
        lowXS *= std::pow (clampf (target.lowXHz, 40.0f, 1000.0f) / lowXS, fCoef);
        highXS *= std::pow (clampf (target.highXHz, 1000.0f, 12000.0f) / highXS, fCoef);
        if (monoOn)
        {
            if (monoFade <= 0.0f) monoS = clampf (target.monoBelowHz, 20.0f, 500.0f);   // fading in: start at the target
            else monoS *= std::pow (clampf (target.monoBelowHz, 20.0f, 500.0f) / monoS, fCoef);
        }
        updateFilters();

        for (int i = start; i < start + len; ++i)
        {
            for (int b = 0; b < 3; ++b) smooth (wS[b], wT[b], pCoef, 1.0e-6f);
            smooth (gmS, gmT, pCoef, 1.0e-6f);
            smooth (gsS, gsT, pCoef, 1.0e-6f);
            smooth (stS, stT, pCoef, 1.0e-6f);
            smooth (monoFade, monoT, pCoef, 1.0e-5f);
            smooth (outS, outT, pCoef, 1.0e-6f);
            smooth (dS[0], dT[0], hCoef, 1.0e-3f);
            smooth (dS[1], dT[1], hCoef, 1.0e-3f);
            xf = xfT > xf ? std::min (xfT, xf + xfStep) : std::max (xfT, xf - xfStep);

            const float l = L[i], r = R[i];
            const float M = 0.5f * (l + r), S = 0.5f * (l - r);

            // side: LR4 tree, low band all-pass compensated for the high crossover
            const float sLow = lowApB.ap (aLp2.lp (aLp1.lp (S)));
            const float hi1 = aHp2.hp (aHp1.hp (S));
            const float sMid = bLp2.lp (bLp1.lp (hi1));
            const float sHigh = bHp2.hp (bHp1.hp (hi1));
            const float mA = midApB.ap (midApA.ap (M));
            bp[0] += (double) (wS[0] * sLow) * (wS[0] * sLow);
            bp[1] += (double) (wS[1] * sMid) * (wS[1] * sMid);
            bp[2] += (double) (wS[2] * sHigh) * (wS[2] * sHigh);
            float side = wS[0] * sLow + wS[1] * sMid + wS[2] * sHigh;

            // stereoize: decorrelated mid copy into the side (mono sum unchanged)
            float d = M;
            for (auto& f : decor) d = f.ap (d);
            side += 0.8f * stS * decorHp.hp (d);

            // M/S balance
            const float mid = gmS * mA;
            side *= gsS;
            float lo = mid + side;
            float ro = mid - side;

            // Haas (both channels have a line; the unused one sits at 0 samples)
            line[0][(size_t) writePos] = lo;
            line[1][(size_t) writePos] = ro;
            auto tap = [this] (int c, float delay, float now)
            {
                if (delay <= 0.0f) return now;
                const float pos = (float) writePos - delay;
                const float fl = std::floor (pos);
                const float fr = pos - fl;
                const int i0 = (int) fl & lineMask;
                const int i1 = (i0 + 1) & lineMask;
                const auto& v = line[(size_t) c];
                return v[(size_t) i0] + fr * (v[(size_t) i1] - v[(size_t) i0]);
            };
            lo = tap (0, dS[0], lo);
            ro = tap (1, dS[1], ro);
            writePos = (writePos + 1) & lineMask;

            // mono below: phase-coherent (LR4) – lows become mono, everything above keeps its image
            const float m2 = 0.5f * (lo + ro), s2 = 0.5f * (lo - ro);
            const float mAp = monoApM.ap (m2);
            const float sH = monoHp2.hp (monoHp1.hp (s2));
            if (monoFade > 0.0f)
            {
                lo += monoFade * ((mAp + sH) - lo);
                ro += monoFade * ((mAp - sH) - ro);
            }

            // blend with the untouched input (xf = 0 -> bit-exact), then the output gain
            float ol = l, orr = r;
            if (xf > 0.0f)
            {
                ol = l + xf * (lo - l);
                orr = r + xf * (ro - r);
            }
            if (outS != 1.0f) { ol *= outS; orr *= outS; }
            L[i] = ol;
            R[i] = orr;
        }
    }

    // ---- meters (on the output)
    for (int i = 0; i < n; ++i)
    {
        const double l = L[i], r = R[i];
        eLR += corrCoef * (l * r - eLR);
        eLL += corrCoef * (l * l - eLL);
        eRR += corrCoef * (r * r - eRR);
        peak[0] = std::max (peak[0], (float) std::abs (l));
        peak[1] = std::max (peak[1], (float) std::abs (r));
        if (++scopeCount >= scopeDecim)
        {
            scopeCount = 0;
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scopeL[(size_t) w].store ((float) l, std::memory_order_relaxed);
            scopeR[(size_t) w].store ((float) r, std::memory_order_relaxed);
            scopeWrite.store ((w + 1) % scopeSize, std::memory_order_release);
        }
    }
    if (eLL < 1.0e-30) eLL = 0.0;
    if (eRR < 1.0e-30) eRR = 0.0;
    if (std::abs (eLR) < 1.0e-30) eLR = 0.0;
    meterCorr.store (correlationOf (eLR, eLL, eRR), std::memory_order_relaxed);
    for (int c = 0; c < 2; ++c) meterPeak[(size_t) c].store (peak[c], std::memory_order_relaxed);
    for (int b = 0; b < 3; ++b)
    {
        const double p = bp[b] / (double) n;
        bandPow[(size_t) b] += 0.5 * (p - bandPow[(size_t) b]);
        meterBand[(size_t) b].store ((float) (10.0 * std::log10 (bandPow[(size_t) b] + 1.0e-12)), std::memory_order_relaxed);
    }

    for (auto* f : allFilters()) f->flush();
    for (auto& f : decor) f.flush();
}

} // namespace widefx
