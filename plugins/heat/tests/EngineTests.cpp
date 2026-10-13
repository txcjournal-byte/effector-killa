// Headless tests for the TrapWin HEAT engine.
#include "../Source/DSP/HeatEngine.h"
#include "../Source/Presets.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <limits>
#include <cstdio>
#include <random>
#include <vector>

using namespace heatfx;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Params clean (int bands = 1, int os = defaultOs)
{
    Params p;
    p.bands = bands; p.oversampling = os;
    for (auto& b : p.band) { b.model = 0; b.drive = 0.0f; b.mix = 1.0f; b.levelDb = 0.0f; b.solo = false; }
    p.autoGain = false; p.punch = 0; p.dynamics = 0; p.mix = 1; p.outputDb = 0; p.lowKeepHz = lowKeepOffHz;
    p.tiltPreDb = p.tiltPostDb = 0; p.lowCutHz = lowCutOffHz; p.highCutHz = highCutOffHz;
    return p;
}

static void run (Engine& e, Stereo& x, int block = 512, int numCh = 2)
{
    for (size_t pos = 0; pos < x.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.l.size() - pos);
        float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
        e.process (ch, numCh, n);
    }
}

static Stereo run (const Params& p, Stereo x, double sr, int block = 512)
{
    Engine e; e.setParams (p); e.prepare (sr, block);
    run (e, x, block);
    return x;
}

static Stereo sine (double f, double sr, size_t n, float a)
{
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = a * (float) std::sin (2.0 * PI * f * (double) i / sr);
    return s;
}

static Stereo pink (size_t n, float rmsTarget, unsigned seed = 7)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (int c = 0; c < 2; ++c)
    {
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        auto& v = c == 0 ? s.l : s.r;
        for (size_t i = 0; i < n; ++i)
        {
            const double w = nd (rng);
            b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
            b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
            v[i] = (float) (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362);
            b6 = w * 0.115926;
        }
        double ss = 0; for (float x : v) ss += (double) x * x;
        const float g = rmsTarget / (float) std::sqrt (ss / (double) n);
        for (auto& x : v) x *= g;
    }
    return s;
}

static double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) (to - from));
}

static double db (double x) { return 20.0 * std::log10 (std::max (1e-20, x)); }

// windowed single-bin amplitude (Hann), returns the amplitude of a sinusoid at f
static double toneAmp (const std::vector<float>& v, size_t from, size_t n, double f, double sr)
{
    std::complex<double> acc (0, 0);
    double wsum = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * PI * (double) i / (double) (n - 1));
        acc += (double) v[from + i] * w * std::polar (1.0, -2.0 * PI * f * (double) i / sr);
        wsum += w;
    }
    return 2.0 * std::abs (acc) / wsum;
}

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const auto wl = std::polar (1.0, -2.0 * PI / (double) len);
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1, 0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], t = a[i + k + len / 2] * w;
                a[i + k] = u + t; a[i + k + len / 2] = u - t;
                w *= wl;
            }
        }
    }
}

// alias power relative to total power for a sine at f0 (harmonic bins excluded)
static double aliasRatioDb (const std::vector<float>& v, size_t from, double f0, double sr)
{
    const size_t N = 16384;
    std::vector<std::complex<double>> a (N);
    for (size_t i = 0; i < N; ++i)
    {
        const double t = 2.0 * PI * (double) i / (double) N;
        const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
        a[i] = v[from + i] * w;
    }
    fft (a);
    double total = 0, alias = 0;
    const double binHz = sr / (double) N;
    for (size_t k = 2; k < N / 2; ++k)
    {
        const double f = (double) k * binHz;
        const double pw = std::norm (a[k]);
        total += pw;
        bool harmonic = false;
        for (int h = 1; h * f0 < sr / 2; ++h)
            if (std::abs (f - h * f0) < 10.0 * binHz) harmonic = true;
        if (! harmonic) alias += pw;
    }
    return 10.0 * std::log10 (std::max (1e-30, alias / total));
}

static bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

int main()
{
    char msg[256];

    // ------------------------------------------------------------------ latency
    std::printf ("-- latency matches the report\n");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int os = 0; os < numOsChoices; ++os)
        {
            Engine e; auto p = clean (1, os); e.setParams (p); e.prepare (sr, 512);
            Stereo x { std::vector<float> (4096, 0.0f), std::vector<float> (4096, 0.0f) };
            x.l[100] = x.r[100] = 1.0f;
            run (e, x);
            size_t at = 0; float best = 0;
            for (size_t i = 0; i < x.l.size(); ++i) if (std::abs (x.l[i]) > best) { best = std::abs (x.l[i]); at = i; }
            std::snprintf (msg, sizeof msg, "%.0f Hz %s: impulse peak at +%d, reported %d (peak %.3f)", sr, osNames[os], (int) at - 100,
                           e.getLatencySamples(), best);
            CHECK ((int) at - 100 == e.getLatencySamples() && e.getLatencySamples() == latencyForChoice (os) && best > 0.9f, msg);
        }
    {
        // dry / wet alignment: mix 50 % must not comb (impulse stays one clean peak)
        auto p = clean (3, 3); p.mix = 0.5f; p.lowKeepHz = 90.0f;
        Stereo x { std::vector<float> (4096, 0.0f), std::vector<float> (4096, 0.0f) };
        x.l[100] = x.r[100] = 1.0f;
        auto y = run (p, x, 48000.0);
        size_t at = 0; float best = 0;
        for (size_t i = 0; i < y.l.size(); ++i) if (std::abs (y.l[i]) > best) { best = std::abs (y.l[i]); at = i; }
        std::snprintf (msg, sizeof msg, "8x, 3 bands, low keep, mix 50 %%: dry and wet aligned (peak %.3f at +%d)", best, (int) at - 100);
        CHECK ((int) at - 100 == latencyForChoice (3) && best > 0.6f, msg);
    }

    // ------------------------------------------------------------------ mix = 0 bit exact
    std::printf ("-- global mix 0 = delayed dry, bit exact\n");
    for (int os = 0; os < numOsChoices; ++os)
    {
        auto p = clean (3, os); p.mix = 0.0f; p.band[0].drive = 1.0f; p.band[0].model = (int) Model::fuzz;
        p.band[1].drive = 0.8f; p.band[2].drive = 0.9f; p.autoGain = true; p.punch = 0.6f; p.lowKeepHz = 80.0f; p.tiltPreDb = 3.0f;
        auto x = pink (48000, 0.2f);
        auto y = run (p, x, 48000.0, 333);
        const int L = latencyForChoice (os);
        bool exact = true;
        for (size_t i = 0; i < x.l.size(); ++i)
        {
            const float ref = (int) i >= L ? x.l[i - (size_t) L] : 0.0f;
            const float refR = (int) i >= L ? x.r[i - (size_t) L] : 0.0f;
            if (y.l[i] != ref || y.r[i] != refR) { exact = false; break; }
        }
        std::snprintf (msg, sizeof msg, "%s: output == input delayed by %d samples", osNames[os], L);
        CHECK (exact, msg);
    }

    // ------------------------------------------------------------------ flat band sum
    std::printf ("-- flat band sum at zero drive (40 Hz - 15 kHz within +-0.1 dB)\n");
    for (int os : { 0, 2, 3 })
        for (int bands = 1; bands <= 3; ++bands)
        {
            const double sr = 48000.0;
            auto p = clean (bands, os);
            const size_t N = 32768;
            Stereo x { std::vector<float> (N, 0.0f), std::vector<float> (N, 0.0f) };
            x.l[0] = x.r[0] = 1.0f;
            auto y = run (p, x, sr);
            std::vector<std::complex<double>> a (N);
            for (size_t i = 0; i < N; ++i) a[i] = y.l[i];
            fft (a);
            double lo = 1e9, hi = -1e9;
            for (size_t k = 1; k < N / 2; ++k)
            {
                const double f = (double) k * sr / (double) N;
                if (f < 40.0 || f > 15000.0) continue;
                const double m = db (std::abs (a[k]));
                lo = std::min (lo, m); hi = std::max (hi, m);
            }
            std::snprintf (msg, sizeof msg, "%s, %d band(s): magnitude %.3f .. %.3f dB", osNames[os], bands, lo, hi);
            CHECK (lo > -0.1 && hi < 0.1, msg);
        }

    // ------------------------------------------------------------------ aliasing
    std::printf ("-- aliasing: 7 kHz sine, heavy drive (alias power / total)\n");
    for (int model : { (int) Model::transistor, (int) Model::fuzz, (int) Model::tube, (int) Model::foldback })
    {
        const double sr = 44100.0;
        double r[numOsChoices];
        for (int os = 0; os < numOsChoices; ++os)
        {
            auto p = clean (1, os); p.band[0].model = model; p.band[0].drive = model == (int) Model::foldback ? 0.4f : 0.75f;
            auto y = run (p, sine (7000.0, sr, 44100, 0.5f), sr);
            r[os] = aliasRatioDb (y.l, 20000, 7000.0, sr);
        }
        std::snprintf (msg, sizeof msg, "%-10s 1x %.1f dB  2x %.1f dB  4x %.1f dB  8x %.1f dB  (4x >= 12 dB, 8x >= 20 dB lower than 1x)",
                       modelNames[model], r[0], r[1], r[2], r[3]);
        CHECK (r[2] < r[0] - 12.0 && r[3] < r[0] - 20.0, msg);
    }

    // ------------------------------------------------------------------ auto-gain
    std::printf ("-- auto-gain on pink noise (+-1 dB)\n");
    {
        struct Case { const char* name; Params p; };
        std::vector<Case> cases;
        auto add = [&] (const char* nm, int model, float drive, int bands = 1)
        {
            auto p = clean (bands); p.autoGain = true;
            for (auto& b : p.band) { b.model = model; b.drive = drive; }
            cases.push_back ({ nm, p });
        };
        add ("TUBE 50 %", (int) Model::tube, 0.5f);
        add ("TAPE 80 %", (int) Model::tape, 0.8f);
        add ("FUZZ 90 %", (int) Model::fuzz, 0.9f);
        add ("FOLDBACK 70 %", (int) Model::foldback, 0.7f);
        add ("WARM CRUSH 80 %", (int) Model::warmCrush, 0.8f);
        add ("RECTIFY 100 %", (int) Model::rectify, 1.0f);
        add ("3 bands TRANSISTOR 60 %", (int) Model::transistor, 0.6f, 3);
        {
            auto p = clean (2); p.autoGain = true; p.band[0].model = (int) Model::transformer; p.band[0].drive = 0.7f;
            p.band[1].model = (int) Model::fuzz; p.band[1].drive = 0.5f; p.band[1].levelDb = 6.0f; p.punch = 0.6f; p.dynamics = 0.5f; p.lowKeepHz = 90.0f;
            cases.push_back ({ "2 bands + punch + dynamics + low keep", p });
        }
        for (auto& c : cases)
        {
            const double sr = 48000.0;
            auto x = pink ((size_t) (sr * 8), 0.125f);
            auto y = run (c.p, x, sr);
            const size_t a = (size_t) (sr * 6), b = (size_t) (sr * 8);
            const double d = db (rms (y.l, a, b)) - db (rms (x.l, a, b));
            std::snprintf (msg, sizeof msg, "%-38s output - input = %+.2f dB", c.name, d);
            CHECK (std::abs (d) <= 1.0, msg);
        }
    }

    // ------------------------------------------------------------------ low keep
    std::printf ("-- LOW KEEP leaves the sub untouched\n");
    {
        const double sr = 48000.0;
        for (int model : { (int) Model::fuzz, (int) Model::foldback, (int) Model::tube })
        {
            auto p = clean (1); p.band[0].model = model; p.band[0].drive = 0.8f; p.autoGain = true; p.lowKeepHz = 120.0f;
            auto x = sine (40.0, sr, (size_t) (sr * 3), 0.5f);
            auto y = run (p, x, sr);
            const size_t from = (size_t) (sr * 2), n = (size_t) sr / 2;
            const double f0 = toneAmp (y.l, from, n, 40.0, sr), in0 = toneAmp (x.l, from, n, 40.0, sr);
            double worstH = -200;
            for (int h = 2; h <= 6; ++h) worstH = std::max (worstH, db (toneAmp (y.l, from, n, 40.0 * h, sr) / f0));
            std::snprintf (msg, sizeof msg, "%-9s keep 120 Hz: 40 Hz %.2f dB vs input, harmonics <= %.1f dB", modelNames[model], db (f0 / in0), worstH);
            CHECK (std::abs (db (f0 / in0)) < 0.5 && worstH < -30.0, msg);

            p.lowKeepHz = lowKeepOffHz;
            auto z = run (p, x, sr);
            double h3 = -200;
            for (int h = 2; h <= 6; ++h) h3 = std::max (h3, db (toneAmp (z.l, from, n, 40.0 * h, sr) / toneAmp (z.l, from, n, 40.0, sr)));
            std::snprintf (msg, sizeof msg, "%-9s keep OFF: the same 40 Hz is distorted (harmonics up to %.1f dB)", modelNames[model], h3);
            CHECK (h3 > -25.0, msg);
        }
        // the part above the keep frequency is still processed
        auto p = clean (1); p.band[0].model = (int) Model::transistor; p.band[0].drive = 0.7f; p.autoGain = true; p.lowKeepHz = 120.0f;
        auto x = sine (40.0, sr, (size_t) (sr * 3), 0.4f);
        auto x2 = sine (1000.0, sr, (size_t) (sr * 3), 0.2f);
        for (size_t i = 0; i < x.l.size(); ++i) { x.l[i] += x2.l[i]; x.r[i] += x2.r[i]; }
        auto y = run (p, x, sr);
        const size_t from = (size_t) (sr * 2), n = (size_t) sr / 2;
        const double f0 = db (toneAmp (y.l, from, n, 40.0, sr) / toneAmp (x.l, from, n, 40.0, sr));
        const double h3 = db (toneAmp (y.l, from, n, 3000.0, sr) / toneAmp (y.l, from, n, 1000.0, sr));
        std::snprintf (msg, sizeof msg, "40 Hz + 1 kHz: sub %.2f dB, 1 kHz still saturated (3rd harmonic %.1f dB)", f0, h3);
        CHECK (std::abs (f0) < 0.5 && h3 > -40.0, msg);
    }

    // ------------------------------------------------------------------ models
    std::printf ("-- models: character\n");
    {
        const double sr = 48000.0;
        for (int m = 0; m < numModels; ++m)
        {
            auto p = clean (1); p.band[0].model = m; p.band[0].drive = 0.6f;
            auto x = sine (200.0, sr, (size_t) sr, 0.5f);
            auto y = run (p, x, sr);
            const size_t from = (size_t) (sr / 2), n = 8192;
            const double f1 = toneAmp (y.l, from, n, 200.0, sr);
            double hsum = 0;
            for (int h = 2; h <= 9; ++h) { const double a = toneAmp (y.l, from, n, 200.0 * h, sr); hsum += a * a; }
            const double thd = std::sqrt (hsum) / std::max (1e-9, f1);
            const double h2 = db (toneAmp (y.l, from, n, 400.0, sr) / f1), h3 = db (toneAmp (y.l, from, n, 600.0, sr) / f1);
            const double lvl = db (rms (y.l, from, from + n) / rms (x.l, from, from + n));
            std::snprintf (msg, sizeof msg, "%-11s THD %5.1f %%  H2 %6.1f dB  H3 %6.1f dB  level %+5.1f dB", modelNames[m], thd * 100.0, h2, h3, lvl);
            CHECK (allFinite (y) && thd > 0.01 && std::abs (lvl) < 15.0, msg);
            if (m == (int) Model::tube) CHECK (h2 > -40.0, "  TUBE makes even (2nd) harmonics");
            if (m == (int) Model::transistor) CHECK (h3 > h2, "  TRANSISTOR is odd dominant");
            if (m == (int) Model::rectify) CHECK (h2 > -20.0, "  RECTIFY adds a strong octave");
        }
        // transformer saturates lows much more than highs
        auto lowHigh = [&] (double f)
        {
            auto p = clean (1); p.band[0].model = (int) Model::transformer; p.band[0].drive = 0.6f;
            auto y = run (p, sine (f, sr, (size_t) sr, 0.5f), sr);
            const double f1 = toneAmp (y.l, (size_t) (sr / 2), 8192, f, sr);
            return db (toneAmp (y.l, (size_t) (sr / 2), 8192, 3 * f, sr) / f1);
        };
        const double l = lowHigh (60.0), h = lowHigh (3000.0);
        std::snprintf (msg, sizeof msg, "TRANSFORMER: H3 at 60 Hz %.1f dB > at 3 kHz %.1f dB", l, h);
        CHECK (l > h + 6.0, msg);
    }

    // ------------------------------------------------------------------ click-free structure changes
    std::printf ("-- click-free switching\n");
    {
        const double sr = 48000.0;
        Engine e; auto p = clean (1, 2); e.setParams (p); e.prepare (sr, 256);
        auto x = sine (100.0, sr, (size_t) sr * 2, 0.5f);
        float maxStep = 0, prev = 0;
        for (size_t pos = 0; pos < x.l.size(); pos += 256)
        {
            const int blk = (int) (pos / 256);
            if (blk % 20 == 10) { p.oversampling = (p.oversampling + 1) % numOsChoices; p.bands = 1 + (blk / 20) % 3; p.band[0].model = (blk / 20) % numModels; e.setParams (p); }
            float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
            e.process (ch, 2, 256);
            for (int i = 0; i < 256; ++i) { if (pos + (size_t) i > 4800) maxStep = std::max (maxStep, std::abs (x.l[pos + (size_t) i] - prev)); prev = x.l[pos + (size_t) i]; }
        }
        std::snprintf (msg, sizeof msg, "oversampling / band count / model switching: max sample step %.4f", maxStep);
        CHECK (maxStep < 0.03f, msg);

        // drive jump 0 -> 100 %: smoothed, no step
        Engine e2; auto q = clean (1, 2); q.band[0].model = (int) Model::tube; e2.setParams (q); e2.prepare (sr, 512);
        auto s = sine (100.0, sr, (size_t) sr, 0.3f);
        maxStep = 0; prev = 0;
        for (size_t pos = 0; pos < s.l.size(); pos += 512)
        {
            if (pos == 24576) { q.band[0].drive = 1.0f; e2.setParams (q); }
            float* ch[2] = { s.l.data() + pos, s.r.data() + pos };
            e2.process (ch, 2, (int) std::min<size_t> (512, s.l.size() - pos));
            for (size_t i = pos; i < std::min (pos + 512, s.l.size()); ++i) { if (i > 4800) maxStep = std::max (maxStep, std::abs (s.l[i] - prev)); prev = s.l[i]; }
        }
        std::snprintf (msg, sizeof msg, "drive jump 0 -> 100 %%: max sample step %.4f", maxStep);
        CHECK (maxStep < 0.05f, msg);
    }

    // ------------------------------------------------------------------ fuzzing + stress
    std::printf ("-- no NaN / inf: fuzzing and stress\n");
    {
        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> u01 (0.0f, 1.0f);
        bool ok = true;
        for (double sr : { 44100.0, 96000.0, 192000.0 })
            for (int numCh : { 1, 2 })
            {
                Engine e; e.prepare (sr, 4096);
                std::vector<float> l (4096), r (4096);
                for (int it = 0; it < 300; ++it)
                {
                    Params p;
                    p.bands = 1 + (int) (u01 (rng) * 3) % 3;
                    p.xoverLoHz = 30.0f + u01 (rng) * 8000.0f; p.xoverHiHz = 300.0f + u01 (rng) * 15700.0f;
                    for (auto& b : p.band) { b.model = (int) (u01 (rng) * numModels) % numModels; b.drive = u01 (rng); b.mix = u01 (rng); b.levelDb = -24.0f + 36.0f * u01 (rng); b.solo = u01 (rng) < 0.15f; }
                    p.oversampling = (int) (u01 (rng) * 4) % 4;
                    p.tiltPreDb = -6.0f + 12.0f * u01 (rng); p.tiltPostDb = -6.0f + 12.0f * u01 (rng);
                    p.lowCutHz = 20.0f + 980.0f * u01 (rng) * u01 (rng); p.highCutHz = 1000.0f + 19000.0f * u01 (rng);
                    p.autoGain = u01 (rng) < 0.6f; p.punch = u01 (rng); p.dynamics = -1.0f + 2.0f * u01 (rng);
                    p.mix = u01 (rng); p.outputDb = -24.0f + 36.0f * u01 (rng); p.lowKeepHz = 20.0f + 280.0f * u01 (rng);
                    e.setParams (p);
                    const int n = 1 + (int) (u01 (rng) * u01 (rng) * 4095.0f);
                    const int kind = it % 5;
                    for (int i = 0; i < n; ++i)
                    {
                        float v = 0.0f;
                        if (kind == 0) v = (u01 (rng) * 2.0f - 1.0f);
                        else if (kind == 1) v = (u01 (rng) * 2.0f - 1.0f) * 50.0f;
                        else if (kind == 2) v = 1.0e-38f * (u01 (rng) - 0.5f);
                        else if (kind == 3) v = (i / 37) % 2 ? 1.0f : -1.0f;
                        l[(size_t) i] = v; r[(size_t) i] = -v * 0.7f;
                    }
                    float* ch[2] = { l.data(), r.data() };
                    e.process (ch, numCh, n);
                    for (int i = 0; i < n; ++i)
                        if (! std::isfinite (l[(size_t) i]) || (numCh == 2 && ! std::isfinite (r[(size_t) i]))) ok = false;
                }
            }
        CHECK (ok, "random parameters, block sizes 1..4096, mono / stereo, 44.1 / 96 / 192 kHz, loud / denormal input");

        // 60 s at the most extreme settings
        Params p;
        p.bands = 3; p.oversampling = 3; p.xoverLoHz = 30.0f; p.xoverHiHz = 300.0f;
        for (auto& b : p.band) { b.model = (int) Model::fuzz; b.drive = 1.0f; b.levelDb = 12.0f; }
        p.band[1].model = (int) Model::foldback; p.band[2].model = (int) Model::warmCrush;
        p.punch = 1.0f; p.dynamics = 1.0f; p.outputDb = 12.0f; p.tiltPreDb = 6.0f; p.tiltPostDb = 6.0f; p.lowKeepHz = 300.0f; p.autoGain = true;
        Engine e; e.setParams (p); e.prepare (44100.0, 512);
        std::vector<float> l (512), r (512);
        std::mt19937 rng2 (9);
        std::normal_distribution<float> nd (0.0f, 0.5f);
        bool fin = true; float peak = 0;
        for (int blk = 0; blk < 44100 * 60 / 512; ++blk)
        {
            for (int i = 0; i < 512; ++i) { l[(size_t) i] = nd (rng2) * ((blk / 200) % 3 == 0 ? 0.0f : 1.0f); r[(size_t) i] = nd (rng2); }
            if (blk % 1000 == 500) { p.dynamics = -p.dynamics; e.setParams (p); }
            float* ch[2] = { l.data(), r.data() };
            e.process (ch, 2, 512);
            for (int i = 0; i < 512; ++i) { fin &= std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]); peak = std::max (peak, std::abs (l[(size_t) i])); }
        }
        std::snprintf (msg, sizeof msg, "60 s stress at extreme settings (8x, max drive, punch, dynamics, +12 dB): finite, peak %.1f", peak);
        CHECK (fin, msg);

        // NaN input does not latch
        Engine e3; e3.setParams (clean (3, 2)); e3.prepare (48000.0, 256);
        std::vector<float> a (256, std::nanf ("")), b (256, std::numeric_limits<float>::infinity());
        float* ch[2] = { a.data(), b.data() };
        e3.process (ch, 2, 256);
        bool rec = true;
        for (int k = 0; k < 40; ++k)
        {
            for (int i = 0; i < 256; ++i) { a[(size_t) i] = 0.3f * (float) std::sin (i * 0.05); b[(size_t) i] = a[(size_t) i]; }
            e3.process (ch, 2, 256);
            if (k > 2) for (int i = 0; i < 256; ++i) rec &= std::isfinite (a[(size_t) i]) && std::isfinite (b[(size_t) i]);
        }
        CHECK (rec, "NaN / inf input: engine recovers, output finite afterwards");
    }

    // ------------------------------------------------------------------ presets
    std::printf ("-- factory presets\n");
    {
        const auto& list = factoryPresets();
        std::snprintf (msg, sizeof msg, "%d factory presets (>= 35)", (int) list.size());
        CHECK (list.size() >= 35, msg);
        const double sr = 48000.0;
        auto x = pink ((size_t) (sr * 6), 0.1f, 3);
        auto s = sine (55.0, sr, x.l.size(), 0.25f);
        for (size_t i = 0; i < x.l.size(); ++i) { x.l[i] += s.l[i]; x.r[i] += s.r[i]; }
        bool fin = true; double worst = 0; const char* worstName = "";
        for (auto& pr : list)
        {
            auto y = run (pr.params, x, sr);
            fin &= allFinite (y);
            const double d = db (rms (y.l, (size_t) (sr * 4), x.l.size())) - db (rms (x.l, (size_t) (sr * 4), x.l.size()));
            if (std::abs (d) > std::abs (worst)) { worst = d; worstName = pr.name; }
        }
        CHECK (fin, "all presets render pink noise + 55 Hz sine without NaN");
        std::snprintf (msg, sizeof msg, "all presets level-matched within +-2 dB (worst %+.2f dB: %s)", worst, worstName);
        CHECK (std::abs (worst) <= 2.0, msg);
    }

    // ------------------------------------------------------------------ CPU
    std::printf ("-- CPU (stereo, 48 kHz, 256 samples)\n");
    for (int os = 0; os < numOsChoices; ++os)
    {
        auto p = clean (3, os);
        p.band[0].model = (int) Model::tube; p.band[1].model = (int) Model::tape; p.band[2].model = (int) Model::fuzz;
        for (auto& b : p.band) b.drive = 0.5f;
        p.punch = 0.3f; p.dynamics = 0.3f; p.lowKeepHz = 80.0f; p.tiltPostDb = 1.0f; p.lowCutHz = 30.0f; p.highCutHz = 16000.0f;
        Engine e; e.setParams (p); e.prepare (48000.0, 256);
        auto x = pink (48000 * 10, 0.1f);
        const auto t0 = std::chrono::steady_clock::now();
        run (e, x, 256);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  info: 3 bands at %s: %.2f %% of one core\n", osNames[os], secs / 10.0 * 100.0);
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
