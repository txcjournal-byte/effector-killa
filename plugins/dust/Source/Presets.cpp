#include "Presets.h"

namespace dust
{

namespace
{
// age, tape, wobble, vinyl, crush, tone, width, mix, outputDb, noiseFollow
Params P (float age, float tape, float wobble, float vinyl, float crush, float tone,
          float width = 0.5f, float mix = 1.0f, float outDb = 0.0f, bool follow = true)
{
    Params p;
    p.age = age; p.tape = tape; p.wobble = wobble; p.vinyl = vinyl; p.crush = crush;
    p.tone = tone; p.width = width; p.mix = mix; p.outputDb = outDb; p.noiseFollow = follow;
    return p;
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",     "Init (clean)",          P (0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f) },

        { "Cassette", "Dusty Cassette",        P (0.00f, 0.45f, 0.35f, 0.35f, 0.00f, 0.42f) },
        { "Cassette", "Walkman Battery Low",   P (0.10f, 0.40f, 0.85f, 0.25f, 0.00f, 0.36f) },
        { "Cassette", "Dubbed Ten Times",      P (0.35f, 0.55f, 0.40f, 0.30f, 0.05f, 0.40f) },
        { "Cassette", "Tape Glue (subtle)",    P (0.00f, 0.35f, 0.10f, 0.00f, 0.00f, 0.60f) },

        { "Vinyl",    "Vinyl Loop",            P (0.00f, 0.30f, 0.15f, 0.60f, 0.00f, 0.35f, 0.70f) },
        { "Vinyl",    "Crate Digger",          P (0.15f, 0.35f, 0.20f, 0.70f, 0.05f, 0.32f) },
        { "Vinyl",    "Basement 45",           P (0.25f, 0.45f, 0.25f, 0.80f, 0.00f, 0.28f, 0.40f) },
        { "Vinyl",    "Crackle Only",          P (0.00f, 0.00f, 0.00f, 0.55f, 0.00f, 1.00f, 0.50f, 1.0f, 0.0f, false) },

        { "Warp",     "Warped Tape",           P (0.00f, 0.60f, 0.80f, 0.20f, 0.00f, 0.38f) },
        { "Warp",     "Seasick Bells",         P (0.00f, 0.25f, 1.00f, 0.10f, 0.00f, 0.50f, 0.80f) },
        { "Warp",     "Melted Pluck",          P (0.20f, 0.50f, 0.65f, 0.15f, 0.00f, 0.45f) },

        { "Digital",  "8-Bit Dream",           P (0.00f, 0.35f, 0.10f, 0.10f, 0.55f, 0.50f) },
        { "Digital",  "Game Boy Keys",         P (0.00f, 0.20f, 0.05f, 0.00f, 0.80f, 0.70f) },
        { "Digital",  "Sampler 12-Bit",        P (0.00f, 0.30f, 0.05f, 0.05f, 0.30f, 0.45f) },

        { "Age",      "Age 50",                P (0.50f, 0.20f, 0.20f, 0.20f, 0.00f, 0.50f) },
        { "Age",      "Age 85",                P (0.85f, 0.20f, 0.20f, 0.20f, 0.00f, 0.50f) },
        { "Age",      "Memory Of A Song",      P (1.00f, 0.30f, 0.40f, 0.40f, 0.10f, 0.40f, 0.30f) },

        { "Mix",      "Parallel Warmth",       P (0.00f, 0.70f, 0.10f, 0.00f, 0.00f, 0.45f, 0.50f, 0.45f) },
        { "Mix",      "Background Texture",    P (0.40f, 0.30f, 0.30f, 0.50f, 0.00f, 0.35f, 0.90f, 0.60f) },
    };
    return presets;
}

} // namespace dust
