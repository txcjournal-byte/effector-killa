#include "SweepEngine.h"

#include <algorithm>
#include <cmath>

namespace sweepfx
{

namespace
{
constexpr double twoPi = 6.283185307179586;
constexpr float ln2 = 0.69314718f;
inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float sanitize (float v) { return std::isfinite (v) ? v : 0.0f; }
inline float coefFor (double seconds, double sr) { return (float) (1.0 - std::exp (-1.0 / (std::max (1.0e-6, seconds) * sr))); }
inline double frac (double v) { return v - std::floor (v); }
constexpr float dbPerLn = 8.6858896f;

inline float dampingFor (float reso)
{
    const float r = 1.0f - clampf (reso, 0.0f, 1.0f);
    return kMin + (2.0f - kMin) * r * std::sqrt (r);
}

inline float driveShape (float x, float drive)
{
    if (drive < 1.0e-4f) return x;
    const float dg = 1.0f + drive * 15.0f;
    return std::tanh (x * dg) * (0.5f / std::tanh (0.5f * dg));
}

inline float safety (float y)
{
    const float a = std::abs (y);
    if (a <= 2.0f) return y;
    const float s = 2.0f + 2.0f * std::tanh ((a - 2.0f) * 0.5f);
    return y < 0.0f ? -s : s;
}

// vowel formants (dark -> bright): U O A E I
struct Vowel { float f[3]; float g[3]; };
constexpr Vowel vowels[5] = {
    { { 325.0f, 700.0f, 2530.0f }, { 1.0f, 0.35f, 0.10f } },
    { { 450.0f, 800.0f, 2830.0f }, { 1.0f, 0.50f, 0.15f } },
    { { 800.0f, 1150.0f, 2900.0f }, { 1.0f, 0.55f, 0.20f } },
    { { 400.0f, 2000.0f, 2800.0f }, { 1.0f, 0.40f, 0.25f } },
    { { 270.0f, 2300.0f, 3000.0f }, { 1.0f, 0.30f, 0.25f } },
};

inline void formantsFor (float cutoff, float* f, float* g)
{
    const float pos = clampf (std::log (cutoff / 150.0f) / std::log (6000.0f / 150.0f), 0.0f, 1.0f) * 4.0f;
    const int i0 = std::min (3, (int) pos);
    const float t = pos - (float) i0;
    for (int k = 0; k < 3; ++k)
    {
        f[k] = std::exp (std::log (vowels[i0].f[k]) + t * (std::log (vowels[i0 + 1].f[k]) - std::log (vowels[i0].f[k])));
        g[k] = vowels[i0].g[k] + t * (vowels[i0 + 1].g[k] - vowels[i0].g[k]);
    }
}

inline float formantQ (float reso) { return 5.0f + 15.0f * clampf (reso, 0.0f, 1.0f); }
} // namespace

// ---------------- static helpers ----------------

double cycleSeconds (const Params& p, double bpm)
{
    const bool steps = p.shape == Shape::pattern;
    if (p.sync)
    {
        const double beats = divisionBeats[std::clamp (p.division, 0, numDivisions - 1)];
        const double b = bpm > 1.0 ? bpm : 140.0;
        return beats * 60.0 / b * (steps ? (double) numSteps : 1.0);
    }
    const double hz = std::clamp ((double) (std::isfinite (p.rateHz) ? p.rateHz : 1.0f), 0.01, 50.0);
    return (steps ? (double) numSteps : 1.0) / hz;
}

float lfoShapeValue (Shape s, double ph, const std::array<float, numSteps>& pattern, uint32_t shRandom)
{
    ph = frac (ph);
    switch (s)
    {
        case Shape::sine:       return (float) (0.5 - 0.5 * std::cos (twoPi * ph));
        case Shape::triangle:   return (float) (ph < 0.5 ? 2.0 * ph : 2.0 - 2.0 * ph);
        case Shape::sawUp:      return (float) ph;
        case Shape::sawDown:    return (float) (1.0 - ph);
        case Shape::square:     return ph < 0.5 ? 1.0f : 0.0f;
        case Shape::sampleHold: return (float) (shRandom >> 8) / 16777216.0f;
        case Shape::pattern:
        default:
        {
            const int st = std::clamp ((int) (ph * numSteps), 0, numSteps - 1);
            return clampf (sanitize (pattern[(size_t) st]), 0.0f, 1.0f);
        }
    }
}

float magnitudeDb (const FilterParams& f, float cutoffHz, float freqHz, double sr)
{
    const float fc = clampf (cutoffHz, minCutoff, (float) std::min ((double) maxCutoff, sr * 0.45));
    const double w = twoPi * freqHz / sr;
    if (f.type == FilterType::comb)
    {
        // y = x + fb * z^-D y,  out = y * sqrt(1 - fb^2)
        const double fb = 0.3 + 0.66 * clampf (f.reso, 0.0f, 1.0f);
        const double D = sr / fc;
        const double re = 1.0 - fb * std::cos (w * D), im = fb * std::sin (w * D);
        const double mag = std::sqrt (1.0 - fb * fb) / std::sqrt (re * re + im * im);
        return (float) (20.0 * std::log10 (std::max (1.0e-9, mag)));
    }
    // bilinear-warped analog prototypes evaluated at the warped frequency (matches the TPT SVF exactly)
    const double g0 = std::tan (3.14159265358979 * fc / sr);
    const double wa = std::tan (w * 0.5) / g0;   // normalised analog frequency
    auto svf = [wa] (double k, int tap) // |H| of the 2-pole section: tap 0 lp, 1 bp(k-normalised), 2 hp, 3 notch
    {
        const double re = 1.0 - wa * wa, im = k * wa;
        const double den = std::sqrt (re * re + im * im);
        switch (tap)
        {
            case 0: return 1.0 / den;
            case 1: return k * wa / den;
            case 2: return wa * wa / den;
            default: return std::abs (1.0 - wa * wa) / den;
        }
    };
    const double k = dampingFor (f.reso);
    double mag = 1.0;
    switch (f.type)
    {
        case FilterType::lp12:  mag = svf (k, 0); break;
        case FilterType::hp12:  mag = svf (k, 2); break;
        case FilterType::lp24:  mag = svf (1.848, 0) * svf (std::min (k, 0.765), 0); break;
        case FilterType::hp24:  mag = svf (1.848, 2) * svf (std::min (k, 0.765), 2); break;
        case FilterType::bp:    mag = svf (k, 1); break;
        case FilterType::notch: mag = svf (k, 3); break;
        case FilterType::formant:
        {
            float ff[3], gg[3];
            formantsFor (fc, ff, gg);
            const double q = formantQ (f.reso);
            double re = 0, im = 0;
            for (int i = 0; i < 3; ++i)
            {
                const double gi = std::tan (3.14159265358979 * std::min ((double) ff[i], sr * 0.45) / sr);
                const double x = std::tan (w * 0.5) / gi, kk = 1.0 / q;
                // H = kk*j*x / (1 - x^2 + j*kk*x)
                const double dr = 1.0 - x * x, di = kk * x;
                const double den = dr * dr + di * di;
                const double nr = 0.0, ni = kk * x;
                re += gg[i] * 2.0 * (nr * dr + ni * di) / den;
                im += gg[i] * 2.0 * (ni * dr - nr * di) / den;
            }
            mag = std::sqrt (re * re + im * im);
            break;
        }
        case FilterType::comb: break;
    }
    if (f.drive > 1.0e-4f)
    {
        const float dg = 1.0f + f.drive * 15.0f;
        mag *= dg * (0.5f / std::tanh (0.5f * dg)) / (1.0f + 0.35f * dg);   // rough small-signal level
    }
    return (float) (20.0 * std::log10 (std::max (1.0e-9, mag)));
}

float responseDb (const Params& p, float cutoffA, float cutoffB, float freqHz, double sr)
{
    const float a = magnitudeDb (p.a, cutoffA, freqHz, sr);
    if (! p.b.on) return a;
    const float b = magnitudeDb (p.b, cutoffB, freqHz, sr);
    if (p.routing == Routing::serial) return a + b;
    // parallel: magnitudes summed (phase ignored for the display)
    const float la = std::pow (10.0f, a / 20.0f), lb = std::pow (10.0f, b / 20.0f);
    return 20.0f * std::log10 (std::max (1.0e-9f, 0.5f * (la + lb)));
}

// ---------------- engine ----------------

float Engine::SVF::softLimit (float v)
{
    const float a = std::abs (v);
    const float s = 6.0f + 2.0f * std::tanh ((a - 6.0f) * 0.5f);
    return v < 0.0f ? -s : s;
}

void Engine::FilterState::clear()
{
    s1.clear(); s2.clear(); f1.clear(); f2.clear(); f3.clear();
    combLp = 0.0f;
    std::fill (comb.begin(), comb.end(), 0.0f);
    combPos = 0;
}

void Engine::prepare (double sr, int /*maxBlock*/)
{
    sampleRate = sr > 1000.0 ? sr : 48000.0;
    combSize = (int) std::ceil (sampleRate / minCutoff) + 8;
    for (auto* set : { &fa, &fb })
        for (auto& f : *set)
            f.comb.assign ((size_t) combSize, 0.0f);
    ctlCoef = coefFor (0.02, sampleRate);
    lfoSmoothCoef = coefFor (0.0007, sampleRate);
    tFastA = coefFor (0.0003, sampleRate);
    tFastR = coefFor (0.020, sampleRate);
    tSlowA = coefFor (0.012, sampleRate);
    tSlowR = coefFor (0.060, sampleRate);
    reset();
}

void Engine::reset()
{
    for (auto* set : { &fa, &fb })
        for (auto& f : *set)
            f.clear();
    phase = 0.0;
    phaseOffset = 0.0;
    lastPhL = lastPhR = 2.0;
    activeA = params.a.type;
    activeB = params.b.type;
    typeGainA = typeGainB = 1.0f;
    lfoSm = { 0.0f, 0.0f };
    env = 0.0f;
    tFast = tSlow = 0.0f;
    holdoff = 0;
    autoOff = -1;
    stage = Stage::idle;
    adsr = 0.0f;
    heldNotes = 0;
    pendingTrigger = pendingRelease = false;
    first = true;
}

void Engine::noteOn()
{
    ++heldNotes;
    if (params.trigger == Trigger::midi) pendingTrigger = true;
}

void Engine::noteOff()
{
    heldNotes = std::max (0, heldNotes - 1);
    if (heldNotes == 0 && params.trigger == Trigger::midi) pendingRelease = true;
}

void Engine::allNotesOff()
{
    heldNotes = 0;
    if (params.trigger == Trigger::midi) pendingRelease = true;
}

void Engine::trigger()
{
    stage = Stage::attack;
    triggerCount.fetch_add (1, std::memory_order_relaxed);
    if (params.retrig)
    {
        phaseOffset = frac (phaseOffset - phase + 1.0);
        phase = 0.0;
        lastPhL = lastPhR = 2.0;
    }
}

void Engine::release()
{
    if (stage != Stage::idle) stage = Stage::release;
}

float Engine::runFilter (FilterState& st, const FilterParams&, FilterType type, float x, float cutoff, float reso, float drive)
{
    x = driveShape (x, drive);
    const float fc = clampf (cutoff, minCutoff, (float) std::min ((double) maxCutoff, sampleRate * 0.45));
    const float k = dampingFor (reso);
    float lp, bp, hp;
    switch (type)
    {
        case FilterType::lp12:
        case FilterType::hp12:
        case FilterType::bp:
        case FilterType::notch:
        {
            const float g = std::tan (3.14159265f * fc / (float) sampleRate);
            st.s1.tick (x, g, k, lp, bp, hp);
            if (type == FilterType::lp12) return lp;
            if (type == FilterType::hp12) return hp;
            if (type == FilterType::bp) return k * bp;
            return x - k * bp;
        }
        case FilterType::lp24:
        case FilterType::hp24:
        {
            const float g = std::tan (3.14159265f * fc / (float) sampleRate);
            st.s1.tick (x, g, 1.848f, lp, bp, hp);
            const float mid = type == FilterType::lp24 ? lp : hp;
            st.s2.tick (mid, g, std::min (k, 0.765f), lp, bp, hp);
            return type == FilterType::lp24 ? lp : hp;
        }
        case FilterType::comb:
        {
            const float fbk = 0.3f + 0.66f * clampf (reso, 0.0f, 1.0f);
            const float d = clampf ((float) sampleRate / fc, 2.0f, (float) (combSize - 3));
            float rp = (float) st.combPos - d;
            if (rp < 0.0f) rp += (float) combSize;
            int i0 = (int) rp;
            if (i0 >= combSize || i0 < 0) { i0 = 0; rp = 0.0f; }
            const int i1 = i0 + 1 >= combSize ? 0 : i0 + 1;
            const float t = rp - (float) i0;
            const float del = st.comb[(size_t) i0] + t * (st.comb[(size_t) i1] - st.comb[(size_t) i0]);
            st.combLp += 0.7f * (del - st.combLp);
            float y = x + fbk * st.combLp;
            if (y > 8.0f || y < -8.0f) y = SVF::softLimit (y * 0.75f);
            if (y > -1e-20f && y < 1e-20f) y = 0.0f;
            st.comb[(size_t) st.combPos] = y;
            st.combPos = st.combPos + 1 >= combSize ? 0 : st.combPos + 1;
            return y * std::sqrt (1.0f - fbk * fbk);
        }
        case FilterType::formant:
        default:
        {
            float ff[3], gg[3];
            formantsFor (fc, ff, gg);
            const float kk = 1.0f / formantQ (reso);
            float sum = 0.0f;
            SVF* svfs[3] = { &st.f1, &st.f2, &st.f3 };
            for (int i = 0; i < 3; ++i)
            {
                const float g = std::tan (3.14159265f * std::min (ff[i], (float) (sampleRate * 0.45)) / (float) sampleRate);
                svfs[i]->tick (x, g, kk, lp, bp, hp);
                sum += gg[i] * kk * bp;
            }
            return 2.0f * sum;
        }
    }
}

void Engine::process (float* const* ch, int numCh, int n, const HostTime& time)
{
    if (numCh <= 0 || n <= 0) return;
    numCh = std::min (numCh, 2);
    const Params& p = params;

    if (time.bpm > 1.0 && std::isfinite (time.bpm)) bpm = std::clamp (time.bpm, 20.0, 999.0);
    const double cyc = cycleSeconds (p, bpm);
    const double inc = 1.0 / (cyc * sampleRate);
    const bool hostLocked = p.sync && time.playing && time.hasPpq && std::isfinite (time.ppq);
    const double beatsPerCycle = cyc * bpm / 60.0;
    if (hostLocked)
        phase = frac (time.ppq / beatsPerCycle + phaseOffset);

    // targets
    auto lg = [] (float hz) { return std::log (clampf (sanitize (hz), minCutoff, maxCutoff)); };
    const float tCutA = lg (p.a.cutoff), tCutB = lg (p.b.cutoff);
    const float tResA = clampf (sanitize (p.a.reso), 0.0f, 1.0f), tResB = clampf (sanitize (p.b.reso), 0.0f, 1.0f);
    const float tDrvA = clampf (sanitize (p.a.drive), 0.0f, 1.0f), tDrvB = clampf (sanitize (p.b.drive), 0.0f, 1.0f);
    const float tModA = clampf (sanitize (p.a.mod), -1.0f, 1.0f), tModB = clampf (sanitize (p.b.mod), -1.0f, 1.0f);
    const float tLfoD = clampf (sanitize (p.lfoDepth), -1.0f, 1.0f);
    const float tEnvD = clampf (sanitize (p.envDepth), -1.0f, 1.0f);
    const float tAdsrD = clampf (sanitize (p.adsrDepth), -1.0f, 1.0f);
    const float tMix = clampf (sanitize (p.mix), 0.0f, 1.0f);
    const float tOut = std::pow (10.0f, clampf (sanitize (p.outputDb), -60.0f, 24.0f) / 20.0f);
    const float tBMix = p.b.on ? 1.0f : 0.0f;
    const float tPar = p.routing == Routing::parallel ? 1.0f : 0.0f;
    const float stereoOff = clampf (sanitize (p.stereo), 0.0f, 1.0f) * 0.5f;
    const float typeStep = 1.0f / (float) std::max (1.0, 0.005 * sampleRate);   // 5 ms duck on a type change

    if (first)
    {
        smCutA = tCutA; smCutB = tCutB; smResA = tResA; smResB = tResB; smDrvA = tDrvA; smDrvB = tDrvB;
        smModA = tModA; smModB = tModB; smLfoD = tLfoD; smEnvD = tEnvD; smAdsrD = tAdsrD; smMix = tMix; smOut = tOut;
        smBMix = tBMix; smPar = tPar;
        first = false;
    }

    // envelope follower / ADSR coefficients
    const float es = clampf (sanitize (p.envSpeed), 0.0f, 1.0f);
    const float envA = coefFor (0.030 - 0.028 * es, sampleRate);
    const float envR = coefFor (0.600 - 0.560 * es, sampleRate);
    const float attackSamples = (float) (std::clamp ((double) sanitize (p.attackMs), 0.1, 10000.0) * 0.001 * sampleRate);
    const float decC = coefFor (std::clamp ((double) sanitize (p.decayMs), 1.0, 10000.0) * 0.001 / 3.0, sampleRate);
    const float relC = coefFor (std::clamp ((double) sanitize (p.releaseMs), 1.0, 10000.0) * 0.001 / 3.0, sampleRate);
    const float sus = clampf (sanitize (p.sustain), 0.0f, 1.0f);
    const float thrDb = 14.0f - 11.0f * clampf (sanitize (p.sensitivity), 0.0f, 1.0f);
    const int oneShotLen = (int) ((std::clamp ((double) sanitize (p.attackMs), 0.1, 10000.0) + std::clamp ((double) sanitize (p.decayMs), 1.0, 10000.0)) * 0.001 * sampleRate);

    // LFO smoothing (anti-click always, glide for stepped shapes)
    const bool stepped = p.shape == Shape::pattern || p.shape == Shape::sampleHold || p.shape == Shape::square;
    const double stepSec = p.shape == Shape::pattern ? cyc / numSteps : cyc;
    const float lfoC = (stepped && p.glide > 0.0f) ? coefFor (0.0007 + clampf (p.glide, 0.0f, 1.0f) * stepSec * 0.5, sampleRate) : lfoSmoothCoef;

    const float nyq = (float) std::min ((double) maxCutoff, sampleRate * 0.45);
    float lastA = 0.0f, lastB = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        smCutA += ctlCoef * (tCutA - smCutA);
        smCutB += ctlCoef * (tCutB - smCutB);
        smResA += ctlCoef * (tResA - smResA);
        smResB += ctlCoef * (tResB - smResB);
        smDrvA += ctlCoef * (tDrvA - smDrvA);
        smDrvB += ctlCoef * (tDrvB - smDrvB);
        smModA += ctlCoef * (tModA - smModA);
        smModB += ctlCoef * (tModB - smModB);
        smLfoD += ctlCoef * (tLfoD - smLfoD);
        smEnvD += ctlCoef * (tEnvD - smEnvD);
        smAdsrD += ctlCoef * (tAdsrD - smAdsrD);
        smOut += ctlCoef * (tOut - smOut);
        smBMix += ctlCoef * (tBMix - smBMix);
        smPar += ctlCoef * (tPar - smPar);
        if (std::abs (smBMix - tBMix) < 1.0e-5f) smBMix = tBMix;
        if (std::abs (smPar - tPar) < 1.0e-5f) smPar = tPar;
        // mix lands exactly (0 = bit-exact dry)
        smMix += ctlCoef * (tMix - smMix);
        if (std::abs (smMix - tMix) < 1.0e-5f) smMix = tMix;

        float x[2];
        for (int c = 0; c < numCh; ++c) x[c] = sanitize (ch[c][i]);
        if (numCh == 1) x[1] = x[0];
        const float det = std::max (std::abs (x[0]), std::abs (x[1]));

        // ---- MIDI / transient triggers
        if (pendingTrigger) { pendingTrigger = false; trigger(); }
        if (pendingRelease) { pendingRelease = false; release(); }
        tFast += (det > tFast ? tFastA : tFastR) * (det - tFast);
        tSlow += (det > tSlow ? tSlowA : tSlowR) * (det - tSlow);
        if (holdoff > 0) --holdoff;
        if (p.trigger == Trigger::transient)
        {
            const float fastDb = dbPerLn * std::log (tFast + 1.0e-9f);
            const float diff = fastDb - dbPerLn * std::log (tSlow + 1.0e-9f);
            if (holdoff == 0 && fastDb > -50.0f && diff > thrDb)
            {
                trigger();
                holdoff = (int) (0.06 * sampleRate);
                autoOff = oneShotLen;
            }
            if (autoOff > 0 && --autoOff == 0) { release(); autoOff = -1; }
        }

        // ---- ADSR
        switch (stage)
        {
            case Stage::idle: adsr = 0.0f; break;
            case Stage::attack:
                adsr += 1.0f / std::max (1.0f, attackSamples);
                if (adsr >= 1.0f) { adsr = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                adsr += decC * (sus - adsr);
                if (std::abs (adsr - sus) < 1.0e-4f) stage = Stage::sustain;
                break;
            case Stage::sustain: adsr += decC * (sus - adsr); break;
            case Stage::release:
                adsr += relC * (0.0f - adsr);
                if (adsr < 1.0e-5f) { adsr = 0.0f; stage = Stage::idle; }
                break;
        }

        // ---- envelope follower
        env += (det > env ? envA : envR) * (det - env);
        if (env < 1.0e-12f) env = 0.0f;
        const float envNorm = env > 1.0e-6f ? clampf ((dbPerLn * std::log (env) + 54.0f) / 48.0f, 0.0f, 1.0f) : 0.0f;

        // ---- LFO
        const double phL = phase, phR = frac (phase + stereoOff);
        if (p.shape == Shape::sampleHold)
        {
            if (phL < lastPhL) { rngState = rngState * 1664525u + 1013904223u; shValue = rngState; }
            if (phR < lastPhR) { rngState = rngState * 1664525u + 1013904223u; shValueR = rngState; }
            if (stereoOff <= 0.0f) shValueR = shValue;
        }
        lastPhL = phL;
        lastPhR = phR;
        const float rawL = lfoShapeValue (p.shape, phL, p.pattern, shValue);
        const float rawR = stereoOff > 0.0f ? lfoShapeValue (p.shape, phR, p.pattern, shValueR) : rawL;
        lfoSm[0] += lfoC * (rawL - lfoSm[0]);
        lfoSm[1] += lfoC * (rawR - lfoSm[1]);
        phase += inc;
        if (phase >= 1.0) phase -= std::floor (phase);

        // ---- filter type changes: duck to dry over 5 ms, switch (fresh state), fade back in
        if (p.a.type != activeA)
        {
            typeGainA -= typeStep;
            if (typeGainA <= 0.0f) { typeGainA = 0.0f; activeA = p.a.type; for (auto& f : fa) f.clear(); }
        }
        else if (typeGainA < 1.0f) typeGainA = std::min (1.0f, typeGainA + typeStep);
        if (p.b.type != activeB)
        {
            typeGainB -= typeStep;
            if (typeGainB <= 0.0f) { typeGainB = 0.0f; activeB = p.b.type; for (auto& f : fb) f.clear(); }
        }
        else if (typeGainB < 1.0f) typeGainB = std::min (1.0f, typeGainB + typeStep);

        // ---- per channel filtering
        for (int c = 0; c < numCh; ++c)
        {
            const float m = (lfoSm[(size_t) c] * smLfoD + envNorm * smEnvD + adsr * smAdsrD) * maxModOctaves * ln2;
            const float cA = clampf (std::exp (smCutA + m * smModA), minCutoff, nyq);
            float y = runFilter (fa[(size_t) c], p.a, activeA, x[c], cA, smResA, smDrvA);
            if (typeGainA < 1.0f) y = x[c] + typeGainA * (y - x[c]);
            if (c == 0) lastA = cA;
            if (smBMix > 0.0f)
            {
                const float cB = clampf (std::exp (smCutB + m * smModB), minCutoff, nyq);
                if (c == 0) lastB = cB;
                const float inB = y + smPar * (x[c] - y);
                float yB = runFilter (fb[(size_t) c], p.b, activeB, inB, cB, smResB, smDrvB);
                if (typeGainB < 1.0f) yB = inB + typeGainB * (yB - inB);
                const float routed = yB + smPar * (0.5f * (y + yB) - yB);
                y += smBMix * (routed - y);
            }
            y = safety (y);
            float o = smMix == 1.0f ? y : (smMix == 0.0f ? x[c] : x[c] + smMix * (y - x[c]));
            if (smOut != 1.0f) o *= smOut;
            ch[c][i] = o;
        }
    }

    if (lastB == 0.0f) lastB = clampf (std::exp (smCutB), minCutoff, nyq);
    cutoffA.store (lastA, std::memory_order_relaxed);
    cutoffB.store (lastB, std::memory_order_relaxed);
    lfoOut.store (lfoSm[0], std::memory_order_relaxed);
    envOut.store (env > 1.0e-6f ? clampf ((dbPerLn * std::log (env) + 54.0f) / 48.0f, 0.0f, 1.0f) : 0.0f, std::memory_order_relaxed);
    adsrOut.store (adsr, std::memory_order_relaxed);
    lfoPhase.store ((float) phase, std::memory_order_relaxed);
}

} // namespace sweepfx
