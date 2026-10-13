#include "DustEngine.h"

#include <algorithm>
#include <cmath>

namespace dust
{

namespace
{
constexpr double pi = 3.14159265358979323846;

inline float clamp01 (float v) { return std::min (1.0f, std::max (0.0f, v)); }
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// Hermite (4-point, 3rd order) interpolation
inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float bNeg = w + a;
    return ((((a * t) - bNeg) * t + c) * t + x0);
}

constexpr float baseDelayMs = 6.0f;
constexpr float maxWowMs = 2.4f;
constexpr float maxFlutterMs = 0.22f;
constexpr int filterUpdateInterval = 32;
} // namespace

// ---------------- OnePole ----------------

void OnePole::setTime (float ms, double sr)
{
    const double samples = std::max (1.0, ms * 0.001 * sr);
    coeff = (float) (1.0 - std::exp (-1.0 / samples));
}

// ---------------- Biquad (RBJ cookbook) ----------------

void Biquad::set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
{
    b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0;
    a1 = na1 / na0; a2 = na2 / na0;
}

void Biquad::setLowpass (double sr, double freq, double q)
{
    const double w = 2.0 * pi * freq / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}

void Biquad::setHighpass (double sr, double freq, double q)
{
    const double w = 2.0 * pi * freq / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}

void Biquad::setLowShelf (double sr, double freq, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * pi * freq / sr, c = std::cos (w), s = std::sin (w);
    const double al = s / 2.0 * std::sqrt (2.0); // S = 1
    const double sq = 2.0 * std::sqrt (A) * al;
    set (A * ((A + 1) - (A - 1) * c + sq),
         2 * A * ((A - 1) - (A + 1) * c),
         A * ((A + 1) - (A - 1) * c - sq),
         (A + 1) + (A - 1) * c + sq,
         -2 * ((A - 1) + (A + 1) * c),
         (A + 1) + (A - 1) * c - sq);
}

void Biquad::setPeak (double sr, double freq, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * pi * freq / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
    set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}

// ---------------- Oversampler2x ----------------

Oversampler2x::Oversampler2x()
{
    // Windowed-sinc half-band lowpass, cutoff = quarter of the oversampled rate.
    const int M = taps - 1;
    double sum = 0.0;
    for (int n = 0; n < taps; ++n)
    {
        const double k = n - M / 2.0;
        const double sinc = (k == 0.0) ? 0.5 : std::sin (pi * 0.5 * k) / (pi * k);
        const double win = 0.42 - 0.5 * std::cos (2 * pi * n / M) + 0.08 * std::cos (4 * pi * n / M); // Blackman
        h[(size_t) n] = (float) (sinc * win);
        sum += h[(size_t) n];
    }
    for (auto& v : h)
        v = (float) (v / sum);
}

void Oversampler2x::reset()
{
    upHist.fill (0.0f);
    downHist.fill (0.0f);
    upPos = downPos = 0;
}

static inline float firAt (const std::array<float, 31>& h, const std::array<float, 31>& hist, int pos)
{
    float acc = 0.0f;
    int idx = pos;
    for (int n = 0; n < 31; ++n)
    {
        acc += h[(size_t) n] * hist[(size_t) idx];
        idx = (idx == 0) ? 30 : idx - 1;
    }
    return acc;
}

void Oversampler2x::up (float in, float& a, float& b)
{
    upPos = (upPos + 1) % taps;
    upHist[(size_t) upPos] = in * 2.0f; // zero-stuffing gain compensation
    a = firAt (h, upHist, upPos);
    upPos = (upPos + 1) % taps;
    upHist[(size_t) upPos] = 0.0f;
    b = firAt (h, upHist, upPos);
}

float Oversampler2x::down (float a, float b)
{
    downPos = (downPos + 1) % taps;
    downHist[(size_t) downPos] = a;
    downPos = (downPos + 1) % taps;
    downHist[(size_t) downPos] = b;
    return firAt (h, downHist, downPos);
}

// ---------------- Engine ----------------

void Engine::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;

    for (auto* s : { &sDrive, &sWobble, &sNoise, &sMix, &sOut, &sWidth, &sCrush, &sCutLog, &sBump, &sSat })
        s->setTime (25.0f, sr);

    // whole number of samples so the dry path can be aligned exactly
    baseDelaySamples = (float) std::lround (baseDelayMs * 0.001 * sr);
    delaySize = (int) std::ceil ((baseDelayMs + maxWowMs * 1.6f + maxFlutterMs + 2.0f) * 0.001 * sr) + 8;

    // Wet path latency: wobble centre delay + oversampler filters (15 + 15 samples at 2x = 15 at base rate).
    latency = (int) std::lround (baseDelaySamples) + 15;

    for (auto& c : ch)
    {
        c.delay.assign ((size_t) delaySize, 0.0f);
        c.dryDelay.assign ((size_t) latency + 1, 0.0f);
    }

    envAttack = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));
    envRelease = (float) (1.0 - std::exp (-1.0 / (0.400 * sr)));

    reset();
}

void Engine::reset()
{
    for (auto& c : ch)
    {
        c.os.reset();
        for (auto* b : { &c.preEmph, &c.deEmph, &c.bump, &c.lowpass, &c.lowpass2, &c.dcBlock, &c.hissBand, &c.crackleTone })
            b->reset();
        std::fill (c.delay.begin(), c.delay.end(), 0.0f);
        std::fill (c.dryDelay.begin(), c.dryDelay.end(), 0.0f);
        c.holdValue = 0.0f;
        c.crackleEnv = 0.0f;
        c.crackleSign = 1.0f;
        c.dcBlock.setHighpass (sr, 12.0, 0.707);
        c.hissBand.setHighpass (sr, 1800.0, 0.6);
        c.crackleTone.setLowpass (sr, std::min (7000.0, sr * 0.45), 0.8);
    }
    writePos = 0;
    dryPos = 0;
    wowPhase = flutterPhase = 0.0;
    driftValue = driftTarget = 0.0;
    driftCounter = 0;
    holdPhase = 0.0f;
    envFollower = 0.0f;
    rng = Rng (0xC0FFEEu);
    firstBlock = true;
    lastCutoff = -1.0f;
    lastBump = -100.0f;
    filterCountdown = 0;
}

Engine::Effective Engine::computeEffective (const Params& p) const
{
    Effective e {};
    const float age = clamp01 (p.age);
    e.tape   = clamp01 (p.tape + 0.6f * age);
    e.wobble = clamp01 (p.wobble + 0.5f * age);
    e.vinyl  = clamp01 (p.vinyl + 0.5f * age);
    e.crush  = clamp01 (p.crush + 0.2f * age);

    // TONE: 0 -> 2.2 kHz, 0.5 -> 11 kHz, 1 -> 20 kHz (log), then darkened by AGE and TAPE.
    const float t = clamp01 (p.tone);
    float logCut = (t < 0.5f) ? std::log (2200.0f) + (std::log (11000.0f) - std::log (2200.0f)) * (t / 0.5f)
                              : std::log (11000.0f) + (std::log (20000.0f) - std::log (11000.0f)) * ((t - 0.5f) / 0.5f);
    logCut += std::log (1.0f - 0.55f * age) + std::log (1.0f - 0.2f * e.tape);
    const float nyqSafe = (float) (sr * 0.45);
    e.cutoffHz = std::min (nyqSafe, std::max (1200.0f, std::exp (logCut)));

    e.satAmount = std::min (1.0f, e.tape * 4.0f);
    e.headBumpDb = 3.5f * e.tape;
    e.drive = dbToGain (18.0f * std::pow (e.tape, 1.15f));
    e.width = clamp01 (p.width) * 2.0f;
    e.mix = clamp01 (p.mix);
    e.outGain = dbToGain (std::min (12.0f, std::max (-24.0f, p.outputDb)));
    e.noiseLevel = (e.vinyl > 0.001f) ? dbToGain (-70.0f + 40.0f * e.vinyl) : 0.0f;
    return e;
}

void Engine::updateFilters (const Effective& e)
{
    (void) e;
    const float cutoff = std::exp (sCutLog.current());
    const float bump = sBump.current();
    if (std::abs (cutoff - lastCutoff) > 0.5f || std::abs (bump - lastBump) > 0.01f)
    {
        for (auto& c : ch)
        {
            c.lowpass.setLowpass (sr, cutoff, 0.54);
            c.lowpass2.setLowpass (sr, cutoff, 1.31);
            c.bump.setLowShelf (sr, 100.0, bump);
        }
        lastCutoff = cutoff;
        lastBump = bump;
    }
}

float Engine::readDelay (int c, float delaySamples) const
{
    const auto& buf = ch[(size_t) c].delay;
    float readPos = (float) writePos - delaySamples;
    while (readPos < 0.0f)
        readPos += (float) delaySize;
    const int i0 = (int) readPos;
    const float frac = readPos - (float) i0;
    auto at = [&] (int i) { return buf[(size_t) ((i % delaySize + delaySize) % delaySize)]; };
    return hermite (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), frac);
}

void Engine::process (float* const* channels, int numChannels, int numSamples)
{
    numChannels = std::min (numChannels, maxChannels);
    if (numChannels <= 0 || numSamples <= 0)
        return;

    const Effective e = computeEffective (target);

    if (firstBlock)
    {
        sDrive.reset (e.drive); sWobble.reset (e.wobble); sNoise.reset (e.noiseLevel);
        sMix.reset (e.mix); sOut.reset (e.outGain); sWidth.reset (e.width); sCrush.reset (e.crush);
        sCutLog.reset (std::log (e.cutoffHz)); sBump.reset (e.headBumpDb); sSat.reset (e.satAmount);
        updateFilters (e);
        firstBlock = false;
    }

    const double wowInc = 2.0 * pi * 0.55 / sr;
    const double flutterInc = 2.0 * pi * 7.3 / sr;
    const int driftPeriod = std::max (1, (int) (sr * 0.35));
    const float bias = 0.12f;
    const float tanhBias = std::tanh (bias);

    for (int n = 0; n < numSamples; ++n)
    {
        // ---- smoothed controls ----
        const float drive = sDrive.process (e.drive);
        const float wob = sWobble.process (e.wobble);
        const float noiseLvl = sNoise.process (e.noiseLevel);
        const float mix = sMix.process (e.mix);
        const float out = sOut.process (e.outGain);
        const float width = sWidth.process (e.width);
        const float crush = sCrush.process (e.crush);
        const float satAmt = sSat.process (e.satAmount);
        sCutLog.process (std::log (e.cutoffHz));
        sBump.process (e.headBumpDb);

        if (--filterCountdown <= 0)
        {
            updateFilters (e);
            filterCountdown = filterUpdateInterval;
        }

        // ---- modulation (shared by both channels, flutter slightly decorrelated) ----
        if (--driftCounter <= 0)
        {
            driftTarget = rng.bipolar();
            driftCounter = driftPeriod;
        }
        driftValue += (driftTarget - driftValue) * (1.0 / (double) driftPeriod);
        wowPhase += wowInc;
        flutterPhase += flutterInc;
        if (wowPhase > 2.0 * pi) wowPhase -= 2.0 * pi;
        if (flutterPhase > 2.0 * pi) flutterPhase -= 2.0 * pi;

        const float wowDepth = maxWowMs * std::pow (wob, 1.3f) * 0.001f * (float) sr;
        const float flutDepth = maxFlutterMs * wob * 0.001f * (float) sr;
        const float wowMod = wowDepth * (float) (std::sin (wowPhase) + 0.5 * driftValue);

        // ---- input envelope (for noise follow) ----
        float peak = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            peak = std::max (peak, std::abs (channels[c][n]));
        const float coef = peak > envFollower ? envAttack : envRelease;
        envFollower += coef * (peak - envFollower);
        const float follow = target.noiseFollow ? std::min (1.0f, envFollower * 20.0f) : 1.0f;

        // ---- crackle event (shared) ----
        const float crackleRate = 1.0f + 28.0f * e.vinyl * e.vinyl; // events per second
        bool newCrackle = noiseLvl > 0.0f && rng.uniform() < crackleRate / (float) sr;
        float crackleAmp = 0.0f, cracklePan = 0.0f;
        if (newCrackle)
        {
            const float u = rng.uniform();
            crackleAmp = 0.25f + 0.75f * u * u * u;
            cracklePan = rng.bipolar() * 0.6f;
        }
        const float crackleDecay = (float) std::exp (-1.0 / (0.00025 * sr));

        // ---- sample & hold clock ----
        const float holdRate = (crush > 0.001f) ? (float) std::exp (std::log (sr) + (std::log (3500.0) - std::log (sr)) * crush) : (float) sr;
        holdPhase += holdRate / (float) sr;
        const bool takeSample = holdPhase >= 1.0f;
        if (takeSample)
            holdPhase -= std::floor (holdPhase);
        const float bits = 16.0f - 11.0f * crush;
        const float q = std::pow (2.0f, bits - 1.0f);

        float wet[maxChannels] {};
        float dry[maxChannels] {};

        for (int c = 0; c < numChannels; ++c)
        {
            auto& st = ch[(size_t) c];
            const float x = channels[c][n];

            // dry latency match
            st.dryDelay[(size_t) dryPos] = x;
            const int readDry = (dryPos + 1) % (latency + 1);
            dry[c] = st.dryDelay[(size_t) readDry];

            // wobble
            st.delay[(size_t) writePos] = x;
            const float flutterOff = (c == 1) ? 0.35f : 0.0f;
            const float d = baseDelaySamples + wowMod + flutDepth * (float) std::sin (flutterPhase + flutterOff);
            float y = readDelay (c, std::max (2.0f, d));

            // tape saturation at 2x
            float a, b;
            st.os.up (y, a, b);
            const float makeup = 1.0f / (std::pow (drive, 0.85f) * (1.0f - tanhBias * tanhBias));
            a += ((std::tanh (a * drive + bias) - tanhBias) * makeup - a) * satAmt;
            b += ((std::tanh (b * drive + bias) - tanhBias) * makeup - b) * satAmt;
            y = st.os.down (a, b);

            y = st.dcBlock.process (y);
            y = st.bump.process (y);
            y = st.lowpass.process (y);
            y = st.lowpass2.process (y);

            // crush
            if (crush > 0.001f)
            {
                if (takeSample)
                    st.holdValue = y;
                float crushed = std::round (st.holdValue * q) / q;
                y = y + (crushed - y) * std::min (1.0f, crush * 4.0f);
            }

            // vinyl noise
            if (noiseLvl > 0.0f)
            {
                float hiss = st.hissBand.process (rng.bipolar()) * 0.5f;
                if (newCrackle)
                {
                    st.crackleEnv = crackleAmp * (c == 0 ? (1.0f - cracklePan) : (1.0f + cracklePan));
                    st.crackleSign = rng.uniform() < 0.5f ? -1.0f : 1.0f;
                }
                const float crackle = st.crackleTone.process (st.crackleEnv * st.crackleSign * (0.6f + 0.4f * rng.uniform()));
                st.crackleEnv *= crackleDecay;
                y += (hiss + crackle * 6.0f) * noiseLvl * follow;
            }

            wet[c] = y;
        }

        // width (mid/side) on wet signal
        if (numChannels == 2)
        {
            const float m = 0.5f * (wet[0] + wet[1]);
            const float s = 0.5f * (wet[0] - wet[1]) * width;
            wet[0] = m + s;
            wet[1] = m - s;
        }

        for (int c = 0; c < numChannels; ++c)
        {
            float o = (dry[c] * (1.0f - mix) + wet[c] * mix) * out;
            if (! std::isfinite (o))
                o = 0.0f;
            channels[c][n] = o;
        }

        writePos = (writePos + 1) % delaySize;
        dryPos = (dryPos + 1) % (latency + 1);
    }
}

} // namespace dust
