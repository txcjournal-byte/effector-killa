// Headless tests for the TrapWin HALO reverb engine.
#include "../Source/DSP/HaloEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace halo;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)

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

static double rms (const std::vector<float>& x, size_t from, size_t len)
{
    double e = 0; for (size_t i = from; i < from + len && i < x.size(); ++i) e += (double) x[i] * x[i];
    return std::sqrt (e / (double) len);
}

static bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

static Params wetOnly()
{
    Params p; p.mix = 1.0f; p.predelayMs = 0.0f; p.lowCutHz = 20.0f; p.hiCutHz = 20000.0f; p.mod = 0.0f; p.duck = 0.0f;
    return p;
}

// RT60 from the Schroeder energy-decay curve, fitted between -5 and -35 dB
static double measureRt60 (double sr, float decay, float size)
{
    Engine e; e.prepare (sr, 512);
    Params p = wetOnly(); p.decaySec = decay; p.size = size; p.mod = 0.3f;
    e.setParams (p); e.reset();
    const size_t n = (size_t) (sr * (decay * 1.6 + 0.5));
    Stereo x; x.l.assign (n, 0.0f); x.r.assign (n, 0.0f); x.l[0] = 1.0f; x.r[0] = 1.0f;
    auto y = run (e, x, 512);
    std::vector<double> edc (n);
    double acc = 0;
    for (size_t i = n; i-- > 0;) { acc += (double) y.l[i] * y.l[i] + (double) y.r[i] * y.r[i]; edc[i] = acc; }
    const double tot = edc[0];
    double t5 = -1, t35 = -1;
    for (size_t i = 0; i < n; ++i)
    {
        const double db = 10 * std::log10 (edc[i] / tot + 1e-30);
        if (t5 < 0 && db <= -5) t5 = (double) i / sr;
        if (t35 < 0 && db <= -35) { t35 = (double) i / sr; break; }
    }
    if (t5 < 0 || t35 < 0) return -1;
    return (t35 - t5) * 2.0;
}

int main()
{
    std::printf ("TrapWin HALO engine tests\n");

    // ---- 1. RT60 tracks the decay setting at several sample rates
    {
        std::printf ("[RT60]\n");
        for (double sr : { 44100.0, 96000.0 })
            for (float d : { 1.0f, 3.0f, 8.0f })
            {
                const double t = measureRt60 (sr, d, 0.6f);
                char msg[160];
                std::snprintf (msg, sizeof msg, "sr %.0f decay %.1f s -> measured %.2f s", sr, d, t);
                CHECK (t > d * 0.7 && t < d * 1.35, msg);
            }
        const double tSmall = measureRt60 (48000.0, 2.0f, 0.05f), tBig = measureRt60 (48000.0, 2.0f, 1.0f);
        char msg[160];
        std::snprintf (msg, sizeof msg, "RT60 independent of size (small %.2f s, huge %.2f s)", tSmall, tBig);
        CHECK (tSmall > 1.4 && tSmall < 2.7 && tBig > 1.4 && tBig < 2.7, msg);
    }

    // ---- 2. mix = 0 is a bit-exact dry passthrough (stereo + mono)
    {
        std::printf ("[passthrough]\n");
        for (int nc : { 2, 1 })
        {
            Engine e; e.prepare (48000.0, 512);
            Params p; p.mix = 0.0f; p.decaySec = 10.0f; p.duck = 0.5f; e.setParams (p); e.reset();
            auto x = noise (48000.0, 1.0, 0.3f, 7);
            auto y = run (e, x, 333, nc);
            double maxErr = 0;
            for (size_t i = 0; i < x.l.size(); ++i)
            {
                maxErr = std::max (maxErr, (double) std::abs (y.l[i] - x.l[i]));
                if (nc == 2) maxErr = std::max (maxErr, (double) std::abs (y.r[i] - x.r[i]));
            }
            CHECK (maxErr == 0.0, nc == 2 ? "stereo mix 0 bit-exact" : "mono mix 0 bit-exact");
        }
    }

    // ---- 3. stability at max settings for 60 s of noise (192 kHz block 64 too)
    {
        std::printf ("[stability]\n");
        for (double sr : { 44100.0, 192000.0 })
        {
            Engine e; e.prepare (sr, 1024);
            Params p; p.size = 1.0f; p.decaySec = 30.0f; p.diffusion = 1.0f; p.mod = 1.0f; p.hiCutHz = 20000.0f; p.lowCutHz = 20.0f;
            p.width = 1.5f; p.mix = 1.0f; p.outputDb = 12.0f; p.predelayMs = 500.0f;
            e.setParams (p); e.reset();
            const double secs = sr > 100000 ? 15.0 : 60.0;
            const auto t0 = std::chrono::steady_clock::now();
            bool finite = true; double peak = 0, lastRms = 0;
            const size_t chunk = (size_t) sr;
            for (int s = 0; s < (int) secs; ++s)
            {
                auto x = noise (sr, 1.0, 0.25f, (unsigned) s + 3);
                if (s % 7 == 3) { p.freeze = ! p.freeze; e.setParams (p); }   // toggle freeze too
                auto y = run (e, x, sr > 100000 ? 64 : 1024);
                finite = finite && allFinite (y);
                for (float v : y.l) peak = std::max (peak, (double) std::abs (v));
                lastRms = rms (y.l, 0, chunk);
            }
            const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / secs;
            char msg[200];
            std::snprintf (msg, sizeof msg, "sr %.0f: %.0f s noise at max settings finite, peak %.2f, last rms %.3f (cpu %.1f%% of realtime)",
                           sr, secs, peak, lastRms, cpu * 100);
            CHECK (finite && peak < 60.0 && lastRms > 0.01, msg);
        }
        // random parameter jumps every block must stay finite
        Engine e; e.prepare (48000.0, 512);
        std::mt19937 rng (5); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        bool finite = true;
        for (int b = 0; b < 2000; ++b)
        {
            Params p; p.size = u (rng); p.decaySec = 0.2f + 29.8f * u (rng); p.diffusion = u (rng); p.mod = u (rng);
            p.hiCutHz = 1000 + 19000 * u (rng); p.lowCutHz = 20 + 980 * u (rng); p.width = 1.5f * u (rng); p.duck = u (rng);
            p.freeze = u (rng) > 0.8f; p.mix = u (rng); p.predelayMs = 500 * u (rng); p.outputDb = -24 + 36 * u (rng);
            e.setParams (p);
            auto x = noise (48000.0, 512.0 / 48000.0, 0.5f, (unsigned) b);
            finite = finite && allFinite (run (e, x, 512));
        }
        CHECK (finite, "random parameter automation stays finite");
    }

    // ---- 4. freeze holds the energy
    {
        std::printf ("[freeze]\n");
        Engine e; e.prepare (48000.0, 512);
        Params p = wetOnly(); p.decaySec = 1.0f; p.mod = 0.4f; p.hiCutHz = 12000.0f;
        e.setParams (p); e.reset();
        run (e, noise (48000.0, 1.0, 0.3f, 11));
        p.freeze = true; e.setParams (p);
        Stereo silence; silence.l.assign (48000 * 10, 0.0f); silence.r = silence.l;
        auto y = run (e, silence);
        const double r1 = rms (y.l, 48000, 24000), r9 = rms (y.l, 48000 * 9, 24000);
        char msg[160];
        std::snprintf (msg, sizeof msg, "frozen tail after 9 s still %.1f dB vs 1 s (decay set to 1 s)", 20 * std::log10 (r9 / r1));
        CHECK (r1 > 1e-3 && r9 > r1 * 0.5 && r9 < r1 * 1.5, msg);
        // input is ignored while frozen
        auto loud = noise (48000.0, 1.0, 0.5f, 12);
        auto y2 = run (e, loud);
        const double r10 = rms (y2.l, 24000, 24000);
        std::snprintf (msg, sizeof msg, "new input does not pile up while frozen (%.1f dB)", 20 * std::log10 (r10 / r9));
        CHECK (r10 < r9 * 1.4, msg);
        // unfreeze -> decays again
        p.freeze = false; e.setParams (p);
        auto y3 = run (e, silence);
        CHECK (rms (y3.l, 48000 * 5, 24000) < r9 * 0.01, "unfreeze releases the tail");
    }

    // ---- 5. ducking reduces the wet level while input is present
    {
        std::printf ("[duck]\n");
        auto measure = [] (float duck, double& during, double& after)
        {
            Engine e; e.prepare (48000.0, 512);
            Params p = wetOnly(); p.decaySec = 3.0f; p.duck = duck; p.mix = 1.0f;
            e.setParams (p); e.reset();
            Stereo x = noise (48000.0, 2.0, 0.25f, 21);
            x.l.resize (48000 * 4, 0.0f); x.r.resize (48000 * 4, 0.0f);   // 2 s signal, 2 s silence
            auto y = run (e, x);
            during = rms (y.l, 48000, 24000);
            after = rms (y.l, 48000 * 2 + 24000, 12000);
        };
        double d0, a0, d1, a1;
        measure (0.0f, d0, a0);
        measure (1.0f, d1, a1);
        char msg[160];
        std::snprintf (msg, sizeof msg, "duck 100%%: wet %.1f dB while singing", 20 * std::log10 (d1 / d0));
        CHECK (d1 < d0 * 0.25, msg);
        std::snprintf (msg, sizeof msg, "duck 100%%: tail blooms back after the input stops (%.1f dB vs no duck)", 20 * std::log10 (a1 / a0));
        CHECK (a1 > a0 * 0.5, msg);
    }

    // ---- 6. level sanity: 50 % mix is not much louder / quieter than the dry signal
    {
        std::printf ("[level]\n");
        for (float d : { 0.8f, 3.5f, 12.0f })
            for (float sz : { 0.2f, 0.6f, 1.0f })
            {
                Engine e; e.prepare (48000.0, 512);
                Params p; p.mix = 0.5f; p.decaySec = d; p.size = sz; p.predelayMs = 10.0f; p.lowCutHz = 100.0f;
                e.setParams (p); e.reset();
                auto x = noise (48000.0, 4.0, 0.2f, 31);
                auto y = run (e, x);
                const double in = rms (x.l, 48000 * 2, 48000 * 2), out = rms (y.l, 48000 * 2, 48000 * 2);
                const double db = 20 * std::log10 (out / in);
                char msg[160];
                std::snprintf (msg, sizeof msg, "decay %.1f size %.1f: 50%% mix level %+.1f dB", d, sz, db);
                CHECK (db > -4.5 && db < 4.5, msg);
            }
    }

    // ---- 7. stereo: wet output decorrelated, width 0 is mono, mono input works
    {
        std::printf ("[stereo]\n");
        auto corr = [] (float width)
        {
            Engine e; e.prepare (48000.0, 512);
            Params p = wetOnly(); p.width = width; e.setParams (p); e.reset();
            Stereo x = noise (48000.0, 2.0, 0.2f, 41); x.r = x.l;   // mono source
            auto y = run (e, x);
            double lr = 0, ll = 0, rr = 0;
            for (size_t i = 48000; i < y.l.size(); ++i) { lr += y.l[i] * y.r[i]; ll += y.l[i] * y.l[i]; rr += y.r[i] * y.r[i]; }
            return lr / std::sqrt (ll * rr + 1e-30);
        };
        const double c1 = corr (1.0f), c0 = corr (0.0f);
        char msg[160];
        std::snprintf (msg, sizeof msg, "mono source -> wide wet (L/R correlation %.2f)", c1);
        CHECK (std::abs (c1) < 0.5, msg);
        CHECK (c0 > 0.999, "width 0 collapses to mono");
        Engine e; e.prepare (44100.0, 512);
        Params p; p.mix = 0.5f; e.setParams (p); e.reset();
        auto x = noise (44100.0, 1.0, 0.2f, 43);
        auto y = run (e, x, 256, 1);
        CHECK (allFinite (y) && rms (y.l, 22050, 22050) > 0.05, "mono (1 channel) processing works");
    }

    // ---- 8. silence in -> silence out (denormal safety, no DC creep)
    {
        std::printf ("[silence]\n");
        Engine e; e.prepare (48000.0, 512);
        Params p; p.mix = 1.0f; p.decaySec = 30.0f; e.setParams (p); e.reset();
        Stereo s; s.l.assign (48000 * 5, 0.0f); s.r = s.l;
        auto y = run (e, s);
        double pk = 0; for (float v : y.l) pk = std::max (pk, (double) std::abs (v));
        CHECK (pk < 1e-12, "silence stays silent");
        CHECK (e.getLatencySamples() == 0, "reports zero latency");
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
