// Headless tests for the AUTO KILLA tuner engine.
#include "../Source/DSP/TunerEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace akill;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;
static double SR = 48000.0;

// ---------------------------------------------------------------- test signals
// voice-like: band-limited glottal pulse train through formant resonators. f0 given per sample (Hz, 0 = silence)
static std::vector<float> voice (const std::vector<double>& f0, double f1 = 700, double f2 = 1200, double f3 = 2600)
{
    const size_t n = f0.size();
    std::vector<float> src (n, 0.0f), out (n, 0.0f);
    double ph = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double f = f0[i];
        if (f <= 0) { src[i] = 0; continue; }
        ph += f / SR; if (ph >= 1) ph -= 1;
        double v = 0;
        for (int h = 1; h * f < SR * 0.45 && h < 60; ++h) v += std::sin (2 * PI * h * ph) / (double) h;
        src[i] = (float) (v * 0.1);
    }
    auto reson = [&] (const std::vector<float>& x, double fc, double bw)
    {
        const double r = std::exp (-PI * bw / SR), c = 2 * r * std::cos (2 * PI * fc / SR);
        double y1 = 0, y2 = 0; std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i) { const double v = (1 - r) * x[i] + c * y1 - r * r * y2; y2 = y1; y1 = v; y[i] = (float) v; }
        return y;
    };
    auto a = reson (src, f1, 90), b = reson (src, f2, 110), c = reson (src, f3, 160);
    for (size_t i = 0; i < n; ++i) out[i] = a[i] * 3.0f + b[i] * 2.0f + c[i] * 1.0f;
    float pk = 1e-9f; for (float v : out) pk = std::max (pk, std::abs (v));
    for (float& v : out) v *= 0.5f / pk;
    return out;
}

static std::vector<double> steady (double hz, double seconds, double vibCents = 0, double vibHz = 5.5)
{
    std::vector<double> f ((size_t) (SR * seconds));
    for (size_t i = 0; i < f.size(); ++i) f[i] = hz * std::pow (2.0, vibCents / 1200.0 * std::sin (2 * PI * vibHz * i / SR));
    return f;
}

struct Run { std::vector<float> l, r; int latency = 0; };

static Run run (const std::vector<float>& x, Params p, int block = 256, Engine* keep = nullptr, int midiMask = 0)
{
    Engine local; Engine& e = keep ? *keep : local;
    e.setParams (p); e.prepare (SR, block); e.setMidiMask (midiMask);
    Run res; res.latency = e.getLatencySamples();
    res.l = x; res.r = x;
    for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.size() - pos);
        float* ch[2] = { res.l.data() + pos, res.r.data() + pos };
        e.process (ch, 2, n);
    }
    return res;
}

// precise f0 of x around 'from' (full-rate YIN, averaged over k windows)
static float f0At (const std::vector<float>& x, size_t from, int k = 6, int hopS = 1200)
{
    PitchTracker y; y.prepare (900);
    double sum = 0; int cnt = 0;
    for (int i = 0; i < k; ++i)
    {
        const size_t at = from + (size_t) (i * hopS);
        if (at + 2600 > x.size()) break;
        float ap; const float t = y.analyse (x.data() + at, 1600, 40, 900, ap);
        if (t > 0) { sum += SR / t; ++cnt; }
    }
    return cnt ? (float) (sum / cnt) : 0.0f;
}

static float cents (float a, float b) { return 1200.0f * std::log2 (a / b); }
static float nearestNoteCents (float hz, int mask)
{
    const float m = hzToMidi (hz);
    float best = 1e9f;
    for (int n = (int) m - 3; n <= (int) m + 3; ++n)
        if ((mask >> (((n % 12) + 12) % 12)) & 1) best = std::min (best, std::abs (m - (float) n) * 100.0f);
    return best;
}

// fraction of energy above 1 kHz
static double brightness (const std::vector<float>& x, size_t from, size_t len)
{
    double lo = 0, hi = 0;
    for (int k = 1; k < 200; ++k)
    {
        const double f = k * 40.0; double re = 0, im = 0;
        for (size_t i = 0; i < len; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * PI * i / len); re += x[from + i] * w * std::cos (2 * PI * f * i / SR); im += x[from + i] * w * std::sin (2 * PI * f * i / SR); }
        (f > 1000 ? hi : lo) += re * re + im * im;
    }
    return hi / (lo + hi);
}

static std::vector<double> melody (const std::vector<std::pair<int, double>>& notes, double seconds, double bpm = 120)
{
    std::vector<double> f ((size_t) (SR * seconds), 0.0);
    size_t i = 0; const double beat = 60.0 / bpm;
    while (i < f.size())
        for (auto [note, beats] : notes)
        {
            const size_t len = (size_t) (beats * beat * SR), gap = (size_t) (0.04 * SR);
            const double hz = 440.0 * std::pow (2.0, (note - 69) / 12.0) * std::pow (2.0, 18.0 / 1200.0); // sung 18 cents sharp
            for (size_t k = 0; k < len && i < f.size(); ++k, ++i) f[i] = k + gap < len ? hz : 0.0;
            if (i >= f.size()) break;
        }
    return f;
}

int main()
{
    std::printf ("[scales & snapping]\n");
    {
        CHECK (scaleMask (7, 0, 0) == 0xFFF, "chromatic = all 12 notes");
        const int am = scaleMask (1, 9, 0);  // A minor = C major notes
        CHECK (am == scaleMask (0, 0, 0), "A minor has the notes of C major");
        CHECK (snapToMask (57.4f, 0xFFF, -1, 0.0f) == 57, "chromatic 57.4 -> 57");
        const int s = snapToMask (58.2f, scaleMask (0, 0, 0), -1, 0.0f);
        CHECK (s == 57 || s == 59, "C major skips A#");
        CHECK (snapToMask (60.55f, 0xFFF, 60, 0.12f) == 60, "hysteresis keeps the held note just past the midpoint");
        CHECK (snapToMask (60.75f, 0xFFF, 60, 0.12f) == 61, "... but moves on when clearly closer to the next");
        CHECK (scaleMask (8, 0, 0b000010010001) == 0b000010010001, "custom per-note mask");
        CHECK (snapToMask (60.0f, 0, -1, 0.1f) == -1, "empty custom mask -> no target");
        const int pentMinor = scaleMask (4, 4, 0); // E minor pentatonic: E G A B D
        CHECK (pentMinor == ((1 << 4) | (1 << 7) | (1 << 9) | (1 << 11) | (1 << 2)), "E minor pentatonic = E G A B D");
    }

    std::printf ("[pitch tracker]\n");
    for (double f : { 82.0, 147.0, 220.0, 440.0, 700.0 })
    {
        auto v = voice (steady (f, 0.5));
        const float m = f0At (v, 6000);
        char msg[96]; std::snprintf (msg, sizeof msg, "%.0f Hz detected as %.2f Hz (%.2f cents)", f, m, cents (m, (float) f));
        CHECK (std::abs (cents (m, (float) f)) < 2.0f, msg);
    }

    for (Mode mode : { Mode::live, Mode::mix })
    {
        const char* mn = mode == Mode::live ? "LIVE" : "MIX";
        std::printf ("[hard tune, %s]\n", mn);
        const int mask = scaleMask (1, 9, 0); // A minor
        float worst = 0.0f;
        for (double hz : { 84.0, 112.0, 151.0, 203.0, 226.0, 262.0, 311.0, 410.0, 560.0 })
        {
            auto v = voice (steady (hz, 1.6));
            Params p; p.mode = mode; p.key = 9; p.scale = 1; p.retuneMs = 0;
            auto r = run (v, p);
            const float out = f0At (r.l, 40000, 8);
            const float c = nearestNoteCents (out, mask);
            worst = std::max (worst, c);
            char m[128]; std::snprintf (m, sizeof m, "%s %.0f Hz -> %.2f Hz, %.2f cents from an A-minor note", mn, hz, out, c);
            CHECK (c < 3.0f, m);
        }
        {
            auto v = voice (steady (226.0, 1.6));
            Params p; p.mode = mode; p.scale = scaleChromatic; p.retuneMs = 0;
            auto r = run (v, p);
            const float out = f0At (r.l, 40000, 8);
            char m[128]; std::snprintf (m, sizeof m, "%s chromatic 226 Hz -> %.2f Hz (A3 220, %.2f cents)", mn, out, cents (out, 220.0f));
            CHECK (std::abs (cents (out, 220.0f)) < 3.0f, m);
        }
    }

    std::printf ("[latency matches report]\n");
    for (Mode mode : { Mode::live, Mode::mix })
    {
        const char* mn = mode == Mode::live ? "LIVE" : "MIX";
        Engine probe; Params p; p.mode = mode; probe.setParams (p); probe.prepare (SR, 256);
        const int rep = probe.getLatencySamples();
        CHECK (rep == Engine::latencyFor (mode, SR), "reported latency = latencyFor()");
        char m[160];
        std::snprintf (m, sizeof m, "%s reports %d samples = %.2f ms", mn, rep, 1000.0 * rep / SR);
        CHECK (mode == Mode::live ? rep <= SR * 0.020 : rep <= SR * 0.050, m);

        // (a) noise (unvoiced) passes with exactly the reported delay
        std::mt19937 g (3); std::normal_distribution<float> nd (0, 0.15f);
        std::vector<float> nz ((size_t) SR); for (auto& v : nz) v = nd (g);
        auto r = run (nz, p);
        int bestLag = -1; double bestC = -1;
        for (int lag = 0; lag < 4000; ++lag)
        {
            double c = 0; for (size_t i = 10000; i < 30000; ++i) c += (double) r.l[i] * nz[i - (size_t) lag];
            if (c > bestC) { bestC = c; bestLag = lag; }
        }
        std::snprintf (m, sizeof m, "%s noise: measured delay %d samples (reported %d)", mn, bestLag, rep);
        CHECK (bestLag == rep, m);

        // (b) tuned voice: syllable onsets come out at the reported delay (+-1 ms)
        std::vector<double> f ((size_t) (SR * 2.0), 0.0);
        for (size_t i = 0; i < f.size(); ++i) { const double t = std::fmod ((double) i / SR, 0.25); f[i] = t < 0.17 ? 207.0 : 0.0; }
        auto v = voice (f);
        Params q = p; q.retuneMs = 0; q.scale = scaleChromatic;
        auto rv = run (v, q);
        auto env = [] (const std::vector<float>& x) { std::vector<float> e (x.size()); float s = 0; for (size_t i = 0; i < x.size(); ++i) { s += 0.01f * (std::abs (x[i]) - s); e[i] = s; } return e; };
        auto ei = env (v), eo = env (rv.l);
        int bl = 0; double bc = -1;
        for (int lag = 0; lag < 4000; ++lag) { double c = 0; for (size_t i = 24000; i < 90000; i += 2) c += (double) eo[i] * ei[i - (size_t) lag]; if (c > bc) { bc = c; bl = lag; } }
        std::snprintf (m, sizeof m, "%s tuned voice: measured delay %.2f ms (reported %.2f ms)", mn, 1000.0 * bl / SR, 1000.0 * rep / SR);
        CHECK (std::abs (bl - rep) <= (int) (SR * 0.001), m);
    }

    std::printf ("[retune speed = glide time]\n");
    {
        // 220 Hz (A3) then a jump to 230 Hz: in C major the nearest note stays A3 -> correction -77 cents
        std::vector<double> f ((size_t) (SR * 2.0));
        for (size_t i = 0; i < f.size(); ++i) f[i] = i < (size_t) SR ? 220.0 : 230.0;
        auto v = voice (f);
        float prev = 0.0f;
        bool mono = true, allOk = true;
        for (float rt : { 0.0f, 60.0f, 150.0f, 300.0f })
        {
            Params p; p.mode = Mode::mix; p.key = 0; p.scale = 0; p.retuneMs = rt;
            auto r = run (v, p);
            // output pitch track after the jump (in output time: shift by latency)
            const size_t jump = (size_t) SR + (size_t) r.latency;
            float t90 = -1;
            for (int k = 0; k < 120; ++k)
            {
                const size_t at = jump + (size_t) (k * 0.005 * SR);
                PitchTracker y; y.prepare (900); float ap;
                const float tt = y.analyse (r.l.data() + at, 700, 40, 900, ap);
                const float hz = tt > 0 ? (float) (SR / tt) : 0.0f;
                // 90 % of the way from 230 to 220 Hz = within 7.7 cents of 220
                if (hz > 0 && std::abs (cents (hz, 220.0f)) < 7.7f) { t90 = (float) (k * 5.0); break; }
            }
            char m[160]; std::snprintf (m, sizeof m, "retune %3.0f ms -> output reaches 90 %% after %.0f ms", rt, t90);
            const bool ok = t90 >= 0 && (rt < 1 ? t90 <= 25.0f : std::abs (t90 - rt) <= 0.35f * rt + 20.0f);
            CHECK (ok, m);
            allOk = allOk && ok;
            if (t90 < prev) mono = false;
            prev = t90;
        }
        CHECK (mono && allOk, "slower retune -> longer glide");
    }

    std::printf ("[vibrato vs humanize]\n");
    {
        auto v = voice (steady (220.0, 2.5, 40.0)); // 80 cents p-p vibrato around A3
        auto spread = [&] (const std::vector<float>& x, float& mean)
        {
            float mn = 1e9f, mx = 0; double s = 0; int c = 0;
            for (size_t at = 40000; at < 110000; at += 600)
            {
                PitchTracker y; y.prepare (900); float ap;
                const float t = y.analyse (x.data() + at, 500, 40, 900, ap);
                if (t <= 0) continue;
                const float hz = (float) (SR / t); mn = std::min (mn, hz); mx = std::max (mx, hz); s += cents (hz, 220.0f); ++c;
            }
            mean = c ? (float) (s / c) : 999.0f;
            return 1200.0f * std::log2 (mx / mn);
        };
        float mIn, mHard, mHum, mSyn;
        const float sIn = spread (v, mIn);
        Params p; p.mode = Mode::mix; p.scale = scaleChromatic; p.retuneMs = 0;
        const float sHard = spread (run (v, p).l, mHard);
        float mLive; Params pl = p; pl.mode = Mode::live;
        const float sLive = spread (run (v, pl).l, mLive);
        p.humanize = 1.0f;
        const float sHum = spread (run (v, p).l, mHum);
        char m[200];
        std::snprintf (m, sizeof m, "input vibrato %.0f cents p-p; hard tune flattens it to %.0f", sIn, sHard);
        CHECK (sHard < 25.0f && sIn > 70.0f, m);
        std::snprintf (m, sizeof m, "LIVE hard tune flattens it to %.0f cents", sLive);
        CHECK (sLive < 30.0f, m);
        std::snprintf (m, sizeof m, "humanize 100 %% keeps %.0f cents of vibrato, centred %.1f cents from A3", sHum, mHum);
        CHECK (sHum > 0.75f * sIn && std::abs (mHum) < 10.0f, m);
        // humanize with an off-pitch singer: vibrato kept, centre still corrected
        auto off = voice (steady (226.0, 2.5, 40.0));
        float mOff; const float sOff = spread (run (off, p).l, mOff);
        std::snprintf (m, sizeof m, "46-cent-sharp singer + humanize: vibrato %.0f cents kept, centre %.1f cents from A3", sOff, mOff);
        CHECK (sOff > 0.75f * sIn && std::abs (mOff) < 10.0f, m);
        // synthetic vibrato on a dead-straight note
        auto flat = voice (steady (220.0, 2.5));
        Params q; q.mode = Mode::mix; q.scale = scaleChromatic; q.retuneMs = 0; q.vibrato = 0.6f;
        const float sSyn = spread (run (flat, q).l, mSyn);
        std::snprintf (m, sizeof m, "VIBRATO 60 %% on a straight note -> %.0f cents p-p (expect ~60)", sSyn);
        CHECK (sSyn > 40.0f && sSyn < 80.0f, m);
    }

    std::printf ("[threshold, transpose, MIDI]\n");
    {
        auto v = voice (steady (226.0, 1.6));   // 46 cents above A3
        Params p; p.mode = Mode::mix; p.scale = scaleChromatic; p.retuneMs = 0; p.thresholdCents = 30.0f;
        float out = f0At (run (v, p).l, 40000, 8);
        char m[160]; std::snprintf (m, sizeof m, "threshold 30 cents leaves a 46-cent miss alone: %.2f Hz (in 226)", out);
        CHECK (std::abs (cents (out, 226.0f)) < 4.0f, m);
        auto v2 = voice (steady (224.0, 1.6));  // 31 cents above A3
        p.thresholdCents = 60.0f;
        out = f0At (run (v2, p).l, 40000, 8);
        std::snprintf (m, sizeof m, "threshold 60 cents corrects a 31-cent miss: %.2f Hz (220)", out);
        CHECK (std::abs (cents (out, 220.0f)) < 3.0f, m);

        Params t; t.mode = Mode::mix; t.scale = scaleChromatic; t.retuneMs = 0; t.transpose = 12;
        out = f0At (run (voice (steady (220.0, 1.6)), t).l, 40000, 8);
        std::snprintf (m, sizeof m, "transpose +12: 220 -> %.2f Hz (440)", out);
        CHECK (std::abs (cents (out, 440.0f)) < 3.0f, m);
        t.transpose = -5; t.mode = Mode::live;
        out = f0At (run (voice (steady (220.0, 1.6)), t).l, 40000, 8);
        std::snprintf (m, sizeof m, "LIVE transpose -5: 220 -> %.2f Hz (164.8)", out);
        CHECK (std::abs (cents (out, 164.81f)) < 3.0f, m);

        Params mi; mi.mode = Mode::mix; mi.key = 9; mi.scale = 1; mi.retuneMs = 0; mi.midiTarget = true;
        // singer at 226 Hz, MIDI holds C (any octave) -> nearest C is C4 = 261.6 Hz (or C3 130.8)
        out = f0At (run (v, mi, 256, nullptr, 1 << 0).l, 40000, 8);
        std::snprintf (m, sizeof m, "MIDI target C: 226 Hz -> %.2f Hz (C4 261.6)", out);
        CHECK (std::abs (cents (out, 261.63f)) < 3.0f, m);
    }

    std::printf ("[formants]\n");
    {
        auto v = voice (steady (146.83, 1.6));   // D3
        Params p; p.mode = Mode::mix; p.scale = scaleChromatic; p.retuneMs = 0;
        auto base = run (v, p).l;
        p.transpose = 7; auto up = run (v, p).l;
        p.preserveFormants = false; auto upRaw = run (v, p).l;
        p.transpose = 0; p.preserveFormants = true; p.throatSemis = 5; auto throatUp = run (v, p).l;
        p.throatSemis = -5; auto throatDn = run (v, p).l;
        const size_t at = 40000;
        const double b0 = brightness (base, at, 4096), bu = brightness (up, at, 4096), br = brightness (upRaw, at, 4096);
        const double bt = brightness (throatUp, at, 4096), bd = brightness (throatDn, at, 4096);
        char m[200];
        std::snprintf (m, sizeof m, "brightness base %.3f, +7 preserved %.3f, +7 unpreserved %.3f", b0, bu, br);
        CHECK (std::abs (bu - b0) < 0.5 * std::abs (br - b0) && br > b0 * 2.0, m);
        const float fu = f0At (up, at, 8), fr = f0At (upRaw, at, 8), ft = f0At (throatUp, at, 8);
        std::snprintf (m, sizeof m, "+7 pitch preserved %.2f / unpreserved %.2f Hz (220); throat keeps %.2f Hz (146.83)", fu, fr, ft);
        CHECK (std::abs (cents (fu, 220.0f)) < 3 && std::abs (cents (fr, 220.0f)) < 3 && std::abs (cents (ft, 146.83f)) < 3, m);
        std::snprintf (m, sizeof m, "throat -5: %.3f < base %.3f < throat +5: %.3f", bd, b0, bt);
        CHECK (bt > b0 * 1.2 && bd < b0 * 0.85, m);
    }

    std::printf ("[auto key]\n");
    {
        // D minor melody (tonic / dominant heavy), sung slightly sharp
        auto dm = voice (melody ({ { 62, 2 }, { 65, 1 }, { 69, 2 }, { 67, 1 }, { 65, 1 }, { 64, 1 }, { 62, 2 }, { 60, 1 }, { 62, 1 },
                                   { 69, 2 }, { 70, 1 }, { 69, 1 }, { 67, 1 }, { 65, 1 }, { 64, 1 }, { 62, 3 } }, 11.0));
        Engine e; Params p; p.mode = Mode::live; p.autoKey = true; p.retuneMs = 10;
        run (dm, p, 512, &e);
        char m[160]; std::snprintf (m, sizeof m, "D minor melody -> %s %s", e.uiDetectedKey.load() >= 0 ? keyNames[(size_t) e.uiDetectedKey.load()] : "?",
                                    e.uiDetectedMinor.load() ? "minor" : "major");
        CHECK (e.uiDetectedKey.load() == 2 && e.uiDetectedMinor.load(), m);
        // Bb major melody
        auto bb = voice (melody ({ { 58, 2 }, { 62, 1 }, { 65, 2 }, { 63, 1 }, { 62, 1 }, { 60, 1 }, { 58, 2 }, { 65, 1 }, { 67, 1 },
                                   { 65, 2 }, { 63, 1 }, { 62, 1 }, { 60, 1 }, { 57, 1 }, { 58, 3 } }, 11.0));
        Engine e2; run (bb, p, 512, &e2);
        std::snprintf (m, sizeof m, "Bb major melody -> %s %s", e2.uiDetectedKey.load() >= 0 ? keyNames[(size_t) e2.uiDetectedKey.load()] : "?",
                       e2.uiDetectedMinor.load() ? "minor" : "major");
        CHECK (e2.uiDetectedKey.load() == 10 && ! e2.uiDetectedMinor.load(), m);
        // and the tuner then corrects to the detected key: a C# (not in D minor) sung note goes to C or D
        std::vector<float> both = dm; auto cs = voice (steady (277.18 * std::pow (2.0, 30 / 1200.0), 1.5)); both.insert (both.end(), cs.begin(), cs.end());
        auto r = run (both, p, 512);
        const float out = f0At (r.l, both.size() - 30000, 6);
        std::snprintf (m, sizeof m, "after learning D minor, a sharp C# is tuned to %.2f Hz (C 261.6 / D 293.7)", out);
        CHECK (std::abs (cents (out, 261.63f)) < 5 || std::abs (cents (out, 293.66f)) < 5, m);
    }

    std::printf ("[silence, noise, mix]\n");
    for (Mode mode : { Mode::live, Mode::mix })
    {
        Params p; p.mode = mode; p.scale = scaleChromatic; p.retuneMs = 0; p.transpose = 0;
        std::vector<float> z ((size_t) SR, 0.0f);
        auto r = run (z, p);
        float pk = 0; for (float x : r.l) pk = std::max (pk, std::abs (x));
        CHECK (pk == 0.0f, mode == Mode::live ? "LIVE silence in -> exact silence out" : "MIX silence in -> exact silence out");
        std::mt19937 g (11); std::normal_distribution<float> nd (0, 0.2f);
        std::vector<float> nz ((size_t) (SR * 2)); for (auto& v : nz) v = nd (g);
        auto rn = run (nz, p);
        double err = 0, ref = 0;
        for (size_t i = 20000; i < nz.size(); ++i) { const double d = rn.l[i] - nz[i - (size_t) rn.latency]; err += d * d; ref += (double) nz[i] * nz[i]; }
        const double db = 10 * std::log10 (err / ref + 1e-30);
        char m[128]; std::snprintf (m, sizeof m, "%s noise passes clean (difference %.1f dB)", mode == Mode::live ? "LIVE" : "MIX", db);
        CHECK (db < -40.0, m);
        // mix = 0 is the bit-exact delayed dry
        auto v = voice (steady (226.0, 1.0));
        Params q = p; q.mix = 0.0f; q.transpose = 5;
        auto rm = run (v, q);
        bool exact = true; for (size_t i = (size_t) rm.latency; i < v.size(); ++i) if (rm.l[i] != v[i - (size_t) rm.latency]) { exact = false; break; }
        CHECK (exact, "mix 0 % = bit-exact dry (latency compensated)");
    }

    std::printf ("[mode switch]\n");
    {
        Engine e; Params p; p.mode = Mode::live; e.setParams (p); e.prepare (SR, 512);
        const int l0 = e.getLatencySamples();
        auto v = voice (steady (200.0, 1.0));
        std::vector<float> l = v, r = v;
        float maxJump = 0, prev = 0;
        for (size_t pos = 0; pos < v.size(); pos += 512)
        {
            if (pos >= 20000) { p.mode = Mode::mix; e.setParams (p); }
            const int n = (int) std::min<size_t> (512, v.size() - pos);
            float* ch[2] = { l.data() + pos, r.data() + pos }; e.process (ch, 2, n);
        }
        for (size_t i = 1; i < l.size(); ++i) { maxJump = std::max (maxJump, std::abs (l[i] - prev)); prev = l[i]; }
        char m[128]; std::snprintf (m, sizeof m, "LIVE -> MIX: latency %d -> %d samples, max step %.3f (no click)", l0, e.getLatencySamples(), maxJump);
        CHECK (e.getLatencySamples() == Engine::latencyFor (Mode::mix, SR) && maxJump < 0.2f, m);
    }

    std::printf ("[60 s stress, random settings]\n");
    for (double rate : { 44100.0, 96000.0, 192000.0 })
    {
        SR = rate;
        std::mt19937 g (7); std::uniform_real_distribution<float> u (0, 1);
        Engine e; Params p0; e.setParams (p0); e.prepare (rate, 4096);
        const double secs = rate == 44100.0 ? 60.0 : 10.0;
        const size_t total = (size_t) (rate * secs);
        std::vector<float> l (4096), r (4096);
        bool ok = true; float pk = 0; size_t done = 0; double ph = 0;
        const auto t0 = std::chrono::steady_clock::now();
        while (done < total)
        {
            if (u (g) < 0.3f)
            {
                Params p; p.mode = u (g) < 0.5f ? Mode::live : Mode::mix; p.key = (int) (u (g) * 12) % 12; p.scale = (int) (u (g) * numScales) % numScales;
                p.customMask = (int) (u (g) * 4096); p.retuneMs = u (g) < 0.3f ? 0 : u (g) * 400; p.humanize = u (g); p.thresholdCents = 5 + u (g) * 95;
                p.vibrato = u (g); p.vibratoRateHz = 3 + u (g) * 5; p.transpose = std::round ((u (g) - 0.5f) * 24); p.throatSemis = (u (g) - 0.5f) * 24;
                p.preserveFormants = u (g) < 0.7f; p.midiTarget = u (g) < 0.3f; p.autoKey = u (g) < 0.3f; p.mix = u (g); p.outputDb = -24 + u (g) * 36;
                e.setParams (p); e.setMidiMask (u (g) < 0.5f ? (int) (u (g) * 4096) : 0);
            }
            const int n = (int) std::min<size_t> (1 + (size_t) (u (g) * 4095), total - done);
            const int kind = (int) (done / (size_t) (rate * 3)) % 4;   // voice / noise / silence / loud square
            for (int i = 0; i < n; ++i)
            {
                const double f = 90 + 500 * (0.5 + 0.5 * std::sin (2 * PI * 0.1 * (double) (done + (size_t) i) / rate));
                ph += f / rate; if (ph > 1) ph -= 1;
                float x = kind == 0 ? (float) (0.5 * std::sin (2 * PI * ph) + 0.2 * std::sin (6 * PI * ph))
                        : kind == 1 ? (u (g) - 0.5f) : kind == 2 ? 0.0f : (ph < 0.5 ? 1.0f : -1.0f);
                l[(size_t) i] = x; r[(size_t) i] = kind == 1 ? (u (g) - 0.5f) : x;
            }
            float* ch[2] = { l.data(), r.data() };
            e.process (ch, u (g) < 0.1f ? 1 : 2, n);
            for (int i = 0; i < n; ++i) { if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) ok = false; pk = std::max (pk, std::abs (l[(size_t) i])); }
            done += (size_t) n;
        }
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        char m[160]; std::snprintf (m, sizeof m, "%.0f Hz, %.0f s audio: finite, peak %.2f, %.1fx real time", rate, secs, pk, secs / el);
        CHECK (ok && pk < 8.0f && secs / el > 5.0, m);
    }
    SR = 48000.0;

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
