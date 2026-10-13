// Headless tests for the VOX-1 engine.
#include "../Source/DSP/VoxEngine.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace vox;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;
static const double SR = 48000.0;

// voice-like test signal: band-limited pulse train through two formant resonators (vowel "a")
static std::vector<float> vowel (double f0, double seconds, double f1 = 750, double f2 = 1150, double vibratoCents = 0)
{
    const size_t n = (size_t) (SR * seconds);
    std::vector<float> src (n), out (n);
    double ph = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double f = f0 * std::pow (2.0, vibratoCents / 1200.0 * std::sin (2 * PI * 5.5 * i / SR));
        ph += f / SR; if (ph >= 1) ph -= 1;
        double v = 0;
        for (int h = 1; h * f < SR * 0.45; ++h) v += std::sin (2 * PI * h * ph) / std::sqrt ((double) h);
        src[i] = (float) (v * 0.08);
    }
    auto reson = [&] (std::vector<float>& x, double fc, double bw)
    {
        const double r = std::exp (-PI * bw / SR), c = 2 * r * std::cos (2 * PI * fc / SR);
        double y1 = 0, y2 = 0; std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i) { const double v = (1 - r) * x[i] + c * y1 - r * r * y2; y2 = y1; y1 = v; y[i] = (float) v; }
        return y;
    };
    auto a = reson (src, f1, 90), b = reson (src, f2, 110);
    for (size_t i = 0; i < n; ++i) out[i] = (a[i] * 3.0f + b[i] * 2.0f);
    float pk = 0; for (float v : out) pk = std::max (pk, std::abs (v));
    for (float& v : out) v *= 0.5f / pk;
    return out;
}

static std::vector<float> run (const std::vector<float>& x, Params p, int block = 256, int& latency = *(new int))
{
    Engine e; e.prepare (SR, block); e.setParams (p);
    latency = e.getLatencySamples();
    std::vector<float> l = x, r = x;
    for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, l.size() - pos);
        float* ch[2] = { l.data() + pos, r.data() + pos };
        e.process (ch, 2, n);
    }
    return l;
}

static float measureF0 (const std::vector<float>& x, size_t from)
{
    YinTracker y; y.prepare (SR);
    float conf; std::vector<float> w (x.begin() + (long) from, x.begin() + (long) from + 4000);
    return y.analyse (w.data(), (int) w.size(), conf);
}

// fraction of energy above 1.6 kHz (rises when formants move up)
static double brightness (const std::vector<float>& x, size_t from, size_t len)
{
    double lo = 0, hi = 0;
    for (int k = 1; k < 200; ++k)
    {
        const double f = k * 40.0; double re = 0, im = 0;
        for (size_t i = 0; i < len; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * PI * i / len); re += x[from + i] * w * std::cos (2 * PI * f * i / SR); im += x[from + i] * w * std::sin (2 * PI * f * i / SR); }
        const double e = re * re + im * im;
        (f > 1600 ? hi : lo) += e;
    }
    return hi / (lo + hi);
}

int main (int argc, char** argv)
{
    (void) argc; (void) argv;
    std::printf ("[scale snapping]\n");
    CHECK (snapToScale (57.4f, 0, 0, true) == 57.0f, "chromatic 57.4 -> 57");
    CHECK (snapToScale (58.2f, 0, 0, false) == 57.0f || snapToScale (58.2f, 0, 0, false) == 59.0f, "C major skips A#");
    CHECK (snapToScale (61.0f, 9, 1, false) == 60.0f || snapToScale (61.0f, 9, 1, false) == 62.0f, "A minor skips C#");

    std::printf ("[pitch tracker]\n");
    for (double f : { 82.0, 147.0, 220.0, 440.0, 700.0 })
    {
        auto v = vowel (f, 0.5, 750, 1150);
        const float m = measureF0 (v, 6000);
        char msg[96]; std::snprintf (msg, sizeof msg, "%.0f Hz detected as %.1f Hz", f, m); CHECK (std::abs (m / f - 1.0) < 0.01, msg);
    }

    std::printf ("[pitch shift keeps formants]\n");
    {
        int lat = 0;
        auto v = vowel (150.0, 2.0);
        Params p; p.pitchSemis = 0; auto same = run (v, p, 256, lat);
        p.pitchSemis = 7; auto up = run (v, p, 256, lat);
        p.pitchSemis = -12; auto down = run (v, p, 256, lat);
        const size_t at = 40000;
        const float f0 = measureF0 (same, at), fu = measureF0 (up, at), fd = measureF0 (down, at);
        char m1[128]; std::snprintf (m1, sizeof m1, "0 st -> %.1f Hz (150), +7 st -> %.1f Hz (224.7), -12 st -> %.1f Hz (75)", f0, fu, fd);
        CHECK (std::abs (f0 / 150 - 1) < 0.015 && std::abs (fu / 224.7 - 1) < 0.015 && std::abs (fd / 75 - 1) < 0.02, m1);
        const double b0 = brightness (same, at, 4096), bu = brightness (up, at, 4096);
        char m2[128]; std::snprintf (m2, sizeof m2, "brightness 0 st %.3f vs +7 st %.3f (formants stay)", b0, bu);
        CHECK (std::abs (bu - b0) < 0.35 * b0 + 0.01, m2);
        std::snprintf (m2, sizeof m2, "latency %d samples (%.1f ms)", lat, 1000.0 * lat / SR);
        CHECK (lat > 0 && lat < SR * 0.08, m2);
        // passthrough level
        double ein = 0, eout = 0; for (size_t i = at; i < at + 20000; ++i) { ein += (double) v[i - (size_t) lat] * v[i - (size_t) lat]; eout += (double) same[i] * same[i]; }
        std::snprintf (m2, sizeof m2, "0 st level change %.2f dB", 10 * std::log10 (eout / ein));
        CHECK (std::abs (10 * std::log10 (eout / ein)) < 1.5, m2);
    }

    std::printf ("[formant moves independently]\n");
    {
        int lat = 0; auto v = vowel (150.0, 2.0);
        Params p; auto base = run (v, p, 256, lat);
        p.formantSemis = 6; auto hi = run (v, p, 256, lat);
        p.formantSemis = -6; auto lo = run (v, p, 256, lat);
        const size_t at = 40000;
        const double b0 = brightness (base, at, 4096), bh = brightness (hi, at, 4096), bl = brightness (lo, at, 4096);
        const float fh = measureF0 (hi, at), fl = measureF0 (lo, at);
        char m[160]; std::snprintf (m, sizeof m, "brightness -6: %.3f  0: %.3f  +6: %.3f; pitch stays %.1f / %.1f Hz", bl, b0, bh, fl, fh);
        CHECK (bh > b0 * 1.3 && bl < b0 * 0.8 && std::abs (fh / 150 - 1) < 0.02 && std::abs (fl / 150 - 1) < 0.02, m);
    }

    std::printf ("[hard tune]\n");
    {
        int lat = 0; auto v = vowel (225.0, 1.5); // between A3 (220) and A#3 (233)
        Params p; p.tune = TuneMode::chromatic; p.retuneMs = 0; auto t = run (v, p, 256, lat);
        const float f = measureF0 (t, 50000);
        char m[96]; std::snprintf (m, sizeof m, "225 Hz chromatic -> %.1f Hz (220)", f); CHECK (std::abs (f / 220 - 1) < 0.012, m);
        auto v2 = vowel (230.0, 1.5);  // closest is A#3, but C major has no A# -> A3 or B3
        p.tune = TuneMode::scale; p.key = 0; p.scale = 0; auto t2 = run (v2, p, 256, lat);
        const float f2 = measureF0 (t2, 50000);
        std::snprintf (m, sizeof m, "230 Hz in C major -> %.1f Hz (220 or 246.9)", f2);
        CHECK (std::abs (f2 / 220 - 1) < 0.012 || std::abs (f2 / 246.94 - 1) < 0.012, m);
        auto v3 = vowel (220.0, 2.0, 750, 1150, 40);   // 40-cent vibrato around A3
        p.tune = TuneMode::chromatic; p.retuneMs = 0; auto robot = run (v3, p, 256, lat);
        float spreadR = 0, mnR = 1e9f, mxR = 0;
        for (size_t at = 40000; at < 90000; at += 2400) { const float q = measureF0 (robot, at); mnR = std::min (mnR, q); mxR = std::max (mxR, q); }
        spreadR = 1200.0f * std::log2 (mxR / mnR);
        std::snprintf (m, sizeof m, "vibrato 80 cents p-p flattened to %.0f cents by hard tune", spreadR);
        CHECK (spreadR < 30.0f, m);
    }

    std::printf ("[random params / silence: finite]\n");
    {
        std::mt19937 g (5); std::uniform_real_distribution<float> u (0, 1);
        Engine e; e.prepare (44100, 1024);
        std::vector<float> l (44100 * 4), r (l.size());
        auto v = vowel (180, 4.0); for (size_t i = 0; i < l.size() && i < v.size(); ++i) { l[i] = v[i] * (i % 40000 < 20000 ? 1.0f : 0.0f); r[i] = l[i]; }
        bool ok = true; size_t pos = 0;
        while (pos < l.size())
        {
            Params p; p.pitchSemis = (u (g) - 0.5f) * 48; p.formantSemis = (u (g) - 0.5f) * 24; p.tune = (TuneMode) ((int) (u (g) * 3) % 3);
            p.key = (int) (u (g) * 12) % 12; p.scale = (int) (u (g) * 6) % 6; p.retuneMs = u (g) * 400; p.midiControl = u (g) > 0.5f; p.mix = u (g);
            e.setParams (p); e.setMidiNote (u (g) > 0.5f ? 60 : -1);
            const int n = (int) std::min<size_t> (1 + (size_t) (u (g) * 1023), l.size() - pos);
            float* ch[2] = { l.data() + pos, r.data() + pos }; e.process (ch, 2, n); pos += (size_t) n;
        }
        float pk = 0; for (float x : l) { if (! std::isfinite (x)) ok = false; pk = std::max (pk, std::abs (x)); }
        CHECK (ok, "no NaN / Inf");
        char m[64]; std::snprintf (m, sizeof m, "peak %.2f bounded", pk); CHECK (pk < 4.0f, m);
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
