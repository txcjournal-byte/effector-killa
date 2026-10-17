/*
    TrapWin BEND DEMON
    PatternPlayer.h - plays a Pattern (the 2.0 line sequencer) as MPE or mono MIDI, locked to a
    musical clock (host ppq or the internal preview clock).

    * Every sounding line gets its own member channel (VoiceAllocator, channel range configurable
      so the live gesture engine can keep a few channels of the same MPE zone).
    * Note-on at the nearest semitone of the line's pitch at the moment it starts, then 14-bit
      pitch bend relative to that note, channel pressure and CC74, recomputed every
      `controlInterval` samples (default 32) - not just once per audio block.
    * Loops the pattern while the clock runs. Each block is split at control ticks; note starts
      and ends are placed sample-accurately inside the block, across loop boundaries too.
    * Clock jumps (seek, host loop, FL Studio pattern restart) and stops send note-offs; after a
      jump the player "chases": lines that are already under the new position start right away.
    * Mono/Legacy mode: everything on channel 1, last-note priority, the note is retriggered when
      the curve leaves the bend range (for non-MPE synths such as Serum 1).

    Audio-thread only, allocation-free, single owner.
*/
#pragma once

#include "Pattern.h"
#include "VoiceAllocator.h"
#include "MpeOutput.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace bd
{

class PatternPlayer
{
public:
    static constexpr int maxVoices = 32;      // virtual voices (lines sounding at once)

    struct Config
    {
        bool mono         = false;
        int  firstChannel = 2;                 // MPE member channel range used by the sequencer
        int  lastChannel  = 16;
        int  bendRange    = 48;                // semitones, member channels (or channel 1 in mono mode)

        bool operator== (const Config& o) const noexcept
        {
            return mono == o.mono && firstChannel == o.firstChannel && lastChannel == o.lastChannel && bendRange == o.bendRange;
        }
        bool operator!= (const Config& o) const noexcept { return ! operator== (o); }
    };

    struct Clock
    {
        bool   playing = false;
        double ppq     = 0.0;      // musical position (quarter notes) at the first sample of the block
        double bpm     = 120.0;
    };

    PatternPlayer();

    void prepare (double sampleRate);
    void setControlInterval (int samples) noexcept     { controlInterval = juce::jmax (1, samples); }

    /** Changing the configuration releases everything and re-chases on the next block. */
    void setConfig (const Config&) noexcept;
    const Config& getConfig() const noexcept           { return config; }

    /** Switch to another (immutable) pattern. Sounding lines that still exist keep playing with
        their new curves; deleted or moved lines are stopped on the next block. */
    void setPattern (const Pattern*) noexcept;

    /** Next process() releases everything and chases (call after someone else sent an
        all-notes-off on our channels, e.g. the gesture engine's release-all). */
    void requestResync() noexcept                      { resyncPending = true; }

    /** Appends MIDI for one block to `out` (not cleared). `notBefore` = first sample that may carry
        notes / controls (after an MPE configuration message sent in the same block). */
    void process (juce::MidiBuffer& out, int numSamples, const Clock&, int notBefore = 0);

    /** Note-off for everything, recentre bend / pressure on the used channels. */
    void releaseAll (juce::MidiBuffer& out, int pos);

    int  getNumActive() const noexcept;
    bool isRunning() const noexcept                    { return wasPlaying; }

private:
    struct Voice
    {
        bool     active   = false;
        bool     kill     = false;     // stop at the next block (line deleted / moved)
        uint32_t lineId   = 0;
        const PatternLine* line = nullptr;
        double   base     = 0.0;       // ppq of the pattern cycle this voice belongs to
        double   absStart = 0.0, absEnd = 0.0;
        int      channel  = 0;
        int      sentNote = 60;
        int      velocity = 100;
        int      lastBend = -1, lastPressure = -1, lastTimbre = -1;
        int64_t  stamp    = 0;
    };

    void startLine (juce::MidiBuffer&, const PatternLine&, double base, double absStartPpq,
                    double nowPpq, int pos);
    void endVoice (juce::MidiBuffer&, Voice&, int pos, double nowPpq);
    void emitControls (juce::MidiBuffer&, Voice&, double nowPpq, int pos, bool force);

    // mono mode
    void updateMono (juce::MidiBuffer&, double nowPpq, int pos);
    void monoNoteOff (juce::MidiBuffer&, int pos);
    int  newestVoice() const noexcept;

    void chase (juce::MidiBuffer&, double ppq, int pos);
    double lengthBeats() const noexcept { return pattern != nullptr ? pattern->lengthBeats() : 0.0; }

    Config config, pendingConfig;
    bool   configChangePending = false;
    const Pattern* pattern = nullptr;
    double patternLength = 0.0;
    VoiceAllocator allocator;
    std::array<Voice, maxVoices> voices {};

    double sampleRate = 44100.0;
    int    controlInterval = 32;
    bool   wasPlaying = false;
    bool   resyncPending = true;
    double lastEndPpq = 0.0;
    int64_t stampCounter = 0;

    // mono output state (channel 1)
    int monoOwner = -1;            // index into voices
    int monoNote  = -1;
    int monoLastBend = -1, monoLastPressure = -1, monoLastTimbre = -1;
};

} // namespace bd
