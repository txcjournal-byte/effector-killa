#include "Presets.h"

namespace ghostdelay
{

namespace
{
// division index helpers (see divisionNames)
enum Div { d64T = 0, d64, d32T, d64D, d32, d16T, d32D, d16, d8T, d16D, d8, d4T, d8D, d4, d2T, d4D, d2, d1T, d2D, d1, d2bT, d1D, d2b };

struct B
{
    Params p;
    B (Mode m, int div, float fb, float mix) { p.mode = m; p.sync = true; p.division = div; p.feedback = fb; p.mix = mix; p.age = 0.0f; }
    B (Mode m, float ms, float fb, float mix) { p.mode = m; p.sync = false; p.timeMs = ms; p.feedback = fb; p.mix = mix; p.age = 0.0f; }
    B& filt (float lo, float hi) { p.lowCutHz = lo; p.highCutHz = hi; return *this; }
    B& mod (float depth, float rate) { p.modDepth = depth; p.modRate = rate; return *this; }
    B& age (float a) { p.age = a; return *this; }
    B& duck (float d) { p.duck = d; return *this; }
    B& ratio (float r) { p.lrRatio = r; return *this; }
    B& pitch (int i) { p.ghostPitch = i; return *this; }
    B& pat (int i) { p.pattern = i; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    operator Params() const { return p; }
};
constexpr auto TP = Mode::tape;
constexpr auto BB = Mode::bbd;
constexpr auto DG = Mode::digital;
constexpr auto GH = Mode::ghost;
constexpr auto RV = Mode::reverse;
constexpr auto DF = Mode::diffuse;
constexpr auto RH = Mode::rhythm;
enum Pitch { up12 = 0, up7 = 1, down12 = 2 };
enum Pat { dotted = 0, triplet = 1, swing = 2 };
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",     "Init (Tape 1/4)",          B (TP, d4, 0.4f, 0.5f).age (0.25f).mod (0.15f, 0.8f) },

        { "Tape",     "Cassette Hook Echo",       B (TP, d4, 0.42f, 0.42f).filt (180, 6500).age (0.35f).mod (0.25f, 0.7f).duck (0.3f) },
        { "Tape",     "Worn Reel 1/8D",           B (TP, d8D, 0.5f, 0.45f).filt (220, 4800).age (0.6f).mod (0.45f, 0.55f).ratio (0.15f) },
        { "Tape",     "Slap Back Booth",          B (TP, 95.0f, 0.12f, 0.32f).filt (160, 7000).age (0.3f).mod (0.1f, 1.2f) },
        { "Tape",     "Dub Spiral",               B (TP, d4D, 0.92f, 0.5f).filt (300, 3800).age (0.55f).mod (0.3f, 0.4f).out (-2.0f) },
        { "Tape",     "Melted Memory",            B (TP, d2, 0.62f, 0.5f).filt (150, 3000).age (0.85f).mod (0.8f, 0.35f).ratio (-0.2f) },
        { "Tape",     "Studio A Quarter",         B (TP, d4, 0.35f, 0.4f).filt (120, 9000).age (0.15f).mod (0.12f, 0.9f).duck (0.2f) },

        { "Analog",   "Bucket Brigade 1/8",       B (BB, d8, 0.45f, 0.42f).filt (150, 9000).age (0.3f).mod (0.3f, 0.6f) },
        { "Analog",   "Chorus Echo Wide",         B (BB, d8D, 0.38f, 0.45f).filt (200, 8000).age (0.25f).mod (0.65f, 0.9f).ratio (0.25f) },
        { "Analog",   "Murky Pedal",              B (BB, d4, 0.6f, 0.45f).filt (250, 5000).age (0.75f).mod (0.35f, 0.5f) },
        { "Analog",   "Pluck Bounce",             B (BB, d16D, 0.48f, 0.38f).filt (400, 9000).age (0.2f).mod (0.2f, 1.4f).ratio (0.5f) },
        { "Analog",   "Warm Keys Repeat",         B (BB, d4T, 0.44f, 0.4f).filt (180, 7000).age (0.4f).mod (0.4f, 0.7f).duck (0.25f) },

        { "Digital",  "Clean 1/4 Stereo",         B (DG, d4, 0.38f, 0.42f) },
        { "Digital",  "Crisp Ping 1/8",           B (DG, d8, 0.45f, 0.4f).filt (300, 12000).ratio (0.5f).duck (0.3f) },
        { "Digital",  "Hi-Hat Ghost Rolls",       B (DG, d32, 0.62f, 0.3f).filt (2000, 16000).ratio (0.33f) },
        { "Digital",  "Crushed Arcade",           B (DG, d16T, 0.55f, 0.42f).filt (250, 9000).age (0.75f) },
        { "Digital",  "Bitwise Ad-Lib",           B (DG, d8D, 0.4f, 0.4f).filt (450, 8000).age (0.45f).duck (0.5f) },
        { "Digital",  "Haas Doubler 18 ms",       B (DG, 18.0f, 0.0f, 0.35f).filt (200, 14000).ratio (0.6f) },

        { "Ghost",    "Octave Ascension",         B (GH, d4, 0.6f, 0.45f).filt (200, 9000).pitch (up12).mod (0.15f, 0.5f) },
        { "Ghost",    "Fifth Spirits",            B (GH, d8D, 0.55f, 0.42f).filt (250, 8000).pitch (up7).mod (0.2f, 0.4f) },
        { "Ghost",    "Basement Below",           B (GH, d4D, 0.58f, 0.45f).filt (60, 6000).pitch (down12).age (0.2f) },
        { "Ghost",    "Shimmer Throw",            B (GH, d2, 0.7f, 0.5f).filt (350, 10000).pitch (up12).duck (0.45f).mod (0.25f, 0.3f) },
        { "Ghost",    "Haunted Bells",            B (GH, d8, 0.66f, 0.4f).filt (500, 12000).pitch (up7).ratio (0.25f) },
        { "Ghost",    "Demon Drop",               B (GH, d8T, 0.62f, 0.45f).filt (40, 5000).pitch (down12).age (0.4f) },

        { "Reverse",  "Backwards Halo",           B (RV, d4, 0.35f, 0.45f).filt (150, 9000) },
        { "Reverse",  "Rewind Vocal",             B (RV, d2, 0.3f, 0.45f).filt (250, 7000).duck (0.4f) },
        { "Reverse",  "Inside Out 1/8",           B (RV, d8, 0.45f, 0.4f).filt (200, 10000).ratio (0.5f) },
        { "Reverse",  "Dream Swell Bar",          B (RV, d1, 0.4f, 0.5f).filt (180, 6000).age (0.35f) },
        { "Reverse",  "Tape Rewind Grit",         B (RV, d4D, 0.5f, 0.45f).filt (300, 5000).age (0.6f) },

        { "Diffuse",  "Cloud Echo",               B (DF, d4, 0.55f, 0.45f).filt (200, 8000).mod (0.35f, 0.3f) },
        { "Diffuse",  "Smoke Room Repeats",       B (DF, d8D, 0.6f, 0.42f).filt (250, 6500).mod (0.25f, 0.45f).duck (0.3f) },
        { "Diffuse",  "Endless Haze",             B (DF, d2, 0.82f, 0.5f).filt (150, 5500).age (0.3f).mod (0.5f, 0.2f) },
        { "Diffuse",  "Pad Wash",                 B (DF, d4D, 0.74f, 0.5f).filt (300, 7000).mod (0.6f, 0.25f).ratio (0.2f) },
        { "Diffuse",  "Ambient Pluck Trail",      B (DF, d8, 0.68f, 0.4f).filt (400, 9000).mod (0.3f, 0.5f) },
        { "Diffuse",  "Dark Matter",              B (DF, d1, 0.88f, 0.5f).filt (80, 2500).age (0.7f).mod (0.4f, 0.15f).out (-1.5f) },

        { "Rhythm",   "Dotted Trap Taps",         B (RH, d8, 0.25f, 0.42f).filt (250, 9000).pat (dotted) },
        { "Rhythm",   "Triplet Flow",             B (RH, d8, 0.3f, 0.42f).filt (300, 8500).pat (triplet).duck (0.3f) },
        { "Rhythm",   "Swing Bounce",             B (RH, d8, 0.3f, 0.4f).filt (250, 9000).pat (swing) },
        { "Rhythm",   "Drill Slide Taps",         B (RH, d16, 0.35f, 0.38f).filt (500, 10000).pat (triplet).age (0.2f) },
        { "Rhythm",   "Lead Gallop",              B (RH, d4, 0.22f, 0.4f).filt (200, 8000).pat (dotted).mod (0.15f, 0.8f) },
        { "Rhythm",   "Bell Cascade",             B (RH, d16, 0.45f, 0.4f).filt (600, 12000).pat (swing) },

        { "FX",       "Infinite Riser",           B (TP, d4, 1.03f, 0.5f).filt (350, 5000).age (0.5f).mod (0.2f, 0.4f).out (-3.0f) },
        { "FX",       "Siren Oscillator",         B (BB, d32, 1.05f, 0.4f).filt (500, 4000).age (0.5f).mod (0.9f, 0.25f).out (-4.0f) },
        { "FX",       "Ghost Staircase",          B (GH, d16, 0.85f, 0.4f).filt (300, 11000).pitch (up12).out (-2.0f) },
        { "FX",       "Phone Line Echo",          B (DG, d8, 0.5f, 0.45f).filt (1000, 3200).age (0.5f) },
        { "FX",       "Freeze Builder",           B (DF, d1, 0.7f, 0.55f).filt (200, 6000).mod (0.4f, 0.2f) },
        { "FX",       "Underwater 1/64",          B (TP, d64, 0.85f, 0.35f).filt (100, 1800).age (0.5f).mod (0.6f, 0.3f) },
    };
    return presets;
}

} // namespace ghostdelay
