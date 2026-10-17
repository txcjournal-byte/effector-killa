/*
    TrapWin BEND DEMON
    EngineTypes.h - plain-data types shared by the gesture engine, the processor and the tests.

    Nothing in here touches the GUI or allocates; everything is safe to use on the audio thread.
*/
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

namespace bd
{

//==============================================================================
/** Shapes used for glides and dives. */
enum class Curve : int
{
    linear = 0,
    ease,      // ease-out (fast start, soft landing) - the classic "slide" feel
    sCurve     // smoothstep in/out
};

/** Maps a normalised time 0..1 to a normalised progress 0..1 through the given curve. */
inline float applyCurve (Curve c, float x) noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);

    switch (c)
    {
        case Curve::linear: return x;
        case Curve::ease:   return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);   // cubic ease-out
        case Curve::sCurve: return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);   // smootherstep
    }

    return x;
}

//==============================================================================
/** Shapes for the per-note pressure / timbre envelopes. */
enum class EnvShape : int
{
    attack = 0,  // quick rise to full, then hold
    swell,       // slow S-shaped rise from a low level to full
    decay        // starts at full and decays to ~30 %
};

/** Evaluates an envelope shape. t = seconds since note start, time = shape time in seconds. Result 0..1. */
inline float evalEnvShape (EnvShape s, float t, float time) noexcept
{
    const float x = time <= 0.0001f ? 1.0f : juce::jlimit (0.0f, 1.0f, t / time);

    switch (s)
    {
        case EnvShape::attack: return applyCurve (Curve::ease, x);
        case EnvShape::swell:  return 0.25f + 0.75f * applyCurve (Curve::sCurve, x);
        case EnvShape::decay:  return 1.0f - 0.7f * applyCurve (Curve::ease, x);
    }

    return 1.0f;
}

//==============================================================================
/** Tempo-sync divisions, shared by vibrato / dive / wobble / trill. */
inline constexpr int numSyncDivisions = 12;

inline const char* const syncDivisionNames[numSyncDivisions] =
    { "1/1", "1/2", "1/2.", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16", "1/16T", "1/32" };

/** Length of a division in quarter-note beats. */
inline double syncDivisionBeats (int index) noexcept
{
    static constexpr double beats[numSyncDivisions] =
        { 4.0, 2.0, 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

    return beats[(size_t) juce::jlimit (0, numSyncDivisions - 1, index)];
}

/** Allowed per-note pitch-bend ranges (semitones). */
inline constexpr int numBendRanges = 5;
inline constexpr int bendRanges[numBendRanges] = { 2, 12, 24, 48, 96 };

//==============================================================================
/** All values the engine needs, snapshotted from the parameters once per block. */
struct EngineSettings
{
    // MPE layout
    int   numVoices        = 15;     // member channels 2..(1 + numVoices), 1..15
    int   bendRange        = 48;     // per-note pitch bend range in semitones
    int   liveChannels     = 0;      // 2.0: member channels the gesture engine may use (2..1+n), 0 = all numVoices
    int   masterBendRange  = 2;      // RPN 0 sent on the master channel (Mono/Legacy mode uses bendRange)

    // GLIDE (mono legato) and CHORD GLIDE
    bool  glideOn          = true;
    bool  chordGlideOn     = false;
    float glideTimeMs      = 120.0f;
    float glideWindowMs    = 60.0f;  // max gap between note-off and next note-on that still glides
    Curve glideCurve       = Curve::ease;
    float chordWindowMs    = 12.0f;  // notes starting within this window are one chord (= lookahead latency)

    // VIBRATO
    bool  vibratoOn        = false;
    float vibRateHz        = 5.5f;
    bool  vibSync          = false;
    int   vibDiv           = 9;      // 1/16
    float vibDepthCents    = 35.0f;
    float vibDelayMs       = 350.0f; // note must be held this long before vibrato starts ramping in

    // 808 DIVE / RISE
    bool  diveOn           = false;
    bool  riseOn           = false;
    float diveSemis        = 12.0f;
    float diveTimeMs       = 400.0f;
    bool  diveSync         = false;
    int   diveDiv          = 6;      // 1/8
    Curve diveCurve        = Curve::ease;

    // WOBBLE / SHAKE / TRILL
    bool  wobbleOn         = false;
    float wobbleSemis      = 1.0f;
    int   wobbleDiv        = 3;      // 1/4
    bool  shakeOn          = false;
    float shakeCents       = 18.0f;
    float shakeRateHz      = 14.0f;
    bool  trillOn          = false;
    int   trillDiv         = 9;      // 1/16

    // SCALE LOCK
    bool  scaleLockOn      = false;
    int   scaleKey         = 0;      // 0 = C
    int   scaleType        = 1;      // see ScaleLock::ScaleType
    bool  quantizeInput    = false;

    // PRESSURE / TIMBRE
    bool     pressureOn     = true;
    float    pressureAmount = 0.8f;   // 0..1
    float    pressureTimeMs = 250.0f;
    EnvShape pressureShape  = EnvShape::attack;
    float    velToPressure  = 0.5f;   // 0..1
    bool     timbreOn       = true;
    float    timbreAmount   = 0.5f;   // 0..1
    float    timbreTimeMs   = 600.0f;
    EnvShape timbreShape    = EnvShape::swell;
    float    velToTimbre    = 0.3f;
};

/** Host transport info for one block (taken from the AudioPlayHead). */
struct TransportState
{
    bool   hasInfo   = false;
    bool   isPlaying = false;
    double bpm       = 120.0;
    double ppq       = 0.0;   // ppq position at the start of the block
    int    timeSigNumerator   = 4;
    int    timeSigDenominator = 4;
};

} // namespace bd
