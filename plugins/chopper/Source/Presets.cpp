#include "Presets.h"

namespace chopper
{

namespace
{
// rates: 0 = 1/8, 1 = 1/16, 2 = 1/32, 3 = 1/16T
// sizes: 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32, 4 = 1/64, 5 = 1/16T
enum { r8 = 0, r16, r32, r16T };
enum { s4 = 0, s8, s16, s32, s64, s16T };
} // namespace

const std::vector<Preset>& factoryPresets()
{
    //                 category      name                    pattern             rate  size  swing  mix   out
    static const std::vector<Preset> presets {
        { "Init",       "Init (Clean)",          "................", r16,  s32,  0.0f,  1.0f, 0.0f },

        { "Stutter",    "Classic Stutter",       "....RR......RR..", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Stutter",    "Trap Stutter",          "..R...R...RR.RRR", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Stutter",    "Machine Gun",           "........RRRRRRRR", r16,  s64,  0.0f,  1.0f, 0.0f },
        { "Stutter",    "Vocal Chop Stutter",    "R...R...R..RR...", r16,  s16,  0.0f,  1.0f, 0.0f },
        { "Stutter",    "Swung Stammer",         ".R.R..RR.R.R..RR", r16,  s32,  0.45f, 1.0f, 0.0f },

        { "Rolls",      "Hat Roll 1/32",         "............RRRR", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Rolls",      "Snare Roll Build",      "........RRRRRRRR", r8,   s16,  0.0f,  1.0f, 0.0f },
        { "Rolls",      "Triplet Roll",          "....RRRR....RRRR", r16T, s16T, 0.0f,  1.0f, 0.0f },
        { "Rolls",      "End Of Bar Roll",       "..............RR", r16,  s64,  0.0f,  1.0f, 0.0f },
        { "Rolls",      "Drill Roll",            "......RR..RR.RRR", r16,  s16T, 0.2f,  1.0f, 0.0f },

        { "Tape Stops", "Bar End Stop",          "............TTTT", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Tape Stops", "Quick Brake",           "......TT......TT", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Tape Stops", "Long Power Down",       "........TTTTTTTT", r8,   s32,  0.0f,  1.0f, 0.0f },
        { "Tape Stops", "Stutter Brake",         "....T...T...TTTT", r16,  s32,  0.0f,  1.0f, 0.0f },

        { "Reverse",    "Reverse Tails",         "...V...V...V...V", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Reverse",    "Reverse Swell",         "............VVVV", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Reverse",    "Flip Flop",             "V.V.V.V.V.V.V.V.", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Reverse",    "Backspin Bar",          "........VVVVVVVV", r8,   s32,  0.0f,  1.0f, 0.0f },

        { "Gates",      "Trance Gate 16th",      ".G.G.G.G.G.G.G.G", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Gates",      "Offbeat Chop",          "G.G.G.G.G.G.G.G.", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Gates",      "Dropout",               "............GGGG", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Gates",      "Swing Gate",            ".GG..GG..GG..GG.", r16,  s32,  0.4f,  1.0f, 0.0f },
        { "Gates",      "32nd Shred",            ".G.G.G.G.GGG.G.G", r32,  s32,  0.0f,  1.0f, 0.0f },

        { "Pitch",      "Octave Drops",          "....PPPP....PPPP", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Pitch",      "Slow Mo Half Bar",      "........HHHHHHHH", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Pitch",      "808 Dive",              "PP..PP..HHHH....", r16,  s32,  0.0f,  1.0f, 0.0f },

        { "Combo",      "Glitch Combo",          "R.V.T.G.P.H.RRVV", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Combo",      "Repeat Then Stop",      "....RRRR....TTTT", r16,  s32,  0.0f,  1.0f, 0.0f },
        { "Combo",      "Chaos Chop",            "RVRGPHTVRRGGVVTT", r16,  s64,  0.0f,  1.0f, 0.0f },
        { "Combo",      "Breakdown FX",          "RRGGVVTTPPHHRRTT", r16,  s32,  0.15f, 1.0f, 0.0f },
        { "Combo",      "Drill Glitch",          "..R.G..V..RR.T..", r16,  s16T, 0.2f,  1.0f, 0.0f },
        { "Combo",      "Parallel Mangle",       "RVTGRVTGRRVVPPHH", r16,  s32,  0.0f,  0.5f, 0.0f },
    };
    return presets;
}

} // namespace chopper
