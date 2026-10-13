#include "Presets.h"

namespace akill
{

namespace
{
struct B
{
    Params p;
    int scale = -1;
    B (Mode m, float retune, float humanize = 0.0f) { p.mode = m; p.retuneMs = retune; p.humanize = humanize; }
    B& thr (float c) { p.thresholdCents = c; return *this; }
    B& vib (float amt, float rate = 5.5f) { p.vibrato = amt; p.vibratoRateHz = rate; return *this; }
    B& shift (float semis) { p.transpose = semis; return *this; }
    B& throat (float semis) { p.throatSemis = semis; return *this; }
    B& raw() { p.preserveFormants = false; return *this; }
    B& mix (float m) { p.mix = m; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    B& chroma() { scale = scaleChromatic; return *this; }
};
constexpr auto LIVE = Mode::live;
constexpr auto MIX = Mode::mix;

Preset P (const char* cat, const char* name, const B& b) { return { cat, name, b.p, b.scale }; }
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        P ("Init",      "Init (Hard Tune)",        B (MIX, 0.0f)),

        P ("Hard Tune", "Robot Lock",              B (MIX, 0.0f)),
        P ("Hard Tune", "Ice Cold Hard",           B (MIX, 0.0f).throat (0.4f)),
        P ("Hard Tune", "Stair Steps",             B (MIX, 0.0f).chroma()),
        P ("Hard Tune", "Melodic Hard",            B (MIX, 6.0f)),
        P ("Hard Tune", "Snap Back",               B (MIX, 12.0f, 0.05f)),
        P ("Hard Tune", "Glass Robot",             B (MIX, 0.0f).throat (2.0f)),
        P ("Hard Tune", "Dark Robot",              B (MIX, 0.0f).throat (-1.5f)),
        P ("Hard Tune", "Wobble Robot",            B (MIX, 0.0f).vib (0.7f, 7.0f)),

        P ("Natural",   "Invisible Fix",           B (MIX, 120.0f, 0.6f).thr (60.0f)),
        P ("Natural",   "Studio Natural",          B (MIX, 80.0f, 0.4f)),
        P ("Natural",   "Soft Polish",             B (MIX, 50.0f, 0.5f)),
        P ("Natural",   "Ballad Breath",           B (MIX, 180.0f, 0.7f).thr (80.0f)),
        P ("Natural",   "Background Stack",        B (MIX, 40.0f, 0.3f)),
        P ("Natural",   "Gentle Pull",             B (MIX, 260.0f, 0.8f)),
        P ("Natural",   "Pitchy Saver",            B (MIX, 35.0f, 0.5f).thr (70.0f)),

        P ("Melodic",   "Melody Glide",            B (MIX, 20.0f, 0.2f)),
        P ("Melodic",   "Sing-Rap Flow",           B (MIX, 15.0f, 0.1f)),
        P ("Melodic",   "Hook Shine",              B (MIX, 10.0f, 0.15f).throat (0.5f)),
        P ("Melodic",   "Wavy Lead",               B (MIX, 25.0f, 0.1f).vib (0.3f, 5.8f)),
        P ("Melodic",   "Late Night Croon",        B (MIX, 35.0f, 0.3f).vib (0.2f, 5.2f)),
        P ("Melodic",   "Emo Melody",              B (MIX, 18.0f, 0.25f).throat (-0.5f)),
        P ("Melodic",   "Ad-Lib Polish",           B (MIX, 8.0f, 0.1f).mix (0.85f)),

        P ("Live",      "Booth Monitor",           B (LIVE, 10.0f)),
        P ("Live",      "Stage Hard",              B (LIVE, 0.0f)),
        P ("Live",      "Live Natural",            B (LIVE, 60.0f, 0.4f)),
        P ("Live",      "Freestyle Track",         B (LIVE, 15.0f, 0.1f)),
        P ("Live",      "Cypher Lock",             B (LIVE, 0.0f).thr (70.0f)),
        P ("Live",      "Low Latency Melodic",     B (LIVE, 22.0f, 0.2f)),

        P ("Character", "Chipmunk Ad-Lib",         B (MIX, 5.0f).shift (12.0f).raw()),
        P ("Character", "Demon Octave",            B (MIX, 0.0f).shift (-12.0f).throat (-3.0f)),
        P ("Character", "Alien Choir",             B (MIX, 0.0f).shift (7.0f).throat (4.0f)),
        P ("Character", "Pitched Down Ghost",      B (MIX, 10.0f).shift (-5.0f).throat (-2.0f)),
        P ("Character", "Baby Voice",              B (MIX, 8.0f).shift (5.0f).throat (5.0f)),
        P ("Character", "Giant",                   B (MIX, 10.0f).shift (-7.0f).throat (-5.0f)),
        P ("Character", "Octave Stack",            B (MIX, 0.0f).shift (-12.0f).mix (0.5f)),
        P ("Character", "Power Fifth",             B (MIX, 0.0f).shift (7.0f).mix (0.5f)),
        P ("Character", "Throat Swap",             B (MIX, 15.0f, 0.2f).throat (-4.0f)),
    };
    return presets;
}

} // namespace akill
