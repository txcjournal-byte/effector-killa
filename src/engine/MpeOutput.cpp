#include "MpeOutput.h"
#include <cmath>

namespace bd
{

int MpeOutput::semitonesToPitchWheel (float semitones, float range) noexcept
{
    if (range <= 0.0f)
        return 8192;

    const float norm = semitones / range;
    const int value = norm >= 0.0f ? 8192 + (int) std::lround (norm * 8191.0f)
                                   : 8192 + (int) std::lround (norm * 8192.0f);
    return juce::jlimit (0, 16383, value);
}

float MpeOutput::pitchWheelToSemitones (int value, float range) noexcept
{
    const int d = value - 8192;
    return d >= 0 ? (float) d / 8191.0f * range
                  : (float) d / 8192.0f * range;
}

int MpeOutput::addRpn (juce::MidiBuffer& out, int pos, int maxPos, int channel, int rpnNumber, int value)
{
    auto add = [&] (const juce::MidiMessage& m) { out.addEvent (m, juce::jmin (pos++, maxPos)); };

    add (juce::MidiMessage::controllerEvent (channel, 101, (rpnNumber >> 7) & 0x7f));
    add (juce::MidiMessage::controllerEvent (channel, 100, rpnNumber & 0x7f));
    // Data entry MSB only. Do NOT send CC38 (LSB): receivers that treat RPN data as 14-bit
    // (juce::MPEInstrument, several synths) would read 48 semitones as 48 * 128.
    add (juce::MidiMessage::controllerEvent (channel, 6, juce::jlimit (0, 127, value)));
    // RPN null
    add (juce::MidiMessage::controllerEvent (channel, 101, 127));
    add (juce::MidiMessage::controllerEvent (channel, 100, 127));
    return pos;
}

int MpeOutput::addZoneConfiguration (juce::MidiBuffer& out, int pos, int maxPos,
                                     int numMembers, int memberBendRange, int masterBendRange)
{
    numMembers = juce::jlimit (1, 15, numMembers);
    maxPos = juce::jmax (pos, maxPos);

    // Master channel: MCM first (per spec it resets member bend sensitivity to 48, so the
    // per-channel ranges below must come after it), then the master bend range.
    int p = addRpn (out, pos, maxPos, masterChannel, 6, numMembers);
    p = addRpn (out, p, maxPos, masterChannel, 0, masterBendRange);
    int span = p - pos;

    // Member channels run in parallel (different channels never interfere), but after the MCM
    // in time, so they start at pos + 5.
    for (int ch = 2; ch <= 1 + numMembers; ++ch)
    {
        int q = addRpn (out, pos + 5, maxPos, ch, 0, memberBendRange);
        out.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), juce::jmin (q++, maxPos));
        out.addEvent (juce::MidiMessage::channelPressureChange (ch, 0), juce::jmin (q++, maxPos));
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 74, 0), juce::jmin (q++, maxPos));
        span = juce::jmax (span, q - pos);
    }

    jassert (span <= zoneConfigurationSpan);
    return span;
}

void MpeOutput::addFullReset (juce::MidiBuffer& out, int pos)
{
    for (int ch = 1; ch <= 16; ++ch)
    {
        out.addEvent (juce::MidiMessage::allNotesOff (ch), pos);
        out.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), pos);
        out.addEvent (juce::MidiMessage::channelPressureChange (ch, 0), pos);
    }
}

} // namespace bd
