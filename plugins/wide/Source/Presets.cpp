#include "Presets.h"

namespace widefx
{

namespace
{
struct B
{
    Params p;
    B (float wLow, float wMid, float wHigh) { p.widthLow = wLow; p.widthMid = wMid; p.widthHigh = wHigh; }
    B& x (float lo, float hi) { p.lowXHz = lo; p.highXHz = hi; return *this; }
    B& mono (float hz) { p.monoBelowHz = hz; return *this; }
    B& haas (float ms, HaasSide s) { p.haasMs = ms; p.haasSide = s; return *this; }
    B& bal (float b) { p.balance = b; return *this; }
    B& st (float s) { p.stereoize = s; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    operator Params() const { return p; }
};
constexpr auto HL = HaasSide::left;
constexpr auto HR = HaasSide::right;
} // namespace

// widths are 0..2 (shown as 0..200 %), balance -1..1, stereoize 0..1
const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",            "Init (Neutral)",          B (1.0f, 1.0f, 1.0f) },

        { "Mix Bus",         "Wide Master Safe",        B (0.85f, 1.12f, 1.25f).x (150, 4000).mono (110) },
        { "Mix Bus",         "Club Mono Lows",          B (1.0f, 1.0f, 1.08f).x (160, 3500).mono (150) },
        { "Mix Bus",         "Headphone Wide",          B (1.0f, 1.25f, 1.4f).x (180, 3000).mono (100) },
        { "Mix Bus",         "Beat Tape Narrow",        B (0.6f, 0.8f, 0.9f).x (200, 3000).mono (120) },

        { "Vocals",          "Hook Vocal Spread",       B (1.0f, 1.1f, 1.35f).x (250, 5000).mono (180).st (0.25f) },
        { "Vocals",          "Ad-Lib Haas Left",        B (1.0f, 1.0f, 1.2f).x (250, 4000).mono (200).haas (12.0f, HL) },
        { "Vocals",          "Ad-Lib Haas Right",       B (1.0f, 1.0f, 1.2f).x (250, 4000).mono (200).haas (12.0f, HR) },
        { "Vocals",          "Fake Double",             B (1.0f, 1.15f, 1.3f).x (300, 4500).mono (220).haas (18.0f, HR).st (0.2f) },
        { "Vocals",          "Lead Vocal Center",       B (0.4f, 0.6f, 0.85f).x (250, 5000).mono (250) },

        { "808 & Bass",      "808 Mono Anchor",         B (0.0f, 1.0f, 1.15f).x (160, 3000).mono (160) },
        { "808 & Bass",      "Reese Wide Top",          B (0.0f, 1.4f, 1.6f).x (220, 2500).mono (220) },
        { "808 & Bass",      "Sub Mono Strict",         B (0.0f, 0.8f, 1.0f).x (300, 3000).mono (300) },

        { "Drums",           "Hat Spread",              B (1.0f, 1.0f, 1.7f).x (300, 6000).mono (200) },
        { "Drums",           "Drum Bus Wide",           B (0.9f, 1.2f, 1.4f).x (150, 5000).mono (120) },
        { "Drums",           "Snare Room Wide",         B (1.0f, 1.4f, 1.3f).x (200, 2500).mono (150).st (0.2f) },
        { "Drums",           "Percussion Scatter",      B (1.0f, 1.3f, 1.6f).x (400, 5000).mono (250).haas (8.0f, HL).st (0.3f) },

        { "Melodies & Pads", "Pad Super Wide",          B (1.0f, 1.6f, 1.8f).x (200, 3000).mono (150).st (0.4f) },
        { "Melodies & Pads", "Mono Synth To Stereo",    B (1.0f, 1.2f, 1.3f).x (250, 3500).mono (180).st (0.8f) },
        { "Melodies & Pads", "Pluck Shimmer",           B (1.0f, 1.1f, 1.6f).x (300, 4000).mono (200).st (0.3f) },
        { "Melodies & Pads", "Piano Air",               B (0.9f, 1.1f, 1.35f).x (200, 5000).mono (120) },
        { "Melodies & Pads", "Guitar Haas Double",      B (1.0f, 1.0f, 1.1f).x (250, 4000).mono (200).haas (15.0f, HR) },
        { "Melodies & Pads", "Bell Halo",               B (1.0f, 1.3f, 1.7f).x (400, 6000).mono (250).st (0.5f) },

        { "FX",              "Ultra Wide FX",           B (1.3f, 2.0f, 2.0f).x (200, 3000).mono (120).st (1.0f).out (-1.5f) },
        { "FX",              "Phase Safe Wide",         B (1.0f, 1.25f, 1.35f).x (200, 3000).mono (200).st (0.5f) },
        { "FX",              "Riser Spread",            B (1.0f, 1.5f, 1.9f).x (300, 4000).mono (250).st (0.6f) },

        { "Check / Fix",     "Mono Check",              B (0.0f, 0.0f, 0.0f) },
        { "Check / Fix",     "Side Solo",               B (1.0f, 1.0f, 1.0f).bal (1.0f) },
        { "Check / Fix",     "Center Focus",            B (0.6f, 0.65f, 0.8f).x (200, 3000).bal (-0.2f) },
        { "Check / Fix",     "Fix Wide Lows",           B (0.3f, 1.0f, 1.0f).x (250, 3000).mono (180) },
    };
    return presets;
}

} // namespace widefx
