/*
    TrapWin BEND DEMON
    HostKeys.h - make sure the space bar (and other keys we don't use) reach the host.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bd::hostkeys
{
    /** Gives the keyboard back to the host window and replays `key` there.
        Windows: SetFocus to the host's top-level window + SendInput of the key.
        Returns true if the key was forwarded (caller should consume it), false if the
        platform passes unused keys on by itself (macOS) and the caller should return false. */
    bool forwardToHost (juce::Component& anyComponentOfThePlugin, const juce::KeyPress& key);

    /** Only gives the keyboard back to the host window (no key replay). */
    void returnFocusToHost (juce::Component& anyComponentOfThePlugin);
}
