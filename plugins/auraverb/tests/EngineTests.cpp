// Headless tests for the TrapWin AURA VERB engine (every test runs for every mode).
#include "../Source/DSP/AuraEngine.h"
#include "../Source/Presets.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace auraverb;
static int failures = 0, passes = 0;
static bool verbose = false;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else { ++passes; std::printf ("  ok:   %s\n", msg); } } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Stereo run (Engine& e, Stereo x, int block = 256, int numCh = 2)
{
    for (size_t pos = 0; pos < x.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.l.size() - pos);
        float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
        e.process (ch, numCh, n);
    }
    return x;
}

static Stereo noise (double sr, double seconds, float amp, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> nd (0.0f, amp);
    Stereo s; s.l.resize ((size_t) (sr * seconds)); s.r.resize (s.l.size());
    for (size_t i = 0; i < s.l.size(); ++i) { s.l[i] = nd (rng); s.r[i] = nd (rng); }
    return s;
}

static Stereo silence (double sr, double seconds)
{
    Stereo s; s.l.assign ((size_t) (sr * seconds), 0.0f); s.r = s.l;
    return s;
}

static double rms (const std::vector<float>& x, size_t from, size_t len)
{
    double e = 0; size_t c = 0;
    for (size_t i = from; i < from + len && i < x.size(); ++i, ++c) e += (double) x[i] * x[i];
    return std::sqrt (e / (double) std::max<size_t> (1, c));
}

static bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

static double db (double x) { return 20.0 * std::log10 (x + 1e-30); }

static Params wetOnly (Mode m)
{
    Params p; p.mode = m; p.mix = 1.0f; p.predelayMs = 0.0f; p.lowCutHz = 20.0f; p.dampingHz = 20000.0f;
    p.earlyLate = 1.0f; p.attack = 0.0f; p.modDepth = 0.3f; p.bassMult = 1.0f; p.duck = 0.0f; p.vintage = false;
    return p;
}

// RT60 from the Schroeder energy-decay curve, fitted between -5 and -35 dB
static double measureRt60 (Mode m, double sr, float decay, float size, bool vintage = false)
{
    Engine e; e.prepare (sr, 512);
    Params p = wetOnly (m); p.decaySec = decay; p.size = size; p.vintage = vintage;
    e.setParams (p); e.reset();
    const size_t n = (size_t) (sr * (decay * 1.6 + 0.8));
    Stereo x = silence (sr, (double) n / sr);
    x.l[0] = x.r[0] = 1.0f;
    auto y = run (e, x, 512);
    const size_t len = std::min (n, y.l.size());   // silence() may round the length down by one sample
    std::vector<double> edc (len);
    double acc = 0;
    for (size_t i = len; i-- > 0;) { acc += (double) y.l[i] * y.l[i] + (double) y.r[i] * y.r[i]; edc[i] = acc; }
    double t5 = -1, t35 = -1;
    for (size_t i = 0; i < len; ++i)
    {
        const double d = 10 * std::log10 (edc[i] / edc[0] + 1e-30);
        if (t5 < 0 && d <= -5) t5 = (double) i / sr;
        if (t35 < 0 && d <= -35) { t35 = (double) i / sr; break; }
    }
    if (t5 < 0 || t35 < 0) return -1;
    return (t35 - t5) * 2.0;
}

static const char* mname (int m) { return modeNames[m]; }

int main (int argc, char**)
{
    verbose = argc > 1;
    std::printf ("TrapWin AURA VERB engine tests\n");
    char msg[256];

    // ---- 1. RT60 tracks the decay setting, every mode, several rates, sizes and both colours
    {
        std::printf ("[RT60 follows DECAY]\n");
        for (int m = 0; m < numModes; ++m)
        {
            for (float d : { 0.6f, 2.0f, 6.0f, 15.0f })
            {
                const double t = measureRt60 ((Mode) m, 48000.0, d, 0.5f);
                std::snprintf (msg, sizeof msg, "%-11s decay %4.1f s -> measured %5.2f s", mname (m), d, t);
                CHECK (t > d * 0.78 && t < d * 1.25, msg);
            }
            for (double sr : { 44100.0, 96000.0 })
            {
                const double t = measureRt60 ((Mode) m, sr, 3.0f, 0.8f, sr > 50000.0);
                std::snprintf (msg, sizeof msg, "%-11s sr %.0f decay 3.0 s (size 0.8%s) -> %5.2f s", mname (m), sr, sr > 50000.0 ? ", vintage" : "", t);
                CHECK (t > 3.0 * 0.78 && t < 3.0 * 1.25, msg);
            }
            const double ts = measureRt60 ((Mode) m, 48000.0, 2.0f, 0.0f), tb = measureRt60 ((Mode) m, 48000.0, 2.0f, 1.0f);
            std::snprintf (msg, sizeof msg, "%-11s RT60 independent of size (tiny %.2f s, huge %.2f s)", mname (m), ts, tb);
            CHECK (ts > 1.5 && ts < 2.5 && tb > 1.5 && tb < 2.5, msg);
        }
        const double tMin = measureRt60 (Mode::room, 48000.0, 0.1f, 0.0f);
        std::snprintf (msg, sizeof msg, "shortest decay 0.1 s -> %.3f s", tMin);
        CHECK (tMin > 0.0 && tMin < 0.2, msg);
    }

    // ---- 2. mix = 0 is a bit-exact dry passthrough (stereo + mono), even while switching modes
    {
        std::printf ("[mix 0 bit-exact]\n");
        for (int m = 0; m < numModes; ++m)
            for (int nc : { 2, 1 })
            {
                Engine e; e.prepare (48000.0, 512);
                Params p; p.mode = (Mode) m; p.mix = 0.0f; p.decaySec = 10.0f; p.duck = 0.5f; p.vintage = (m & 1) != 0; p.width = 2.0f;
                e.setParams (p); e.reset();
                auto x = noise (48000.0, 0.5, 0.3f, 7);
                Stereo y = x;
                for (size_t pos = 0; pos < y.l.size(); pos += 333)
                {
                    if (pos > 12000) { p.mode = (Mode) ((m + 3) % numModes); e.setParams (p); }
                    const int n = (int) std::min<size_t> (333, y.l.size() - pos);
                    float* ch[2] = { y.l.data() + pos, y.r.data() + pos };
                    e.process (ch, nc, n);
                }
                double maxErr = 0;
                for (size_t i = 0; i < x.l.size(); ++i)
                {
                    maxErr = std::max (maxErr, (double) std::abs (y.l[i] - x.l[i]));
                    if (nc == 2) maxErr = std::max (maxErr, (double) std::abs (y.r[i] - x.r[i]));
                }
                std::snprintf (msg, sizeof msg, "%-11s %s mix 0 bit-exact", mname (m), nc == 2 ? "stereo" : "mono  ");
                CHECK (maxErr == 0.0, msg);
            }
    }

    // ---- 3. stability: 60 s of noise at extreme settings per mode (+ 192 kHz block 64), random automation
    {
        std::printf ("[stability]\n");
        double worstCpu = 0.0;
        for (int m = 0; m < numModes; ++m)
            for (double sr : { 44100.0, 192000.0 })
            {
                Engine e; e.prepare (sr, 1024);
                Params p; p.mode = (Mode) m; p.size = 1.0f; p.decaySec = 60.0f; p.diffusion = 1.0f; p.modDepth = 1.0f; p.modRateHz = 5.0f;
                p.dampingHz = 20000.0f; p.lowCutHz = 20.0f; p.bassMult = 2.5f; p.attack = 1.0f; p.earlyLate = 0.5f;
                p.width = 2.0f; p.mix = 1.0f; p.outputDb = 12.0f; p.predelayMs = 500.0f; p.vintage = true;
                e.setParams (p); e.reset();
                const int secs = sr > 100000 ? 6 : 60;
                const auto t0 = std::chrono::steady_clock::now();
                bool finite = true; double peak = 0, lastRms = 0;
                for (int s = 0; s < secs; ++s)
                {
                    auto x = noise (sr, 1.0, 0.25f, (unsigned) s + 3);
                    if (s % 7 == 3) { p.freeze = ! p.freeze; e.setParams (p); }
                    if (s % 11 == 5) { p.vintage = ! p.vintage; e.setParams (p); }
                    auto y = run (e, x, sr > 100000 ? 64 : 1024);
                    finite = finite && allFinite (y);
                    for (float v : y.l) peak = std::max (peak, (double) std::abs (v));
                    lastRms = rms (y.l, 0, y.l.size());
                }
                const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / secs;
                if (sr < 50000.0) worstCpu = std::max (worstCpu, cpu);
                std::snprintf (msg, sizeof msg, "%-11s sr %.0f: %d s noise at extreme settings finite, peak %.1f, rms %.3f (cpu %.1f%% of realtime)",
                               mname (m), sr, secs, peak, lastRms, cpu * 100);
                CHECK (finite && peak < 100.0 && lastRms > 0.01, msg);
            }
        std::snprintf (msg, sizeof msg, "worst-case CPU at 44.1 kHz %.1f%% of one core (< 15%%)", worstCpu * 100);
        CHECK (worstCpu < 0.15, msg);

        Engine e; e.prepare (48000.0, 512);
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        bool finite = true;
        for (int b = 0; b < 3000; ++b)
        {
            Params p; p.mode = (Mode) (int) (u (rng) * 6.999f); p.size = u (rng); p.decaySec = 0.1f + 59.9f * u (rng) * u (rng);
            p.diffusion = u (rng); p.modDepth = u (rng); p.modRateHz = 0.05f + 4.95f * u (rng); p.dampingHz = 1000 + 19000 * u (rng);
            p.lowCutHz = 20 + 980 * u (rng); p.bassMult = 0.25f + 2.25f * u (rng); p.width = 2.0f * u (rng); p.duck = u (rng);
            p.attack = u (rng); p.earlyLate = u (rng); p.freeze = u (rng) > 0.85f; p.vintage = u (rng) > 0.5f; p.mix = u (rng);
            p.predelaySync = u (rng) > 0.5f; p.predelayDiv = (int) (u (rng) * 10.99f); p.bpm = 40 + 160 * u (rng);
            p.predelayMs = 500 * u (rng); p.outputDb = -24 + 36 * u (rng);
            e.setParams (p);
            auto x = noise (48000.0, 512.0 / 48000.0, 0.5f, (unsigned) b);
            finite = finite && allFinite (run (e, x, 512));
        }
        CHECK (finite, "random parameter + mode automation (3000 blocks) stays finite");

        // hot input
        Engine h; h.prepare (48000.0, 512);
        Params p; p.mode = Mode::shimmer; p.decaySec = 60.0f; p.size = 1.0f; p.mix = 1.0f; p.modDepth = 1.0f; h.setParams (p); h.reset();
        auto y = run (h, noise (48000.0, 10.0, 4.0f, 77));
        double pk = 0; for (float v : y.l) pk = std::max (pk, (double) std::abs (v));
        std::snprintf (msg, sizeof msg, "+12 dB hot noise into a 60 s shimmer stays bounded (peak %.1f)", pk);
        CHECK (allFinite (y) && pk < 200.0, msg);
    }

    // ---- 4. freeze holds the energy, ignores new input and releases again
    {
        std::printf ("[freeze]\n");
        for (int m = 0; m < numModes; ++m)
        {
            Engine e; e.prepare (48000.0, 512);
            Params p = wetOnly ((Mode) m); p.decaySec = 1.0f; p.modDepth = 0.4f; p.dampingHz = 6000.0f; p.earlyLate = 0.65f;
            e.setParams (p); e.reset();
            run (e, noise (48000.0, 1.0, 0.3f, 11));
            p.freeze = true; e.setParams (p);
            auto y = run (e, silence (48000.0, 10.0));
            const double r1 = rms (y.l, 48000, 24000), r9 = rms (y.l, 48000 * 9, 24000);
            std::snprintf (msg, sizeof msg, "%-11s frozen tail after 9 s at %+.1f dB vs 1 s (decay set to 1 s)", mname (m), db (r9 / r1));
            CHECK (r1 > 1e-3 && r9 > r1 * 0.5 && r9 < r1 * 1.5, msg);
            auto y2 = run (e, noise (48000.0, 1.0, 0.5f, 12));
            const double r10 = rms (y2.l, 24000, 24000);
            std::snprintf (msg, sizeof msg, "%-11s new input does not pile up while frozen (%+.1f dB)", mname (m), db (r10 / r9));
            CHECK (r10 < r9 * 1.6, msg);
            p.freeze = false; e.setParams (p);
            auto y3 = run (e, silence (48000.0, 6.0));
            std::snprintf (msg, sizeof msg, "%-11s unfreeze releases the tail (%.1f dB after 5 s)", mname (m), db (rms (y3.l, 48000 * 5, 24000) / r9));
            CHECK (rms (y3.l, 48000 * 5, 24000) < r9 * 0.01, msg);
        }
    }

    // ---- 5. ducking reduces the wet level while input is present, then blooms back
    {
        std::printf ("[duck]\n");
        for (int m = 0; m < numModes; ++m)
        {
            auto measure = [m] (float duck, double& during, double& after)
            {
                Engine e; e.prepare (48000.0, 512);
                Params p = wetOnly ((Mode) m); p.decaySec = 3.0f; p.duck = duck; p.earlyLate = 0.65f;
                e.setParams (p); e.reset();
                Stereo x = noise (48000.0, 2.0, 0.25f, 21);
                x.l.resize (48000 * 4, 0.0f); x.r.resize (48000 * 4, 0.0f);
                auto y = run (e, x);
                during = rms (y.l, 48000, 24000);
                after = rms (y.l, 48000 * 2 + 24000, 12000);
            };
            double d0, a0, d1, a1;
            measure (0.0f, d0, a0);
            measure (1.0f, d1, a1);
            std::snprintf (msg, sizeof msg, "%-11s duck 100%%: wet %+.1f dB while playing, tail %+.1f dB after", mname (m), db (d1 / d0), db (a1 / a0));
            CHECK (d1 < d0 * 0.25 && a1 > a0 * 0.5, msg);
        }
    }

    // ---- 6. click-free mode switching (every mode -> every other mode, sine and noise programme)
    {
        std::printf ("[click-free mode switch]\n");
        const double sr = 48000.0;
        auto secondDiffMax = [] (const std::vector<float>& y, size_t a, size_t b)
        {
            double mx = 0;
            for (size_t i = std::max<size_t> (a, 2); i < b && i < y.size(); ++i)
                mx = std::max (mx, (double) std::abs (y[i] - 2.0f * y[i - 1] + y[i - 2]));
            return mx;
        };
        double worst = 0; int bad = 0;
        for (int from = 0; from < numModes; ++from)
            for (int to = 0; to < numModes; ++to)
            {
                if (from == to) continue;
                for (float mix : { 1.0f, 0.4f })
                {
                    Engine e; e.prepare (sr, 512);
                    Params p; p.mode = (Mode) from; p.mix = mix; p.decaySec = 2.0f; p.size = 0.6f; e.setParams (p); e.reset();
                    const size_t n = (size_t) (sr * 3.5), sw = (size_t) (sr * 1.5);
                    Stereo x; x.l.resize (n); x.r.resize (n);
                    for (size_t i = 0; i < n; ++i) x.l[i] = x.r[i] = 0.3f * (float) std::sin (2.0 * PI * 220.0 * (double) i / sr);
                    for (size_t pos = 0; pos < n; pos += 256)
                    {
                        if (pos == (sw / 256) * 256) { p.mode = (Mode) to; e.setParams (p); }
                        float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
                        e.process (ch, 2, (int) std::min<size_t> (256, n - pos));
                    }
                    const size_t s0 = (sw / 256) * 256;
                    const double before = secondDiffMax (x.l, s0 - (size_t) (0.5 * sr), s0);
                    const double after = secondDiffMax (x.l, s0 + (size_t) (1.2 * sr), s0 + (size_t) (1.9 * sr));
                    const double during = secondDiffMax (x.l, s0, s0 + (size_t) (0.4 * sr));
                    const double ratio = during / std::max (before, after);
                    worst = std::max (worst, ratio);
                    if (! (ratio < 2.0) || ! allFinite (x))
                    {
                        ++bad;
                        std::printf ("    click: %s -> %s mix %.1f ratio %.2f (before %.2g after %.2g during %.2g)\n", mname (from), mname (to), mix, ratio, before, after, during);
                    }
                }
            }
        std::snprintf (msg, sizeof msg, "42 mode pairs x 2 mixes: no clicks (worst 2nd-difference ratio %.2f, < 2.0)", worst);
        CHECK (bad == 0, msg);

        // the old tail fades instead of stopping dead, the new mode is fully in after the crossfade
        Engine e; e.prepare (sr, 512);
        Params p = wetOnly (Mode::hall); p.decaySec = 4.0f; e.setParams (p); e.reset();
        run (e, noise (sr, 1.0, 0.3f, 5));
        p.mode = Mode::room; e.setParams (p);
        Stereo s = silence (sr, 0.4);
        float* ch[2] = { s.l.data(), s.r.data() };
        e.process (ch, 2, 64);
        const bool fadingNow = e.isCrossfading();
        e.process (ch, 2, (int) s.l.size() - 64);   // (offsets do not matter here)
        CHECK (fadingNow && ! e.isCrossfading() && e.getActiveMode() == Mode::room, "crossfade engages on mode change and completes in 150 ms");
    }

    // ---- 7. level sanity: 50 % mix stays close to the dry level for every mode
    {
        std::printf ("[level]\n");
        for (int m = 0; m < numModes; ++m)
        {
            double lo = 1e9, hi = -1e9;
            for (float d : { 0.8f, 3.0f, 12.0f })
                for (float sz : { 0.2f, 0.9f })
                {
                    Engine e; e.prepare (48000.0, 512);
                    Params p; p.mode = (Mode) m; p.mix = 0.5f; p.decaySec = d; p.size = sz; p.predelayMs = 10.0f; p.lowCutHz = 100.0f;
                    e.setParams (p); e.reset();
                    auto x = noise (48000.0, 4.0, 0.2f, 31);
                    auto y = run (e, x);
                    const double v = db (rms (y.l, 48000 * 2, 48000 * 2) / rms (x.l, 48000 * 2, 48000 * 2));
                    lo = std::min (lo, v); hi = std::max (hi, v);
                }
            std::snprintf (msg, sizeof msg, "%-11s 50%% mix level %+.1f .. %+.1f dB re dry", mname (m), lo, hi);
            CHECK (lo > -4.5 && hi < 4.5, msg);
        }
    }

    // ---- 8. parameters do what they say
    {
        std::printf ("[parameters]\n");
        const double sr = 48000.0;
        // pre-delay: free + tempo synced onset
        for (bool sync : { false, true })
        {
            Engine e; e.prepare (sr, 512);
            Params p = wetOnly (Mode::plate); p.earlyLate = 0.5f; p.predelaySync = sync; p.predelayMs = 120.0f; p.predelayDiv = 7; p.bpm = 100.0;
            e.setParams (p); e.reset();
            Stereo x = silence (sr, 1.0); x.l[0] = x.r[0] = 1.0f;
            auto y = run (e, x);
            size_t first = 0;
            for (size_t i = 0; i < y.l.size(); ++i) if (std::abs (y.l[i]) > 1e-4f || std::abs (y.r[i]) > 1e-4f) { first = i; break; }
            const double expectMs = sync ? 300.0 : 120.0;
            std::snprintf (msg, sizeof msg, "%s pre-delay: wet starts at %.1f ms (set %.0f ms)", sync ? "synced 1/8 @ 100 BPM" : "free", 1000.0 * first / sr, expectMs);
            CHECK (first >= (size_t) (expectMs * sr / 1000.0) && first < (size_t) ((expectMs + 6.0) * sr / 1000.0), msg);
        }
        CHECK (std::abs (predelayMsFor ([] { Params q; q.predelaySync = true; q.predelayDiv = 10; q.bpm = 20.0; return q; }()) - 2000.0) < 1e-6,
               "synced pre-delay clamps at 2 s");

        // early / late balance
        {
            auto tail = [&] (float el)
            {
                Engine e; e.prepare (sr, 512);
                Params p = wetOnly (Mode::hall); p.earlyLate = el; p.decaySec = 4.0f; e.setParams (p); e.reset();
                Stereo x = silence (sr, 1.5); x.l[0] = x.r[0] = 1.0f;
                auto y = run (e, x);
                return std::pair<double, double> (rms (y.l, 0, (size_t) (0.25 * sr)), rms (y.l, (size_t) (0.6 * sr), (size_t) (0.6 * sr)));
            };
            const auto earlyOnly = tail (0.0f), both = tail (0.5f);
            std::snprintf (msg, sizeof msg, "EARLY/LATE 0 = reflections only (late tail %.1f dB below the full mix)", db (earlyOnly.second / both.second));
            CHECK (earlyOnly.second < both.second * 0.01 && earlyOnly.first > 0.01 * both.first, msg);
        }
        // attack / build: energy arrives later
        {
            auto centroid = [&] (float att)
            {
                Engine e; e.prepare (sr, 512);
                Params p = wetOnly (Mode::hall); p.attack = att; p.decaySec = 1.5f; p.size = 0.7f; e.setParams (p); e.reset();
                Stereo x = silence (sr, 3.0); x.l[0] = x.r[0] = 1.0f;
                auto y = run (e, x);
                double t = 0, w = 0;
                for (size_t i = 0; i < y.l.size(); ++i) { const double en = (double) y.l[i] * y.l[i]; t += en * (double) i; w += en; }
                return t / w / sr;
            };
            const double c0 = centroid (0.0f), c1 = centroid (1.0f);
            std::snprintf (msg, sizeof msg, "ATTACK swells the tail (energy centroid %.0f ms -> %.0f ms)", c0 * 1000, c1 * 1000);
            CHECK (c1 > c0 + 0.08, msg);
        }
        // damping darkens, bass multiplier lengthens the lows
        {
            auto bands = [&] (float dampHz, float bass, double& lowE, double& highE)
            {
                Engine e; e.prepare (sr, 512);
                Params p = wetOnly (Mode::hall); p.dampingHz = dampHz; p.bassMult = bass; p.decaySec = 3.0f; e.setParams (p); e.reset();
                Stereo x = silence (sr, 3.0); x.l[0] = x.r[0] = 1.0f;
                auto y = run (e, x);
                // crude split: 1st-order low-pass at ~200 Hz vs first difference (high band) on the late tail
                double lp = 0; lowE = highE = 0;
                for (size_t i = 1; i < y.l.size(); ++i)
                {
                    lp += 0.026 * ((double) y.l[i] - lp);
                    if (i > (size_t) (1.5 * sr)) { lowE += lp * lp; const double d = (double) y.l[i] - y.l[i - 1]; highE += d * d; }
                }
            };
            double l0, h0, l1, h1, l2, h2;
            bands (20000.0f, 1.0f, l0, h0);
            bands (2000.0f, 1.0f, l1, h1);
            bands (20000.0f, 2.0f, l2, h2);
            std::snprintf (msg, sizeof msg, "DAMPING 2 kHz removes highs from the tail (%.1f dB)", 10 * std::log10 (h1 / h0));
            CHECK (h1 < h0 * 0.1, msg);
            std::snprintf (msg, sizeof msg, "BASS x2 keeps the lows ringing longer (%+.1f dB at 1.5 s)", 10 * std::log10 (l2 / l0));
            CHECK (l2 > l0 * 2.0, msg);
        }
        // stereo
        {
            auto corr = [&] (Mode m, float width)
            {
                Engine e; e.prepare (sr, 512);
                Params p = wetOnly (m); p.width = width; e.setParams (p); e.reset();
                Stereo x = noise (sr, 2.0, 0.2f, 41); x.r = x.l;
                auto y = run (e, x);
                double lr = 0, ll = 0, rr = 0;
                for (size_t i = 48000; i < y.l.size(); ++i) { lr += y.l[i] * y.r[i]; ll += y.l[i] * y.l[i]; rr += y.r[i] * y.r[i]; }
                return lr / std::sqrt (ll * rr + 1e-30);
            };
            bool wide = true, mono = true;
            double worst = 0;
            for (int m = 0; m < numModes; ++m)
            {
                const double c = corr ((Mode) m, 1.0f);
                worst = std::max (worst, std::abs (c));
                wide = wide && std::abs (c) < 0.5;
                mono = mono && corr ((Mode) m, 0.0f) > 0.999;
            }
            std::snprintf (msg, sizeof msg, "mono source -> wide wet in every mode (worst |L/R correlation| %.2f)", worst);
            CHECK (wide, msg);
            CHECK (mono, "WIDTH 0 collapses every mode to mono");
        }
        // low-cut
        {
            auto lowE = [&] (float hz)
            {
                Engine e; e.prepare (sr, 512);
                Params p = wetOnly (Mode::hall); p.lowCutHz = hz; e.setParams (p); e.reset();
                std::vector<float> s ((size_t) sr * 2);
                for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * (float) std::sin (2.0 * PI * 60.0 * (double) i / sr);
                Stereo x { s, s };
                auto y = run (e, x);
                return rms (y.l, (size_t) sr, (size_t) sr);
            };
            const double a = lowE (20.0f), b = lowE (600.0f);
            std::snprintf (msg, sizeof msg, "LOW CUT 600 Hz removes 60 Hz from the wet (%.1f dB)", db (b / a));
            CHECK (b < a * 0.05, msg);
        }
    }

    // ---- 9. mono processing, silence, latency
    {
        std::printf ("[misc]\n");
        bool ok = true;
        for (int m = 0; m < numModes; ++m)
        {
            Engine e; e.prepare (44100.0, 512);
            Params p; p.mode = (Mode) m; p.mix = 0.5f; e.setParams (p); e.reset();
            auto y = run (e, noise (44100.0, 1.0, 0.2f, 43), 256, 1);
            ok = ok && allFinite (y) && rms (y.l, 22050, 22050) > 0.05;
        }
        CHECK (ok, "mono (1 channel) processing works in every mode");
        bool silent = true;
        for (int m = 0; m < numModes; ++m)
        {
            Engine e; e.prepare (48000.0, 512);
            Params p; p.mode = (Mode) m; p.mix = 1.0f; p.decaySec = 30.0f; p.vintage = true; e.setParams (p); e.reset();
            auto y = run (e, silence (48000.0, 3.0));
            double pk = 0; for (float v : y.l) pk = std::max (pk, (double) std::abs (v));
            silent = silent && pk < 1e-12;
        }
        CHECK (silent, "silence stays silent in every mode");
        bool sizes = true;
        for (int bs : { 1, 7, 64, 4096 })
        {
            Engine e; e.prepare (96000.0, bs);
            Params p; p.mode = Mode::shimmer; e.setParams (p); e.reset();
            sizes = sizes && allFinite (run (e, noise (96000.0, 0.3, 0.2f, 9), bs));
        }
        CHECK (sizes, "block sizes 1 / 7 / 64 / 4096 at 96 kHz");
        Engine e; e.prepare (48000.0, 512);
        CHECK (e.getLatencySamples() == 0, "reports zero latency");
    }

    // ---- 10. factory presets: all load, all modes covered, sane output
    {
        std::printf ("[presets]\n");
        const auto& list = factoryPresets();
        bool modesSeen[numModes] {};
        bool ok = true;
        double lo = 1e9, hi = -1e9;
        for (const auto& pr : list)
        {
            modesSeen[(int) pr.params.mode] = true;
            Engine e; e.prepare (48000.0, 512);
            Params p = pr.params; p.bpm = 140.0;
            e.setParams (p); e.reset();
            auto x = noise (48000.0, 3.0, 0.2f, 61);
            auto y = run (e, x, 512);
            const double v = db (rms (y.l, 48000, 96000) / rms (x.l, 48000, 96000));
            lo = std::min (lo, v); hi = std::max (hi, v);
            if (! allFinite (y) || v < -8.0 || v > 6.0)
            {
                ok = false;
                std::printf ("    preset %s: %+.1f dB\n", pr.name, v);
            }
        }
        bool all = true; for (bool b2 : modesSeen) all = all && b2;
        std::snprintf (msg, sizeof msg, "%d factory presets render cleanly (level %+.1f .. %+.1f dB re dry)", (int) list.size(), lo, hi);
        CHECK (ok && list.size() >= 40, msg);
        CHECK (all, "presets cover all 7 modes");
    }

    std::printf ("%d checks passed\n", passes);
    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
