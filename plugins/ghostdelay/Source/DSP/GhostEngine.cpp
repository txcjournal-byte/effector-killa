#include "GhostEngine.h"

#include <algorithm>
#include <cmath>

namespace ghostdelay
{

namespace
{
constexpr double twoPi = 6.283185307179586;
constexpr float pi = 3.14159265358979f;

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }
inline void flush (float& v) { if (v > -1e-20f && v < 1e-20f) v = 0.0f; }

// transparent below 0.8, smoothly limited to +-1.2 above: keeps self-oscillation bounded
inline float guardClip (float x)
{
    const float a = std::abs (x);
    if (a <= 0.8f) return x;
    const float y = 0.8f + 0.4f * std::tanh ((a - 0.8f) * 2.5f);
    return x < 0.0f ? -y : y;
}

// wet-path safety: transparent below 1.5, soft ceiling at 2 (stacked RHYTHM taps under self-oscillation)
inline float safetyClip (float x)
{
    const float a = std::abs (x);
    if (a <= 1.5f) return x;
    const float y = 1.5f + 0.5f * std::tanh ((a - 1.5f) * 2.0f);
    return x < 0.0f ? -y : y;
}

inline double frac (double x) { return x - std::floor (x); }

const std::array<RhythmPattern, numPatterns> patterns { {
    // DOTTED: dotted steps bouncing L / R, loop after four of them
    { 4, { 0.75, 1.5, 2.25, 3.0 }, { 1.0f, 0.78f, 0.62f, 0.5f }, { -0.75f, 0.75f, -0.4f, 0.0f }, 3.0 },
    // TRIPLET: three triplet steps L, R, centre
    { 3, { 2.0 / 3.0, 4.0 / 3.0, 2.0, 0.0 }, { 1.0f, 0.8f, 0.64f, 0.0f }, { -0.7f, 0.7f, 0.0f, 0.0f }, 2.0 },
    // SWING: long-short pairs (58 % swing), alternating sides
    { 4, { 0.58, 1.0, 1.58, 2.0 }, { 0.8f, 1.0f, 0.68f, 0.82f }, { 0.65f, -0.65f, 0.65f, -0.65f }, 2.0 },
} };

// allpass diffuser delays in ms (L / R slightly different for a wide, decorrelated smear)
constexpr std::array<std::array<double, 4>, 2> apMs { { { 4.77, 3.59, 12.73, 9.31 }, { 5.23, 3.91, 13.61, 8.87 } } };
} // namespace

const RhythmPattern& rhythmPattern (int index)
{
    return patterns[(size_t) std::min (numPatterns - 1, std::max (0, index))];
}

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

void ratioFactors (float lrRatio, double& left, double& right)
{
    const double r = std::min (1.0, std::max (-1.0, (double) lrRatio));
    left = r < 0.0 ? 1.0 + 0.5 * r : 1.0;
    right = r > 0.0 ? 1.0 - 0.5 * r : 1.0;
}

// ---------------------------------------------------------------------------------------------------------

void Engine::SVF::set (float cutoff, float sr)
{
    const float fc = clampf (cutoff, 10.0f, sr * 0.45f);
    g = std::tan (pi * fc / sr);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void Engine::Smooth::setTime (double seconds, double sr)
{
    coef = (float) (1.0 - std::exp (-1.0 / (seconds * sr)));
}

void Engine::Ring::allocate (int minSize)
{
    int size = 1;
    while (size < minSize) size <<= 1;
    buf.assign ((size_t) size, 0.0f);
    mask = size - 1;
    wp = 0;
}

void Engine::Ring::clear()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    wp = 0;
}

float Engine::Ring::read (double delay) const
{
    const double pos = (double) wp - delay;
    const double fl = std::floor (pos);
    const float t = (float) (pos - fl);
    const int i = (int) fl;
    const float x0 = buf[(size_t) (i & mask)];
    if (t <= 0.0f) return x0;
    const float xm1 = buf[(size_t) ((i - 1) & mask)];
    const float x1 = buf[(size_t) ((i + 1) & mask)];
    const float x2 = buf[(size_t) ((i + 2) & mask)];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

void Engine::PitchShifter::configure (double sr)
{
    grain = (int) std::round (0.050 * sr);    // 50 ms grains
    search = (int) std::round (0.012 * sr);   // splice search span (covers periods down to ~85 Hz)
    corrLen = (int) std::round (0.008 * sr);  // correlation window
    step = std::max (1, (int) std::round (sr / 24000.0));
    ring.allocate (grain + search + corrLen + 64);
    clear();
}

// best start delay for a restarting head: the candidate (nominal + 0..search) most in phase with the other head
double Engine::PitchShifter::alignedStart (double ratio, double otherDelay) const
{
    const int base = (int) std::round (startDelay (ratio));
    const int other = (int) std::round (otherDelay);
    auto corr = [&] (int dA)
    {
        double c = 0.0, e = 1e-12;
        for (int j = 0; j < corrLen; j += step)
        {
            const float a = ring.buf[(size_t) ((ring.wp - 1 - dA - j) & ring.mask)];
            const float b = ring.buf[(size_t) ((ring.wp - 1 - other - j) & ring.mask)];
            c += (double) a * b;
            e += (double) a * a;
        }
        return c / std::sqrt (e);
    };
    int best = base;
    double bestC = -1e30;
    for (int dlt = 0; dlt <= search; dlt += step)
    {
        const double c = corr (base + dlt);
        if (c > bestC) { bestC = c; best = base + dlt; }
    }
    if (step > 1) // refine around the coarse optimum
        for (int dA = std::max (base, best - step + 1); dA <= std::min (base + search, best + step - 1); ++dA)
        {
            const double c = corr (dA);
            if (c > bestC) { bestC = c; best = dA; }
        }
    return (double) best;
}

float Engine::PitchShifter::process (float x, double ratio)
{
    ring.push (x);
    if (! started)
    {
        const double mid = startDelay (ratio) + 0.5 * search;
        delay = { mid, mid + 0.5 * (1.0 - ratio) * grain };
        age = { 0.0, 0.5 };
        started = true;
    }
    const double maxDelay = (double) (grain + search + 8);
    const double inc = 1.0 / (double) grain;
    float y = 0.0f;
    for (int k = 0; k < 2; ++k)
    {
        age[(size_t) k] += inc;
        if (age[(size_t) k] >= 1.0)
        {
            age[(size_t) k] -= 1.0;
            delay[(size_t) k] = alignedStart (ratio, delay[(size_t) (1 - k)]);
        }
        delay[(size_t) k] = std::min (maxDelay, std::max (2.0, delay[(size_t) k] + (1.0 - ratio)));
        const float s = (float) std::sin (3.141592653589793 * age[(size_t) k]);
        y += s * s * ring.read (delay[(size_t) k]);
    }
    return y;
}

float Engine::Allpass::process (float x, double delay, float g)
{
    const float z = ring.read (delay);
    float s = x + g * z;
    flush (s);
    ring.push (s);
    return z - g * s;
}

void Engine::Chan::clearState()
{
    hp.clear(); lp.clear(); bbdLp.clear(); ghostLp.clear();
    tapeLp = dcX = dcY = held = 0.0f;
    shPhase = 1.0;
    ps.clear();
    for (auto& a : ap) a.ring.clear();
}

// ---------------------------------------------------------------------------------------------------------

void Engine::prepare (double sr, int /*maxBlock*/)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    const int needed = (int) std::ceil ((maxDelaySeconds + 0.05) * sampleRate) + 8;
    for (auto& l : lines) l.allocate (needed);
    maxRead = (double) lines[0].mask - 8.0;
    for (int c = 0; c < 2; ++c)
    {
        auto& ch = chans[(size_t) c];
        ch.ps.configure (sampleRate);
        for (int k = 0; k < 4; ++k)
            ch.ap[(size_t) k].ring.allocate ((int) std::ceil ((apMs[(size_t) c][(size_t) k] + 2.0) * 0.001 * sampleRate) + 8);
    }

    for (auto* s : { &sFeedback, &sLowCut, &sHighCut, &sDepth, &sAge, &sDuck, &sDry, &sWet, &sOut })
        s->setTime (0.02, sampleRate);
    sSend.setTime (0.004, sampleRate);
    sFreeze.setTime (0.03, sampleRate);
    sDuckGain.setTime (0.008, sampleRate);
    glideCoef = 1.0 - std::exp (-1.0 / (0.11 * sampleRate)); // tape glide
    envAtt = (float) (1.0 - std::exp (-1.0 / (0.004 * sampleRate)));
    envRel = (float) (1.0 - std::exp (-1.0 / (0.22 * sampleRate)));
    modeStep = (float) (1.0 / (0.012 * sampleRate));
    dcCoef = (float) (1.0 - twoPi * 5.0 / sampleRate);
    reset();
}

void Engine::reset()
{
    for (auto& l : lines) l.clear();
    for (auto& ch : chans) { ch.clearState(); ch.revPhase = 0.0; }
    env = wetEnv = 0.0f;
    wetLevel = 0.0f;
    duckGainNow = 1.0f;
    lfoPhase = flutterPhase = 0.0;
    drift = driftTarget = 0.0f;
    driftCounter = 0;
    modeGain = 1.0f;
    activeMode = params.mode;
    first = true;
}

void Engine::resetModeState()
{
    for (auto& ch : chans)
    {
        ch.ps.clear();
        for (auto& a : ch.ap) a.ring.clear();
        ch.bbdLp.clear(); ch.ghostLp.clear();
        ch.tapeLp = ch.dcX = ch.dcY = ch.held = 0.0f;
        ch.shPhase = 1.0;
    }
}

void Engine::updateCoefficients()
{
    const float sr = (float) sampleRate;
    const float lc = sLowCut.v, hc = sHighCut.v, age = sAge.v;
    for (auto& ch : chans)
    {
        ch.hp.set (lc, sr);
        ch.lp.set (hc, sr);
    }
    // at the extremes the loop filters are bypassed completely (exact, uncoloured echoes)
    hpBlend = clampf ((lc - 20.0f) / 10.0f, 0.0f, 1.0f);
    lpBlend = clampf ((20000.0f - hc) / 1500.0f, 0.0f, 1.0f);

    const float tapeFc = 15000.0f - 10500.0f * age;
    tapeCoef = 1.0f - std::exp (-6.2831853f * std::min (tapeFc, sr * 0.45f) / sr);

    // BBD: anti-alias filter follows the clock of a 4096..1024-stage chip (longer delay / more age = darker)
    const float stages = 4096.0f - 3072.0f * age;
    for (int c = 0; c < 2; ++c)
    {
        const float secs = (float) std::max (1e-3, delaySmp[(size_t) c] / sampleRate);
        const float fc = clampf (0.45f * stages / (2.0f * secs), 900.0f, 12000.0f);
        chans[(size_t) c].bbdLp.set (fc, sr);
    }

    float toneFc = 16000.0f - 12500.0f * age;
    if (activeMode == Mode::ghost) toneFc = 11000.0f - 8000.0f * age;
    else if (activeMode == Mode::diffuse) toneFc = 14000.0f - 10000.0f * age;
    for (auto& ch : chans) ch.ghostLp.set (toneFc, sr);
    (void) bbdCoef;
}

float Engine::loopColour (int c, float x, Mode m)
{
    auto& ch = chans[(size_t) c];
    float l, h;
    if (hpBlend > 0.0f) { ch.hp.tick (x, l, h); x += hpBlend * (h - x); }
    if (lpBlend > 0.0f) { ch.lp.tick (x, l, h); x += lpBlend * (l - x); }
    const float age = sAge.v;
    bool dc = true;

    switch (m)
    {
        case Mode::tape:
        {
            // asymmetric tanh (even + odd harmonics), unity small-signal gain, then head-gap HF loss
            const float drv = 0.2f + 0.8f * age;
            const float ceil = 1.0f - 0.5f * drv;
            const float bias = 0.18f * drv;
            const float tb = std::tanh (bias);
            const float sat = ceil * (std::tanh (x / ceil + bias) - tb) / (1.0f - tb * tb);
            x += drv * (sat - x);
            ch.tapeLp += tapeCoef * (x - ch.tapeLp);
            flush (ch.tapeLp);
            x += std::min (1.0f, 0.45f + age) * (ch.tapeLp - x);
            break;
        }
        case Mode::bbd:
        {
            ch.bbdLp.tick (x, l, h);
            x = l;
            const float amt = 0.3f + 0.7f * age;
            const float sat = std::tanh (x * 1.5f) / 1.5f;
            x += amt * (sat - x);
            break;
        }
        case Mode::digital:
        {
            if (age > 1e-4f)
            {
                ch.shPhase += 1.0 / (1.0 + 11.0 * (double) (age * age));   // down to 1/12 of the sample rate
                if (ch.shPhase >= 1.0) { ch.shPhase -= 1.0; ch.held = x; }
                const float q = std::exp2 (15.0f - 11.0f * age);                  // 16 .. 5 bits
                x = std::round (ch.held * q) / q;
            }
            break;
        }
        case Mode::ghost:
        {
            ch.ghostLp.tick (x, l, h);
            x = l;
            if (age > 1e-4f) x += 0.6f * age * (std::tanh (x * 1.6f) / 1.6f - x);
            break;
        }
        case Mode::diffuse:
        {
            ch.ghostLp.tick (x, l, h);
            x = l;
            break;
        }
        case Mode::reverse:
        case Mode::rhythm:
        default:
        {
            if (age > 1e-4f)
            {
                ch.ghostLp.tick (x, l, h);
                x += std::min (1.0f, age * 4.0f) * (l - x);
                x += 0.7f * age * (std::tanh (x * 1.4f) / 1.4f - x);
            }
            break;
        }
    }

    if (dc)
    {
        const float y = x - ch.dcX + dcCoef * ch.dcY;
        ch.dcX = x;
        ch.dcY = y;
        flush (ch.dcY);
        x = y;
    }
    return guardClip (x);
}

void Engine::process (float* const* io, int numCh, int n)
{
    if (numCh <= 0 || n <= 0 || lines[0].buf.empty()) return;
    const Params& p = params;
    const double sr = sampleRate;

    // ---- targets
    const double baseSmp = delayMsFor (p) * 0.001 * sr;
    double fL, fR;
    ratioFactors (p.lrRatio, fL, fR);
    const double maxD = std::min (maxDelaySeconds * sr, maxRead);
    auto target = [&] (double f) { return std::min (maxD, std::max (4.0, std::round (baseSmp * f))); };
    const std::array<double, 2> tStereo { target (fL), target (fR) };
    const double tMono = target (1.0);

    const float tFb = clampf (p.feedback, 0.0f, maxFeedback);
    const float tLc = clampf (p.lowCutHz, 20.0f, 2000.0f);
    const float tHc = clampf (p.highCutHz, 1000.0f, 20000.0f);
    const float tDepth = clampf (p.modDepth, 0.0f, 1.0f);
    const float rate = clampf (p.modRate, minModRate, maxModRate);
    const float tAge = clampf (p.age, 0.0f, 1.0f);
    const float tDuck = clampf (p.duck, 0.0f, 1.0f);
    const float mix = clampf (p.mix, 0.0f, 1.0f);
    const float tDry = mix <= 0.5f ? 1.0f : 2.0f * (1.0f - mix);
    const float tWet = mix >= 0.5f ? 1.0f : 2.0f * mix;
    const float tOut = std::pow (10.0f, clampf (p.outputDb, -60.0f, 24.0f) / 20.0f);
    const float tFreeze = p.freeze ? 1.0f : 0.0f;
    const Mode requested = (int) p.mode >= 0 && (int) p.mode < numModes ? p.mode : Mode::tape;
    const double ghostRatio = ghostPitchRatios[(size_t) std::min (numGhostPitches - 1, std::max (0, p.ghostPitch))];
    const RhythmPattern& pat = rhythmPattern (p.pattern);
    // RHYTHM reads up to pat.loop base times back: keep the whole pattern inside the buffer
    const double rhythmMax = std::max (4.0, std::floor ((maxRead - 4.0) / std::max (1.0, pat.loop)));

    if (first)
    {
        activeMode = requested;
        modeGain = 1.0f;
        const bool mono = activeMode == Mode::rhythm;
        delaySmp = { mono ? std::min (tMono, rhythmMax) : tStereo[0], mono ? std::min (tMono, rhythmMax) : tStereo[1] };
        sFeedback.v = tFb; sLowCut.v = tLc; sHighCut.v = tHc; sDepth.v = tDepth; sAge.v = tAge; sDuck.v = tDuck;
        sDry.v = tDry; sWet.v = tWet; sOut.v = tOut; sSend.v = 1.0f; sFreeze.v = tFreeze; sDuckGain.v = 1.0f;
        updateCoefficients();
        first = false;
    }

    // REVERSE: lock the grain grid to the host bar position while the transport runs
    if (activeMode == Mode::reverse && requested == Mode::reverse && p.sync && p.playing && p.ppq >= 0.0)
    {
        const double beats = divisionBeats[(size_t) std::min (numDivisions - 1, std::max (0, p.division))];
        for (int c = 0; c < 2; ++c)
        {
            const double expect = frac (p.ppq / (beats * (c == 0 ? fL : fR)));
            double diff = std::abs (expect - chans[(size_t) c].revPhase);
            diff = std::min (diff, 1.0 - diff);
            if (diff > 0.02) chans[(size_t) c].revPhase = expect;
        }
    }

    float* L = io[0];
    float* R = numCh > 1 ? io[1] : nullptr;
    float peak = 0.0f;
    const double lfoInc = (double) rate / sr, flutInc = (double) rate * 7.3 / sr;
    const double psLatency = chans[0].ps.latency (ghostRatio);
    std::array<double, 2> apComp {};
    for (int c = 0; c < 2; ++c)
        for (double ms : apMs[(size_t) c]) apComp[(size_t) c] += ms * 0.001 * sr;
    const float hissAmt = 0.0012f;

    for (int i = 0; i < n; ++i)
    {
        // ---- parameter smoothing
        const float fb = sFeedback.next (tFb);
        sLowCut.next (tLc);
        sHighCut.next (tHc);
        const float depth = sDepth.next (tDepth);
        const float age = sAge.next (tAge);
        const float duck = sDuck.next (tDuck);
        const float dry = sDry.next (tDry);
        const float wet = sWet.next (tWet);
        const float outG = sOut.next (tOut);
        const float fz = sFreeze.next (tFreeze);
        const float send = sSend.next (1.0f) * (1.0f - fz);

        // ---- mode switch: fade the wet path out, swap the path, fade back in
        if (requested != activeMode)
        {
            modeGain -= modeStep;
            if (modeGain <= 0.0f)
            {
                modeGain = 0.0f;
                activeMode = requested;
                resetModeState();
                coefCounter = 0;
            }
        }
        else if (modeGain < 1.0f)
            modeGain = std::min (1.0f, modeGain + modeStep);
        const Mode m = activeMode;
        if (--coefCounter <= 0) { updateCoefficients(); coefCounter = 8; }

        // ---- tape-style glide of the delay time
        for (int c = 0; c < 2; ++c)
        {
            const double t = m == Mode::rhythm ? std::min (tMono, rhythmMax) : tStereo[(size_t) c];
            auto& d = delaySmp[(size_t) c];
            d += glideCoef * (t - d);
            if (std::abs (t - d) < 1e-3) d = t;
        }

        // ---- modulation sources (off while frozen so a held loop stays put)
        const double modAmt = (double) (depth * (1.0f - fz));
        lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        flutterPhase += flutInc; if (flutterPhase >= 1.0) flutterPhase -= 1.0;
        std::array<double, 2> mod { 0.0, 0.0 };
        if (modAmt > 1e-6)
        {
            switch (m)
            {
                case Mode::tape:
                {
                    if (--driftCounter <= 0)
                    {
                        driftTarget = noise();
                        driftCounter = (int) (sr * 0.35);
                    }
                    drift += (driftTarget - drift) * (float) (3.0 / sr);
                    const double w = (0.003 + 0.0015 * age) * sr * modAmt;   // wow: up to 3-4.5 ms
                    const double f = 0.00014 * sr * modAmt;                   // flutter
                    const double dr = 0.0009 * sr * modAmt;                   // slow drift
                    for (int c = 0; c < 2; ++c)
                        mod[(size_t) c] = w * (1.0 + std::sin (twoPi * (lfoPhase + 0.23 * c)))
                                        + f * (1.0 + std::sin (twoPi * (flutterPhase + 0.31 * c)))
                                        + dr * (1.0 + drift);
                    break;
                }
                case Mode::bbd:
                {
                    const double tri = 4.0 * std::abs (lfoPhase - 0.5);  // 0..2
                    const double a = 0.004 * sr * modAmt;
                    mod = { a * tri, a * (2.0 - tri) };                  // L / R in anti-phase (chorus spread)
                    break;
                }
                case Mode::diffuse:
                case Mode::digital:
                case Mode::ghost:
                case Mode::rhythm:
                {
                    const double a = (m == Mode::rhythm ? 0.001 : 0.002) * sr * modAmt;
                    for (int c = 0; c < 2; ++c)
                        mod[(size_t) c] = a * (1.0 + std::sin (twoPi * (lfoPhase + 0.25 * c)));
                    break;
                }
                case Mode::reverse:
                default: break;
            }
        }

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

        // ---- mode paths
        std::array<float, 2> in { inL, inR }, wetOut { 0.0f, 0.0f }, w { 0.0f, 0.0f };
        if (m == Mode::rhythm)
        {
            const double T = delaySmp[0];
            for (int k = 0; k < pat.numTaps; ++k)
            {
                const float y = readMain (0, pat.times[(size_t) k] * T + mod[0]);
                const float g = pat.gains[(size_t) k], pan = pat.pans[(size_t) k];
                wetOut[0] += y * g * std::min (1.0f, 1.0f - pan);
                wetOut[1] += y * g * std::min (1.0f, 1.0f + pan);
            }
            const double loopD = std::min (maxRead, std::round (pat.loop * T));
            const float fbSig = readMain (0, loopD + mod[0]);
            float x = loopColour (0, send * 0.5f * (inL + inR) + fb * fbSig, m);
            if (fz > 1e-5f) x += fz * (readMain (0, loopD) - x);
            w = { x, x };
        }
        else
        {
            std::array<float, 2> fbSig { 0.0f, 0.0f };
            for (int c = 0; c < 2; ++c)
            {
                const double d = delaySmp[(size_t) c];
                auto& ch = chans[(size_t) c];
                switch (m)
                {
                    case Mode::reverse:
                    {
                        // grain = delay time; plays the previous grain backwards, short raised-cosine edges
                        ch.revPhase += 1.0 / std::max (8.0, d);
                        if (ch.revPhase >= 1.0) ch.revPhase -= std::floor (ch.revPhase);
                        const double ph = ch.revPhase;
                        const double fade = std::min (0.12, (0.006 * sr) / std::max (8.0, d));
                        const double e = std::min (1.0, std::min (ph, 1.0 - ph) / fade);
                        const float win = (float) (0.5 - 0.5 * std::cos (3.141592653589793 * e));
                        const float y = win * readMain (c, 2.0 + 2.0 * ph * d);
                        wetOut[(size_t) c] = y;
                        fbSig[(size_t) c] = y;
                        break;
                    }
                    case Mode::diffuse:
                    {
                        // the allpass chain's main lobe arrives ~apComp late: read that much earlier to stay on the grid
                        float y = readMain (c, std::max (4.0, d - apComp[(size_t) c]) + mod[(size_t) c]);
                        const float g = 0.6f + 0.12f * age;
                        for (int k = 0; k < 4; ++k)
                        {
                            const double base = apMs[(size_t) c][(size_t) k] * 0.001 * sr;
                            const double wob = k >= 2 ? (0.0004 * sr * (double) depth) * (1.0 + std::sin (twoPi * (lfoPhase * (1.0 + 0.37 * k) + 0.19 * k + 0.5 * c))) : 0.0;
                            y = ch.ap[(size_t) k].process (y, base + wob, g);
                        }
                        wetOut[(size_t) c] = y;
                        fbSig[(size_t) c] = y;
                        break;
                    }
                    case Mode::ghost:
                    {
                        wetOut[(size_t) c] = readMain (c, d + mod[(size_t) c]);
                        // the shifter's average latency is taken off the feedback tap so the repeats stay in time
                        const float tap = readMain (c, std::max (4.0, d - psLatency) + mod[(size_t) c]);
                        fbSig[(size_t) c] = ch.ps.process (tap, ghostRatio);
                        break;
                    }
                    case Mode::tape:
                    case Mode::bbd:
                    case Mode::digital:
                    case Mode::rhythm:
                    default:
                    {
                        const float y = readMain (c, d + mod[(size_t) c]);
                        wetOut[(size_t) c] = y;
                        fbSig[(size_t) c] = y;
                        break;
                    }
                }
            }
            if (m == Mode::diffuse)
                fbSig = { 0.72f * fbSig[0] + 0.28f * fbSig[1], 0.72f * fbSig[1] + 0.28f * fbSig[0] };

            for (int c = 0; c < 2; ++c)
            {
                float x = loopColour (c, send * in[(size_t) c] + fb * fbSig[(size_t) c], m);
                if (fz > 1e-5f)
                {
                    // freeze: the line repeats itself with the loop period (feedback 1, no colour, no input)
                    const double period = std::round (delaySmp[(size_t) c]);
                    x += fz * (readMain (c, period) - x);
                }
                w[(size_t) c] = x;
            }
        }

        for (int c = 0; c < 2; ++c)
        {
            flush (w[(size_t) c]);
            lines[(size_t) c].push (w[(size_t) c]);
        }

        // ---- wet: age noise (tape hiss / BBD clock noise, gated by the echo level), duck, mix
        float eL = wetOut[0], eR = wetOut[1];
        const float wl = std::max (std::abs (eL), std::abs (eR));
        wetEnv += (wl > wetEnv ? 0.01f : 0.0003f) * (wl - wetEnv);
        flush (wetEnv);
        if ((m == Mode::tape || m == Mode::bbd) && age > 1e-4f)
        {
            const float nz = hissAmt * age * std::min (1.0f, wetEnv * 6.0f);
            eL += nz * noise();
            eR += nz * noise();
        }
        const float g = wet * duckG * modeGain;
        eL = safetyClip (eL) * g;
        eR = safetyClip (eR) * g;
        peak = std::max (peak, std::max (std::abs (eL), std::abs (eR)));

        if (R != nullptr)
        {
            L[i] = (inL * dry + eL) * outG;
            R[i] = (inR * dry + eR) * outG;
        }
        else
            L[i] = (inL * dry + 0.5f * (eL + eR)) * outG;
    }

    wetLevel = peak;
    duckGainNow = sDuckGain.v;
}

} // namespace ghostdelay
