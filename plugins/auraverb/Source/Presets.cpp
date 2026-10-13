#include "Presets.h"

namespace auraverb
{

namespace
{
constexpr Mode RM = Mode::room, HL = Mode::hall, PL = Mode::plate, CH = Mode::chamber, DG = Mode::digital80s, SH = Mode::shimmer,
               DS = Mode::darkSpace;

// P (mode, mix, predelay ms, decay s, size, early/late, attack, diffusion, mod depth, mod rate Hz,
//    damping Hz, low-cut Hz, bass mult, width, duck, vintage)
Params P (Mode m, float mix, float pre, float decay, float size, float el, float att, float diff, float modD, float modR,
          float damp, float lowCut, float bass, float width, float duck, bool vintage = false)
{
    Params p;
    p.mode = m; p.mix = mix; p.predelayMs = pre; p.decaySec = decay; p.size = size; p.earlyLate = el; p.attack = att;
    p.diffusion = diff; p.modDepth = modD; p.modRateHz = modR; p.dampingHz = damp; p.lowCutHz = lowCut; p.bassMult = bass;
    p.width = width; p.duck = duck; p.vintage = vintage; p.freeze = false; p.predelaySync = false; p.predelayDiv = defaultDivision;
    p.outputDb = 0.0f;
    return p;
}

Params synced (Params p, int div) { p.predelaySync = true; p.predelayDiv = div; return p; }
Params frozen (Params p) { p.freeze = true; return p; }
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",         "Init Aura",             P (HL, 0.30f, 20,  2.5f, 0.60f, 0.65f, 0.00f, 0.80f, 0.35f, 0.60f,  8000,   60, 1.00f, 1.00f, 0.00f) },

        { "Vocals",       "Hook Hall Glow",        P (HL, 0.25f, 35,  2.8f, 0.60f, 0.70f, 0.00f, 0.85f, 0.35f, 0.50f,  7500,  250, 0.80f, 1.20f, 0.50f) },
        { "Vocals",       "Verse Plate Sheen",     P (PL, 0.20f, 20,  1.6f, 0.45f, 0.85f, 0.00f, 0.80f, 0.25f, 0.70f,  9000,  300, 0.70f, 1.00f, 0.40f) },
        { "Vocals",       "Ad-Lib Cathedral",      P (HL, 0.35f, 80,  5.5f, 0.85f, 0.80f, 0.15f, 0.85f, 0.45f, 0.40f,  6500,  300, 0.80f, 1.40f, 0.70f) },
        { "Vocals",       "Melodic Rap Air",       P (CH, 0.22f, 25,  1.9f, 0.55f, 0.60f, 0.00f, 0.90f, 0.30f, 0.60f, 11000,  280, 0.80f, 1.20f, 0.45f) },
        { "Vocals",       "Golden Era Plate",      P (PL, 0.24f, 30,  2.2f, 0.55f, 0.90f, 0.00f, 0.75f, 0.35f, 0.50f,  8000,  250, 0.80f, 1.10f, 0.35f, true) },
        { "Vocals",       "Booth Room",            P (RM, 0.15f,  5,  0.5f, 0.25f, 0.40f, 0.00f, 0.80f, 0.15f, 0.80f,  9000,  200, 0.80f, 1.00f, 0.20f) },
        { "Vocals",       "Halo Harmonies",        P (SH, 0.25f, 60,  6.0f, 0.80f, 0.90f, 0.30f, 0.85f, 0.50f, 0.40f,  9000,  350, 0.70f, 1.50f, 0.50f) },
        { "Vocals",       "Dark Verse Chamber",    P (CH, 0.18f, 15,  1.3f, 0.35f, 0.55f, 0.00f, 0.80f, 0.25f, 0.60f,  4500,  220, 0.90f, 0.90f, 0.30f, true) },
        { "Vocals",       "Whisper Tape Verb",     P (DG, 0.22f, 40,  2.4f, 0.60f, 0.70f, 0.00f, 0.80f, 0.60f, 0.70f,  7000,  300, 0.80f, 1.20f, 0.35f) },

        { "Drums",        "Snare Plate Crack",     P (PL, 0.22f,  0,  1.3f, 0.40f, 0.85f, 0.00f, 0.75f, 0.20f, 0.80f,  8000,  300, 0.80f, 1.00f, 0.00f) },
        { "Drums",        "Clap Hall Spread",      P (HL, 0.25f, 10,  2.0f, 0.55f, 0.70f, 0.00f, 0.85f, 0.30f, 0.60f,  8500,  400, 0.80f, 1.30f, 0.00f) },
        { "Drums",        "Gated Neon Snare",      P (DG, 0.30f,  0,  0.6f, 0.50f, 0.30f, 0.00f, 0.90f, 0.30f, 0.80f, 10000,  250, 0.70f, 1.20f, 0.00f, true) },
        { "Drums",        "Tight Kit Room",        P (RM, 0.15f,  0, 0.45f, 0.20f, 0.35f, 0.00f, 0.80f, 0.10f, 0.80f,  7000,  150, 0.90f, 1.10f, 0.00f) },
        { "Drums",        "Hi-Hat Air",            P (CH, 0.18f,  5,  0.9f, 0.30f, 0.60f, 0.00f, 0.85f, 0.20f, 0.70f, 16000, 1000, 0.50f, 1.50f, 0.00f) },
        { "Drums",        "Perc Chamber",          P (CH, 0.20f,  8,  1.1f, 0.40f, 0.50f, 0.00f, 0.85f, 0.25f, 0.70f,  9000,  350, 0.80f, 1.10f, 0.00f) },
        { "Drums",        "Drum Bus Room",         P (RM, 0.10f,  3,  0.7f, 0.35f, 0.40f, 0.00f, 0.85f, 0.15f, 0.70f,  6500,  400, 0.80f, 1.00f, 0.30f) },

        { "Keys & Melody", "Midnight Piano Hall",  P (HL, 0.30f, 25,  3.2f, 0.75f, 0.60f, 0.00f, 0.85f, 0.35f, 0.50f,  8000,  100, 1.10f, 1.10f, 0.00f) },
        { "Keys & Melody", "Velvet Keys Plate",    P (PL, 0.28f, 20,  2.4f, 0.60f, 0.85f, 0.00f, 0.80f, 0.40f, 0.60f,  7500,  150, 1.00f, 1.10f, 0.00f, true) },
        { "Keys & Melody", "Bell Shimmer",         P (SH, 0.30f, 40,  5.0f, 0.70f, 0.90f, 0.00f, 0.85f, 0.40f, 0.50f, 10000,  300, 0.80f, 1.30f, 0.20f) },
        { "Keys & Melody", "Pluck Chamber",        P (CH, 0.25f, 15,  1.4f, 0.45f, 0.60f, 0.00f, 0.85f, 0.30f, 0.70f, 10000,  200, 0.90f, 1.20f, 0.20f) },
        { "Keys & Melody", "Lo-Fi Guitar 80s",     P (DG, 0.30f, 30,  2.6f, 0.65f, 0.70f, 0.00f, 0.80f, 0.70f, 0.60f,  6500,  150, 1.00f, 1.20f, 0.10f, true) },
        { "Keys & Melody", "Memphis Flute Hall",   P (HL, 0.30f, 45,  4.0f, 0.80f, 0.70f, 0.10f, 0.85f, 0.40f, 0.40f,  6000,  200, 0.90f, 1.20f, 0.20f, true) },
        { "Keys & Melody", "Lead Sync Plate",      synced (P (PL, 0.20f, 0, 1.8f, 0.50f, 0.85f, 0.00f, 0.80f, 0.30f, 0.60f, 9000, 250, 0.80f, 1.20f, 0.50f), 4) },

        { "Pads & Ambient", "Endless Heaven",      P (SH, 0.50f, 80, 18.0f, 1.00f, 1.00f, 0.50f, 0.90f, 0.60f, 0.35f,  9000,  250, 0.80f, 1.60f, 0.00f) },
        { "Pads & Ambient", "Dark Space Drift",    P (DS, 0.45f, 60, 14.0f, 0.90f, 0.85f, 0.60f, 0.90f, 0.60f, 0.30f,  7000,  150, 1.10f, 1.50f, 0.00f) },
        { "Pads & Ambient", "Nebula Wash",         P (DS, 0.55f, 120, 30.0f, 1.00f, 1.00f, 0.80f, 0.95f, 0.80f, 0.25f, 5000,  120, 1.20f, 1.60f, 0.00f) },
        { "Pads & Ambient", "Pad Blossom Hall",    P (HL, 0.40f, 50,  8.0f, 0.90f, 0.85f, 0.50f, 0.90f, 0.50f, 0.40f,  8000,  150, 1.00f, 1.40f, 0.00f) },
        { "Pads & Ambient", "Neon Dream Pad",      P (DG, 0.45f, 40,  9.0f, 0.85f, 0.85f, 0.30f, 0.85f, 0.80f, 0.40f,  7000,  150, 1.00f, 1.50f, 0.00f, true) },
        { "Pads & Ambient", "Frozen Choir",        frozen (P (SH, 0.60f, 30, 20.0f, 0.90f, 1.00f, 0.40f, 0.90f, 0.50f, 0.30f, 9000, 200, 0.80f, 1.50f, 0.00f)) },
        { "Pads & Ambient", "Infinite Hold",       frozen (P (HL, 0.55f, 20, 30.0f, 0.90f, 1.00f, 0.20f, 0.90f, 0.50f, 0.30f, 8000, 150, 1.00f, 1.40f, 0.00f)) },

        { "808 & Bass",   "808 Room Tail",         P (RM, 0.12f,  0,  0.8f, 0.30f, 0.50f, 0.00f, 0.80f, 0.10f, 0.60f,  6000,  400, 0.50f, 0.80f, 0.20f) },
        { "808 & Bass",   "Sub Space Glow",        P (DS, 0.15f, 30,  4.0f, 0.60f, 0.85f, 0.30f, 0.85f, 0.40f, 0.30f,  6000,  600, 0.30f, 1.20f, 0.60f) },
        { "808 & Bass",   "Bass Plate Tick",       P (PL, 0.12f,  0,  1.0f, 0.35f, 0.80f, 0.00f, 0.75f, 0.20f, 0.60f,  8000,  500, 0.40f, 1.00f, 0.30f) },

        { "FX & Special", "Swell Build Up",        P (HL, 0.50f,  0,  3.0f, 0.80f, 1.00f, 1.00f, 0.90f, 0.40f, 0.50f,  8000,  200, 0.90f, 1.30f, 0.00f) },
        { "FX & Special", "Ghost Trail",           P (DS, 0.60f, 250, 10.0f, 1.00f, 0.90f, 0.40f, 0.90f, 0.60f, 0.40f, 6000,  250, 0.90f, 1.80f, 0.80f) },
        { "FX & Special", "Haunted Tape",          P (DG, 0.50f, 30,  6.0f, 0.75f, 0.80f, 0.20f, 0.80f, 1.00f, 1.80f,  3500,  200, 1.00f, 1.30f, 0.00f, true) },
        { "FX & Special", "Shimmer Riser",         P (SH, 0.70f, 20, 12.0f, 1.00f, 1.00f, 0.90f, 0.90f, 0.60f, 0.50f, 12000,  300, 0.70f, 2.00f, 0.00f) },
        { "FX & Special", "Mono Tunnel",           P (CH, 0.40f, 15,  2.5f, 0.60f, 0.70f, 0.00f, 0.85f, 0.30f, 0.50f,  6000,  200, 1.00f, 0.00f, 0.00f) },
        { "FX & Special", "Metal Box",             P (RM, 0.35f,  0,  1.2f, 0.00f, 0.50f, 0.00f, 0.30f, 0.05f, 0.50f, 20000,  150, 1.00f, 1.00f, 0.00f) },
        { "FX & Special", "Payphone Space",        P (PL, 0.40f, 15,  2.0f, 0.50f, 0.85f, 0.00f, 0.80f, 0.30f, 0.60f,  3000,  900, 0.50f, 1.00f, 0.20f) },

        { "Mix Bus",      "Mix Glue Chamber",      P (CH, 0.08f, 10,  1.0f, 0.45f, 0.50f, 0.00f, 0.85f, 0.20f, 0.60f,  9000,  300, 0.80f, 1.00f, 0.30f) },
        { "Mix Bus",      "Master Air Room",       P (RM, 0.06f,  0,  0.6f, 0.40f, 0.50f, 0.00f, 0.85f, 0.15f, 0.60f, 12000,  500, 0.60f, 1.10f, 0.00f) },
        { "Mix Bus",      "Wide Mix Hall",         P (HL, 0.10f, 20,  1.8f, 0.60f, 0.70f, 0.00f, 0.85f, 0.30f, 0.50f,  8000,  350, 0.70f, 1.50f, 0.40f) },
    };
    return presets;
}

} // namespace auraverb
