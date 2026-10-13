#include "VoxEngine.h"

#include <algorithm>
#include <cmath>

namespace vox
{

namespace
{
constexpr double pi = 3.14159265358979323846;

const std::array<std::vector<int>, numScales>& scaleSteps()
{
    static const std::array<std::vector<int>, numScales> s { {
        { 0, 2, 4, 5, 7, 9, 11 },  // major
        { 0, 2, 3, 5, 7, 8, 10 },  // natural minor
        { 0, 2, 3, 5, 7, 8, 11 },  // harmonic minor
        { 0, 3, 5, 7, 10 },        // minor pentatonic
        { 0, 2, 3, 5, 7, 9, 10 },  // dorian
        { 0, 1, 3, 5, 7, 8, 10 },  // phrygian
    } };
    return s;
}
} // namespace

float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (std::max (1.0f, hz) / 440.0f); }
float midiToHz (float midi) { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); }

float snapToScale (float midi, int key, int scale, bool chromatic)
{
    if (chromatic)
        return std::round (midi);
    const auto& steps = scaleSteps()[(size_t) std::max (0, std::min (numScales - 1, scale))];
    const int base = (int) std::floor (midi);
    float best = (float) base, bestDist = 1e9f;
    for (int n = base - 3; n <= base + 3; ++n)
    {
        const int pc = ((n - key) % 12 + 12) % 12;
        if (std::find (steps.begin(), steps.end(), pc) == steps.end())
            continue;
        const float dist = std::abs ((float) n - midi);
        if (dist < bestDist) { bestDist = dist; best = (float) n; }
    }
    return best;
}

// ---------------- YIN ----------------

void YinTracker::prepare (double analysisRate)
{
    rate = analysisRate;
    tauMin = std::max (2, (int) (rate / 1000.0));   // up to 1 kHz
    tauMax = (int) (rate / 65.0);                   // down to 65 Hz
    winSize = tauMax;
    diff.assign ((size_t) tauMax + 2, 0.0f);
}

float YinTracker::analyse (const float* x, int size, float& confidence)
{
    confidence = 0.0f;
    if (size < winSize + tauMax)
        return 0.0f;

    // difference function
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double acc = 0.0;
        for (int j = 0; j < winSize; ++j)
        {
            const double d = (double) x[j] - (double) x[j + tau];
            acc += d * d;
        }
        diff[(size_t) tau] = (float) acc;
    }
    // cumulative mean normalised difference
    diff[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += diff[(size_t) tau];
        diff[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
    }
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (diff[(size_t) tau] < 0.12f)
        {
            while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        // no clear dip: accept the global minimum only if it is reasonably deep
        float mn = 1e9f;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (diff[(size_t) tau] < mn) { mn = diff[(size_t) tau]; best = tau; }
        if (mn > 0.25f)
            return 0.0f;
    }
    // parabolic interpolation
    float t = (float) best;
    if (best > 1 && best < tauMax - 1)
    {
        const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const float den = a - 2.0f * b + c;
        if (std::abs (den) > 1e-9f)
            t += 0.5f * (a - c) / den;
    }
    confidence = 1.0f - std::min (1.0f, diff[(size_t) best]);
    return (float) (rate / t);
}

// ---------------- Engine ----------------

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    decim = sr >= 88200.0 ? 4 : 2;
    hop = std::max (64, (int) (sr * 0.005));        // analysis every ~5 ms
    yin.prepare (sr / decim);

    minPeriod = (int) (sr / 1000.0);
    maxPeriod = (int) (sr / 65.0);
    // a grain reaches 2 periods / formant factor (min 0.5) around its mark, and needs one period of input ahead
    latency = 4 * maxPeriod + 64;

    int need = 1;
    while (need < latency * 2 + 8 * maxPeriod + 4096) need <<= 1;
    ringSize = need;
    for (auto& b : in) b.assign ((size_t) ringSize, 0.0f);
    for (auto& b : outAcc) b.assign ((size_t) ringSize, 0.0f);
    winAcc.assign ((size_t) ringSize, 0.0f);
    decimBuf.assign ((size_t) (yin.windowSize() * 2 + 8) * 2, 0.0f);
    reset();
}

void Engine::reset()
{
    for (auto& b : in) std::fill (b.begin(), b.end(), 0.0f);
    for (auto& b : outAcc) std::fill (b.begin(), b.end(), 0.0f);
    std::fill (winAcc.begin(), winAcc.end(), 0.0f);
    std::fill (decimBuf.begin(), decimBuf.end(), 0.0f);
    writePos = 0;
    decimWrite = 0;
    decimLp = decimLp2 = 0.0f;
    hopCounter = 0;
    currentPeriod = (float) (sr * 0.006);
    currentRatio = 1.0f;
    voiced = false;
    lastF0 = 0.0f;
    analysisMarks.clear();
    nextAnalysis = 0;
    nextSynth = 0;
    smoothedTargetMidi = 0.0f; // used as the smoothed correction (semitones)
    mixSm = std::max (0.0f, std::min (1.0f, params.mix));
    outSm = std::pow (10.0f, params.outputDb / 20.0f);
    uiDetected = uiTarget = 0.0f;
}

float Engine::pitchRatioFor (float f0)
{
    const float base = std::pow (2.0f, std::max (-24.0f, std::min (24.0f, params.pitchSemis)) / 12.0f);
    float correction = 0.0f;
    if (f0 > 0.0f)
    {
        const float det = hzToMidi (f0);
        float target = det;
        if (params.midiControl && midiNote >= 0)
        {
            // keep the singer's octave, take the note from MIDI
            float n = (float) midiNote;
            while (n - det > 6.0f) n -= 12.0f;
            while (det - n > 6.0f) n += 12.0f;
            target = n;
        }
        else if (params.tune != TuneMode::off)
            target = snapToScale (det, params.key, params.scale, params.tune == TuneMode::chromatic);
        correction = target - det;
        uiDetected = f0;
        uiTarget = midiToHz (target);
    }
    else
        uiDetected = uiTarget = 0.0f;

    // retune speed: how fast the correction follows (0 ms = instant, robotic)
    const float hopMs = (float) (1000.0 * hop / sr);
    const float coef = params.retuneMs <= 0.5f ? 1.0f : 1.0f - std::exp (-hopMs / params.retuneMs);
    if (f0 > 0.0f)
        smoothedTargetMidi += coef * (correction - smoothedTargetMidi);
    return base * std::pow (2.0f, smoothedTargetMidi / 12.0f);
}

void Engine::analysisStep()
{
    const int need = yin.windowSize() * 2 + 2;
    const int size = (int) decimBuf.size();
    if (decimWrite < need)
        return;
    std::vector<float> win ((size_t) need);
    double energy = 0.0;
    for (int k = 0; k < need; ++k)
    {
        const float v = decimBuf[(size_t) ((decimWrite - need + k) % size)];
        win[(size_t) k] = v;
        energy += (double) v * v;
    }
    const float rms = (float) std::sqrt (energy / need);
    float conf = 0.0f;
    float f0 = rms > 0.003f ? yin.analyse (win.data(), need, conf) : 0.0f;
    if (f0 < 65.0f || f0 > 1000.0f)
        f0 = 0.0f;
    voiced = f0 > 0.0f;
    currentPeriod = voiced ? (float) (sr / f0) : (float) (sr * 0.006);
    currentPeriod = std::max ((float) minPeriod, std::min ((float) maxPeriod, currentPeriod));
    lastF0 = f0;
    const float ratio = pitchRatioFor (f0);
    currentRatio = ratio; // used by the marks created from now on

    // The analysis window is centred about half a window in the past. Analysis marks are laid down only up
    // to that centre, one *measured* period apart, so grains are cut exactly one real period apart even
    // during fast vibrato (the synthesis runs far enough behind to wait for them).
    const long long centre = writePos - (long long) (need / 2) * decim;
    while (nextAnalysis <= centre)
    {
        analysisMarks.push_back ({ nextAnalysis, currentPeriod, voiced, voiced ? ratio : 1.0f });
        nextAnalysis += std::max (1L, std::lround (currentPeriod));
    }
}

void Engine::addGrain (long long s)
{
    if (analysisMarks.empty())
        return;
    // nearest analysis mark
    const Mark* m = &analysisMarks.front();
    long long bestD = std::llabs (m->pos - s);
    for (const auto& a : analysisMarks)
    {
        const long long d = std::llabs (a.pos - s);
        if (d < bestD) { bestD = d; m = &a; }
    }
    const float phi = std::pow (2.0f, std::max (-12.0f, std::min (12.0f, params.formantSemis)) / 12.0f);
    const float T = m->period;
    const int hOut = std::max (2, (int) std::lround (T / phi));
    const int mask = ringSize - 1;
    for (int j = -hOut; j <= hOut; ++j)
    {
        const float w = 0.5f * (1.0f + (float) std::cos (pi * (double) j / (double) hOut));
        const double xpos = (double) m->pos + (double) j * phi;
        const long long i0 = (long long) std::floor (xpos);
        const float fr = (float) (xpos - (double) i0);
        const int o = (int) ((s + j) & mask);
        for (int c = 0; c < numCh; ++c)
        {
            const auto& b = in[(size_t) c];
            const float a0 = b[(size_t) (i0 & mask)], a1 = b[(size_t) ((i0 + 1) & mask)];
            outAcc[(size_t) c][(size_t) o] += w * (a0 + (a1 - a0) * fr);
        }
        winAcc[(size_t) o] += w;
    }
}

void Engine::process (float* const* channels, int numChannels, int numSamples)
{
    numCh = std::max (1, std::min (numChannels, maxChannels));
    const int mask = ringSize - 1;
    const float targetMix = std::max (0.0f, std::min (1.0f, params.mix));
    const float targetOut = std::pow (10.0f, std::max (-24.0f, std::min (12.0f, params.outputDb)) / 20.0f);
    const float pc = (float) (1.0 - std::exp (-1.0 / (0.02 * sr)));
    const int dsize = (int) decimBuf.size();
    // decimation lowpass (2 one-poles) at ~0.35 of the analysis Nyquist
    const float lpc = (float) (1.0 - std::exp (-2.0 * pi * (0.35 * sr / decim / 2.0) / sr));

    for (int n = 0; n < numSamples; ++n)
    {
        const long long w = writePos;
        float m = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            const float x = channels[c][n];
            in[(size_t) c][(size_t) (w & mask)] = x;
            m += x;
        }
        m /= (float) numCh;

        // ---- decimate for the pitch tracker
        decimLp += lpc * (m - decimLp);
        decimLp2 += lpc * (decimLp - decimLp2);
        if (w % decim == 0)
            decimBuf[(size_t) (decimWrite++ % dsize)] = decimLp2;
        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            analysisStep();
        }

        // ---- synthesis marks: add every grain that can reach the sample we output now
        const long long o = w - latency;
        while (nextSynth <= o + 2 * maxPeriod + 1)
        {
            // period and ratio of the nearest analysis mark
            float T = currentPeriod, ratio = 1.0f;
            if (! analysisMarks.empty())
            {
                const Mark* best = &analysisMarks.front();
                for (const auto& a : analysisMarks)
                    if (std::llabs (a.pos - nextSynth) < std::llabs (best->pos - nextSynth)) best = &a;
                T = best->period;
                ratio = best->voiced ? best->ratio : 1.0f;
            }
            if (nextSynth >= 0)
                addGrain (nextSynth);
            nextSynth += std::max ((long long) std::max (2, minPeriod / 2), (long long) std::lround (T / std::max (0.1f, ratio)));
        }
        while (! analysisMarks.empty() && analysisMarks.front().pos < nextSynth - 3LL * maxPeriod)
            analysisMarks.pop_front();

        // ---- output
        mixSm += pc * (targetMix - mixSm);
        outSm += pc * (targetOut - outSm);
        for (int c = 0; c < numCh; ++c)
        {
            float y = 0.0f, dry = 0.0f;
            if (o >= 0)
            {
                const int idx = (int) (o & mask);
                const float ws = winAcc[(size_t) idx];
                y = outAcc[(size_t) c][(size_t) idx] / std::max (ws, 0.35f);
                outAcc[(size_t) c][(size_t) idx] = 0.0f;
                dry = in[(size_t) c][(size_t) idx];
            }
            float out = (dry + (y - dry) * mixSm) * outSm;
            if (! std::isfinite (out)) out = 0.0f;
            channels[c][n] = out;
        }
        if (o >= 0)
            winAcc[(size_t) (o & mask)] = 0.0f;
        ++writePos;
    }
}

} // namespace vox
