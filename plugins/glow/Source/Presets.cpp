#include "Presets.h"

namespace glowfx
{

namespace
{
struct B
{
    Params p;
    B (Mode m, float glow, float contrast = 0.0f, float colour = 0.0f)
    {
        p.mode = m; p.glow = glow; p.contrast = contrast; p.colour = colour; p.outputDb = 0.0f; p.autoGain = true;
    }
    B& out (float db) { p.outputDb = db; return *this; }
    B& raw() { p.autoGain = false; return *this; }
    operator Params() const { return p; }
};
constexpr auto WA = Mode::warm;
constexpr auto BR = Mode::bright;
constexpr auto FA = Mode::fat;
constexpr auto LO = Mode::loud;
} // namespace

// contrast / colour are -1..1 (shown as -100..+100 in the plugin)
const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",        "Init (Warm 50)",         B (WA, 0.50f) },

        { "Vocals",      "Vocal Sheen",            B (BR, 0.45f, 0.10f, 0.20f) },
        { "Vocals",      "Front Row Vocal",        B (WA, 0.60f, 0.25f, 0.10f) },
        { "Vocals",      "Silk Top",               B (BR, 0.35f, -0.20f, 0.35f) },
        { "Vocals",      "Ad-Lib Pop Out",         B (LO, 0.40f, 0.40f, 0.30f) },
        { "Vocals",      "Whisper Lift",           B (WA, 0.75f, -0.40f, 0.15f) },
        { "Vocals",      "Melodic Hook Gloss",     B (BR, 0.60f, 0.00f, 0.10f) },

        { "808 & Bass",  "808 Thicken",            B (FA, 0.55f, 0.10f, -0.10f) },
        { "808 & Bass",  "Phone Speaker 808",      B (FA, 0.85f, 0.20f, 0.30f) },
        { "808 & Bass",  "Sub Warmth",             B (WA, 0.45f, -0.10f, -0.35f) },
        { "808 & Bass",  "Distorted Glide Glue",   B (LO, 0.50f, -0.20f, -0.10f) },
        { "808 & Bass",  "Reese Body",             B (FA, 0.40f, -0.30f, 0.10f) },

        { "Drums",       "Knock Harder",           B (FA, 0.50f, 0.60f, 0.00f) },
        { "Drums",       "Snare Crack",            B (BR, 0.55f, 0.70f, 0.15f) },
        { "Drums",       "Hi-Hat Shine",           B (BR, 0.45f, 0.30f, 0.45f) },
        { "Drums",       "Drum Bus Smash",         B (LO, 0.70f, 0.30f, 0.00f) },
        { "Drums",       "Lo-Fi Kit Warmth",       B (WA, 0.65f, -0.30f, -0.50f) },

        { "Melodies",    "Piano Glow",             B (WA, 0.40f, 0.00f, 0.05f) },
        { "Melodies",    "Bell Air",               B (BR, 0.50f, 0.10f, 0.40f) },
        { "Melodies",    "Pad Bloom",              B (WA, 0.70f, -0.50f, -0.15f) },
        { "Melodies",    "Guitar Loop Body",       B (FA, 0.35f, 0.00f, 0.00f) },
        { "Melodies",    "Rage Lead Push",         B (LO, 0.60f, 0.20f, 0.25f) },
        { "Melodies",    "Dusty Sample Revive",    B (BR, 0.65f, 0.15f, 0.30f) },

        { "Mix Bus",     "Beat Polish",            B (WA, 0.30f, 0.10f, 0.05f) },
        { "Mix Bus",     "Club Weight",            B (FA, 0.30f, 0.10f, -0.05f) },
        { "Mix Bus",     "Bright Master",          B (BR, 0.25f, 0.00f, 0.15f) },
        { "Mix Bus",     "Loud & Proud",           B (LO, 0.55f, 0.00f, 0.00f).raw() },
        { "Mix Bus",     "Demo Finisher",          B (LO, 0.35f, 0.15f, 0.10f).raw() },
        { "Mix Bus",     "Max Glow",               B (LO, 1.00f, 0.00f, 0.00f) },
    };
    return presets;
}

} // namespace glowfx
