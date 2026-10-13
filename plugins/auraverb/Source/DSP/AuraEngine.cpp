#include "AuraEngine.h"

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define AURA_HAS_SSE 1
#endif

namespace auraverb
{

const char* const modeNames[numModes] = { "ROOM", "HALL", "PLATE", "CHAMBER", "80s DIGITAL", "SHIMMER", "DARK SPACE" };
const char* const divisionNames[numDivisions] = { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4" };
const double divisionBeats[numDivisions] = { 0.0625, 1.0 / 12.0, 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0 };

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr int kControlChunk = 16;
constexpr float kSvfK = 1.41421356f;   // Q = 0.707
constexpr float kDattorroFs = 29761.0f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// sin (2 pi p) for any p, max error ~0.1 %
inline float fastSin (float p)
{
    float t = p - std::floor (p);
    if (t >= 0.5f) t -= 1.0f;               // [-0.5, 0.5)
    const float u = 2.0f * t;                // [-1, 1)
    float y = 4.0f * u * (1.0f - std::abs (u));
    return 0.225f * (y * std::abs (y) - y) + y;
}

inline float softSat (float x)
{
    x = clampf (x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

#if AURA_HAS_SSE
struct FlushDenormals
{
    unsigned int old;
    FlushDenormals() : old (_mm_getcsr()) { _mm_setcsr (old | 0x8040); }   // FTZ | DAZ
    ~FlushDenormals() { _mm_setcsr (old); }
};
#else
struct FlushDenormals {};
#endif

// input diffusers (ms at multiplier 1)
constexpr float kDiffMsL[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
constexpr float kDiffMsR[4] = { 4.93f, 3.73f, 13.17f, 8.97f };
constexpr float kInSign[8] = { 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f };
constexpr float kLfoMul[8] = { 0.71f, 0.83f, 0.97f, 1.09f, 0.77f, 1.21f, 0.91f, 1.37f };
constexpr float kMaxLineMs = 345.0f, kMaxLoopApMs = 112.0f, kMaxErMs = 560.0f;
constexpr float kMaxDiffMul = 1.5f;
} // namespace

struct ModeSpec
{
    float lineMs[8];
    float apMs[8];
    float scaleMin, scaleMax;
    float modMaxMs;
    float xoverHz;
    float dampMul;
    float bandwidthHz;
    float diffMul;
    float apMin, apMax;
    float diffMax;
    bool linear, randomMod;
    float trim;          // late level trim
    float normK;         // unused (reserved)
    float decayComp;     // RT60 compensation: own loss time constant of the structure in s (0 = none)
    // early reflections (base pattern at size factor 1)
    int erN; float erFirst, erSpan, erK; bool erRegular; float erLevel;
};

namespace
{
// PLATE: Dattorro-style tank, lengths in ms (lines 0..3 = 4453, 3720, 4217, 3163; APs 672, 1800, 908, 2656 @ 29761 Hz)
const ModeSpec kSpecs[numModes] = {
    // ROOM
    { { 7.3f, 9.1f, 10.7f, 12.3f, 13.9f, 15.7f, 17.9f, 20.3f }, { 1.9f, 2.3f, 2.7f, 3.1f, 1.7f, 2.9f, 2.1f, 3.3f },
      0.45f, 1.6f, 0.25f, 350.0f, 1.0f, 16000.0f, 0.55f, 0.20f, 0.60f, 0.72f, false, false, 0.78f, 0.0f, 300.0f,
      12, 1.5f, 28.0f, 2.2f, false, 1.0f },
    // HALL
    { { 31.1f, 37.3f, 41.9f, 47.9f, 53.3f, 59.9f, 67.1f, 73.9f }, { 5.1f, 6.7f, 3.9f, 7.3f, 4.3f, 8.1f, 5.7f, 6.3f },
      0.5f, 1.9f, 0.7f, 250.0f, 1.0f, 14000.0f, 1.0f, 0.10f, 0.50f, 0.72f, false, false, 0.76f, 0.0f, 250.0f,
      10, 9.0f, 75.0f, 1.6f, false, 1.0f },
    // PLATE
    { { 4453.0f / 29.761f, 3720.0f / 29.761f, 4217.0f / 29.761f, 3163.0f / 29.761f, 0, 0, 0, 0 },
      { 672.0f / 29.761f, 1800.0f / 29.761f, 908.0f / 29.761f, 2656.0f / 29.761f, 0, 0, 0, 0 },
      0.3f, 1.15f, 0.45f, 300.0f, 1.3f, 18000.0f, 1.0f, 0.0f, 1.0f, 0.75f, false, false, 0.80f, 0.0f, 160.0f,
      4, 0.4f, 5.0f, 3.0f, false, 0.66f },
    // CHAMBER
    { { 13.1f, 16.3f, 19.1f, 22.7f, 25.3f, 28.9f, 31.7f, 35.3f }, { 2.9f, 3.7f, 4.3f, 3.3f, 5.1f, 2.7f, 4.7f, 3.9f },
      0.5f, 1.6f, 0.2f, 400.0f, 1.15f, 15000.0f, 0.8f, 0.25f, 0.65f, 0.75f, false, false, 0.73f, 0.0f, 300.0f,
      14, 2.5f, 45.0f, 1.8f, false, 1.0f },
    // 80s DIGITAL
    { { 19.7f, 23.9f, 29.3f, 33.1f, 37.7f, 41.3f, 46.1f, 51.7f }, { 3.1f, 4.1f, 5.3f, 3.7f, 4.9f, 2.9f, 6.1f, 4.5f },
      0.5f, 1.8f, 2.6f, 300.0f, 0.9f, 9500.0f, 0.9f, 0.05f, 0.35f, 0.60f, true, false, 0.96f, 0.0f, 85.0f,
      8, 6.0f, 60.0f, 1.2f, true, 1.2f },
    // SHIMMER
    { { 29.3f, 35.9f, 43.1f, 48.7f, 55.3f, 61.1f, 69.7f, 77.3f }, { 5.3f, 6.1f, 4.1f, 7.7f, 4.7f, 7.1f, 5.9f, 6.7f },
      0.6f, 1.9f, 0.8f, 250.0f, 1.2f, 16000.0f, 1.0f, 0.10f, 0.50f, 0.72f, false, false, 0.72f, 0.0f, 24.0f,
      6, 12.0f, 90.0f, 1.4f, false, 0.78f },
    // DARK SPACE
    { { 61.3f, 73.1f, 83.9f, 97.3f, 109.1f, 123.7f, 137.9f, 151.3f }, { 7.1f, 9.7f, 11.3f, 8.3f, 12.7f, 10.1f, 13.9f, 9.1f },
      0.6f, 2.2f, 1.6f, 200.0f, 0.5f, 6000.0f, 1.5f, 0.20f, 0.60f, 0.75f, false, true, 1.20f, 0.0f, 0.0f,
      8, 25.0f, 160.0f, 1.0f, false, 1.0f },
};

float erSizeFactor (float size) { return 0.35f + 0.9f * clampf (size, 0.0f, 1.0f); }

int basePattern (Mode m, float* t, float* g)
{
    const ModeSpec& s = kSpecs[(int) m];
    uint32_t r = 0x9E3779B9u ^ (uint32_t) ((int) m * 7919 + 17);
    auto rnd = [&r] { r = r * 1664525u + 1013904223u; return (float) (r >> 8) * (1.0f / 16777216.0f); };
    const int n = std::min (s.erN, maxEarlyTaps);
    double e = 0.0;
    for (int k = 0; k < n; ++k)
    {
        const float pos = s.erRegular ? (float) k / (float) (n - 1)
                                      : std::pow (((float) k + 0.15f + 0.7f * rnd()) / (float) n, 1.25f);
        t[k] = s.erFirst + s.erSpan * pos;
        const float sign = (rnd() > 0.45f) ? 1.0f : -1.0f;
        g[k] = sign * std::exp (-s.erK * pos) * (s.erRegular ? 0.9f + 0.1f * rnd() : 0.6f + 0.4f * rnd());
        e += (double) g[k] * g[k];
    }
    const float norm = (float) std::sqrt (0.5 / (e + 1e-12)) * s.erLevel;
    for (int k = 0; k < n; ++k) g[k] *= norm;
    return n;
}
} // namespace

double predelayMsFor (const Params& p)
{
    if (! p.predelaySync)
        return clampf (p.predelayMs, 0.0f, 500.0f);
    const int d = std::max (0, std::min (numDivisions - 1, p.predelayDiv));
    const double bpm = p.bpm > 1.0 ? p.bpm : 120.0;
    return std::min ((double) Engine::maxPredelayMs, divisionBeats[d] * 60000.0 / bpm);
}

int earlyPattern (Mode m, float size, float* timesMs, float* gains)
{
    const int n = basePattern (m, timesMs, gains);
    const float f = erSizeFactor (size);
    for (int k = 0; k < n; ++k) timesMs[k] *= f;
    return n;
}

// ---------------------------------------------------------------- primitives

void Engine::Delay::init (int minLength)
{
    int n = 16;
    while (n < minLength) n <<= 1;
    buf.assign ((size_t) n, 0.0f);
    mask = n - 1;
    w = 0;
}

inline float Engine::Delay::readCubic (float d) const
{
    // read BEFORE write; 3rd-order Lagrange on points k-1..k+2 (passive, |H| <= 1)
    const float pos = (float) w - d;
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

inline float Engine::Shifter::process (float x)
{
    phase -= inc;
    if (phase < 0.0f) phase += 1.0f;
    float p2 = phase + 0.5f;
    if (p2 >= 1.0f) p2 -= 1.0f;
    const float w1 = 0.5f - 0.5f * fastSin (phase + 0.25f);   // sin^2 (pi phase): 0 where the tap jumps
    const float y = w1 * d.read (2.0f + phase * win) + (1.0f - w1) * d.read (2.0f + p2 * win);
    d.write (x);
    return y;
}

// ---------------------------------------------------------------- tank

void Engine::Tank::allocate (double sampleRate)
{
    sr = sampleRate;
    const float ms = (float) sr * 0.001f;
    erL.init ((int) (kMaxErMs * ms) + 16);
    erR.init ((int) (kMaxErMs * ms) + 16);
    for (auto& a : erAp) a.d.init ((int) (8.0f * ms) + 4);
    for (int j = 0; j < 4; ++j)
    {
        diffL[(size_t) j].d.init ((int) (kDiffMsL[j] * kMaxDiffMul * ms) + 4);
        diffR[(size_t) j].d.init ((int) (kDiffMsR[j] * kMaxDiffMul * ms) + 4);
    }
    for (int i = 0; i < numLines; ++i)
    {
        lines[(size_t) i].init ((int) (kMaxLineMs * ms) + 32);
        loopAp[(size_t) i].d.init ((int) (kMaxLoopApMs * ms) + 32);
    }
    shOct.d.init ((int) (0.09 * sr) + 8);
    shFifth.d.init ((int) (0.09 * sr) + 8);
    shOct.setup (sr, 2.0f, 0.060f);
    shFifth.setup (sr, 1.5f, 0.080f);
    chL.init ((int) (16.0f * ms) + 8);
    chR.init ((int) (16.0f * ms) + 8);

    bwGs.setTime (sr, 0.03);
    erScale.setTime (sr, 0.2);
    buildSpan.setTime (sr, 0.2);
    attackS.setTime (sr, 0.05);
    diffCoef.setTime (sr, 0.05);
    for (auto& g : gHigh) g.setTime (sr, 0.03);
    for (auto& g : gLow) g.setTime (sr, 0.03);
    scale.setTime (sr, 0.25);
    modDepth.setTime (sr, 0.10);
    dampCoef.setTime (sr, 0.03);
    apCoef.setTime (sr, 0.05);
    inGain.setTime (sr, 0.03);
    shimAmt.setTime (sr, 0.10);
    earlyGain.setTime (sr, 0.03);
    lateGain.setTime (sr, 0.03);
    wetNorm.setTime (sr, 0.05);
    hfGs.setTime (sr, 0.03);
    configure (mode);
}

void Engine::Tank::clear()
{
    bwL.clear(); bwR.clear(); hfL.clear(); hfR.clear();
    erL.clear(); erR.clear();
    for (auto& a : erAp) a.d.clear();
    for (auto& a : diffL) a.d.clear();
    for (auto& a : diffR) a.d.clear();
    for (auto& l : lines) l.clear();
    for (auto& a : loopAp) a.d.clear();
    shOct.d.clear(); shFifth.d.clear();
    chL.clear(); chR.clear();
    std::fill (std::begin (lpLow), std::end (lpLow), 0.0f);
    std::fill (std::begin (damp), std::end (damp), 0.0f);
    plateFbL = plateFbR = 0.0f;
    shHp = shLp = 0.0f;
}

void Engine::Tank::configure (Mode m)
{
    mode = m;
    spec = &kSpecs[(int) m];
    plate = m == Mode::plate;
    const float ms = (float) sr * 0.001f;
    for (int i = 0; i < numLines; ++i)
    {
        baseLen[i] = spec->lineMs[i] * ms;
        loopApLen[i] = std::max (1, (int) std::lround (spec->apMs[i] * ms));
        lfoPhase[i] = (float) i / (float) numLines;
        lfoMul[i] = kLfoMul[i];
    }
    for (int j = 0; j < 4; ++j)
    {
        plateAp[j] = spec->apMs[j] * ms;
        diffLenL[j] = std::max (1, (int) std::lround (kDiffMsL[j] * spec->diffMul * ms));
        diffLenR[j] = std::max (1, (int) std::lround (kDiffMsR[j] * spec->diffMul * ms));
    }
    erApLen[0] = std::max (1, (int) std::lround (2.3f * ms));
    erApLen[1] = std::max (1, (int) std::lround (2.9f * ms));
    erCount = basePattern (m, erMs, erGain);
    rng = 0x2545F491u ^ (uint32_t) ((int) m * 104729);
    for (int i = 0; i < numLines; ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        rndFrom[i] = 0.0f;
        rndTo[i] = (float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }
    xoverCoef = 1.0f - std::exp (-2.0f * kPi * spec->xoverHz / (float) sr);
    chPhase = 0.0f;
    shOct.phase = 0.0f;
    shFifth.phase = 0.37f;
    clear();
}

void Engine::Tank::setTargets (const Params& p)
{
    const ModeSpec& s = *spec;
    const float fs = (float) sr, ms = fs * 0.001f;
    const bool frozen = p.freeze;
    vintage = p.vintage;

    const float size = clampf (p.size, 0.0f, 1.0f);
    scale.t = s.scaleMin + (s.scaleMax - s.scaleMin) * std::pow (size, 1.3f);
    erScale.t = erSizeFactor (size);
    const float att = clampf (p.attack, 0.0f, 1.0f);
    attackS.t = att;
    buildSpan.t = std::max (1.0f, (60.0f + 440.0f * size) * att * ms);
    const float diff = clampf (p.diffusion, 0.0f, 1.0f);
    diffCoef.t = s.diffMax * diff;
    apCoef.t = plate ? diff : s.apMin + (s.apMax - s.apMin) * diff;
    modDepth.t = clampf (p.modDepth, 0.0f, 1.0f) * s.modMaxMs * (vintage ? 1.4f : 1.0f) * ms;
    modRate = clampf (p.modRateHz, 0.02f, 10.0f) * (vintage ? 0.8f : 1.0f);

    const float fc = std::min (clampf (p.dampingHz, 200.0f, 20000.0f) * s.dampMul, 0.45f * fs);
    dampCoef.t = (frozen || p.dampingHz >= 19500.0f) ? 0.0f : std::exp (-2.0f * kPi * fc / fs);
    const float bwHz = std::min (vintage ? std::min (s.bandwidthHz, 8500.0f) : s.bandwidthHz, 0.45f * fs);
    bwGs.t = std::tan (kPi * bwHz / fs);
    inGain.t = frozen ? 0.0f : 1.0f;
    shimAmt.t = (mode == Mode::shimmer && ! frozen) ? 0.38f : 0.0f;

    // the structures lose a little energy of their own (interpolation, pitch shifting): compensate so RT60 follows DECAY
    auto comp = [&s] (float t) { return s.decayComp > 0.0f ? t / std::max (0.08f, 1.0f - t / s.decayComp) : t; };
    const float Tset = clampf (p.decaySec, minDecay, maxDecay);
    const float T = comp (Tset);
    const float Tl = comp (Tset * clampf (p.bassMult, 0.25f, 2.5f));
    const float sc = scale.v, dep = modDepth.v;
    float meanLen = 0.0f;
    if (plate)
    {
        const float half[2] = { (baseLen[0] + baseLen[1] + plateAp[0] + plateAp[1]) * sc + dep + 2.0f,
                                (baseLen[2] + baseLen[3] + plateAp[2] + plateAp[3]) * sc + dep + 2.0f };
        for (int h = 0; h < 2; ++h)
        {
            gHigh[h].t = frozen ? 1.0f : std::pow (10.0f, -3.0f * half[h] / (T * fs));
            gLow[h].t = frozen ? 1.0f : std::pow (10.0f, -3.0f * half[h] / (Tl * fs));
        }
        meanLen = 0.5f * (half[0] + half[1]) / fs;
    }
    else
    {
        for (int i = 0; i < numLines; ++i)
        {
            const float leff = baseLen[i] * sc + (float) loopApLen[i] + dep + 4.0f;
            gHigh[i].t = frozen ? 1.0f : std::pow (10.0f, -3.0f * leff / (T * fs));
            gLow[i].t = frozen ? 1.0f : std::pow (10.0f, -3.0f * leff / (Tl * fs));
            meanLen += leff;
        }
        meanLen /= (float) numLines * fs;
    }

    const float el = clampf (p.earlyLate, 0.0f, 1.0f);
    earlyGain.t = std::min (1.0f, 2.0f * (1.0f - el));
    lateGain.t = std::min (1.0f, 2.0f * el);
    // steady-state tail energy ~ 1 / (1 - g^2) per pass: normalise it away (freeze keeps the level of the set decay)
    const float Tn = std::min (clampf (p.decaySec, minDecay, maxDecay), 20.0f);
    const float g2 = std::pow (10.0f, -6.0f * meanLen / Tn);
    // (short decays in big spaces: lift the heavily attenuated first passes a bit)
    wetNorm.t = s.trim * std::pow (1.0f - g2, 0.33f) * std::sqrt (1.3f / (g2 + 0.3f));

    bitsQ = mode == Mode::digital80s ? (vintage ? 2048.0f : 8192.0f) : 0.0f;   // 12 / 14 bit
    float hfHz = 0.0f;
    if (mode == Mode::digital80s) hfHz = vintage ? 6000.0f : 9500.0f;
    else if (vintage) hfHz = 9000.0f;
    hfOn = hfHz > 0.0f;
    hfGs.t = std::tan (kPi * std::min (hfOn ? hfHz : 20000.0f, 0.45f * fs) / fs);
}

void Engine::Tank::snapSmoothers()
{
    for (Smooth* s : { &bwGs, &erScale, &buildSpan, &attackS, &diffCoef, &scale, &modDepth, &dampCoef, &apCoef,
                       &inGain, &shimAmt, &earlyGain, &lateGain, &wetNorm, &hfGs })
        s->snap();
    for (auto& g : gHigh) g.snap();
    for (auto& g : gLow) g.snap();
}

inline void Engine::Tank::process (float inL, float inR, float& outL, float& outR)
{
    const float fs = (float) sr;
    const float bw = bwGs.next();
    const float xl = bwL.lp (inL, bw, kSvfK), xr = bwR.lp (inR, bw, kSvfK);

    // ---- early reflections (read before write)
    const float es = erScale.next() * fs * 0.001f;
    const float dg = diffCoef.next();
    float eL = 0.0f, eR = 0.0f;
    for (int j = 0; j < erCount; ++j)
    {
        const float t = erMs[j] * es + 1.0f, tr = erMs[j] * es * 1.071f + 0.3f * fs * 0.001f + 1.0f, g = erGain[j];
        if (j & 1) { eL += g * erR.read (t); eR += g * erL.read (tr); }
        else       { eL += g * erL.read (t); eR += g * erR.read (tr); }
    }
    eL = erAp[0].processInt (eL, erApLen[0], 0.25f + 0.35f * dg);
    eR = erAp[1].processInt (eR, erApLen[1], 0.25f + 0.35f * dg);

    // ---- build / attack: a swelling smear of taps feeds the tail
    const float a = attackS.next();
    const float span = buildSpan.next();
    float ll = xl, lr = xr;
    if (a > 1.0e-4f)
    {
        constexpr float w[6] = { 0.0857f, 0.1714f, 0.2571f, 0.3428f, 0.4285f, 0.7714f };   // rising, sum of squares ~ 1
        float bl = 0.0f, br = 0.0f;
        for (int j = 0; j < 6; ++j)
        {
            const float tj = span * (float) (j + 1) * (1.0f / 6.0f) + 1.0f;
            bl += w[j] * erL.read (tj);
            br += w[j] * erR.read (tj * 1.047f + 2.0f);
        }
        const float ca = std::sqrt (std::max (0.0f, 1.0f - a * a));
        ll = ca * xl + a * bl;
        lr = ca * xr + a * br;
    }
    erL.write (xl);
    erR.write (xr);

    // ---- input diffusion
    for (int j = 0; j < 4; ++j)
    {
        const float g = plate ? dg * (j < 2 ? 1.0f : 0.8333f) : dg;
        ll = diffL[(size_t) j].processInt (ll, diffLenL[j], g);
        lr = diffR[(size_t) j].processInt (lr, diffLenR[j], g);
    }
    const float ig = inGain.next();

    // ---- late structure
    const float sc = scale.next(), dep = modDepth.next(), dc = dampCoef.next(), apg = apCoef.next(), sa = shimAmt.next();
    const float rateInc = modRate / fs;
    float wl = 0.0f, wr = 0.0f;
    denorm = -denorm;

    if (plate)
    {
        for (int i = 0; i < 2; ++i)
        {
            lfoPhase[i] += rateInc * (i == 0 ? 0.9f : 1.13f);
            if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
        }
        const float m0 = fastSin (lfoPhase[0]), m1 = fastSin (lfoPhase[1]);
        const float dd1 = 0.30f + 0.42f * apg, dd2 = 0.20f + 0.35f * apg;
        const float g0h = gHigh[0].next(), g0l = gLow[0].next(), g1h = gHigh[1].next(), g1l = gLow[1].next();

        // left half
        float x = ig * 0.6f * ll + plateFbR;
        x = loopAp[0].process (x, plateAp[0] * sc + dep * (1.0f + m0) + 2.0f, -dd1);
        const float y0 = lines[0].read (baseLen[0] * sc);
        lines[0].write (x);
        damp[0] = y0 + dc * (damp[0] - y0);
        lpLow[0] += xoverCoef * (damp[0] - lpLow[0]);
        float t0 = g0l * lpLow[0] + g0h * (damp[0] - lpLow[0]);
        t0 = loopAp[1].process (t0, plateAp[1] * sc, dd2);
        const float y1 = lines[1].read (baseLen[1] * sc);
        lines[1].write (t0);

        // right half
        float xr2 = ig * 0.6f * lr + plateFbL;
        xr2 = loopAp[2].process (xr2, plateAp[2] * sc + dep * (1.0f + m1) + 2.0f, -dd1);
        const float y2 = lines[2].read (baseLen[2] * sc);
        lines[2].write (xr2);
        damp[1] = y2 + dc * (damp[1] - y2);
        lpLow[1] += xoverCoef * (damp[1] - lpLow[1]);
        float t1 = g1l * lpLow[1] + g1h * (damp[1] - lpLow[1]);
        t1 = loopAp[3].process (t1, plateAp[3] * sc, dd2);
        const float y3 = lines[3].read (baseLen[3] * sc);
        lines[3].write (t1);

        plateFbL = y1 + denorm;
        plateFbR = y3 + denorm;

        // multi-tap stereo output (tap positions in samples @ 29.761 kHz, scaled)
        const float k = sc * fs / kDattorroFs;
        wl = lines[2].read (266.0f * k + 1.0f) + lines[2].read (2974.0f * k) - loopAp[3].d.read (1913.0f * k)
           + lines[3].read (1996.0f * k) - lines[0].read (1990.0f * k) - loopAp[1].d.read (187.0f * k + 1.0f)
           - lines[1].read (1066.0f * k);
        wr = lines[0].read (353.0f * k + 1.0f) + lines[0].read (3627.0f * k) - loopAp[1].d.read (1228.0f * k)
           + lines[1].read (2673.0f * k) - lines[2].read (2111.0f * k) - loopAp[3].d.read (335.0f * k + 1.0f)
           - lines[3].read (121.0f * k + 1.0f);
        wl *= 0.6f;
        wr *= 0.6f;
    }
    else
    {
        const bool lin = spec->linear, rnd = spec->randomMod;
        float v[numLines];
        for (int i = 0; i < numLines; ++i)
        {
            float m;
            if (rnd)
            {
                lfoPhase[i] += rateInc * lfoMul[i] * 0.5f;
                if (lfoPhase[i] >= 1.0f)
                {
                    lfoPhase[i] -= 1.0f;
                    rndFrom[i] = rndTo[i];
                    rng = rng * 1664525u + 1013904223u;
                    rndTo[i] = (float) (rng >> 8) * (2.0f / 16777216.0f) - 1.0f;
                }
                const float f = lfoPhase[i], sm = f * f * (3.0f - 2.0f * f);
                m = rndFrom[i] + (rndTo[i] - rndFrom[i]) * sm;
            }
            else
            {
                lfoPhase[i] += rateInc * lfoMul[i];
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                m = fastSin (lfoPhase[i]);
            }
            const float dly = baseLen[i] * sc + dep * (1.0f + m) + 4.0f;
            float y = lin ? lines[(size_t) i].read (dly) : lines[(size_t) i].readCubic (dly);
            y = loopAp[(size_t) i].processInt (y, loopApLen[i], apg);
            damp[i] = y + dc * (damp[i] - y);
            lpLow[i] += xoverCoef * (damp[i] - lpLow[i]);
            v[i] = gLow[i].next() * lpLow[i] + gHigh[i].next() * (damp[i] - lpLow[i]);
        }
        wl = v[0] - v[1] + v[2] - v[3] + v[4] - v[5] + v[6] - v[7];
        wr = v[0] + v[1] - v[2] - v[3] + v[4] + v[5] - v[6] - v[7];

        // shimmer: octave + fifth up, injected as a convex blend (never adds energy)
        float sh = 0.0f;
        if (mode == Mode::shimmer)
        {
            const float mono = 0.5f * (v[0] + v[3] + v[5] + v[6]);
            shHp += 0.033f * 48000.0f / fs * (mono - shHp);          // ~250 Hz high-pass
            const float h = mono - shHp;
            shLp += std::min (1.0f, 0.55f * 48000.0f / fs) * (h - shLp);   // ~6 kHz low-pass
            sh = 0.62f * shOct.process (shLp) + 0.38f * shFifth.process (shLp);
        }

        for (int h = 1; h < numLines; h <<= 1)
            for (int i = 0; i < numLines; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float p0 = v[j], p1 = v[j + h];
                    v[j] = p0 + p1;
                    v[j + h] = p0 - p1;
                }
        constexpr float hScale = 0.35355339f;
        // lines 1 and 4 trade part of their content for the pitch-shifted signal (energy preserving)
        const float keep = hScale * std::sqrt (1.0f - sa * sa), inj = sa * sh * 0.70710678f;
        const float igs = ig * 0.5f;
        for (int i = 0; i < numLines; ++i)
        {
            float x = igs * kInSign[i] * ((i & 1) ? lr : ll) + denorm;
            if (i == 1) x += v[i] * keep + inj;
            else if (i == 4) x += v[i] * keep - inj;
            else x += v[i] * hScale;
            if (mode == Mode::shimmer) x = 4.0f * softSat (0.25f * x);   // safety limiter for the pitch loop
            lines[(size_t) i].write (x);
        }
    }

    // ---- output colour
    const float eg = earlyGain.next() * ig, lg = lateGain.next(), wn = wetNorm.next();   // frozen: no new reflections
    float oL = eL * eg + wl * lg * wn;
    float oR = eR * eg + wr * lg * wn;
    const float hg = hfGs.next();
    if (hfOn)
    {
        oL = hfL.lp (oL, hg, kSvfK);
        oR = hfR.lp (oR, hg, kSvfK);
    }
    if (mode == Mode::digital80s)
    {
        chL.write (oL);
        chR.write (oR);
        chPhase += 0.83f / fs;
        if (chPhase >= 1.0f) chPhase -= 1.0f;
        const float base = 6.0f * fs * 0.001f, d = 1.6f * fs * 0.001f;
        const float cl = chL.read (base + d * (1.0f + fastSin (chPhase)));
        const float cr = chR.read (base + d * (1.0f + fastSin (chPhase + 0.25f)));
        oL = 0.72f * oL + 0.62f * cl;
        oR = 0.72f * oR + 0.62f * cr;
        oL = std::floor (oL * bitsQ + 0.5f) / bitsQ;
        oR = std::floor (oR * bitsQ + 0.5f) / bitsQ;
    }
    if (vintage)
    {
        oL = softSat (oL * 1.15f) * 0.87f;
        oR = softSat (oR * 1.15f) * 0.87f;
    }
    outL = oL;
    outR = oR;
}

// ---------------------------------------------------------------- engine

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    preL.init ((int) (maxPredelayMs * (float) sr * 0.001f) + 8);
    preR.init ((int) (maxPredelayMs * (float) sr * 0.001f) + 8);
    for (auto& t : tanks) t.allocate (sr);

    preSamples.setTime (sr, 0.12);
    widthS.setTime (sr, 0.03);
    dryS.setTime (sr, 0.03);
    wetS.setTime (sr, 0.03);
    outS.setTime (sr, 0.03);
    duckGain.setTime (sr, 0.012);
    duckAtt = (float) (1.0 - std::exp (-1.0 / (0.003 * sr)));
    duckRel = (float) (1.0 - std::exp (-1.0 / (0.07 * sr)));
    duckUp = (float) (1.0 - std::exp (-1.0 / (0.16 * sr)));
    fadeInc = (float) (1.0 / (modeFadeSeconds * sr));

    prepared = true;
    reset();
}

void Engine::reset()
{
    if (! prepared) return;
    preL.clear(); preR.clear();
    hpL.clear(); hpR.clear();
    fading = false;
    fadePos = 0.0f;
    active = 0;
    tanks[1].configure (tanks[1].mode);
    tanks[0].configure (target.mode);
    for (auto& t : tanks)
    {
        t.setTargets (target);
        t.snapSmoothers();
        t.setTargets (target);
        t.snapSmoothers();
    }
    duckEnv = 0.0f;
    inLevel = wetLevel = 0.0f;
    updateControl();
    for (Smooth* s : { &preSamples, &widthS, &dryS, &wetS, &outS, &duckGain })
        s->snap();
    duckGainNow = duckGain.v;
    controlCounter = kControlChunk;
}

void Engine::updateControl()
{
    const Params& p = target;
    const float fs = (float) sr;

    if (! fading && p.mode != tanks[active].mode)
    {
        const int other = 1 - active;
        tanks[other].configure (p.mode);
        tanks[other].setTargets (p);
        tanks[other].snapSmoothers();
        active = other;
        fading = true;
        fadePos = 0.0f;
    }
    tanks[active].setTargets (p);
    if (fading) tanks[1 - active].setTargets (p);

    preSamples.t = (float) std::min ((double) maxPredelayMs, std::max (0.0, predelayMsFor (p))) * fs * 0.001f;
    hpG = std::tan (kPi * std::min (clampf (p.lowCutHz, 10.0f, 2000.0f), 0.45f * fs) / fs);
    widthS.t = clampf (p.width, 0.0f, 2.0f);
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

void Engine::process (float* const* ch, int numCh, int numSamples)
{
    if (! prepared || numCh <= 0 || numSamples <= 0) return;
    FlushDenormals ftz;
    (void) ftz;

    float* outL = ch[0];
    float* outR = numCh > 1 ? ch[1] : nullptr;
    float peakIn = 0.0f, peakWet = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        if (--controlCounter <= 0)
        {
            updateControl();
            controlCounter = kControlChunk;
        }

        const float xl = outL[n];
        const float xr = outR != nullptr ? outR[n] : xl;

        const float a = std::max (std::abs (xl), std::abs (xr));
        duckEnv += (a > duckEnv ? duckAtt : duckRel) * (a - duckEnv);
        peakIn = std::max (peakIn, a);

        // ---- pre-delay (write, then read: delay 0 = this sample)
        preL.write (xl);
        preR.write (xr);
        const float pd = preSamples.next() + 1.0f;
        const float pl = preL.read (pd), pr = preR.read (pd);

        // ---- tank(s)
        float wl, wr;
        if (fading)
        {
            // the incoming tank gets its input faded in as well: no step into an empty structure
            const float ramp = fadePos * fadePos * (3.0f - 2.0f * fadePos);
            tanks[active].process (pl * ramp, pr * ramp, wl, wr);
            float ol, orr;
            tanks[1 - active].process (pl, pr, ol, orr);
            fadePos += fadeInc;
            if (fadePos >= 1.0f)
            {
                fading = false;
                fadePos = 1.0f;
            }
            const float gNew = std::sin (fadePos * kPi * 0.5f), gOld = std::cos (fadePos * kPi * 0.5f);
            wl = wl * gNew + ol * gOld;
            wr = wr * gNew + orr * gOld;
        }
        else
            tanks[active].process (pl, pr, wl, wr);

        // ---- low-cut, width, ducking, mix
        wl = hpL.hp (wl, hpG, kSvfK);
        wr = hpR.hp (wr, hpG, kSvfK);
        const float wd = widthS.next();
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * wd;
        wl = mid + side;
        wr = mid - side;
        peakWet = std::max (peakWet, std::max (std::abs (wl), std::abs (wr)));

        if (duckGain.v > duckGain.t) duckGain.next();
        else duckGain.v = std::min (duckGain.t, duckGain.v + duckUp * (duckGain.t - duckGain.v) + 1.0e-7f);
        const float wetG = wetS.next() * duckGain.v;
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

    // safety net: never let a non-finite value live in the tanks
    bool finite = std::isfinite (peakWet);
    for (const auto& t : tanks)
        for (float d : t.damp) finite = finite && std::isfinite (d);
    finite = finite && std::isfinite (tanks[0].plateFbL) && std::isfinite (tanks[1].plateFbR);
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

} // namespace auraverb
