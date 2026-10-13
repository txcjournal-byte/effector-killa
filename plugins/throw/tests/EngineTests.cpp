// Headless tests for the TrapWin THROW engine.
#include "../Source/DSP/ThrowEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace throwfx;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Params clean()
{
    Params p;
    p.sync = true; p.bpm = 140.0; p.division = defaultDivision; p.mode = Mode::stereo;
    p.feedback = 0.0f; p.drive = 0.0f; p.wobble = 0.0f; p.lowCutHz = 20.0f; p.highCutHz = 20000.0f;
    p.duck = 0.0f; p.width = 1.0f; p.mix = 1.0f; p.outputDb = 0.0f;
    return p;
}

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
    Engine e; e.prepare (sr, block); e.setParams (p);
    return run (e, std::move (x), block);
}

static Stereo impulse (size_t n, float a = 1.0f)
{
    Stereo s { std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f) };
    s.l[0] = s.r[0] = a;
    return s;
}

static size_t argmaxAbs (const std::vector<float>& v, size_t from, size_t to)
{
    size_t best = from; float bv = -1;
    for (size_t i = from; i < std::min (to, v.size()); ++i) if (std::abs (v[i]) > bv) { bv = std::abs (v[i]); best = i; }
    return best;
}

static float peakIn (const std::vector<float>& v, size_t from, size_t to)
{
    float m = 0; for (size_t i = from; i < std::min (to, v.size()); ++i) m = std::max (m, std::abs (v[i])); return m;
}

static double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (to - from));
}

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("-- echo time @ %.0f Hz, 140 BPM\n", sr);
        for (int div : { 10, 9 }) // 1/4, 1/8D
        {
            auto p = clean(); p.division = div;
            const double expect = divisionBeats[(size_t) div] * 60.0 / 140.0 * sr;
            auto y = run (p, impulse ((size_t) (sr * 1.0), 0.5f), sr);
            const size_t at = argmaxAbs (y.l, 1, y.l.size());
            char msg[160];
            std::snprintf (msg, sizeof msg, "%s echo at %zu, expected %.2f (+-1)", divisionNames[(size_t) div], at, expect);
            CHECK (std::abs ((double) at - expect) <= 1.0 && std::abs (y.l[at] - 0.5f) < 1e-4f, msg);
            CHECK (std::abs (y.l[0]) < 1e-6f, "no dry signal at mix 100 %");
        }
    }

    const double SR = 48000.0;
    const size_t T = (size_t) std::lround (60.0 / 140.0 * SR); // 1/4 at 140 BPM

    std::printf ("-- feedback\n");
    {
        auto p = clean(); p.feedback = 0.5f;
        auto y = run (p, impulse (T * 6 + 100), SR);
        float a[5];
        for (int k = 1; k <= 5; ++k) a[k - 1] = peakIn (y.l, T * (size_t) k - 4, T * (size_t) k + 4);
        bool decays = true;
        for (int k = 1; k < 5; ++k) decays = decays && a[k] < a[k - 1] && std::abs (a[k] / a[k - 1] - 0.5f) < 0.03f;
        std::printf ("     taps %.3f %.3f %.3f %.3f %.3f\n", a[0], a[1], a[2], a[3], a[4]);
        CHECK (decays, "echoes decay by the feedback amount (0.5 per repeat)");
    }

    std::printf ("-- ping-pong\n");
    {
        auto p = clean(); p.feedback = 0.7f; p.mode = Mode::pingPong;
        auto y = run (p, impulse (T * 5 + 100), SR);
        bool alt = true;
        for (int k = 1; k <= 4; ++k)
        {
            const float l = peakIn (y.l, T * (size_t) k - 4, T * (size_t) k + 4);
            const float r = peakIn (y.r, T * (size_t) k - 4, T * (size_t) k + 4);
            std::printf ("     tap %d  L %.3f  R %.3f\n", k, l, r);
            const bool left = (k % 2) == 1;
            alt = alt && (left ? (l > 0.1f && r < 1e-4f) : (r > 0.1f && l < 1e-4f));
        }
        CHECK (alt, "ping-pong taps alternate L, R, L, R");
    }

    std::printf ("-- ducking\n");
    {
        const size_t n = (size_t) (SR * 2.0);
        Stereo x { std::vector<float> (n), std::vector<float> (n) };
        for (size_t i = 0; i < n; ++i) x.l[i] = x.r[i] = (float) (0.3 * std::sin (2 * PI * 220.0 * (double) i / SR));
        auto p = clean(); p.sync = false; p.timeMs = 100.0f; p.feedback = 0.3f;
        const double open = rms (run (p, x, SR).l, (size_t) SR, (size_t) (SR * 1.5));
        p.duck = 1.0f;
        const double ducked = rms (run (p, x, SR).l, (size_t) SR, (size_t) (SR * 1.5));
        std::printf ("     wet rms open %.4f  ducked %.4f\n", open, ducked);
        CHECK (ducked < open * 0.2, "duck lowers the wet signal while the vocal plays");

        // and the echo comes back once the vocal stops
        Stereo y2 { std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f) };
        for (size_t i = 0; i < (size_t) (SR * 0.5); ++i) y2.l[i] = y2.r[i] = x.l[i];
        auto p2 = clean(); p2.sync = false; p2.timeMs = 400.0f; p2.feedback = 0.6f; p2.duck = 1.0f;
        auto out = run (p2, y2, SR);
        CHECK (peakIn (out.l, (size_t) (SR * 1.0), (size_t) (SR * 1.3)) > 0.05f, "echo recovers after the vocal stops");
    }

    std::printf ("-- throw\n");
    {
        auto p = clean(); p.feedback = 0.5f; p.throwOnly = true; p.throwOn = false;
        auto y = run (p, impulse (T * 4), SR);
        CHECK (peakIn (y.l, 0, y.l.size()) < 1e-7f && peakIn (y.r, 0, y.r.size()) < 1e-7f, "throw-only + throw off: no new input reaches the delay");
        p.throwOn = true;
        y = run (p, impulse (T * 4), SR);
        CHECK (peakIn (y.l, T - 4, T + 4) > 0.5f, "throw on: input is thrown into the delay");

        // throw released: the tail keeps ringing, new input is ignored
        Engine e; e.prepare (SR, 256);
        p.throwOn = true; e.setParams (p);
        auto a = run (e, impulse (T / 2));
        p.throwOn = false; e.setParams (p);
        Stereo b { std::vector<float> (T * 3, 0.0f), std::vector<float> (T * 3, 0.0f) };
        b.l[4800] = b.r[4800] = 1.0f; // a second hit 100 ms after the release
        b = run (e, b);
        const float tail = peakIn (b.l, T / 2 - 8, T / 2 + 8);           // first echo of the thrown hit
        const float leak = peakIn (b.l, 4800 + T - 8, 4800 + T + 8);                    // would-be echo of the 2nd hit
        std::printf ("     tail %.3f  leak %.6f\n", tail, leak);
        CHECK (tail > 0.5f && leak < 1e-4f, "released throw: tail rings out, new input not sent");
        (void) a;
    }

    std::printf ("-- freeze\n");
    {
        auto p = clean(); p.feedback = 0.3f;
        Engine e; e.prepare (SR, 256); e.setParams (p);
        Stereo x { std::vector<float> (T), std::vector<float> (T) };
        for (size_t i = 0; i < T; ++i) x.l[i] = x.r[i] = (float) (0.5 * std::sin (2 * PI * 330.0 * (double) i / SR));
        run (e, x);
        p.freeze = true; e.setParams (p);
        auto held = run (e, Stereo { std::vector<float> (T * 12, 0.3f), std::vector<float> (T * 12, 0.3f) });
        const double early = rms (held.l, T * 2, T * 3), late = rms (held.l, T * 11, T * 12);
        std::printf ("     held rms early %.4f late %.4f\n", early, late);
        CHECK (early > 0.1 && std::abs (late / early - 1.0) < 0.05, "freeze holds the loop without decay or new input");
    }

    std::printf ("-- stability: max feedback, full tape, 60 s noise\n");
    {
        auto p = clean(); p.feedback = maxFeedback; p.drive = 1.0f; p.wobble = 1.0f; p.mode = Mode::pingPong; p.width = 2.0f;
        p.sync = false; p.timeMs = 180.0f;
        Engine e; e.prepare (SR, 512); e.setParams (p);
        std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 0.5f);
        std::vector<float> l (512), r (512);
        bool finite = true; float pk = 0.0f;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < (int) (SR * 60 / 512); ++b)
        {
            for (int i = 0; i < 512; ++i) { const bool on = (b / 200) % 2 == 0; l[(size_t) i] = on ? nd (rng) : 0.0f; r[(size_t) i] = on ? nd (rng) : 0.0f; }
            if (b % 300 == 0) { p.timeMs = 60.0f + (float) ((b / 300) % 7) * 150.0f; e.setParams (p); } // time sweeps
            float* ch[2] = { l.data(), r.data() };
            e.process (ch, 2, 512);
            for (int i = 0; i < 512; ++i)
            {
                finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
                pk = std::max (pk, std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("     peak %.3f, %.2f s CPU for 60 s audio\n", pk, secs);
        CHECK (finite, "no NaN / inf at 110 % feedback");
        CHECK (pk < 6.0f, "self-oscillation stays bounded by the guard clipper");
    }

    std::printf ("-- mix 0 passthrough, mono\n");
    {
        auto p = clean(); p.mix = 0.0f; p.feedback = 0.9f; p.drive = 1.0f; p.wobble = 1.0f;
        const size_t n = 20000;
        Stereo x { std::vector<float> (n), std::vector<float> (n) };
        std::mt19937 rng (3); std::uniform_real_distribution<float> ud (-1.0f, 1.0f);
        for (size_t i = 0; i < n; ++i) { x.l[i] = ud (rng); x.r[i] = ud (rng); }
        auto y = run (p, x, SR);
        float err = 0; for (size_t i = 0; i < n; ++i) err = std::max (err, std::max (std::abs (y.l[i] - x.l[i]), std::abs (y.r[i] - x.r[i])));
        CHECK (err == 0.0f, "mix 0 = bit-exact passthrough");

        // mono bus
        auto pm = clean(); pm.feedback = 0.4f;
        Engine e; e.prepare (44100.0, 128); e.setParams (pm);
        std::vector<float> m (44100, 0.0f); m[0] = 0.5f;
        const size_t Tm = (size_t) std::lround (60.0 / 140.0 * 44100.0);
        for (size_t pos = 0; pos < m.size(); pos += 128) { float* ch[1] = { m.data() + pos }; e.process (ch, 1, (int) std::min<size_t> (128, m.size() - pos)); }
        CHECK (std::abs (m[Tm] - 0.5f) < 1e-4f, "mono I/O works");
    }

    std::printf ("-- tape glide\n");
    {
        auto p = clean(); p.sync = false; p.timeMs = 300.0f; p.feedback = 0.0f;
        Engine e; e.prepare (SR, 256); e.setParams (p);
        run (e, impulse (4800));
        p.timeMs = 600.0f; e.setParams (p);
        run (e, impulse (256));
        const double mid = e.getCurrentDelayMs();
        run (e, impulse ((size_t) SR));
        CHECK (mid > 300.5 && mid < 590.0 && std::abs (e.getCurrentDelayMs() - 600.0) < 0.05, "delay time glides smoothly to the new value");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
