/*
    TrapWin BEND DEMON - engine unit tests.

    Covers: channel allocation, 14-bit pitch-bend maths, MPE zone configuration, mono glide,
    chord-glide voice matching, 808 dive timing + delayed note-off, scale snapping and
    "no stuck notes" (randomised fuzz + release-all).

    2.0 line sequencer: curve interpolation, pattern playback across loop boundaries / seeks /
    stops, MPE channel allocation for overlapping lines, mono/legacy mode, pattern hot-swap,
    capture conversion, MIDI file export, gesture baking, magnet, state round trip, the
    lock-free pattern exchange and the internal synth.
*/
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "engine/GestureEngine.h"
#include "engine/Pattern.h"
#include "engine/PatternExchange.h"
#include "engine/PatternPlayer.h"
#include "engine/PatternTools.h"
#include "engine/InternalSynth.h"
#include "engine/CaptureRecorder.h"

#include <map>
#include <set>
#include <vector>

using namespace bd;

namespace
{
    constexpr double kSR = 48000.0;
    constexpr int    kBlock = 512;

    struct Rec
    {
        int64_t time;
        juce::MidiMessage msg;
    };

    struct InEvent
    {
        int64_t time;
        juce::MidiMessage msg;
    };

    /** Drives the engine block by block from an absolute-time input list and records output. */
    struct Harness
    {
        GestureEngine engine;
        std::vector<Rec> out;
        TransportState ts;

        explicit Harness (EngineSettings s)
        {
            engine.setSettings (s);
            engine.prepare (kSR);
            ts.hasInfo = true; ts.isPlaying = true; ts.bpm = 120.0;
        }

        void run (const std::vector<InEvent>& events, int64_t untilSample)
        {
            size_t next = 0;
            while (engine.getSampleClock() < untilSample)
            {
                const int64_t start = engine.getSampleClock();
                juce::MidiBuffer in, o;

                while (next < events.size() && events[next].time < start + kBlock)
                {
                    in.addEvent (events[next].msg, (int) (events[next].time - start));
                    ++next;
                }

                engine.process (in, o, kBlock, ts);

                for (const auto meta : o)
                    out.push_back ({ start + meta.samplePosition, meta.getMessage() });
            }
        }

        std::vector<Rec> noteOns() const
        {
            std::vector<Rec> r;
            for (auto& e : out) if (e.msg.isNoteOn()) r.push_back (e);
            return r;
        }

        std::vector<Rec> noteOffs() const
        {
            std::vector<Rec> r;
            for (auto& e : out) if (e.msg.isNoteOff()) r.push_back (e);
            return r;
        }

        /** Last pitch-wheel value sent on a channel at or before time t (8192 if none). */
        int lastBendOn (int ch, int64_t t = INT64_MAX) const
        {
            int v = 8192;
            for (auto& e : out)
                if (e.time <= t && e.msg.isPitchWheel() && e.msg.getChannel() == ch)
                    v = e.msg.getPitchWheelValue();
            return v;
        }

        /** Number of (channel, note) pairs that got a note-on but no matching note-off. */
        int countHangingNotes() const
        {
            std::map<int, int> balance;   // key = ch*128 + note
            for (auto& e : out)
            {
                const int key = e.msg.getChannel() * 128 + e.msg.getNoteNumber();
                if (e.msg.isNoteOn())  balance[key]++;
                if (e.msg.isNoteOff()) balance[key] = juce::jmax (0, balance[key] - 1);
            }
            int hanging = 0;
            for (auto& [k, n] : balance) hanging += n;
            return hanging;
        }
    };

    EngineSettings plainSettings()
    {
        EngineSettings s;
        s.glideOn = false; s.chordGlideOn = false; s.vibratoOn = false;
        s.diveOn = false; s.riseOn = false; s.wobbleOn = false; s.shakeOn = false; s.trillOn = false;
        s.pressureOn = false; s.timbreOn = false; s.scaleLockOn = false;
        return s;
    }

    juce::MidiMessage on (int note, int vel = 100)  { return juce::MidiMessage::noteOn (1, note, (juce::uint8) vel); }
    juce::MidiMessage off (int note)                { return juce::MidiMessage::noteOff (1, note); }
    int64_t ms (double m)                           { return (int64_t) std::llround (m * 0.001 * kSR); }
}

//==============================================================================
class VoiceAllocatorTests final : public juce::UnitTest
{
public:
    VoiceAllocatorTests() : juce::UnitTest ("Voice allocation", "BendDemon") {}

    void runTest() override
    {
        beginTest ("Round robin over member channels 2..16");
        {
            VoiceAllocator a;
            a.setNumMemberChannels (15);
            expectEquals (a.allocate (60, 0).channel, 2);
            expectEquals (a.allocate (64, 1).channel, 3);
            expectEquals (a.allocate (67, 2).channel, 4);
            a.free (3);
            // next free after the last allocated (4) is 5, not the just-freed 3
            expectEquals (a.allocate (70, 3).channel, 5);
        }

        beginTest ("Free channel that last played the same note is reused");
        {
            VoiceAllocator a;
            a.setNumMemberChannels (15);
            const int ch = a.allocate (60, 0).channel;
            a.allocate (62, 1);
            a.free (ch);
            expectEquals (a.allocate (60, 2).channel, ch);
        }

        beginTest ("Voice limit + steal oldest (releasing voices first)");
        {
            VoiceAllocator a;
            a.setNumMemberChannels (3);
            expectEquals (a.allocate (60, 10).channel, 2);
            expectEquals (a.allocate (62, 20).channel, 3);
            expectEquals (a.allocate (64, 30).channel, 4);

            auto r = a.allocate (65, 40);          // all held -> oldest held = ch 2
            expect (r.stolen);
            expectEquals (r.channel, 2);

            a.markReleasing (4);                   // ch 4 is releasing (dive tail)
            r = a.allocate (67, 50);               // releasing beats older held voices
            expect (r.stolen);
            expectEquals (r.channel, 4);
            expectEquals (a.numBusy(), 3);
        }

        beginTest ("Channel count is clamped to 1..15 and never leaves the zone");
        {
            VoiceAllocator a;
            a.setNumMemberChannels (40);
            expectEquals (a.getNumMemberChannels(), 15);
            for (int i = 0; i < 100; ++i)
            {
                const int ch = a.allocate (i % 128, i).channel;
                expect (ch >= 2 && ch <= 16);
            }
            a.setNumMemberChannels (1);
            for (int i = 0; i < 10; ++i)
                expectEquals (a.allocate (i, i).channel, 2);
        }

        beginTest ("Engine: three simultaneous notes land on three channels");
        {
            Harness h (plainSettings());
            h.run ({ { 100, on (60) }, { 100, on (64) }, { 100, on (67) } }, 4096);
            auto ons = h.noteOns();
            expectEquals ((int) ons.size(), 3);
            std::set<int> chans;
            for (auto& e : ons) { chans.insert (e.msg.getChannel()); expect (e.msg.getChannel() >= 2); }
            expectEquals ((int) chans.size(), 3);
        }
    }
};

//==============================================================================
class PitchBendTests final : public juce::UnitTest
{
public:
    PitchBendTests() : juce::UnitTest ("Pitch-bend maths", "BendDemon") {}

    void runTest() override
    {
        beginTest ("14-bit values at range 48");
        expectEquals (MpeOutput::semitonesToPitchWheel (0.0f, 48.0f), 8192);
        expectEquals (MpeOutput::semitonesToPitchWheel (48.0f, 48.0f), 16383);
        expectEquals (MpeOutput::semitonesToPitchWheel (-48.0f, 48.0f), 0);
        expectEquals (MpeOutput::semitonesToPitchWheel (12.0f, 48.0f), 8192 + 2048);   // 12/48*8191 = 2047.75
        expectEquals (MpeOutput::semitonesToPitchWheel (-12.0f, 48.0f), 8192 - 2048);  // 12/48*8192 = 2048
        expectEquals (MpeOutput::semitonesToPitchWheel (1.0f, 48.0f), 8192 + 171);     // 170.65
        expectEquals (MpeOutput::semitonesToPitchWheel (-24.0f, 48.0f), 4096);
        expectEquals (MpeOutput::semitonesToPitchWheel (100.0f, 48.0f), 16383);        // clamped
        expectEquals (MpeOutput::semitonesToPitchWheel (-100.0f, 48.0f), 0);

        beginTest ("Round trip error < 0.5 cent per step at range 48");
        for (float st = -48.0f; st <= 48.0f; st += 0.37f)
        {
            const float back = MpeOutput::pitchWheelToSemitones (MpeOutput::semitonesToPitchWheel (st, 48.0f), 48.0f);
            expectWithinAbsoluteError (back, st, 48.0f / 8191.0f);
        }

        beginTest ("Other ranges");
        expectEquals (MpeOutput::semitonesToPitchWheel (2.0f, 2.0f), 16383);
        expectEquals (MpeOutput::semitonesToPitchWheel (-1.0f, 2.0f), 4096);
        expectEquals (MpeOutput::semitonesToPitchWheel (96.0f, 96.0f), 16383);

        beginTest ("MPE zone configuration (RPN 6 + RPN 0) is sent first");
        {
            EngineSettings s = plainSettings();
            s.numVoices = 7;
            s.bendRange = 48;
            Harness h (s);
            h.run ({}, kBlock);

            // Parse RPNs: collect (channel, rpn, value)
            std::vector<std::tuple<int, int, int>> rpns;
            int msb[17] {}, lsb[17] {};
            for (auto& e : h.out)
            {
                if (! e.msg.isController()) continue;
                const int ch = e.msg.getChannel(), cc = e.msg.getControllerNumber(), v = e.msg.getControllerValue();
                if (cc == 101) msb[ch] = v;
                if (cc == 100) lsb[ch] = v;
                if (cc == 6)   rpns.emplace_back (ch, msb[ch] * 128 + lsb[ch], v);
            }

            expect (! rpns.empty());
            expect (rpns.front() == std::make_tuple (1, 6, 7), "first RPN must be the MCM on channel 1 with 7 members");

            int rangeMsgs = 0;
            for (auto& [ch, rpn, v] : rpns)
                if (rpn == 0 && ch >= 2 && ch <= 8) { expectEquals (v, 48); ++rangeMsgs; }
            expectEquals (rangeMsgs, 7);
        }

        beginTest ("Config messages of one channel never share a sample offset (VST3 CC regrouping safe)");
        {
            juce::MidiBuffer buf;
            const int span = MpeOutput::addZoneConfiguration (buf, 0, 511, 15, 48);
            expect (span <= MpeOutput::zoneConfigurationSpan);

            std::map<int, std::set<int>> offsetsPerChannel;
            std::map<int, int> countPerChannel;
            for (const auto meta : buf)
            {
                const int ch = meta.getMessage().getChannel();
                offsetsPerChannel[ch].insert (meta.samplePosition);
                countPerChannel[ch]++;
            }
            for (auto& [ch, n] : countPerChannel)
                expectEquals ((int) offsetsPerChannel[ch].size(), n, "channel " + juce::String (ch));

            // MCM (master) must precede every member RPN 0
            int mcmData = -1, firstMemberRpn = 1 << 30;
            for (const auto meta : buf)
            {
                const auto& m = meta.getMessage();
                if (m.isController() && m.getControllerNumber() == 6)
                {
                    if (m.getChannel() == 1 && mcmData < 0) mcmData = meta.samplePosition;
                    if (m.getChannel() >= 2) firstMemberRpn = juce::jmin (firstMemberRpn, meta.samplePosition);
                }
            }
            expect (mcmData >= 0 && mcmData < firstMemberRpn);

            // and the first note-on of a session comes after the whole configuration
            Harness h (plainSettings());
            h.run ({ { 0, on (60) } }, kBlock);
            int lastConfig = -1, noteOn = -1;
            for (auto& e : h.out)
            {
                if (e.msg.isController() && (e.msg.getControllerNumber() == 6 || e.msg.getControllerNumber() >= 100)) lastConfig = juce::jmax (lastConfig, (int) e.time);
                if (e.msg.isNoteOn()) noteOn = (int) e.time;
            }
            expect (noteOn > lastConfig, "note-on at " + juce::String (noteOn) + " after config end " + juce::String (lastConfig));
        }

        beginTest ("Glide produces a 14-bit bend that reaches the exact target");
        {
            EngineSettings s = plainSettings();
            s.glideOn = true;
            s.glideTimeMs = 100.0f;
            s.glideCurve = Curve::linear;
            Harness h (s);
            // Overlapping legato 60 -> 67
            h.run ({ { 0, on (60) }, { ms (200), on (67) }, { ms (210), off (60) }, { ms (600), off (67) } }, ms (500));

            const auto ons = h.noteOns();
            expectEquals ((int) ons.size(), 1, "legato must not retrigger");
            const int ch = ons[0].msg.getChannel();
            expectEquals (h.lastBendOn (ch, ms (450)), MpeOutput::semitonesToPitchWheel (7.0f, 48.0f));

            // midway through the glide the bend should be roughly half way
            const int mid = h.lastBendOn (ch, ms (250));
            const float midSt = MpeOutput::pitchWheelToSemitones (mid, 48.0f);
            expect (midSt > 2.5f && midSt < 4.5f, "mid-glide pitch " + juce::String (midSt));
        }
    }
};

//==============================================================================
class GlideTests final : public juce::UnitTest
{
public:
    GlideTests() : juce::UnitTest ("Glide + chord glide", "BendDemon") {}

    void runTest() override
    {
        beginTest ("Touching notes (piano-roll legato) glide and the note-off is delayed");
        {
            EngineSettings s = plainSettings();
            s.glideOn = true; s.glideWindowMs = 60.0f; s.glideTimeMs = 80.0f;
            Harness h (s);
            const int64_t t1 = ms (300);
            h.run ({ { 0, on (60) }, { t1, off (60) }, { t1, on (63) }, { ms (700), off (63) } }, ms (1200));

            expectEquals ((int) h.noteOns().size(), 1);
            const auto offs = h.noteOffs();
            expectEquals ((int) offs.size(), 1);
            expectEquals (offs[0].msg.getNoteNumber(), 60, "voice keeps its original note number");
            expectEquals (offs[0].time, ms (700) + ms (60), "note-off delayed by the glide window");
            expectEquals (h.countHangingNotes(), 0);
        }

        beginTest ("Gap larger than the glide window retriggers");
        {
            EngineSettings s = plainSettings();
            s.glideOn = true; s.glideWindowMs = 30.0f;
            Harness h (s);
            h.run ({ { 0, on (60) }, { ms (100), off (60) }, { ms (200), on (63) }, { ms (300), off (63) } }, ms (600));
            expectEquals ((int) h.noteOns().size(), 2);
            expectEquals (h.countHangingNotes(), 0);
        }

        beginTest ("Glide beyond bend range falls back to a retrigger");
        {
            EngineSettings s = plainSettings();
            s.glideOn = true; s.bendRange = 2;
            Harness h (s);
            h.run ({ { 0, on (60) }, { ms (100), on (67) }, { ms (110), off (60) }, { ms (300), off (67) } }, ms (600));
            expectEquals ((int) h.noteOns().size(), 2);
            expectEquals (h.countHangingNotes(), 0);
        }

        beginTest ("Chord glide matches old voices to new notes by nearest pitch");
        {
            EngineSettings s = plainSettings();
            s.chordGlideOn = true; s.glideWindowMs = 60.0f; s.glideTimeMs = 100.0f;
            s.chordWindowMs = 10.0f;
            Harness h (s);

            const int64_t t1 = ms (500);
            std::vector<InEvent> ev {
                { 0, on (48) }, { ms (2), on (55) }, { ms (4), on (64) },                 // C3 G3 E4
                { t1, off (48) }, { t1, off (55) }, { t1, off (64) },
                { t1, on (65) }, { t1 + ms (3), on (50) }, { t1 + ms (5), on (57) },      // F4 D3 A3 (unordered)
                { ms (1200), off (65) }, { ms (1200), off (50) }, { ms (1200), off (57) } };
            h.run (ev, ms (1100));

            const auto ons = h.noteOns();
            expectEquals ((int) ons.size(), 3, "second chord must not retrigger");

            // Which channel got which original note?
            std::map<int, int> chOfNote;
            for (auto& e : ons) chOfNote[e.msg.getNoteNumber()] = e.msg.getChannel();

            // 48 -> 50, 55 -> 57, 64 -> 65
            auto* v48 = &h.engine.getVoiceOnChannel (chOfNote[48]);
            auto* v55 = &h.engine.getVoiceOnChannel (chOfNote[55]);
            auto* v64 = &h.engine.getVoiceOnChannel (chOfNote[64]);
            expectEquals (v48->targetNote, 50);
            expectEquals (v55->targetNote, 57);
            expectEquals (v64->targetNote, 65);

            expectEquals (h.lastBendOn (chOfNote[48]), MpeOutput::semitonesToPitchWheel (2.0f, 48.0f));
            expectEquals (h.lastBendOn (chOfNote[55]), MpeOutput::semitonesToPitchWheel (2.0f, 48.0f));
            expectEquals (h.lastBendOn (chOfNote[64]), MpeOutput::semitonesToPitchWheel (1.0f, 48.0f));

            h.run ({ { ms (1200), off (65) }, { ms (1200), off (50) }, { ms (1200), off (57) } }, ms (1600));
        }

        beginTest ("Chord glide: bigger new chord glides what it can and starts the rest");
        {
            EngineSettings s = plainSettings();
            s.chordGlideOn = true;
            Harness h (s);
            const int64_t t1 = ms (400);
            h.run ({ { 0, on (60) }, { 0, on (64) },
                     { t1, off (60) }, { t1, off (64) },
                     { t1, on (59) }, { t1, on (62) }, { t1, on (67) },
                     { ms (800), off (59) }, { ms (800), off (62) }, { ms (800), off (67) } }, ms (1500));
            expectEquals ((int) h.noteOns().size(), 3);   // 2 + 1 extra
            expectEquals (h.countHangingNotes(), 0);
        }

        beginTest ("Chord glide reports lookahead latency");
        {
            EngineSettings s = plainSettings();
            s.chordGlideOn = true; s.chordWindowMs = 10.0f;
            GestureEngine e; e.setSettings (s); e.prepare (kSR);
            expectEquals (e.getLatencySamples(), 480);
            s.chordGlideOn = false; e.setSettings (s);
            expectEquals (e.getLatencySamples(), 0);
        }
    }
};

//==============================================================================
class DiveTests final : public juce::UnitTest
{
public:
    DiveTests() : juce::UnitTest ("808 dive / rise", "BendDemon") {}

    void runTest() override
    {
        beginTest ("Dive keeps the voice alive for dive time, then sends the note-off");
        {
            EngineSettings s = plainSettings();
            s.diveOn = true; s.diveSemis = 12.0f; s.diveTimeMs = 400.0f; s.diveCurve = Curve::linear;
            Harness h (s);
            const int64_t keyUp = 4800;
            h.run ({ { 0, on (48) }, { keyUp, off (48) } }, ms (1000));

            const auto offs = h.noteOffs();
            expectEquals ((int) offs.size(), 1);
            expectEquals (offs[0].time, keyUp + ms (400));

            const int ch = offs[0].msg.getChannel();
            expectEquals (h.lastBendOn (ch, keyUp), 8192, "no bend before key-up");
            expectEquals (h.lastBendOn (ch), MpeOutput::semitonesToPitchWheel (-12.0f, 48.0f));

            const float mid = MpeOutput::pitchWheelToSemitones (h.lastBendOn (ch, keyUp + ms (200)), 48.0f);
            expect (mid < -4.5f && mid > -7.5f, "half-way through a linear dive: " + juce::String (mid));
        }

        beginTest ("Dive starts after the glide window when glide is on");
        {
            EngineSettings s = plainSettings();
            s.diveOn = true; s.glideOn = true; s.glideWindowMs = 50.0f; s.diveTimeMs = 300.0f;
            Harness h (s);
            h.run ({ { 0, on (48) }, { ms (100), off (48) } }, ms (800));
            const auto offs = h.noteOffs();
            expectEquals ((int) offs.size(), 1);
            expectEquals (offs[0].time, ms (100) + ms (50) + ms (300));
        }

        beginTest ("Tempo-synced dive: 1/8 at 120 bpm = 250 ms");
        {
            EngineSettings s = plainSettings();
            s.diveOn = true; s.diveSync = true; s.diveDiv = 6;
            Harness h (s);
            h.run ({ { 0, on (36) }, { ms (100), off (36) } }, ms (800));
            const auto offs = h.noteOffs();
            expectEquals ((int) offs.size(), 1);
            expectEquals (offs[0].time, ms (100) + ms (250));
        }

        beginTest ("Rise bends up");
        {
            EngineSettings s = plainSettings();
            s.riseOn = true; s.diveSemis = 7.0f; s.diveTimeMs = 100.0f;
            Harness h (s);
            h.run ({ { 0, on (60) }, { ms (100), off (60) } }, ms (500));
            const int ch = h.noteOns()[0].msg.getChannel();
            expectEquals (h.lastBendOn (ch), MpeOutput::semitonesToPitchWheel (7.0f, 48.0f));
        }

        beginTest ("Dive target snaps to the scale");
        {
            EngineSettings s = plainSettings();
            s.diveOn = true; s.diveSemis = 4.0f; s.diveTimeMs = 100.0f;
            s.scaleLockOn = true; s.scaleKey = 0; s.scaleType = ScaleLock::minor;   // C minor
            Harness h (s);
            // G4 (67) - 4 = Eb4 (63) which IS in C minor; D4 (62) - 4 = A#3 (58) also in; E? use 62-4=58 ok.
            // 65 (F) - 4 = 61 (C#) -> not in C minor, nearest = 60 (C) or 62 (D): tie resolves to 60.
            h.run ({ { 0, on (65) }, { ms (50), off (65) } }, ms (400));
            const int ch = h.noteOns()[0].msg.getChannel();
            const float st = MpeOutput::pitchWheelToSemitones (h.lastBendOn (ch), 48.0f);
            expectWithinAbsoluteError (st, -5.0f, 0.01f);
        }

        beginTest ("A new note during a dive tail starts a new voice (tail is not cut)");
        {
            EngineSettings s = plainSettings();
            s.diveOn = true; s.diveTimeMs = 500.0f;
            Harness h (s);
            h.run ({ { 0, on (36) }, { ms (100), off (36) }, { ms (200), on (38) }, { ms (300), off (38) } }, ms (1500));
            const auto ons = h.noteOns();
            expectEquals ((int) ons.size(), 2);
            expect (ons[0].msg.getChannel() != ons[1].msg.getChannel());
            expectEquals (h.countHangingNotes(), 0);
        }
    }
};

//==============================================================================
class ScaleTests final : public juce::UnitTest
{
public:
    ScaleTests() : juce::UnitTest ("Scale lock", "BendDemon") {}

    void runTest() override
    {
        ScaleLock s;

        beginTest ("C major membership + snapping");
        s.set (0, ScaleLock::major);
        expect (s.isInScale (60) && s.isInScale (62) && s.isInScale (64) && ! s.isInScale (61));
        expectEquals (s.snap (61), 60);      // tie between C and D -> down
        expectEquals (s.snap (66), 65);      // F# -> F (tie F/G -> down)
        expectEquals (s.snap (70), 69);      // A# -> A (tie A/B -> down)
        expectEquals (s.snap (64), 64);

        beginTest ("A minor / harmonic minor / phrygian / dorian");
        s.set (9, ScaleLock::minor);
        expect (s.isInScale (69) && s.isInScale (72) && ! s.isInScale (73));
        s.set (9, ScaleLock::harmonicMinor);
        expect (s.isInScale (68) && ! s.isInScale (67));      // G# in, G out
        s.set (4, ScaleLock::phrygian);                       // E phrygian: E F G A B C D
        expect (s.isInScale (65) && ! s.isInScale (66));
        s.set (2, ScaleLock::dorian);                         // D dorian = white keys
        for (int n : { 62, 64, 65, 67, 69, 71, 72 }) expect (s.isInScale (n));
        for (int n : { 61, 63, 66, 68, 70 })         expect (! s.isInScale (n));

        beginTest ("Pentatonics");
        s.set (0, ScaleLock::majorPentatonic);                // C D E G A
        expectEquals (s.snap (65), 64);                       // F -> E
        expectEquals (s.snap (71), 72);                       // B -> C
        s.set (9, ScaleLock::minorPentatonic);                // A C D E G
        expectEquals (s.stepDegrees (69, 1), 72);
        expectEquals (s.stepDegrees (69, -1), 67);
        expectEquals (s.stepDegrees (69, 5), 81);

        beginTest ("Chromatic leaves everything alone");
        s.set (0, ScaleLock::chromatic);
        for (int n = 0; n < 128; ++n) expectEquals (s.snap (n), n);
        expectEquals (s.stepDegrees (60, 1), 61);

        beginTest ("Fractional snap + offset targets");
        s.set (0, ScaleLock::minor);
        expectEquals (s.snapPitch (62.4f), 62.0f);
        expectEquals (s.snapPitch (62.6f), 63.0f);
        expectEquals (s.snapOffsetTarget (60, -12.0f), 48.0f);
        expectEquals (s.snapOffsetTarget (60, -5.0f), 55.0f);
        expect (s.isInScale ((int) s.snapOffsetTarget (63, -4.0f)));

        beginTest ("Quantize input in the engine");
        {
            EngineSettings es = plainSettings();
            es.scaleLockOn = true; es.quantizeInput = true; es.scaleKey = 0; es.scaleType = ScaleLock::major;
            Harness h (es);
            h.run ({ { 0, on (61) }, { ms (100), off (61) } }, ms (300));
            const auto ons = h.noteOns();
            expectEquals ((int) ons.size(), 1);
            expectEquals (ons[0].msg.getNoteNumber(), 60);
            expectEquals (h.countHangingNotes(), 0);
        }

        beginTest ("Trill alternates to the next scale degree");
        {
            EngineSettings es = plainSettings();
            es.trillOn = true; es.trillDiv = 6;    // 1/8 at 120 bpm = 250 ms
            es.scaleLockOn = true; es.scaleKey = 0; es.scaleType = ScaleLock::major;
            Harness h (es);
            h.run ({ { 0, on (64) } }, ms (1000));  // E -> F is one semitone in C major
            const int ch = h.noteOns()[0].msg.getChannel();
            expectWithinAbsoluteError (MpeOutput::pitchWheelToSemitones (h.lastBendOn (ch, ms (200)), 48.0f), 0.0f, 0.01f);
            expectWithinAbsoluteError (MpeOutput::pitchWheelToSemitones (h.lastBendOn (ch, ms (450)), 48.0f), 1.0f, 0.01f);
            expectWithinAbsoluteError (MpeOutput::pitchWheelToSemitones (h.lastBendOn (ch, ms (700)), 48.0f), 0.0f, 0.01f);
        }
    }
};

//==============================================================================
class StuckNoteTests final : public juce::UnitTest
{
public:
    StuckNoteTests() : juce::UnitTest ("No stuck notes", "BendDemon") {}

    void runTest() override
    {
        beginTest ("Randomised fuzz with every gesture combination ends with zero hanging notes");
        {
            juce::Random rng (1234);

            for (int round = 0; round < 64; ++round)
            {
                EngineSettings s;
                s.glideOn      = rng.nextBool();
                s.chordGlideOn = rng.nextBool();
                s.vibratoOn    = rng.nextBool();
                s.diveOn       = rng.nextBool();
                s.riseOn       = rng.nextBool();
                s.wobbleOn     = rng.nextBool();
                s.shakeOn      = rng.nextBool();
                s.trillOn      = rng.nextBool();
                s.scaleLockOn  = rng.nextBool();
                s.quantizeInput= rng.nextBool();
                s.numVoices    = 1 + rng.nextInt (15);
                s.bendRange    = bendRanges[rng.nextInt (numBendRanges)];
                s.glideWindowMs= (float) rng.nextInt (200);
                s.diveTimeMs   = 20.0f + (float) rng.nextInt (1500);

                Harness h (s);
                std::vector<InEvent> ev;
                bool held[128] {};
                int64_t t = 0;

                for (int i = 0; i < 400; ++i)
                {
                    t += rng.nextInt ((int) ms (60));
                    const int note = 36 + rng.nextInt (36);
                    if (held[note] && rng.nextBool()) { ev.push_back ({ t, off (note) }); held[note] = false; }
                    else                              { ev.push_back ({ t, on (note, 1 + rng.nextInt (127)) }); held[note] = true; }
                }

                t += ms (10);
                for (int n = 0; n < 128; ++n)
                    if (held[n]) ev.push_back ({ t, off (n) });

                std::stable_sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.time < b.time; });
                h.run (ev, t + ms (2500));

                expectEquals (h.countHangingNotes(), 0, "round " + juce::String (round));
                expectEquals (h.engine.getNumActiveVoices(), 0, "round " + juce::String (round));

                for (auto& e : h.out)
                    if (e.msg.isNoteOnOrOff())
                        expect (e.msg.getChannel() >= 2 && e.msg.getChannel() <= 1 + s.numVoices);
            }
        }

        beginTest ("releaseAll (transport stop / synth change / bypass) silences held notes and recentres bends");
        {
            EngineSettings s = plainSettings();
            s.diveOn = true; s.vibratoOn = true; s.vibDelayMs = 0.0f;
            Harness h (s);
            h.run ({ { 0, on (60) }, { 0, on (64) }, { ms (50), off (64) } }, ms (300));
            expect (h.engine.getNumActiveVoices() > 0);

            h.engine.requestReleaseAll();
            h.run ({}, ms (400));

            expectEquals (h.countHangingNotes(), 0);
            expectEquals (h.engine.getNumActiveVoices(), 0);
            for (int ch = 2; ch <= 16; ++ch)
                expectEquals (h.lastBendOn (ch), 8192);
        }

        beginTest ("Incoming all-notes-off (CC123) releases everything");
        {
            Harness h (plainSettings());
            h.run ({ { 0, on (60) }, { 0, on (67) }, { ms (100), juce::MidiMessage::allNotesOff (1) } }, ms (300));
            expectEquals (h.countHangingNotes(), 0);
        }

        beginTest ("Changing voice count / bend range releases and reconfigures");
        {
            Harness h (plainSettings());
            h.run ({ { 0, on (60) } }, ms (100));
            auto s = plainSettings(); s.bendRange = 24;
            h.engine.setSettings (s);
            h.run ({}, ms (200));
            expectEquals (h.countHangingNotes(), 0);
        }

        beginTest ("Retriggering a held key without note-off does not leak voices");
        {
            Harness h (plainSettings());
            h.run ({ { 0, on (60) }, { ms (10), on (60) }, { ms (20), on (60) }, { ms (30), off (60) } }, ms (200));
            expectEquals (h.countHangingNotes(), 0);
            expectEquals (h.engine.getNumActiveVoices(), 0);
        }
    }
};

//==============================================================================
/** Interop: feed our MPE output into juce::MPEInstrument (a spec-following MPE receiver, the same
    logic many synths use) and check that it sees the zone, the notes and the bent pitch. */
class MpeInteropTests final : public juce::UnitTest
{
public:
    MpeInteropTests() : juce::UnitTest ("MPE interop (juce::MPEInstrument)", "BendDemon") {}

    void runTest() override
    {
        beginTest ("Receiver decodes zone layout, notes and per-note bend from our stream");

        EngineSettings s = plainSettings();
        s.glideOn = true; s.glideTimeMs = 50.0f; s.numVoices = 15; s.bendRange = 48;
        Harness h (s);
        h.run ({ { 0, on (60) }, { ms (100), on (64) }, { ms (100), off (60) } }, ms (400));

        juce::MPEInstrument receiver;   // starts with no zone; must learn it from our RPN 6
        for (auto& e : h.out)
            receiver.processNextMidiEvent (e.msg);

        const auto layout = receiver.getZoneLayout();
        expect (layout.getLowerZone().isActive(), "lower zone configured via MCM");
        expectEquals (layout.getLowerZone().numMemberChannels, 15);
        expectEquals (layout.getLowerZone().perNotePitchbendRange, 48);

        expectEquals (receiver.getNumPlayingNotes(), 1, "glided legato = one sounding note");
        const auto note = receiver.getNote (0);
        expectEquals ((int) note.initialNote, 60);
        expectWithinAbsoluteError ((float) note.totalPitchbendInSemitones, 4.0f, 0.02f);
    }
};

static MpeInteropTests     mpeInteropTests;
static VoiceAllocatorTests voiceAllocatorTests;
static PitchBendTests      pitchBendTests;
static GlideTests          glideTests;
static DiveTests           diveTests;
static ScaleTests          scaleTests;
static StuckNoteTests      stuckNoteTests;

//==============================================================================
//  2.0 LINE SEQUENCER
//==============================================================================
namespace
{
    constexpr double kBpm = 120.0;
    constexpr double kSamplesPerBeat = kSR * 60.0 / kBpm;   // 24000

    int64_t beats (double b) { return (int64_t) std::llround (b * kSamplesPerBeat); }

    PatternLine lineOf (double start, double len, Curve1D pitch, bool smoothCurve = true)
    {
        PatternLine l;
        l.start = start; l.length = len; l.pitch = std::move (pitch); l.smooth = smoothCurve;
        return l;
    }

    /** Drives a PatternPlayer from an absolute sample clock; records output with absolute times. */
    struct SeqHarness
    {
        PatternPlayer player;
        std::vector<Rec> out;
        int64_t clock = 0;        // absolute output time
        double ppq = 0.0;         // musical position at `clock`

        explicit SeqHarness (const Pattern& p, PatternPlayer::Config c = {})
        {
            player.prepare (kSR);
            player.setConfig (c);
            player.setPattern (&p);
        }

        void play (int64_t numSamples, int block = 512, bool playing = true)
        {
            int64_t done = 0;
            while (done < numSamples)
            {
                const int n = (int) std::min<int64_t> (block, numSamples - done);
                juce::MidiBuffer buf;
                player.process (buf, n, { playing, ppq, kBpm });
                for (const auto m : buf) out.push_back ({ clock + m.samplePosition, m.getMessage() });
                clock += n; done += n;
                if (playing) ppq += (double) n / kSamplesPerBeat;
            }
        }

        void seek (double newPpq) { ppq = newPpq; }

        std::vector<Rec> filter (std::function<bool (const juce::MidiMessage&)> f) const
        {
            std::vector<Rec> r;
            for (auto& e : out) if (f (e.msg)) r.push_back (e);
            return r;
        }
        std::vector<Rec> ons() const  { return filter ([] (auto& m) { return m.isNoteOn(); }); }
        std::vector<Rec> offs() const { return filter ([] (auto& m) { return m.isNoteOff(); }); }

        int hanging() const
        {
            std::map<int, int> bal;
            for (auto& e : out)
            {
                const int key = e.msg.getChannel() * 128 + e.msg.getNoteNumber();
                if (e.msg.isNoteOn())  bal[key]++;
                if (e.msg.isNoteOff()) bal[key] = juce::jmax (0, bal[key] - 1);
            }
            int n = 0;
            for (auto& [k, v] : bal) n += v;
            return n;
        }

        /** Max number of notes sounding at the same time. */
        int maxPolyphony() const
        {
            std::map<int, int> bal;
            int cur = 0, mx = 0;
            for (auto& e : out)
            {
                const int key = e.msg.getChannel() * 128 + e.msg.getNoteNumber();
                if (e.msg.isNoteOn())  { bal[key]++; ++cur; }
                if (e.msg.isNoteOff() && bal[key] > 0) { bal[key]--; --cur; }
                mx = juce::jmax (mx, cur);
            }
            return mx;
        }

        float semisAt (int ch, int64_t t, float range = 48.0f) const
        {
            int v = 8192;
            for (auto& e : out)
                if (e.time <= t && e.msg.isPitchWheel() && e.msg.getChannel() == ch)
                    v = e.msg.getPitchWheelValue();
            return MpeOutput::pitchWheelToSemitones (v, range);
        }
    };
}

class CurveTests final : public juce::UnitTest
{
public:
    CurveTests() : juce::UnitTest ("Line sequencer: curves", "BendDemon") {}

    void runTest() override
    {
        beginTest ("Monotone cubic passes through every point and never overshoots");
        {
            const Curve1D c { { 0.0, 60.0f }, { 1.0, 62.0f }, { 2.0, 62.0f }, { 3.0, 70.0f }, { 3.5, 65.0f } };
            for (auto& p : c)
                expectWithinAbsoluteError (curves::evalMonotone (c, p.t), p.v, 1.0e-4f);

            for (double t = 0.0; t <= 1.0; t += 0.01)
            {
                const float v = curves::evalMonotone (c, t);
                expect (v >= 60.0f - 1.0e-4f && v <= 62.0f + 1.0e-4f, "rising segment stays inside its points");
            }
            for (double t = 1.0; t <= 2.0; t += 0.01)
                expectWithinAbsoluteError (curves::evalMonotone (c, t), 62.0f, 1.0e-4f);   // a hold stays a hold
            for (double t = 3.0; t <= 3.5; t += 0.01)
            {
                const float v = curves::evalMonotone (c, t);
                expect (v <= 70.0f + 1.0e-4f && v >= 65.0f - 1.0e-4f);
            }

            // smooth (no kink): small steps give small changes
            float prev = curves::evalMonotone (c, 0.0);
            for (double t = 0.001; t <= 3.5; t += 0.001)
            {
                const float v = curves::evalMonotone (c, t);
                expect (std::abs (v - prev) < 0.05f, "continuous");
                prev = v;
            }
        }

        beginTest ("Linear segments, clamping outside, single point");
        {
            const Curve1D c { { 0.0, 60.0f }, { 2.0, 64.0f } };
            expectWithinAbsoluteError (curves::evalLinear (c, 1.0), 62.0f, 1.0e-5f);
            expectWithinAbsoluteError (curves::evalLinear (c, -1.0), 60.0f, 1.0e-5f);
            expectWithinAbsoluteError (curves::evalLinear (c, 5.0), 64.0f, 1.0e-5f);
            expectWithinAbsoluteError (curves::evalMonotone ({ { 0.5, 61.0f } }, 3.0), 61.0f, 1.0e-5f);
        }

        beginTest ("Line pitch includes vibrato with fade-in; expression defaults");
        {
            auto l = lineOf (0.0, 4.0, { { 0.0, 60.0f } });
            l.vibDepth = 0.5f; l.vibRate = 1.0f; l.vibFade = 1.0f;
            expectWithinAbsoluteError (l.pitchAt (0.0), 60.0f, 1.0e-5f);
            expectWithinAbsoluteError (l.pitchAt (2.25), 60.5f, 1.0e-3f);    // full depth, sin peak
            expectWithinAbsoluteError (l.pitchAt (0.25), 60.0f + 0.25f * 0.5f, 1.0e-3f);   // fading in
            expectWithinAbsoluteError (l.pressureAt (1.0), PatternLine::defaultPressure, 1.0e-6f);
            expectWithinAbsoluteError (l.timbreAt (1.0), PatternLine::defaultTimbre, 1.0e-6f);
        }

        beginTest ("Simplify keeps the shape with fewer points");
        {
            Curve1D dense;
            for (int i = 0; i <= 100; ++i) dense.push_back ({ i * 0.01, 60.0f + (float) i * 0.04f });
            const auto s = curves::simplify (dense, 0.01f);
            expectEquals ((int) s.size(), 2, "a straight line needs 2 points");

            Curve1D bent = dense;
            for (int i = 50; i <= 100; ++i) bent[(size_t) i].v = 62.0f;
            const auto s2 = curves::simplify (bent, 0.01f);
            expect (s2.size() >= 3 && s2.size() <= 4);
            for (double t = 0.0; t <= 1.0; t += 0.01)
                expectWithinAbsoluteError (curves::evalLinear (s2, t), curves::evalLinear (bent, t), 0.02f);
        }

        beginTest ("Normalise: sorted, start point inserted, values clamped");
        {
            auto l = lineOf (-2.0, 0.0, { { 2.0, 200.0f }, { 1.0, 62.0f } });
            l.normalise();
            expect (l.start <= 0.0 && l.length > 0.0);
            expect (l.pitch.front().t <= 0.0);
            expect (l.pitch.back().v <= 127.0f);
            for (size_t i = 1; i < l.pitch.size(); ++i) expect (l.pitch[i].t > l.pitch[i - 1].t);
        }
    }
};

class PatternStateTests final : public juce::UnitTest
{
public:
    PatternStateTests() : juce::UnitTest ("Line sequencer: state", "BendDemon") {}

    void runTest() override
    {
        beginTest ("XML round trip is exact (every factory pattern + odd values)");
        for (int i = 0; i < tools::numFactoryPatterns(); ++i)
        {
            auto p = tools::makeFactoryPattern (i);
            expect (! p.lines.empty(), tools::factoryPatternName (i));
            const auto back = Pattern::fromXml (*p.toXml());
            expect (back == p, juce::String ("round trip: ") + tools::factoryPatternName (i));
        }
        {
            Pattern p;
            p.bars = 7;
            auto l = lineOf (1.0 / 3.0, 2.0 / 7.0, { { 0.0, 60.123456f }, { 0.1, 61.9f } }, false);
            l.pressure = { { 0.0, 0.1f }, { 0.2, 0.9f } };
            l.timbre = { { 0.05, 0.33f } };
            l.vibDepth = 0.31f; l.vibRate = 3.3f; l.vibFade = 0.77f; l.velocity = 0.42f;
            p.add (l);
            p.normalise();
            const auto back = Pattern::fromXml (*p.toXml());
            expect (back == p, "odd values");
            expectEquals (back.bars, 7);
            expect (back.nextId > back.lines.front().id, "next id survives");
        }

        beginTest ("Garbage XML is repaired, not trusted");
        {
            juce::XmlElement x (Pattern::xmlTag);
            x.setAttribute ("bars", 99);
            auto* l1 = x.createNewChildElement ("LINE");
            l1->setAttribute ("id", 5); l1->setAttribute ("length", -3); l1->setAttribute ("pitch", "nonsense 0,64 x,y");
            auto* l2 = x.createNewChildElement ("LINE");
            l2->setAttribute ("id", 5); l2->setAttribute ("start", 1.0);
            const auto p = Pattern::fromXml (x);
            expectEquals (p.bars, Pattern::maxBars);
            expectEquals ((int) p.lines.size(), 2);
            expect (p.lines[0].id != p.lines[1].id, "duplicate ids repaired");
            for (auto& l : p.lines) { expect (l.length > 0.0); expect (! l.pitch.empty()); }
        }

        beginTest ("Lock-free exchange frees only snapshots the audio thread cannot reach");
        {
            PatternExchange ex;
            Pattern p;
            bool changed = false;
            expect (ex.acquire (changed) == nullptr && ! changed);

            p.bars = 1; ex.publish (p);
            p.bars = 2; ex.publish (p);
            p.bars = 3; ex.publish (p);
            expectEquals ((int) ex.numOwnedSnapshots(), 1, "unconsumed snapshots are dropped right away");

            auto* s = ex.acquire (changed);
            expect (changed && s != nullptr && s->pattern.bars == 3);

            p.bars = 4; ex.publish (p);
            expectEquals ((int) ex.numOwnedSnapshots(), 2, "the one in use stays alive");
            expect (ex.getCurrent()->pattern.bars == 3);

            s = ex.acquire (changed);
            expect (changed && s->pattern.bars == 4);
            ex.collectGarbage();
            expectEquals ((int) ex.numOwnedSnapshots(), 1);
            s = ex.acquire (changed);
            expect (! changed && s->pattern.bars == 4);
        }
    }
};

class PlaybackTests final : public juce::UnitTest
{
public:
    PlaybackTests() : juce::UnitTest ("Line sequencer: playback", "BendDemon") {}

    void runTest() override
    {
        beginTest ("Notes start/end on time, loop, and bend follows the curve");
        {
            Pattern p; p.bars = 1;
            p.add (lineOf (0.0, 1.0, { { 0.0, 60.0f }, { 1.0, 62.0f } }, false));
            p.add (lineOf (2.0, 1.0, { { 0.0, 67.0f } }));
            SeqHarness h (p);
            h.play (beats (8.0) - 10, 512);

            const auto ons = h.ons(), offs = h.offs();
            expectEquals ((int) ons.size(), 4);
            expectEquals ((int) offs.size(), 4);
            const int64_t onTimes[] = { 0, beats (2), beats (4), beats (6) };
            const int64_t offTimes[] = { beats (1), beats (3), beats (5), beats (7) };
            for (int i = 0; i < 4 && i < (int) ons.size(); ++i)
            {
                expect (std::abs (ons[(size_t) i].time - onTimes[i]) <= 1, "note-on time " + juce::String (i));
                expectEquals (ons[(size_t) i].msg.getNoteNumber(), i % 2 == 0 ? 60 : 67);
                const int ch = ons[(size_t) i].msg.getChannel();
                expect (ch >= 2 && ch <= 16);
            }
            for (int i = 0; i < 4 && i < (int) offs.size(); ++i)
                expect (std::abs (offs[(size_t) i].time - offTimes[i]) <= 1, "note-off time " + juce::String (i));

            const int chA = ons[0].msg.getChannel();
            expectWithinAbsoluteError (h.semisAt (chA, beats (0.5)), 1.0f, 0.02f);
            expectWithinAbsoluteError (h.semisAt (chA, beats (0.95)), 1.9f, 0.02f);

            // bend is updated at control rate (32 samples), not once per block
            int bendsInFirstBeat = 0;
            for (auto& e : h.out) if (e.msg.isPitchWheel() && e.msg.getChannel() == chA && e.time < beats (1)) ++bendsInFirstBeat;
            expect (bendsInFirstBeat > 250, "sub-block resolution (341 distinct 14-bit steps in this glide)");

            // controls are sent before the note-on of each line
            for (auto& on : ons)
            {
                bool sawBend = false;
                for (auto& e : h.out)
                {
                    if (&e == &on || e.time > on.time) break;
                    if (e.time == on.time && e.msg.isPitchWheel() && e.msg.getChannel() == on.msg.getChannel()) sawBend = true;
                }
                expect (sawBend, "bend before note-on");
            }
        }

        beginTest ("Loop boundary: the ending line is released before the next cycle starts (odd block sizes)");
        for (int block : { 441, 512, 64, 1000, 7 })
        {
            Pattern p; p.bars = 1;
            p.add (lineOf (0.0, 1.0, { { 0.0, 60.0f } }));
            p.add (lineOf (3.0, 1.0, { { 0.0, 72.0f } }));      // ends exactly at the loop point
            SeqHarness h (p);
            h.play (beats (12.0) - 3, block);

            expectEquals ((int) h.ons().size(), 6, "block " + juce::String (block));
            // at every loop point: off(72) comes before on(60), same sample
            for (int k = 1; k <= 2; ++k)
            {
                const int64_t t = beats (4.0 * k);
                int idxOff = -1, idxOn = -1;
                for (int i = 0; i < (int) h.out.size(); ++i)
                {
                    const auto& e = h.out[(size_t) i];
                    if (std::abs (e.time - t) > 1) continue;
                    if (e.msg.isNoteOff() && e.msg.getNoteNumber() == 72 && idxOff < 0) idxOff = i;
                    if (e.msg.isNoteOn()  && e.msg.getNoteNumber() == 60 && idxOn < 0)  idxOn = i;
                }
                expect (idxOff >= 0 && idxOn >= 0 && idxOff < idxOn, "off before on at loop " + juce::String (k));
            }
            h.play (64, 64, false);
            expectEquals (h.hanging(), 0);
        }

        beginTest ("Seek: notes off, then chase the line under the new position");
        {
            Pattern p; p.bars = 1;
            p.add (lineOf (0.0, 2.0, { { 0.0, 60.0f }, { 2.0, 64.0f } }, false));
            SeqHarness h (p);
            h.play (beats (1.5), 256);
            expectEquals ((int) h.ons().size(), 1);

            const int64_t jumpAt = h.clock;
            h.seek (0.5);
            h.play (256, 256);
            const auto offs = h.offs();
            const auto ons = h.ons();
            expectEquals ((int) offs.size(), 1, "jump releases");
            expect (offs[0].time == jumpAt);
            expectEquals ((int) ons.size(), 2, "chased");
            expect (ons[1].time == jumpAt);
            // chased note starts at the curve's pitch at 0.5 beats (61) - nearest semitone + bend
            const int ch = ons[1].msg.getChannel();
            expectEquals (ons[1].msg.getNoteNumber(), 61);
            expectWithinAbsoluteError ((float) ons[1].msg.getNoteNumber() + h.semisAt (ch, jumpAt), 61.0f, 0.02f);
        }

        beginTest ("Stop releases everything; restart chases");
        {
            Pattern p; p.bars = 2;
            p.add (lineOf (0.0, 6.0, { { 0.0, 48.0f } }));
            p.add (lineOf (1.0, 2.0, { { 0.0, 55.0f } }));
            SeqHarness h (p);
            h.play (beats (1.5));
            h.play (4800, 512, false);   // stopped
            expectEquals (h.hanging(), 0, "stop = no hanging notes");
            const int onsBefore = (int) h.ons().size();
            h.play (512);                 // resumes at the same ppq -> chase both lines
            expectEquals ((int) h.ons().size(), onsBefore + 2);
            h.play (512, 512, false);
            expectEquals (h.hanging(), 0);
        }

        beginTest ("Small forward ppq jitter (host rounding) does not retrigger");
        {
            Pattern p; p.bars = 4;
            p.add (lineOf (0.0, 8.0, { { 0.0, 60.0f } }));
            SeqHarness h (p);
            for (int i = 0; i < 200; ++i)
            {
                h.play (512);
                h.ppq += (i % 3 == 0 ? 1.0e-5 : 0.0);
            }
            expectEquals ((int) h.ons().size(), 1);
        }

        beginTest ("Overlapping lines get their own channels inside the configured range");
        {
            Pattern p; p.bars = 1;
            for (int i = 0; i < 5; ++i)
                p.add (lineOf (0.25 * i, 2.0, { { 0.0, 60.0f + (float) i } }));

            PatternPlayer::Config c; c.firstChannel = 6; c.lastChannel = 16;
            SeqHarness h (p, c);
            h.play (beats (1.5));
            std::set<int> chans;
            for (auto& on : h.ons())
            {
                chans.insert (on.msg.getChannel());
                expect (on.msg.getChannel() >= 6 && on.msg.getChannel() <= 16);
            }
            expectEquals ((int) chans.size(), 5);
            for (auto& e : h.out)
                if (e.msg.getChannel() > 0) expect (e.msg.getChannel() >= 6, "nothing on the live channels");
        }

        beginTest ("More lines than channels: oldest is stolen, never more voices than channels");
        {
            Pattern p; p.bars = 1;
            for (int i = 0; i < 4; ++i)
                p.add (lineOf (0.5 * i, 3.0, { { 0.0, 60.0f + 2.0f * (float) i } }));
            PatternPlayer::Config c; c.firstChannel = 15; c.lastChannel = 16;
            SeqHarness h (p, c);
            h.play (beats (8.0));
            expect (h.maxPolyphony() <= 2);
            h.play (512, 512, false);
            expectEquals (h.hanging(), 0);
        }

        beginTest ("Editing while playing: deleted line stops, changed curve continues");
        {
            Pattern p; p.bars = 1;
            const auto a = p.add (lineOf (0.0, 4.0, { { 0.0, 60.0f } }));
            p.add (lineOf (0.0, 4.0, { { 0.0, 67.0f } }));
            SeqHarness h (p);
            h.play (beats (1.0));
            expectEquals ((int) h.ons().size(), 2);

            Pattern p2 = p;
            p2.remove (a);
            p2.lines[0].pitch = { { 0.0, 69.0f } };   // 67 -> 69: continues with a bend
            h.player.setPattern (&p2);
            h.play (beats (0.5));
            expectEquals ((int) h.offs().size(), 1, "deleted line released");
            expectEquals (h.offs()[0].msg.getNoteNumber(), 60);
            const int ch67 = h.ons()[1].msg.getNoteNumber() == 67 ? h.ons()[1].msg.getChannel() : h.ons()[0].msg.getChannel();
            expectWithinAbsoluteError (h.semisAt (ch67, h.clock), 2.0f, 0.02f);
            h.play (512, 512, false);
            expectEquals (h.hanging(), 0);
        }

        beginTest ("Fuzz: random patterns, seeks, stops, edits and block sizes never hang notes");
        {
            juce::Random rng (1234);
            for (int round = 0; round < 40; ++round)
            {
                Pattern p; p.bars = 1 + rng.nextInt (4);
                const int n = rng.nextInt (12);
                for (int i = 0; i < n; ++i)
                    p.add (lineOf (rng.nextDouble() * p.lengthBeats(), 0.05 + rng.nextDouble() * 3.0,
                                   { { 0.0, 40.0f + rng.nextFloat() * 40.0f }, { 0.5, 40.0f + rng.nextFloat() * 40.0f } }));
                p.normalise();

                PatternPlayer::Config c;
                c.mono = rng.nextBool();
                c.firstChannel = 2 + rng.nextInt (8);
                c.bendRange = rng.nextBool() ? 48 : 2;
                SeqHarness h (p, c);
                Pattern edited = p;

                for (int step = 0; step < 60; ++step)
                {
                    const int r = rng.nextInt (10);
                    if (r == 0) h.seek (rng.nextDouble() * 20.0);
                    if (r == 1) { h.play (300, 300, false); continue; }
                    if (r == 2 && ! edited.lines.empty())
                    {
                        edited.lines.erase (edited.lines.begin() + rng.nextInt ((int) edited.lines.size()));
                        h.player.setPattern (&edited);
                    }
                    h.play (1 + rng.nextInt (3000), 1 + rng.nextInt (1024));
                }
                h.play (64, 64, false);
                expectEquals (h.hanging(), 0, "round " + juce::String (round));
            }
        }
    }
};

class MonoModeTests final : public juce::UnitTest
{
public:
    MonoModeTests() : juce::UnitTest ("Line sequencer: mono / legacy", "BendDemon") {}

    void runTest() override
    {
        PatternPlayer::Config mono; mono.mono = true; mono.bendRange = 48;

        beginTest ("Everything on channel 1, last-note priority, legato note order");
        {
            Pattern p; p.bars = 1;
            p.add (lineOf (0.0, 2.0, { { 0.0, 60.0f } }));
            p.add (lineOf (1.0, 2.0, { { 0.0, 64.0f } }));
            SeqHarness h (p, mono);
            h.play (beats (3.5));

            for (auto& e : h.out) expectEquals (e.msg.getChannel(), 1);
            const auto ons = h.ons();
            expectEquals ((int) ons.size(), 2);
            expectEquals (ons[1].msg.getNoteNumber(), 64);
            expect (std::abs (ons[1].time - beats (1.0)) <= 1);

            // new note-on comes before the old note-off at the same sample (legato for mono synths)
            int idxOn = -1, idxOff = -1;
            for (int i = 0; i < (int) h.out.size(); ++i)
            {
                const auto& e = h.out[(size_t) i];
                if (e.msg.isNoteOn() && e.msg.getNoteNumber() == 64) idxOn = i;
                if (e.msg.isNoteOff() && e.msg.getNoteNumber() == 60 && idxOff < 0) idxOff = i;
            }
            expect (idxOn >= 0 && idxOff > idxOn);
            expectEquals (h.hanging(), 0);
        }

        beginTest ("Returns to the still-sounding older line when the newer one ends");
        {
            Pattern p; p.bars = 1;
            p.add (lineOf (0.0, 4.0, { { 0.0, 60.0f } }));
            p.add (lineOf (1.0, 1.0, { { 0.0, 64.0f } }));
            SeqHarness h (p, mono);
            h.play (beats (3.0));
            const auto ons = h.ons();
            expectEquals ((int) ons.size(), 3);
            expectEquals (ons[2].msg.getNoteNumber(), 60);
            expect (std::abs (ons[2].time - beats (2.0)) <= 1);
        }

        beginTest ("Retrigger when the curve leaves the bend range (range 2)");
        {
            Pattern p; p.bars = 1;
            p.add (lineOf (0.0, 2.0, { { 0.0, 60.0f }, { 1.0, 67.0f }, { 2.0, 67.0f } }, false));
            PatternPlayer::Config c = mono; c.bendRange = 2;
            SeqHarness h (p, c);
            h.play (beats (1.9));
            const auto ons = h.ons();
            expect (ons.size() >= 3, "retriggered while gliding 7 semitones with +/-2");
            const int lastNote = ons.back().msg.getNoteNumber();
            const float total = (float) lastNote + h.semisAt (1, beats (1.5), 2.0f);
            expectWithinAbsoluteError (total, 67.0f, 0.02f);
            expect (h.maxPolyphony() <= 2, "at most a brief legato overlap");
        }
    }
};

class CaptureExportTests final : public juce::UnitTest
{
public:
    CaptureExportTests() : juce::UnitTest ("Line sequencer: capture + MIDI export", "BendDemon") {}

    static std::vector<tools::CapturedEvent> toEvents (const std::vector<Rec>& out)
    {
        std::vector<tools::CapturedEvent> evs;
        for (auto& r : out)
        {
            tools::CapturedEvent e;
            e.ppq = (double) r.time / kSamplesPerBeat;
            e.channel = (uint8_t) r.msg.getChannel();
            if (r.msg.isNoteOn())       { e.type = tools::CapturedEvent::noteOn; e.note = (uint8_t) r.msg.getNoteNumber(); e.velocity = r.msg.getVelocity(); }
            else if (r.msg.isNoteOff()) { e.type = tools::CapturedEvent::noteOff; e.note = (uint8_t) r.msg.getNoteNumber(); }
            else if (r.msg.isPitchWheel()) { e.type = tools::CapturedEvent::bend; e.value = (int16_t) r.msg.getPitchWheelValue(); }
            else if (r.msg.isChannelPressure()) { e.type = tools::CapturedEvent::pressure; e.value = (int16_t) r.msg.getChannelPressureValue(); }
            else if (r.msg.isControllerOfType (74)) { e.type = tools::CapturedEvent::timbre; e.value = (int16_t) r.msg.getControllerValue(); }
            else continue;
            evs.push_back (e);
        }
        return evs;
    }

    void runTest() override
    {
        beginTest ("Capture: a gesture-engine glide becomes one line with the glide in its curve");
        {
            EngineSettings s = plainSettings();
            s.glideOn = true; s.glideTimeMs = 100.0f;
            Harness h (s);
            // 120 bpm: beat 1 = 0.5 s
            h.run ({ { ms (500), on (60) }, { ms (1000), on (64) }, { ms (1000), off (60) }, { ms (1500), off (64) } }, ms (2200));

            std::vector<tools::CapturedEvent> evs;
            for (auto& r : h.out)
            {
                tools::CapturedEvent e;
                e.ppq = (double) r.time / kSR * 2.0;
                e.channel = (uint8_t) r.msg.getChannel();
                if (r.msg.isNoteOn())       { e.type = tools::CapturedEvent::noteOn; e.note = (uint8_t) r.msg.getNoteNumber(); e.velocity = r.msg.getVelocity(); }
                else if (r.msg.isNoteOff()) { e.type = tools::CapturedEvent::noteOff; e.note = (uint8_t) r.msg.getNoteNumber(); }
                else if (r.msg.isPitchWheel()) { e.type = tools::CapturedEvent::bend; e.value = (int16_t) r.msg.getPitchWheelValue(); }
                else continue;
                evs.push_back (e);
            }

            const auto lines = tools::linesFromCapture (evs, 0.0, 8.0, 48);
            expectEquals ((int) lines.size(), 1, "legato glide = one voice = one line");
            const auto& l = lines.front();
            expectWithinAbsoluteError (l.start, 1.0, 0.01);
            expectWithinAbsoluteError (l.length, 2.0 + 0.06 * 2.0, 0.05);   // + glide window (60 ms)
            expectWithinAbsoluteError (l.curvePitchAt (0.5), 60.0f, 0.05f);
            expectWithinAbsoluteError (l.curvePitchAt (1.6), 64.0f, 0.05f);
            const float mid = l.curvePitchAt (1.1);
            expect (mid > 60.5f && mid < 63.9f, "the glide itself is in the curve");
            expect (l.pitch.size() < 40, "simplified");
        }

        beginTest ("Capture: notes are clipped to the window and re-timed");
        {
            std::vector<tools::CapturedEvent> evs;
            auto ev = [&] (double ppq, uint8_t type, uint8_t ch, uint8_t note, int16_t value = 0)
            {
                tools::CapturedEvent e; e.ppq = ppq; e.type = type; e.channel = ch; e.note = note; e.velocity = 100; e.value = value;
                evs.push_back (e);
            };
            ev (3.0, tools::CapturedEvent::noteOn, 2, 50);
            ev (5.0, tools::CapturedEvent::noteOff, 2, 50);
            ev (6.0, tools::CapturedEvent::noteOn, 3, 55);
            ev (6.5, tools::CapturedEvent::pressure, 3, 0, 127);
            ev (9.0, tools::CapturedEvent::noteOff, 3, 55);
            ev (4.5, tools::CapturedEvent::noteOn, 1, 70);   // master channel: ignored
            std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.ppq < b.ppq; });

            const auto lines = tools::linesFromCapture (evs, 4.0, 4.0, 48);
            expectEquals ((int) lines.size(), 2);
            expectWithinAbsoluteError (lines[0].start, 0.0, 1.0e-9);
            expectWithinAbsoluteError (lines[0].length, 1.0, 1.0e-9);
            expectWithinAbsoluteError (lines[1].start, 2.0, 1.0e-9);
            expectWithinAbsoluteError (lines[1].length, 2.0, 1.0e-9);
            expectWithinAbsoluteError (lines[1].pressureAt (1.5), 1.0f, 0.01f);
        }

        beginTest ("Capture of the sequencer's own output reproduces the pattern");
        {
            const auto p = tools::makeFactoryPattern (0);
            SeqHarness h (p);
            h.play (beats (p.lengthBeats()) + 64);
            const auto lines = tools::linesFromCapture (toEvents (h.out), 0.0, p.lengthBeats(), 48);
            expectEquals (lines.size(), p.lines.size());
            for (size_t i = 0; i < juce::jmin (lines.size(), p.lines.size()); ++i)
            {
                const auto& a = p.lines[i];
                const auto& b = lines[i];
                expectWithinAbsoluteError (b.start, a.start, 0.01);
                expectWithinAbsoluteError (b.length, a.length, 0.01);
                for (double t = 0.05; t < a.length - 0.05; t += 0.1)
                    expectWithinAbsoluteError (b.curvePitchAt (t), a.pitchAt (t), 0.12f);
            }
        }

        beginTest ("MIDI export (MPE): config, one note per line, bends, readable file");
        {
            const auto p = tools::makeFactoryPattern (2);   // chord glide pad: 3 long gliding lines
            auto file = tools::exportMidi (p, false, 48, 140.0);
            expectEquals (file.getNumTracks(), 1);

            juce::MemoryOutputStream mo;
            expect (file.writeTo (mo, 1));
            juce::MidiFile back;
            juce::MemoryInputStream mi (mo.getData(), mo.getDataSize(), false);
            expect (back.readFrom (mi));
            expectEquals (back.getTimeFormat(), (short) 960);

            const auto* seq = back.getTrack (0);
            int ons = 0, offs = 0, bends = 0, tempo = 0, mcm = 0;
            std::set<int> chans;
            double lastTime = 0.0;
            for (auto* e : *seq)
            {
                const auto& m = e->message;
                if (m.isTempoMetaEvent()) { ++tempo; expectWithinAbsoluteError (60.0 / m.getTempoSecondsPerQuarterNote(), 140.0, 0.01); }
                if (m.isNoteOn()) { ++ons; chans.insert (m.getChannel()); }
                if (m.isNoteOff()) ++offs;
                if (m.isPitchWheel()) ++bends;
                if (m.isController() && m.getChannel() == 1 && m.getControllerNumber() == 6 && m.getControllerValue() == 15) ++mcm;
                lastTime = juce::jmax (lastTime, m.getTimeStamp());
            }
            expectEquals (tempo, 1);
            expectEquals (ons, (int) p.lines.size());
            expectEquals (offs, ons);
            expect (bends > 100, "glides are written as pitch bend");
            expectEquals ((int) chans.size(), 3, "one member channel per line");
            for (int c : chans) expect (c >= 2 && c <= 16);
            expectEquals (mcm, 1, "MPE configuration message");
            expectWithinAbsoluteError (lastTime, p.lengthBeats() * 960.0, 1.0);
        }

        beginTest ("MIDI import: our own MPE export comes back as the same lines");
        {
            const auto p = tools::makeFactoryPattern (2);
            const auto file = tools::exportMidi (p, false, 48, 120.0);
            Pattern back;
            expect (tools::importMidi (file, 48, back));
            expectEquals (back.lines.size(), p.lines.size());
            expectEquals (back.bars, p.bars);
            for (size_t i = 0; i < juce::jmin (back.lines.size(), p.lines.size()); ++i)
                for (double t = 0.1; t < p.lines[i].length - 0.1; t += 0.25)
                    expectWithinAbsoluteError (back.lines[i].curvePitchAt (t), p.lines[i].pitchAt (t), 0.12f);
        }

        beginTest ("MIDI import: a plain piano-roll chord on channel 1 becomes three lines");
        {
            juce::MidiMessageSequence seq;
            for (int n : { 60, 64, 67 })
            {
                auto on = juce::MidiMessage::noteOn (1, n, (juce::uint8) 100);  on.setTimeStamp (0.0);
                auto off = juce::MidiMessage::noteOff (1, n);                   off.setTimeStamp (960.0 * 2);
                seq.addEvent (on); seq.addEvent (off);
            }
            seq.updateMatchedPairs();
            juce::MidiFile f;
            f.setTicksPerQuarterNote (960);
            f.addTrack (seq);
            Pattern back;
            expect (tools::importMidi (f, 48, back));
            expectEquals ((int) back.lines.size(), 3);
            expectEquals (back.bars, 1);
            for (auto& l : back.lines) expectWithinAbsoluteError (l.length, 2.0, 1.0e-6);
        }

        beginTest ("MIDI export (mono): channel 1 only");
        {
            const auto p = tools::makeFactoryPattern (0);
            auto file = tools::exportMidi (p, true, 24, 120.0);
            int ons = 0;
            for (auto* e : *file.getTrack (0))
            {
                const auto& m = e->message;
                if (m.getChannel() > 0) expectEquals (m.getChannel(), 1);
                if (m.isNoteOn()) ++ons;
            }
            expect (ons >= (int) p.lines.size());
        }
    }
};

class GestureBakeTests final : public juce::UnitTest
{
public:
    GestureBakeTests() : juce::UnitTest ("Line sequencer: gesture baking + edit tools", "BendDemon") {}

    void runTest() override
    {
        EngineSettings s = plainSettings();

        beginTest ("Dive extends the line and bends down by the dive amount");
        {
            Pattern p;
            const auto id = p.add (lineOf (0.0, 1.0, { { 0.0, 60.0f } }));
            s.diveSemis = 12.0f; s.diveTimeMs = 400.0f; s.diveSync = false; s.diveCurve = Curve::ease;
            tools::bakeGesture (p, { id }, tools::Gesture::dive, s, 120.0);
            const auto& l = *p.find (id);
            expectWithinAbsoluteError (l.length, 1.8, 1.0e-6);       // 400 ms at 120 bpm = 0.8 beats
            expectWithinAbsoluteError (l.curvePitchAt (0.9), 60.0f, 0.01f);
            expectWithinAbsoluteError (l.curvePitchAt (1.8), 48.0f, 0.01f);
            expect (l.curvePitchAt (1.3) < 59.0f);
        }

        beginTest ("Rise with scale lock lands on a scale tone");
        {
            Pattern p;
            const auto id = p.add (lineOf (0.0, 1.0, { { 0.0, 60.0f } }));
            EngineSettings r = s;
            r.diveSemis = 6.0f; r.scaleLockOn = true; r.scaleKey = 0; r.scaleType = ScaleLock::major;
            tools::bakeGesture (p, { id }, tools::Gesture::rise, r, 120.0);
            const float end = p.find (id)->curvePitchAt (100.0);
            expect (std::abs (end - 65.0f) < 1.0e-4f || std::abs (end - 67.0f) < 1.0e-4f, "F# snapped to F or G");
        }

        beginTest ("Glide merges a run of lines into one legato line");
        {
            Pattern p;
            const auto a = p.add (lineOf (0.0, 1.0, { { 0.0, 60.0f } }));
            const auto b = p.add (lineOf (1.0, 1.0, { { 0.0, 63.0f } }));
            const auto c = p.add (lineOf (2.5, 1.0, { { 0.0, 67.0f } }));
            EngineSettings g = s; g.glideTimeMs = 100.0f; g.glideCurve = Curve::sCurve;
            const auto res = tools::bakeGesture (p, { a, b, c }, tools::Gesture::glide, g, 120.0);
            expectEquals ((int) p.lines.size(), 1);
            expectEquals ((int) res.size(), 1);
            const auto& l = p.lines.front();
            expectWithinAbsoluteError (l.start, 0.0, 1.0e-9);
            expectWithinAbsoluteError (l.length, 3.5, 1.0e-6);
            expectWithinAbsoluteError (l.curvePitchAt (0.9), 60.0f, 0.01f);
            expectWithinAbsoluteError (l.curvePitchAt (1.5), 63.0f, 0.01f);
            expectWithinAbsoluteError (l.curvePitchAt (3.0), 67.0f, 0.01f);
            const float mid = l.curvePitchAt (1.1);
            expect (mid > 60.2f && mid < 62.8f, "glide over 0.2 beats");
        }

        beginTest ("Chord Glide pairs voices by nearest pitch");
        {
            Pattern p;
            tools::IdList ids;
            for (float n : { 60.0f, 63.0f, 67.0f }) ids.push_back (p.add (lineOf (0.0, 2.0, { { 0.0, n } })));
            for (float n : { 58.0f, 62.0f, 65.0f, 70.0f }) ids.push_back (p.add (lineOf (2.0, 2.0, { { 0.0, n } })));
            tools::bakeGesture (p, ids, tools::Gesture::chordGlide, s, 120.0);
            expectEquals ((int) p.lines.size(), 4, "3 chains + the extra new note");
            int merged = 0;
            for (auto& l : p.lines)
                if (l.length > 3.0)
                {
                    ++merged;
                    const float from = l.curvePitchAt (0.0), to = l.curvePitchAt (3.5);
                    expect (std::abs (from - to) <= 3.0f, "nearest-pitch pairing");
                }
            expectEquals (merged, 3);
        }

        beginTest ("Vibrato, wobble, trill, shake");
        {
            Pattern p;
            const auto v = p.add (lineOf (0.0, 4.0, { { 0.0, 60.0f } }));
            const auto w = p.add (lineOf (0.0, 4.0, { { 0.0, 60.0f } }));
            const auto t = p.add (lineOf (0.0, 2.0, { { 0.0, 60.0f } }));
            const auto k = p.add (lineOf (0.0, 2.0, { { 0.0, 60.0f } }));
            EngineSettings g = s;
            g.vibDepthCents = 35.0f; g.vibRateHz = 6.0f; g.vibSync = false;
            g.wobbleSemis = 1.0f; g.wobbleDiv = 3;   // 1/4 = 1 beat period
            g.trillDiv = 9;                          // 1/16
            g.shakeCents = 20.0f; g.shakeRateHz = 14.0f;
            tools::bakeGesture (p, { v }, tools::Gesture::vibrato, g, 120.0);
            tools::bakeGesture (p, { w }, tools::Gesture::wobble, g, 120.0);
            tools::bakeGesture (p, { t }, tools::Gesture::trill, g, 120.0);
            tools::bakeGesture (p, { k }, tools::Gesture::shake, g, 120.0);

            expectWithinAbsoluteError (p.find (v)->vibDepth, 0.35f, 1.0e-6f);
            expectWithinAbsoluteError (p.find (v)->vibRate, 3.0f, 1.0e-4f);           // 6 Hz at 120 bpm
            expectWithinAbsoluteError (p.find (w)->curvePitchAt (0.25), 61.0f, 0.05f);
            expectWithinAbsoluteError (p.find (w)->curvePitchAt (0.75), 59.0f, 0.05f);
            expectWithinAbsoluteError (p.find (t)->curvePitchAt (0.1), 60.0f, 0.01f);
            expectWithinAbsoluteError (p.find (t)->curvePitchAt (0.37), 62.0f, 0.01f);  // second 1/16
            expectWithinAbsoluteError (p.find (t)->curvePitchAt (0.6), 60.0f, 0.01f);
            float mx = 0.0f;
            for (double x = 0.0; x < 2.0; x += 0.01) mx = juce::jmax (mx, std::abs (p.find (k)->curvePitchAt (x) - 60.0f));
            expect (mx > 0.05f && mx <= 0.2001f, "shake within its depth");
        }

        beginTest ("Transpose / quantize / duplicate / split / retime / smooth");
        {
            Pattern p;
            const auto a = p.add (lineOf (0.13, 1.0, { { 0.0, 60.0f }, { 0.5, 62.0f } }));
            tools::transpose (p, { a }, 12.0f);
            expectWithinAbsoluteError (p.find (a)->curvePitchAt (0.5), 74.0f, 1.0e-5f);
            tools::quantizeStarts (p, { a }, 0.25);
            expectWithinAbsoluteError (p.find (a)->start, 0.25, 1.0e-9);

            const auto dup = tools::duplicate (p, { a }, 4.0, -12.0f);
            expectEquals ((int) dup.size(), 1);
            expectWithinAbsoluteError (p.find (dup[0])->start, 4.25, 1.0e-9);
            expectWithinAbsoluteError (p.find (dup[0])->curvePitchAt (0.5), 62.0f, 1.0e-5f);

            const auto b = tools::split (p, a, 0.25);
            expect (b != 0);
            expectWithinAbsoluteError (p.find (a)->length, 0.25, 1.0e-9);
            expectWithinAbsoluteError (p.find (b)->start, 0.5, 1.0e-9);
            expectWithinAbsoluteError (p.find (b)->curvePitchAt (0.25), 74.0f, 1.0e-3f);

            auto l = lineOf (1.0, 2.0, { { 0.0, 60.0f }, { 1.0, 64.0f }, { 2.0, 64.0f } }, false);
            tools::retime (l, 1.5, 2.5);
            expectWithinAbsoluteError (l.curvePitchAt (0.0), 62.0f, 1.0e-4f);   // the shape stays in place
            expectWithinAbsoluteError (l.curvePitchAt (0.5), 64.0f, 1.0e-4f);
            expectWithinAbsoluteError (l.length, 1.0, 1.0e-9);

            Pattern q;
            Curve1D jag;
            juce::Random rng (7);
            for (int i = 0; i <= 64; ++i) jag.push_back ({ i / 32.0, 60.0f + rng.nextFloat() * 0.6f });
            const auto j = q.add (lineOf (0.0, 2.0, jag, false));
            tools::smooth (q, { j }, 0.6f);
            const auto& sm = *q.find (j);
            expect (sm.smooth && sm.pitch.size() < jag.size(), "fewer points, spline");
            float jump = 0.0f;
            for (double x = 0.01; x < 2.0; x += 0.01) jump = juce::jmax (jump, std::abs (sm.curvePitchAt (x) - sm.curvePitchAt (x - 0.01)));
            expect (jump < 0.1f, "no jagged steps left");
        }

        beginTest ("Magnet pulls held notes onto the scale and leaves steep glides smooth");
        {
            ScaleLock sc; sc.set (0, ScaleLock::major);
            Curve1D c { { 0.0, 60.4f }, { 1.0, 60.4f }, { 1.1, 63.3f }, { 1.2, 66.8f }, { 2.0, 66.8f } };
            tools::magnetize (c, sc, 1.0f);
            expectWithinAbsoluteError (c[0].v, 60.0f, 1.0e-4f);
            expectWithinAbsoluteError (c[1].v, 60.0f, 1.0e-4f);
            expect (std::abs (c[2].v - 63.3f) < 0.2f, "mid-glide point barely moves");
            expectWithinAbsoluteError (c[4].v, 67.0f, 1.0e-4f);

            Curve1D d { { 0.0, 60.4f } };
            tools::magnetize (d, sc, 0.5f);
            expectWithinAbsoluteError (d[0].v, 60.2f, 1.0e-4f);
        }
    }
};

class InternalSynthTests final : public juce::UnitTest
{
public:
    InternalSynthTests() : juce::UnitTest ("Internal synth", "BendDemon") {}

    static double estimateHz (const juce::AudioBuffer<float>& b, int start, int len)
    {
        const float* d = b.getReadPointer (0);
        int crossings = 0, first = -1, last = -1;
        for (int i = start + 1; i < start + len; ++i)
            if (d[i - 1] < 0.0f && d[i] >= 0.0f)
            {
                if (first < 0) first = i;
                last = i;
                ++crossings;
            }
        return crossings > 1 ? (crossings - 1) * kSR / (double) (last - first) : 0.0;
    }

    void runTest() override
    {
        beginTest ("Every sound makes finite, audible sound and releases to silence");
        for (int snd = 0; snd < InternalSynth::numSounds; ++snd)
        {
            InternalSynth synth;
            synth.prepare (kSR, 512);
            synth.setSound (snd);
            synth.setBendRanges (48.0f, 2.0f, false);

            juce::AudioBuffer<float> out (2, (int) kSR * 3);
            out.clear();
            for (int pos = 0; pos < out.getNumSamples(); pos += 512)
            {
                juce::MidiBuffer m;
                if (pos == 0)            m.addEvent (juce::MidiMessage::noteOn (2, 57, (juce::uint8) 110), 0);
                if (pos == 512 * 60)     m.addEvent (juce::MidiMessage::noteOff (2, 57), 0);
                synth.render (out, pos, juce::jmin (512, out.getNumSamples() - pos), m);
            }

            bool finite = true;
            float peak = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < out.getNumSamples(); ++i)
                {
                    const float x = out.getSample (ch, i);
                    finite = finite && std::isfinite (x);
                    peak = juce::jmax (peak, std::abs (x));
                }
            const juce::String snd_ = InternalSynth::getSoundName (snd);
            expect (finite, snd_ + " finite");
            expect (out.getRMSLevel (0, 0, (int) (kSR * 0.4)) > 0.01f, snd_ + " audible");
            expect (peak <= 1.0f, snd_ + " soft-clipped");
            expectEquals (synth.getNumActiveVoices(), 0, snd_ + " released");
        }

        beginTest ("Pitch follows the MPE channel bend (sub bass: sine)");
        {
            InternalSynth synth;
            synth.prepare (kSR, 512);
            synth.setSound (InternalSynth::subBass);
            synth.setBendRanges (48.0f, 2.0f, false);
            juce::AudioBuffer<float> out (2, (int) kSR);
            out.clear();
            for (int pos = 0; pos < out.getNumSamples(); pos += 512)
            {
                juce::MidiBuffer m;
                if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (3, 45, (juce::uint8) 100), 0);   // A2 = 110 Hz
                if (pos == 512 * 40) m.addEvent (juce::MidiMessage::pitchWheel (3, MpeOutput::semitonesToPitchWheel (12.0f, 48.0f)), 0);
                synth.render (out, pos, juce::jmin (512, out.getNumSamples() - pos), m);
            }
            expectWithinAbsoluteError (estimateHz (out, 4000, 12000), 110.0, 2.0);
            expectWithinAbsoluteError (estimateHz (out, 26000, 16000), 220.0, 3.0);
        }

        beginTest ("Legacy mode: channel 1 bend uses the member range");
        {
            InternalSynth synth;
            synth.prepare (kSR, 512);
            synth.setSound (InternalSynth::subBass);
            synth.setBendRanges (12.0f, 2.0f, true);
            juce::AudioBuffer<float> out (2, 24000);
            out.clear();
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::pitchWheel (1, MpeOutput::semitonesToPitchWheel (12.0f, 12.0f)), 0);
            m.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
            for (int pos = 0; pos < 24000; pos += 512)
            {
                synth.render (out, pos, juce::jmin (512, 24000 - pos), m);
                m.clear();
            }
            expectWithinAbsoluteError (estimateHz (out, 4000, 16000), 220.0, 3.0);
        }
    }
};

class CaptureRecorderTests final : public juce::UnitTest
{
public:
    CaptureRecorderTests() : juce::UnitTest ("Line sequencer: capture recorder", "BendDemon") {}

    static tools::CapturedEvent ev (double ppq, uint8_t type, uint8_t note, uint8_t ch = 2)
    {
        tools::CapturedEvent e; e.ppq = ppq; e.type = type; e.channel = ch; e.note = note; e.velocity = 100;
        return e;
    }

    void runTest() override
    {
        beginTest ("Capture last N bars uses the newest pass, bar aligned");
        {
            CaptureRecorder rec;
            // pass 1 (0..16), then the host loops back and plays 0..9.5
            rec.add (ev (1.0, tools::CapturedEvent::noteOn, 60));
            rec.add (ev (2.0, tools::CapturedEvent::noteOff, 60));
            rec.add (ev (15.0, tools::CapturedEvent::noteOn, 62));
            rec.add (ev (15.9, tools::CapturedEvent::noteOff, 62));
            rec.add (ev (0.5, tools::CapturedEvent::noteOn, 64));
            rec.add (ev (1.5, tools::CapturedEvent::noteOff, 64));
            rec.add (ev (9.0, tools::CapturedEvent::noteOn, 67));
            rec.add (ev (9.5, tools::CapturedEvent::noteOff, 67));

            std::vector<PatternLine> lines;
            double len = 0.0;
            expect (rec.captureLast (2, 4, 48, lines, len));
            expectWithinAbsoluteError (len, 8.0, 1.0e-9);   // bars 1..2 of the running pass (ends at bar 3 = beat 8)
            expectEquals ((int) lines.size(), 1);
            expectWithinAbsoluteError (lines[0].startPitch(), 64.0f, 1.0e-4f);
            expectWithinAbsoluteError (lines[0].start, 0.5, 1.0e-9);

            expect (rec.captureLast (4, 4, 48, lines, len));
            expectWithinAbsoluteError (len, 8.0, 1.0e-9);   // the pass only covers 2 full bars so far
        }

        beginTest ("Armed capture records exactly one pass from the next boundary");
        {
            CaptureRecorder rec;
            std::vector<PatternLine> out;
            rec.arm (4.0);
            expect (! rec.poll (1.0, true, 48, out));
            expectWithinAbsoluteError (rec.getRecordStart(), 4.0, 1.0e-9);
            rec.add (ev (2.0, tools::CapturedEvent::noteOn, 50));     // before the pass: ignored
            rec.add (ev (3.0, tools::CapturedEvent::noteOff, 50));
            expect (! rec.poll (4.05, true, 48, out));
            expect (rec.isRecording());
            rec.add (ev (4.5, tools::CapturedEvent::noteOn, 60));
            rec.add (ev (6.0, tools::CapturedEvent::noteOff, 60));
            expect (! rec.poll (7.0, true, 48, out));
            expect (rec.poll (8.2, true, 48, out));
            expect (! rec.isArmed());
            expectEquals ((int) out.size(), 1);
            expectWithinAbsoluteError (out[0].start, 0.5, 1.0e-9);
            expectWithinAbsoluteError (out[0].length, 1.5, 1.0e-9);
        }

        beginTest ("Armed capture finishes on a host loop back to the start");
        {
            CaptureRecorder rec;
            std::vector<PatternLine> out;
            rec.arm (4.0);
            expect (! rec.poll (0.0, true, 48, out));
            expect (! rec.poll (0.1, true, 48, out));
            expect (rec.isRecording());
            rec.add (ev (1.0, tools::CapturedEvent::noteOn, 61));
            rec.add (ev (3.95, tools::CapturedEvent::noteOff, 61));
            expect (! rec.poll (3.9, true, 48, out));
            expect (rec.poll (0.05, true, 48, out));      // FL wrapped to 0
            expectEquals ((int) out.size(), 1);
        }
    }
};

static CaptureRecorderTests captureRecorderTests;
static CurveTests          curveTests;
static PatternStateTests   patternStateTests;
static PlaybackTests       playbackTests;
static MonoModeTests       monoModeTests;
static CaptureExportTests  captureExportTests;
static GestureBakeTests    gestureBakeTests;
static InternalSynthTests  internalSynthTests;
