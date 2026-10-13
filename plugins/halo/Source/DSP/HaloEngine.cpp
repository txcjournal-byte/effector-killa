#include "HaloEngine.h"

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define HALO_HAS_SSE 1
#endif

namespace halo
{

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr int kControlChunk = 16;

// mutually incommensurate line lengths (ms at size scale 1)
constexpr float kLineMs[Engine::numLines] = { 29.7f, 37.1f, 41.9f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f };
// in-loop allpass lengths (ms, fixed)
constexpr float kLoopApMs[Engine::numLines] = { 5.1f, 6.7f, 3.9f, 7.3f, 4.3f, 8.1f, 5.7f, 6.3f };
// input diffusers (ms)
constexpr float kDiffMsL[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
constexpr float kDiffMsR[4] = { 4.93f, 3.73f, 13.17f, 8.97f };
// LFO rates (Hz): spread so the lines never move together
constexpr float kLfoHz[Engine::numLines] = { 0.113f, 0.171f, 0.229f, 0.293f, 0.367f, 0.431f, 0.523f, 0.617f };
// input injection signs
constexpr float kInSign[Engine::numLines] = { 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f };

constexpr float kMaxScale = 2.2f;
constexpr float kMaxModMs = 1.1f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

#if HALO_HAS_SSE
struct FlushDenormals
{
    unsigned int old;
    FlushDenormals() : old (_mm_getcsr()) { _mm_setcsr (old | 0x8040); }   // FTZ | DAZ
    ~FlushDenormals() { _mm_setcsr (old); }
};
#else
struct FlushDenormals {};
#endif
} // namespace

float Engine::sizeToScale (float size)
{
    size = clampf (size, 0.0f, 1.0f);
    return 0.22f + (kMaxScale - 0.22f) * std::pow (size, 1.5f);
}

// ---------------------------------------------------------------- delay line

void Engine::ModDelay::init (int minLength)
{
    int n = 16;
    while (n < minLength) n <<= 1;
    buf.assign ((size_t) n, 0.0f);
    mask = n - 1;
    w = 0;
}

inline float Engine::ModDelay::readCubic (float delay) const
{
    // read BEFORE write: the newest sample is at w-1; 3rd-order Lagrange on points k-1..k+2 (passive, |H| <= 1)
    const float pos = (float) w - delay;
    const float fl = std::floor (pos);
    const float f = pos - fl;
    const int k = (int) fl;
    const float xm1 = buf[(size_t) ((k - 1) & mask)], x0 = buf[(size_t) (k & mask)];
    const float x1 = buf[(size_t) ((k + 1) & mask)], x2 = buf[(size_t) ((k + 2) & mask)];
    const float fm1 = f - 1.0f, fm2 = f - 2.0f, fp1 = f + 1.0f;
    const float c0 = -f * fm1 * fm2 * (1.0f / 6.0f);
    const float c1 = fp1 * fm1 * fm2 * 0.5f;
    const float c2 = -fp1 * f * fm2 * 0.5f;
    const float c3 = fp1 * f * fm1 * (1.0f / 6.0f);
    return c0 * xm1 + c1 * x0 + c2 * x1 + c3 * x2;
}

inline float Engine::ModDelay::readLinear (float delay) const
{
    // read AFTER write: delay 0 = newest sample
    const float pos = (float) (w - 1) - delay;
    const float fl = std::floor (pos);
    const float f = pos - fl;
    const int k = (int) fl;
    const float a = buf[(size_t) (k & mask)], b = buf[(size_t) ((k + 1) & mask)];
    return a + f * (b - a);
}

// ---------------------------------------------------------------- lifecycle

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    const float msToS = (float) sr * 0.001f;

    preL.init ((int) (maxPredelayMs * msToS) + 8);
    preR.init ((int) (maxPredelayMs * msToS) + 8);
    for (int j = 0; j < 4; ++j)
    {
        diffL[(size_t) j].init ((int) std::lround (kDiffMsL[j] * msToS));
        diffR[(size_t) j].init ((int) std::lround (kDiffMsR[j] * msToS));
    }
    for (int i = 0; i < numLines; ++i)
    {
        baseDelay[(size_t) i] = kLineMs[i] * msToS;
        lines[(size_t) i].init ((int) (kLineMs[i] * kMaxScale * msToS + kMaxModMs * 2.0f * msToS) + 16);
        loopAp[(size_t) i].init ((int) std::lround (kLoopApMs[i] * msToS));
        const float w = 2.0f * kPi * kLfoHz[i] / (float) sr;
        lfoRotS[(size_t) i] = std::sin (w);
        lfoRotC[(size_t) i] = std::cos (w);
    }

    preSamples.setTime (sr, 0.12);
    diffCoef.setTime (sr, 0.05);
    sizeScale.setTime (sr, 0.25);
    modDepth.setTime (sr, 0.10);
    dampCoef.setTime (sr, 0.03);
    inGain.setTime (sr, 0.03);
    loopApCoef.setTime (sr, 0.05);
    for (auto& g : gain) g.setTime (sr, 0.03);
    widthS.setTime (sr, 0.03);
    dryS.setTime (sr, 0.03);
    wetS.setTime (sr, 0.03);
    outS.setTime (sr, 0.03);
    duckGain.setTime (sr, 0.012);
    duckAtt = (float) (1.0 - std::exp (-1.0 / (0.003 * sr)));
    duckRel = (float) (1.0 - std::exp (-1.0 / (0.07 * sr)));
    duckUp = (float) (1.0 - std::exp (-1.0 / (0.16 * sr)));

    prepared = true;
    reset();
}

void Engine::reset()
{
    if (! prepared) return;
    preL.clear(); preR.clear();
    inLpL.clear(); inLpR.clear(); hpL.clear(); hpR.clear();
    for (auto& a : diffL) a.clear();
    for (auto& a : diffR) a.clear();
    for (auto& l : lines) l.clear();
    for (auto& a : loopAp) a.clear();
    damp.fill (0.0f);
    for (int i = 0; i < numLines; ++i)
    {
        // start the LFOs at spread phases
        const float ph = 2.0f * kPi * (float) i / (float) numLines;
        lfoS[(size_t) i] = std::sin (ph);
        lfoC[(size_t) i] = std::cos (ph);
    }
    duckEnv = 0.0f;
    inLevel = wetLevel = 0.0f;

    // jump every smoother to its target
    sizeScale.snap (sizeToScale (target.size));
    updateControl();
    for (Smooth* s : { &preSamples, &diffCoef, &sizeScale, &modDepth, &dampCoef, &inGain, &loopApCoef,
                       &widthS, &dryS, &wetS, &outS, &duckGain })
        s->snap (s->t);
    for (auto& g : gain) g.snap (g.t);
    duckGainNow = duckGain.v;
    controlCounter = kControlChunk;
}

// ---------------------------------------------------------------- control rate

void Engine::updateControl()
{
    const Params& p = target;
    const float fs = (float) sr;
    const bool frozen = p.freeze;

    preSamples.t = clampf (p.predelayMs, 0.0f, maxPredelayMs) * fs * 0.001f;
    diffCoef.t = 0.72f * clampf (p.diffusion, 0.0f, 1.0f);
    loopApCoef.t = 0.10f + 0.50f * clampf (p.diffusion, 0.0f, 1.0f);
    sizeScale.t = sizeToScale (p.size);
    modDepth.t = clampf (p.mod, 0.0f, 1.0f) * kMaxModMs * 0.001f * fs;

    const float hiCut = clampf (p.hiCutHz, 200.0f, 20000.0f);
    const float lowCut = clampf (p.lowCutHz, 10.0f, 2000.0f);
    dampCoef.t = frozen ? 0.0f : std::exp (-2.0f * kPi * std::min (hiCut * 1.6f, 0.49f * fs) / fs);
    lpG = std::tan (kPi * std::min (hiCut, 0.45f * fs) / fs);
    hpG = std::tan (kPi * std::min (lowCut, 0.45f * fs) / fs);
    inGain.t = frozen ? 0.0f : 1.0f;

    const float decay = clampf (p.decaySec, minDecay, maxDecay);
    const float scale = sizeScale.v;
    for (int i = 0; i < numLines; ++i)
    {
        const float leff = baseDelay[(size_t) i] * scale + (float) loopAp[(size_t) i].len;
        gain[(size_t) i].t = frozen ? 1.0f : std::pow (10.0f, -3.0f * leff / (decay * fs));
    }

    // level: steady-state wet energy grows ~ with decay / mean line length; normalise most of that away
    const float meanLen = (51.25f * scale + 5.9f) * 0.001f;     // s
    wetNorm = 0.62f / std::sqrt (1.0f + 0.35f * std::min (decay, 12.0f) / (meanLen * 30.0f));

    widthS.t = clampf (p.width, 0.0f, 1.5f);
    const float mix = clampf (p.mix, 0.0f, 1.0f);
    dryS.t = mix <= 0.0f ? 1.0f : (mix >= 1.0f ? 0.0f : std::cos (mix * kPi * 0.5f));
    wetS.t = mix <= 0.0f ? 0.0f : (mix >= 1.0f ? 1.0f : std::sin (mix * kPi * 0.5f));
    outS.t = std::pow (10.0f, clampf (p.outputDb, -60.0f, 24.0f) / 20.0f);

    // ducking: up to -26 dB of wet reduction while the dry input is loud
    const float duck = clampf (p.duck, 0.0f, 1.0f);
    if (duck <= 0.0f)
        duckGain.t = 1.0f;
    else
    {
        const float envDb = 20.0f * std::log10 (duckEnv + 1.0e-9f);
        const float amt = clampf ((envDb + 42.0f) / 30.0f, 0.0f, 1.0f);
        duckGain.t = std::pow (10.0f, -26.0f * duck * amt / 20.0f);
    }
}

// ---------------------------------------------------------------- audio

void Engine::process (float* const* ch, int numCh, int numSamples)
{
    if (! prepared || numCh <= 0 || numSamples <= 0) return;
    FlushDenormals ftz;
    (void) ftz;

    float* outL = ch[0];
    float* outR = numCh > 1 ? ch[1] : nullptr;
    constexpr float k = 1.41421356f;   // SVF damping for Q = 0.707
    constexpr float hScale = 0.35355339f;
    float peakIn = 0.0f, peakWet = 0.0f;
    float v[numLines];

    for (int n = 0; n < numSamples; ++n)
    {
        if (--controlCounter <= 0)
        {
            updateControl();
            controlCounter = kControlChunk;
        }

        const float xl = outL[n];
        const float xr = outR != nullptr ? outR[n] : xl;

        // ---- ducking envelope (dry input)
        const float a = std::max (std::abs (xl), std::abs (xr));
        duckEnv += (a > duckEnv ? duckAtt : duckRel) * (a - duckEnv);
        peakIn = std::max (peakIn, a);

        // ---- pre-delay
        preL.write (xl);
        preR.write (xr);
        const float pd = preSamples.next();
        float pl = preL.readLinear (pd), pr = preR.readLinear (pd);

        // ---- hi-cut + diffusion
        pl = inLpL.lp (pl, lpG, k);
        pr = inLpR.lp (pr, lpG, k);
        const float dg = diffCoef.next();
        for (int j = 0; j < 4; ++j)
        {
            pl = diffL[(size_t) j].process (pl, dg);
            pr = diffR[(size_t) j].process (pr, dg);
        }
        const float ig = inGain.next() * 0.5f;

        // ---- tank
        const float scale = sizeScale.next();
        const float depth = modDepth.next();
        const float dc = dampCoef.next();
        const float apg = loopApCoef.next();
        for (int i = 0; i < numLines; ++i)
        {
            const size_t u = (size_t) i;
            const float s = lfoS[u] * lfoRotC[u] + lfoC[u] * lfoRotS[u];
            lfoC[u] = lfoC[u] * lfoRotC[u] - lfoS[u] * lfoRotS[u];
            lfoS[u] = s;
            const float dly = baseDelay[u] * scale + depth * (1.0f + s) + 4.0f;
            float y = lines[u].readCubic (dly);
            y = loopAp[u].process (y, apg);
            damp[u] = y + dc * (damp[u] - y);
            v[i] = damp[u] * gain[u].next();
        }

        float wl = v[0] - v[1] + v[2] - v[3] + v[4] - v[5] + v[6] - v[7];
        float wr = v[0] + v[1] - v[2] - v[3] + v[4] + v[5] - v[6] - v[7];

        // 8x8 Hadamard (fast Walsh-Hadamard, orthonormal -> lossless)
        for (int h = 1; h < numLines; h <<= 1)
            for (int i = 0; i < numLines; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float p0 = v[j], p1 = v[j + h];
                    v[j] = p0 + p1;
                    v[j + h] = p0 - p1;
                }
        denormFlip = -denormFlip;
        for (int i = 0; i < numLines; ++i)
            lines[(size_t) i].write (v[i] * hScale + ig * kInSign[i] * ((i & 1) ? pr : pl) + denormFlip);

        // ---- output: low-cut, width, ducking, mix
        wl = hpL.hp (wl * wetNorm, hpG, k);
        wr = hpR.hp (wr * wetNorm, hpG, k);
        const float wd = widthS.next();
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * wd;
        wl = mid + side;
        wr = mid - side;
        peakWet = std::max (peakWet, std::max (std::abs (wl), std::abs (wr)));

        // ducking gain: fast to duck (12 ms), smooth bloom back (160 ms)
        if (duckGain.v > duckGain.t) duckGain.next();
        else duckGain.v = std::min (duckGain.t, duckGain.v + duckUp * (duckGain.t - duckGain.v) + 1.0e-7f);
        const float duckG = duckGain.v;
        const float wetG = wetS.next() * duckG;
        const float dryG = dryS.next();
        const float og = outS.next();
        if (outR != nullptr)
        {
            outL[n] = (xl * dryG + wl * wetG) * og;
            outR[n] = (xr * dryG + wr * wetG) * og;
        }
        else
            outL[n] = (xl * dryG + (wl + wr) * 0.70710678f * wetG) * og;
    }

    // keep the LFO rotors on the unit circle
    if ((lfoRenorm += numSamples) > 8192)
    {
        lfoRenorm = 0;
        for (int i = 0; i < numLines; ++i)
        {
            const float m = 1.0f / std::sqrt (lfoS[(size_t) i] * lfoS[(size_t) i] + lfoC[(size_t) i] * lfoC[(size_t) i]);
            lfoS[(size_t) i] *= m;
            lfoC[(size_t) i] *= m;
        }
    }

    // safety net: never let a non-finite value live in the tank
    bool finite = std::isfinite (peakWet);
    for (float d : damp) finite = finite && std::isfinite (d);
    if (! finite)
    {
        reset();
        for (int n = 0; n < numSamples; ++n)
        {
            if (! std::isfinite (outL[n])) outL[n] = 0.0f;
            if (outR != nullptr && ! std::isfinite (outR[n])) outR[n] = 0.0f;
        }
        peakWet = 0.0f;
    }

    duckGainNow = duckGain.v;
    inLevel = std::max (peakIn, inLevel * 0.8f);
    wetLevel = std::max (peakWet, wetLevel * 0.8f);
}

} // namespace halo
