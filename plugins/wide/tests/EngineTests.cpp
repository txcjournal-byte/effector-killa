// Headless tests for the TrapWin WIDE engine.
#include "../Source/DSP/WideEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace widefx;
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

static Stereo noise (size_t n, unsigned seed, float amp = 0.3f, float corr = 0.0f)
{
    std::mt19937 rng (seed); std::normal_distribution<float> nd (0.0f, 1.0f);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i)
    {
        const float a = nd (rng), b = nd (rng);
        s.l[i] = amp * a;
        s.r[i] = amp * (corr * a + std::sqrt (1.0f - corr * corr) * b);
    }
    return s;
}

static Stereo sines (size_t n, double sr, double f, float ampL, float ampR, double phaseR = 0.0)
{
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i)
    {
        const double t = 2 * PI * f * (double) i / sr;
        s.l[i] = ampL * (float) std::sin (t);
        s.r[i] = ampR * (float) std::sin (t + phaseR);
    }
    return s;
}

static bool bitExact (const Stereo& a, const Stereo& b)
{
    return std::memcmp (a.l.data(), b.l.data(), a.l.size() * sizeof (float)) == 0
        && std::memcmp (a.r.data(), b.r.data(), a.r.size() * sizeof (float)) == 0;
}

static double rmsMid (const Stereo& s, size_t from, size_t to)
{
    double a = 0; for (size_t i = from; i < to; ++i) { const double m = 0.5 * (s.l[i] + s.r[i]); a += m * m; } return std::sqrt (a / (double) (to - from));
}
static double rmsSide (const Stereo& s, size_t from, size_t to)
{
    double a = 0; for (size_t i = from; i < to; ++i) { const double m = 0.5 * (s.l[i] - s.r[i]); a += m * m; } return std::sqrt (a / (double) (to - from));
}
static double db (double v) { return 20.0 * std::log10 (v + 1e-20); }

static double corr (const Stereo& s, size_t from, size_t to)
{
    double lr = 0, ll = 0, rr = 0;
    for (size_t i = from; i < to; ++i) { lr += (double) s.l[i] * s.r[i]; ll += (double) s.l[i] * s.l[i]; rr += (double) s.r[i] * s.r[i]; }
    return lr / std::sqrt (ll * rr + 1e-30);
}

static float meterAfter (const Stereo& x, double sr)
{
    Engine e; Params p; e.setParams (p); e.prepare (sr, 512);
    run (e, x);
    return e.getCorrelation();
}

int main()
{
    const double SR = 48000.0;

    std::printf ("-- width 100 %% = bit-exact passthrough\n");
    {
        bool ok = true;
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int block : { 1, 64, 333, 4096 })
            {
                Params p;   // all defaults: widths 100 %
                auto x = noise (12000, 1 + (unsigned) block, 0.5f, 0.3f);
                ok = ok && bitExact (x, run (p, x, sr, block));
            }
        CHECK (ok, "default settings (width 100 %) leave the audio bit-identical (44.1 - 192 kHz, blocks 1 - 4096)");

        Params p; p.lowXHz = 90.0f; p.highXHz = 7000.0f; p.haasSide = HaasSide::left; p.monoBelowHz = 20.0f;
        auto x = noise (20000, 9, 0.5f, -0.4f);
        CHECK (bitExact (x, run (p, x, SR, 256)), "width 100 % stays bit-exact with moved crossovers / Haas side / mono-below OFF");

        // after real processing, returning every control to neutral returns to bit-exact
        Params q; q.widthLow = 0.0f; q.widthHigh = 1.8f; q.stereoize = 0.6f; q.haasMs = 12.0f; q.monoBelowHz = 150.0f; q.balance = 0.3f;
        Engine e; e.setParams (q); e.prepare (SR, 256);
        run (e, noise ((size_t) SR, 3), 256);
        e.setParams (Params {});
        run (e, noise ((size_t) SR, 4), 256);
        auto y = noise (20000, 5);
        CHECK (bitExact (y, run (e, y, 256)), "back to neutral after processing: bit-exact again");
    }

    std::printf ("-- mono below\n");
    {
        Params p; p.monoBelowHz = 200.0f;
        const size_t n = (size_t) SR * 2, from = (size_t) SR;
        auto lowSide = sines (n, SR, 40.0, 0.5f, -0.5f);                // pure side at 40 Hz
        auto y = run (p, lowSide, SR);
        const double att = db (rmsSide (y, from, n)) - db (rmsSide (lowSide, from, n));
        std::printf ("     side @ 40 Hz: %.1f dB\n", att);
        CHECK (att < -40.0, "side content far below the corner is removed (lows mono)");

        auto lowMid = sines (n, SR, 40.0, 0.5f, 0.5f);
        y = run (p, lowMid, SR);
        const double midDb = db (rmsMid (y, from, n)) - db (rmsMid (lowMid, from, n));
        std::printf ("     mid @ 40 Hz: %+.2f dB\n", midDb);
        CHECK (std::abs (midDb) < 0.1, "the mono low end keeps its level (all-pass, no dip)");

        auto hiSide = sines (n, SR, 3000.0, 0.5f, -0.5f);
        y = run (p, hiSide, SR);
        const double hiDb = db (rmsSide (y, from, n)) - db (rmsSide (hiSide, from, n));
        std::printf ("     side @ 3 kHz: %+.2f dB\n", hiDb);
        CHECK (std::abs (hiDb) < 0.2, "stereo above the corner is untouched");

        // wide noise: the low-passed output is mono (correlation +1)
        auto wide = noise (n, 7, 0.4f, -0.5f);
        y = run (p, wide, SR);
        Stereo lp { std::vector<float> (n), std::vector<float> (n) };
        float zl[4] = {}, zr[4] = {};
        const float a = (float) std::exp (-2 * PI * 40.0 / SR);
        for (size_t i = 0; i < n; ++i)
        {
            float vl = y.l[i], vr = y.r[i];
            for (int k = 0; k < 4; ++k) { zl[k] += (1 - a) * (vl - zl[k]); vl = zl[k]; zr[k] += (1 - a) * (vr - zr[k]); vr = zr[k]; }
            lp.l[i] = vl; lp.r[i] = vr;
        }
        const double c = corr (lp, from, n);
        std::printf ("     correlation of the < 40 Hz (4-pole) output: %.4f (input %.2f)\n", c, corr (wide, from, n));
        CHECK (c > 0.99, "anti-correlated noise: everything below the corner sums to mono");
    }

    std::printf ("-- correlation meter\n");
    {
        const size_t n = (size_t) SR;
        auto same = noise (n, 11, 0.3f, 1.0f);
        const float cSame = meterAfter (same, SR);
        Stereo inv = same; for (auto& v : inv.r) v = -v;
        const float cInv = meterAfter (inv, SR);
        const float cInd = meterAfter (noise (n, 12, 0.3f, 0.0f), SR);
        const float cHalf = meterAfter (noise (n, 13, 0.3f, 0.5f), SR);
        const float cQuad = meterAfter (sines (n, SR, 440.0, 0.5f, 0.5f, PI / 2), SR);
        const float cSine = meterAfter (sines (n, SR, 440.0, 0.5f, 0.2f), SR);
        Stereo leftOnly = same; for (auto& v : leftOnly.r) v = 0.0f;
        const float cLeft = meterAfter (leftOnly, SR);
        std::printf ("     L=R %.4f  L=-R %.4f  indep %.4f  rho0.5 %.4f  sin/cos %.4f  L only %.4f\n", cSame, cInv, cInd, cHalf, cQuad, cLeft);
        CHECK (std::abs (cSame - 1.0f) < 1e-4f, "identical channels read +1");
        CHECK (std::abs (cInv + 1.0f) < 1e-4f, "polarity-inverted channels read -1");
        CHECK (std::abs (cInd) < 0.1f, "independent noise reads ~0");
        CHECK (std::abs (cHalf - 0.5f) < 0.1f, "partially correlated noise (rho 0.5) reads ~0.5");
        CHECK (std::abs (cQuad) < 0.02f, "90 degree phase shift reads ~0");
        CHECK (std::abs (cSine - 1.0f) < 1e-3f, "in-phase sines at different levels read +1");
        CHECK (std::abs (cLeft) < 1e-6f, "one silent channel reads 0");
        CHECK (Engine::correlationOf (2.0, 2.0, 2.0) == 1.0f && Engine::correlationOf (-1.0, 1.0, 1.0) == -1.0f
               && Engine::correlationOf (0.0, 0.0, 0.0) == 0.0f, "correlation helper: +1, -1, silence = 0");
    }

    std::printf ("-- width\n");
    {
        const size_t n = (size_t) SR, from = (size_t) (SR * 0.25);
        auto x = noise (n, 21, 0.3f, 0.2f);
        Params p; p.widthLow = p.widthMid = p.widthHigh = 0.0f;
        auto y = run (p, x, SR);
        float diff = 0; for (size_t i = from; i < n; ++i) diff = std::max (diff, std::abs (y.l[i] - y.r[i]));
        CHECK (diff < 1e-6f, "width 0 % in every band = mono (L == R)");
        const double midErr = db (rmsMid (y, from, n)) - db (rmsMid (x, from, n));
        CHECK (std::abs (midErr) < 0.01, "width never touches the mid");

        p.widthLow = p.widthMid = p.widthHigh = 2.0f;
        y = run (p, x, SR);
        const double g = rmsSide (y, from, n) / rmsSide (x, from, n);
        std::printf ("     width 200 %%: side x%.4f\n", g);
        CHECK (std::abs (g - 2.0) < 0.01, "width 200 % doubles the side signal exactly");

        // per band
        Params pb; pb.widthLow = 0.0f; pb.lowXHz = 200.0f; pb.highXHz = 3000.0f;
        auto lo = run (pb, sines (n, SR, 50.0, 0.5f, -0.5f), SR);
        auto hi = run (pb, sines (n, SR, 9000.0, 0.5f, -0.5f), SR);
        const double loDb = db (rmsSide (lo, from, n)) - db (0.5 / std::sqrt (2.0));
        const double hiDb = db (rmsSide (hi, from, n)) - db (0.5 / std::sqrt (2.0));
        std::printf ("     low band width 0: side @ 50 Hz %.1f dB, @ 9 kHz %+.2f dB\n", loDb, hiDb);
        CHECK (loDb < -20.0 && std::abs (hiDb) < 0.3, "per-band width: the low band narrows, the highs stay");
    }

    std::printf ("-- stereoize, Haas, balance\n");
    {
        const size_t n = (size_t) SR, from = (size_t) (SR * 0.25);
        auto mono = noise (n, 31, 0.3f, 1.0f);
        Params p; p.stereoize = 1.0f;
        auto y = run (p, mono, SR);
        const double c = corr (y, from, n);
        Params p0; p0.stereoize = 0.001f;   // same processed path (incl. the crossover all-pass), almost no stereoize
        auto y0 = run (p0, mono, SR);
        float sumErr = 0;
        for (size_t i = from; i < n; ++i) sumErr = std::max (sumErr, std::abs ((y.l[i] + y.r[i]) - (y0.l[i] + y0.r[i])));
        std::printf ("     stereoize 100 %% on mono noise: correlation %.3f, mono-sum error %.2e\n", c, sumErr);
        CHECK (c < 0.85, "stereoize widens a mono source");
        CHECK (sumErr < 1e-5f, "stereoize is mono-compatible: it adds nothing to L+R");

        Params ph; ph.haasMs = 10.0f; ph.haasSide = HaasSide::right;
        Stereo imp { std::vector<float> (2000, 0.0f), std::vector<float> (2000, 0.0f) };
        imp.l[100] = imp.r[100] = 1.0f;
        auto yh = run (ph, imp, SR);
        float err = 0, before = 0;
        for (size_t i = 0; i + 480 < yh.r.size(); ++i) err = std::max (err, std::abs (yh.r[i + 480] - yh.l[i]));
        for (size_t i = 0; i < 580; ++i) before = std::max (before, std::abs (yh.r[i]));
        std::printf ("     Haas 10 ms right: |R(t+10ms) - L(t)| max %.2e, R before the delay %.2e\n", err, before);
        CHECK (err < 1e-6f && before < 1e-6f, "Haas delays only the chosen side by exactly 10 ms (480 samples)");

        auto st = noise (n, 41, 0.3f, 0.1f);
        Params pm; pm.balance = -1.0f;
        auto ym = run (pm, st, SR);
        Params ps; ps.balance = 1.0f;
        auto ys = run (ps, st, SR);
        CHECK (rmsSide (ym, from, n) < 1e-6 && rmsMid (ys, from, n) < 1e-6, "M/S balance -100 = mid only, +100 = side only");
    }

    std::printf ("-- click-free control jumps\n");
    {
        Engine e; Params p; e.setParams (p); e.prepare (SR, 128);
        auto s = sines ((size_t) SR * 3, SR, 220.0, 0.4f, -0.1f);
        float maxStep = 0.0f, prevL = s.l[0], prevR = s.r[0];
        for (size_t pos = 0; pos < s.l.size(); pos += 128)
        {
            const int blk = (int) (pos / 128);
            if (blk % 150 == 75)
            {
                const bool on = (blk / 150) % 2 == 0;
                p.widthLow = on ? 0.0f : 1.0f; p.widthMid = on ? 2.0f : 1.0f; p.balance = on ? 0.5f : 0.0f;
                p.monoBelowHz = on ? 300.0f : 0.0f; p.stereoize = on ? 1.0f : 0.0f; p.outputDb = on ? -6.0f : 0.0f;
                e.setParams (p);
            }
            float* c[2] = { s.l.data() + pos, s.r.data() + pos };
            e.process (c, 2, 128);
            for (size_t i = pos; i < pos + 128 && i < s.l.size(); ++i)
            {
                maxStep = std::max (maxStep, std::max (std::abs (s.l[i] - prevL), std::abs (s.r[i] - prevR)));
                prevL = s.l[i]; prevR = s.r[i];
            }
        }
        const float sineStep = (float) (0.4 * 2 * PI * 220.0 / SR);
        std::printf ("     max sample step %.4f (dry sine %.4f)\n", maxStep, sineStep);
        CHECK (maxStep < sineStep * 3.0f, "no clicks when width / balance / mono-below / stereoize jump");
    }

    std::printf ("-- robustness\n");
    {
        bool ok = true;
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Engine e; Params p; e.setParams (p); e.prepare (sr, 4096);
            std::vector<float> l (4096), r (4096);
            for (int it = 0; it < 300; ++it)
            {
                p.lowXHz = 40.0f + 960.0f * u (rng); p.highXHz = 1000.0f + 11000.0f * u (rng);
                p.widthLow = 2 * u (rng); p.widthMid = 2 * u (rng); p.widthHigh = 2 * u (rng);
                p.monoBelowHz = (rng() & 1) ? 0.0f : 20.0f + 480.0f * u (rng);
                p.haasMs = (rng() & 1) ? 0.0f : 30.0f * u (rng); p.haasSide = (rng() & 1) ? HaasSide::left : HaasSide::right;
                p.balance = 2 * u (rng) - 1; p.stereoize = u (rng); p.outputDb = -24.0f + 36.0f * u (rng);
                if ((rng() % 6) == 0) p = Params {};
                e.setParams (p);
                const int n = 1 + (int) (rng() % 4096);
                const int kind = (int) (rng() % 5);
                for (int i = 0; i < n; ++i)
                {
                    float v = 0, w = 0;
                    switch (kind)
                    {
                        case 0: v = (u (rng) * 2 - 1) * 8.0f; w = (u (rng) * 2 - 1) * 8.0f; break;
                        case 1: v = i == 0 ? 1.0f : 0.0f; w = -v; break;
                        case 2: v = 1.0f; w = -1.0f; break;
                        case 3: break;
                        default: v = 1e-30f * (u (rng) - 0.5f); w = v; break;
                    }
                    l[(size_t) i] = v; r[(size_t) i] = w;
                }
                float* c[2] = { l.data(), r.data() };
                e.process (c, (rng() % 5) == 0 ? 1 : 2, n);
                for (int i = 0; i < n; ++i) ok = ok && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]) && std::abs (l[(size_t) i]) < 200.0f;
            }
            float sl[512], sr2[512];
            e.copyScope (sl, sr2, 512);
            for (int i = 0; i < 512; ++i) ok = ok && std::isfinite (sl[i]) && std::isfinite (sr2[i]);
            ok = ok && std::isfinite (e.getCorrelation());
        }
        CHECK (ok, "no NaN / inf / blow-up under fuzzing at 44.1 - 192 kHz, blocks 1 - 4096, mono + stereo");

        Params p; p.widthLow = 2.0f; p.widthMid = 2.0f; p.widthHigh = 2.0f; p.stereoize = 1.0f; p.haasMs = 30.0f; p.balance = 1.0f;
        p.monoBelowHz = 500.0f; p.outputDb = 12.0f; p.lowXHz = 1000.0f; p.highXHz = 1000.0f;
        Engine e; e.setParams (p); e.prepare (SR, 512);
        std::mt19937 r2 (8); std::normal_distribution<float> nd (0.0f, 1.0f);
        std::vector<float> l (512), r (512);
        bool fin = true; float pk = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < (int) (SR * 60 / 512); ++b)
        {
            for (int i = 0; i < 512; ++i) { l[(size_t) i] = 2.0f * nd (r2); r[(size_t) i] = 2.0f * nd (r2); }
            if (b % 500 == 0) { p.haasSide = p.haasSide == HaasSide::left ? HaasSide::right : HaasSide::left; e.setParams (p); }
            float* c[2] = { l.data(), r.data() };
            e.process (c, 2, 512);
            for (int i = 0; i < 512; ++i) { fin = fin && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]); pk = std::max (pk, std::abs (l[(size_t) i])); }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("     60 s stress: peak %.2f, %.2f s CPU (%.2f %% of a core)\n", pk, secs, secs / 60.0 * 100.0);
        CHECK (fin && pk < 200.0f, "60 s at extreme settings: finite and bounded");
        CHECK (e.getLatencySamples() == 0, "zero latency");

        // mono bus
        Engine em; Params pmn; pmn.widthLow = 0.0f; pmn.outputDb = -6.0206f; em.setParams (pmn); em.prepare (SR, 64);
        std::vector<float> m (4800, 0.5f);
        for (size_t pos = 0; pos < m.size(); pos += 64) { float* c[1] = { m.data() + pos }; em.process (c, 1, 64); }
        CHECK (std::abs (m.back() - 0.25f) < 1e-4f, "mono bus: passes through with the output gain");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
