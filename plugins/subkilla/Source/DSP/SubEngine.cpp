#include "SubEngine.h"

#include <algorithm>
#include <cmath>

namespace subkilla
{

namespace
{
constexpr double twoPi = 6.283185307179586;
constexpr float pi = 3.14159265358979f;

float onePoleCoef (double seconds, double sr)
{
    if (seconds <= 0.0) return 0.0f;
    return (float) std::exp (-1.0 / (seconds * sr));
}

double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

float maxDriveDb (SatType t)
{
    switch (t)
    {
        case SatType::tube: return 30.0f;
        case SatType::tape: return 30.0f;
        case SatType::hard: return 32.0f;
        case SatType::fold: return 20.0f;
    }
    return 30.0f;
}
} // namespace

float driveGainFor (SatType t, float drive)
{
    return std::pow (10.0f, std::clamp (drive, 0.0f, 1.0f) * maxDriveDb (t) / 20.0f);
}

const char* noteName (int midiNote)
{
    static const auto table = []
    {
        std::array<std::array<char, 6>, 128> t {};
        const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int n = 0; n < 128; ++n)
        {
            const char* nm = names[n % 12];
            const int oct = n / 12 - 1;
            int k = 0;
            for (const char* c = nm; *c != 0; ++c) t[(size_t) n][(size_t) k++] = *c;
            if (oct < 0) { t[(size_t) n][(size_t) k++] = '-'; t[(size_t) n][(size_t) k++] = (char) ('0' - oct); }
            else t[(size_t) n][(size_t) k++] = (char) ('0' + oct);
            t[(size_t) n][(size_t) k] = 0;
        }
        return t;
    }();
    return table[(size_t) std::clamp (midiNote, 0, 127)].data();
}

// ================= Halfband =================

void Halfband::init()
{
    constexpr int c = taps / 2;
    constexpr double beta = 7.5;
    double sum = 0.0;
    std::array<double, taps> tmp {};
    for (int n = 0; n < taps; ++n)
    {
        const int k = n - c;
        double s = k == 0 ? 0.5 : std::sin (0.5 * 3.141592653589793 * k) / (3.141592653589793 * k);
        const double r = (double) k / c;
        s *= besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
        tmp[(size_t) n] = s;
        sum += s;
    }
    for (int n = 0; n < taps; ++n)
        h[(size_t) n] = (float) (tmp[(size_t) n] / sum);
    reset();
}

void Halfband::reset()
{
    upBuf.fill (0.0f);
    downBuf.fill (0.0f);
    upPos = downPos = 0;
}

float Halfband::push (float x, std::array<float, taps * 2>& buf, int& pos) const
{
    pos = (pos == 0 ? taps : pos) - 1;
    buf[(size_t) pos] = buf[(size_t) (pos + taps)] = x;
    const float* b = buf.data() + pos;
    float acc = 0.0f;
    for (int k = 0; k < taps; ++k)
        acc += h[(size_t) k] * b[k];
    return acc;
}

void Halfband::up (float x, float& y0, float& y1)
{
    y0 = 2.0f * push (x, upBuf, upPos);
    y1 = 2.0f * push (0.0f, upBuf, upPos);
}

float Halfband::down (float x0, float x1)
{
    // the first output of the pair is discarded; only the filter state matters
    downPos = (downPos == 0 ? taps : downPos) - 1;
    downBuf[(size_t) downPos] = downBuf[(size_t) (downPos + taps)] = x0;
    return push (x1, downBuf, downPos);
}

// ================= Engine =================

Engine::Engine()
{
    os.init();
    // gain compensation: RMS of a -6 dBFS sine before / after each saturator, across the drive range
    constexpr int N = 512;
    for (int t = 0; t < 4; ++t)
        for (int s = 0; s < compSteps; ++s)
        {
            const float g = driveGainFor ((SatType) t, (float) s / (compSteps - 1));
            double mean = 0.0, sq = 0.0;
            std::array<float, N> y {};
            for (int i = 0; i < N; ++i)
            {
                y[(size_t) i] = saturate ((SatType) t, g * 0.5f * (float) std::sin (twoPi * i / N));
                mean += y[(size_t) i];
            }
            mean /= N;
            for (int i = 0; i < N; ++i) sq += (y[(size_t) i] - mean) * (y[(size_t) i] - mean);
            const double rmsOut = std::sqrt (sq / N), rmsIn = 0.5 / std::sqrt (2.0);
            // full compensation would make drive pure "tone"; keep a little extra loudness like real hardware
            compTable[(size_t) t][(size_t) s] = (float) std::clamp (std::pow (rmsIn / std::max (1e-6, rmsOut), 0.85), 0.08, 2.0);
        }
    prepare (48000.0, 512);
}

float Engine::saturate (SatType t, float x)
{
    switch (t)
    {
        case SatType::tube:
        {
            constexpr float b = 0.25f;
            static const float tb = std::tanh (b), norm = 1.0f / (1.0f - tb * tb);
            return (std::tanh (x + b) - tb) * norm;
        }
        case SatType::tape:
            return (2.0f / pi) * std::atan (x * (pi * 0.5f));
        case SatType::hard:
            return std::clamp (x, -1.0f, 1.0f);
        case SatType::fold:
        {
            float u = (x + 1.0f) * 0.25f;
            u -= std::floor (u);
            return u < 0.5f ? 4.0f * u - 1.0f : 3.0f - 4.0f * u;
        }
    }
    return x;
}

float Engine::compensation (SatType t, float drive) const
{
    const float pos = std::clamp (drive, 0.0f, 1.0f) * (compSteps - 1);
    const int i = std::min ((int) pos, compSteps - 2);
    const float f = pos - (float) i;
    const auto& row = compTable[(size_t) t];
    return row[(size_t) i] + (row[(size_t) i + 1] - row[(size_t) i]) * f;
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v = Voice {};
    stackSize = 0;
    pedal = false;
    os.reset();
    tapeLp = svfIc1 = svfIc2 = dcX = dcY = 0.0f;
    std::fill (std::begin (hp1), std::end (hp1), 0.0f);
    std::fill (std::begin (hp2), std::end (hp2), 0.0f);
    std::fill (std::begin (ap), std::end (ap), 0.0f);
    limGain = 1.0f;
    bendTarget = sBend = 0.0f;
    updateCoefficients();
    // snap smoothers to their targets
    sDriveGain = driveGainFor (params.sat, params.drive);
    sWet = params.distMix * std::min (1.0f, params.drive * 25.0f);
    sComp = compensation (params.sat, params.drive);
    sCutoff = std::log2 (std::clamp (params.cutoffHz, 20.0f, 20000.0f));
    sReso = params.reso;
    sWidth = params.width;
    sOut = std::pow (10.0f, params.outputDb / 20.0f);
    sSub = params.sub;
    sClick = params.click;
    sTune = params.tuneSemis + params.fineCents * 0.01f;
    sFltEnv = params.fltEnv;
    uiHz = uiNote = uiEnv = uiGrDb = 0.0f;
    uiGliding = false;
}

void Engine::updateCoefficients()
{
    punchCoef = onePoleCoef (std::max (1.0f, params.punchMs) * 0.001 / 4.605, sr);       // to 1 % in punchMs
    decayCoef = onePoleCoef (std::max (5.0f, params.decayMs) * 0.001 / 6.9078, sr);      // to -60 dB in decayMs
    releaseCoef = onePoleCoef (std::max (2.0f, params.releaseMs) * 0.001 / 6.9078, sr);
    clickCoef = onePoleCoef (0.0012, sr);
    smoothCoef = 1.0f - onePoleCoef (0.012, sr);
    dcCoef = (float) (1.0 - twoPi * 5.0 / sr);
    hpG = (float) std::tan (3.141592653589793 * 200.0 / sr);
    const double apF[4] = { 330.0, 790.0, 1900.0, 4400.0 };
    for (int i = 0; i < 4; ++i)
    {
        const double t = std::tan (3.141592653589793 * std::min (apF[i], sr * 0.45) / sr);
        apC[i] = (float) ((t - 1.0) / (t + 1.0));
    }
    limRelease = onePoleCoef (0.09, sr);
    tapeCoef = 1.0f - onePoleCoef (1.0 / (twoPi * 9000.0), sr * 2.0);
}

int Engine::getActiveVoices() const
{
    int n = 0;
    for (const auto& v : voices) n += v.stage != Voice::idle ? 1 : 0;
    return n;
}

// ---------------- note handling ----------------

void Engine::trigger (Voice& v, bool fromSilence)
{
    if (fromSilence)
    {
        v.phase = v.subPhase = 0.0;   // start on a zero crossing: no click, consistent punch
        v.env = 0.0f;
    }
    v.stage = Voice::attack;
    v.attackStep = 1.0f / (float) std::max (1.0, std::max (0.3f, params.attackMs) * 0.001 * sr);
    v.pitchEnv = params.punchSemis;
    v.clickEnv = 1.0f;
    v.clickPhase = 0.0;
    ++retriggers;
}

void Engine::slideTo (Voice& v, int note)
{
    v.note = note;
    v.target = (float) note;
    const double samples = params.glideMs * 0.001 * sr;
    if (samples < 1.0)
    {
        v.pitch = v.target;
        v.glideStep = 0.0f;
    }
    else
        v.glideStep = (float) ((v.target - v.pitch) / samples);
}

void Engine::releaseVoice (Voice& v)
{
    v.gate = false;
    v.sustained = false;
    if (v.stage != Voice::idle) v.stage = Voice::release;
}

void Engine::noteOn (int note, float vel)
{
    vel = std::clamp (vel, 0.0f, 1.0f);
    if (params.mode == VoiceMode::poly)
    {
        Voice* pick = nullptr;
        for (auto& v : voices)   // same note re-struck: reuse its voice
            if (v.stage != Voice::idle && v.note == note) { pick = &v; break; }
        if (pick == nullptr)
            for (auto& v : voices)
                if (v.stage == Voice::idle) { pick = &v; break; }
        if (pick == nullptr)     // steal: oldest released voice, else oldest
        {
            for (auto& v : voices)
                if (! v.gate && ! v.sustained && (pick == nullptr || v.age < pick->age)) pick = &v;
            if (pick == nullptr)
                for (auto& v : voices)
                    if (pick == nullptr || v.age < pick->age) pick = &v;
        }
        Voice& v = *pick;
        const bool fromSilence = v.stage == Voice::idle || v.env < 1.0e-3f;
        v.note = note;
        v.pitch = v.target = (float) note;
        v.glideStep = 0.0f;
        v.vel = vel;
        v.gate = true;
        v.sustained = false;
        v.age = ++ageCounter;
        trigger (v, fromSilence);
        return;
    }

    // mono / legato: note stack
    for (int i = 0; i < stackSize; ++i)
        if (stack[(size_t) i] == note)
        {
            std::copy (stack.begin() + i + 1, stack.begin() + stackSize, stack.begin() + i);
            --stackSize;
            break;
        }
    if (stackSize == (int) stack.size())
    {
        std::copy (stack.begin() + 1, stack.end(), stack.begin());
        --stackSize;
    }
    stack[(size_t) stackSize++] = note;

    Voice& v = voices[0];
    const bool overlapping = v.stage != Voice::idle && (stackSize > 1 || v.sustained);
    if (overlapping)
    {
        slideTo (v, note);                       // the trap 808 slide
        if (params.mode == VoiceMode::mono)
        {
            v.vel = vel;
            trigger (v, false);
        }
    }
    else
    {
        v.note = note;
        v.pitch = v.target = (float) note;
        v.glideStep = 0.0f;
        v.vel = vel;
        trigger (v, v.stage == Voice::idle || v.env < 1.0e-3f);
    }
    v.gate = true;
    v.sustained = false;
    v.age = ++ageCounter;
}

void Engine::noteOff (int note)
{
    if (params.mode == VoiceMode::poly)
    {
        for (auto& v : voices)
            if (v.gate && v.note == note)
            {
                if (pedal) { v.gate = false; v.sustained = true; }
                else releaseVoice (v);
            }
        return;
    }
    for (int i = 0; i < stackSize; ++i)
        if (stack[(size_t) i] == note)
        {
            std::copy (stack.begin() + i + 1, stack.begin() + stackSize, stack.begin() + i);
            --stackSize;
            break;
        }
    Voice& v = voices[0];
    if (v.note != note || ! v.gate) return;
    if (stackSize > 0)
        slideTo (v, stack[(size_t) stackSize - 1]);   // fall back to the still-held note
    else if (pedal)
    {
        v.gate = false;
        v.sustained = true;
    }
    else
        releaseVoice (v);
}

void Engine::allOff()
{
    stackSize = 0;
    pedal = false;
    for (auto& v : voices) releaseVoice (v);
}

void Engine::handle (const Event& e)
{
    switch (e.type)
    {
        case Event::noteOn:
            if (e.value <= 0.0f) noteOff (e.note);
            else noteOn (e.note, e.value);
            break;
        case Event::noteOff: noteOff (e.note); break;
        case Event::pitchBend: bendTarget = std::clamp (e.value, -1.0f, 1.0f) * bendRangeSemis; break;
        case Event::allNotesOff: allOff(); break;
        case Event::sustainPedal:
        {
            const bool down = e.value > 0.5f;
            if (pedal && ! down)
            {
                for (auto& v : voices)
                    if (v.sustained && (params.mode == VoiceMode::poly || stackSize == 0)) releaseVoice (v);
            }
            pedal = down;
            break;
        }
    }
}

// ---------------- rendering ----------------

void Engine::process (float* left, float* right, int n, const Event* events, int numEvents)
{
    updateCoefficients();
    float minGain = 1.0f;
    int pos = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        const int at = std::clamp (events[i].offset, pos, n);
        if (at > pos) render (left, right, pos, at);
        minGain = std::min (minGain, limGain);
        handle (events[i]);
        pos = at;
    }
    if (pos < n) render (left, right, pos, n);
    minGain = std::min (minGain, limGain);

    // UI probes: most recent sounding voice
    const Voice* latest = nullptr;
    for (const auto& v : voices)
        if (v.stage != Voice::idle && (latest == nullptr || v.age > latest->age)) latest = &v;
    if (latest != nullptr)
    {
        uiNote = latest->pitch + sTune + sBend;
        uiHz = 440.0f * std::exp2 ((uiNote - 69.0f) / 12.0f);
        uiGliding = std::abs (latest->glideStep) > 0.0f;
        uiEnv = latest->env;
    }
    else
    {
        uiHz = 0.0f;
        uiGliding = false;
        uiEnv = 0.0f;
    }
    uiGrDb = minGain < 0.9999f ? 20.0f * std::log10 (std::max (1.0e-4f, minGain)) : 0.0f;
}

void Engine::render (float* L, float* R, int start, int end)
{
    const Params& p = params;
    // smoothing targets
    const float tDrive = driveGainFor (p.sat, p.drive);
    const float tWet = std::clamp (p.distMix, 0.0f, 1.0f) * std::min (1.0f, p.drive * 25.0f);
    const float tComp = compensation (p.sat, p.drive);
    const float tCut = std::log2 (std::clamp (p.cutoffHz, 20.0f, 20000.0f));
    const float tReso = std::clamp (p.reso, 0.0f, 1.0f);
    const float tWidth = std::clamp (p.width, 0.0f, 1.0f);
    const float tOut = std::pow (10.0f, std::clamp (p.outputDb, -60.0f, 24.0f) / 20.0f);
    const float tSub = std::clamp (p.sub, 0.0f, 1.0f);
    const float tClick = std::clamp (p.click, 0.0f, 1.0f);
    const float tTune = p.tuneSemis + p.fineCents * 0.01f;
    const float tFltEnv = std::clamp (p.fltEnv, 0.0f, 1.0f);
    const float k = smoothCoef;
    const float sustain = std::clamp (p.sustain, 0.0f, 1.0f);
    const float nyq = (float) (sr * 0.45);
    const float invSr = (float) (1.0 / sr);
    const float clickInc = (float) (2600.0 / sr);
    const SatType sat = p.sat;

    for (int i = start; i < end; ++i)
    {
        sDriveGain += (tDrive - sDriveGain) * k;
        sWet += (tWet - sWet) * k;
        sComp += (tComp - sComp) * k;
        sCutoff += (tCut - sCutoff) * k;
        sReso += (tReso - sReso) * k;
        sWidth += (tWidth - sWidth) * k;
        sOut += (tOut - sOut) * k;
        sSub += (tSub - sSub) * k;
        sClick += (tClick - sClick) * k;
        sTune += (tTune - sTune) * k;
        sFltEnv += (tFltEnv - sFltEnv) * k;
        sBend += (bendTarget - sBend) * k;

        float sum = 0.0f, envMax = 0.0f;
        for (auto& v : voices)
        {
            if (v.stage == Voice::idle) continue;

            if (std::abs (v.glideStep) > 0.0f)
            {
                v.pitch += v.glideStep;
                if ((v.glideStep > 0.0f && v.pitch >= v.target) || (v.glideStep < 0.0f && v.pitch <= v.target))
                {
                    v.pitch = v.target;
                    v.glideStep = 0.0f;
                }
            }
            v.pitchEnv *= punchCoef;
            const float semis = v.pitch + sTune + sBend + v.pitchEnv;
            const float hz = std::min (nyq, 440.0f * std::exp2 ((semis - 69.0f) * (1.0f / 12.0f)));
            const double inc = (double) hz * invSr;

            const float body = (float) std::sin (twoPi * v.phase);
            const float subS = (float) std::sin (twoPi * v.subPhase);
            v.phase += inc;
            if (v.phase >= 1.0) v.phase -= 1.0;
            v.subPhase += inc * 0.5;
            if (v.subPhase >= 1.0) v.subPhase -= 1.0;

            switch (v.stage)
            {
                case Voice::attack:
                    v.env += v.attackStep;
                    if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Voice::decay; }
                    break;
                case Voice::decay:
                    v.env = sustain + (v.env - sustain) * decayCoef;
                    if (sustain < 1.0e-4f && v.env < 1.0e-4f) v.stage = Voice::idle;
                    break;
                case Voice::release:
                    v.env *= releaseCoef;
                    if (v.env < 1.0e-4f) v.stage = Voice::idle;
                    break;
                case Voice::idle: break;
            }

            const float vg = 0.5f * (0.7f + 0.3f * v.vel);
            const float osc = body * (1.0f - 0.45f * sSub) + subS * sSub * 0.85f;
            float s = osc * v.env * vg;

            if (v.clickEnv > 1.0e-4f)
            {
                noise = noise * 1664525u + 1013904223u;
                const float nz = (float) (int) (noise >> 9) * (1.0f / 4194304.0f) - 1.0f;
                const float ck = 0.55f * (float) std::sin (twoPi * v.clickPhase) + 0.45f * nz;
                v.clickPhase += clickInc;
                if (v.clickPhase >= 1.0) v.clickPhase -= 1.0;
                s += ck * v.clickEnv * sClick * 0.7f * vg;
                v.clickEnv *= clickCoef;
            }
            if (v.stage == Voice::idle) { v.env = 0.0f; v.clickEnv = 0.0f; }
            sum += s;
            envMax = std::max (envMax, v.env);
        }

        // ---- saturation, 2x oversampled
        float u0, u1;
        os.up (sum, u0, u1);
        auto shape = [&] (float x)
        {
            float w = saturate (sat, x * sDriveGain) * sComp;
            if (sat == SatType::tape)
            {
                tapeLp += (w - tapeLp) * tapeCoef;
                w = tapeLp;
            }
            return x + (w - x) * sWet;
        };
        float y = os.down (shape (u0), shape (u1));

        // ---- low-pass (TPT SVF), opened by the envelope
        {
            const float cutLog = sCutoff + sFltEnv * 7.0f * envMax;
            const float fc = std::min (nyq, std::exp2 (cutLog));
            const float g = std::tan (pi * fc * invSr);
            const float kq = 1.0f / (0.707f + sReso * 6.5f);
            const float a1 = 1.0f / (1.0f + g * (g + kq));
            const float a2 = g * a1, a3 = g * a2;
            const float v3 = y - svfIc2;
            const float v1 = a1 * svfIc1 + a2 * v3;
            const float v2 = svfIc2 + a2 * svfIc1 + a3 * v3;
            svfIc1 = 2.0f * v1 - svfIc1;
            svfIc2 = 2.0f * v2 - svfIc2;
            y = v2;
        }

        // ---- DC block
        const float dc = y - dcX + dcCoef * dcY;
        dcX = y;
        dcY = dc;
        y = dc;

        // ---- width: side signal from the band above ~200 Hz only (24 dB/oct), the sub stays mono
        float l = y, r = y;
        {
            float h = y;   // filters always run so moving WIDTH never clicks
            float* st[4] = { &hp1[0], &hp1[1], &hp2[0], &hp2[1] };
            for (auto* s : st)
            {
                const float vv = (h - *s) * hpG / (1.0f + hpG);
                const float lp = vv + *s;
                *s = lp + vv;
                h -= lp;
            }
            float a = h;
            for (int j = 0; j < 4; ++j)
            {
                const float o = apC[j] * a + ap[j];
                ap[j] = a - apC[j] * o;
                a = o;
            }
            const float side = a * sWidth * 0.75f;
            l += side;
            r -= side;
        }

        l *= sOut;
        r *= sOut;

        // ---- soft limiter: instant gain riding to the ceiling + soft knee that can never pass it
        if (p.limiter)
        {
            const float peak = std::max (std::abs (l), std::abs (r));
            const float target = peak > limiterCeiling ? limiterCeiling / peak : 1.0f;
            limGain = target < limGain ? target : target + (limGain - target) * limRelease;
            l *= limGain;
            r *= limGain;
            auto knee = [] (float x)
            {
                constexpr float t = limiterCeiling * 0.85f, span = limiterCeiling - t;
                const float a = std::abs (x);
                if (a <= t) return x;
                const float yv = t + span * std::tanh ((a - t) / span);
                return x < 0.0f ? -yv : yv;
            };
            l = knee (l);
            r = knee (r);
        }
        else
            limGain = 1.0f;

        L[i] = l;
        R[i] = r;
    }
}

} // namespace subkilla
