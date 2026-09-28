#pragma once

// Snapshot of everything the TV picture needs for one frame (message thread -> GL thread).

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace ek::ui
{
struct TvFrame
{
    float time = 0.0f;           // seconds (UI clock)
    float beat = 0.0f;           // beats (host tempo), for pulsing
    int channel = 9;
    juce::Colour colour { 0xff6a5acd };
    float level = 0.0f;          // smoothed output level 0..1
    float transient = 0.0f;
    std::array<float, 5> macros { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

    float glitch = 0.0f;         // tearing (KILL, AV3 corner hit, CRASH OUT)
    float snow = 0.0f;           // channel switch noise
    float blue = 0.0f;           // NO SIGNAL blue screen
    float power = 1.0f;          // 1 = on, 0 = off (OFF AIR shrink animation)
    float pocket = 0.0f;         // POCKET TV shrink + blur
    float calm = 0.0f;           // BASIC CABLE: animations toned down

    int avMode = 1;
    bool dotVisible = false;
    juce::Point<float> dot { 0.5f, 0.5f };       // 0..1, y up
    std::vector<juce::Point<float>> path;         // AV2 drawing (0..1, y up)
    bool logoVisible = false;
    juce::Point<float> logo { 0.5f, 0.5f };       // AV3 logo centre, 0..1 (y up)
    bool osdDirty = true;
};
} // namespace ek::ui
