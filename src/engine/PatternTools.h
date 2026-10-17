/*
    TrapWin BEND DEMON
    PatternTools.h - pure functions on Patterns (no GUI, unit tested):

      * edit commands      transpose, quantize start, smooth, magnet (scale pull), duplicate
      * gesture baking     the 1.x gesture shapes (Glide, Chord Glide, Vibrato, 808 Dive, Rise,
                           Wobble, Shake, Trill) as one-click curve generators on selected lines
      * capture            converts recorded MPE (what the live gesture engine sent: notes + per
                           channel bend / pressure / CC74) into editable lines
      * MIDI export        renders the pattern through the real PatternPlayer into a .mid file
                           (MPE: per-channel notes + bend + pressure + CC74, or mono pitch-bend)
      * factory patterns   the demo patterns shipped with the presets
*/
#pragma once

#include "Pattern.h"
#include "EngineTypes.h"
#include "ScaleLock.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace bd::tools
{

using IdList = std::vector<uint32_t>;

//==============================================================================
// Edit commands (all return true if something changed)
bool transpose (Pattern&, const IdList&, float semitones);
bool quantizeStarts (Pattern&, const IdList&, double gridBeats);
bool smooth (Pattern&, const IdList&, float amount = 0.5f);

/** Pulls curve points towards scale tones. Points on steep glides are pulled less, so glides
    stay smooth while the notes they connect land on the scale. strength 0..1. */
void magnetize (Curve1D& pitch, const ScaleLock&, float strength);

/** Copies the lines, shifted by `offsetBeats` (and `semitones`). Returns the new ids. */
IdList duplicate (Pattern&, const IdList&, double offsetBeats, float semitones = 0.0f);

/** Splits off the part of `line` after `atBeatRel` (relative to its start). Returns the new
    line's id or 0. */
uint32_t split (Pattern&, uint32_t lineId, double atBeatRel);

/** A plain held note (one pitch point). */
PatternLine makeNote (double start, double length, float pitch, float velocity = 0.8f);

/** Trims/extends a line: newStart/newEnd in absolute beats; curves are re-based so the drawn
    shape stays where it is in time. */
void retime (PatternLine&, double newStart, double newEnd);

//==============================================================================
enum class Gesture { glide = 0, chordGlide, vibrato, dive, rise, wobble, shake, trill, numGestures };
const char* gestureName (Gesture);

/** Bakes a gesture into the given lines using the 1.x gesture parameters (`s`) at `bpm`.
    Returns the ids that should be selected afterwards (glides merge lines). */
IdList bakeGesture (Pattern&, const IdList&, Gesture, const EngineSettings& s, double bpm);

//==============================================================================
/** One recorded MPE event (what the live engine sent), timestamped in ppq. */
struct CapturedEvent
{
    enum Type : uint8_t { noteOn = 0, noteOff, bend, pressure, timbre };

    double  ppq     = 0.0;
    uint8_t type    = noteOn;
    uint8_t channel = 1;     // 1..16
    uint8_t note    = 0;
    uint8_t velocity = 0;
    int16_t value   = 0;     // bend 0..16383, pressure / timbre 0..127
};

/** Converts the events into lines inside [startPpq, startPpq + lengthBeats). Notes are clipped to
    the window; bends are expressed in semitones with `bendRange`. Master-channel (1) events are
    ignored unless `includeChannel1` (mono captures). */
std::vector<PatternLine> linesFromCapture (const std::vector<CapturedEvent>&, double startPpq, double lengthBeats,
                                           int bendRange, bool includeChannel1 = false);

//==============================================================================
/** Renders one pass of the pattern to a type-1 MIDI file (one track, tempo + time signature,
    960 ticks per quarter). mono = channel 1 pitch-bend variant (bend range RPN sent on ch 1). */
juce::MidiFile exportMidi (const Pattern&, bool mono, int bendRange, double bpm);

//==============================================================================
int numFactoryPatterns();
const char* factoryPatternName (int index);
Pattern makeFactoryPattern (int index);

} // namespace bd::tools
