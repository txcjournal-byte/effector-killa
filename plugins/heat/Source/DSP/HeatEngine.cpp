#include "HeatEngine.h"

#include <algorithm>
#include <cmath>

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define HEAT_HAS_SSE 1
#endif

namespace heatfx
{

namespace
{
constexpr double PI = 3.14159265358979323846;

// half-band stages: (taps, Kaiser beta). Stage 1 carries the steep anti-alias edge, later stages are short.
constexpr int stageTaps[3] = { 115, 31, 19 };
constexpr double stageBeta[3] = { 8.6, 8.0, 8.0 };
constexpr int stageCentre[3] = { (115 - 1) / 2, (31 - 1) / 2, (19 - 1) / 2 };

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 64; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1.0e-12 * sum) break;
    }
    return sum;
}

int highestRateDelay (int osChoice)
{
    const int f = factorForChoice (osChoice);
    int t = 0;
    for (int s = 0; s < osChoice; ++s)
        t += stageCentre[s] * (f >> s);   // stage s runs at input rate base * 2^s -> its delay in highest-rate samples
    return t;
}

struct DenormalGuard
{
#ifdef HEAT_HAS_SSE
    unsigned int old = _mm_getcsr();
    DenormalGuard() { _mm_setcsr (old | 0x8040); }   // FTZ + DAZ
    ~DenormalGuard() { _mm_setcsr (old); }
#endif
};
} // namespace

// ================= curves =================

float compensation (float g) { return 1.0f / std::sqrt (g); }

float transformerHiK (float drive) { return driveGain (-0.6f * drive * maxDriveDb); }
float transformerHiOut (float drive) { return driveGain (0.1f * drive * maxDriveDb); }

float cleanBlend (float drive)
{
    const float t = clampf (drive / 0.08f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float shapeCore (int model, float u, float a, float lowPart, float hiK, float hiOut)
{
    switch ((Model) model)
    {
        case Model::tube:
        {
            // biased triode-like curve: soft, asymmetric -> 2nd harmonic
            constexpr float b = 0.25f, tb = 0.24491866f, s0 = 0.94001485f;
            return (std::tanh (u + b) - tb) / s0;
        }
        case Model::tape:
        {
            // very soft knee that keeps compressing, a touch of even content
            const float uu = u * u;
            return u / std::sqrt (1.0f + uu) + 0.05f * uu / (1.0f + uu);
        }
        case Model::transformer:
        {
            // core saturation is frequency dependent: the low end takes the full drive and saturates first,
            // the highs only see 40 % of the drive (in dB) and a gentle curve
            const float hiPart = (u - lowPart) * hiK;
            const float hiSat = hiPart / std::sqrt (1.0f + 0.25f * hiPart * hiPart) * hiOut;   // highs end up ~unity after compensation
            const float tl = std::tanh (lowPart);
            return hiSat + std::tanh (1.4f * lowPart) / 1.4f + 0.10f * tl * tl;
        }
        case Model::transistor:
        {
            // firmer knee, odd dominant, slight asymmetry
            const float v = u + 0.06f * u * std::abs (u) / (1.0f + std::abs (u));
            const float v2 = v * v;
            return v / std::sqrt (std::sqrt (1.0f + v2 * v2));
        }
        case Model::fuzz:
        {
            // high gain, hard asymmetric clip (positive rail higher than negative)
            const float v = 2.0f * u;
            return 0.85f * (v > 0.0f ? std::tanh (v) : 0.55f * std::tanh (v / 0.55f));
        }
        case Model::rectify:
        {
            // from half-wave to full-wave (octave up) as drive rises
            const float s = std::tanh (u);
            const float r = clampf (a * 1.25f, 0.0f, 1.0f);
            return s > 0.0f ? s : s * (1.0f - 2.0f * r);
        }
        case Model::foldback:
        {
            // sine wavefolder: folds back instead of clipping
            return std::sin (u);
        }
        case Model::warmCrush:
        {
            // warm tube curve into a reducing bit depth (14 -> 4 bits), smoothed afterwards in the engine
            const float t = std::tanh (1.15f * u) / 1.15f;
            const float q = std::exp2 (13.0f - 10.0f * a);
            return std::round (t * q) / q;
        }
        case Model::count:
        default: break;
    }
    return u;
}

float transferCurve (int model, float drive, float x)
{
    model = std::max (0, std::min (numModels - 1, model));
    drive = clampf (drive, 0.0f, 1.0f);
    const float g = driveGain (drive * maxDriveDb);
    const float u = g * x;
    const float y = shapeCore (model, u, drive, u, transformerHiK (drive), transformerHiOut (drive)) * compensation (g);
    const float w = cleanBlend (drive);
    return x + w * (y - x);
}

int factorForChoice (int c) { return 1 << std::max (0, std::min (numOsChoices - 1, c)); }

int padForChoice (int c)
{
    c = std::max (0, std::min (numOsChoices - 1, c));
    const int f = factorForChoice (c);
    return (f - highestRateDelay (c) % f) % f;
}

int latencyForChoice (int c)
{
    c = std::max (0, std::min (numOsChoices - 1, c));
    return (highestRateDelay (c) + padForChoice (c)) / factorForChoice (c);
}

// ================= filters =================

void SvfCoefs::set (double fc, double fs, double q)
{
    fc = std::max (1.0, std::min (fc, fs * 0.49));
    const double gg = std::tan (PI * fc / fs);
    const double kk = 1.0 / q;
    const double aa1 = 1.0 / (1.0 + gg * (gg + kk));
    g = (float) gg; k = (float) kk;
    a1 = (float) aa1; a2 = (float) (gg * aa1); a3 = (float) (gg * gg * aa1);
}

static void rbjShelf (Biquad& q, double fs, double f, double gainDb, double slope, bool high)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * PI * std::min (f, fs * 0.45) / fs;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
    const double sa = 2.0 * std::sqrt (A) * alpha;
    double b0, b1, b2, a0, a1, a2;
    if (high)
    {
        b0 = A * ((A + 1) + (A - 1) * cw + sa);
        b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        b2 = A * ((A + 1) + (A - 1) * cw - sa);
        a0 = (A + 1) - (A - 1) * cw + sa;
        a1 = 2 * ((A - 1) - (A + 1) * cw);
        a2 = (A + 1) - (A - 1) * cw - sa;
    }
    else
    {
        b0 = A * ((A + 1) - (A - 1) * cw + sa);
        b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - sa);
        a0 = (A + 1) + (A - 1) * cw + sa;
        a1 = -2 * ((A - 1) + (A + 1) * cw);
        a2 = (A + 1) + (A - 1) * cw - sa;
    }
    q.b0 = (float) (b0 / a0); q.b1 = (float) (b1 / a0); q.b2 = (float) (b2 / a0);
    q.a1 = (float) (a1 / a0); q.a2 = (float) (a2 / a0);
}

void Biquad::setLowShelf (double fs, double f, double gainDb, double slope) { rbjShelf (*this, fs, f, gainDb, slope, false); }
void Biquad::setHighShelf (double fs, double f, double gainDb, double slope) { rbjShelf (*this, fs, f, gainDb, slope, true); }

void HalfbandStage::design (int numTaps, double beta)
{
    const int N = numTaps;
    c = (N - 1) / 2;
    m = (N + 1) / 2;
    he.assign ((size_t) m, 0.0f);
    std::vector<double> h ((size_t) m);
    double sum = 0.0;
    const double i0b = besselI0 (beta);
    for (int j = 0; j < m; ++j)
    {
        const int n = 2 * j;
        const double t = (double) (n - c);                 // odd
        const double ideal = std::sin (PI * t / 2.0) / (PI * t);
        const double r = 2.0 * n / (N - 1) - 1.0;
        const double win = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
        h[(size_t) j] = ideal * win;
        sum += h[(size_t) j];
    }
    for (int j = 0; j < m; ++j)
        he[(size_t) j] = (float) (h[(size_t) j] * 0.5 / sum);   // even taps sum to 0.5, centre tap = 0.5
    dUp = (c - 1) / 2;
    dDown = (c + 1) / 2;
    upBuf.assign ((size_t) (2 * m), 0.0f);
    evBuf.assign ((size_t) (2 * m), 0.0f);
    odBuf.assign ((size_t) (2 * m), 0.0f);
    upPos = evPos = odPos = 0;
}

void HalfbandStage::reset()
{
    std::fill (upBuf.begin(), upBuf.end(), 0.0f);
    std::fill (evBuf.begin(), evBuf.end(), 0.0f);
    std::fill (odBuf.begin(), odBuf.end(), 0.0f);
    upPos = evPos = odPos = 0;
}

// ================= engine =================

void Engine::Channel::reset()
{
    keepSplit.reset();
    tiltPreLo.reset(); tiltPreHi.reset(); tiltPostLo.reset(); tiltPostHi.reset();
    for (auto& s : os) s.reset();
    split1.reset(); split2.reset(); ap2.reset();
    for (auto& b : band) b.reset();
    dcX = dcY = 0;
    lowCut.reset(); highCut.reset();
    dryDelay.fill (0.0f); lowDelay.fill (0.0f);
    pad.fill (0.0f);
    padPos = 0;
}

Engine::Engine()
{
    for (auto& p : bandPeak) p.store (0.0f);
    for (auto& ch : chans)
        for (int s = 0; s < 3; ++s)
            ch.os[(size_t) s].design (stageTaps[s], stageBeta[s]);
    prepare (44100.0, 512);
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
    dcR = (float) (1.0 - 2.0 * PI * 3.0 / sr);
    reset();
}

void Engine::reset()
{
    activeOs = std::max (0, std::min (numOsChoices - 1, target.oversampling));
    activeBands = std::max (1, std::min (maxBands, target.bands));
    for (auto& c : chans) c.reset();
    fadeGain = 1.0f; fadeDir = 0; holdSamples = 0;
    fadeStep = 1.0f / (float) std::max (64.0, sr * 0.003);
    delayPos = 0;

    const auto& p = target;
    sXlo.reset (p.xoverLoHz); sXhi.reset (p.xoverHiHz);
    sTiltPre.reset (p.tiltPreDb); sTiltPost.reset (p.tiltPostDb);
    sLowCut.reset (std::max (lowCutOffHz, p.lowCutHz)); sHighCut.reset (std::min (highCutOffHz, p.highCutHz));
    sLowCutOn.reset (p.lowCutHz > lowCutOffHz + 0.5f ? 1.0f : 0.0f);
    sHighCutOn.reset (p.highCutHz < highCutOffHz - 5.0f ? 1.0f : 0.0f);
    sPunch.reset (p.punch); sDyn.reset (p.dynamics); sMix.reset (p.mix); sOut.reset (p.outputDb);
    sKeep.reset (std::max (10.0f, p.lowKeepHz <= lowKeepOffHz ? 10.0f : p.lowKeepHz));
    bool anySolo = false;
    for (int b = 0; b < activeBands; ++b) anySolo |= p.band[(size_t) b].solo;
    for (int b = 0; b < maxBands; ++b)
    {
        const auto& bp = p.band[(size_t) b];
        sDrive[(size_t) b].reset (bp.drive);
        sBandMix[(size_t) b].reset (bp.mix);
        sBandLevel[(size_t) b].reset (bp.levelDb);
        sSolo[(size_t) b].reset (anySolo ? (bp.solo ? 1.0f : 0.0f) : 1.0f);
        curModel[(size_t) b] = prevModel[(size_t) b] = std::max (0, std::min (numModels - 1, bp.model));
        modelXf[(size_t) b] = 1.0f;
    }
    punchEnv = dynEnv = punchGrDb = 0.0f;
    msIn = msOut = 0.0;
    agDb = agPrev = 0.0f;
    lastTiltPre = lastTiltPost = 1.0e9f;
    updateCoefficients();
}

void Engine::applyStructure()
{
    const int newOs = std::max (0, std::min (numOsChoices - 1, target.oversampling));
    activeBands = std::max (1, std::min (maxBands, target.bands));
    // stay muted until everything still inside the oversampling pipeline (old structure) has left it
    holdSamples = latencyForChoice (activeOs) + (int) (sr * 0.001);
    if (newOs != activeOs)
    {
        activeOs = newOs;
        for (auto& c : chans)
        {
            for (auto& s : c.os) s.reset();
            c.pad.fill (0.0f);
            c.padPos = 0;
        }
        // ... and while the new filter chain fills up and settles
        holdSamples = std::max (holdSamples, latencyForChoice (activeOs)) + (int) (sr * 0.006);
    }
    updateCoefficients();
}

void Engine::updateCoefficients()
{
    const double fsOs = sr * factorForChoice (activeOs);
    keepC.set (sKeep.cur, sr, 0.70710678);
    const float xlo = clampf (sXlo.cur, 20.0f, 16000.0f);
    const float xhi = std::max (xlo * 1.5f, clampf (sXhi.cur, 30.0f, 20000.0f));
    x1C.set (xlo, fsOs, 0.70710678);
    x2C.set (xhi, fsOs, 0.70710678);
    lowCutC.set (std::max (lowCutOffHz, sLowCut.cur), sr, 0.70710678);
    highCutC.set (std::min ((double) sHighCut.cur, sr * 0.45), sr, 0.70710678);
    if (std::abs (sTiltPre.cur - lastTiltPre) > 1.0e-4f)
    {
        lastTiltPre = sTiltPre.cur;
        preLo.setLowShelf (sr, 650.0, -lastTiltPre, 0.5);
        preHi.setHighShelf (sr, 650.0, lastTiltPre, 0.5);
    }
    if (std::abs (sTiltPost.cur - lastTiltPost) > 1.0e-4f)
    {
        lastTiltPost = sTiltPost.cur;
        postLo.setLowShelf (sr, 650.0, -lastTiltPost, 0.5);
        postHi.setHighShelf (sr, 650.0, lastTiltPost, 0.5);
    }
    auto onePole = [fsOs] (double fc) { return (float) (1.0 - std::exp (-2.0 * PI * std::min (fc, fsOs * 0.45) / fsOs)); };
    for (int b = 0; b < maxBands; ++b)
    {
        const float a = clampf (sDrive[(size_t) b].cur, 0.0f, 1.0f);
        tapeA[b] = onePole (20000.0 - 12000.0 * a);
        crushA[b] = onePole (16000.0 - 10000.0 * a);
        xfK[b] = transformerHiK (a);
        xfO[b] = transformerHiOut (a);
    }
    xfA = onePole (120.0);
}

float Engine::runModel (int model, BandState& st, float u, float a, int b) const
{
    float lowPart = u;
    if (model == (int) Model::transformer)
    {
        st.xfLp += xfA * (u - st.xfLp);
        lowPart = st.xfLp;
    }
    float y = shapeCore (model, u, a, lowPart, xfK[b], xfO[b]);
    if (model == (int) Model::tape)
    {
        st.tapeLp += tapeA[b] * (y - st.tapeLp);
        y = st.tapeLp;
    }
    else if (model == (int) Model::warmCrush)
    {
        st.crushLp += crushA[b] * (y - st.crushLp);
        st.crushLp2 += crushA[b] * (st.crushLp - st.crushLp2);
        y = st.crushLp2;
    }
    return y;
}

void Engine::process (float* const* ch, int numCh, int n)
{
    DenormalGuard dg;
    numCh = std::max (0, std::min (maxCh, numCh));
    if (numCh == 0 || n <= 0) return;

    float peaks[maxBands] = { 0, 0, 0 };

    for (int start = 0; start < n; start += chunk)
    {
        const int len = std::min (chunk, n - start);
        const auto& p = target;

        // ---- structural changes (oversampling, band count) happen behind a short fade
        const int wantOs = std::max (0, std::min (numOsChoices - 1, p.oversampling));
        const int wantBands = std::max (1, std::min (maxBands, p.bands));
        if (fadeDir == 0 && (wantOs != activeOs || wantBands != activeBands)) fadeDir = -1;
        if (fadeDir == -1 && fadeGain <= 0.0f)
        {
            applyStructure();
            fadeDir = 1;
        }
        for (int i = 0; i < len; ++i)
        {
            if (holdSamples > 0) --holdSamples;
            else if (fadeDir < 0) fadeGain = std::max (0.0f, fadeGain - fadeStep);
            else if (fadeDir > 0)
            {
                fadeGain += fadeStep;
                if (fadeGain >= 1.0f) { fadeGain = 1.0f; fadeDir = 0; }
            }
            fadeArr[(size_t) i] = fadeGain;
        }

        // ---- parameter smoothing (one step per chunk, linear interpolation inside)
        const float coef = 1.0f - std::exp (-(float) len / (float) (0.025 * sr));
        sXlo.step (clampf (p.xoverLoHz, 20.0f, 16000.0f), coef);
        sXhi.step (clampf (p.xoverHiHz, 30.0f, 20000.0f), coef);
        sTiltPre.step (clampf (p.tiltPreDb, -12.0f, 12.0f), coef);
        sTiltPost.step (clampf (p.tiltPostDb, -12.0f, 12.0f), coef);
        sLowCut.step (clampf (p.lowCutHz, lowCutOffHz, 2000.0f), coef);
        sHighCut.step (clampf (p.highCutHz, 500.0f, highCutOffHz), coef);
        sLowCutOn.step (p.lowCutHz > lowCutOffHz + 0.5f ? 1.0f : 0.0f, coef);
        sHighCutOn.step (p.highCutHz < highCutOffHz - 5.0f ? 1.0f : 0.0f, coef);
        sPunch.step (clampf (p.punch, 0.0f, 1.0f), coef);
        sDyn.step (clampf (p.dynamics, -1.0f, 1.0f), coef);
        sMix.step (clampf (p.mix, 0.0f, 1.0f), coef);
        sOut.step (clampf (p.outputDb, -48.0f, 24.0f), coef);
        sKeep.step (p.lowKeepHz <= lowKeepOffHz ? 10.0f : clampf (p.lowKeepHz, 10.0f, 1000.0f), coef);
        bool anySolo = false;
        for (int b = 0; b < activeBands; ++b) anySolo |= p.band[(size_t) b].solo;
        float xfPrev[maxBands];
        for (int b = 0; b < maxBands; ++b)
        {
            const auto& bp = p.band[(size_t) b];
            sDrive[(size_t) b].step (clampf (bp.drive, 0.0f, 1.0f), coef);
            sBandMix[(size_t) b].step (clampf (bp.mix, 0.0f, 1.0f), coef);
            sBandLevel[(size_t) b].step (clampf (bp.levelDb, -48.0f, 24.0f), coef);
            sSolo[(size_t) b].step (anySolo ? (bp.solo ? 1.0f : 0.0f) : 1.0f, coef);
            const int want = std::max (0, std::min (numModels - 1, bp.model));
            if (want != curModel[(size_t) b] && modelXf[(size_t) b] >= 1.0f)
            {
                prevModel[(size_t) b] = curModel[(size_t) b];
                curModel[(size_t) b] = want;
                modelXf[(size_t) b] = 0.0f;
            }
            xfPrev[b] = modelXf[(size_t) b];
            modelXf[(size_t) b] = std::min (1.0f, modelXf[(size_t) b] + (float) len / (float) (0.006 * sr));
        }
        updateCoefficients();

        const float invLen = 1.0f / (float) len;
        const float tiltPreOn = std::abs (sTiltPre.cur) > 1.0e-4f || std::abs (sTiltPre.prev) > 1.0e-4f ? 1.0f : 0.0f;
        const float tiltPostOn = std::abs (sTiltPost.cur) > 1.0e-4f || std::abs (sTiltPost.prev) > 1.0e-4f ? 1.0f : 0.0f;

        // ---- stage 1: low keep split + tilt (pre)
        for (int c = 0; c < numCh; ++c)
        {
            auto& st = chans[(size_t) c];
            const float* x = ch[c] + start;
            for (int i = 0; i < len; ++i)
            {
                float lo, h;
                st.keepSplit.split (keepC, x[i], lo, h);
                low[(size_t) c][(size_t) i] = lo;
                hiRaw[(size_t) c][(size_t) i] = h;
                if (tiltPreOn > 0.0f)
                    h = st.tiltPreHi.process (preHi, st.tiltPreLo.process (preLo, h));
                hi[(size_t) c][(size_t) i] = h;
            }
        }

        // ---- detectors (linked): punch compression gain + drive modulation (dynamics)
        {
            const float aP = 1.0f - std::exp (-1.0f / (float) (0.010 * sr));
            const float rP = 1.0f - std::exp (-1.0f / (float) (0.120 * sr));
            const float aD = 1.0f - std::exp (-1.0f / (float) (0.001 * sr));
            const float rD = 1.0f - std::exp (-1.0f / (float) (0.060 * sr));
            const bool punchOn = sPunch.cur > 1.0e-4f || sPunch.prev > 1.0e-4f;
            const bool dynOn = std::abs (sDyn.cur) > 1.0e-4f || std::abs (sDyn.prev) > 1.0e-4f;
            float maxGr = 0.0f, lastMod = 0.0f;
            for (int i = 0; i < len; ++i)
            {
                float lvl = std::abs (hi[0][(size_t) i]);
                if (numCh > 1) lvl = std::max (lvl, std::abs (hi[1][(size_t) i]));
                punchEnv += (lvl > punchEnv ? aP : rP) * (lvl - punchEnv);
                dynEnv += (lvl > dynEnv ? aD : rD) * (lvl - dynEnv);
                const float frac = (float) (i + 1) * invLen;
                float pg = 1.0f;
                if (punchOn)
                {
                    const float pp = sPunch.at (frac);
                    const float envDb = 20.0f * std::log10 (punchEnv + 1.0e-9f);
                    const float thr = -8.0f - 22.0f * pp;
                    const float slope = 1.0f - 1.0f / (1.0f + 5.0f * pp);
                    const float over = envDb - thr;
                    float gr = 0.0f;
                    if (over >= 3.0f) gr = over * slope;
                    else if (over > -3.0f) gr = (over + 3.0f) * (over + 3.0f) / 12.0f * slope;
                    maxGr = std::max (maxGr, gr);
                    pg = dbToGain (-gr + 5.0f * pp);
                }
                punchGain[(size_t) i] = pg;
                float mod = 0.0f;
                if (dynOn)
                {
                    const float dDb = 20.0f * std::log10 (dynEnv + 1.0e-9f);
                    mod = sDyn.at (frac) * clampf ((dDb + 18.0f) * 0.6f, -12.0f, 12.0f);
                }
                driveMod[(size_t) i] = mod;
                lastMod = mod;
            }
            punchGrMeter.store (maxGr, std::memory_order_relaxed);
            dynModMeter.store (lastMod, std::memory_order_relaxed);
        }

        // ---- stage 2: oversampled multiband shaping
        const int F = factorForChoice (activeOs);
        const int nOsStages = activeOs;
        const int padN = padForChoice (activeOs);
        const int nb = activeBands;
        for (int c = 0; c < numCh; ++c)
        {
            auto& st = chans[(size_t) c];
            for (int i = 0; i < len; ++i)
            {
                const float frac = (float) (i + 1) * invLen;
                // per band gains for this base sample
                float g[maxBands], comp[maxBands], w[maxBands], a[maxBands], bmix[maxBands], lvl[maxBands], xf[maxBands];
                for (int b = 0; b < nb; ++b)
                {
                    const float d = sDrive[(size_t) b].at (frac);
                    const float baseDb = d * maxDriveDb;
                    const float effDb = clampf (baseDb + driveMod[(size_t) i], -18.0f, 48.0f);
                    g[b] = dbToGain (effDb);
                    comp[b] = dbToGain (0.25f * baseDb - 0.75f * effDb);
                    w[b] = cleanBlend (d);
                    a[b] = d;
                    bmix[b] = sBandMix[(size_t) b].at (frac);
                    lvl[b] = dbToGain (sBandLevel[(size_t) b].at (frac)) * sSolo[(size_t) b].at (frac);
                    xf[b] = xfPrev[b] + (modelXf[(size_t) b] - xfPrev[b]) * frac;
                }

                const float x = hi[(size_t) c][(size_t) i] * punchGain[(size_t) i];

                float buf[8];
                buf[0] = x;
                // upsample
                int cnt = 1;
                for (int s = 0; s < nOsStages; ++s)
                {
                    float tmp[8];
                    for (int k = 0; k < cnt; ++k)
                        st.os[(size_t) s].up (buf[k], tmp[2 * k], tmp[2 * k + 1]);
                    cnt *= 2;
                    for (int k = 0; k < cnt; ++k) buf[k] = tmp[k];
                }

                for (int k = 0; k < F; ++k)
                {
                    const float s = buf[k];
                    // the crossovers always run (warm states -> click-free band count changes)
                    float bands[maxBands];
                    float l1, h1;
                    st.split1.split (x1C, s, l1, h1);
                    const float l1ap = st.ap2.process (x2C, l1);
                    float m2, h2;
                    st.split2.split (x2C, h1, m2, h2);
                    if (nb == 1) bands[0] = s;
                    else if (nb == 2) { bands[0] = l1; bands[1] = h1; }
                    else { bands[0] = l1ap; bands[1] = m2; bands[2] = h2; }
                    float sum = 0.0f;
                    for (int b = 0; b < nb; ++b)
                    {
                        const float xb = bands[b];
                        peaks[b] = std::max (peaks[b], std::abs (xb));
                        float y = xb;
                        if (w[b] > 0.0f)
                        {
                            const float u = g[b] * xb;
                            auto& bs = st.band[(size_t) b];
                            float sh = runModel (curModel[(size_t) b], bs, u, a[b], b);
                            if (xf[b] < 1.0f)
                                sh = xf[b] * sh + (1.0f - xf[b]) * runModel (prevModel[(size_t) b], bs, u, a[b], b);
                            y = xb + w[b] * (sh * comp[b] - xb);
                        }
                        sum += (xb + bmix[b] * (y - xb)) * lvl[b];
                    }
                    // integer-latency pad at the highest rate
                    if (padN > 0)
                    {
                        st.pad[(size_t) st.padPos] = sum;
                        sum = st.pad[(size_t) ((st.padPos - padN) & 7)];
                        st.padPos = (st.padPos + 1) & 7;
                    }
                    buf[k] = sum;
                }

                // downsample
                for (int s = nOsStages - 1; s >= 0; --s)
                {
                    cnt /= 2;
                    for (int k = 0; k < cnt; ++k)
                        buf[k] = st.os[(size_t) s].down (buf[2 * k], buf[2 * k + 1]);
                }
                float y = buf[0];

                // DC block, tilt (post), low / high cut
                st.dcY = y - st.dcX + dcR * st.dcY;
                st.dcX = y;
                y = st.dcY;
                if (tiltPostOn > 0.0f)
                    y = st.tiltPostHi.process (postHi, st.tiltPostLo.process (postLo, y));
                {
                    float lp, bp;
                    st.lowCut.tick (lowCutC, y, lp, bp);
                    const float hp = y - lowCutC.k * bp - lp;
                    const float on = sLowCutOn.at (frac);
                    if (on > 0.0f) y += on * (hp - y);
                }
                {
                    float lp, bp;
                    st.highCut.tick (highCutC, y, lp, bp);
                    const float on = sHighCutOn.at (frac);
                    if (on > 0.0f) y += on * (lp - y);
                }
                proc[(size_t) c][(size_t) i] = y;
            }
        }

        // ---- auto-gain target (processed high part matched to the unprocessed high part)
        {
            float t = 0.0f;
            if (p.autoGain)
            {
                if (msIn > 1.0e-7 && msOut > 1.0e-12)
                    t = clampf ((float) (10.0 * std::log10 (msIn / msOut)), -24.0f, 24.0f);
                else
                    t = agDb;   // silence: hold
            }
            agPrev = agDb;
            agDb += (t - agDb) * (1.0f - std::exp (-(float) len / (float) (0.25 * sr)));
            if (std::abs (t - agDb) < 1.0e-4f) agDb = t;
            agDbMeter.store (agDb, std::memory_order_relaxed);
        }

        // ---- output: wet = delayed low part + processed high; global mix with the delayed dry
        const int L = latencyForChoice (activeOs);
        const double msA = 1.0 - std::exp (-1.0 / (0.6 * sr));
        const float agA = dbToGain (agPrev), agB = dbToGain (agDb);
        const float outA = dbToGain (sOut.prev), outB = dbToGain (sOut.cur);
        const float invCh = 1.0f / (float) numCh;
        for (int i = 0; i < len; ++i)
        {
            const float frac = (float) (i + 1) * invLen;
            float eIn = 0.0f, eOut = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                eIn += hiRaw[(size_t) c][(size_t) i] * hiRaw[(size_t) c][(size_t) i];
                eOut += proc[(size_t) c][(size_t) i] * proc[(size_t) c][(size_t) i];
            }
            msIn += msA * ((double) (eIn * invCh) - msIn);
            msOut += msA * ((double) (eOut * invCh) - msOut);

            const float ag = agA + (agB - agA) * frac;
            const float mix = sMix.at (frac);
            const float og = (outA + (outB - outA) * frac) * fadeArr[(size_t) i];
            const int rd = (delayPos - L) & (delaySize - 1);
            for (int c = 0; c < numCh; ++c)
            {
                auto& st = chans[(size_t) c];
                float* io = ch[c] + start;
                const float in = io[i];
                st.dryDelay[(size_t) delayPos] = in;
                st.lowDelay[(size_t) delayPos] = low[(size_t) c][(size_t) i];
                const float dry = st.dryDelay[(size_t) rd];
                const float wet = st.lowDelay[(size_t) rd] + proc[(size_t) c][(size_t) i] * ag;
                float out = dry + mix * (wet - dry);
                out *= og;
                if (! std::isfinite (out)) out = 0.0f;
                io[i] = out;
            }
            delayPos = (delayPos + 1) & (delaySize - 1);
        }
        // a non-finite state can only come from non-finite input: recover instead of latching
        if (! std::isfinite (msIn) || ! std::isfinite (msOut)) { msIn = msOut = 0.0; }
        for (int c = 0; c < numCh; ++c)
        {
            auto& st = chans[(size_t) c];
            if (! std::isfinite (st.dcY) || ! std::isfinite (st.keepSplit.l1.ic1) || ! std::isfinite (st.split1.l1.ic1)
                || ! std::isfinite (st.highCut.ic1) || ! std::isfinite (st.tiltPreLo.z1))
                st.reset();
        }
        if (! std::isfinite (punchEnv)) punchEnv = 0.0f;
        if (! std::isfinite (dynEnv)) dynEnv = 0.0f;
    }

    for (int b = 0; b < maxBands; ++b)
        bandPeak[(size_t) b].store (b < activeBands ? peaks[b] : 0.0f, std::memory_order_relaxed);
}

} // namespace heatfx
