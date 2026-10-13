// Headless tests for the TrapWin GHOST DELAY engine.
#include "../Source/DSP/GhostEngine.h"
#include "../Source/Presets.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace ghostdelay;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Params clean (Mode m = Mode::digital)
{
    Params p;
    p.mode = m; p.sync = true; p.bpm = 140.0; p.division = defaultDivision;
    p.lrRatio = 0.0f; p.feedback = 0.0f; p.lowCutHz = 20.0f; p.highCutHz = 20000.0f;
    p.modDepth = 0.0f; p.age = 0.0f; p.duck = 0.0f; p.mix = 1.0f; p.outputDb = 0.0f;
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

static Stereo silence (size_t n) { return { std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f) }; }

static Stereo impulse (size_t n, float a = 1.0f)
{
    auto s = silence (n);
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
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) std::max<size_t> (1, to - from));
}

static bool finite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

// frequency of the strongest spectral peak in v[from, to) (Hann window, 0.5 Hz grid scan with parabolic refinement)
static double spectralPeak (const std::vector<float>& v, size_t from, size_t to, double sr, double fLo, double fHi)
{
    const size_t n = to - from;
    std::vector<double> w (n);
    for (size_t i = 0; i < n; ++i) w[i] = v[from + i] * (0.5 - 0.5 * std::cos (2.0 * PI * (double) i / (double) (n - 1)));
    auto mag = [&] (double f)
    {
        double re = 0, im = 0;
        const double dw = 2.0 * PI * f / sr;
        for (size_t i = 0; i < n; ++i) { re += w[i] * std::cos (dw * (double) i); im -= w[i] * std::sin (dw * (double) i); }
        return std::sqrt (re * re + im * im);
    };
    double best = fLo, bm = -1;
    for (double f = fLo; f <= fHi; f += 2.0) { const double m = mag (f); if (m > bm) { bm = m; best = f; } }
    double lo = best - 2.0, hi = best + 2.0;
    for (int it = 0; it < 30; ++it) // golden-ish refinement
    {
        const double a = lo + (hi - lo) / 3.0, b = hi - (hi - lo) / 3.0;
        if (mag (a) > mag (b)) hi = b; else lo = a;
    }
    return 0.5 * (lo + hi);
}

int main()
{
    // ------------------------------------------------------------------------------------------------
    std::printf ("-- exact sync timing @ 140 BPM (DIGITAL, every division)\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        int bad = 0;
        for (int div = 0; div < numDivisions; ++div)
        {
            auto p = clean(); p.division = div;
            const double expect = std::round (divisionBeats[(size_t) div] * 60.0 / 140.0 * sr);
            auto y = run (p, impulse ((size_t) (expect + sr * 0.2), 0.5f), sr);
            const size_t at = argmaxAbs (y.l, 1, y.l.size());
            const size_t atR = argmaxAbs (y.r, 1, y.r.size());
            if ((double) at != expect || (double) atR != expect || std::abs (y.l[at] - 0.5f) > 1e-3f)
            {
                ++bad;
                std::printf ("     %s: echo at %zu / %zu, expected %.0f (%.4f)\n", divisionNames[(size_t) div], at, atR, expect, y.l[at]);
            }
        }
        char msg[160];
        std::snprintf (msg, sizeof msg, "all %d divisions (1/64T .. 2 BARS) land on the exact sample @ %.0f Hz", numDivisions, sr);
        CHECK (bad == 0, msg);
    }
    {
        // the 1/4 at 140 BPM is 428.571 ms; 1/8D = 321.43 ms; 1/16T = 71.43 ms
        auto p = clean(); p.division = defaultDivision;
        CHECK (std::abs (delayMsFor (p) - 60000.0 / 140.0) < 1e-9, "1/4 @ 140 BPM = 428.571 ms");
        p.division = 12; CHECK (std::abs (delayMsFor (p) - 0.75 * 60000.0 / 140.0) < 1e-9, "1/8D @ 140 BPM = 321.43 ms");
        p.division = 5;  CHECK (std::abs (delayMsFor (p) - 60000.0 / 140.0 / 6.0) < 1e-9, "1/16T @ 140 BPM = 71.43 ms");
        p.division = numDivisions - 1; CHECK (std::abs (delayMsFor (p) - 8.0 * 60000.0 / 140.0) < 1e-9, "2 BARS @ 140 BPM = 3428.6 ms");
        p.sync = false; p.timeMs = 250.0f; CHECK (std::abs (delayMsFor (p) - 250.0) < 1e-9, "free time 250 ms");
    }
    {
        // repeats keep the grid: 2nd and 3rd echo at exactly 2T and 3T
        const double sr = 48000.0;
        auto p = clean(); p.division = 10; p.feedback = 0.5f; // 1/8
        const double T = std::round (0.5 * 60.0 / 140.0 * sr);
        auto y = run (p, impulse ((size_t) (T * 3.5)), sr);
        const size_t e2 = argmaxAbs (y.l, (size_t) (T * 1.5), (size_t) (T * 2.5));
        const size_t e3 = argmaxAbs (y.l, (size_t) (T * 2.5), (size_t) (T * 3.4));
        char msg[160];
        std::snprintf (msg, sizeof msg, "repeats at 2T / 3T: %zu / %zu (expected %.0f / %.0f), level %.3f / %.3f", e2, e3, 2 * T, 3 * T, y.l[e2], y.l[e3]);
        CHECK ((double) e2 == 2 * T && (double) e3 == 3 * T && std::abs (y.l[e2] - 0.5f) < 0.01f && std::abs (y.l[e3] - 0.25f) < 0.01f, msg);
    }
    {
        // L/R ratio: +1 = right side at half the time
        const double sr = 48000.0;
        auto p = clean(); p.lrRatio = 1.0f;
        const double T = std::round (60.0 / 140.0 * sr);
        auto y = run (p, impulse ((size_t) (T * 1.3)), sr);
        const size_t aL = argmaxAbs (y.l, 1, y.l.size()), aR = argmaxAbs (y.r, 1, y.r.size());
        char msg[160];
        std::snprintf (msg, sizeof msg, "L/R ratio +100 %%: L at %zu (T=%.0f), R at %zu (T/2=%.0f)", aL, T, aR, std::round (T * 0.5));
        CHECK ((double) aL == T && (double) aR == std::round (T * 0.5), msg);
        p.lrRatio = -0.5f;
        y = run (p, impulse ((size_t) (T * 1.3)), sr);
        CHECK ((double) argmaxAbs (y.l, 1, y.l.size()) == std::round (60.0 / 140.0 * sr * 0.75) && (double) argmaxAbs (y.r, 1, y.r.size()) == T, "L/R ratio -50 %: left at 3/4 T");
    }
    {
        // every analog mode keeps the echo near the grid when modulation is off
        const double sr = 48000.0;
        const double T = std::round (60.0 / 140.0 * sr);
        for (Mode m : { Mode::tape, Mode::bbd, Mode::ghost, Mode::diffuse })
        {
            auto p = clean (m);
            auto y = run (p, impulse ((size_t) (T * 1.3)), sr);
            const size_t at = argmaxAbs (y.l, 1, y.l.size());
            char msg[160];
            std::snprintf (msg, sizeof msg, "%s first echo at %zu (T=%.0f, within 1.5 ms)", modeNames[(size_t) m], at, T);
            const double tol = m == Mode::diffuse ? 0.020 * sr : 0.0015 * sr; // diffusion smears the peak
            CHECK (std::abs ((double) at - T) <= tol && (double) at >= T - 2, msg);
        }
    }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- GHOST: pitch-shifted feedback (spectral peak of the 2nd and 3rd repeat)\n");
    {
        const double sr = 48000.0;
        const double f0 = 330.0;
        const double T = std::round (2.0 * 60.0 / 140.0 * sr); // 1/2 note
        for (int gp = 0; gp < numGhostPitches; ++gp)
        {
            auto p = clean (Mode::ghost); p.feedback = 0.8f; p.ghostPitch = gp; p.highCutHz = 20000.0f; p.division = 16;
            auto x = silence ((size_t) (T * 3.8));
            const size_t burst = (size_t) (0.6 * sr);
            for (size_t i = 0; i < burst; ++i)
            {
                const double env = std::sin (PI * (double) i / (double) burst);
                x.l[i] = x.r[i] = (float) (0.5 * env * std::sin (2.0 * PI * f0 * (double) i / sr));
            }
            auto y = run (p, x, sr);
            const double ratio = ghostPitchRatios[(size_t) gp];
            // analyse the steady middle of each repeat
            auto seg = [&] (double k) { return std::pair<size_t, size_t> ((size_t) (k * T + 0.1 * sr), (size_t) (k * T + 0.5 * sr)); };
            const auto s1 = seg (1), s2 = seg (2), s3 = seg (3);
            const double p1 = spectralPeak (y.l, s1.first, s1.second, sr, 80, 2600);
            const double p2 = spectralPeak (y.l, s2.first, s2.second, sr, 80, 2600);
            const double p3 = spectralPeak (y.l, s3.first, s3.second, sr, 60, 3000);
            const double cents2 = 1200.0 * std::log2 (p2 / (f0 * ratio));
            const double cents3 = 1200.0 * std::log2 (p3 / (f0 * ratio * ratio));
            char msg[220];
            std::snprintf (msg, sizeof msg, "%s: repeat 1 %.1f Hz, repeat 2 %.1f Hz (%+.1f cents), repeat 3 %.1f Hz (%+.1f cents)",
                           ghostPitchNames[(size_t) gp], p1, p2, cents2, p3, cents3);
            CHECK (std::abs (p1 - f0) < 3.0 && std::abs (cents2) < 6.0 && std::abs (cents3) < 8.0, msg);
            std::snprintf (msg, sizeof msg, "%s: repeat 2 level %.3f (audible, bounded)", ghostPitchNames[(size_t) gp], rms (y.l, s2.first, s2.second));
            CHECK (rms (y.l, s2.first, s2.second) > 0.05 && finite (y), msg);
        }
    }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- REVERSE: tempo-locked grains play backwards\n");
    {
        const double sr = 48000.0;
        auto p = clean (Mode::reverse); p.division = 13; // 1/4
        const size_t T = (size_t) std::round (60.0 / 140.0 * sr);
        // smooth, asymmetric test signal: low partials under a rising ramp
        auto x = silence (T * 3);
        std::mt19937 rng (5);
        std::uniform_real_distribution<double> ph (0.0, 2.0 * PI);
        double phs[5]; for (auto& v : phs) v = ph (rng);
        for (size_t i = 0; i < T * 3; ++i)
        {
            double s = 0;
            for (int k = 0; k < 5; ++k) s += std::sin (2.0 * PI * (60.0 + 47.0 * k) * (double) i / sr + phs[k]);
            const double ramp = (double) (i % T) / (double) T;
            x.l[i] = x.r[i] = (float) (0.15 * s * (0.2 + ramp));
        }
        auto y = run (p, x, sr);
        // during grain [T, 2T) the output must be the input of [0, T) reversed: y[T + k] = x[T - k]
        const size_t fade = (size_t) (0.012 * sr);
        double sxy = 0, sxx = 0, syy = 0, fxy = 0, fxx = 0;
        for (size_t k = fade; k < T - fade; ++k)
        {
            const double a = y.l[T + k], b = x.l[T - k], c = x.l[k];
            sxy += a * b; sxx += b * b; syy += a * a; fxy += a * c; fxx += c * c;
        }
        const double corr = sxy / std::sqrt (sxx * syy), fwd = fxy / std::sqrt (fxx * syy);
        char msg[180];
        std::snprintf (msg, sizeof msg, "grain 2 vs reversed input: correlation %.4f (forward %.3f)", corr, fwd);
        CHECK (corr > 0.98 && std::abs (fwd) < 0.5, msg);
        // the rising ramp turns into a falling envelope
        const double first = rms (y.l, T + fade, T + T / 2), second = rms (y.l, T + T / 2, 2 * T - fade);
        std::snprintf (msg, sizeof msg, "rising input ramp plays as a falling envelope (%.3f -> %.3f)", first, second);
        CHECK (first > second * 1.8, msg);
        // grain boundaries are locked to the host bar grid (ppq) while playing
        Engine e; e.prepare (sr, 512);
        auto pp = p; pp.playing = true; pp.ppq = 2.5; // half way into a beat
        e.setParams (pp);
        auto z = impulse (16);
        float* ch[2] = { z.l.data(), z.r.data() };
        e.process (ch, 2, 16);
        std::snprintf (msg, sizeof msg, "grain phase locked to the host position (phase %.3f, expected ~0.5)", e.getReversePhase());
        CHECK (std::abs (e.getReversePhase() - 0.5) < 0.01, msg);
    }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- RHYTHM: multi-tap patterns\n");
    {
        const double sr = 48000.0;
        const double T = std::round (60.0 / 140.0 * sr);
        for (int pi = 0; pi < numPatterns; ++pi)
        {
            auto p = clean (Mode::rhythm); p.pattern = pi;
            const auto& pat = rhythmPattern (pi);
            auto y = run (p, impulse ((size_t) (T * 3.3), 0.5f), sr);
            int ok = 0;
            for (int k = 0; k < pat.numTaps; ++k)
            {
                const double at = std::round (pat.times[(size_t) k] * T);
                const float a = std::abs (y.l[(size_t) at]) + std::abs (y.r[(size_t) at]);
                if (a > 0.1f) ++ok;
            }
            char msg[160];
            std::snprintf (msg, sizeof msg, "%s: %d/%d taps on their grid positions", patternNames[(size_t) pi], ok, pat.numTaps);
            CHECK (ok == pat.numTaps, msg);
        }
    }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- DIFFUSE smears, TAPE glides\n");
    {
        const double sr = 48000.0;
        const double T = std::round (60.0 / 140.0 * sr);
        auto pd = clean (Mode::diffuse); pd.feedback = 0.6f;
        auto pg = clean (Mode::digital); pg.feedback = 0.6f;
        auto yd = run (pd, impulse ((size_t) (T * 3.2)), sr);
        auto yg = run (pg, impulse ((size_t) (T * 3.2)), sr);
        // energy spread: count samples above -40 dB around the 3rd repeat
        auto spread = [&] (const std::vector<float>& v)
        {
            const float pk = peakIn (v, (size_t) (2.5 * T), (size_t) (3.2 * T));
            int c = 0; for (size_t i = (size_t) (2.5 * T); i < (size_t) (3.2 * T); ++i) if (std::abs (v[i]) > pk * 0.01f) ++c;
            return c;
        };
        char msg[160];
        std::snprintf (msg, sizeof msg, "3rd repeat is smeared: %d samples above -40 dB (digital: %d)", spread (yd.l), spread (yg.l));
        CHECK (spread (yd.l) > 50 * spread (yg.l) && spread (yd.l) > 1000, msg);

        // time change glides (pitch bend) instead of jumping
        Engine e; e.prepare (sr, 256);
        auto p = clean (Mode::tape); p.sync = false; p.timeMs = 400.0f;
        e.setParams (p);
        auto x = silence (256);
        float* ch[2] = { x.l.data(), x.r.data() };
        for (int b = 0; b < 40; ++b) e.process (ch, 2, 256);
        p.timeMs = 200.0f; e.setParams (p);
        e.process (ch, 2, 256);
        const double mid = e.getCurrentDelayMs();
        for (int b = 0; b < 400; ++b) e.process (ch, 2, 256);
        std::snprintf (msg, sizeof msg, "400 -> 200 ms glides (%.1f ms after 5 ms, %.2f ms after 2 s)", mid, e.getCurrentDelayMs());
        CHECK (mid > 350.0 && mid < 399.0 && std::abs (e.getCurrentDelayMs() - 200.0) < 0.05, msg);
    }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- mix = 0 is bit-exact dry (every mode, 44.1 / 96 kHz, mono + stereo)\n");
    for (double sr : { 44100.0, 96000.0 })
        for (int m = 0; m < numModes; ++m)
        {
            auto p = clean ((Mode) m); p.mix = 0.0f; p.feedback = 0.9f; p.age = 0.8f; p.modDepth = 0.7f; p.duck = 0.5f; p.lowCutHz = 300; p.highCutHz = 4000;
            std::mt19937 rng (11 + m);
            std::uniform_real_distribution<float> u (-1.0f, 1.0f);
            auto x = silence ((size_t) (sr * 0.5));
            for (size_t i = 0; i < x.l.size(); ++i) { x.l[i] = u (rng); x.r[i] = u (rng); }
            auto y = run (p, x, sr, 333);
            bool exact = std::memcmp (x.l.data(), y.l.data(), x.l.size() * sizeof (float)) == 0
                      && std::memcmp (x.r.data(), y.r.data(), x.r.size() * sizeof (float)) == 0;
            // mono
            Engine e; e.prepare (sr, 333); e.setParams (p);
            auto xm = x.l;
            for (size_t pos = 0; pos < xm.size(); pos += 333)
            {
                float* ch[1] = { xm.data() + pos };
                e.process (ch, 1, (int) std::min<size_t> (333, xm.size() - pos));
            }
            exact = exact && std::memcmp (xm.data(), x.l.data(), xm.size() * sizeof (float)) == 0;
            char msg[120];
            std::snprintf (msg, sizeof msg, "%s @ %.0f Hz", modeNames[(size_t) m], sr);
            CHECK (exact, msg);
        }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- 60 s at maximum feedback (110 %%), every mode: bounded self-oscillation\n");
    for (int m = 0; m < numModes; ++m)
    {
        const double sr = 48000.0;
        auto p = clean ((Mode) m); p.feedback = maxFeedback; p.age = 1.0f; p.modDepth = 1.0f; p.modRate = 6.0f; p.mix = 1.0f;
        p.division = 7; // 1/16
        Engine e; e.prepare (sr, 512); e.setParams (p);
        std::mt19937 rng (3 + m);
        std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        std::vector<float> l (512), r (512);
        float pk = 0.0f, lastPk = 0.0f;
        bool ok = true;
        const int blocks = (int) (60.0 * sr / 512.0);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i) { const float v = b < 40 ? u (rng) * 0.9f : 0.0f; l[(size_t) i] = v; r[(size_t) i] = b < 40 ? u (rng) * 0.9f : 0.0f; }
            float* ch[2] = { l.data(), r.data() };
            e.process (ch, 2, 512);
            for (int i = 0; i < 512; ++i)
            {
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) ok = false;
                pk = std::max (pk, std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
                if (b >= blocks - 100) lastPk = std::max (lastPk, std::abs (l[(size_t) i]));
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        char msg[200];
        std::snprintf (msg, sizeof msg, "%-10s finite, peak %.2f (< 2.2), still sounding after 60 s (%.2f), %.1fx realtime",
                       modeNames[(size_t) m], pk, lastPk, 60.0 / secs);
        CHECK (ok && pk < 2.2f, msg);
    }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- FREEZE holds, duck ducks, fuzzing\n");
    {
        const double sr = 48000.0;
        Engine e; e.prepare (sr, 512);
        auto p = clean (Mode::tape); p.feedback = 0.3f; p.division = 10;
        e.setParams (p);
        std::mt19937 rng (9);
        std::uniform_real_distribution<float> u (-0.5f, 0.5f);
        std::vector<float> l (512), r (512);
        for (int b = 0; b < 60; ++b)
        {
            for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = u (rng);
            float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, 512);
        }
        p.freeze = true; e.setParams (p);
        float lvl = 0.0f;
        for (int b = 0; b < 1000; ++b) // ~10 s, silent input
        {
            std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f);
            float* ch[2] = { l.data(), r.data() }; e.process (ch, 2, 512);
            if (b > 990) lvl = std::max (lvl, peakIn (l, 0, 512));
        }
        char msg[120];
        std::snprintf (msg, sizeof msg, "freeze still holds the loop after 10 s of silence (peak %.3f)", lvl);
        CHECK (lvl > 0.1f && lvl < 1.3f, msg);

        // duck: echoes drop while the input is loud
        auto pd = clean (Mode::digital); pd.feedback = 0.0f; pd.duck = 1.0f; pd.mix = 1.0f;
        auto x = silence ((size_t) sr);
        for (size_t i = 0; i < x.l.size(); ++i) x.l[i] = x.r[i] = (float) (0.5 * std::sin (2.0 * PI * 200.0 * (double) i / sr));
        auto yd = run (pd, x, sr);
        auto pn = pd; pn.duck = 0.0f;
        auto yn = run (pn, x, sr);
        const double a = rms (yd.l, (size_t) (0.6 * sr), (size_t) sr), b = rms (yn.l, (size_t) (0.6 * sr), (size_t) sr);
        std::snprintf (msg, sizeof msg, "duck 100 %% under a constant vocal: %.1f dB", 20.0 * std::log10 (a / b));
        CHECK (a < b * 0.1, msg);

        // fuzz: random params every block, random block sizes, all sample rates
        bool ok = true;
        for (double fsr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            Engine f; f.prepare (fsr, 2048);
            std::mt19937 g (77);
            std::uniform_real_distribution<float> uu (0.0f, 1.0f);
            std::vector<float> fl (2048), fr (2048);
            for (int b = 0; b < 300; ++b)
            {
                Params q;
                q.mode = (Mode) (g() % numModes); q.sync = (g() & 1) != 0; q.division = (int) (g() % numDivisions);
                q.timeMs = minFreeMs + uu (g) * (maxFreeMs - minFreeMs); q.bpm = 40.0 + uu (g) * 260.0;
                q.lrRatio = uu (g) * 2.0f - 1.0f; q.feedback = uu (g) * maxFeedback; q.lowCutHz = 20.0f + uu (g) * 1980.0f;
                q.highCutHz = 1000.0f + uu (g) * 19000.0f; q.modDepth = uu (g); q.modRate = minModRate + uu (g) * (maxModRate - minModRate);
                q.age = uu (g); q.duck = uu (g); q.freeze = (g() % 7) == 0; q.ghostPitch = (int) (g() % 3); q.pattern = (int) (g() % 3);
                q.mix = uu (g); q.outputDb = -24.0f + uu (g) * 36.0f; q.playing = (g() & 1) != 0; q.ppq = uu (g) * 64.0;
                f.setParams (q);
                const int n = 1 + (int) (g() % 2048);
                for (int i = 0; i < n; ++i) { fl[(size_t) i] = (uu (g) - 0.5f) * 2.0f; fr[(size_t) i] = (uu (g) - 0.5f) * 2.0f; }
                float* ch[2] = { fl.data(), fr.data() };
                f.process (ch, (g() & 3) == 0 ? 1 : 2, n);
                for (int i = 0; i < n; ++i) if (! std::isfinite (fl[(size_t) i]) || std::abs (fl[(size_t) i]) > 50.0f) ok = false;
            }
        }
        CHECK (ok, "fuzzing (random params / block sizes / mono, 44.1-192 kHz): finite and bounded");

        // silence in -> silence out (no hiss without signal), denormal-safe tail
        auto pz = clean (Mode::tape); pz.age = 1.0f; pz.feedback = 0.95f;
        auto yz = run (pz, impulse ((size_t) (sr * 20), 0.5f), sr);
        const float tail = peakIn (yz.l, (size_t) (sr * 19), (size_t) (sr * 20));
        std::snprintf (msg, sizeof msg, "tape tail decays to silence, no free-running hiss (last second peak %.2e)", tail);
        CHECK (finite (yz) && tail < 1e-3f, msg);
    }

    // ------------------------------------------------------------------------------------------------
    std::printf ("-- factory presets\n");
    {
        const auto& list = factoryPresets();
        std::set<std::string> names;
        std::set<int> modesUsed;
        for (const auto& pr : list) { names.insert (pr.name); modesUsed.insert ((int) pr.params.mode); }
        char msg[160];
        std::snprintf (msg, sizeof msg, "%zu presets (>= 40), names unique, all %d modes covered", list.size(), numModes);
        CHECK (list.size() >= 40 && names.size() == list.size() && (int) modesUsed.size() == numModes, msg);
        const double sr = 48000.0;
        int bad = 0;
        float loudest = 0.0f;
        for (const auto& pr : list)
        {
            auto p = pr.params; p.bpm = 140.0;
            auto x = silence ((size_t) (sr * 4));
            std::mt19937 rng (21);
            std::uniform_real_distribution<float> u (-0.5f, 0.5f);
            for (size_t i = 0; i < (size_t) (sr * 0.5); ++i) { x.l[i] = u (rng); x.r[i] = u (rng); }
            auto y = run (p, x, sr);
            const float pk = std::max (peakIn (y.l, 0, y.l.size()), peakIn (y.r, 0, y.r.size()));
            loudest = std::max (loudest, pk);
            if (! finite (y) || pk > 4.0f) { ++bad; std::printf ("     %s: peak %.2f\n", pr.name, pk); }
        }
        std::snprintf (msg, sizeof msg, "every preset renders finite and bounded (loudest peak %.2f)", loudest);
        CHECK (bad == 0, msg);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
