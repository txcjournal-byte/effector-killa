#include "InternalSynth.h"
#include <cmath>
#include <limits>

namespace bd
{

namespace
{
    constexpr double kTwoPi = 6.283185307179586;
    constexpr int kCoefInterval = 16;

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    inline float wrap01 (double& ph, double inc) noexcept
    {
        ph += inc;
        if (ph >= 1.0) ph -= std::floor (ph);
        return (float) ph;
    }

    inline float bendNorm (int v) noexcept
    {
        const int d = v - 8192;
        return d >= 0 ? (float) d / 8191.0f : (float) d / 8192.0f;
    }

    using P = juce::ADSR::Parameters;

    InternalSynth::Def makeDef (int s)
    {
        InternalSynth::Def d;
        switch (s)
        {
            case InternalSynth::glideLead:
                d.saw = 0.7f; d.square = 0.32f; d.sub = 0.18f; d.unison = 9.0f;
                d.cutoff = 1700.0f; d.keyTrack = 0.6f; d.resonance = 0.28f; d.envAmount = 1.4f;
                d.timbreAmount = 2.6f; d.pressureCut = 1.0f; d.pressureAmp = 0.35f;
                d.amp = P { 0.004f, 0.4f, 0.85f, 0.25f }; d.filter = P { 0.003f, 0.5f, 0.4f, 0.3f };
                d.drive = 0.25f; d.chorus = 0.35f; d.reverb = 0.18f; d.gain = 0.42f;
                break;

            case InternalSynth::pluck:
                d.saw = 0.8f; d.square = 0.3f; d.pulseWidth = 0.3f; d.unison = 6.0f;
                d.cutoff = 520.0f; d.keyTrack = 0.7f; d.resonance = 0.3f; d.envAmount = 4.6f;
                d.timbreAmount = 2.0f; d.pressureCut = 0.5f; d.pressureAmp = 0.2f;
                d.amp = P { 0.001f, 0.5f, 0.0f, 0.25f }; d.filter = P { 0.001f, 0.22f, 0.0f, 0.2f };
                d.chorus = 0.25f; d.reverb = 0.22f; d.gain = 0.5f;
                break;

            case InternalSynth::eightOhEight:
                d.saw = 0.0f; d.sine = 1.0f;
                d.cutoff = 2200.0f; d.keyTrack = 0.0f; d.resonance = 0.0f; d.envAmount = 0.6f;
                d.timbreAmount = 2.2f; d.pressureCut = 0.6f; d.pressureAmp = 0.3f;
                d.amp = P { 0.001f, 2.2f, 0.55f, 0.35f }; d.filter = P { 0.001f, 0.3f, 0.3f, 0.3f };
                d.drive = 2.2f; d.punchSemis = 7.0f; d.punchTime = 0.035f; d.gain = 0.55f;
                break;

            case InternalSynth::pad:
                d.saw = 0.8f; d.sub = 0.1f; d.unison = 14.0f;
                d.cutoff = 850.0f; d.keyTrack = 0.4f; d.resonance = 0.15f; d.envAmount = 1.0f;
                d.timbreAmount = 2.6f; d.pressureCut = 1.5f; d.pressureAmp = 0.5f;
                d.amp = P { 0.45f, 1.0f, 0.85f, 1.2f }; d.filter = P { 0.8f, 1.5f, 0.5f, 1.0f };
                d.chorus = 0.6f; d.reverb = 0.45f; d.gain = 0.34f;
                break;

            case InternalSynth::bell:
                d.saw = 0.0f; d.sine = 1.0f; d.fmRatio = 3.5f; d.fmIndex = 2.2f; d.fmDecay = 0.9f;
                d.cutoff = 7000.0f; d.keyTrack = 0.2f; d.resonance = 0.0f; d.envAmount = 0.5f;
                d.timbreAmount = 1.5f; d.pressureCut = 0.5f; d.pressureAmp = 0.3f;
                d.amp = P { 0.001f, 1.6f, 0.0f, 0.9f }; d.filter = P { 0.001f, 1.0f, 0.4f, 0.8f };
                d.chorus = 0.2f; d.reverb = 0.4f; d.gain = 0.45f;
                break;

            case InternalSynth::subBass:
            default:
                d.saw = 0.0f; d.sine = 1.0f; d.square = 0.06f;
                d.cutoff = 600.0f; d.keyTrack = 0.3f; d.resonance = 0.0f; d.envAmount = 0.0f;
                d.timbreAmount = 1.5f; d.pressureCut = 0.5f; d.pressureAmp = 0.25f;
                d.amp = P { 0.003f, 0.2f, 1.0f, 0.12f }; d.filter = P { 0.001f, 0.2f, 1.0f, 0.2f };
                d.drive = 0.6f; d.gain = 0.6f;
                break;
        }
        return d;
    }
}

const char* InternalSynth::getSoundName (int s) noexcept
{
    static const char* const names[numSounds] = { "Glide Lead", "Pluck", "808", "Pad", "Bell", "Sub Bass" };
    return juce::isPositiveAndBelow (s, (int) numSounds) ? names[s] : "";
}

const InternalSynth::Def& InternalSynth::getDef (int s) noexcept
{
    static const std::array<Def, numSounds> defs { makeDef (0), makeDef (1), makeDef (2), makeDef (3), makeDef (4), makeDef (5) };
    return defs[(size_t) juce::jlimit (0, (int) numSounds - 1, s)];
}

InternalSynth::InternalSynth()
{
    chanPressure.fill (0.5f);
    chanTimbre.fill (0.5f);
}

void InternalSynth::prepare (double sr, int maxBlock)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    scratch.setSize (2, juce::jmax (16, maxBlock), false, true, false);
    reverb.setSampleRate (sampleRate);

    const int chorusLen = (int) juce::nextPowerOfTwo ((int) (sampleRate * 0.05));
    for (auto& b : chorusBuf) b.assign ((size_t) chorusLen, 0.0f);
    chorusPos = 0;

    pitchCoef = 1.0f - std::exp (-1.0f / (0.0015f * (float) sampleRate));
    ctrlCoef  = 1.0f - std::exp (-1.0f / (0.004f * (float) sampleRate));

    for (auto& v : voices)
    {
        v.ampEnv.setSampleRate (sampleRate);
        v.filtEnv.setSampleRate (sampleRate);
    }

    setSound (sound);
    reset();
}

void InternalSynth::reset() noexcept
{
    for (auto& v : voices)
    {
        v.active = false;
        v.ampEnv.reset();
        v.filtEnv.reset();
    }
    chanBend.fill (0.0f);
    chanPressure.fill (0.5f);
    chanTimbre.fill (0.5f);
    reverb.reset();
    for (auto& b : chorusBuf) std::fill (b.begin(), b.end(), 0.0f);
}

void InternalSynth::setSound (int s) noexcept
{
    sound = juce::jlimit (0, (int) numSounds - 1, s);
    const auto& d = getDef (sound);
    chorusMixTarget = d.chorus;
    reverbMixTarget = d.reverb;

    juce::Reverb::Parameters rp;
    rp.roomSize = 0.72f; rp.damping = 0.45f; rp.width = 1.0f;
    rp.wetLevel = d.reverb * 0.55f; rp.dryLevel = 1.0f - d.reverb * 0.2f;
    reverb.setParameters (rp);
}

void InternalSynth::setBendRanges (float member, float master, bool legacyChannel1) noexcept
{
    memberRange = member;
    masterRange = master;
    legacy1 = legacyChannel1;
}

void InternalSynth::allNotesOff() noexcept
{
    for (auto& v : voices)
        if (v.active) { v.ampEnv.noteOff(); v.filtEnv.noteOff(); v.released = true; }
}

int InternalSynth::getNumActiveVoices() const noexcept
{
    int n = 0;
    for (auto& v : voices) if (v.active) ++n;
    return n;
}

//==============================================================================
float InternalSynth::targetPitch (const Voice& v) const noexcept
{
    auto rangeFor = [this] (int ch) { return ch == 1 ? (legacy1 ? memberRange : masterRange) : memberRange; };
    float p = (float) v.note + chanBend[(size_t) v.channel] * rangeFor (v.channel);
    if (v.channel != 1)
        p += chanBend[1] * rangeFor (1);
    return p;
}

void InternalSynth::handle (const juce::MidiMessage& m) noexcept
{
    const int ch = juce::jlimit (1, 16, m.getChannel());

    if (m.isNoteOn())
        noteOn (ch, m.getNoteNumber(), (float) m.getVelocity() / 127.0f);
    else if (m.isNoteOff())
        noteOff (ch, m.getNoteNumber());
    else if (m.isPitchWheel())
        chanBend[(size_t) ch] = bendNorm (m.getPitchWheelValue());
    else if (m.isChannelPressure())
        chanPressure[(size_t) ch] = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 74)
            chanTimbre[(size_t) ch] = (float) m.getControllerValue() / 127.0f;
        else if (cc == 123 || cc == 120)
            for (auto& v : voices)
                if (v.active && (v.channel == ch || ch == 1))
                {
                    v.ampEnv.noteOff(); v.filtEnv.noteOff(); v.released = true;
                }
    }
}

void InternalSynth::noteOn (int ch, int note, float vel) noexcept
{
    Voice* slot = nullptr;

    // same key on the same channel: retrigger that voice
    for (auto& v : voices)
        if (v.active && v.channel == ch && v.note == note) { slot = &v; break; }

    if (slot == nullptr)
        for (auto& v : voices)
            if (! v.active) { slot = &v; break; }

    if (slot == nullptr)
    {
        // steal: the oldest releasing voice first, otherwise the oldest voice
        for (int pass = 0; pass < 2 && slot == nullptr; ++pass)
        {
            int64_t oldest = std::numeric_limits<int64_t>::max();
            for (auto& v : voices)
                if ((pass == 1 || v.released) && v.age < oldest) { oldest = v.age; slot = &v; }
        }
    }

    auto& v = *slot;
    v.def      = &getDef (sound);
    v.active   = true;
    v.channel  = ch;
    v.note     = note;
    v.velocity = 0.35f + 0.65f * juce::jlimit (0.0f, 1.0f, vel);
    v.age      = ++ageCounter;
    v.semis    = targetPitch (v);
    v.pressure = chanPressure[(size_t) ch];
    v.timbre   = chanTimbre[(size_t) ch];
    v.punch    = v.def->punchSemis;
    v.fmEnv    = 1.0f;
    v.released = false;
    v.coefPhase = 0;
    v.filtLast = 0.0f;
    v.ic1 = v.ic2 = 0.0f;
    for (auto& ph : v.phase) ph = 0.0;
    v.subPhase = v.sinePhase = v.modPhase = 0.0;
    v.phase[1] = 0.33; v.phase[2] = 0.67;   // unison voices start apart

    v.ampEnv.setParameters (v.def->amp);
    v.filtEnv.setParameters (v.def->filter);
    v.ampEnv.reset();  v.ampEnv.noteOn();
    v.filtEnv.reset(); v.filtEnv.noteOn();
    updateCoefficients (v);
}

void InternalSynth::noteOff (int ch, int note) noexcept
{
    for (auto& v : voices)
        if (v.active && v.channel == ch && v.note == note && ! v.released)
        {
            v.ampEnv.noteOff();
            v.filtEnv.noteOff();
            v.released = true;
        }
}

void InternalSynth::updateCoefficients (Voice& v) noexcept
{
    const auto& d = *v.def;
    const double sr = sampleRate;
    const double f0 = 440.0 * std::exp2 (((double) v.semis + (double) v.punch - 69.0) / 12.0);
    const double base = juce::jlimit (1.0, sr * 0.45, f0) / sr;

    if (d.unison > 0.0f)
    {
        const double r = std::exp2 ((double) d.unison / 1200.0);
        v.inc[0] = (float) base; v.inc[1] = (float) (base * r); v.inc[2] = (float) (base / r);
    }
    else
    {
        v.inc[0] = v.inc[1] = v.inc[2] = (float) base;
    }
    v.subInc  = (float) (base * 0.5);
    v.sineInc = (float) base;
    v.modInc  = (float) (base * (double) d.fmRatio);

    // filter cutoff
    const float env = v.filtEnv.isActive() ? v.filtLast : 0.0f;
    const float oct = d.keyTrack * (v.semis - 60.0f) / 12.0f
                    + d.envAmount * env
                    + d.timbreAmount * (v.timbre - 0.5f) * 2.0f
                    + d.pressureCut * (v.pressure - 0.5f);
    const double fc = juce::jlimit (30.0, sr * 0.45, (double) d.cutoff * std::exp2 ((double) oct));
    const float g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
    const float k = 2.0f - 1.94f * juce::jlimit (0.0f, 1.0f, d.resonance);
    v.g = g; v.k = k;
    v.a1 = 1.0f / (1.0f + g * (g + k));
    v.a2 = g * v.a1;
    v.a3 = g * v.a2;

    // punch + FM index decay
    if (std::abs (d.punchSemis) > 0.0f)
        v.punch *= (float) std::exp (-(double) kCoefInterval / ((double) d.punchTime * sr));
    if (d.fmRatio > 0.0f)
        v.fmEnv *= (float) std::exp (-(double) kCoefInterval / ((double) d.fmDecay * sr));
}

float InternalSynth::renderVoice (Voice& v) noexcept
{
    const auto& d = *v.def;

    v.semis    += (targetPitch (v) - v.semis) * pitchCoef;
    v.pressure += (chanPressure[(size_t) v.channel] - v.pressure) * ctrlCoef;
    v.timbre   += (chanTimbre[(size_t) v.channel] - v.timbre) * ctrlCoef;

    float x = 0.0f;

    if (d.saw > 0.0f || d.square > 0.0f)
    {
        const int n = d.unison > 0.0f ? 3 : 1;
        float saw = 0.0f;
        for (int u = 0; u < n; ++u)
        {
            const float dt = v.inc[u];
            const float t = (float) v.phase[u];
            saw += 2.0f * t - 1.0f - polyBlep (t, dt);
            if (u == 0 && d.square > 0.0f)
            {
                float sq = t < d.pulseWidth ? 1.0f : -1.0f;
                sq += polyBlep (t, dt);
                float t2 = t + 1.0f - d.pulseWidth;
                t2 -= std::floor (t2);
                sq -= polyBlep (t2, dt);
                x += sq * d.square;
            }
            wrap01 (v.phase[u], dt);
        }
        x += saw * d.saw * (n == 3 ? 0.45f : 1.0f);
    }

    if (d.sine > 0.0f)
    {
        float phase = (float) (kTwoPi * v.sinePhase);
        if (d.fmRatio > 0.0f)
        {
            const float mod = std::sin ((float) (kTwoPi * v.modPhase)) * d.fmIndex * v.fmEnv * (0.4f + v.timbre);
            phase += mod;
            wrap01 (v.modPhase, v.modInc);
        }
        x += std::sin (phase) * d.sine;
        wrap01 (v.sinePhase, v.sineInc);
    }

    if (d.sub > 0.0f)
    {
        x += std::sin ((float) (kTwoPi * v.subPhase)) * d.sub;
        wrap01 (v.subPhase, v.subInc);
    }

    if (d.drive > 0.0f)
    {
        const float k = 1.0f + d.drive * 2.5f;
        x = std::tanh (x * k) / std::tanh (k);
    }

    // TPT state-variable low-pass
    const float v3 = x - v.ic2;
    const float v1 = v.a1 * v.ic1 + v.a2 * v3;
    const float v2 = v.ic2 + v.a2 * v.ic1 + v.a3 * v3;
    v.ic1 = 2.0f * v1 - v.ic1;
    v.ic2 = 2.0f * v2 - v.ic2;

    v.filtLast = v.filtEnv.getNextSample();
    const float amp = v.ampEnv.getNextSample() * v.velocity
                    * (1.0f - d.pressureAmp + d.pressureAmp * juce::jlimit (0.0f, 1.0f, v.pressure * 1.3f));

    if (! v.ampEnv.isActive())
        v.active = false;

    return v2 * amp * d.gain;
}

void InternalSynth::renderSegment (juce::AudioBuffer<float>& buf, int start, int num) noexcept
{
    auto* l = buf.getWritePointer (0, start);
    auto* r = buf.getWritePointer (1, start);

    for (auto& v : voices)
    {
        if (! v.active) continue;

        for (int i = 0; i < num; ++i)
        {
            if (++v.coefPhase >= kCoefInterval)
            {
                v.coefPhase = 0;
                updateCoefficients (v);
            }

            const float s = renderVoice (v);
            l[i] += s;
            r[i] += s;
            if (! v.active) break;
        }
    }
}

void InternalSynth::render (juce::AudioBuffer<float>& out, int startSample, int num, const juce::MidiBuffer& midi) noexcept
{
    if (num <= 0) return;
    num = juce::jmin (num, scratch.getNumSamples());
    scratch.clear (0, num);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int p = juce::jlimit (0, num, meta.samplePosition);
        if (p > pos) { renderSegment (scratch, pos, p - pos); pos = p; }
        handle (meta.getMessage());
    }
    if (pos < num)
        renderSegment (scratch, pos, num - pos);

    auto* l = scratch.getWritePointer (0);
    auto* r = scratch.getWritePointer (1);

    // chorus (two modulated taps, quadrature LFO)
    chorusMix += (chorusMixTarget - chorusMix) * 0.05f;
    if (chorusMix > 0.001f && ! chorusBuf[0].empty())
    {
        const int len = (int) chorusBuf[0].size();
        const int mask = len - 1;
        const double lfoInc = 0.35 / sampleRate;
        for (int i = 0; i < num; ++i)
        {
            chorusBuf[0][(size_t) chorusPos] = l[i];
            chorusBuf[1][(size_t) chorusPos] = r[i];

            chorusPhase += lfoInc;
            if (chorusPhase >= 1.0) chorusPhase -= 1.0;

            auto tap = [&] (const std::vector<float>& b, double ph)
            {
                const double delay = (0.012 + 0.004 * std::sin (kTwoPi * ph)) * sampleRate;
                const double rp = (double) chorusPos - delay;
                const int i0 = (int) std::floor (rp);
                const float f = (float) (rp - (double) i0);
                const float a = b[(size_t) (i0 & mask)], c = b[(size_t) ((i0 + 1) & mask)];
                return a + (c - a) * f;
            };

            const float wl = tap (chorusBuf[0], chorusPhase);
            const float wr = tap (chorusBuf[1], chorusPhase + 0.25);
            l[i] = l[i] * (1.0f - 0.35f * chorusMix) + wl * 0.75f * chorusMix;
            r[i] = r[i] * (1.0f - 0.35f * chorusMix) + wr * 0.75f * chorusMix;
            chorusPos = (chorusPos + 1) & mask;
        }
    }

    reverbMix += (reverbMixTarget - reverbMix) * 0.05f;
    if (reverbMix > 0.001f)
        reverb.processStereo (l, r, num);

    // soft clip + add to the output
    const int outCh = out.getNumChannels();
    for (int i = 0; i < num; ++i)
    {
        const float a = std::tanh (l[i]), b = std::tanh (r[i]);
        if (outCh >= 2)
        {
            out.addSample (0, startSample + i, a);
            out.addSample (1, startSample + i, b);
        }
        else if (outCh == 1)
        {
            out.addSample (0, startSample + i, 0.5f * (a + b));
        }
    }
}

} // namespace bd
