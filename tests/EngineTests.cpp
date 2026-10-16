/*
    TrapWin BEND DEMON - engine unit tests.

    Covers: channel allocation, 14-bit pitch-bend maths, MPE zone configuration, mono glide,
    chord-glide voice matching, 808 dive timing + delayed note-off, scale snapping and
    "no stuck notes" (randomised fuzz + release-all).
*/
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "engine/GestureEngine.h"

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
                    t += rng.nextInt (ms (60));
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
