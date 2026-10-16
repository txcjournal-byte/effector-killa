/*
    TrapWin BEND DEMON
    MpeOutput.h - stateless helpers that build MPE MIDI messages.

    Pitch-bend convention (14-bit, centre 8192):
        semis >= 0 : 8192 + round (semis / range * 8191)   -> +range = 16383
        semis <  0 : 8192 + round (semis / range * 8192)   -> -range = 0
    so both extremes are reachable and 0 semitones is exactly 8192.
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace bd
{

struct MpeOutput
{
    static constexpr int masterChannel = 1;

    /** Semitones -> 14-bit pitch-wheel value for a given +/- range. Clamped to 0..16383. */
    static int semitonesToPitchWheel (float semitones, float rangeSemitones) noexcept;

    /** Inverse of semitonesToPitchWheel (used by tests and the UI). */
    static float pitchWheelToSemitones (int value, float rangeSemitones) noexcept;

    /** Appends the full lower-zone setup to `out`, starting at sample position `pos`:
          - MPE Configuration Message (RPN 6) on the master channel = number of member channels
          - RPN 0 (pitch-bend sensitivity) = `memberBendRange` on EVERY member channel 2..(1+n)
            (MPE only requires it once, but sending it per channel also configures synths that
            treat every channel independently)
          - RPN 0 = `masterBendRange` on the master channel
          - per member channel: pitch bend centre, pressure 0, CC74 = 0
        Each RPN is followed by an RPN-null (101/100 = 127) so later data-entry CCs are harmless.

        IMPORTANT (VST3): when a VST3 plugin is hosted, MIDI CCs travel as *parameter changes*
        and the receiving side rebuilds MIDI parameter by parameter. Messages that share a sample
        offset therefore come out grouped by CC number (all CC101, then all CC100, then CC6),
        which destroys RPN sequences. So every message of a channel's sequence gets its OWN
        sample offset: pos, pos+1, pos+2 ... Returns the number of samples the sequence spans
        (notes should not start before pos + returned value). Offsets are clamped to maxPos. */
    static int addZoneConfiguration (juce::MidiBuffer& out, int pos, int maxPos,
                                     int numMemberChannels, int memberBendRange,
                                     int masterBendRange = 2);

    /** Note-offs are handled by the engine; this appends "all sound off + all notes off +
        reset controllers" style cleanup for every channel 1..16 (used by panic). */
    static void addFullReset (juce::MidiBuffer& out, int pos);

    /** Helper: RPN (CC101/CC100 number, CC6 7-bit value, no CC38) followed by RPN null.
        Messages go to pos, pos+1, ... (clamped to maxPos); returns the next free position. */
    static int addRpn (juce::MidiBuffer& out, int pos, int maxPos, int channel, int rpnNumber, int value);

    /** Number of sample offsets a zone configuration needs (see addZoneConfiguration). */
    static constexpr int zoneConfigurationSpan = 13;
};

} // namespace bd
