#include "ThrowEngine.h"

#include <algorithm>
#include <cmath>

namespace throwfx
{

namespace
{
constexpr double twoPi = 6.283185307179586;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

// transparent below 0.8, smoothly limited to +-1.2 above: keeps self-oscillation bounded
inline float guardClip (float x)
{
    const float a = std::abs (x);
    if (a <= 0.8f) return x;
    const float y = 0.8f + 0.4f * std::tanh ((a - 0.8f) * 2.5f);
    return x < 0.0f ? -y : y;
}
} // namespace

double delayMsFor (const Params& p)
{
    double ms;
    if (p.sync)
    {
        const int d = std::min (numDivisions - 1, std::max (0, p.division));
        const double bpm = std::min (400.0, std::max (20.0, p.bpm > 0.0 ? p.bpm : 120.0));
        ms = divisionBeats[(size_t) d] * 60000.0 / bpm;
    }
    else
        ms = std::min ((double) maxFreeMs, std::max ((double) minFreeMs, (double) p.timeMs));
    return std::min (maxDelaySeconds * 1000.0, std::max (1.0, ms));
}

void Engine::SVF::set (float cutoff, float sr)
{
    const float fc = clampf (cutoff, 10.0f, sr * 0.45f);
    g = std::tan (3.14159265f * fc / sr);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void Engine::Smooth::setTime (double seconds, double sr)
{
    coef = (float) (1.0 - std::exp (-1.0 / (seconds * sr)));
}

void Engine::prepare (double sr, int /*maxBlock*/)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    const int needed = (int) std::ceil ((maxDelaySeconds + 0.05) * sampleRate) + 8;
    bufSize = 1;
    while (bufSize < needed) bufSize <<= 1;
    bufMask = bufSize - 1;
    for (auto& l : lines) l.assign ((size_t) bufSize, 0.0f);

    for (auto* s : { &sFeedback, &sDrive, &sWobble, &sLowCut, &sHighCut, &sDuck, &sWidth, &sDry, &sWet, &sOut })
        s->setTime (0.02, sampleRate);
    sSend.setTime (0.004, sampleRate);
    sFreeze.setTime (0.03, sampleRate);
    sDuckGain.setTime (0.008, sampleRate);
    glideCoef = 1.0 - std::exp (-1.0 / (0.11 * sampleRate)); // tape glide
    envAtt = (float) (1.0 - std::exp (-1.0 / (0.004 * sampleRate)));
    envRel = (float) (1.0 - std::exp (-1.0 / (0.22 * sampleRate)));
    reset();
}

void Engine::reset()
{
    for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
    writePos = 0;
    for (auto& f : hp) f.clear();
    for (auto& f : lp) f.clear();
    tapeLp = { 0.0f, 0.0f };
    env = 0.0f;
    wetLevel = 0.0f;
    duckGainNow = 1.0f;
    wowPhase = flutterPhase = 0.0;
    drift = driftTarget = 0.0f;
    first = true;
}

float Engine::readLine (int line, double delay) const
{
    const auto& b = lines[(size_t) line];
    const double pos = (double) writePos - delay;
    const double fl = std::floor (pos);
    const float t = (float) (pos - fl);
    const int i = (int) fl;
    const float xm1 = b[(size_t) ((i - 1) & bufMask)];
    const float x0 = b[(size_t) (i & bufMask)];
    if (t <= 0.0f) return x0;
    const float x1 = b[(size_t) ((i + 1) & bufMask)];
    const float x2 = b[(size_t) ((i + 2) & bufMask)];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

void Engine::updateCoefficients()
{
    const float sr = (float) sampleRate;
    const float lc = sLowCut.v, hc = sHighCut.v;
    for (auto& f : hp) f.set (lc, sr);
    for (auto& f : lp) f.set (hc, sr);
    // at the extremes the filters are bypassed completely (exact, uncoloured echoes)
    hpBlend = clampf ((lc - 20.0f) / 10.0f, 0.0f, 1.0f);
    lpBlend = clampf ((20000.0f - hc) / 1500.0f, 0.0f, 1.0f);
    const float tapeFc = 18000.0f - 11000.0f * sDrive.v;
    tapeCoef = 1.0f - std::exp (-6.2831853f * std::min (tapeFc, sr * 0.45f) / sr);
}

float Engine::loopProcess (int line, float x)
{
    float l, h;
    hp[(size_t) line].tick (x, l, h);
    x += hpBlend * (h - x);
    lp[(size_t) line].tick (x, l, h);
    x += lpBlend * (l - x);

    const float drv = sDrive.v;
    if (drv > 1e-4f)
    {
        // tape: asymmetric tanh (even + odd harmonics), unity small-signal gain, plus head-gap HF loss
        const float c = 1.0f - 0.55f * drv;              // saturation ceiling (1.0 .. 0.45)
        const float bias = 0.2f * drv;
        const float tb = std::tanh (bias);
        const float sech2 = 1.0f - tb * tb;
        const float sat = c * (std::tanh (x / c + bias) - tb) / sech2;
        x += drv * (sat - x);
        auto& s = tapeLp[(size_t) line];
        s += tapeCoef * (x - s);
        if (s > -1e-20f && s < 1e-20f) s = 0.0f;
        x += std::min (1.0f, drv * 1.5f) * (s - x);
    }
    return guardClip (x);
}

void Engine::process (float* const* ch, int numCh, int n)
{
    if (numCh <= 0 || n <= 0 || bufSize == 0) return;
    const Params& p = params;
    const float sr = (float) sampleRate;

    // ---- targets
    const double targetDelay = std::max (4.0, std::round (delayMsFor (p) * 0.001 * sampleRate));
    const float tFb = clampf (p.feedback, 0.0f, maxFeedback);
    const float tDrive = clampf (p.drive, 0.0f, 1.0f);
    const float tWob = clampf (p.wobble, 0.0f, 1.0f);
    const float tLc = clampf (p.lowCutHz, 20.0f, 2000.0f);
    const float tHc = clampf (p.highCutHz, 1000.0f, 20000.0f);
    const float tDuck = clampf (p.duck, 0.0f, 1.0f);
    const float tWidth = clampf (p.width, 0.0f, 2.0f);
    const float mix = clampf (p.mix, 0.0f, 1.0f);
    const float tDry = mix <= 0.5f ? 1.0f : 2.0f * (1.0f - mix);
    const float tWet = mix >= 0.5f ? 1.0f : 2.0f * mix;
    const float tOut = std::pow (10.0f, clampf (p.outputDb, -60.0f, 24.0f) / 20.0f);
    const float tSend = (! p.throwOnly || p.throwOn) ? 1.0f : 0.0f;
    const float tFreeze = p.freeze ? 1.0f : 0.0f;
    const Mode mode = p.mode;

    if (first)
    {
        delaySmp = targetDelay;
        sFeedback.v = tFb; sDrive.v = tDrive; sWobble.v = tWob; sLowCut.v = tLc; sHighCut.v = tHc;
        sDuck.v = tDuck; sWidth.v = tWidth; sDry.v = tDry; sWet.v = tWet; sOut.v = tOut; sSend.v = tSend;
        sFreeze.v = tFreeze; sDuckGain.v = 1.0f;
        updateCoefficients();
        first = false;
    }

    float* L = ch[0];
    float* R = numCh > 1 ? ch[1] : nullptr;
    float peak = 0.0f;
    const double wowInc = 0.52 / sampleRate, flutInc = 6.4 / sampleRate;

    for (int i = 0; i < n; ++i)
    {
        // ---- parameter smoothing
        const float fb = sFeedback.next (tFb);
        sDrive.next (tDrive);
        const float wob = sWobble.next (tWob);
        sLowCut.next (tLc);
        sHighCut.next (tHc);
        const float duck = sDuck.next (tDuck);
        const float width = sWidth.next (tWidth);
        const float dry = sDry.next (tDry);
        const float wet = sWet.next (tWet);
        const float outG = sOut.next (tOut);
        const float fz = sFreeze.next (tFreeze);
        const float send = sSend.next (tSend) * (1.0f - fz);
        if (--coefCounter <= 0) { updateCoefficients(); coefCounter = 8; }

        // tape-style glide of the delay time
        delaySmp += glideCoef * (targetDelay - delaySmp);
        if (std::abs (targetDelay - delaySmp) < 1e-3) delaySmp = targetDelay;

        // ---- wow & flutter (off while frozen so the held loop stays put)
        double modL = 0.0, modR = 0.0;
        const float wobAmt = wob * (1.0f - fz);
        if (wobAmt > 1e-5f)
        {
            wowPhase += wowInc; if (wowPhase >= 1.0) wowPhase -= 1.0;
            flutterPhase += flutInc; if (flutterPhase >= 1.0) flutterPhase -= 1.0;
            if (--driftCounter <= 0)
            {
                rng = rng * 1664525u + 1013904223u;
                driftTarget = (float) ((rng >> 8) & 0xffff) / 32768.0f - 1.0f;
                driftCounter = (int) (sampleRate * 0.35);
            }
            drift += (driftTarget - drift) * (float) (3.0 / sampleRate);
            const double wl = std::sin (twoPi * wowPhase), wr = std::sin (twoPi * (wowPhase + 0.23));
            const double fl = std::sin (twoPi * flutterPhase), fr = std::sin (twoPi * (flutterPhase + 0.31));
            const double scale = (double) wobAmt * sampleRate;
            modL = scale * (0.0024 * (1.0 + wl) + 0.0009 * (1.0 + drift) + 0.00011 * (1.0 + fl));
            modR = scale * (0.0024 * (1.0 + wr) + 0.0009 * (1.0 + drift) + 0.00011 * (1.0 + fr));
        }
        const double maxD = (double) bufSize - 8.0;
        const double dL = std::min (maxD, std::max (4.0, delaySmp + modL));
        const double dR = std::min (maxD, std::max (4.0, delaySmp + modR));

        // ---- input + ducking envelope (on the dry signal)
        const float inL = L[i];
        const float inR = R != nullptr ? R[i] : inL;
        const float lvl = std::max (std::abs (inL), std::abs (inR));
        env += (lvl > env ? envAtt : envRel) * (lvl - env);
        if (env < 1e-12f) env = 0.0f;
        float duckTarget = 1.0f;
        if (duck > 1e-4f)
        {
            const float envDb = 20.0f * std::log10 (env + 1e-9f);
            const float amt = clampf ((envDb + 48.0f) / 30.0f, 0.0f, 1.0f);
            duckTarget = std::pow (10.0f, -duck * 30.0f * amt / 20.0f);
        }
        const float duckG = sDuckGain.next (duckTarget);

        // ---- delay lines
        const float yL = readLine (0, dL);
        const float yR = mode == Mode::mono ? yL : readLine (1, dR);
        float wL, wR;
        if (mode == Mode::stereo)
        {
            wL = loopProcess (0, send * inL + fb * yL);
            wR = loopProcess (1, send * inR + fb * yR);
        }
        else if (mode == Mode::pingPong)
        {
            wL = loopProcess (0, send * 0.5f * (inL + inR) + fb * yR);
            wR = loopProcess (1, fb * yL);
        }
        else
        {
            wL = loopProcess (0, send * 0.5f * (inL + inR) + fb * yL);
            wR = 0.0f;
        }
        // freeze: hold whatever is in the line (feedback 1, no colour)
        if (fz > 1e-5f)
        {
            wL += fz * ((mode == Mode::pingPong ? yR : yL) - wL);
            wR += fz * ((mode == Mode::pingPong ? yL : yR) - wR);
        }
        if (wL > -1e-20f && wL < 1e-20f) wL = 0.0f;
        if (wR > -1e-20f && wR < 1e-20f) wR = 0.0f;
        lines[0][(size_t) writePos] = wL;
        lines[1][(size_t) writePos] = wR;
        writePos = (writePos + 1) & bufMask;

        // ---- wet: width, duck, mix
        const float m = 0.5f * (yL + yR), s = 0.5f * (yL - yR) * width;
        const float g = wet * duckG;
        const float eL = (m + s) * g, eR = (m - s) * g;
        peak = std::max (peak, std::max (std::abs (eL), std::abs (eR)));

        if (R != nullptr)
        {
            L[i] = (inL * dry + eL) * outG;
            R[i] = (inR * dry + eR) * outG;
        }
        else
            L[i] = (inL * dry + 0.5f * (eL + eR)) * outG;
    }

    (void) sr;
    wetLevel = peak;
    duckGainNow = sDuckGain.v;
    sendGate = sSend.v * (1.0f - sFreeze.v);
}

} // namespace throwfx
