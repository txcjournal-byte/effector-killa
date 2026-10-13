// Headless tests for the TrapWin SNAP engine.
#include "../Source/DSP/SnapEngine.h"
#include "../Source/Presets.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace snapfx;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Stereo run (Engine& e, Stereo x, int block = 512, int numCh = 2)
{
    for (size_t pos = 0; pos < x.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.l.size() - pos);
        float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
        e.process (ch, numCh, n);
    }
    return x;
}

static Stereo run (const Params& p, Stereo x, double sr, int block = 512, int numCh = 2)
{
    Engine e; e.prepare (sr, block); e.setParams (p);
    return run (e, std::move (x), block, numCh);
}

// drum-ish hits: 2 ms attack, exponential decay, tone + noise, one every `period` seconds
static Stereo drums (double sr, double seconds, double period = 0.5, double tau = 0.12, double freq = 120.0, float amp = 0.7f, unsigned seed = 3)
{
    const size_t n = (size_t) (sr * seconds);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = std::fmod ((double) i / sr, period);
        const double att = std::min (1.0, t / 0.002);
        const double env = att * std::exp (-t / tau);
        const float v = (float) (env * (0.8 * std::sin (2.0 * PI * freq * t) + 0.25 * u (rng) * std::exp (-t / 0.01)));
        s.l[i] = amp * v;
        s.r[i] = amp * v * 0.9f;
    }
    return s;
}

static double energy (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < std::min (to, v.size()); ++i) s += (double) v[i] * v[i]; return s;
}

// summed energy over windows [a, b) seconds after each hit, skipping the first hit (settling)
static double windowEnergy (const std::vector<float>& v, double sr, double period, double a, double b)
{
    double s = 0;
    for (double t0 = period; t0 + b < (double) v.size() / sr; t0 += period)
        s += energy (v, (size_t) ((t0 + a) * sr), (size_t) ((t0 + b) * sr));
    return s;
}

static bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

static float peak (const Stereo& s)
{
    float m = 0; for (size_t i = 0; i < s.l.size(); ++i) m = std::max (m, std::max (std::abs (s.l[i]), std::abs (s.r[i]))); return m;
}

static double db (double r) { return 10.0 * std::log10 (std::max (1e-30, r)); }

int main()
{
    char msg[200];

    // ---------------- neutral = bit-exact ----------------
    std::printf ("-- attack 0 / sustain 0, lookahead off: bit-exact\n");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int band = 0; band < 3; ++band)
        {
            Params p; p.band = (Band) band; p.speed = 0.8f; p.mix = 0.6f; p.splitHz = 300.0f;
            auto x = drums (sr, 1.0);
            auto y = run (p, x, sr, 333);
            bool exact = true;
            for (size_t i = 0; i < x.l.size(); ++i) if (x.l[i] != y.l[i] || x.r[i] != y.r[i]) { exact = false; break; }
            std::snprintf (msg, sizeof msg, "%.0f Hz, band %d: output == input", sr, band);
            CHECK (exact, msg);
        }
    {
        Params p;
        auto x = drums (48000.0, 0.5);
        auto y = run (p, x, 48000.0, 1, 1);
        bool exact = true;
        for (size_t i = 0; i < x.l.size(); ++i) if (x.l[i] != y.l[i]) { exact = false; break; }
        CHECK (exact, "mono, 1-sample blocks: output == input");
    }
    {
        // amounts moved away and back to 0 must return to bit-exact
        Engine e; e.prepare (48000.0, 256);
        Params p; p.attack = 0.8f; p.sustain = -0.6f; e.setParams (p);
        run (e, drums (48000.0, 0.6), 256);
        p.attack = 0.0f; p.sustain = 0.0f; e.setParams (p);
        run (e, drums (48000.0, 0.6), 256);   // ramps settle
        auto x = drums (48000.0, 0.5, 0.5, 0.12, 120.0, 0.7f, 11);
        auto y = run (e, x, 256);
        bool exact = true;
        for (size_t i = 0; i < x.l.size(); ++i) if (x.l[i] != y.l[i] || x.r[i] != y.r[i]) { exact = false; break; }
        CHECK (exact, "back to 0/0 after shaping: bit-exact again");
    }

    // ---------------- lookahead ----------------
    std::printf ("-- lookahead\n");
    for (double sr : { 44100.0, 96000.0 })
    {
        Engine e; e.prepare (sr, 512);
        Params p; p.lookahead = true; e.setParams (p);
        const int L = e.getLatencySamples();
        auto x = drums (sr, 0.6);
        auto y = run (e, x, 512);
        bool exact = true;
        for (size_t i = (size_t) L; i < x.l.size(); ++i) if (y.l[i] != x.l[i - (size_t) L] || y.r[i] != x.r[i - (size_t) L]) { exact = false; break; }
        std::snprintf (msg, sizeof msg, "%.0f Hz: latency %d samples (%.2f ms), neutral output == input delayed by latency", sr, L, 1000.0 * L / sr);
        CHECK (exact && L == (int) std::lround (lookaheadSeconds * sr), msg);
        Params off; Engine e2; e2.prepare (sr, 512); e2.setParams (off);
        CHECK (e2.getLatencySamples() == 0, "lookahead off reports 0 latency");
    }
    {
        // with negative attack, lookahead catches the very first milliseconds of the onset
        const double sr = 48000.0;
        Params p; p.attack = -1.0f;
        auto x = drums (sr, 3.0, 0.5, 0.12, 120.0, 0.7f, 5);
        auto yNo = run (p, x, sr);
        p.lookahead = true;
        auto yLa = run (p, x, sr);
        const int L = (int) std::lround (lookaheadSeconds * sr);
        // compare the first 3 ms of each onset (lookahead output shifted back by its latency)
        double eNo = 0, eLa = 0;
        for (double t0 = 0.5; t0 + 0.01 < 3.0; t0 += 0.5)
        {
            const size_t a = (size_t) (t0 * sr), b = a + (size_t) (0.003 * sr);
            eNo += energy (yNo.l, a, b);
            eLa += energy (yLa.l, a + (size_t) L, b + (size_t) L);
        }
        std::snprintf (msg, sizeof msg, "attack -100 %%: lookahead tames the first 3 ms more (%.1f dB vs %.1f dB)", db (eLa), db (eNo));
        CHECK (eLa < eNo * 0.8, msg);
    }

    // ---------------- attack / sustain behaviour ----------------
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("-- shaping @ %.0f Hz\n", sr);
        auto x = drums (sr, 4.0);
        Params n0;
        auto y0 = run (n0, x, sr);
        const double on0 = windowEnergy (y0.l, sr, 0.5, 0.0, 0.012);
        const double tail0 = windowEnergy (y0.l, sr, 0.5, 0.10, 0.40);

        Params pa; pa.attack = 1.0f;
        auto ya = run (pa, x, sr);
        const double onA = windowEnergy (ya.l, sr, 0.5, 0.0, 0.012);
        std::snprintf (msg, sizeof msg, "attack +100 %% boosts onset energy (%+.1f dB)", db (onA / on0));
        CHECK (onA > on0 * 1.6, msg);
        const double tailA = windowEnergy (ya.l, sr, 0.5, 0.15, 0.40);
        const double tailA0 = windowEnergy (y0.l, sr, 0.5, 0.15, 0.40);
        std::snprintf (msg, sizeof msg, "attack +100 %% leaves the late tail (%+.2f dB)", db (tailA / tailA0));
        CHECK (std::abs (db (tailA / tailA0)) < 1.5, msg);

        Params pam; pam.attack = -1.0f;
        auto yam = run (pam, x, sr);
        const double onAm = windowEnergy (yam.l, sr, 0.5, 0.0, 0.012);
        std::snprintf (msg, sizeof msg, "attack -100 %% softens the onset (%+.1f dB)", db (onAm / on0));
        CHECK (onAm < on0 * 0.6, msg);

        Params ps; ps.sustain = -1.0f;
        auto ys = run (ps, x, sr);
        const double tailS = windowEnergy (ys.l, sr, 0.5, 0.10, 0.40);
        const double onS = windowEnergy (ys.l, sr, 0.5, 0.0, 0.006);
        const double onS0 = windowEnergy (y0.l, sr, 0.5, 0.0, 0.006);
        std::snprintf (msg, sizeof msg, "sustain -100 %% shortens the tail (%+.1f dB), onset kept (%+.2f dB)", db (tailS / tail0), db (onS / onS0));
        CHECK (tailS < tail0 * 0.3 && std::abs (db (onS / onS0)) < 1.5, msg);

        // decay time to -40 dB re hit peak: shorter with sustain -
        auto decayTime = [&] (const std::vector<float>& v)
        {
            const size_t a = (size_t) (1.0 * sr), b = (size_t) (1.5 * sr);
            float pk = 0; for (size_t i = a; i < b; ++i) pk = std::max (pk, std::abs (v[i]));
            size_t last = a;
            for (size_t i = a; i < b; ++i) if (std::abs (v[i]) > pk * 0.01f) last = i;
            return (double) (last - a) / sr;
        };
        std::snprintf (msg, sizeof msg, "sustain -100 %%: -40 dB decay %.0f ms -> %.0f ms", 1000 * decayTime (y0.l), 1000 * decayTime (ys.l));
        CHECK (decayTime (ys.l) < decayTime (y0.l) * 0.7, msg);

        Params psp; psp.sustain = 1.0f;
        auto ysp = run (psp, x, sr);
        const double tailSp = windowEnergy (ysp.l, sr, 0.5, 0.10, 0.40);
        std::snprintf (msg, sizeof msg, "sustain +100 %% lengthens the tail (%+.1f dB)", db (tailSp / tail0));
        CHECK (tailSp > tail0 * 2.0, msg);
    }

    // ---------------- split band ----------------
    std::printf ("-- split band\n");
    {
        const double sr = 48000.0;
        // low kick hits + steady high hat-like tone: shaping only above the split must leave the kick alone
        auto x = drums (sr, 3.0, 0.5, 0.15, 55.0, 0.7f);
        Params pf; pf.sustain = -1.0f;
        Params ph = pf; ph.band = Band::above; ph.splitHz = 2000.0f;
        Params pl = pf; pl.band = Band::below; pl.splitHz = 2000.0f;
        auto y0 = run (Params(), x, sr), yf = run (pf, x, sr), yh = run (ph, x, sr), yl = run (pl, x, sr);
        const double t0 = windowEnergy (y0.l, sr, 0.5, 0.1, 0.4), tf = windowEnergy (yf.l, sr, 0.5, 0.1, 0.4);
        const double th = windowEnergy (yh.l, sr, 0.5, 0.1, 0.4), tl = windowEnergy (yl.l, sr, 0.5, 0.1, 0.4);
        std::snprintf (msg, sizeof msg, "55 Hz hits, split 2 kHz: ABOVE leaves the tail (%+.2f dB), BELOW / FULL shape it (%+.1f / %+.1f dB)",
                       db (th / t0), db (tl / t0), db (tf / t0));
        CHECK (std::abs (db (th / t0)) < 1.0 && tl < t0 * 0.3 && tf < t0 * 0.3, msg);
    }

    // ---------------- mix / output / clip ----------------
    std::printf ("-- mix, output, clipper\n");
    {
        const double sr = 48000.0;
        auto x = drums (sr, 1.0);
        Params p; p.attack = 1.0f; p.sustain = -0.7f; p.mix = 0.0f;
        auto y = run (p, x, sr);
        bool exact = true;
        for (size_t i = 0; i < x.l.size(); ++i) if (x.l[i] != y.l[i] || x.r[i] != y.r[i]) { exact = false; break; }
        CHECK (exact, "mix 0 % = bit-exact dry while shaping");

        p.mix = 0.0f; p.outputDb = -6.0f;
        y = run (p, x, sr);
        CHECK (std::abs (peak (y) / peak (x) - 0.501f) < 0.01f, "output -6 dB scales the signal");

        Params c; c.attack = 1.0f; c.clip = true; c.ceilingDb = -1.0f;
        auto loud = drums (sr, 2.0, 0.25, 0.1, 150.0, 3.0f);
        y = run (c, loud, sr);
        const float ceilLin = std::pow (10.0f, -1.0f / 20.0f);
        std::snprintf (msg, sizeof msg, "clipper holds the ceiling (-1 dBFS): peak %.4f <= %.4f", peak (y), ceilLin);
        CHECK (peak (y) <= ceilLin + 1e-5f && allFinite (y), msg);
        c.clip = false;
        y = run (c, loud, sr);
        CHECK (peak (y) > 1.5f, "clipper off lets the boosted peaks through");
    }

    // ---------------- robustness ----------------
    std::printf ("-- robustness\n");
    {
        std::mt19937 rng (77);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        bool finite = true; float maxPeak = 0.0f; bool prevHuge = false;
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Engine e; e.prepare (sr, 4096);
            for (int blk = 0; blk < 400; ++blk)
            {
                Params p;
                p.attack = u (rng) * 2.0f - 1.0f; p.sustain = u (rng) * 2.0f - 1.0f;
                p.band = (Band) (int) (u (rng) * 2.99f); p.splitHz = 40.0f + u (rng) * 11960.0f;
                p.speed = u (rng); p.lookahead = u (rng) > 0.5f; p.clip = u (rng) > 0.5f;
                p.ceilingDb = -12.0f * u (rng); p.mix = u (rng); p.outputDb = -24.0f + 36.0f * u (rng);
                if (blk % 37 == 0) { p.attack = NAN; p.splitHz = INFINITY; }
                e.setParams (p);
                const int n = 1 + (int) (u (rng) * 4095.0f);
                std::vector<float> l ((size_t) n), r ((size_t) n);
                const int kind = blk % 5;
                for (int i = 0; i < n; ++i)
                {
                    float v = 0.0f;
                    if (kind == 0) v = (u (rng) * 2.0f - 1.0f) * 1000.0f;           // huge
                    else if (kind == 1) v = 0.0f;                                   // silence
                    else if (kind == 2) v = 1.0e-38f * (u (rng) - 0.5f);            // denormals
                    else if (kind == 3) v = (i % 200 == 0) ? 1.0f : 0.0f;           // clicks
                    else v = u (rng) * 2.0f - 1.0f;
                    l[(size_t) i] = v; r[(size_t) i] = -v;
                }
                float* ch[2] = { l.data(), r.data() };
                e.process (ch, (blk % 3 == 0) ? 1 : 2, n);
                const bool wasHuge = prevHuge;
                for (int i = 0; i < n; ++i)
                {
                    if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                    if (kind != 0 && ! wasHuge) maxPeak = std::max (maxPeak, std::abs (l[(size_t) i]));
                }
                prevHuge = kind == 0 || (wasHuge && n < 2000);
            }
        }
        CHECK (finite, "fuzzing (random params, NaN params, huge / denormal / silent input, 1..4096 blocks, mono+stereo): no NaN/inf");
        std::snprintf (msg, sizeof msg, "fuzzing: bounded output for unit-level input (peak %.1f)", maxPeak);
        CHECK (maxPeak < 40.0f, msg);
    }
    {
        // 60 s stress at extreme settings
        const double sr = 48000.0;
        Engine e; e.prepare (sr, 512);
        Params p; p.attack = 1.0f; p.sustain = 1.0f; p.speed = 1.0f; p.lookahead = true; p.outputDb = 12.0f; p.band = Band::below; p.splitHz = 40.0f;
        e.setParams (p);
        auto x = drums (sr, 60.0, 0.23, 0.3, 80.0, 1.0f, 9);
        const auto t0 = std::chrono::steady_clock::now();
        auto y = run (e, x, 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::snprintf (msg, sizeof msg, "60 s stress (+100/+100, fast, +12 dB): finite, peak %.2f, %.0fx realtime", peak (y), 60.0 / secs);
        CHECK (allFinite (y) && peak (y) < 64.0f, msg);
        p.attack = -1.0f; p.sustain = -1.0f; p.speed = 0.0f; p.band = Band::above; p.splitHz = 12000.0f; p.clip = true; p.ceilingDb = -12.0f;
        e.setParams (p);
        y = run (e, x, 64);
        CHECK (allFinite (y) && peak (y) < 4.0f, "60 s stress (-100/-100, slow, clip -12 dB): finite and bounded");
    }

    // ---------------- display history ----------------
    {
        Engine e; e.prepare (48000.0, 512);
        Params p; p.attack = 1.0f; e.setParams (p);
        run (e, drums (48000.0, 1.0), 512);
        float maxIn = 0, maxG = 0;
        for (int i = 0; i < historySize; ++i) { maxIn = std::max (maxIn, e.histIn[(size_t) i].load()); maxG = std::max (maxG, e.histGain[(size_t) i].load()); }
        CHECK (maxIn > 0.3f && maxG > 3.0f, "display history records input envelope and gain");
    }

    // ---------------- factory presets ----------------
    std::printf ("-- factory presets\n");
    {
        const auto& list = factoryPresets();
        bool ok = true; int idx = 0;
        for (const auto& pr : list)
        {
            auto x = drums (48000.0, 2.0, 0.25, 0.1, 100.0, 0.8f);
            double inE = energy (x.l, 0, x.l.size());
            auto y = run (pr.params, x, 48000.0);
            const double d = db (energy (y.l, 0, y.l.size()) / inE);
            if (! allFinite (y) || peak (y) > 4.0f || std::abs (d) > 12.0) { ok = false; std::printf ("    preset %d '%s': %+.1f dB\n", idx, pr.name, d); }
            if (pr.params.clip && peak (y) > std::pow (10.0f, pr.params.ceilingDb / 20.0f) * std::pow (10.0f, pr.params.outputDb / 20.0f) + 1e-4f && pr.params.mix >= 1.0f)
            { ok = false; std::printf ("    preset %d '%s': clipper exceeded\n", idx, pr.name); }
            ++idx;
        }
        std::snprintf (msg, sizeof msg, "%d presets render finite, bounded, within +-12 dB", (int) list.size());
        CHECK (ok && list.size() >= 25, msg);
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
