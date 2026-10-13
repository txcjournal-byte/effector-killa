// Headless tests for the TrapWin CHOPPER engine.
#include "../Source/DSP/ChopperEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

using namespace chopper;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Pattern pat (const char* s)
{
    Pattern p {};
    patternFromString (s, p);
    return p;
}

static Params base()
{
    Params p;
    p.rate = 1; p.size = 3; p.swing = 0.0f; p.mix = 1.0f; p.outputDb = 0.0f; p.hold = false;
    return p;
}

// runs the engine with a playing host transport starting at ppq 0 (block-wise ppq like a DAW)
static Stereo run (Engine& e, Stereo x, double sr, double bpm, bool playing, int block = 512,
                   const std::vector<std::pair<size_t, bool>>* holdEvents = nullptr, Params* prm = nullptr)
{
    std::mt19937 rng (7);
    for (size_t pos = 0; pos < x.l.size();)
    {
        int n = block > 0 ? block : (int) (rng() % 700 + 1);
        n = (int) std::min<size_t> ((size_t) n, x.l.size() - pos);
        if (holdEvents && prm)
            for (auto& ev : *holdEvents)
                if (ev.first >= pos && ev.first < pos + (size_t) n)
                {
                    // split the block so the hold toggles exactly at the event
                    if (ev.first > pos) { n = (int) (ev.first - pos); break; }
                    prm->hold = ev.second;
                    e.setParams (*prm);
                }
        HostTime t;
        t.playing = playing; t.hasPpq = playing; t.bpm = bpm;
        t.ppq = (double) pos * bpm / 60.0 / sr;
        float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
        e.process (ch, 2, n, t);
        pos += (size_t) n;
    }
    return x;
}

static Stereo dc (size_t n, float v) { return { std::vector<float> (n, v), std::vector<float> (n, v) }; }

static Stereo noise (size_t n, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-0.8f, 0.8f);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i) { s.l[i] = d (rng); s.r[i] = d (rng); }
    return s;
}

static Stereo sine (size_t n, double f, double sr, float a = 0.5f)
{
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i)
    {
        s.l[i] = a * (float) std::sin (2.0 * PI * f * (double) i / sr);
        s.r[i] = a * (float) std::sin (2.0 * PI * f * (double) i / sr + 0.7);
    }
    return s;
}

static float maxJump (const std::vector<float>& v)
{
    float m = 0.0f;
    for (size_t i = 1; i < v.size(); ++i) m = std::max (m, std::abs (v[i] - v[i - 1]));
    return m;
}

static bool finite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]) || std::abs (s.l[i]) > 4.0f || std::abs (s.r[i]) > 4.0f) return false;
    return true;
}

int main()
{
    const double sr = 44100.0;

    std::printf ("[step boundaries: 140 BPM, 1/16 -> 4725 samples per step]\n");
    {
        Engine e; e.prepare (sr, 512);
        e.setParams (base());
        e.setPattern (pat (".G.G.G.G.G.G.G.G"));
        const int F = e.getFadeSamples();
        auto y = run (e, dc (4725 * 20, 1.0f), sr, 140.0, true, 512);
        bool exact = true, starts = true, silent = true;
        for (int k = 1; k < 20; ++k)
        {
            const size_t b = (size_t) k * 4725;
            const bool gated = (k % 2) == 1;
            // the last sample of the previous step is untouched, the boundary sample already moves
            if (gated)
            {
                exact &= y.l[b - 1] == 1.0f;
                starts &= y.l[b] < 1.0f;
                for (size_t i = b + (size_t) F + 1; i < b + 4725; ++i) silent &= y.l[i] == 0.0f && y.r[i] == 0.0f;
            }
            else
            {
                exact &= y.l[b - 1] == 0.0f;
                starts &= y.l[b] > 0.0f;
                for (size_t i = b + (size_t) F + 1; i < b + 4725; ++i) exact &= y.l[i] == 1.0f && y.r[i] == 1.0f;
            }
        }
        CHECK (exact, "previous step untouched up to the sample before each boundary (incl. pattern wrap)");
        CHECK (starts, "every step change starts exactly on its boundary sample");
        CHECK (silent, "Gate steps are fully silent after the crossfade");
        std::printf ("        fade = %d samples (%.2f ms)\n", F, F * 1000.0 / sr);
    }
    {
        // odd block sizes + transport running from a non-zero position
        Engine e; e.prepare (sr, 512);
        e.setParams (base());
        e.setPattern (pat (".G.G.G.G.G.G.G.G"));
        auto y = run (e, dc (4725 * 8, 1.0f), sr, 140.0, true, 0);
        bool ok = true;
        for (int k = 1; k < 8; ++k)
            ok &= y.l[(size_t) k * 4725 - 1] == ((k % 2) ? 1.0f : 0.0f) && (k % 2 ? y.l[(size_t) k * 4725] < 1.0f : y.l[(size_t) k * 4725] > 0.0f);
        CHECK (ok, "boundaries are sample-exact with random block sizes");
    }
    {
        // swing 50 % -> even steps are 1.25x long: step 1 starts at 4725 * 1.25 = 5906.25 -> sample 5907
        Engine e; e.prepare (sr, 512);
        auto p = base(); p.swing = 0.5f;
        e.setParams (p);
        e.setPattern (pat (".G.G.G.G.G.G.G.G"));
        auto y = run (e, dc (4725 * 4, 1.0f), sr, 140.0, true, 512);
        CHECK (y.l[5906] == 1.0f && y.l[5907] < 1.0f, "swing delays odd steps (step 2 starts at sample 5907)");
        CHECK (y.l[9449] == 0.0f && y.l[9450] > 0.0f, "swing keeps the pair length (step 3 at sample 9450)");
    }
    {
        // transport stopped: free-run at 120 BPM -> 1/16 = 5512.5 samples
        Engine e; e.prepare (sr, 512);
        e.setParams (base());
        e.setPattern (pat (".G.G.G.G.G.G.G.G"));
        auto y = run (e, dc (5513 * 3, 1.0f), sr, 0.0, false, 512);
        CHECK (y.l[5512] == 1.0f && y.l[5513] < 1.0f, "free-run fallback at 120 BPM when the host is stopped");
    }
    {
        // other rates: 1/8, 1/32, 1/16T at 140 BPM -> 9450, 2362.5, 3150 samples
        const int rates[] = { 0, 2, 3 };
        const double lens[] = { 9450.0, 2362.5, 3150.0 };
        bool ok = true;
        for (int k = 0; k < 3; ++k)
        {
            Engine e; e.prepare (sr, 512);
            auto p = base(); p.rate = rates[k];
            e.setParams (p);
            e.setPattern (pat (".G.G.G.G.G.G.G.G"));
            auto y = run (e, dc ((size_t) (lens[k] * 3), 1.0f), sr, 140.0, true, 512);
            const auto b = (size_t) std::ceil (lens[k] - 1e-9);
            ok &= y.l[b - 1] == 1.0f && y.l[b] < 1.0f;
        }
        CHECK (ok, "rates 1/8, 1/32 and 1/16T land on the right sample");
    }

    std::printf ("[repeat]\n");
    {
        Engine e; e.prepare (sr, 512);
        auto p = base(); p.size = 3;  // 1/32
        e.setParams (p);
        e.setPattern (pat ("RRRR............"));  // one run of 4 steps = 1 beat
        const int R = e.sliceSamples (3, 140.0);
        const int F = std::min (e.getFadeSamples(), R / 4);
        auto x = noise (4725 * 6, 3);
        auto y = run (e, x, sr, 140.0, true, 512);
        bool exact = true;
        int checked = 0;
        for (int k = 1; (k + 1) * R <= 4 * 4725; ++k)
            for (int j = F; j < R; ++j)
            {
                const size_t i = (size_t) (k * R + j);
                exact &= y.l[i] == x.l[(size_t) j] && y.r[i] == x.r[(size_t) j];
                ++checked;
            }
        CHECK (R == 2363, "1/32 slice at 140 BPM = 2363 samples");
        CHECK (exact && checked > 10000, "Repeat replays the captured slice sample-exactly on every retrigger");
        bool firstPass = true;
        for (int j = e.getFadeSamples(); j < R; ++j) firstPass &= y.l[(size_t) j] == x.l[(size_t) j];
        CHECK (firstPass, "first pass of the slice is the live input");
        bool after = true;
        for (size_t i = 4 * 4725 + (size_t) e.getFadeSamples() + 1; i < 6 * 4725; ++i) after &= y.l[i] == x.l[i];
        CHECK (after, "Off steps after the roll are bit-exact dry again");
    }
    {
        // HOLD forces Repeat from the current step start, on an all-Off pattern
        Engine e; e.prepare (sr, 512);
        auto p = base(); p.size = 2;  // 1/16 = 4725
        e.setParams (p);
        e.setPattern (pat ("................"));
        std::vector<std::pair<size_t, bool>> ev { { 4725 * 2 + 1000, true }, { 4725 * 7, false } };
        auto x = noise (4725 * 9, 5);
        auto y = run (e, x, sr, 140.0, true, 512, &ev, &p);
        const int R = e.sliceSamples (2, 140.0);
        const int F = e.getFadeSamples();
        bool ok = true;
        // the slice starts at the step start (sample 9450); after the press it loops
        for (int k = 1; k < 4; ++k)
            for (int j = F; j < R; ++j)
                ok &= y.l[(size_t) (9450 + k * R + j)] == x.l[(size_t) (9450 + j)];
        CHECK (R == 4725 && ok, "HOLD forces a step-aligned repeat while held");
        bool rel = true;
        for (size_t i = 4725 * 7 + (size_t) F + 1; i < 4725 * 9; ++i) rel &= y.l[i] == x.l[i];
        CHECK (rel, "releasing HOLD returns to the pattern (dry, bit-exact)");
    }

    std::printf ("[reverse / gate / off]\n");
    {
        Engine e; e.prepare (sr, 512);
        e.setParams (base());
        e.setPattern (pat ("....V..........."));
        auto x = noise (4725 * 8, 9);
        auto y = run (e, x, sr, 140.0, true, 512);
        const size_t s0 = 4 * 4725;
        bool ok = true;
        for (size_t t = (size_t) e.getFadeSamples(); t < 4725 - (size_t) e.getFadeSamples(); ++t)
            ok &= y.l[s0 + t] == x.l[s0 - 1 - t];
        CHECK (ok, "Reverse plays the audio before the step backwards (mirrored at the boundary)");
    }
    {
        Engine e; e.prepare (sr, 512);
        e.setParams (base());
        e.setPattern (pat ("GGGGGGGGGGGGGGGG"));
        auto y = run (e, noise (44100, 2), sr, 140.0, true, 333);
        bool silent = true;
        for (size_t i = (size_t) e.getFadeSamples() + 1; i < y.l.size(); ++i) silent &= y.l[i] == 0.0f && y.r[i] == 0.0f;
        CHECK (silent, "an all-Gate pattern is digital silence");
    }
    {
        Engine e; e.prepare (sr, 512);
        e.setParams (base());
        e.setPattern (pat ("................"));
        auto x = noise (44100 * 3, 4);
        auto y = run (e, x, sr, 140.0, true, 0);
        CHECK (std::memcmp (x.l.data(), y.l.data(), x.l.size() * 4) == 0 && std::memcmp (x.r.data(), y.r.data(), x.r.size() * 4) == 0,
               "Off steps are bit-exact dry (random block sizes)");
        Engine e2; e2.prepare (sr, 512);
        auto p = base(); p.mix = 0.37f;
        e2.setParams (p);
        e2.setPattern (pat ("................"));
        auto y2 = run (e2, x, sr, 140.0, false, 512);
        CHECK (std::memcmp (x.l.data(), y2.l.data(), x.l.size() * 4) == 0, "Off is bit-exact at any mix (transport stopped too)");
    }
    {
        // mix 0 = dry even with effects active (after smoothing)
        Engine e; e.prepare (sr, 512);
        auto p = base(); p.mix = 0.0f;
        e.setParams (p);
        e.setPattern (pat ("RVTGPHRVTGPHRVTG"));
        auto x = noise (44100, 6);
        auto y = run (e, x, sr, 140.0, true, 512);
        float err = 0.0f;
        for (size_t i = 0; i < x.l.size(); ++i) err = std::max (err, std::abs (y.l[i] - x.l[i]));
        CHECK (err < 1.0e-6f, "mix 0 % passes the dry signal");
    }
    {
        // output gain
        Engine e; e.prepare (sr, 512);
        auto p = base(); p.outputDb = -6.0206f;
        e.setParams (p);
        e.setPattern (pat ("................"));
        e.reset();
        auto y = run (e, dc (4096, 1.0f), sr, 140.0, true, 512);
        CHECK (std::abs (y.l[4000] - 0.5f) < 1e-4f, "output -6 dB halves the level");
    }

    std::printf ("[effects sanity]\n");
    {
        // half-speed: a 400 Hz sine comes out near 200 Hz (zero-crossing count)
        auto zc = [] (const std::vector<float>& v, size_t a, size_t b)
        {
            int z = 0;
            for (size_t i = a + 1; i < b; ++i) if ((v[i - 1] < 0.0f) != (v[i] < 0.0f)) ++z;
            return z;
        };
        for (auto fx : { "H...............", "P..............." })
        {
            Engine e; e.prepare (sr, 512);
            auto p = base(); p.rate = 0;  // 1/8 -> 9450 samples
            e.setParams (p);
            e.setPattern (pat (fx));
            auto x = sine (9450 * 2, 400.0, sr);
            auto y = run (e, x, sr, 140.0, true, 512);
            const int zin = zc (x.l, 1000, 9000), zout = zc (y.l, 1000, 9000);
            char msg[128];
            std::snprintf (msg, sizeof msg, "%s halves the pitch (zero crossings %d -> %d)", fx[0] == 'H' ? "Half-Speed" : "Pitch Down", zin, zout);
            CHECK (std::abs (zout * 2 - zin) <= 6, msg);
        }
        // tape stop slows down then goes quiet before the step ends
        Engine e; e.prepare (sr, 512);
        auto p = base(); p.rate = 0;
        e.setParams (p);
        e.setPattern (pat ("TT.............."));
        auto x = sine (9450 * 3, 300.0, sr);
        auto y = run (e, x, sr, 140.0, true, 512);
        float tail = 0.0f, head = 0.0f;
        for (size_t i = 18900 - 800; i < 18900; ++i) tail = std::max (tail, std::abs (y.l[i]));
        for (size_t i = 200; i < 3000; ++i) head = std::max (head, std::abs (y.l[i]));
        const int zA = zc (y.l, 0, 4000), zB = zc (y.l, 9000, 13000);
        CHECK (head > 0.4f && tail < 0.05f && zB < zA, "Tape Stop decelerates over the run and lands in silence");
    }

    std::printf ("[no clicks]\n");
    for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Engine e; e.prepare (rate, 512);
        auto p = base(); p.size = 4; p.swing = 0.3f;
        e.setParams (p);
        e.setPattern (pat ("RVTGPH.RRHHVVPGT"));
        const size_t n = (size_t) (rate * 6.0);
        std::vector<std::pair<size_t, bool>> ev { { (size_t) (rate * 2.13), true }, { (size_t) (rate * 2.91), false },
                                                  { (size_t) (rate * 4.05), true }, { (size_t) (rate * 4.4), false } };
        auto x = sine (n, 220.0, rate);
        auto y = run (e, x, rate, 140.0, true, 0, &ev, &p);
        const float natural = maxJump (x.l);
        const float j = std::max (maxJump (y.l), maxJump (y.r));
        // a hard cut on this sine would jump up to ~1.0; fades keep it within ~3x the sine's own slope
        const float bound = natural * 1.5f + 1.6f / (float) e.getFadeSamples() * 1.2f;
        char msg[160];
        std::snprintf (msg, sizeof msg, "%.0f Hz: max sample jump %.4f (sine slope %.4f, bound %.4f)", rate, j, natural, bound);
        CHECK (j <= bound, msg);
    }

    std::printf ("[robustness]\n");
    {
        std::mt19937 rng (11);
        bool ok = true;
        for (int trial = 0; trial < 24; ++trial)
        {
            const double rate = std::vector<double> { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 }[(size_t) (rng() % 6)];
            Engine e; e.prepare (rate, 1024);
            Pattern pt {};
            for (auto& s : pt) s = (Fx) (rng() % numFx);
            e.setPattern (pt);
            const bool mono = trial % 3 == 0;
            std::vector<float> l ((size_t) (rate * 1.5)), r (l.size());
            std::uniform_real_distribution<float> d (-1.0f, 1.0f);
            for (size_t i = 0; i < l.size(); ++i) { l[i] = d (rng) * (trial % 5 == 0 ? 4.0f : 1.0f); r[i] = d (rng); }
            double ppq = (double) (rng() % 1000) * 0.37;
            for (size_t pos = 0; pos < l.size();)
            {
                const int nb = (int) std::min<size_t> (l.size() - pos, rng() % 1024 + 1);
                Params p;
                p.rate = (int) (rng() % numRates); p.size = (int) (rng() % numSizes);
                p.swing = (float) (rng() % 101) / 100.0f; p.mix = (float) (rng() % 101) / 100.0f;
                p.outputDb = -24.0f + (float) (rng() % 37); p.hold = (rng() % 7) == 0;
                e.setParams (p);
                HostTime t;
                t.playing = (rng() % 9) != 0; t.hasPpq = t.playing;
                t.bpm = (rng() % 13 == 0) ? 0.0 : 30.0 + (double) (rng() % 271);
                if (rng() % 17 == 0) ppq = (double) (rng() % 500) * 0.25;   // loop / locate
                t.ppq = ppq;
                ppq += nb * (t.bpm > 1 ? t.bpm : 120.0) / 60.0 / rate;
                float* ch[2] = { l.data() + pos, r.data() + pos };
                e.process (ch, mono ? 1 : 2, nb, t);
                pos += (size_t) nb;
            }
            for (size_t i = 0; i < l.size(); ++i)
                ok &= std::isfinite (l[i]) && std::isfinite (r[i]) && std::abs (l[i]) < 64.0f;
        }
        CHECK (ok, "no NaN / inf with random params, patterns, tempo, transport jumps, mono + stereo, 44.1-192 kHz");
        Engine e; e.prepare (sr, 512);
        e.setPattern (pat ("RVTGPHRVTGPHRVTG"));
        auto y = run (e, dc (44100, 0.0f), sr, 140.0, true, 512);
        bool zero = true;
        for (float v : y.l) zero &= v == 0.0f;
        CHECK (zero && finite (y), "silence in -> silence out");
    }

    std::printf ("[pattern state]\n");
    {
        Pattern a = pat ("RVTGPH.RRHHVVPGT");
        const auto s = patternToString (a);
        Pattern b {};
        CHECK (patternFromString (s, b) && a == b && s == "RVTGPH.RRHHVVPGT", "pattern -> text -> pattern round trip");
        Pattern c = a;
        CHECK (! patternFromString ("RRR", c) && c == a, "malformed pattern text is rejected and leaves the pattern unchanged");
        Pattern d {};
        patternFromString ("RxR?............", d);
        CHECK (d[1] == Fx::off && d[2] == Fx::repeat, "unknown characters decode as Off");
    }

    std::printf ("[performance]\n");
    {
        Engine e; e.prepare (48000.0, 512);
        e.setPattern (pat ("PPPPHHHHTTTTRRRR"));
        auto x = noise (48000 * 10, 8);
        const auto t0 = std::chrono::steady_clock::now();
        run (e, x, 48000.0, 140.0, true, 512);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("        10 s stereo @48k in %.3f s (%.0fx realtime)\n", secs, 10.0 / secs);
        CHECK (secs < 2.0, "faster than 5x realtime");
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
