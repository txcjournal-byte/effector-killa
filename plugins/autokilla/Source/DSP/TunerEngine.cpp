#include "TunerEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace akill
{

namespace
{
constexpr double pi = 3.14159265358979323846;

constexpr int intervals[7][8] = {
    { 0, 2, 4, 5, 7, 9, 11, -1 },  // major
    { 0, 2, 3, 5, 7, 8, 10, -1 },  // natural minor
    { 0, 2, 3, 5, 7, 8, 11, -1 },  // harmonic minor
    { 0, 2, 4, 7, 9, -1 },         // major pentatonic
    { 0, 3, 5, 7, 10, -1 },        // minor pentatonic
    { 0, 1, 3, 5, 7, 8, 10, -1 },  // phrygian
    { 0, 2, 3, 5, 7, 9, 10, -1 },  // dorian
};

// Krumhansl-Kessler key profiles
constexpr float profMajor[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
constexpr float profMinor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

inline int pcOf (int n) { return ((n % 12) + 12) % 12; }

void designLowpass (float& b0, float& b1, float& b2, float& a1, float& a2, double fc, double q, double fs)
{
    const double w = 2.0 * pi * fc / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = (float) ((1.0 - cw) * 0.5 / a0);
    b1 = (float) ((1.0 - cw) / a0);
    b2 = b0;
    a1 = (float) (-2.0 * cw / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}
} // namespace

float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (std::max (1.0f, hz) / 440.0f); }
float midiToHz (float midi) { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); }

int scaleMask (int scale, int key, int customMask)
{
    if (scale == scaleChromatic) return 0xFFF;
    if (scale == scaleCustom) return customMask & 0xFFF;
    scale = std::max (0, std::min (6, scale));
    int m = 0;
    for (int i = 0; i < 8 && intervals[scale][i] >= 0; ++i)
        m |= 1 << pcOf (intervals[scale][i] + key);
    return m;
}

int snapToMask (float midi, int mask, int previous, float hysteresis)
{
    mask &= 0xFFF;
    if (mask == 0 || ! std::isfinite (midi)) return -1;
    const int base = (int) std::lround (midi);
    int best = -1;
    float bestD = 1e9f;
    for (int n = base - 12; n <= base + 12; ++n)
    {
        if (! ((mask >> pcOf (n)) & 1)) continue;
        const float dd = std::abs ((float) n - midi);
        if (dd < bestD) { bestD = dd; best = n; }
    }
    if (previous >= 0 && ((mask >> pcOf (previous)) & 1) && std::abs ((float) previous - midi) < bestD + hysteresis)
        return previous;
    return best;
}

KeyGuess detectKey (const float* h)
{
    KeyGuess g;
    double total = 0.0;
    for (int i = 0; i < 12; ++i) total += h[i];
    if (total <= 0.0) return g;
    double hm = total / 12.0, hv = 0.0;
    for (int i = 0; i < 12; ++i) hv += (h[i] - hm) * (h[i] - hm);
    if (hv <= 1e-12) return g;
    float best = -2.0f;
    for (int minor = 0; minor < 2; ++minor)
    {
        const float* prof = minor ? profMinor : profMajor;
        double pm = 0.0;
        for (int i = 0; i < 12; ++i) pm += prof[i];
        pm /= 12.0;
        double pv = 0.0;
        for (int i = 0; i < 12; ++i) pv += (prof[i] - pm) * (prof[i] - pm);
        for (int k = 0; k < 12; ++k)
        {
            double cov = 0.0;
            for (int i = 0; i < 12; ++i)
                cov += (h[i] - hm) * (prof[pcOf (i - k)] - pm);
            const float r = (float) (cov / std::sqrt (hv * pv));
            if (r > best) { best = r; g.key = k; g.minor = minor != 0; g.score = r; }
        }
    }
    return g;
}

// ---------------- YIN ----------------

void PitchTracker::prepare (int maxTau)
{
    d.assign ((size_t) maxTau + 4, 0.0f);
    dn.assign ((size_t) maxTau + 4, 1.0f);
}

float PitchTracker::analyse (const float* x, int W, int tauMin, int tauMax, float& aper)
{
    aper = 1.0f;
    tauMax = std::min (tauMax, (int) d.size() - 3);
    if (tauMax <= tauMin + 2) return 0.0f;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        float a = 0.0f;
        const float* y = x + tau;
        for (int j = 0; j < W; ++j)
        {
            const float df = x[j] - y[j];
            a += df * df;
        }
        d[(size_t) tau] = a;
    }
    dn[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        running += d[(size_t) tau];
        dn[(size_t) tau] = running > 1e-20 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
    }
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (dn[(size_t) tau] < 0.2f)
        {
            while (tau + 1 < tauMax && dn[(size_t) tau + 1] < dn[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        float mn = 1e9f;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (dn[(size_t) tau] < mn) { mn = dn[(size_t) tau]; best = tau; }
    }
    if (best <= 1) return 0.0f;
    aper = dn[(size_t) best];
    // parabolic interpolation on the raw difference function
    float t = (float) best;
    const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
    const float den = a - 2.0f * b + c;
    if (den > 1e-20f)
        t += std::max (-0.5f, std::min (0.5f, 0.5f * (a - c) / den));
    return t;
}

// ---------------- Engine ----------------

int Engine::latencyFor (Mode m, double sampleRate)
{
    if (m == Mode::live)
    {
        const int cap = (int) std::lround (0.0065 * sampleRate);
        return cap + cap + 4 + (int) std::lround (0.004 * sampleRate);
    }
    const int cap = (int) std::lround (sampleRate / 60.0);
    return cap + cap + 4 + cap / 2;
}

Engine::ModeConfig Engine::configFor (Mode m) const
{
    ModeConfig c;
    c.minPeriod = (int) (sr / 1000.0);
    c.tauMin = std::max (2, (int) (arate / 1000.0));
    if (m == Mode::live)
    {
        c.capOut = c.capIn = (int) std::lround (0.0065 * sr);
        c.sel = (int) std::lround (0.004 * sr);
        c.maxPeriod = (int) (sr / 70.0);
        c.tauMax = (int) (arate / 70.0) + 1;
        c.window = c.tauMax;
        c.hop = std::max (16, (int) std::lround (0.0025 * sr));
        c.unvoiced = (int) std::lround (0.004 * sr);
        c.layLag = (int) (0.13 * c.maxPeriod) + 4;
    }
    else
    {
        c.capOut = c.capIn = (int) std::lround (sr / 60.0);
        c.sel = c.capIn / 2;
        c.maxPeriod = (int) (sr / 60.0);
        c.tauMax = (int) (arate / 60.0) + 1;
        c.window = c.tauMax;
        c.hop = std::max (16, (int) std::lround (0.004 * sr));
        c.unvoiced = (int) std::lround (0.005 * sr);
        c.layLag = c.capIn - 6;
    }
    c.lBase = c.capOut + c.capIn + 4;
    c.total = c.lBase + c.sel;
    return c;
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    decim = std::max (1, (int) std::lround (sr / 24000.0));
    arate = sr / decim;
    const int maxTau = (int) (arate / 55.0) + 8;
    yin.prepare (maxTau);
    int need = 1;
    while (need < (maxTau * 2 + 16) * 2) need <<= 1;
    dbuf.assign ((size_t) need, 0.0f);
    dmask = need - 1;
    ywin.assign ((size_t) (maxTau * 2 + 16), 0.0f);

    const int maxLat = std::max (latencyFor (Mode::live, sr), latencyFor (Mode::mix, sr));
    int rs = 1;
    while (rs < maxLat * 4 + 4096) rs <<= 1;
    ringSize = rs;
    ringMask = rs - 1;
    for (auto& b : in) b.assign ((size_t) rs, 0.0f);
    for (auto& b : acc) b.assign ((size_t) rs, 0.0f);
    wacc.assign ((size_t) rs, 0.0f);
    epochSig.assign ((size_t) rs, 0.0f);

    // anti-alias: 4th-order Butterworth at 40 % of the decimated Nyquist
    const double fc = 0.4 * arate * 0.5;
    designLowpass (aa1.b0, aa1.b1, aa1.b2, aa1.a1, aa1.a2, fc, 0.5412, sr);
    designLowpass (aa2.b0, aa2.b1, aa2.b2, aa2.a1, aa2.a2, fc, 1.3066, sr);
    epc = (float) (1.0 - std::exp (-2.0 * pi * 900.0 / sr));
    frameInterval = std::max (1, (int) std::lround (sr * frameSeconds));

    active = pending = params.mode;
    reset();
}

void Engine::reset()
{
    for (auto& b : in) std::fill (b.begin(), b.end(), 0.0f);
    for (auto& b : acc) std::fill (b.begin(), b.end(), 0.0f);
    std::fill (wacc.begin(), wacc.end(), 0.0f);
    std::fill (epochSig.begin(), epochSig.end(), 0.0f);
    std::fill (dbuf.begin(), dbuf.end(), 0.0f);
    aa1.z1 = aa1.z2 = aa2.z1 = aa2.z2 = 0.0f;
    ep1 = ep2 = 0.0f;
    writePos = 0;
    dwrite = 0;
    hopCounter = 0;
    voiced = false;
    lastF0 = 0.0f;
    octaveHolds = 0;
    histHead = 0;
    slowMidi = correction = 0.0f;
    targetNote = -1;
    vibPhase = vibEnv = vibTime = 0.0f;
    ratioNow = 1.0f;
    corrNow = detNow = 0.0f;
    keyRing.fill (-1);
    keyRingPos = keyRingFill = 0;
    keyHist.fill (0.0f);
    binVotes.fill (0.0f);
    binHops = binsSinceDetect = voicedBins = 0;
    binTime = 0.0;
    keyGuess = {};
    throatSm = std::pow (2.0f, params.throatSemis / 12.0f);
    mixSm = std::max (0.0f, std::min (1.0f, params.mix));
    outSm = std::pow (10.0f, std::max (-24.0f, std::min (12.0f, params.outputDb)) / 20.0f);
    modeGain = 1.0f;
    switching = false;
    frameCounter = 0;
    uiDetectedMidi = 0.0f;
    uiCorrectedMidi = 0.0f;
    uiTargetNote = -1;
    uiDetectedKey = -1;
    uiKeyProgress = 0.0f;
    applyMode (active);
}

void Engine::applyMode (Mode m)
{
    active = m;
    cfg = configFor (m);
    latencyTotal = cfg.total;
    for (auto& b : acc) std::fill (b.begin(), b.end(), 0.0f);
    std::fill (wacc.begin(), wacc.end(), 0.0f);
    markHead = markTail = cursor = 0;
    nextMark = (double) std::max (0LL, writePos - cfg.layLag);
    nextSynth = (double) (writePos - cfg.lBase + 1);
    hopCounter = 0;
}

void Engine::setParams (const Params& p)
{
    params = p;
    pending = p.mode;
}

float Engine::readIn (int ch, double pos) const
{
    const double fl = std::floor (pos);
    const long long i = (long long) fl;
    const float t = (float) (pos - fl);
    const auto& b = in[(size_t) ch];
    const float y0 = b[(size_t) ((i - 1) & ringMask)], y1 = b[(size_t) (i & ringMask)];
    const float y2 = b[(size_t) ((i + 1) & ringMask)], y3 = b[(size_t) ((i + 2) & ringMask)];
    if (t == 0.0f) return y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void Engine::pushHist (bool v)
{
    double t = pendingCentre;
    if (histHead > 0) t = std::max (t, hist[(size_t) ((histHead - 1) % histCap)].t + 1.0);
    hist[(size_t) (histHead % histCap)] = { t, v ? std::max ((float) cfg.minPeriod, std::min ((float) cfg.maxPeriod, periodSamples)) : (float) cfg.unvoiced,
                                           v ? ratioNow : 1.0f, v };
    ++histHead;
}

Engine::Hist Engine::lookup (double pos) const
{
    if (histHead == 0) return { pos, (float) cfg.unvoiced, 1.0f, false };
    const long long lo = std::max (0LL, histHead - histCap + 1);
    long long a = lo, b = histHead - 1;
    const auto at = [this] (long long i) -> const Hist& { return hist[(size_t) (i % histCap)]; };
    if (pos >= at (b).t) return at (b);
    if (pos <= at (a).t) return at (a);
    while (b - a > 1)
    {
        const long long mid = (a + b) / 2;
        if (at (mid).t <= pos) a = mid; else b = mid;
    }
    const Hist& h0 = at (a);
    const Hist& h1 = at (b);
    if (h0.voiced != h1.voiced)
        return (pos - h0.t < h1.t - pos) ? h0 : h1;
    const float f = (float) ((pos - h0.t) / std::max (1e-9, h1.t - h0.t));
    return { pos, h0.period + f * (h1.period - h0.period), h0.ratio + f * (h1.ratio - h0.ratio), h0.voiced };
}

void Engine::updateCorrection (float f0, bool isVoiced)
{
    const float hopSec = (float) (cfg.hop / sr);

    // ---- auto key: 20 ms bins of the dominant pitch class, 10 s window
    if (isVoiced)
        binVotes[(size_t) pcOf ((int) std::lround (hzToMidi (f0)))] += 1.0f;
    ++binHops;
    binTime += hopSec;
    if (binTime >= 0.02)
    {
        int pc = -1;
        float best = 0.0f;
        for (int i = 0; i < 12; ++i)
            if (binVotes[(size_t) i] > best) { best = binVotes[(size_t) i]; pc = i; }
        if (best < 0.5f * (float) binHops) pc = -1;
        const int old = keyRing[(size_t) keyRingPos];
        if (keyRingFill == keyBins && old >= 0) { keyHist[(size_t) old] -= 1.0f; --voicedBins; }
        keyRing[(size_t) keyRingPos] = (int8_t) pc;
        if (pc >= 0) { keyHist[(size_t) pc] += 1.0f; ++voicedBins; }
        keyRingPos = (keyRingPos + 1) % keyBins;
        keyRingFill = std::min (keyBins, keyRingFill + 1);
        binVotes.fill (0.0f);
        binHops = 0;
        binTime -= 0.02;
        uiKeyProgress = std::min (1.0f, (float) voicedBins / 150.0f);
        if (++binsSinceDetect >= 25)
        {
            binsSinceDetect = 0;
            if (voicedBins >= 75)
            {
                const auto g = detectKey (keyHist.data());
                if (g.key >= 0) { keyGuess = g; uiDetectedKey = g.key; uiDetectedMinor = g.minor; }
            }
        }
    }

    // ---- allowed notes
    int mask;
    if (params.midiTarget && midiMask != 0) mask = midiMask;
    else if (params.autoKey && keyGuess.key >= 0) mask = scaleMask (keyGuess.minor ? 1 : 0, keyGuess.key, 0);
    else mask = scaleMask (params.scale, params.key, params.customMask);
    maskNow = mask;
    uiMask = mask;

    if (! isVoiced)
    {
        voiced = false;
        ratioNow = 1.0f;
        detNow = corrNow = 0.0f;
        targetNote = -1;
        vibTime = 0.0f;
        pushHist (false);
        return;
    }

    const float det = hzToMidi (f0);
    const bool onset = ! voiced;
    const float aSlow = 1.0f - std::exp (-hopSec / 0.12f);
    if (onset || std::abs (det - slowMidi) > 0.75f) slowMidi = det;
    else slowMidi += aSlow * (det - slowMidi);
    const float hum = std::max (0.0f, std::min (1.0f, params.humanize));
    const float ref = det + hum * (slowMidi - det);
    if (onset) { correction = 0.0f; vibTime = 0.0f; }

    const int tn = snapToMask (ref, mask, onset ? -1 : targetNote, 0.12f);
    float desired = 0.0f;
    if (tn >= 0)
    {
        const float dev = (float) tn - ref;
        float weight = 1.0f;
        if (params.thresholdCents < 99.5f)
        {
            const float thr = std::max (0.01f, params.thresholdCents / 100.0f), a = std::abs (dev);
            weight = a <= 0.6f * thr ? 1.0f : (a >= thr ? 0.0f : (thr - a) / (0.4f * thr));
        }
        desired = weight * dev;
        if (tn != targetNote) vibTime = 0.0f;
    }
    targetNote = tn;

    const float rt = std::max (0.0f, params.retuneMs);
    const float coef = rt <= 0.5f ? 1.0f : 1.0f - std::exp (-hopSec * 1000.0f / (rt / 2.302585f));
    correction += coef * (desired - correction);

    vibPhase += (float) (2.0 * pi) * std::max (0.5f, std::min (12.0f, params.vibratoRateHz)) * hopSec;
    if (vibPhase > (float) (2.0 * pi)) vibPhase -= (float) (2.0 * pi);
    vibTime += hopSec;
    const float env = std::max (0.0f, std::min (1.0f, (vibTime - 0.12f) / 0.3f));
    const float vib = std::max (0.0f, std::min (1.0f, params.vibrato)) * 0.5f * std::sin (vibPhase) * env;

    const float shift = correction + vib + std::max (-12.0f, std::min (12.0f, params.transpose));
    ratioNow = std::pow (2.0f, shift / 12.0f);
    detNow = det;
    corrNow = det + correction + vib;
    uiCorrectionCents = correction * 100.0f;
    voiced = true;
    pushHist (true);
}

void Engine::analysisStep()
{
    // window: about 1.3 periods of the current note (short = little lag), the full range when unvoiced
    int W = cfg.window;
    if (voiced && lastF0 > 0.0f)
        W = std::max ((int) (0.35f * (float) cfg.tauMax), std::min (cfg.window, (int) (1.3 * arate / lastF0)));
    const int need = W + cfg.tauMax + 2;
    pendingCentre = (double) ((dwrite - need) * decim + (W + cfg.tauMax / 2) * decim / 2);
    if (dwrite < need)
    {
        updateCorrection (0.0f, false);
        return;
    }
    double energy = 0.0;
    for (int k = 0; k < need; ++k)
    {
        const float v = dbuf[(size_t) ((dwrite - need + k) & dmask)];
        ywin[(size_t) k] = v;
        energy += (double) v * v;
    }
    const float rms = (float) std::sqrt (energy / need);
    float aper = 1.0f, f0 = 0.0f;
    if (rms > 0.0025f)
    {
        const float tau = yin.analyse (ywin.data(), W, cfg.tauMin, cfg.tauMax, aper);
        if (tau > 0.0f)
        {
            f0 = (float) (arate / tau);
            pendingCentre = (double) (dwrite - need) * decim + 0.5 * (W + tau) * decim + 4.0;
        }
    }
    bool v = f0 >= (active == Mode::live ? 70.0f : 60.0f) && f0 <= 1000.0f && aper < (voiced ? 0.3f : 0.18f);
    if (v && voiced && lastF0 > 0.0f)
    {
        const float r = f0 / lastF0;
        if ((std::abs (r - 2.0f) < 0.08f || std::abs (r - 0.5f) < 0.03f || std::abs (r - 1.5f) < 0.05f) && octaveHolds < 4)
        {
            f0 = lastF0;   // hold through a likely octave error
            ++octaveHolds;
        }
        else
            octaveHolds = 0;
    }
    if (v)
    {
        lastF0 = f0;
        periodSamples = (float) (sr / f0);
    }
    else
        lastF0 = 0.0f;
    updateCorrection (f0, v);
    aa1.flush(); aa2.flush();
    if (ep1 * ep1 < 1e-30f) ep1 = 0.0f;
    if (ep2 * ep2 < 1e-30f) ep2 = 0.0f;
}

void Engine::layMarks()
{
    while (nextMark <= (double) (writePos - cfg.layLag))
    {
        double pos = nextMark;
        const Hist hh = lookup (pos);
        const bool mv = hh.voiced;
        const float T = mv ? hh.period : (float) cfg.unvoiced;
        if (mv)
        {
            // pull the mark towards the nearest pulse peak (glottal epoch) of the low-passed signal
            const double r = 0.12 * T;
            long long i0 = (long long) std::ceil (pos - r), i1 = (long long) std::floor (pos + r);
            i1 = std::min (i1, writePos - 1);
            long long bi = -1;
            float bv = -1e30f;
            for (long long i = i0; i <= i1; ++i)
            {
                const float e = epochSig[(size_t) (i & ringMask)];
                if (e > bv) { bv = e; bi = i; }
            }
            if (bi > i0 && bi < i1)
            {
                const float a = epochSig[(size_t) ((bi - 1) & ringMask)], c = epochSig[(size_t) ((bi + 1) & ringMask)];
                const float den = a - 2.0f * bv + c;
                double peak = (double) bi;
                if (den < -1e-12f) peak += std::max (-0.5f, std::min (0.5f, 0.5f * (a - c) / den));
                pos += 0.3 * (peak - pos);
            }
        }
        if (markHead - markTail >= markCap) { ++markTail; cursor = std::max (cursor, markTail); }
        marks[(size_t) (markHead % markCap)] = { pos, T, mv };
        ++markHead;
        nextMark = pos + T;
    }
}

void Engine::addGrain (double s)
{
    const double tauPos = s - cfg.sel;
    const double limit = (double) (writePos - cfg.capIn - 3);
    // drop marks far behind
    while (markHead - markTail > 2 && marks[(size_t) ((markTail + 1) % markCap)].pos < tauPos - 4.0 * cfg.maxPeriod)
        ++markTail;
    cursor = std::max (cursor, markTail);
    if (markHead == markTail || marks[(size_t) (markTail % markCap)].pos > limit)
    {
        nextSynth = s + cfg.unvoiced;
        return;
    }
    while (cursor + 1 < markHead && marks[(size_t) ((cursor + 1) % markCap)].pos <= tauPos)
        ++cursor;
    long long k = cursor;
    if (k + 1 < markHead)
    {
        const auto& n = marks[(size_t) ((k + 1) % markCap)];
        if (n.pos <= limit && std::abs (n.pos - tauPos) < std::abs (marks[(size_t) (k % markCap)].pos - tauPos))
            k = k + 1;
    }
    while (k > markTail && marks[(size_t) (k % markCap)].pos > limit) --k;
    const Mark& M = marks[(size_t) (k % markCap)];

    const Hist hh = lookup (M.pos);
    const float ratio = M.voiced && hh.voiced ? std::max (0.25f, std::min (4.0f, hh.ratio)) : 1.0f;
    const float T = M.period;
    float dm = T;
    if (k + 1 < markHead)
    {
        const auto& n = marks[(size_t) ((k + 1) % markCap)];
        if (n.voiced == M.voiced) dm = (float) (n.pos - M.pos);
    }
    dm = std::max (0.5f * T, std::min (1.5f * T, dm));
    const float P = std::max (dm / ratio, 0.5f * (float) cfg.minPeriod);

    const float throatTarget = std::pow (2.0f, std::max (-12.0f, std::min (12.0f, params.throatSemis)) / 12.0f);
    throatSm += 0.25f * (throatTarget - throatSm);
    float phi = throatSm * (params.preserveFormants ? 1.0f : ratio);
    phi = std::max (0.25f, std::min (4.0f, phi));

    float h = std::max (T / phi, 0.6f * P);
    h = std::min ({ h, (float) (cfg.capOut - 1), (float) (cfg.capIn - 3) / phi });
    h = std::max (h, 2.0f);
    const float X = std::max (0.3f * h, std::min (h, 2.0f * h - P));
    const float flat = h - X;

    const long long t0 = (long long) std::ceil (s - h), t1 = (long long) std::floor (s + h);
    for (long long t = t0; t <= t1; ++t)
    {
        const float u = (float) ((double) t - s);
        const float au = std::abs (u);
        const float w = au <= flat ? 1.0f : 0.5f * (1.0f + std::cos ((float) pi * (au - flat) / X));
        if (w <= 0.0f) continue;
        const double src = M.pos + (double) u * phi;
        const int o = (int) (t & ringMask);
        for (int c = 0; c < numCh; ++c)
            acc[(size_t) c][(size_t) o] += w * readIn (c, src);
        wacc[(size_t) o] += w;
    }
    nextSynth = s + P;
    // unvoiced grains drift back to the nominal latency (exact delay on consonants / noise)
    if (! M.voiced && k + 1 < markHead)
    {
        const double want = marks[(size_t) ((k + 1) % markCap)].pos + cfg.sel;
        nextSynth = std::max (s + 0.5 * P, std::min (s + 1.5 * P, want));
    }
}

void Engine::pushFrame()
{
    const uint32_t n = frameCount.load (std::memory_order_relaxed);
    const size_t i = (size_t) (n % frameCapacity);
    frameDet[i].store (voiced ? detNow : 0.0f, std::memory_order_relaxed);
    frameCorr[i].store (voiced ? corrNow : 0.0f, std::memory_order_relaxed);
    frameTarget[i].store (voiced ? targetNote : -1, std::memory_order_relaxed);
    frameCount.store (n + 1, std::memory_order_release);
    uiDetectedMidi.store (voiced ? detNow : 0.0f, std::memory_order_relaxed);
    uiCorrectedMidi.store (voiced ? corrNow : 0.0f, std::memory_order_relaxed);
    uiTargetNote.store (voiced ? targetNote : -1, std::memory_order_relaxed);
}

int Engine::readFrames (Frame* dest, int n) const
{
    const uint32_t count = frameCount.load (std::memory_order_acquire);
    const int avail = (int) std::min<uint32_t> (count, (uint32_t) frameCapacity - 8);
    n = std::min (n, avail);
    for (int k = 0; k < n; ++k)
    {
        const size_t i = (size_t) ((count - (uint32_t) n + (uint32_t) k) % frameCapacity);
        dest[k] = { frameDet[i].load (std::memory_order_relaxed), frameCorr[i].load (std::memory_order_relaxed),
                    frameTarget[i].load (std::memory_order_relaxed) };
    }
    return n;
}

void Engine::process (float* const* channels, int numChannels, int numSamples)
{
    numCh = std::max (1, std::min (numChannels, maxChannels));
    const float targetMix = std::max (0.0f, std::min (1.0f, params.mix));
    const float targetOut = std::pow (10.0f, std::max (-24.0f, std::min (12.0f, params.outputDb)) / 20.0f);
    const float pc = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
    const float modeStep = (float) (1.0 / (0.004 * sr));
    if (pending != active) switching = true;

    for (int n = 0; n < numSamples; ++n)
    {
        const long long w = writePos;
        float m = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            float x = channels[c][n];
            if (! std::isfinite (x)) x = 0.0f;
            in[(size_t) c][(size_t) (w & ringMask)] = x;
            m += x;
        }
        if (numCh == 1)
            in[1][(size_t) (w & ringMask)] = in[0][(size_t) (w & ringMask)];
        m /= (float) numCh;

        // tracker feed
        const float f = aa2.run (aa1.run (m));
        if (w % decim == 0)
            dbuf[(size_t) (dwrite++ & dmask)] = f;
        ep1 += epc * (m - ep1);
        ep2 += epc * (ep1 - ep2);
        epochSig[(size_t) (w & ringMask)] = ep2;

        if (++hopCounter >= cfg.hop)
        {
            hopCounter = 0;
            analysisStep();
        }
        layMarks();

        const long long o = w - cfg.lBase;
        int guard = 0;
        while (nextSynth <= (double) (o + cfg.capOut) && guard++ < 64)
            addGrain (nextSynth);
        if (guard >= 64) nextSynth = (double) (o + cfg.capOut + 1);

        // ---- output
        if (std::abs (targetMix - mixSm) < 1e-5f) mixSm = targetMix; else mixSm += pc * (targetMix - mixSm);
        if (std::abs (targetOut - outSm) < 1e-6f) outSm = targetOut; else outSm += pc * (targetOut - outSm);
        if (switching)
        {
            modeGain -= modeStep;
            if (modeGain <= 0.0f)
            {
                modeGain = 0.0f;
                applyMode (pending);
                switching = false;
            }
        }
        else if (modeGain < 1.0f)
            modeGain = std::min (1.0f, modeGain + modeStep);

        const long long dIdx = w - cfg.total;
        for (int c = 0; c < numCh; ++c)
        {
            float y = 0.0f;
            if (o >= 0)
            {
                const int idx = (int) (o & ringMask);
                const float ws = wacc[(size_t) idx];
                if (ws > 1e-6f) y = acc[(size_t) c][(size_t) idx] / std::max (ws, 0.1f);
                acc[(size_t) c][(size_t) idx] = 0.0f;
            }
            const float dry = dIdx >= 0 ? in[(size_t) c][(size_t) (dIdx & ringMask)] : 0.0f;
            float out = mixSm <= 0.0f ? dry : dry + (y - dry) * mixSm;
            if (outSm != 1.0f) out *= outSm;
            if (modeGain < 1.0f) out *= modeGain;
            if (! std::isfinite (out)) out = 0.0f;
            channels[c][n] = out;
        }
        if (o >= 0) wacc[(size_t) (o & ringMask)] = 0.0f;

        if (++frameCounter >= frameInterval)
        {
            frameCounter = 0;
            pushFrame();
        }
        ++writePos;
    }
}

} // namespace akill
