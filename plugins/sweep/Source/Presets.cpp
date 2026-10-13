#include "Presets.h"

namespace sweepfx
{

namespace
{
using FT = FilterType;

// division indices (see divisionNames)
enum Div { d32 = 0, d16T, d16, d16D, d8T, d8, d8D, d4T, d4, d4D, d2T, d2, bar1, bar2, bar4, bar8 };

struct B
{
    Params p;
    B (FT type, float cutoff, float reso, float drive = 0.0f)
    {
        p.a.type = type; p.a.cutoff = cutoff; p.a.reso = reso; p.a.drive = drive;
        p.lfoDepth = 0.0f;
    }
    B& filterB (FT type, float cutoff, float reso, float mod = 1.0f, float drive = 0.0f)
    {
        p.b.on = true; p.b.type = type; p.b.cutoff = cutoff; p.b.reso = reso; p.b.mod = mod; p.b.drive = drive;
        return *this;
    }
    B& parallel() { p.routing = Routing::parallel; return *this; }
    B& modA (float m) { p.a.mod = m; return *this; }
    B& lfo (Shape s, int div, float depth) { p.shape = s; p.sync = true; p.division = div; p.lfoDepth = depth; return *this; }
    B& lfoHz (Shape s, float hz, float depth) { p.shape = s; p.sync = false; p.rateHz = hz; p.lfoDepth = depth; return *this; }
    // 16 hex digits, 0 = closed .. f = open
    B& steps (const char* hex, int div, float depth, float glide = 0.0f)
    {
        for (int i = 0; i < numSteps; ++i)
        {
            const char c = hex[i];
            const int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0);
            p.pattern[(size_t) i] = (float) v / 15.0f;
        }
        p.shape = Shape::pattern; p.sync = true; p.division = div; p.lfoDepth = depth; p.glide = glide;
        return *this;
    }
    B& stereo (float s) { p.stereo = s; return *this; }
    B& glide (float g) { p.glide = g; return *this; }
    B& retrig() { p.retrig = true; return *this; }
    B& env (float depth, float speed) { p.envDepth = depth; p.envSpeed = speed; return *this; }
    B& adsr (Trigger t, float depth, float a, float d, float s, float r, float sens = 0.5f)
    {
        p.trigger = t; p.adsrDepth = depth; p.attackMs = a; p.decayMs = d; p.sustain = s; p.releaseMs = r; p.sensitivity = sens;
        return *this;
    }
    B& mix (float m) { p.mix = m; return *this; }
    B& out (float db) { p.outputDb = db; return *this; }
    operator Params() const { return p; }
};
constexpr auto SIN = Shape::sine;
constexpr auto TRI = Shape::triangle;
constexpr auto UP = Shape::sawUp;
constexpr auto DN = Shape::sawDown;
constexpr auto SQR = Shape::square;
constexpr auto SH = Shape::sampleHold;
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init",       "Init (LP 1/4 Sweep)",     B (FT::lp24, 800.0f, 0.35f).lfo (SIN, d4, 0.5f) },

        { "Risers",     "4 Bar Riser",             B (FT::lp24, 120.0f, 0.45f, 0.1f).lfo (UP, bar4, 1.0f) },
        { "Risers",     "8 Bar Build",             B (FT::lp12, 150.0f, 0.6f, 0.2f).lfo (UP, bar8, 1.0f) },
        { "Risers",     "High Pass Lift",          B (FT::hp12, 30.0f, 0.45f).lfo (UP, bar4, 0.9f) },
        { "Risers",     "Snare Roll Sweep",        B (FT::bp, 300.0f, 0.55f).filterB (FT::hp12, 120.0f, 0.2f, 0.5f).lfo (UP, bar2, 0.8f) },
        { "Risers",     "Drop Tension",            B (FT::lp24, 200.0f, 0.5f, 0.15f).filterB (FT::hp12, 40.0f, 0.35f, 0.7f).lfo (UP, bar4, 0.9f) },
        { "Risers",     "Downlifter",              B (FT::lp12, 12000.0f, 0.5f).lfo (UP, bar2, -0.9f) },

        { "Wobbles",    "Classic Wobble 1/8",      B (FT::lp24, 300.0f, 0.7f, 0.3f).lfo (SIN, d8, 0.6f) },
        { "Wobbles",    "Triplet Wob",             B (FT::lp12, 250.0f, 0.75f, 0.4f).lfo (SIN, d8T, 0.55f) },
        { "Wobbles",    "Wide Wobble",             B (FT::lp24, 400.0f, 0.6f, 0.2f).lfo (TRI, d4, 0.5f).stereo (0.5f) },
        { "Wobbles",    "Growl Steps",             B (FT::lp24, 200.0f, 0.7f, 0.5f).steps ("f39c6f2a9f3c6fa4", d16, 0.7f, 0.3f) },
        { "Wobbles",    "808 Wub",                 B (FT::lp12, 120.0f, 0.5f, 0.25f).lfo (SIN, d4, 0.45f) },
        { "Wobbles",    "Half-Time Womp",          B (FT::lp24, 180.0f, 0.65f, 0.35f).lfo (DN, d2, 0.65f) },

        { "Gates",      "Trance Gate 1/16",        B (FT::lp12, 300.0f, 0.15f).filterB (FT::hp12, 6000.0f, 0.1f, -1.0f).steps ("f0f0ff0ff0f0f0ff", d16, 1.0f) },
        { "Gates",      "Chop Gate 1/8",           B (FT::lp12, 300.0f, 0.15f).filterB (FT::hp12, 6000.0f, 0.1f, -1.0f).steps ("f0f0f00ff0f0f0f0", d8, 1.0f) },
        { "Gates",      "Stutter 1/32",            B (FT::lp12, 400.0f, 0.1f).filterB (FT::hp12, 5000.0f, 0.1f, -1.0f).lfo (SQR, d32, 1.0f) },
        { "Gates",      "Bounce Gate",             B (FT::lp24, 500.0f, 0.3f).steps ("f06f0f60f0af06f0", d16, 0.9f, 0.15f) },
        { "Gates",      "Pump Filter 1/4",         B (FT::lp12, 350.0f, 0.3f).lfo (UP, d4, 0.85f) },
        { "Gates",      "Hat Shimmer Gate",        B (FT::hp12, 9000.0f, 0.3f).steps ("0f0f8f0f0f0fcf0f", d16, -0.7f) },

        { "Telephone",  "Telephone",               B (FT::bp, 1500.0f, 0.35f, 0.3f).filterB (FT::hp12, 400.0f, 0.1f) },
        { "Telephone",  "Walkie Talkie",           B (FT::bp, 1800.0f, 0.5f, 0.6f).filterB (FT::hp24, 600.0f, 0.1f) },
        { "Telephone",  "Old Radio",               B (FT::bp, 1200.0f, 0.3f, 0.3f).filterB (FT::lp12, 3500.0f, 0.1f, 0.0f).lfo (SH, d16, 0.04f).glide (0.6f) },
        { "Telephone",  "Phone Speaker Lead",      B (FT::hp24, 500.0f, 0.25f).filterB (FT::lp12, 3500.0f, 0.2f) },
        { "Telephone",  "Megaphone",               B (FT::bp, 1100.0f, 0.55f, 0.8f).out (-2.0f) },

        { "Underwater", "Underwater",              B (FT::lp24, 450.0f, 0.35f).lfo (SIN, bar1, 0.15f).stereo (0.3f) },
        { "Underwater", "Deep Sea",                B (FT::lp24, 250.0f, 0.5f).filterB (FT::comb, 180.0f, 0.4f, 0.5f).lfo (SIN, bar2, 0.2f).stereo (0.5f) },
        { "Underwater", "Next Room",               B (FT::lp12, 600.0f, 0.1f) },
        { "Underwater", "Submerged Vox",           B (FT::lp24, 700.0f, 0.4f).filterB (FT::formant, 350.0f, 0.4f, 0.5f).lfo (TRI, bar1, 0.2f) },
        { "Underwater", "Muffled Club",            B (FT::lp24, 300.0f, 0.3f).env (0.35f, 0.4f) },

        { "Formant",    "Talking Bass",            B (FT::formant, 300.0f, 0.6f, 0.2f).lfo (SIN, d4, 0.5f) },
        { "Formant",    "Yoy Wobble",              B (FT::formant, 300.0f, 0.7f, 0.3f).lfo (SIN, d8, 0.45f) },
        { "Formant",    "Vowel Steps",             B (FT::formant, 250.0f, 0.6f).steps ("06c3a09f5c2e80b4", d8, 0.6f, 0.4f) },
        { "Formant",    "Robot Talk",              B (FT::formant, 400.0f, 0.65f).lfo (SH, d16, 0.5f).glide (0.2f) },
        { "Formant",    "Choir Morph",             B (FT::formant, 600.0f, 0.5f).lfo (TRI, bar2, 0.45f).stereo (0.4f).mix (0.85f) },

        { "Motion",     "Auto Wah",                B (FT::bp, 400.0f, 0.6f).env (0.55f, 0.7f) },
        { "Motion",     "MIDI Pluck Filter",       B (FT::lp24, 150.0f, 0.55f, 0.2f).adsr (Trigger::midi, 0.75f, 2.0f, 280.0f, 0.15f, 180.0f) },
        { "Motion",     "Hit Sweep",               B (FT::lp12, 200.0f, 0.5f).adsr (Trigger::transient, 0.7f, 1.0f, 180.0f, 0.0f, 120.0f, 0.55f) },
        { "Motion",     "Comb Flange",             B (FT::comb, 400.0f, 0.75f).lfo (TRI, bar2, 0.5f).stereo (0.5f).mix (0.6f) },
        { "Motion",     "Retrig Wobble (MIDI)",    B (FT::lp24, 220.0f, 0.65f, 0.3f).lfo (SIN, d8T, 0.55f).retrig().adsr (Trigger::midi, 0.0f, 5.0f, 250.0f, 0.0f, 200.0f) },
    };
    return presets;
}

} // namespace sweepfx
