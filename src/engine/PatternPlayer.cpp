#include "PatternPlayer.h"
#include <cmath>

namespace bd
{

namespace
{
    int toMidi7 (float v01) noexcept   { return juce::jlimit (0, 127, (int) std::lround (v01 * 127.0f)); }
    int toVelocity (float v01) noexcept { return juce::jlimit (1, 127, (int) std::lround (v01 * 127.0f)); }
    int toNote (float pitch) noexcept   { return juce::jlimit (0, 127, (int) std::lround (pitch)); }
}

PatternPlayer::PatternPlayer()
{
    allocator.setChannelRange (config.firstChannel, config.lastChannel);
}

void PatternPlayer::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    for (auto& v : voices) v = Voice{};
    allocator.setChannelRange (config.firstChannel, config.lastChannel);
    wasPlaying = false;
    resyncPending = true;
    monoOwner = monoNote = -1;
}

void PatternPlayer::setConfig (const Config& c) noexcept
{
    if (c == config)
        return;

    // Called on the audio thread right before process(): voices that are still sounding must be
    // released on the channels they were started on, so the caller's next process() handles it.
    pendingConfig = c;
    configChangePending = true;
}

void PatternPlayer::setPattern (const Pattern* p) noexcept
{
    pattern = p;
    const double L = lengthBeats();

    if (! juce::exactlyEqual (L, patternLength))
    {
        patternLength = L;
        resyncPending = true;     // cycle arithmetic changed: start over
    }

    for (auto& v : voices)
    {
        if (! v.active) continue;

        const auto* line = pattern != nullptr ? pattern->find (v.lineId) : nullptr;
        if (line == nullptr || std::abs ((v.base + line->start) - v.absStart) > 1.0e-9 || line->start >= L)
        {
            v.kill = true;
            v.line = nullptr;
            continue;
        }

        v.line   = line;
        v.absEnd = v.base + std::min (line->end(), L);
    }
}

int PatternPlayer::getNumActive() const noexcept
{
    int n = 0;
    for (auto& v : voices) if (v.active) ++n;
    return n;
}

//==============================================================================
void PatternPlayer::process (juce::MidiBuffer& out, int numSamples, const Clock& clk, int notBefore)
{
    if (numSamples <= 0)
        return;

    const int nb = juce::jlimit (0, numSamples - 1, notBefore);

    if (configChangePending)
    {
        configChangePending = false;
        if (getNumActive() > 0 || wasPlaying)
            releaseAll (out, nb);
        config = pendingConfig;
        allocator.setChannelRange (config.firstChannel, config.lastChannel);
        resyncPending = true;
    }

    if (! clk.playing)
    {
        if (getNumActive() > 0)
            releaseAll (out, nb);
        wasPlaying = false;
        resyncPending = true;
        return;
    }

    const double bpm = clk.bpm > 1.0 ? clk.bpm : 120.0;
    const double bps = bpm / 60.0 / sampleRate;          // beats per sample
    const double L = lengthBeats();

    // --- discontinuity detection (seek, host loop, transport start) -----------------------------
    bool doChase = ! wasPlaying || resyncPending;
    if (wasPlaying)
    {
        const double diff = clk.ppq - lastEndPpq;
        if (diff < -(2.0 * bps + 1.0e-9) || diff > 1.0 / 64.0)
            doChase = true;
    }
    resyncPending = false;

    if (doChase && getNumActive() > 0)
        releaseAll (out, nb);

    for (auto& v : voices)
        if (v.active && v.kill)
            endVoice (out, v, nb, clk.ppq);

    // Continuous playback: also catch starts in a tiny gap between blocks (host rounding).
    const double windowStart = doChase ? clk.ppq : std::min (lastEndPpq, clk.ppq);

    if (doChase && pattern != nullptr && L > 0.0)
        chase (out, clk.ppq, nb);

    wasPlaying = true;

    auto posOf = [&] (double t, int lo) noexcept
    {
        const int p = (int) std::floor ((t - clk.ppq) / bps + 0.5);
        return juce::jlimit (lo, numSamples - 1, p);
    };

    for (int s = 0; s < numSamples; s += controlInterval)
    {
        const int n  = juce::jmin (controlInterval, numSamples - s);
        const int lo = juce::jmax (s, nb);
        const double a = s == 0 ? windowStart : clk.ppq + (double) s * bps;
        const double b = clk.ppq + (double) (s + n) * bps;

        // 1. ends
        for (auto& v : voices)
            if (v.active && v.absEnd < b)
                endVoice (out, v, posOf (v.absEnd, lo), v.absEnd);

        // 2. starts (every pattern cycle the chunk touches)
        if (pattern != nullptr && L > 0.0)
        {
            const auto k0 = (int64_t) std::floor (a / L);
            const auto k1 = (int64_t) std::floor (b / L);

            for (int64_t k = k0; k <= k1; ++k)
            {
                const double base = (double) k * L;
                for (auto& line : pattern->lines)
                {
                    if (line.start >= L) continue;
                    const double st = base + line.start;
                    if (st >= a && st < b)
                        startLine (out, line, base, st, st, posOf (st, lo));
                }
            }
        }

        // 3. control tick
        const double tickPpq = clk.ppq + (double) s * bps;
        if (config.mono)
        {
            updateMono (out, tickPpq, lo);
        }
        else
        {
            for (auto& v : voices)
                if (v.active && v.absStart <= tickPpq)
                    emitControls (out, v, tickPpq, lo, false);
        }
    }

    lastEndPpq = clk.ppq + (double) numSamples * bps;
}

void PatternPlayer::chase (juce::MidiBuffer& out, double ppq, int pos)
{
    const double L = lengthBeats();
    const double base = std::floor (ppq / L) * L;
    constexpr double minRemaining = 1.0 / 64.0;

    for (auto& line : pattern->lines)
    {
        if (line.start >= L) continue;
        const double st = base + line.start;
        const double en = base + std::min (line.end(), L);
        if (st < ppq && ppq < en - minRemaining)
            startLine (out, line, base, st, ppq, pos);
    }
}

//==============================================================================
void PatternPlayer::startLine (juce::MidiBuffer& out, const PatternLine& line, double base,
                               double absStartPpq, double nowPpq, int pos)
{
    // Already sounding (e.g. chased in this block)?
    for (auto& v : voices)
        if (v.active && v.lineId == line.id && std::abs (v.absStart - absStartPpq) < 1.0e-9)
            return;

    // free virtual voice, otherwise steal the oldest
    Voice* slot = nullptr;
    for (auto& v : voices)
        if (! v.active) { slot = &v; break; }

    if (slot == nullptr)
    {
        for (auto& v : voices)
            if (slot == nullptr || v.stamp < slot->stamp)
                slot = &v;
        endVoice (out, *slot, pos, nowPpq);
    }

    const double L = lengthBeats();
    const float startPitch = line.pitchAt (nowPpq - absStartPpq);

    Voice nv;
    nv.active   = true;
    nv.lineId   = line.id;
    nv.line     = &line;
    nv.base     = base;
    nv.absStart = absStartPpq;
    nv.absEnd   = base + std::min (line.end(), L);
    nv.sentNote = toNote (startPitch);
    nv.velocity = toVelocity (line.velocity);
    nv.stamp    = ++stampCounter;

    if (config.mono)
    {
        *slot = nv;
        updateMono (out, nowPpq, pos);
        return;
    }

    const auto alloc = allocator.allocate (nv.sentNote, nv.stamp);
    if (alloc.stolen)
    {
        for (auto& v : voices)
            if (v.active && v.channel == alloc.channel && &v != slot)
            {
                endVoice (out, v, pos, nowPpq);   // frees the allocator slot ...
                allocator.markHeld (alloc.channel); // ... take it back for the new line
                break;
            }
    }

    nv.channel = alloc.channel;
    *slot = nv;

    // MPE: per-note controls go out BEFORE the note-on so the synth starts at the right values.
    emitControls (out, *slot, nowPpq, pos, true);
    out.addEvent (juce::MidiMessage::noteOn (slot->channel, slot->sentNote, (juce::uint8) slot->velocity), pos);
}

void PatternPlayer::endVoice (juce::MidiBuffer& out, Voice& v, int pos, double nowPpq)
{
    if (! v.active)
        return;

    v.active = false;
    v.kill = false;

    if (config.mono)
    {
        const int idx = (int) (&v - voices.data());
        if (idx == monoOwner)
        {
            monoOwner = -1;
            // hand over to the next newest line (last-note priority) or stop
            const int next = newestVoice();
            if (next < 0)
                monoNoteOff (out, pos);
            else
                updateMono (out, nowPpq, pos);
        }
        return;
    }

    // The channel's bend is NOT recentred: the release tail keeps its pitch. The next note-on on
    // this channel sends its own bend first.
    out.addEvent (juce::MidiMessage::noteOff (v.channel, v.sentNote, (juce::uint8) 0), pos);
    allocator.free (v.channel);
}

void PatternPlayer::emitControls (juce::MidiBuffer& out, Voice& v, double nowPpq, int pos, bool force)
{
    if (v.line == nullptr)
        return;

    const double tRel = nowPpq - v.absStart;
    const float range = (float) config.bendRange;
    const float bendSemis = juce::jlimit (-range, range, v.line->pitchAt (tRel) - (float) v.sentNote);

    const int bend = MpeOutput::semitonesToPitchWheel (bendSemis, range);
    const int pres = toMidi7 (v.line->pressureAt (tRel));
    const int timb = toMidi7 (v.line->timbreAt (tRel));

    if (force || bend != v.lastBend)     { out.addEvent (juce::MidiMessage::pitchWheel (v.channel, bend), pos); v.lastBend = bend; }
    if (force || pres != v.lastPressure) { out.addEvent (juce::MidiMessage::channelPressureChange (v.channel, pres), pos); v.lastPressure = pres; }
    if (force || timb != v.lastTimbre)   { out.addEvent (juce::MidiMessage::controllerEvent (v.channel, 74, timb), pos); v.lastTimbre = timb; }
}

//==============================================================================
int PatternPlayer::newestVoice() const noexcept
{
    int best = -1;
    for (int i = 0; i < maxVoices; ++i)
    {
        const auto& v = voices[(size_t) i];
        if (! v.active || v.kill || v.line == nullptr) continue;
        if (best < 0 || v.absStart > voices[(size_t) best].absStart
            || (juce::exactlyEqual (v.absStart, voices[(size_t) best].absStart) && v.stamp > voices[(size_t) best].stamp))
            best = i;
    }
    return best;
}

void PatternPlayer::monoNoteOff (juce::MidiBuffer& out, int pos)
{
    if (monoNote >= 0)
        out.addEvent (juce::MidiMessage::noteOff (1, monoNote, (juce::uint8) 0), pos);
    monoNote = -1;
}

void PatternPlayer::updateMono (juce::MidiBuffer& out, double nowPpq, int pos)
{
    const int newest = newestVoice();
    const float range = (float) config.bendRange;

    auto sendControls = [&] (const Voice& v, double tRel, bool force)
    {
        const float semis = juce::jlimit (-range, range, v.line->pitchAt (tRel) - (float) monoNote);
        const int bend = MpeOutput::semitonesToPitchWheel (semis, range);
        const int pres = toMidi7 (v.line->pressureAt (tRel));
        const int timb = toMidi7 (v.line->timbreAt (tRel));
        if (force || bend != monoLastBend)     { out.addEvent (juce::MidiMessage::pitchWheel (1, bend), pos); monoLastBend = bend; }
        if (force || pres != monoLastPressure) { out.addEvent (juce::MidiMessage::channelPressureChange (1, pres), pos); monoLastPressure = pres; }
        if (force || timb != monoLastTimbre)   { out.addEvent (juce::MidiMessage::controllerEvent (1, 74, timb), pos); monoLastTimbre = timb; }
    };

    // (re)trigger: a new owner, or the curve left the bend range
    auto trigger = [&] (const Voice& v, double tRel)
    {
        const int note = toNote (v.line->pitchAt (tRel));
        const int oldNote = monoNote;

        if (oldNote == note)
            monoNoteOff (out, pos);   // same key: off first, or the off would cancel the new note

        monoNote = note;
        sendControls (v, tRel, true);
        out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) v.velocity), pos);

        if (oldNote >= 0 && oldNote != note)   // after the new note-on: legato for mono synths
            out.addEvent (juce::MidiMessage::noteOff (1, oldNote, (juce::uint8) 0), pos);
    };

    if (newest != monoOwner)
    {
        monoOwner = newest;
        if (newest < 0)
        {
            monoNoteOff (out, pos);
            return;
        }

        const auto& v = voices[(size_t) newest];
        trigger (v, std::max (0.0, nowPpq - v.absStart));
        return;
    }

    if (monoOwner < 0)
        return;

    const auto& v = voices[(size_t) monoOwner];
    const double tRel = std::max (0.0, nowPpq - v.absStart);
    const float pitch = v.line->pitchAt (tRel);

    if (std::abs (pitch - (float) monoNote) > range - 0.01f)
        trigger (v, tRel);
    else
        sendControls (v, tRel, false);
}

//==============================================================================
void PatternPlayer::releaseAll (juce::MidiBuffer& out, int pos)
{
    if (config.mono)
    {
        monoNoteOff (out, pos);
        out.addEvent (juce::MidiMessage::pitchWheel (1, 8192), pos);
        out.addEvent (juce::MidiMessage::channelPressureChange (1, 0), pos);
        monoOwner = -1;
        monoLastBend = monoLastPressure = monoLastTimbre = -1;
        for (auto& v : voices) v.active = v.kill = false;
        return;
    }

    for (auto& v : voices)
        if (v.active)
        {
            out.addEvent (juce::MidiMessage::noteOff (v.channel, v.sentNote, (juce::uint8) 0), pos);
            v.active = v.kill = false;
        }

    for (int ch = allocator.getFirstChannel(); ch <= allocator.getLastChannel(); ++ch)
    {
        out.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), pos);
        out.addEvent (juce::MidiMessage::channelPressureChange (ch, 0), pos);
    }

    allocator.freeAll();
}

} // namespace bd
