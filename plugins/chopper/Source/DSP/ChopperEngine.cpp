#include "ChopperEngine.h"

#include <algorithm>
#include <cmath>

namespace chopper
{

namespace
{
constexpr double kPi = 3.14159265358979323846;

int nextPow2 (int v)
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

// raised-cosine fade-out gain for position k of a fade lasting len samples (k = 0 .. len-1)
inline float fadeOut (int k, int len)
{
    return (float) (0.5 + 0.5 * std::cos (kPi * (double) (k + 1) / (double) (len + 1)));
}

inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}
} // namespace

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    fadeLen = std::max (16, (int) std::lround (fadeMs * 0.001 * sr));
    histLen = nextPow2 ((int) std::ceil (historySeconds * sr) + std::max (0, maxBlock));
    histMask = histLen - 1;
    capLen = (int) std::ceil (sr * 60.0 / minBpmForBuffers * sizeBeats (0)) + 4 * fadeLen + 16;
    for (auto& h : hist) h.assign ((size_t) histLen, 0.0f);
    for (auto& b : cap)
        for (auto& c : b) c.assign ((size_t) capLen, 0.0f);
    maxRun = (int64_t) (histLen / 2) - 4 * fadeLen - 64;
    grainLen = std::round (0.070 * sr);   // 70 ms grains: keeps 808 lows intact
    reset();
}

void Engine::reset()
{
    for (auto& h : hist) std::fill (h.begin(), h.end(), 0.0f);
    now = 0;
    current = Voice {};
    current.active = true;
    current.runStart = 0;
    for (auto& f : fading) f = Voice {};
    capFilled[0] = capFilled[1] = 0;
    fadeInPos = 1 << 30;
    curStep = -1;
    lastBeat = -1.0e9;
    freeBeat = 0.0;
    wasPlaying = false;
    lastHold = false;
    runFromHold = false;
    snapControls = true;
}

int Engine::sliceSamples (int sizeIndex, double bpm) const
{
    const double b = bpm > 1.0 ? bpm : 120.0;
    const auto s = (int64_t) std::llround (sizeBeats (sizeIndex) * 60.0 / b * sr);
    const int64_t lo = 4 * (int64_t) fadeLen + 8, hi = (int64_t) capLen - 2 * fadeLen - 4;
    return (int) std::clamp (s, lo, hi);
}

float Engine::readHist (int c, int64_t absIndex) const
{
    if (absIndex > now || now - absIndex >= (int64_t) histLen - 1) return 0.0f;
    return hist[c][(size_t) (absIndex & histMask)];
}

float Engine::readHistFrac (int c, double absPos) const
{
    const auto i0 = (int64_t) std::floor (absPos);
    const float t = (float) (absPos - (double) i0);
    if (t <= 0.0f) return readHist (c, i0);
    auto at = [&] (int64_t i) { return readHist (c, std::min (i, now)); };
    return hermite (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), t);
}

float Engine::renderVoice (const Voice& v, int c) const
{
    const int64_t t = now - v.runStart;
    switch (v.fx)
    {
        case Fx::off:
            return readHist (c, now);
        case Fx::gate:
            return 0.0f;
        case Fx::reverse:
        {
            // the audio leading up to the run, mirrored around the run start
            if (2 * t + 2 >= (int64_t) histLen) return 0.0f;
            return readHist (c, v.runStart - 1 - t);
        }
        case Fx::tapeStop:
        {
            const double L = (double) std::max<int64_t> (1, v.runLen);
            const double u = std::min (1.0, (double) t / L);
            constexpr double p = 1.4;  // speed = (1 - u)^p
            const double pos = (double) v.runStart + L * (1.0 - std::pow (1.0 - u, p + 1.0)) / (p + 1.0);
            // the last stretch of the stop fades to silence (real tape goes quiet as it halts)
            const double g = std::clamp ((1.0 - u) * 4.0, 0.0, 1.0);
            const double sg = g * g * (3.0 - 2.0 * g);
            const float gain = (float) (sg * sg);
            return gain > 0.0f ? gain * readHistFrac (c, std::min (pos, (double) now)) : 0.0f;
        }
        case Fx::halfSpeed:
            return readHistFrac (c, (double) v.runStart + 0.5 * (double) t);
        case Fx::pitchDown:
        {
            // two-grain delay-line shifter at ratio 0.5: delay grows 0.5 sample / sample, wraps every grain
            const double G = grainLen;
            const double d1 = std::fmod (0.5 * (double) t, G);
            const double d2 = std::fmod (0.5 * (double) t + 0.5 * G, G);
            const double s1 = std::sin (kPi * d1 / G), s2 = std::sin (kPi * d2 / G);
            return (float) (s1 * s1) * readHistFrac (c, (double) now - d1)
                 + (float) (s2 * s2) * readHistFrac (c, (double) now - d2);
        }
        case Fx::repeat:
        {
            const int64_t p = t - v.wrapBase;
            if (p < 0 || p >= capFilled[v.buf] || p >= capLen) return 0.0f;
            return cap[v.buf][c][(size_t) p];
        }
    }
    return 0.0f;
}

void Engine::pushFade (int len)
{
    int slot = -1;
    for (int i = 0; i < maxFades; ++i)
        if (! fading[(size_t) i].active) { slot = i; break; }
    if (slot < 0)
    {
        slot = 0;
        for (int i = 1; i < maxFades; ++i)
            if (fading[(size_t) i].fade < fading[(size_t) slot].fade) slot = i;
    }
    auto& f = fading[(size_t) slot];
    // a voice that is itself still fading in leaves from the level it reached
    const float startGain = fadeInPos < fadeInLen ? 1.0f - fadeOut (fadeInPos, fadeInLen) : 1.0f;
    f = current;
    f.startGain = startGain;
    f.active = true;
    f.fadeTotal = len;
    f.fade = len;
    fadeInPos = 0;
    fadeInLen = len;
}

void Engine::startVoice (Fx fx, int64_t runStart, int64_t runLen, int numCh)
{
    pushFade (fadeLen);
    runStart = std::max (runStart, now - (int64_t) histLen / 4);
    current = Voice {};
    current.active = true;
    current.fx = fx;
    current.runStart = runStart;
    current.runLen = std::clamp<int64_t> (runLen, 1, maxRun);
    current.wrapBase = 0;
    if (fx == Fx::repeat)
    {
        // use the other capture buffer: a fading repeat voice may still read the old one
        const int b = 1 - lastCapBuf;
        lastCapBuf = b;
        current.buf = b;
        // the part of the slice that already went by (run starts in the past) comes from the history
        const int64_t have = std::min<int64_t> (now - runStart, capLen);
        for (int c = 0; c < numCh; ++c)
            for (int64_t k = 0; k < have; ++k)
                cap[b][c][(size_t) k] = readHist (c, runStart + k);
        capFilled[b] = have;
    }
}

int64_t Engine::runLenFrom (int step, Fx fx, double stepB, double sw, double spb) const
{
    double beats = 0.0;
    for (int s = step; s < numSteps && pattern[(size_t) s] == fx; ++s)
        beats += (s & 1) ? stepB * (1.0 - sw) : stepB * (1.0 + sw);
    return (int64_t) std::llround (beats * spb);
}

void Engine::process (float* const* ch, int numCh, int n, const HostTime& t)
{
    numCh = std::min (numCh, 2);
    if (numCh <= 0 || n <= 0 || histLen == 0) return;

    const double bpm = std::clamp (t.bpm > 1.0 ? t.bpm : 120.0, 10.0, 999.0);
    const double spb = sr * 60.0 / bpm;      // samples per beat
    const double bps = 1.0 / spb;            // beats per sample
    const bool playing = t.playing && t.hasPpq;
    uiBpm.store (bpm, std::memory_order_relaxed);

    // musical position of the first sample
    bool jump = false;
    double beat0;
    if (playing)
    {
        const double predicted = lastBeat + bps;
        if (wasPlaying && std::abs (t.ppq - predicted) < 1.0e-3)
            beat0 = predicted;               // ignore host rounding jitter
        else
        {
            beat0 = t.ppq;
            jump = ! wasPlaying || std::abs (t.ppq - predicted) > 0.05;
        }
    }
    else
    {
        if (wasPlaying) { freeBeat = 0.0; jump = true; }
        beat0 = freeBeat;
    }
    wasPlaying = playing;

    const double stepB = rateBeats (params.rate);
    const double sw = std::clamp ((double) params.swing, 0.0, 1.0) * 0.5;
    const double patB = stepB * numSteps;
    const double evenLen = stepB * (1.0 + sw);
    const int slice = sliceSamples (params.size, bpm);
    const bool hold = params.hold;

    const float mixT = std::clamp (params.mix, 0.0f, 1.0f);
    const float gainT = (float) std::pow (10.0, std::clamp (params.outputDb, -60.0f, 24.0f) / 20.0);
    const float smooth = (float) (1.0 - std::exp (-1.0 / (0.010 * sr)));
    if (snapControls) { mixS = mixT; gainS = gainT; snapControls = false; }

    for (int i = 0; i < n; ++i)
    {
        ++now;
        for (int c = 0; c < numCh; ++c)
            hist[c][(size_t) (now & histMask)] = ch[c][i];

        // ---------- sequencer position ----------
        const double beat = beat0 + (double) i * bps;
        const double beatE = beat + 1.0e-9;   // a boundary that lands exactly on a sample belongs to the new step
        double pos = beatE - std::floor (beatE / patB) * patB;
        if (pos >= patB) pos -= patB;
        const double pair = std::floor (pos / (2.0 * stepB));
        const double within = pos - pair * 2.0 * stepB;
        const bool odd = within >= evenLen;
        const int step = std::clamp ((int) pair * 2 + (odd ? 1 : 0), 0, numSteps - 1);
        const double stepStartB = pair * 2.0 * stepB + (odd ? evenLen : 0.0);
        const double stepLenB = odd ? stepB * (1.0 - sw) : evenLen;
        const double elapsedB = std::max (0.0, pos - stepStartB - 1.0e-9);
        const int64_t stepStartAbs = now - (int64_t) std::llround (elapsedB * spb);

        const Fx want = hold ? Fx::repeat : pattern[(size_t) step];
        const bool stepChanged = step != curStep;
        const bool holdChanged = hold != lastHold;
        bool restart = (i == 0 && jump) || want != current.fx;

        if (! restart && stepChanged)
        {
            // keep a run going across consecutive steps with the same effect
            bool cont = false;
            if (runFromHold && hold) cont = true;
            else if (step != 0 && stepStartAbs < current.runStart + current.runLen - fadeLen) cont = true;
            if (want == Fx::off) cont = true;
            restart = ! cont;
        }

        if (restart && want == current.fx && (want == Fx::off || want == Fx::gate))
            restart = false;   // nothing to retrigger
        if (restart)
        {
            int64_t runLen = runLenFrom (step, want, stepB, sw, spb);
            if (hold) runLen = maxRun;
            startVoice (want, stepStartAbs, runLen, numCh);
            runFromHold = hold;
        }
        else if (holdChanged && ! hold && runFromHold)
        {
            // HOLD released on a pattern Repeat step: the held run becomes a normal run ending with this step's run
            current.runLen = (stepStartAbs - current.runStart) + runLenFrom (step, want, stepB, sw, spb);
            runFromHold = false;
        }
        else if (holdChanged && hold && current.fx == Fx::repeat)
            runFromHold = true;   // HOLD pressed during a pattern repeat: keep the slice, never break the run

        curStep = step;
        lastHold = hold;
        lastBeat = beat;

        // ---------- repeat capture + slice wrap ----------
        if (current.fx == Fx::repeat)
        {
            const int b = current.buf;
            if (capFilled[b] < capLen && current.runStart + capFilled[b] == now)
            {
                for (int c = 0; c < numCh; ++c)
                    cap[b][c][(size_t) capFilled[b]] = ch[c][i];
                ++capFilled[b];
            }
            const int64_t p = (now - current.runStart) - current.wrapBase;
            const int64_t R = std::min<int64_t> (slice, capLen - 2 * fadeLen - 2);
            if (p >= R)
            {
                pushFade (std::min<int> (fadeLen, (int) (R / 4)));
                current.wrapBase = now - current.runStart;
            }
        }

        // ---------- render ----------
        bool anyFade = false;
        float wet[2] = { 0.0f, 0.0f };
        const float gin = fadeInPos < fadeInLen ? 1.0f - fadeOut (fadeInPos, fadeInLen) : 1.0f;
        for (int c = 0; c < numCh; ++c)
            wet[c] = renderVoice (current, c) * gin;
        for (auto& f : fading)
        {
            if (! f.active) continue;
            anyFade = true;
            const float g = f.startGain * fadeOut (f.fadeTotal - f.fade, f.fadeTotal);
            for (int c = 0; c < numCh; ++c)
                wet[c] += renderVoice (f, c) * g;
            if (--f.fade <= 0) f.active = false;
        }
        if (fadeInPos < fadeInLen) ++fadeInPos;

        mixS += (mixT - mixS) * smooth;
        gainS += (gainT - gainS) * smooth;
        if (std::abs (mixS - mixT) < 1.0e-5f) mixS = mixT;
        if (std::abs (gainS - gainT) < 1.0e-6f) gainS = gainT;

        const bool pureDry = current.fx == Fx::off && ! anyFade && fadeInPos >= fadeInLen;
        for (int c = 0; c < numCh; ++c)
        {
            const float dry = ch[c][i];
            const float y = pureDry ? dry : wet[c] * mixS + dry * (1.0f - mixS);
            ch[c][i] = y * gainS;
        }

        if (i == n - 1)
        {
            uiStep.store (step, std::memory_order_relaxed);
            uiFx.store ((int) want, std::memory_order_relaxed);
            uiStepPhase.store (stepLenB > 0.0 ? elapsedB / stepLenB : 0.0, std::memory_order_relaxed);
        }
    }

    if (! playing) freeBeat = beat0 + (double) n * bps;
}

} // namespace chopper
