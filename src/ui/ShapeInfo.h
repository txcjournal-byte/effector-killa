/*
    TrapWin BEND DEMON
    ShapeInfo.h - plain-language names / hints for the 8 gesture shapes (2.0 UI).
*/
#pragma once

#include "GesturePad.h"
#include "../Parameters.h"

namespace bd
{

struct ShapeInfo
{
    const char* name;        // shown on buttons
    const char* hint;        // one line, plain words (status bar / tooltip)
    const char* liveHint;    // what the live version does with piano-roll notes
    GesturePad::Icon icon;
    const char* liveParam;
};

inline const ShapeInfo& shapeInfo (int i)
{
    static const ShapeInfo shapes[] = {
        { "Slide",       "SLIDE - joins the selected notes into one voice that slides from note to note",
                         "Piano-roll notes that touch or overlap slide into each other", GesturePad::Icon::glide, ids::glideOn },
        { "Chord Slide", "CHORD SLIDE - every voice of a chord slides to the nearest note of the next chord",
                         "Touching chords melt into each other voice by voice", GesturePad::Icon::chordGlide, ids::chordGlideOn },
        { "Vibrato",     "VIBRATO - adds a vibrato that fades in on the selected notes",
                         "Held notes get a vibrato that fades in", GesturePad::Icon::vibrato, ids::vibratoOn },
        { "Dive",        "DIVE - the pitch falls at the end of the selected notes",
                         "Every note bends down when it ends", GesturePad::Icon::dive, ids::diveOn },
        { "Rise",        "RISE - the pitch climbs at the end of the selected notes",
                         "Every note bends up when it ends", GesturePad::Icon::rise, ids::riseOn },
        { "Wobble",      "WOBBLE - slow, tempo-synced pitch wobble on the selected notes",
                         "Notes wobble in time with the beat", GesturePad::Icon::wobble, ids::wobbleOn },
        { "Shake",       "SHAKE - fast, small random pitch shake (nervous, vocal-like)",
                         "Notes shake slightly, like a nervous voice", GesturePad::Icon::shake, ids::shakeOn },
        { "Trill",       "TRILL - rapidly alternates with the next note of the scale",
                         "Notes trill to the next scale note", GesturePad::Icon::trill, ids::trillOn },
    };
    return shapes[juce::jlimit (0, 7, i)];
}

} // namespace bd
