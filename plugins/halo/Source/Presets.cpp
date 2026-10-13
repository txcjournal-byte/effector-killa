#include "Presets.h"

namespace halo
{

namespace
{
// P (predelay ms, size, decay s, diffusion, mod, hi-cut Hz, low-cut Hz, width, duck, mix, freeze)
Params P (float pre, float size, float decay, float diff, float mod, float hiCut, float lowCut,
          float width, float duck, float mix, bool freeze = false)
{
    Params p;
    p.predelayMs = pre; p.size = size; p.decaySec = decay; p.diffusion = diff; p.mod = mod;
    p.hiCutHz = hiCut; p.lowCutHz = lowCut; p.width = width; p.duck = duck; p.mix = mix; p.freeze = freeze;
    p.outputDb = 0.0f;
    return p;
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",        "Init Halo",            P (20, 0.60f, 3.5f, 0.80f, 0.40f,  9000, 150, 1.00f, 0.00f, 0.35f) },

        { "Vocal",       "Trap Hook Halo",       P (40, 0.60f, 3.2f, 0.85f, 0.45f,  7500, 250, 1.10f, 0.55f, 0.30f) },
        { "Vocal",       "Vocal Plate Glow",     P (25, 0.35f, 1.6f, 0.90f, 0.30f,  8500, 220, 1.00f, 0.35f, 0.22f) },
        { "Vocal",       "Ad-Lib Throw",         P (90, 0.75f, 5.0f, 0.80f, 0.55f,  7000, 300, 1.30f, 0.70f, 0.40f) },
        { "Vocal",       "Melodic Air",          P (30, 0.50f, 2.4f, 0.85f, 0.50f, 12000, 320, 1.20f, 0.40f, 0.26f) },
        { "Vocal",       "Dark Verse Room",      P (12, 0.25f, 0.9f, 0.75f, 0.25f,  5000, 180, 0.90f, 0.20f, 0.18f) },
        { "Vocal",       "Whisper Chamber",      P (18, 0.40f, 1.9f, 0.95f, 0.35f, 10000, 400, 1.00f, 0.50f, 0.28f) },

        { "Pads",        "Endless Pad",          P (60, 0.90f, 14.0f, 0.90f, 0.70f,  8000, 150, 1.30f, 0.00f, 0.50f) },
        { "Pads",        "Glass Choir",          P (45, 0.80f, 9.0f, 0.95f, 0.60f, 14000, 220, 1.40f, 0.00f, 0.45f) },
        { "Pads",        "Warm Blanket",         P (35, 0.70f, 6.0f, 0.85f, 0.45f,  4000, 120, 1.10f, 0.00f, 0.40f) },
        { "Pads",        "Slow Bloom Keys",      P (150, 0.85f, 7.5f, 1.00f, 0.55f,  9000, 200, 1.25f, 0.25f, 0.38f) },

        { "808/Drums",   "Snare Halo",           P (10, 0.45f, 1.4f, 0.85f, 0.25f,  9000, 300, 1.00f, 0.00f, 0.25f) },
        { "808/Drums",   "Clap Tail",            P (0,  0.55f, 2.2f, 0.90f, 0.30f,  8000, 400, 1.20f, 0.00f, 0.30f) },
        { "808/Drums",   "Hi-Hat Air",           P (5,  0.20f, 0.6f, 0.70f, 0.20f, 16000, 1000, 1.30f, 0.00f, 0.20f) },
        { "808/Drums",   "Perc Room",            P (8,  0.15f, 0.5f, 0.65f, 0.15f,  7000, 250, 0.90f, 0.00f, 0.22f) },
        { "808/Drums",   "Drum Bus Space",       P (15, 0.35f, 1.1f, 0.80f, 0.25f,  6500, 450, 1.00f, 0.30f, 0.15f) },

        { "Huge/Space",  "Halo Galaxy",          P (80, 1.00f, 20.0f, 0.95f, 0.80f,  9000, 180, 1.40f, 0.00f, 0.50f) },
        { "Huge/Space",  "Night Cathedral",      P (55, 0.95f, 11.0f, 0.90f, 0.40f,  6000, 140, 1.20f, 0.00f, 0.42f) },
        { "Huge/Space",  "Infinite Ocean",       P (120, 1.00f, 30.0f, 1.00f, 0.75f,  5500, 160, 1.50f, 0.00f, 0.55f) },
        { "Huge/Space",  "Deep Space Wash",      P (250, 0.90f, 16.0f, 0.90f, 0.65f,  7000, 250, 1.35f, 0.30f, 0.45f) },

        { "Special",     "Freeze Drone",         P (0,  0.85f, 30.0f, 1.00f, 0.60f,  8000, 200, 1.30f, 0.00f, 0.60f, true) },
        { "Special",     "Ghost Delay",          P (400, 0.55f, 4.0f, 0.40f, 0.50f,  6000, 300, 1.20f, 0.40f, 0.35f) },
        { "Special",     "Lo-Fi Haze",           P (30, 0.60f, 3.5f, 0.85f, 1.00f,  2500, 400, 1.00f, 0.00f, 0.40f) },
        { "Special",     "Mono Tunnel",          P (20, 0.65f, 3.0f, 0.80f, 0.30f,  6000, 200, 0.00f, 0.00f, 0.35f) },
        { "Special",     "Ducked Wash",          P (40, 0.85f, 10.0f, 0.90f, 0.60f,  8000, 250, 1.30f, 1.00f, 0.60f) },
        { "Special",     "Tiny Box",             P (0,  0.00f, 0.3f, 0.60f, 0.10f, 11000, 200, 0.80f, 0.00f, 0.25f) },
    };
    return presets;
}

} // namespace halo
