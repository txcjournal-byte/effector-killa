#include "Presets.h"

namespace throwfx
{

namespace
{
// division index helpers (see divisionNames)
enum Div { d32T = 0, d32, d16T, d32D, d16, d8T, d16D, d8, d4T, d8D, d4, d2T, d4D, d2, d1T, d2D, d1 };

struct B
{
    Params p;
    B (int div, Mode m, float fb, float mix) { p.sync = true; p.division = div; p.mode = m; p.feedback = fb; p.mix = mix; }
    B (float ms, Mode m, float fb, float mix) { p.sync = false; p.timeMs = ms; p.mode = m; p.feedback = fb; p.mix = mix; }
    B& tape (float drive, float wobble) { p.drive = drive; p.wobble = wobble; return *this; }
    B& filt (float lo, float hi) { p.lowCutHz = lo; p.highCutHz = hi; return *this; }
    B& duck (float d) { p.duck = d; return *this; }
    B& width (float w) { p.width = w; return *this; }
    B& throwOnly() { p.throwOnly = true; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    operator Params() const { return p; }
};
constexpr auto ST = Mode::stereo;
constexpr auto PP = Mode::pingPong;
constexpr auto MO = Mode::mono;
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",        "Init (1/4 Stereo)",        B (d4, ST, 0.35f, 0.5f) },

        { "Vocal Throws", "End-Of-Bar Throw",        B (d4, PP, 0.45f, 0.5f).filt (250, 6500).tape (0.25f, 0.15f).duck (0.25f).throwOnly() },
        { "Vocal Throws", "Dotted Eighth Throw",     B (d8D, PP, 0.42f, 0.5f).filt (300, 7000).tape (0.2f, 0.1f).duck (0.2f).throwOnly() },
        { "Vocal Throws", "Long Tail Throw",         B (d2, ST, 0.62f, 0.5f).filt (220, 5200).tape (0.3f, 0.2f).width (1.4f).throwOnly() },
        { "Vocal Throws", "Hook Lift Wide",          B (d4, PP, 0.5f, 0.5f).filt (350, 9000).tape (0.15f, 0.1f).width (1.8f).duck (0.3f).throwOnly() },
        { "Vocal Throws", "Dark Room Throw",         B (d4D, ST, 0.55f, 0.5f).filt (180, 3200).tape (0.45f, 0.25f).throwOnly() },
        { "Vocal Throws", "Last Word Repeat",        B (d8, MO, 0.58f, 0.5f).filt (400, 6000).tape (0.2f, 0.05f).throwOnly() },

        { "Ad-Libs",     "Ad-Lib Ping",              B (d8, PP, 0.38f, 0.4f).filt (450, 8500).duck (0.45f) },
        { "Ad-Libs",     "Ad-Lib Triplets",          B (d8T, PP, 0.4f, 0.4f).filt (500, 8000).duck (0.4f).tape (0.1f, 0.05f) },
        { "Ad-Libs",     "Ad-Lib Bounce 1/16D",      B (d16D, PP, 0.45f, 0.38f).filt (600, 9000).duck (0.5f).width (1.6f) },
        { "Ad-Libs",     "Ad-Lib Haze",              B (d4D, ST, 0.5f, 0.42f).filt (300, 4500).tape (0.4f, 0.45f).duck (0.55f).width (1.5f) },
        { "Ad-Libs",     "Hype Man Stutter",         B (d16, MO, 0.55f, 0.45f).filt (700, 7000).duck (0.2f) },

        { "Slapback",    "Rap Slap 90",              B (90.0f, MO, 0.08f, 0.3f).filt (200, 7500).tape (0.25f, 0.05f) },
        { "Slapback",    "Tape Slap 120",            B (120.0f, ST, 0.15f, 0.32f).filt (150, 6000).tape (0.55f, 0.25f) },
        { "Slapback",    "Double Up 45",             B (45.0f, ST, 0.0f, 0.35f).filt (250, 9000).width (2.0f).tape (0.1f, 0.35f) },
        { "Slapback",    "Phone Slap",               B (110.0f, MO, 0.2f, 0.4f).filt (900, 3000).tape (0.6f, 0.1f) },

        { "Ping-Pong",   "Ping-Pong 1/8",            B (d8, PP, 0.45f, 0.45f).filt (200, 10000) },
        { "Ping-Pong",   "Ping-Pong 1/4 Ducked",     B (d4, PP, 0.5f, 0.5f).filt (250, 8000).duck (0.6f) },
        { "Ping-Pong",   "Triplet Ping-Pong",        B (d4T, PP, 0.48f, 0.45f).filt (220, 8500).duck (0.3f) },
        { "Ping-Pong",   "Wide 1/16 Bounce",         B (d16, PP, 0.55f, 0.35f).filt (500, 9500).width (2.0f).duck (0.35f) },

        { "Tape / Dub",  "Warm Tape 1/4",            B (d4, ST, 0.5f, 0.45f).filt (120, 5500).tape (0.5f, 0.3f) },
        { "Tape / Dub",  "Worn Cassette",            B (d8D, ST, 0.45f, 0.45f).filt (300, 3800).tape (0.65f, 0.75f) },
        { "Tape / Dub",  "Dub Siren Spiral",         B (d4D, PP, 0.88f, 0.5f).filt (260, 4200).tape (0.6f, 0.3f).out (-2.0f) },
        { "Tape / Dub",  "Melting Reel",             B (d2, ST, 0.7f, 0.5f).filt (200, 3000).tape (0.7f, 1.0f).width (1.3f) },

        { "Special",     "Infinite Riser",           B (d4, PP, 1.02f, 0.5f).filt (300, 5000).tape (0.5f, 0.15f).throwOnly().out (-3.0f) },
        { "Special",     "Lo-Fi Radio Echo",         B (d8, MO, 0.5f, 0.5f).filt (1000, 2800).tape (0.8f, 0.4f) },
        { "Special",     "Freeze Pad Builder",       B (d1, ST, 0.75f, 0.55f).filt (200, 6000).tape (0.3f, 0.5f).width (1.8f).throwOnly() },
        { "Special",     "Underwater 1/32",          B (d32, ST, 0.82f, 0.35f).filt (100, 1800).tape (0.4f, 0.6f) },
        { "Special",     "Telephone Throw",          B (d8D, PP, 0.5f, 0.5f).filt (1100, 3200).tape (0.7f, 0.1f).throwOnly() },
    };
    return presets;
}

} // namespace throwfx
