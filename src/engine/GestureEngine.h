/*
    TrapWin BEND DEMON
    GestureEngine.h - turns plain MIDI into MPE with automatic per-note gestures.

    Signal flow inside one processBlock:

        plain MIDI in --> event queue (optional lookahead for chord detection)
                      --> note-on / note-off handling (allocation, glide matching, dives)
                      --> control-rate ticks (every `controlInterval` samples) that compute
                          per-voice pitch offset, pressure and timbre and emit only changed values
                      --> MPE MIDI out (sample-accurate positions inside the block)

    Design decisions (also documented in README):

    * GLIDE note-off delay. When GLIDE (or CHORD GLIDE) is on, a key-up does not immediately send
      a note-off: the voice is kept alive for "glide window" ms. If a new note starts inside that
      window (or while the old key is still held = overlapping legato) the old voice is *re-used*:
      its pitch bends from where it currently is to the new note over "glide time" and no new
      note-on is sent. If nothing arrives, the real note-off is sent when the window expires.
      Cost: release tails start `glideWindow` ms late (default 60 ms). This is what makes legato
      work with a piano roll, where note ends and starts usually touch exactly.

    * The voice keeps its ORIGINAL note number; every subsequent pitch is expressed as per-note
      pitch bend relative to it. If a glide would exceed the bend range, the engine falls back to a
      normal retrigger.

    * Mono vs poly: a single new note only glides when at most one key is currently held (a
      monophonic line). With 2+ keys held, single notes start new voices (polyphony preserved).

    * CHORD GLIDE needs to see the whole chord before deciding, so when it is enabled the engine
      delays all input by "chord window" ms (default 12 ms) and reports that as latency, which the
      host compensates (FL Studio does PDC for plugin latency).

    * 808 DIVE / RISE: on key-up the voice stays alive for dive time and bends by N semitones
      (snapped to the scale when SCALE LOCK is on), then the real note-off is sent. If DIVE and
      RISE are both enabled, DIVE wins.

    Thread-safety: single-threaded (audio thread). No allocation after prepare().
*/
#pragma once

#include "EngineTypes.h"
#include "VoiceAllocator.h"
#include "ScaleLock.h"
#include "MpeOutput.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace bd
{

//==============================================================================
/** Lightweight event pushed to the UI (via a lock-free FIFO owned by the processor). */
struct VisualEvent
{
    enum Type : uint8_t { noteOn = 0, update, keyUp, noteOff, glide, allOff };

    uint8_t type     = update;
    uint8_t channel  = 0;      // MPE member channel 2..16 (= voice id)
    uint8_t note     = 0;      // note number the voice was started with
    uint8_t velocity = 0;
    float   pitch    = 0.0f;   // absolute pitch in semitones incl. bend (e.g. 60.37)
    float   pressure = 0.0f;   // 0..1
    float   timbre   = 0.0f;   // 0..1
    int64_t time     = 0;      // engine sample clock
};

/** Receives visual events on the audio thread. Implementations must be lock-free. */
struct VisualSink
{
    virtual ~VisualSink() = default;
    virtual void pushVisualEvent (const VisualEvent&) noexcept = 0;
};

//==============================================================================
class GestureEngine
{
public:
    static constexpr int controlInterval = 32;   // samples between control-rate ticks
    static constexpr int queueCapacity   = 4096;

    struct Ramp
    {
        float   from = 0.0f, to = 0.0f;
        int64_t start = 0, length = 0;
        Curve   curve = Curve::linear;

        float valueAt (int64_t t) const noexcept
        {
            if (length <= 0 || t >= start + length) return to;
            if (t <= start) return from;
            return from + (to - from) * applyCurve (curve, (float) (t - start) / (float) length);
        }
    };

    struct Voice
    {
        bool    active     = false;
        int     channel    = 0;
        int     sentNote   = 0;      // note number of the note-on actually sent
        int     targetNote = 0;      // note the voice is currently gliding / sitting on
        int     inputKey   = -1;     // incoming key currently mapped to this voice
        int     velocity   = 0;
        bool    keyHeld    = false;
        bool    diving     = false;
        int64_t noteStart  = 0;      // when the note-on was sent
        int64_t pitchStart = 0;      // last time the target pitch changed (vibrato/trill timing)
        int64_t keyUpTime  = 0;
        int64_t releaseAt  = -1;     // when the delayed note-off is due (-1 = none)
        Ramp    ramp;                // glide / dive pitch ramp (semitones relative to sentNote)

        float   smoothedBend = 0.0f; // semitones, after smoothing
        int     lastBend     = 8192;
        int     lastPressure = 0;
        int     lastTimbre   = 64;

        double  vibPhase = 0.0, wobblePhase = 0.0, shakePhase = 0.0;
        float   shakePrev = 0.0f, shakeNext = 0.0f;
        uint32_t rng = 1;
        int     visCounter = 0;
    };

    GestureEngine();

    void prepare (double sampleRate);
    void reset();                                   // silent reset (no MIDI output)

    /** Snapshot of all parameters. Changing voice count or bend range schedules a release-all
        and a fresh MPE zone configuration at the start of the next block. */
    void setSettings (const EngineSettings&);
    const EngineSettings& getSettings() const noexcept { return settings; }

    void setVisualSink (VisualSink* s) noexcept     { sink = s; }

    /** Send the MPE configuration (RPN 6 + RPN 0 ranges) at the start of the next block. */
    void requestZoneConfiguration() noexcept        { configPending = true; }

    /** Release every voice at the start of the next block (transport stop, synth change ...). */
    void requestReleaseAll() noexcept               { releasePending = true; }

    /** True while a release-all or a zone configuration is waiting for the next block. */
    bool isResetPending() const noexcept            { return configPending.load() || releasePending.load(); }

    /** Number of member channels the gesture engine allocates from (2 .. 1 + n). */
    int getLiveChannelCount() const noexcept;

    /** Latency introduced by chord detection lookahead. */
    int getLatencySamples() const noexcept;

    /** Main entry point. `in` = plain MIDI, `out` receives MPE (out is NOT cleared). */
    void process (const juce::MidiBuffer& in, juce::MidiBuffer& out, int numSamples,
                  const TransportState& transport);

    /** Immediately sends note-off for every sounding voice and recentres bend/pressure/timbre. */
    void releaseAll (juce::MidiBuffer& out, int pos);

    //==============================================================================
    // Introspection (UI + tests)
    int     getNumActiveVoices() const noexcept;
    int64_t getSampleClock() const noexcept       { return clock; }
    double  getSampleRate() const noexcept        { return sampleRate; }
    const Voice& getVoiceOnChannel (int ch) const noexcept { return voices[(size_t) juce::jlimit (0, 16, ch)]; }
    const Voice* findVoiceForKey (int key) const noexcept;
    const ScaleLock& getScaleLock() const noexcept { return scale; }

private:
    struct QueuedEvent
    {
        int64_t time = 0;
        uint8_t isNoteOn = 0, note = 0, velocity = 0, consumed = 0;
    };

    // event queue -------------------------------------------------------------
    bool queuePush (const QueuedEvent&) noexcept;
    QueuedEvent& queueAt (int i) noexcept { return queue[(size_t) ((queueHead + i) % queueCapacity)]; }
    void queuePopConsumed() noexcept;
    void queueClear() noexcept { queueHead = queueSize = 0; }

    // event handling ------------------------------------------------------------
    void handleDueEvent (juce::MidiBuffer& out, int64_t t, int64_t blockStart);
    void handleNoteOnGroup (juce::MidiBuffer& out, int64_t t, int64_t blockStart,
                            const int* notes, const int* keys, const int* vels, int count);
    void handleKeyUp (juce::MidiBuffer& out, Voice& v, int64_t t, int64_t blockStart);
    void startVoice (juce::MidiBuffer& out, int note, int key, int vel, int64_t t, int64_t blockStart);
    void glideVoice (Voice& v, int newNote, int key, int vel, int64_t t);
    void finishVoice (juce::MidiBuffer& out, Voice& v, int pos);
    void controlTick (juce::MidiBuffer& out, int64_t t, int64_t blockStart, bool forceSend);
    void emitVoiceControls (juce::MidiBuffer& out, Voice& v, int64_t t, int pos, bool force, bool smooth);

    // gesture maths -------------------------------------------------------------
    float modulationOffset (Voice& v, int64_t t, double dtSeconds) noexcept;
    float pressureValue (const Voice& v, int64_t t) const noexcept;   // 0..1
    float timbreValue (const Voice& v, int64_t t) const noexcept;     // 0..1
    bool  isGlideCandidate (const Voice& v, int64_t t) const noexcept;
    int64_t msToSamples (double ms) const noexcept { return (int64_t) std::llround (ms * 0.001 * sampleRate); }
    double beatsToSamples (double beats) const noexcept { return beats * 60.0 / bpm() * sampleRate; }
    double bpm() const noexcept { return transport.bpm > 1.0 ? transport.bpm : 120.0; }
    float bendRangeF() const noexcept { return (float) settings.bendRange; }
    static float nextRandom (uint32_t& state) noexcept;   // -1..1
    void pushVis (const Voice& v, VisualEvent::Type type, int64_t t) noexcept;

    //==============================================================================
    EngineSettings settings;
    TransportState transport;
    ScaleLock      scale;
    VoiceAllocator allocator;
    VisualSink*    sink = nullptr;

    std::array<Voice, 17> voices {};                // indexed by MIDI channel (2..16 used)
    std::array<QueuedEvent, queueCapacity> queue {};
    int queueHead = 0, queueSize = 0;

    double  sampleRate = 44100.0;
    int64_t clock = 0;              // absolute sample clock (start of current block)
    int64_t nextTick = 0;
    uint32_t voiceSeed = 0x9e3779b9u;

    // Set from the message thread (synth change, panic), consumed on the audio thread.
    std::atomic<bool> configPending  { true };
    std::atomic<bool> releasePending { false };
};

} // namespace bd
