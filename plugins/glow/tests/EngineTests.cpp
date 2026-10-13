// Headless tests for the TrapWin GLOW engine.
#include "../Source/DSP/GlowEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace glowfx;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Stereo run (Engine& e, Stereo x, int block = 512)
{
    for (size_t pos = 0; pos < x.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.l.size() - pos);
        float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
        e.process (ch, 2, n);
    }
    return x;
}

static Stereo run (const Params& p, Stereo x, double sr, int block = 512)
{
    Engine e; e.setParams (p); e.prepare (sr, block);
    return run (e, std::move (x), block);
}

// ---- test signals ----
static Stereo pink (size_t n, float amp, unsigned seed)
{
    std::mt19937 rng (seed); std::normal_distribution<float> nd (0.0f, 1.0f);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (int c = 0; c < 2; ++c)
    {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        auto& v = c == 0 ? s.l : s.r;
        for (size_t i = 0; i < n; ++i)
        {
            const float w = nd (rng);
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
            b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            v[i] = amp * 0.11f * (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f);
            b6 = w * 0.115926f;
        }
    }
    return s;
}

// generated trap loop at 140 BPM: kick (pitch-swept sine), snare (noise + tone), hats, 808-ish sub line
static Stereo drumLoop (size_t n, double sr, unsigned seed)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> ud (-1.0f, 1.0f);
    Stereo s { std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f) };
    const double beat = 60.0 / 140.0 * sr;
    for (size_t i = 0; i < n; ++i)
    {
        const double t16 = std::fmod ((double) i, beat / 4.0) / sr;   // time since the last 16th
        const int step = (int) ((double) i / (beat / 4.0)) % 16;
        float v = 0.0f;
        if (step == 0 || step == 7 || step == 10)
            v += 0.8f * (float) (std::sin (2 * PI * (50.0 * t16 + 90.0 * (1.0 - std::exp (-t16 * 30.0)) / 30.0)) * std::exp (-t16 * 7.0));
        if (step == 4 || step == 12)
            v += (float) std::exp (-t16 * 22.0) * (0.45f * ud (rng) + 0.25f * (float) std::sin (2 * PI * 190.0 * t16));
        v += 0.12f * ud (rng) * (float) std::exp (-t16 * 90.0);
        v += 0.25f * (float) std::sin (2 * PI * 49.0 * (double) i / sr) * (step < 8 ? 1.0f : 0.6f);
        s.l[i] = v;
        s.r[i] = v * 0.9f + 0.03f * ud (rng);
    }
    return s;
}

// formant "vocal": pulse train through three resonators with syllable envelope
static Stereo vocal (size_t n, double sr)
{
    Stereo s { std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f) };
    struct Res { double f, bw, y1 = 0, y2 = 0; };
    Res r[3] = { { 650, 80 }, { 1100, 90 }, { 2600, 120 } };
    double phase = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        const double f0 = 180.0 + 30.0 * std::sin (2 * PI * 0.7 * t);
        phase += f0 / sr;
        const double src = (phase >= 1.0) ? 1.0 : 0.0;
        if (phase >= 1.0) phase -= 1.0;
        double out = 0;
        for (auto& q : r)
        {
            const double R = std::exp (-PI * q.bw / sr), th = 2 * PI * q.f / sr;
            const double y = src + 2 * R * std::cos (th) * q.y1 - R * R * q.y2;
            q.y2 = q.y1; q.y1 = y;
            out += y * 0.05;
        }
        const double env = 0.5 + 0.5 * std::sin (2 * PI * 2.3 * t);
        s.l[i] = s.r[i] = (float) (out * env * env);
    }
    return s;
}

// independent BS.1770 K-weighting (48 kHz coefficients) for loudness checks
static double kLoudness (const Stereo& s, size_t from, size_t to)
{
    double sum = 0.0;
    for (int c = 0; c < 2; ++c)
    {
        const auto& v = c == 0 ? s.l : s.r;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0, z1 = 0, z2 = 0, w1 = 0, w2 = 0;
        for (size_t i = 0; i < to; ++i)
        {
            const double x = v[i];
            const double y = 1.53512485958697 * x - 2.69169618940638 * x1 + 1.19839281085285 * x2 + 1.69065929318241 * y1 - 0.73248077421585 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            const double z = y - 2 * z1 + z2 + 1.99004745483398 * w1 - 0.99007225036621 * w2;
            z2 = z1; z1 = y; w2 = w1; w1 = z;
            if (i >= from) sum += z * z;
        }
    }
    return 10.0 * std::log10 (sum / (double) (to - from) + 1e-20) - 0.691;
}

static double rmsDb (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return 10.0 * std::log10 (s / (double) (to - from) + 1e-30);
}

static float peak (const Stereo& s, size_t from = 0)
{
    float m = 0; for (size_t i = from; i < s.l.size(); ++i) m = std::max (m, std::max (std::abs (s.l[i]), std::abs (s.r[i]))); return m;
}

static bool finite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

static bool bitExact (const Stereo& a, const Stereo& b)
{
    return std::memcmp (a.l.data(), b.l.data(), a.l.size() * sizeof (float)) == 0
        && std::memcmp (a.r.data(), b.r.data(), a.r.size() * sizeof (float)) == 0;
}

// Goertzel magnitude of a single frequency
static double tone (const std::vector<float>& v, size_t from, size_t to, double f, double sr)
{
    const double w = 2 * PI * f / sr, cw = 2 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < to; ++i) { const double s0 = v[i] + cw * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt (s1 * s1 + s2 * s2 - cw * s1 * s2) / (double) (to - from) * 2.0;
}

int main()
{
    const double SR = 48000.0;

    std::printf ("-- GLOW 0 = bit-exact passthrough\n");
    {
        bool all = true;
        for (int m = 0; m < numModes; ++m)
            for (float contrast : { -1.0f, 0.0f, 1.0f })
                for (float colour : { -1.0f, 0.7f })
                    for (bool ag : { true, false })
                    {
                        Params p; p.mode = (Mode) m; p.glow = 0.0f; p.contrast = contrast; p.colour = colour; p.autoGain = ag;
                        auto x = drumLoop (24000, SR, 5);
                        auto y = run (p, x, SR, 333);
                        all = all && bitExact (x, y);
                    }
        CHECK (all, "glow 0: output identical to input (all modes, contrast, colour, auto-gain)");

        // after a ramp down from 80 % the engine settles back to exact passthrough
        Params p; p.mode = Mode::loud; p.glow = 0.8f;
        Engine e; e.setParams (p); e.prepare (SR, 256);
        run (e, pink ((size_t) SR, 0.5f, 3), 256);
        p.glow = 0.0f; e.setParams (p);
        run (e, pink ((size_t) SR, 0.5f, 4), 256);
        auto x = pink (20000, 0.5f, 6);
        CHECK (bitExact (x, run (e, x, 256)), "glow ramped to 0 returns to bit-exact passthrough");

        // mono, odd block sizes, 192 kHz
        Params pm; pm.glow = 0.0f;
        Engine em; em.setParams (pm); em.prepare (192000.0, 77);
        std::vector<float> mono (5000), ref;
        std::mt19937 rng (1); std::uniform_real_distribution<float> ud (-1.0f, 1.0f);
        for (auto& s : mono) s = ud (rng);
        ref = mono;
        for (size_t pos = 0; pos < mono.size(); pos += 77) { float* c[1] = { mono.data() + pos }; em.process (c, 1, (int) std::min<size_t> (77, mono.size() - pos)); }
        CHECK (std::memcmp (mono.data(), ref.data(), mono.size() * sizeof (float)) == 0, "glow 0 bit-exact on a mono bus at 192 kHz");
    }

    std::printf ("-- auto-gain keeps the loudness within +-1 dB\n");
    {
        const size_t n = (size_t) (SR * 14.0), from = (size_t) (SR * 9.0);
        struct Sig { const char* name; Stereo s; };
        Sig sigs[] = { { "pink -18", pink (n, 0.35f, 11) }, { "drums", drumLoop (n, SR, 12) }, { "vocal", vocal (n, SR) } };
        bool ok = true;
        for (auto& sg : sigs)
        {
            const double inL = kLoudness (sg.s, from, n);
            for (int m = 0; m < numModes; ++m)
                for (float g : { 0.5f, 1.0f })
                {
                    Params p; p.mode = (Mode) m; p.glow = g; p.autoGain = true;
                    auto y = run (p, sg.s, SR);
                    const double d = kLoudness (y, from, n) - inL;
                    std::printf ("     %-8s %-6s glow %3d%%  dLUFS %+5.2f  (RMS %+5.2f)\n", sg.name, modeNames[(size_t) m], (int) (g * 100), d,
                                 rmsDb (y.l, from, n) - rmsDb (sg.s.l, from, n));
                    ok = ok && std::abs (d) <= 1.0 && finite (y);
                }
        }
        CHECK (ok, "auto-gain: every mode at 50 % and 100 % within +-1 dB (BS.1770 K-weighted)");

        // auto-gain off: Loud mode really is louder
        Params p; p.mode = Mode::loud; p.glow = 1.0f; p.autoGain = false;
        auto y = run (p, sigs[0].s, SR);
        const double d = kLoudness (y, from, n) - kLoudness (sigs[0].s, from, n);
        std::printf ("     auto-gain off, Loud 100 %%: %+5.2f dB\n", d);
        CHECK (d > 3.0, "auto-gain off: Loud mode raises the loudness");
    }

    std::printf ("-- limiter ceiling\n");
    {
        bool ok = true;
        for (int m = 0; m < numModes; ++m)
        {
            Params p; p.mode = (Mode) m; p.glow = 1.0f; p.autoGain = false;
            auto x = drumLoop ((size_t) SR * 3, SR, 2);
            for (auto* v : { &x.l, &x.r }) for (auto& s : *v) s *= 1.6f;    // hot input, peaks above 0 dBFS
            auto y = run (p, x, SR);
            const float pk = peak (y, (size_t) SR);
            std::printf ("     %-6s peak %.3f (limit weight %.2f)\n", modeNames[(size_t) m], pk, voiceFor ((Mode) m).limit);
            const float inPk = peak (x, (size_t) SR);
            if (voiceFor ((Mode) m).limit >= 0.999f) ok = ok && pk <= 0.9661f + 1e-4f;
            ok = ok && pk <= std::max (0.9661f, inPk) + 1e-3f;
            std::printf ("            input peak %.3f\n", inPk);
        }
        CHECK (ok, "Loud never exceeds -0.3 dBFS; no mode adds peaks above max (ceiling, input peak)");
    }

    std::printf ("-- the processors do what they say\n");
    {
        // upward compression lifts quiet material (auto-gain off)
        Params p; p.mode = Mode::warm; p.glow = 1.0f; p.autoGain = false;
        auto quiet = pink ((size_t) SR * 3, 0.02f, 21);
        auto y = run (p, quiet, SR);
        const double lift = rmsDb (y.l, (size_t) SR, y.l.size()) - rmsDb (quiet.l, (size_t) SR, quiet.l.size());
        std::printf ("     quiet pink lift %.2f dB\n", lift);
        CHECK (lift > 3.0, "upward compression lifts quiet signals");

        // FAT adds the octave (2nd harmonic) of a 55 Hz sub
        Stereo sub { std::vector<float> ((size_t) SR * 2), std::vector<float> ((size_t) SR * 2) };
        for (size_t i = 0; i < sub.l.size(); ++i) sub.l[i] = sub.r[i] = (float) (0.4 * std::sin (2 * PI * 55.0 * (double) i / SR));
        Params pf; pf.mode = Mode::fat; pf.glow = 1.0f; pf.autoGain = false;
        auto yf = run (pf, sub, SR);
        const double h2 = tone (yf.l, (size_t) SR, sub.l.size(), 110.0, SR), h2dry = tone (sub.l, (size_t) SR, sub.l.size(), 110.0, SR);
        std::printf ("     110 Hz: dry %.5f  fat %.5f\n", h2dry, h2);
        CHECK (h2 > 0.02 && h2 > 100 * h2dry, "Fat mode adds the octave above an 808 sub");

        // BRIGHT adds harmonics above the exciter corner for a 2.5 kHz tone
        Stereo t { std::vector<float> ((size_t) SR * 2), std::vector<float> ((size_t) SR * 2) };
        for (size_t i = 0; i < t.l.size(); ++i) t.l[i] = t.r[i] = (float) (0.2 * std::sin (2 * PI * 2500.0 * (double) i / SR));
        Params pb; pb.mode = Mode::bright; pb.glow = 1.0f; pb.autoGain = false;
        auto yb = run (pb, t, SR);
        const double h = tone (yb.l, (size_t) SR, t.l.size(), 5000.0, SR) + tone (yb.l, (size_t) SR, t.l.size(), 10000.0, SR);
        std::printf ("     5k + 10k harmonics: %.5f\n", h);
        CHECK (h > 0.003, "Bright mode excites the top end");

        // colour tilts the spectrum
        Params pc; pc.mode = Mode::warm; pc.glow = 0.6f; pc.autoGain = false; pc.colour = -1.0f;
        auto pn = pink ((size_t) SR * 2, 0.2f, 33);
        auto dark = run (pc, pn, SR);
        pc.colour = 1.0f;
        auto bright = run (pc, pn, SR);
        const double hiDark = tone (dark.l, (size_t) SR, pn.l.size(), 9000.0, SR), hiBright = tone (bright.l, (size_t) SR, pn.l.size(), 9000.0, SR);
        CHECK (hiBright > hiDark * 1.5, "colour +100 % is brighter than -100 %");
    }

    std::printf ("-- mode switch and knob moves are click-free\n");
    {
        Params p; p.mode = Mode::warm; p.glow = 0.7f; p.autoGain = true;
        Engine e; e.setParams (p); e.prepare (SR, 128);
        Stereo s { std::vector<float> ((size_t) SR * 4), std::vector<float> ((size_t) SR * 4) };
        for (size_t i = 0; i < s.l.size(); ++i) s.l[i] = s.r[i] = (float) (0.3 * std::sin (2 * PI * 220.0 * (double) i / SR));
        float maxStep = 0.0f, prev = 0.0f;
        for (size_t pos = 0; pos < s.l.size(); pos += 128)
        {
            const int blk = (int) (pos / 128);
            if (blk % 250 == 0) { p.mode = (Mode) ((blk / 250) % numModes); p.glow = (blk / 250) % 2 ? 1.0f : 0.0f; e.setParams (p); }
            float* c[2] = { s.l.data() + pos, s.r.data() + pos };
            e.process (c, 2, 128);
            for (int i = 0; i < 128; ++i) { if (pos + (size_t) i > 4800) maxStep = std::max (maxStep, std::abs (s.l[pos + (size_t) i] - prev)); prev = s.l[pos + (size_t) i]; }
        }
        const float sineStep = (float) (0.3 * 2 * PI * 220.0 / SR);
        std::printf ("     max sample step %.4f (pure sine %.4f)\n", maxStep, sineStep);
        CHECK (maxStep < sineStep * 2.5f, "no clicks when modes and GLOW jump");
    }

    std::printf ("-- robustness: fuzzing (all rates, random params / block sizes / signals)\n");
    {
        bool ok = true;
        std::mt19937 rng (99);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Engine e; Params p; e.setParams (p); e.prepare (sr, 4096);
            std::vector<float> l (4096), r (4096);
            for (int it = 0; it < 300; ++it)
            {
                p.mode = (Mode) (rng() % numModes); p.glow = u (rng); p.contrast = u (rng) * 2 - 1; p.colour = u (rng) * 2 - 1;
                p.outputDb = -24.0f + 36.0f * u (rng); p.autoGain = (rng() & 1) != 0;
                e.setParams (p);
                const int n = 1 + (int) (rng() % 4096);
                const int kind = (int) (rng() % 5);
                for (int i = 0; i < n; ++i)
                {
                    float v = 0.0f;
                    switch (kind)
                    {
                        case 0: v = (u (rng) * 2 - 1) * 8.0f; break;       // +18 dB noise
                        case 1: v = i == 0 ? 1.0f : 0.0f; break;          // impulse
                        case 2: v = 1.0f; break;                          // DC
                        case 3: v = 0.0f; break;                          // silence
                        default: v = 1e-30f * (u (rng) - 0.5f); break;    // denormal-range
                    }
                    l[(size_t) i] = v; r[(size_t) i] = -v;
                }
                float* c[2] = { l.data(), r.data() };
                e.process (c, (rng() & 3) == 0 ? 1 : 2, n);
                for (int i = 0; i < n; ++i) ok = ok && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]) && std::abs (l[(size_t) i]) < 100.0f;
            }
        }
        CHECK (ok, "no NaN / inf / blow-up under fuzzing at 44.1 - 192 kHz, blocks 1 - 4096");
    }

    std::printf ("-- 60 s stress at extreme settings\n");
    {
        Params p; p.mode = Mode::loud; p.glow = 1.0f; p.contrast = 1.0f; p.colour = 1.0f; p.outputDb = 12.0f; p.autoGain = true;
        Engine e; e.setParams (p); e.prepare (SR, 512);
        std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 1.0f);
        std::vector<float> l (512), r (512);
        bool ok = true; float pk = 0.0f, inPk = 0.0f;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < (int) (SR * 60 / 512); ++b)
        {
            const bool on = (b / 300) % 3 != 2;
            for (int i = 0; i < 512; ++i)
            {
                l[(size_t) i] = on ? 2.0f * nd (rng) : 0.0f; r[(size_t) i] = on ? 2.0f * nd (rng) : 0.0f;
                inPk = std::max (inPk, std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
            }
            if (b % 400 == 0) { p.mode = (Mode) ((b / 400) % numModes); p.contrast = -p.contrast; e.setParams (p); }
            float* c[2] = { l.data(), r.data() };
            e.process (c, 2, 512);
            for (int i = 0; i < 512; ++i)
            {
                ok = ok && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
                pk = std::max (pk, std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("     peak %.3f, %.2f s CPU for 60 s of audio (%.2f %% of a core)\n", pk, secs, secs / 60.0 * 100.0);
        std::printf ("     input peak %.3f, output gain +12 dB\n", inPk);
        CHECK (ok && pk <= 3.982f * std::max (inPk, 0.9661f) + 1e-2f, "60 s stress: finite, peaks bounded by max (ceiling, input peak) + output gain");
        CHECK (e.getLatencySamples() == 0, "zero latency");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
