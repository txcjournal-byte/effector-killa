#include "Presets.h"

#include <algorithm>

namespace heatfx
{

namespace
{
enum M { TU = 0, TA, XF, TR, FZ, RC, FB, WC };

struct B
{
    Params p;
    explicit B (int bands, float lo = 180.0f, float hi = 2500.0f)
    {
        p.bands = bands; p.xoverLoHz = lo; p.xoverHiHz = hi < lo * 1.5f ? std::min (16000.0f, lo * 2.5f) : hi;
        for (auto& b : p.band) { b.model = TU; b.drive = 0.0f; b.mix = 1.0f; b.levelDb = 0.0f; b.solo = false; }
    }
    B& band (int i, int model, float drive, float mix = 1.0f, float levelDb = 0.0f)
    {
        auto& b = p.band[(size_t) i];
        b.model = model; b.drive = drive; b.mix = mix; b.levelDb = levelDb;
        return *this;
    }
    B& os (int c) { p.oversampling = c; return *this; }
    B& tilt (float pre, float post) { p.tiltPreDb = pre; p.tiltPostDb = post; return *this; }
    B& cut (float lo, float hi) { p.lowCutHz = lo; p.highCutHz = hi; return *this; }
    B& punch (float v) { p.punch = v; return *this; }
    B& dyn (float v) { p.dynamics = v; return *this; }
    B& mix (float v) { p.mix = v; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    B& keep (float hz) { p.lowKeepHz = hz; return *this; }
    B& manual() { p.autoGain = false; return *this; }
    operator Params() const { return p; }
};
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",    "Init (Warm Tube)",      B (1).band (0, TU, 0.30f) },

        { "808",     "808 Grit Keeper",       B (1).band (0, TU, 0.45f).keep (80) },
        { "808",     "Phone Speaker 808",     B (2, 150).band (0, TA, 0.20f).band (1, TR, 0.60f).keep (60).tilt (0, 1.0f) },
        { "808",     "Blown Sub Rage",        B (1).band (0, FZ, 0.70f).keep (70).cut (20, 9000) },
        { "808",     "Knock Tonka",           B (2, 200).band (0, XF, 0.35f).band (1, TU, 0.55f).punch (0.35f).keep (50) },
        { "808",     "Reese Teeth",           B (3, 120, 1500).band (1, FB, 0.35f, 0.6f).band (2, TR, 0.40f, 0.5f, -3.0f).keep (90) },
        { "808",     "Warm Long 808",         B (1).band (0, TA, 0.35f).keep (45).dyn (0.2f) },
        { "808",     "Distorted Glide",       B (1).band (0, RC, 0.45f, 0.6f).keep (100).cut (20, 7000) },

        { "Drums",   "Drum Bus Crunch",       B (1).band (0, TR, 0.35f).punch (0.4f).mix (0.7f) },
        { "Drums",   "Kick Knock",            B (2, 120).band (0, XF, 0.45f).band (1, TU, 0.25f).punch (0.5f) },
        { "Drums",   "Snare Crack",           B (2, 900).band (0, TU, 0.20f).band (1, FZ, 0.40f, 0.5f).dyn (0.5f) },
        { "Drums",   "Hat Sizzle",            B (2, 5000).band (1, TA, 0.50f, 0.6f).tilt (0, 1.5f) },
        { "Drums",   "Tape Drums",            B (1).band (0, TA, 0.55f).punch (0.25f).cut (20, 12000) },
        { "Drums",   "Lo-Fi Kit",             B (1).band (0, WC, 0.55f, 0.8f).cut (40, 9000) },
        { "Drums",   "Parallel Smash",        B (1).band (0, TR, 0.75f, 0.35f).punch (0.7f) },

        { "Vocals",  "Vocal Warmth",          B (1).band (0, TU, 0.25f).tilt (-0.5f, 0.5f) },
        { "Vocals",  "Rap Presence",          B (2, 2500).band (0, TU, 0.20f).band (1, TR, 0.35f, 0.5f).cut (80, 20000) },
        { "Vocals",  "Ad-Lib Grit",           B (3, 300, 3000).band (1, FZ, 0.40f, 0.5f).band (2, TA, 0.30f).cut (150, 20000) },
        { "Vocals",  "Radio Voice",           B (1).band (0, TR, 0.50f).cut (400, 4000) },
        { "Vocals",  "Tape Vocal",            B (1).band (0, TA, 0.40f).dyn (0.3f) },
        { "Vocals",  "Demon Scream",          B (1).band (0, FB, 0.50f, 0.4f).cut (200, 20000) },

        { "Synths",  "Lead Heat",             B (1).band (0, TU, 0.50f).mix (0.8f) },
        { "Synths",  "Rage Saw Fuzz",         B (1).band (0, FZ, 0.60f).cut (20, 10000) },
        { "Synths",  "Pluck Bite",            B (1).band (0, TR, 0.45f).dyn (0.5f) },
        { "Synths",  "Pad Thickener",         B (1).band (0, TA, 0.40f, 0.6f).tilt (-1.0f, 0) },
        { "Synths",  "Bell Fold",             B (1).band (0, FB, 0.30f, 0.45f) },
        { "Synths",  "Octave Lead",           B (1).band (0, RC, 0.70f, 0.5f) },

        { "Bus",     "Mix Glue",              B (1).band (0, XF, 0.20f).mix (0.6f) },
        { "Bus",     "Master Warmth",         B (1).band (0, TA, 0.15f).mix (0.7f) },
        { "Bus",     "Beat Bus Push",         B (3, 150, 3000).band (0, XF, 0.30f).band (1, TU, 0.25f).band (2, TA, 0.20f).punch (0.2f) },
        { "Bus",     "Loud & Proud",          B (1).band (0, TR, 0.35f).punch (0.3f).keep (40) },
        { "Bus",     "Air Saturator",         B (2, 6000).band (1, TA, 0.45f, 0.6f) },
        { "Bus",     "Low-End Weight",        B (2, 120).band (0, XF, 0.45f) },

        { "Destroy", "Total Meltdown",        B (1).band (0, FZ, 0.90f).os (3) },
        { "Destroy", "Wavefold Hell",         B (1).band (0, FB, 0.85f).os (3) },
        { "Destroy", "Bit Furnace",           B (1).band (0, WC, 0.90f) },
        { "Destroy", "Octave Shred",          B (2, 400).band (0, RC, 1.00f).band (1, FZ, 0.70f) },
        { "Destroy", "Three-Way Inferno",     B (3, 200, 2500).band (0, FZ, 0.80f).band (1, FB, 0.60f).band (2, RC, 0.70f).os (3) },
        { "Destroy", "Broken Speaker",        B (1).band (0, TR, 0.90f).cut (300, 3500).dyn (-0.5f) },
    };
    return presets;
}

} // namespace heatfx
