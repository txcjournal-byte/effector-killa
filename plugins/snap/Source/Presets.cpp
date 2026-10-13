#include "Presets.h"

namespace snapfx
{

namespace
{
struct B
{
    Params p;
    B (float attack, float sustain, float speed = 0.5f) { p.attack = attack; p.sustain = sustain; p.speed = speed; }
    B& above (float hz) { p.band = Band::above; p.splitHz = hz; return *this; }
    B& below (float hz) { p.band = Band::below; p.splitHz = hz; return *this; }
    B& look() { p.lookahead = true; return *this; }
    B& clip (float ceilingDb) { p.clip = true; p.ceilingDb = ceilingDb; return *this; }
    B& mix (float m) { p.mix = m; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    operator Params() const { return p; }
};
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",      "Init (Neutral)",          B (0.0f, 0.0f) },

        { "Kick",      "Kick Knock",              B (0.55f, -0.15f, 0.6f).clip (-0.5f) },
        { "Kick",      "Click Up Top",            B (0.8f, 0.0f, 0.75f).above (2500.0f).clip (-0.3f) },
        { "Kick",      "Tight Kick Tail",         B (0.2f, -0.6f, 0.55f).out (1.0f) },
        { "Kick",      "Boom Bap Thump",          B (0.35f, 0.3f, 0.4f).below (160.0f) },
        { "Kick",      "Soft Pillow Kick",        B (-0.45f, 0.15f, 0.45f).look() },

        { "Snare",     "Snare Crack",             B (0.7f, -0.2f, 0.7f).clip (-0.5f) },
        { "Snare",     "Fat Snare Body",          B (0.15f, 0.55f, 0.45f).below (1200.0f) },
        { "Snare",     "Dry Clap Snap",           B (0.5f, -0.7f, 0.65f) },
        { "Snare",     "Room Snare Bloom",        B (0.0f, 0.8f, 0.4f).mix (0.7f) },
        { "Snare",     "Rimshot Edge",            B (0.9f, -0.35f, 0.85f).above (1800.0f).look().clip (-1.0f) },

        { "Hats",      "Hat Sparkle",             B (0.45f, -0.3f, 0.8f).above (6000.0f) },
        { "Hats",      "Short Hats",              B (0.1f, -0.85f, 0.75f) },
        { "Hats",      "Smooth Hat Roll",         B (-0.5f, 0.0f, 0.7f).look() },
        { "Hats",      "Open Hat Wash",           B (0.0f, 0.6f, 0.5f).above (5000.0f).mix (0.8f) },

        { "808",       "808 Punch In",            B (0.6f, 0.0f, 0.55f).look().clip (-0.3f) },
        { "808",       "808 Short Cut",           B (0.15f, -0.65f, 0.4f).below (250.0f) },
        { "808",       "808 Long Glide",          B (0.0f, 0.55f, 0.35f).below (150.0f).clip (-0.5f) },
        { "808",       "808 Click Tamer",         B (-0.6f, 0.0f, 0.6f).above (900.0f).look() },
        { "808",       "Distorted 808 Bite",      B (0.5f, 0.2f, 0.5f).above (400.0f).clip (-1.5f).out (1.0f) },

        { "Drum Bus",  "Bus Punch",               B (0.35f, -0.1f, 0.5f).mix (0.6f).clip (-0.3f) },
        { "Drum Bus",  "Bus Tighten",             B (0.1f, -0.45f, 0.45f).mix (0.75f) },
        { "Drum Bus",  "Loop Smack",              B (0.65f, 0.0f, 0.6f).look().clip (-0.8f).mix (0.8f) },
        { "Drum Bus",  "Lo-Fi Loop Breathe",      B (-0.25f, 0.5f, 0.35f).mix (0.7f) },
        { "Drum Bus",  "Highs Only Snap",         B (0.5f, -0.2f, 0.6f).above (3000.0f).mix (0.85f) },

        { "Vocals",    "Vocal Upfront",           B (0.3f, -0.15f, 0.55f).above (1500.0f).look() },
        { "Vocals",    "De-Room Vocal",           B (0.0f, -0.55f, 0.45f) },
        { "Vocals",    "Soft Consonants",         B (-0.4f, 0.0f, 0.75f).above (3000.0f).look() },
        { "Vocals",    "Ad-Lib Bloom",            B (0.0f, 0.6f, 0.4f).mix (0.8f) },
        { "Vocals",    "Rap Bite",                B (0.45f, -0.2f, 0.7f).above (2000.0f).clip (-1.0f) },
    };
    return presets;
}

} // namespace snapfx
