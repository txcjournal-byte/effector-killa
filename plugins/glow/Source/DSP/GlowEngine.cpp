#include "GlowEngine.h"

#include <algorithm>
#include <cmath>

namespace glowfx
{

namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr int subBlock = 32;
constexpr float ln10over20 = 0.11512925465f;

inline float dbToGain (float db) { return std::exp (db * ln10over20); }
inline float powToDb (float p) { return 4.342944819f * std::log (p + 1.0e-15f); } // 10·log10
inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

// rational tanh approximation (exact limits, smooth, < 2.5 % error), clamps beyond |x| = 3
inline float softTanh (float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float onePole (double timeSeconds, double rate)
{
    return (float) std::exp (-1.0 / std::max (1.0, timeSeconds * rate));
}
} // namespace

Voice voiceFor (Mode m)
{
    switch (m)
    {
        case Mode::bright:
            //        up                 down               eq dB
            return { { 0.20f, 0.55f, 0.95f }, { 0.20f, 0.35f, 0.25f }, { -0.8f, 0.6f, 3.2f },
                     0.14f, 0.10f, 0.00f, 0.70f, 4200.0f, 1.0f, 0.55f, -0.3f };
        case Mode::fat:
            return { { 0.95f, 0.45f, 0.15f }, { 0.30f, 0.25f, 0.20f }, { 2.6f, -0.6f, -0.4f },
                     0.32f, 0.60f, 0.85f, 0.06f, 7000.0f, 1.5f, 0.70f, -0.3f };
        case Mode::loud:
            return { { 0.70f, 0.80f, 0.70f }, { 0.55f, 0.55f, 0.45f }, { 1.0f, 0.4f, 1.6f },
                     0.42f, 0.22f, 0.25f, 0.28f, 5000.0f, 6.0f, 1.00f, -0.3f };
        case Mode::warm:
        default:
            return { { 0.55f, 0.60f, 0.30f }, { 0.25f, 0.30f, 0.30f }, { 1.4f, 0.3f, -1.0f },
                     0.40f, 0.45f, 0.18f, 0.10f, 6500.0f, 1.0f, 0.60f, -0.3f };
    }
}

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

void KMeter::prepare (double rate, double tau)
{
    for (auto& f : pre) f.set (60.0, rate, 0.5);
    for (auto& f : shelf) f.set (1500.0, rate, 0.7071);
    coef = (float) (1.0 - std::exp (-1.0 / (tau * rate)));
    reset();
}

void KMeter::reset()
{
    ms = 0.0f;
    for (auto& f : pre) f.reset();
    for (auto& f : shelf) f.reset();
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    for (auto& c : chans)
    {
        c.lowSplit.set (lowSplitHz, sr);
        c.midSplit.set (highSplitHz, sr);
        c.fatLp1.set (140.0, sr);
        c.fatLp2.set (140.0, sr);
        c.fatHp.set (45.0, sr);
        c.fatLpOut.set (520.0, sr);
    }
    bandAtk = onePole (0.004, sr);
    bandRel = onePole (0.160, sr);
    fastAtk = onePole (0.0005, sr);
    fastRel = onePole (0.030, sr);
    slowAtk = onePole (0.025, sr);
    slowRel = onePole (0.300, sr);
    limRel = onePole (0.080, sr);
    dcCoef = onePole (1.0 / (2.0 * PI * 8.0), sr);
    ctlCoef = (float) (1.0 - std::exp (-(double) subBlock / (0.05 * sr)));
    agStep = (float) ((double) subBlock / (0.9 * sr));
    agReturn = (float) (1.0 - std::exp (-(double) subBlock / (0.25 * sr)));
    inMeter.prepare (sr, 0.4);
    outMeter.prepare (sr, 0.4);
    reset();
}

void Engine::reset()
{
    for (auto& c : chans)
    {
        for (auto* f : { &c.lowSplit, &c.midSplit, &c.fatLp1, &c.fatLp2, &c.fatHp, &c.fatLpOut, &c.exHp1, &c.exHp2, &c.exHpOut })
            f->reset();
        c.dcX = c.dcY = 0.0f;
    }
    bandEnv.fill (0.0f);
    fastEnv = slowEnv = limEnv = inPkEnv = 0.0f;
    inMeter.reset();
    outMeter.reset();
    agDb = 0.0f;
    // jump straight to the target settings (no ramp after a reset)
    glowS = std::min (1.0f, std::max (0.0f, target.glow));
    outS = dbToGain (target.outputDb);
    if (std::abs (target.outputDb) < 1.0e-6f) outS = 1.0f;
    contrastS = target.contrast;
    colourS = target.colour;
    voiceS = voiceFor (target.mode);
    exFreqCur = -1.0f;
    setExciter (voiceS.exFreq);
    for (auto& m : meterLift) m.store (0.0f);
}

void Engine::setExciter (float hz)
{
    hz = clampf (hz, 1500.0f, (float) std::min (12000.0, sr * 0.4));
    if (std::abs (hz - exFreqCur) < 1.0f) return;
    exFreqCur = hz;
    for (auto& c : chans)
    {
        c.exHp1.set (hz, sr);
        c.exHp2.set (hz, sr);
        c.exHpOut.set (hz * 0.85, sr);
    }
}

void Engine::updateControls (int)
{
    const Voice t = voiceFor (target.mode);
    auto sm = [this] (float& v, float to) { v += ctlCoef * (to - v); if (std::abs (to - v) < 1.0e-5f) v = to; };
    for (int b = 0; b < 3; ++b)
    {
        sm (voiceS.up[b], t.up[b]);
        sm (voiceS.down[b], t.down[b]);
        sm (voiceS.eqDb[b], t.eqDb[b]);
    }
    sm (voiceS.sat, t.sat);
    sm (voiceS.satBias, t.satBias);
    sm (voiceS.fat, t.fat);
    sm (voiceS.exciter, t.exciter);
    sm (voiceS.pushDb, t.pushDb);
    sm (voiceS.limit, t.limit);
    sm (voiceS.ceilingDb, t.ceilingDb);
    voiceS.exFreq *= std::pow (t.exFreq / voiceS.exFreq, ctlCoef);
    sm (contrastS, clampf (target.contrast, -1.0f, 1.0f));
    sm (colourS, clampf (target.colour, -1.0f, 1.0f));
    setExciter (voiceS.exFreq * std::pow (2.0f, -0.6f * colourS));
}

void Engine::process (float* const* ch, int numCh, int n)
{
    numCh = std::min (numCh, 2);
    if (numCh <= 0 || n <= 0) return;

    const float glowT = clampf (target.glow, 0.0f, 1.0f);
    const float outT = std::abs (target.outputDb) < 1.0e-6f ? 1.0f : dbToGain (clampf (target.outputDb, -60.0f, 24.0f));
    const float pCoef = (float) (1.0 - std::exp (-1.0 / (0.03 * sr)));
    const float invCh = 1.0f / (float) numCh;

    float liftSum[3] = { 0, 0, 0 };
    float grMin = 1.0f, inPk = 0.0f, outPk = 0.0f;

    for (int start = 0; start < n; start += subBlock)
    {
        const int len = std::min (subBlock, n - start);
        updateControls (len);

        const Voice& v = voiceS;
        const float ceiling = dbToGain (v.ceilingDb);
        const float upScale = 1.0f - 0.35f * contrastS;
        const float tilt[3] = { -2.5f * colourS, 0.0f, 4.0f * colourS };
        // feed-forward part of the auto-gain: undo the limiter push immediately, the loop trims the rest
        const float ffDb = target.autoGain ? -0.8f * v.pushDb * glowS : 0.0f;
        const float agGain = dbToGain (agDb + ffDb);
        float inAcc = 0.0f, outAcc = 0.0f;

        for (int i = start; i < start + len; ++i)
        {
            // per-sample smoothing of the big knob and the output gain
            glowS += pCoef * (glowT - glowS);
            if (std::abs (glowT - glowS) < 1.0e-6f) glowS = glowT;
            outS += pCoef * (outT - outS);
            if (std::abs (outT - outS) < 1.0e-6f) outS = outT;

            const float amt = glowS;
            const float fade = std::min (1.0f, amt * 25.0f);
            const float cAmt = std::min (1.0f, amt * 2.5f);

            float x[2], low[2], mid[2], high[2];
            float pb[3] = { 0, 0, 0 }, pAll = 0.0f, inPeakNow = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                auto& s = chans[(size_t) c];
                x[c] = ch[c][i];
                low[c] = s.lowSplit.lp (x[c]);
                const float rest = x[c] - low[c];
                mid[c] = s.midSplit.lp (rest);
                high[c] = rest - mid[c];
                pb[0] += low[c] * low[c];
                pb[1] += mid[c] * mid[c];
                pb[2] += high[c] * high[c];
                pAll += x[c] * x[c];
                inPeakNow = std::max (inPeakNow, std::abs (x[c]));
            }

            inPk = std::max (inPk, inPeakNow);

            // transient detector (contrast)
            pAll *= invCh;
            fastEnv = pAll + (pAll > fastEnv ? fastAtk : fastRel) * (fastEnv - pAll);
            slowEnv = pAll + (pAll > slowEnv ? slowAtk : slowRel) * (slowEnv - pAll);
            const float tDb = clampf (powToDb (fastEnv) - powToDb (slowEnv), -6.0f, 12.0f);
            const float contrastDb = contrastS * 0.55f * tDb * amt;

            float G[3];
            for (int b = 0; b < 3; ++b)
            {
                const float p = pb[b] * invCh;
                bandEnv[(size_t) b] = p + (p > bandEnv[(size_t) b] ? bandAtk : bandRel) * (bandEnv[(size_t) b] - p);
                const float L = powToDb (bandEnv[(size_t) b]);
                const float lift = clampf ((-20.0f - L) * 0.55f, 0.0f, 10.0f) * clampf ((L + 80.0f) * 0.04f, 0.0f, 1.0f);
                const float comp = clampf ((L + 14.0f) * 0.5f, 0.0f, 8.0f);
                const float gDb = amt * (v.up[b] * upScale * lift - v.down[b] * comp + v.eqDb[b]) + cAmt * tilt[b] + contrastDb;
                liftSum[b] += gDb;
                G[b] = dbToGain (gDb) - 1.0f;
            }

            const float satMix = std::min (1.0f, v.sat * amt * 2.0f);
            const float drive = 1.0f + 2.2f * v.sat * amt;
            const float bias = 0.45f * v.satBias * amt;
            const float t0 = softTanh (bias);
            const float satNorm = 1.0f / (drive * std::max (0.2f, 1.0f - t0 * t0));
            const float fatAmt = v.fat * amt * 1.1f;
            const float exAmt = v.exciter * amt * 1.6f;
            const float push = dbToGain (v.pushDb * amt);

            float y[2];
            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                auto& s = chans[(size_t) c];
                float o = x[c] + G[0] * low[c] + G[1] * mid[c] + G[2] * high[c];

                // FAT: octave-up harmonics of the sub band
                const float sub = s.fatLp2.lp (s.fatLp1.lp (x[c]));
                o += fatAmt * s.fatLpOut.lp (s.fatHp.hp (std::abs (sub)));

                // AIR: rectified top band (even harmonics), high-passed again
                const float top = s.exHp2.hp (s.exHp1.hp (x[c]));
                o += exAmt * s.exHpOut.hp (std::abs (top));

                // saturation (unity small-signal gain, DC-blocked difference)
                const float sat = (softTanh (drive * o + bias) - t0) * satNorm;
                const float diff = sat - o;
                s.dcY = diff - s.dcX + dcCoef * s.dcY;
                s.dcX = diff;
                o += satMix * s.dcY;

                o *= push * agGain;
                y[c] = o;
                peak = std::max (peak, std::abs (o));
            }

            // linked peak limiter: instant attack, smooth release. Its threshold sits between the ceiling (limit = 1)
            // and the input's own peak level, so the enhancement can never add peaks above max (ceiling, input peak).
            inPkEnv = std::max (inPeakNow, inPkEnv * limRel);
            const float thr = ceiling + (1.0f - v.limit) * std::max (0.0f, inPkEnv - ceiling);
            limEnv = std::max (peak, limEnv * limRel);
            const float gl = limEnv > thr ? thr / limEnv : 1.0f;
            const float w = std::min (1.0f, amt * 4.0f);
            const float limGain = 1.0f + w * (gl - 1.0f);
            grMin = std::min (grMin, limGain);

            float inK = 0.0f, outK = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                const float wet = y[c] * limGain;
                const float kIn = inMeter.weight (c, x[c]);
                const float kOut = outMeter.weight (c, wet);
                inK += kIn * kIn;
                outK += kOut * kOut;

                float o = x[c];
                if (fade > 0.0f) o = x[c] + fade * (wet - x[c]);
                if (outS != 1.0f) o *= outS;
                ch[c][i] = o;
                outPk = std::max (outPk, std::abs (o));
            }
            inMeter.push (inK * invCh);
            outMeter.push (outK * invCh);
            inAcc += inK;
            outAcc += outK;
        }

        // auto-gain loop: integrate the loudness error while there is signal, freeze in silence
        if (target.autoGain)
        {
            if (inMeter.ms > 1.0e-7f && inAcc > 0.0f && glowS > 0.0f)
            {
                const float err = clampf (powToDb (inMeter.ms) - powToDb (outMeter.ms), -12.0f, 12.0f);
                agDb = clampf (agDb + err * agStep, -agMaxDb, agMaxDb);
            }
            else if (glowS <= 0.0f)
                agDb -= agReturn * agDb;
        }
        else
            agDb -= agReturn * agDb;
        (void) outAcc;
    }

    // denormal hygiene
    for (auto& c : chans)
    {
        for (auto* f : { &c.lowSplit, &c.midSplit, &c.fatLp1, &c.fatLp2, &c.fatHp, &c.fatLpOut, &c.exHp1, &c.exHp2, &c.exHpOut })
            f->flush();
        if (std::abs (c.dcY) < 1.0e-15f) c.dcY = 0.0f;
        if (std::abs (c.dcX) < 1.0e-15f) c.dcX = 0.0f;
    }
    for (auto& e : bandEnv) if (e < 1.0e-20f) e = 0.0f;
    if (fastEnv < 1.0e-20f) fastEnv = 0.0f;
    if (slowEnv < 1.0e-20f) slowEnv = 0.0f;
    if (limEnv < 1.0e-20f) limEnv = 0.0f;
    if (inPkEnv < 1.0e-20f) inPkEnv = 0.0f;
    for (auto* m : { &inMeter, &outMeter })
    {
        if (m->ms < 1.0e-20f) m->ms = 0.0f;
        for (auto& f : m->pre) f.flush();
        for (auto& f : m->shelf) f.flush();
    }

    for (int b = 0; b < 3; ++b)
        meterLift[(size_t) b].store (liftSum[b] / (float) n, std::memory_order_relaxed);
    meterAg.store (target.autoGain ? agDb - 0.8f * voiceS.pushDb * glowS : agDb, std::memory_order_relaxed);
    meterGr.store (-20.0f * std::log10 (std::max (1.0e-6f, grMin)), std::memory_order_relaxed);
    meterIn.store (inPk, std::memory_order_relaxed);
    meterOut.store (outPk, std::memory_order_relaxed);
    meterHarm.store (glowS * (voiceS.sat + voiceS.fat + voiceS.exciter), std::memory_order_relaxed);
}

} // namespace glowfx
