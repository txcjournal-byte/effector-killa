#include "Presets.h"

namespace subkilla
{

namespace
{
struct B
{
    Params p;
    B (VoiceMode m, float glideMs) { p.mode = m; p.glideMs = glideMs; }
    B& punch (float semis, float ms) { p.punchSemis = semis; p.punchMs = ms; return *this; }
    B& click (float c) { p.click = c; return *this; }
    B& sub (float s) { p.sub = s; return *this; }
    B& dist (SatType t, float drive, float mix = 1.0f) { p.sat = t; p.drive = drive; p.distMix = mix; return *this; }
    B& filter (float hz, float reso, float env = 0.0f) { p.cutoffHz = hz; p.reso = reso; p.fltEnv = env; return *this; }
    B& amp (float a, float d, float s, float r) { p.attackMs = a; p.decayMs = d; p.sustain = s; p.releaseMs = r; return *this; }
    B& tune (float semis, float cents = 0.0f) { p.tuneSemis = semis; p.fineCents = cents; return *this; }
    B& width (float w) { p.width = w; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    operator Params() const { return p; }
};
constexpr auto MONO = VoiceMode::mono;
constexpr auto LEG = VoiceMode::legato;
constexpr auto POLY = VoiceMode::poly;
constexpr auto TUBE = SatType::tube;
constexpr auto TAPE = SatType::tape;
constexpr auto HARD = SatType::hard;
constexpr auto FOLD = SatType::fold;
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list = {
        // ---- Classic 808
        { "Classic 808", "Init 808",          B (LEG, 90).punch (14, 45).click (0.2f).dist (TUBE, 0.2f).amp (1, 2500, 0, 250) },
        { "Classic 808", "Atlanta Boom",      B (LEG, 110).punch (12, 55).click (0.25f).sub (0.15f).dist (TUBE, 0.32f).amp (1, 3200, 0, 300) },
        { "Classic 808", "Clean Floor Shaker",B (LEG, 80).punch (9, 40).click (0.1f).sub (0.25f).dist (TUBE, 0.0f).filter (6000, 0.0f).amp (1, 4000, 0, 350) },
        { "Classic 808", "Rack Room Sub",     B (MONO, 60).punch (16, 35).click (0.3f).dist (TAPE, 0.35f).amp (1, 1800, 0, 200).out (-1) },
        { "Classic 808", "Hold The Note",     B (LEG, 120).punch (10, 50).click (0.15f).dist (TUBE, 0.25f).amp (2, 1500, 0.55f, 400) },
        { "Classic 808", "Warm Blanket",      B (LEG, 100).punch (8, 60).click (0.05f).sub (0.2f).dist (TAPE, 0.4f).filter (900, 0.15f, 0.35f).amp (3, 3000, 0, 300) },

        // ---- Distorted
        { "Distorted", "Gravel Throat",       B (LEG, 90).punch (14, 40).click (0.35f).dist (TUBE, 0.7f).filter (12000, 0.1f).amp (1, 2600, 0, 250).width (0.35f).out (-2) },
        { "Distorted", "Blown Trunk",         B (LEG, 100).punch (18, 45).click (0.3f).sub (0.2f).dist (HARD, 0.62f).amp (1, 2800, 0, 250).width (0.3f).out (-3) },
        { "Distorted", "Tape Grit 808",       B (LEG, 90).punch (12, 50).click (0.2f).dist (TAPE, 0.75f).amp (1, 3000, 0, 280).width (0.25f) },
        { "Distorted", "Concrete Mixer",      B (MONO, 70).punch (20, 35).click (0.45f).dist (HARD, 0.85f, 0.85f).filter (8000, 0.2f).amp (1, 2000, 0, 200).width (0.5f).out (-3) },
        { "Distorted", "Smoked Out",          B (LEG, 140).punch (10, 60).click (0.1f).sub (0.3f).dist (TUBE, 0.55f).filter (1600, 0.25f, 0.4f).amp (2, 3600, 0, 350).width (0.2f) },
        { "Distorted", "Razor Wire",          B (LEG, 80).punch (24, 30).click (0.5f).dist (FOLD, 0.45f, 0.7f).filter (10000, 0.1f).amp (1, 2200, 0, 220).width (0.55f).out (-3) },

        // ---- Long Slides
        { "Long Slides", "Slide Season",      B (LEG, 180).punch (12, 50).click (0.2f).dist (TUBE, 0.35f).amp (1, 5000, 0, 400) },
        { "Long Slides", "Glide On Em",       B (LEG, 260).punch (10, 55).click (0.15f).sub (0.15f).dist (TAPE, 0.45f).amp (1, 6000, 0, 450).width (0.2f) },
        { "Long Slides", "Drill Dive",        B (LEG, 140).punch (14, 40).click (0.3f).dist (HARD, 0.45f).amp (1, 4000, 0.3f, 300).width (0.25f).out (-1) },
        { "Long Slides", "Slow Motion Slide", B (LEG, 520).punch (8, 60).click (0.1f).sub (0.2f).dist (TUBE, 0.3f).amp (2, 8000, 0, 600) },
        { "Long Slides", "Rubber Band",       B (LEG, 320).punch (18, 45).click (0.25f).dist (TUBE, 0.5f).filter (3000, 0.3f, 0.3f).amp (1, 5000, 0.4f, 400).width (0.3f) },

        // ---- Punchy Kicks
        { "Punchy Kicks", "Kick & Sub",       B (MONO, 0).punch (30, 55).click (0.6f).dist (TUBE, 0.3f).amp (0.5f, 900, 0, 150) },
        { "Punchy Kicks", "Knockout",         B (MONO, 0).punch (36, 40).click (0.75f).dist (HARD, 0.4f).amp (0.5f, 600, 0, 120).out (-1) },
        { "Punchy Kicks", "Thump Machine",    B (MONO, 0).punch (24, 70).click (0.45f).sub (0.2f).dist (TAPE, 0.4f).amp (0.5f, 1200, 0, 180) },
        { "Punchy Kicks", "Tight Pocket",     B (MONO, 0).punch (28, 30).click (0.55f).dist (TUBE, 0.25f).filter (5000, 0.0f).amp (0.5f, 350, 0, 80) },

        // ---- Reese / Sub
        { "Reese / Sub", "Pure Sine Sub",     B (LEG, 60).punch (0, 40).click (0.0f).dist (TUBE, 0.0f).amp (5, 3000, 0.8f, 300) },
        { "Reese / Sub", "Underground Hum",   B (LEG, 90).punch (4, 80).click (0.05f).sub (0.6f).dist (TAPE, 0.3f).filter (400, 0.2f, 0.25f).amp (8, 4000, 0.7f, 400) },
        { "Reese / Sub", "Growl Reese",       B (LEG, 120).punch (2, 60).click (0.05f).sub (0.35f).dist (FOLD, 0.55f, 0.6f).filter (1200, 0.55f, 0.3f).amp (10, 4000, 0.75f, 350).width (0.6f).out (-2) },
        { "Reese / Sub", "Octave Down Chest", B (LEG, 100).punch (6, 50).click (0.1f).sub (1.0f).dist (TUBE, 0.4f).amp (2, 3500, 0.5f, 300) },

        // ---- Special
        { "Special", "Toxic Fold",            B (LEG, 150).punch (20, 50).click (0.3f).dist (FOLD, 0.8f, 0.8f).filter (6000, 0.3f, 0.2f).amp (1, 3000, 0, 300).width (0.6f).out (-4) },
        { "Special", "Laser Zap 808",         B (MONO, 40).punch (48, 120).click (0.2f).dist (TUBE, 0.4f).amp (1, 2000, 0, 200) },
        { "Special", "Wah Bass",              B (LEG, 200).punch (8, 50).click (0.1f).dist (TAPE, 0.5f).filter (250, 0.75f, 0.75f).amp (2, 3000, 0.3f, 300).width (0.3f) },
        { "Special", "Poly Sub Chords",       B (POLY, 0).punch (5, 40).click (0.05f).dist (TAPE, 0.3f).filter (2500, 0.1f, 0.2f).amp (8, 3500, 0.6f, 600).width (0.4f).out (-5) },
        { "Special", "Detroit Siren",         B (LEG, 700).punch (12, 200).click (0.15f).dist (HARD, 0.5f).filter (4000, 0.4f, 0.3f).amp (1, 9000, 0.6f, 800).width (0.45f).out (-2) },
    };
    return list;
}

} // namespace subkilla
