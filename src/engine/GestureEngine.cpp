#include "GestureEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bd
{

namespace
{
    constexpr int64_t kNever = std::numeric_limits<int64_t>::max();
    constexpr double  kTwoPi = 6.283185307179586;
    constexpr int     kVisEveryNTicks = 8;      // UI update every 8 * 32 = 256 samples
    constexpr float   kVibRampSeconds = 0.35f;  // vibrato fade-in after the delay
    constexpr float   kSmoothTauSeconds = 0.004f;
}

//==============================================================================
GestureEngine::GestureEngine()
{
    for (int ch = 0; ch < (int) voices.size(); ++ch)
        voices[(size_t) ch].channel = ch;
}

void GestureEngine::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    reset();
}

void GestureEngine::reset()
{
    for (auto& v : voices)
    {
        const int ch = v.channel;
        v = Voice{};
        v.channel = ch;
    }

    allocator.setNumMemberChannels (getLiveChannelCount());
    queueClear();
    clock = 0;
    nextTick = 0;
    configPending = true;
    releasePending = false;
}

void GestureEngine::setSettings (const EngineSettings& s)
{
    const bool layoutChanged = s.numVoices != settings.numVoices || s.bendRange != settings.bendRange
                            || s.liveChannels != settings.liveChannels || s.masterBendRange != settings.masterBendRange;

    settings = s;
    settings.numVoices = juce::jlimit (1, 15, settings.numVoices);
    scale.set (settings.scaleKey, settings.scaleLockOn ? settings.scaleType : (int) ScaleLock::chromatic);

    if (layoutChanged)
    {
        releasePending = true;
        configPending  = true;
    }
}

int GestureEngine::getLiveChannelCount() const noexcept
{
    const int n = juce::jlimit (1, 15, settings.numVoices);
    return settings.liveChannels > 0 ? juce::jmin (n, settings.liveChannels) : n;
}

int GestureEngine::getLatencySamples() const noexcept
{
    return settings.chordGlideOn ? (int) msToSamples (settings.chordWindowMs) : 0;
}

int GestureEngine::getNumActiveVoices() const noexcept
{
    int n = 0;
    for (int ch = 2; ch <= 16; ++ch)
        if (voices[(size_t) ch].active)
            ++n;
    return n;
}

const GestureEngine::Voice* GestureEngine::findVoiceForKey (int key) const noexcept
{
    for (int ch = 2; ch <= 16; ++ch)
        if (voices[(size_t) ch].active && voices[(size_t) ch].inputKey == key)
            return &voices[(size_t) ch];
    return nullptr;
}

//==============================================================================
// Event queue (sorted by time, note-offs before note-ons at equal times)

bool GestureEngine::queuePush (const QueuedEvent& e) noexcept
{
    if (queueSize >= queueCapacity)
        return false;

    auto before = [] (const QueuedEvent& a, const QueuedEvent& b)
    {
        return a.time < b.time || (a.time == b.time && a.isNoteOn < b.isNoteOn);
    };

    int i = queueSize++;
    queueAt (i) = e;

    while (i > 0 && before (queueAt (i), queueAt (i - 1)))
    {
        std::swap (queueAt (i), queueAt (i - 1));
        --i;
    }

    return true;
}

void GestureEngine::queuePopConsumed() noexcept
{
    while (queueSize > 0 && queueAt (0).consumed)
    {
        queueHead = (queueHead + 1) % queueCapacity;
        --queueSize;
    }
}

//==============================================================================
void GestureEngine::process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                             const TransportState& ts)
{
    transport = ts;
    const int64_t blockStart = clock;
    const int64_t blockEnd   = clock + numSamples;
    const int64_t latency    = getLatencySamples();

    if (releasePending.exchange (false))
    {
        releaseAll (out, 0);
        queueClear();
    }

    // Earliest sample in this block at which notes / controls may be sent (after the config).
    int64_t notBefore = blockStart;

    if (configPending.exchange (false))
    {
        allocator.setNumMemberChannels (getLiveChannelCount());
        const int span = MpeOutput::addZoneConfiguration (out, 0, juce::jmax (0, numSamples - 1),
                                                          settings.numVoices, settings.bendRange,
                                                          juce::jlimit (1, 96, settings.masterBendRange));
        notBefore = blockStart + juce::jmin (span, juce::jmax (0, numSamples - 1));
    }

    // 1. Queue incoming notes; pass everything else through on the master channel.
    for (const auto meta : in)
    {
        const auto& m = meta.getMessage();
        const int pos = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);

        if (m.isNoteOn())
        {
            queuePush ({ blockStart + pos + latency, 1, (uint8_t) m.getNoteNumber(), (uint8_t) juce::jmax (1, (int) m.getVelocity()), 0 });
        }
        else if (m.isNoteOff())
        {
            queuePush ({ blockStart + pos + latency, 0, (uint8_t) m.getNoteNumber(), 0, 0 });
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            releaseAll (out, pos);
            queueClear();
        }
        else if (m.isPitchWheel())
        {
            out.addEvent (juce::MidiMessage::pitchWheel (MpeOutput::masterChannel, m.getPitchWheelValue()), pos);
        }
        else if (m.isController())
        {
            out.addEvent (juce::MidiMessage::controllerEvent (MpeOutput::masterChannel, m.getControllerNumber(), m.getControllerValue()), pos);
        }
        else if (m.isChannelPressure())
        {
            out.addEvent (juce::MidiMessage::channelPressureChange (MpeOutput::masterChannel, m.getChannelPressureValue()), pos);
        }
        else if (m.isProgramChange())
        {
            out.addEvent (juce::MidiMessage::programChange (MpeOutput::masterChannel, m.getProgramChangeNumber()), pos);
        }
    }

    // 2. Walk through the block in time order: due releases, due input events, control ticks.
    if (nextTick < blockStart)
        nextTick = ((blockStart + controlInterval - 1) / controlInterval) * controlInterval;

    for (int guard = 0; guard < 100000; ++guard)
    {
        int64_t tRelease = kNever;
        for (int ch = 2; ch <= 16; ++ch)
        {
            const auto& v = voices[(size_t) ch];
            if (v.active && v.releaseAt >= 0)
                tRelease = std::min (tRelease, v.releaseAt);
        }

        queuePopConsumed();
        const int64_t tEvent = queueSize > 0 ? queueAt (0).time : kNever;
        const int64_t tTick  = nextTick;

        const int64_t t = std::min ({ tRelease, tEvent, tTick });
        if (t >= blockEnd)
            break;

        const int64_t tc = std::max (t, notBefore);    // late events / events during the config fire right after it

        if (tRelease == t)
        {
            for (int ch = 2; ch <= 16; ++ch)
            {
                auto& v = voices[(size_t) ch];
                if (v.active && v.releaseAt >= 0 && v.releaseAt <= t)
                    finishVoice (out, v, (int) (tc - blockStart));
            }
        }
        else if (tEvent == t)
        {
            handleDueEvent (out, tc, blockStart);
        }
        else
        {
            controlTick (out, tc, blockStart, false);
            nextTick += controlInterval;
        }
    }

    clock = blockEnd;
}

//==============================================================================
void GestureEngine::handleDueEvent (juce::MidiBuffer& out, int64_t t, int64_t blockStart)
{
    auto& e = queueAt (0);
    e.consumed = 1;

    if (! e.isNoteOn)
    {
        // Key up: find the voice currently mapped to this key.
        for (int ch = 2; ch <= 16; ++ch)
        {
            auto& v = voices[(size_t) ch];
            if (v.active && v.keyHeld && v.inputKey == e.note)
            {
                handleKeyUp (out, v, t, blockStart);
                break;
            }
        }
        return;
    }

    // Note-on: collect the group of note-ons that form one chord.
    // With CHORD GLIDE the window is chordWindowMs (covered by the reported latency);
    // otherwise only notes at exactly the same sample are grouped.
    const int64_t window = settings.chordGlideOn ? msToSamples (settings.chordWindowMs) : 0;

    int notes[16], keys[16], vels[16];
    int count = 0;

    for (int i = 0; i < queueSize && count < 16; ++i)
    {
        auto& q = queueAt (i);
        if (q.time > e.time + window) break;
        if (! q.isNoteOn || (q.consumed && &q != &e)) continue;

        q.consumed = 1;
        keys[count]  = q.note;
        vels[count]  = q.velocity;
        notes[count] = (settings.scaleLockOn && settings.quantizeInput) ? scale.snap (q.note) : q.note;
        ++count;
    }

    // Retriggered key that is still held (no note-off in between): release the old voice first.
    for (int i = 0; i < count; ++i)
        for (int ch = 2; ch <= 16; ++ch)
        {
            auto& v = voices[(size_t) ch];
            if (v.active && v.keyHeld && v.inputKey == keys[i])
                handleKeyUp (out, v, t, blockStart);
        }

    handleNoteOnGroup (out, t, blockStart, notes, keys, vels, count);
}

bool GestureEngine::isGlideCandidate (const Voice& v, int64_t t) const noexcept
{
    if (! v.active)
        return false;

    if (v.keyHeld)
        return true;

    // Released voice still inside the glide window (dive has not started yet)
    const int64_t window = msToSamples (settings.glideWindowMs);
    return v.releaseAt >= 0 && (t - v.keyUpTime) <= window
        && (! v.diving || t < v.ramp.start);
}

void GestureEngine::handleNoteOnGroup (juce::MidiBuffer& out, int64_t t, int64_t blockStart,
                                       const int* notes, const int* keys, const int* vels, int count)
{
    const float range = bendRangeF();

    // ---------------------------------------------------------------- CHORD GLIDE
    if (count >= 2 && settings.chordGlideOn)
    {
        int candidates[16];
        int numCand = 0;

        for (int ch = 2; ch <= allocator.getLastChannel(); ++ch)
            if (isGlideCandidate (voices[(size_t) ch], t))
                candidates[numCand++] = ch;

        bool noteDone[16] = {};
        bool candUsed[16] = {};

        // Greedy nearest-pitch matching: repeatedly take the globally closest (new note, voice) pair.
        for (;;)
        {
            int bestN = -1, bestC = -1;
            float bestDist = 1.0e9f;

            for (int n = 0; n < count; ++n)
            {
                if (noteDone[n]) continue;

                for (int c = 0; c < numCand; ++c)
                {
                    if (candUsed[c]) continue;
                    const auto& v = voices[(size_t) candidates[c]];
                    const float cur = (float) v.sentNote + v.ramp.valueAt (t);
                    const float dist = std::abs ((float) notes[n] - cur);
                    const bool reachable = std::abs (notes[n] - v.sentNote) <= (int) range;

                    if (reachable && dist < bestDist)
                    {
                        bestDist = dist; bestN = n; bestC = c;
                    }
                }
            }

            if (bestN < 0)
                break;

            noteDone[bestN] = true;
            candUsed[bestC] = true;
            glideVoice (voices[(size_t) candidates[bestC]], notes[bestN], keys[bestN], vels[bestN], t);
        }

        for (int n = 0; n < count; ++n)
            if (! noteDone[n])
                startVoice (out, notes[n], keys[n], vels[n], t, blockStart);

        return;
    }

    // ---------------------------------------------------------------- mono GLIDE
    if (count == 1 && settings.glideOn)
    {
        int heldOther = 0;
        Voice* heldVoice = nullptr;

        for (int ch = 2; ch <= 16; ++ch)
        {
            auto& v = voices[(size_t) ch];
            if (v.active && v.keyHeld) { ++heldOther; heldVoice = &v; }
        }

        Voice* target = nullptr;

        if (heldOther == 1)
        {
            // Overlapping legato: glide the voice of the one held key.
            target = heldVoice;
        }
        else if (heldOther == 0)
        {
            // Touching / small-gap legato: newest voice still inside its glide window.
            int64_t newest = -1;
            for (int ch = 2; ch <= 16; ++ch)
            {
                auto& v = voices[(size_t) ch];
                if (isGlideCandidate (v, t) && v.keyUpTime > newest)
                {
                    newest = v.keyUpTime;
                    target = &v;
                }
            }
        }

        if (target != nullptr
            && notes[0] != target->targetNote
            && std::abs (notes[0] - target->sentNote) <= (int) range)
        {
            glideVoice (*target, notes[0], keys[0], vels[0], t);
            return;
        }
    }

    // ---------------------------------------------------------------- plain new voices
    for (int n = 0; n < count; ++n)
        startVoice (out, notes[n], keys[n], vels[n], t, blockStart);
}

//==============================================================================
void GestureEngine::startVoice (juce::MidiBuffer& out, int note, int key, int vel, int64_t t, int64_t blockStart)
{
    const int pos = (int) (t - blockStart);
    const auto alloc = allocator.allocate (note, t);
    auto& v = voices[(size_t) alloc.channel];

    if (v.active)              // stolen voice: send its note-off first
    {
        finishVoice (out, v, pos);           // frees the slot in the allocator ...
        allocator.markHeld (alloc.channel);  // ... so take it back for the new note
    }

    const int ch = v.channel;
    v = Voice{};
    v.channel    = ch;
    v.active     = true;
    v.sentNote   = note;
    v.targetNote = note;
    v.inputKey   = key;
    v.velocity   = vel;
    v.keyHeld    = true;
    v.noteStart  = t;
    v.pitchStart = t;
    v.ramp       = { 0.0f, 0.0f, t, 0, Curve::linear };
    v.rng        = (voiceSeed = voiceSeed * 1664525u + 1013904223u) | 1u;
    v.shakeNext  = nextRandom (v.rng);

    // MPE: per-note controls go out BEFORE the note-on so the synth starts at the right values.
    emitVoiceControls (out, v, t, pos, true, false);
    out.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) juce::jlimit (1, 127, vel)), pos);
    pushVis (v, VisualEvent::noteOn, t);
}

void GestureEngine::glideVoice (Voice& v, int newNote, int key, int vel, int64_t t)
{
    // Start from wherever the voice currently is (mid-glide or mid-dive included).
    const float current = v.ramp.valueAt (t);

    v.ramp       = { current, (float) (newNote - v.sentNote), t,
                     msToSamples (settings.glideTimeMs), settings.glideCurve };
    v.targetNote = newNote;
    v.inputKey   = key;
    v.velocity   = juce::jmax (v.velocity, vel);
    v.keyHeld    = true;
    v.diving     = false;
    v.releaseAt  = -1;
    v.pitchStart = t;
    allocator.markHeld (v.channel);
    pushVis (v, VisualEvent::glide, t);
}

void GestureEngine::handleKeyUp (juce::MidiBuffer& out, Voice& v, int64_t t, int64_t blockStart)
{
    v.keyHeld   = false;
    v.keyUpTime = t;
    allocator.markReleasing (v.channel);
    pushVis (v, VisualEvent::keyUp, t);

    const int64_t glideWindow = (settings.glideOn || settings.chordGlideOn) ? msToSamples (settings.glideWindowMs) : 0;
    const bool doDive = settings.diveOn || settings.riseOn;

    if (doDive)
    {
        const float dir   = settings.diveOn ? -1.0f : 1.0f;
        const int64_t len = settings.diveSync ? (int64_t) std::llround (beatsToSamples (syncDivisionBeats (settings.diveDiv)))
                                              : msToSamples (settings.diveTimeMs);
        const int64_t start = t + glideWindow;

        float targetAbs = (float) v.targetNote + dir * settings.diveSemis;
        if (settings.scaleLockOn)
            targetAbs = scale.snapOffsetTarget (v.targetNote, dir * settings.diveSemis);

        const float range = bendRangeF();
        const float target = juce::jlimit (-range, range, targetAbs - (float) v.sentNote);

        // The dive ramp starts where the (possibly still running) glide ramp will be at `start`.
        const float from = v.ramp.valueAt (start);
        v.ramp      = { from, target, start, std::max<int64_t> (1, len), settings.diveCurve };
        v.diving    = true;
        v.releaseAt = start + std::max<int64_t> (1, len);
    }
    else
    {
        v.releaseAt = t + glideWindow;
    }

    if (v.releaseAt <= t)
        finishVoice (out, v, (int) (t - blockStart));
}

void GestureEngine::finishVoice (juce::MidiBuffer& out, Voice& v, int pos)
{
    if (! v.active)
        return;

    // Note: the channel's pitch bend is NOT recentred here - the synth's release tail keeps the
    // bent pitch (that's what makes an 808 dive tail sound right). It is reset before the next
    // note-on on this channel instead.
    // A finished dive/rise lands exactly on its target (the smoother lags by a few ms).
    if (v.diving)
    {
        v.smoothedBend = v.ramp.to;
        const int bend = MpeOutput::semitonesToPitchWheel (v.ramp.to, bendRangeF());
        if (bend != v.lastBend)
        {
            out.addEvent (juce::MidiMessage::pitchWheel (v.channel, bend), pos);
            v.lastBend = bend;
        }
    }

    out.addEvent (juce::MidiMessage::noteOff (v.channel, v.sentNote, (juce::uint8) 0), pos);
    allocator.free (v.channel);
    v.active    = false;
    v.keyHeld   = false;
    v.diving    = false;
    v.releaseAt = -1;
    pushVis (v, VisualEvent::noteOff, clock + pos);
}

void GestureEngine::releaseAll (juce::MidiBuffer& out, int pos)
{
    for (int ch = 2; ch <= 16; ++ch)
    {
        auto& v = voices[(size_t) ch];
        if (v.active)
            finishVoice (out, v, pos);
    }

    // Belt and braces: recentre and silence every member channel, even ones we think are idle.
    for (int ch = 2; ch <= 16; ++ch)
    {
        out.addEvent (juce::MidiMessage::allNotesOff (ch), pos);
        out.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), pos);
        out.addEvent (juce::MidiMessage::channelPressureChange (ch, 0), pos);
        auto& v = voices[(size_t) ch];
        v.lastBend = 8192; v.lastPressure = 0; v.smoothedBend = 0.0f;
    }

    allocator.freeAll();

    if (sink != nullptr)
    {
        VisualEvent e;
        e.type = VisualEvent::allOff;
        e.time = clock + pos;
        sink->pushVisualEvent (e);
    }
}

//==============================================================================
// Control rate

void GestureEngine::controlTick (juce::MidiBuffer& out, int64_t t, int64_t blockStart, bool forceSend)
{
    const int pos = (int) (t - blockStart);

    for (int ch = 2; ch <= 16; ++ch)
    {
        auto& v = voices[(size_t) ch];
        if (v.active)
            emitVoiceControls (out, v, t, pos, forceSend, true);
    }
}

float GestureEngine::nextRandom (uint32_t& s) noexcept
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return (float) (s & 0xffffff) / (float) 0x7fffff - 1.0f;
}

float GestureEngine::modulationOffset (Voice& v, int64_t t, double dt) noexcept
{
    float offset = 0.0f;
    const bool inDive = v.diving && t >= v.ramp.start;
    const double heldSec = (double) (t - v.pitchStart) / sampleRate;

    // VIBRATO: after the delay, depth ramps in over kVibRampSeconds.
    if (settings.vibratoOn && ! inDive)
    {
        const double rate = settings.vibSync ? bpm() / 60.0 / syncDivisionBeats (settings.vibDiv)
                                             : (double) settings.vibRateHz;
        v.vibPhase = std::fmod (v.vibPhase + kTwoPi * rate * dt, kTwoPi);

        const double delay = settings.vibDelayMs * 0.001;
        if (heldSec > delay)
        {
            const float amt = juce::jlimit (0.0f, 1.0f, (float) ((heldSec - delay) / kVibRampSeconds));
            offset += (float) std::sin (v.vibPhase) * settings.vibDepthCents * 0.01f * amt;
        }
    }
    else
    {
        v.vibPhase = 0.0;
    }

    // WOBBLE: slow, wide, tempo-synced sine. Starts at phase 0 so the attack is on pitch.
    if (settings.wobbleOn)
    {
        const double rate = bpm() / 60.0 / syncDivisionBeats (settings.wobbleDiv);
        v.wobblePhase = std::fmod (v.wobblePhase + kTwoPi * rate * dt, kTwoPi);
        offset += (float) std::sin (v.wobblePhase) * settings.wobbleSemis;
    }

    // SHAKE: fast jitter - linear interpolation between random targets.
    if (settings.shakeOn)
    {
        v.shakePhase += settings.shakeRateHz * dt;
        while (v.shakePhase >= 1.0)
        {
            v.shakePhase -= 1.0;
            v.shakePrev = v.shakeNext;
            v.shakeNext = nextRandom (v.rng);
        }
        const float s = v.shakePrev + (v.shakeNext - v.shakePrev) * (float) v.shakePhase;
        offset += s * settings.shakeCents * 0.01f;
    }

    // TRILL: alternate between the note and the next scale degree up (whole tone if no scale lock).
    if (settings.trillOn && ! inDive)
    {
        const double periodSec = syncDivisionBeats (settings.trillDiv) * 60.0 / bpm();
        const auto step = (int64_t) std::floor (heldSec / periodSec);

        if ((step & 1) == 1)
        {
            const int upper = settings.scaleLockOn ? scale.stepDegrees (v.targetNote, 1) : v.targetNote + 2;
            offset += (float) (upper - v.targetNote);
        }
    }

    return offset;
}

float GestureEngine::pressureValue (const Voice& v, int64_t t) const noexcept
{
    if (! settings.pressureOn)
        return 0.0f;

    const float sec = (float) ((double) (t - v.noteStart) / sampleRate);
    const float env = evalEnvShape (settings.pressureShape, sec, settings.pressureTimeMs * 0.001f);
    const float vel = (float) v.velocity / 127.0f;
    const float velScale = 1.0f + (vel - 1.0f) * settings.velToPressure;
    return juce::jlimit (0.0f, 1.0f, settings.pressureAmount * env * velScale);
}

float GestureEngine::timbreValue (const Voice& v, int64_t t) const noexcept
{
    if (! settings.timbreOn)
        return 0.5f;   // CC74 neutral = 64

    const float sec = (float) ((double) (t - v.noteStart) / sampleRate);
    const float env = evalEnvShape (settings.timbreShape, sec, settings.timbreTimeMs * 0.001f);
    const float vel = (float) v.velocity / 127.0f;
    const float velScale = 1.0f + (vel - 1.0f) * settings.velToTimbre;
    return juce::jlimit (0.0f, 1.0f, settings.timbreAmount * env * velScale);
}

void GestureEngine::emitVoiceControls (juce::MidiBuffer& out, Voice& v, int64_t t, int pos, bool force, bool smooth)
{
    const double dt = smooth ? (double) controlInterval / sampleRate : 0.0;
    const float range = bendRangeF();

    float target = v.ramp.valueAt (t) + modulationOffset (v, t, dt);
    target = juce::jlimit (-range, range, target);

    if (smooth)
    {
        const float a = 1.0f - std::exp (-(float) dt / kSmoothTauSeconds);
        v.smoothedBend += (target - v.smoothedBend) * a;
        if (std::abs (target - v.smoothedBend) < 0.0005f)
            v.smoothedBend = target;
    }
    else
    {
        v.smoothedBend = target;
    }

    const int bend     = MpeOutput::semitonesToPitchWheel (v.smoothedBend, range);
    const float pres   = pressureValue (v, t);
    const float timb   = timbreValue (v, t);
    const int pressure = juce::jlimit (0, 127, (int) std::lround (pres * 127.0f));
    const int timbre   = juce::jlimit (0, 127, (int) std::lround (timb * 127.0f));

    if (force || bend != v.lastBend)
    {
        out.addEvent (juce::MidiMessage::pitchWheel (v.channel, bend), pos);
        v.lastBend = bend;
    }

    if (force || pressure != v.lastPressure)
    {
        out.addEvent (juce::MidiMessage::channelPressureChange (v.channel, pressure), pos);
        v.lastPressure = pressure;
    }

    if (force || timbre != v.lastTimbre)
    {
        out.addEvent (juce::MidiMessage::controllerEvent (v.channel, 74, timbre), pos);
        v.lastTimbre = timbre;
    }

    if (smooth && ++v.visCounter >= kVisEveryNTicks)
    {
        v.visCounter = 0;
        pushVis (v, VisualEvent::update, t);
    }
}

void GestureEngine::pushVis (const Voice& v, VisualEvent::Type type, int64_t t) noexcept
{
    if (sink == nullptr)
        return;

    VisualEvent e;
    e.type     = (uint8_t) type;
    e.channel  = (uint8_t) v.channel;
    e.note     = (uint8_t) v.sentNote;
    e.velocity = (uint8_t) v.velocity;
    e.pitch    = (float) v.sentNote + (type == VisualEvent::glide ? (float) (v.targetNote - v.sentNote) : v.smoothedBend);
    e.pressure = (float) v.lastPressure / 127.0f;
    e.timbre   = (float) v.lastTimbre / 127.0f;
    e.time     = t;
    sink->pushVisualEvent (e);
}

} // namespace bd
