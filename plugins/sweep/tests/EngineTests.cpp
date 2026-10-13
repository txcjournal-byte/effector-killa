// Headless tests for the TrapWin SWEEP engine.
#include "../Source/DSP/SweepEngine.h"
#include "../Source/Presets.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace sweepfx;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct Stereo { std::vector<float> l, r; };

static Stereo sine (double sr, double seconds, double freq, float amp)
{
    const size_t n = (size_t) (sr * seconds);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i) s.l[i] = s.r[i] = amp * (float) std::sin (2.0 * PI * freq * (double) i / sr);
    return s;
}

static Stereo noise (double sr, double seconds, float amp, unsigned seed = 1)
{
    const size_t n = (size_t) (sr * seconds);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    for (size_t i = 0; i < n; ++i) { s.l[i] = amp * u (rng); s.r[i] = amp * u (rng); }
    return s;
}

static Stereo drums (double sr, double seconds, double period, float amp = 0.7f)
{
    const size_t n = (size_t) (sr * seconds);
    Stereo s { std::vector<float> (n), std::vector<float> (n) };
    for (size_t i = 0; i < n; ++i)
    {
        const double t = std::fmod ((double) i / sr, period);
        const double env = std::min (1.0, t / 0.001) * std::exp (-t / 0.06);
        s.l[i] = s.r[i] = amp * (float) (env * std::sin (2.0 * PI * 90.0 * t));
    }
    return s;
}

static void run (Engine& e, Stereo& x, int block = 512, int numCh = 2, HostTime t = {})
{
    for (size_t pos = 0; pos < x.l.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, x.l.size() - pos);
        float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
        e.process (ch, numCh, n, t);
        if (t.playing) t.ppq += (double) n / 48000.0 * t.bpm / 60.0;
    }
}

static bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

static float peak (const Stereo& s, size_t from = 0)
{
    float m = 0; for (size_t i = from; i < s.l.size(); ++i) m = std::max (m, std::max (std::abs (s.l[i]), std::abs (s.r[i]))); return m;
}

static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i]; return std::sqrt (s / (double) std::max<size_t> (1, b - a));
}

// records the LFO output sample by sample (1-sample blocks)
static std::vector<float> recordLfo (Params p, double sr, double seconds, double bpm, std::vector<float>* cutoffs = nullptr)
{
    Engine e; e.prepare (sr, 1); e.setParams (p);
    HostTime t; t.bpm = bpm;
    std::vector<float> out;
    float l = 0, r = 0;
    for (size_t i = 0; i < (size_t) (sr * seconds); ++i)
    {
        float* ch[2] = { &l, &r };
        l = r = 0.0f;
        e.process (ch, 2, 1, t);
        out.push_back (e.lfoOut.load());
        if (cutoffs) cutoffs->push_back (e.cutoffA.load());
    }
    return out;
}

// mean period (seconds) between rising crossings of `level`
static double meanPeriod (const std::vector<float>& v, double sr, float level)
{
    std::vector<double> cross;
    for (size_t i = 1; i < v.size(); ++i)
        if (v[i - 1] < level && v[i] >= level)
            cross.push_back ((double) (i - 1) + (level - v[i - 1]) / (v[i] - v[i - 1]));
    if (cross.size() < 3) return -1.0;
    return (cross.back() - cross[1]) / (double) (cross.size() - 2) / sr;
}

int main()
{
    char msg[256];

    // ---------------- LFO timing ----------------
    std::printf ("-- LFO period vs tempo\n");
    for (double bpm : { 140.0, 87.0, 174.0 })
        for (int div : { 2, 5, 8, 9, 12 })
        {
            Params p; p.sync = true; p.division = div; p.shape = Shape::sine;
            const double expect = divisionBeats[div] * 60.0 / bpm;
            const double secs = std::max (1.0, expect * 4.5);
            const auto v = recordLfo (p, 48000.0, secs, bpm);
            const double got = meanPeriod (v, 48000.0, 0.5f);
            std::snprintf (msg, sizeof msg, "%.0f BPM, %s: period %.4f s (expected %.4f s)", bpm, divisionNames[div], got, expect);
            CHECK (std::abs (got - expect) < expect * 0.002, msg);
        }
    {
        Params p; p.sync = false; p.rateHz = 3.0f; p.shape = Shape::triangle;
        const auto v = recordLfo (p, 44100.0, 2.0, 140.0);
        const double got = meanPeriod (v, 44100.0, 0.5f);
        std::snprintf (msg, sizeof msg, "free 3 Hz triangle: period %.4f s", got);
        CHECK (std::abs (got - 1.0 / 3.0) < 0.001, msg);
    }
    {
        // steps: a 1/16 step pattern spans one bar; one high step per cycle -> period = 1 bar
        Params p; p.sync = true; p.division = 2; p.shape = Shape::pattern;
        p.pattern.fill (0.0f); p.pattern[4] = 1.0f;
        const auto v = recordLfo (p, 48000.0, 6.0, 140.0);
        const double got = meanPeriod (v, 48000.0, 0.5f);
        std::snprintf (msg, sizeof msg, "16-step pattern @ 1/16, 140 BPM: cycle %.4f s (1 bar = %.4f s)", got, 4 * 60.0 / 140.0);
        CHECK (std::abs (got - 4 * 60.0 / 140.0) < 0.002, msg);
        // step duration: the high step lasts one 1/16
        size_t a = 0, b = 0;
        for (size_t i = 1; i < v.size(); ++i) { if (! a && v[i - 1] < 0.5f && v[i] >= 0.5f) a = i; else if (a && ! b && v[i - 1] >= 0.5f && v[i] < 0.5f) b = i; }
        const double stepS = (double) (b - a) / 48000.0;
        std::snprintf (msg, sizeof msg, "pattern step length %.4f s (1/16 = %.4f s)", stepS, 0.25 * 60.0 / 140.0);
        CHECK (std::abs (stepS - 0.25 * 60.0 / 140.0) < 0.0015, msg);
    }
    {
        // host lock: phase follows the song position
        Params p; p.sync = true; p.division = 8; p.shape = Shape::sawUp;
        Engine e; e.prepare (48000.0, 512); e.setParams (p);
        HostTime t; t.playing = true; t.hasPpq = true; t.bpm = 120.0; t.ppq = 10.25;
        float l[512] = {}, r[512] = {};
        float* ch[2] = { l, r };
        bool ok = true;
        for (int b = 0; b < 40; ++b)
        {
            e.process (ch, 2, 512, t);
            const double expectPhase = std::fmod (t.ppq + 512.0 / 48000.0 * 2.0, 1.0);   // 1/4 cycle at 120 BPM = 1 beat
            if (std::abs (e.lfoPhase.load() - expectPhase) > 1.0e-3 && std::abs (std::abs (e.lfoPhase.load() - expectPhase) - 1.0) > 1.0e-3) ok = false;
            t.ppq += 512.0 / 48000.0 * 2.0;
        }
        CHECK (ok, "host transport: LFO phase locked to the song position (ppq)");
    }

    // ---------------- modulation range ----------------
    std::printf ("-- modulation range\n");
    {
        Params p; p.a.cutoff = 200.0f; p.lfoDepth = 0.5f; p.sync = false; p.rateHz = 4.0f; p.shape = Shape::sine;
        std::vector<float> cut;
        recordLfo (p, 48000.0, 1.5, 140.0, &cut);
        float lo = 1e9f, hi = 0.0f;
        for (size_t i = 24000; i < cut.size(); ++i) { lo = std::min (lo, cut[i]); hi = std::max (hi, cut[i]); }
        const float expectHi = 200.0f * std::pow (2.0f, 0.5f * maxModOctaves);
        std::snprintf (msg, sizeof msg, "LFO depth +50 %% from 200 Hz: sweeps %.1f .. %.1f Hz (expected 200 .. %.0f = +%.0f oct)", lo, hi, expectHi, 0.5f * maxModOctaves);
        CHECK (std::abs (lo - 200.0f) < 2.0f && std::abs (hi / expectHi - 1.0f) < 0.01f, msg);

        p.a.cutoff = 4000.0f; p.lfoDepth = -0.5f;
        cut.clear();
        recordLfo (p, 48000.0, 1.5, 140.0, &cut);
        lo = 1e9f; hi = 0.0f;
        for (size_t i = 24000; i < cut.size(); ++i) { lo = std::min (lo, cut[i]); hi = std::max (hi, cut[i]); }
        std::snprintf (msg, sizeof msg, "LFO depth -50 %% from 4 kHz: sweeps %.0f .. %.0f Hz (expected 500 .. 4000)", lo, hi);
        CHECK (std::abs (hi - 4000.0f) < 40.0f && std::abs (lo / 500.0f - 1.0f) < 0.01f, msg);

        p.a.cutoff = 500.0f; p.lfoDepth = 1.0f; p.a.mod = 0.25f;
        cut.clear();
        recordLfo (p, 48000.0, 1.5, 140.0, &cut);
        hi = 0.0f;
        for (size_t i = 24000; i < cut.size(); ++i) hi = std::max (hi, cut[i]);
        std::snprintf (msg, sizeof msg, "filter MOD 25 %% scales the sweep: max %.0f Hz (expected %.0f)", hi, 500.0f * std::pow (2.0f, 0.25f * maxModOctaves));
        CHECK (std::abs (hi / (500.0f * std::pow (2.0f, 0.25f * maxModOctaves)) - 1.0f) < 0.01f, msg);

        p.a.cutoff = 15000.0f; p.lfoDepth = 1.0f; p.a.mod = 1.0f;
        cut.clear();
        recordLfo (p, 48000.0, 1.0, 140.0, &cut);
        hi = 0.0f;
        for (float c : cut) hi = std::max (hi, c);
        std::snprintf (msg, sizeof msg, "cutoff clamped below Nyquist (max %.0f Hz at 48 kHz)", hi);
        CHECK (hi <= 48000.0f * 0.45f + 1.0f, msg);
    }
    {
        // ADSR via MIDI: peak = base * 2^(depth * maxOct), back to base after release
        Params p; p.a.cutoff = 300.0f; p.lfoDepth = 0.0f; p.adsrDepth = 0.5f; p.attackMs = 10.0f; p.decayMs = 100.0f; p.sustain = 0.5f; p.releaseMs = 50.0f;
        Engine e; e.prepare (48000.0, 4); e.setParams (p);
        float l[4] = {}, r[4] = {};
        float* ch[2] = { l, r };
        HostTime t;
        e.noteOn();
        float hi = 0.0f;
        for (int b = 0; b < 12000; ++b) { e.process (ch, 2, 4, t); hi = std::max (hi, e.cutoffA.load()); }
        const float sustained = e.cutoffA.load();
        e.noteOff();
        for (int b = 0; b < 12000; ++b) e.process (ch, 2, 4, t);
        const float after = e.cutoffA.load();
        std::snprintf (msg, sizeof msg, "ADSR (MIDI): peak %.0f Hz (exp. %.0f), sustain %.0f Hz (exp. %.0f), released %.1f Hz",
                       hi, 300.0f * std::pow (2.0f, 3.0f), sustained, 300.0f * std::pow (2.0f, 1.5f), after);
        CHECK (std::abs (hi / 2400.0f - 1.0f) < 0.02f && std::abs (sustained / (300.0f * std::pow (2.0f, 1.5f)) - 1.0f) < 0.02f && std::abs (after - 300.0f) < 1.0f, msg);
    }
    {
        // transients trigger the ADSR once per hit
        Params p; p.trigger = Trigger::transient; p.sensitivity = 0.5f; p.adsrDepth = 0.5f; p.lfoDepth = 0.0f;
        Engine e; e.prepare (48000.0, 256); e.setParams (p);
        auto x = drums (48000.0, 4.0, 0.25);
        run (e, x, 256);
        std::snprintf (msg, sizeof msg, "transient trigger: %d triggers for 16 hits", e.triggerCount.load());
        CHECK (e.triggerCount.load() == 16, msg);
        Params q = p; q.trigger = Trigger::midi;
        Engine e2; e2.prepare (48000.0, 256); e2.setParams (q);
        auto x2 = drums (48000.0, 2.0, 0.25);
        run (e2, x2, 256);
        CHECK (e2.triggerCount.load() == 0, "MIDI trigger mode ignores transients");
    }
    {
        // envelope follower: loud = up, silence = base
        Params p; p.a.cutoff = 400.0f; p.lfoDepth = 0.0f; p.envDepth = 0.5f; p.envSpeed = 0.8f;
        Engine e; e.prepare (48000.0, 512); e.setParams (p);
        auto x = sine (48000.0, 1.0, 220.0, 0.5f);
        run (e, x);
        const float loud = e.cutoffA.load();
        Stereo sil { std::vector<float> (48000), std::vector<float> (48000) };
        run (e, sil);
        std::snprintf (msg, sizeof msg, "envelope follower: -6 dBFS opens to %.0f Hz, silence returns to %.0f Hz", loud, e.cutoffA.load());
        CHECK (loud > 400.0f * 6.0f && std::abs (e.cutoffA.load() - 400.0f) < 1.0f, msg);
    }

    // ---------------- filters vs display response ----------------
    std::printf ("-- filter responses (measured vs display)\n");
    {
        const double sr = 48000.0;
        struct Case { FilterType t; float fc, reso; float f; };
        const Case cases[] = { { FilterType::lp12, 1000, 0.2f, 250 }, { FilterType::lp12, 1000, 0.2f, 4000 }, { FilterType::lp24, 1000, 0.5f, 4000 },
                               { FilterType::hp12, 1000, 0.2f, 250 }, { FilterType::hp24, 1000, 0.6f, 300 }, { FilterType::bp, 1000, 0.7f, 1000 },
                               { FilterType::bp, 1000, 0.7f, 3000 }, { FilterType::notch, 1000, 0.3f, 900 }, { FilterType::lp12, 1000, 0.9f, 1000 },
                               { FilterType::comb, 500, 0.6f, 1000 }, { FilterType::comb, 500, 0.6f, 750 }, { FilterType::formant, 1200, 0.5f, 800 } };
        for (const auto& cs : cases)
        {
            Params p; p.a.type = cs.t; p.a.cutoff = cs.fc; p.a.reso = cs.reso; p.lfoDepth = 0.0f;
            Engine e; e.prepare (sr, 512); e.setParams (p);
            auto x = sine (sr, 1.0, cs.f, 0.1f);
            auto in = x;
            run (e, x);
            const double measured = 20.0 * std::log10 (rms (x.l, 24000, 48000) / rms (in.l, 24000, 48000));
            const double shown = magnitudeDb (p.a, cs.fc, cs.f, sr);
            std::snprintf (msg, sizeof msg, "%-7s fc %.0f reso %.1f @ %.0f Hz: measured %+6.1f dB, display %+6.1f dB", filterTypeNames[(int) cs.t], cs.fc, cs.reso, cs.f, measured, shown);
            CHECK (std::abs (measured - shown) < 1.0, msg);
        }
        Params p; p.a.cutoff = 200.0f; p.lfoDepth = 0.0f;
        Engine e; e.prepare (sr, 512); e.setParams (p);
        auto hiS = sine (sr, 0.5, 5000.0, 0.5f);
        run (e, hiS);
        std::snprintf (msg, sizeof msg, "LP 12 @ 200 Hz attenuates 5 kHz by %.1f dB", 20.0 * std::log10 (rms (hiS.l, 12000, 24000) / (0.5 / std::sqrt (2.0))));
        CHECK (rms (hiS.l, 12000, 24000) < 0.5 / std::sqrt (2.0) * 0.01, msg);
    }

    // ---------------- self-oscillation guard ----------------
    std::printf ("-- resonance / self-oscillation guard\n");
    for (int t = 0; t < numFilterTypes; ++t)
    {
        Params p; p.a.type = (FilterType) t; p.a.reso = 1.0f; p.a.drive = 1.0f; p.a.cutoff = 800.0f;
        p.b.on = true; p.b.type = (FilterType) ((t + 3) % numFilterTypes); p.b.reso = 1.0f; p.b.drive = 1.0f; p.b.cutoff = 2000.0f;
        p.lfoDepth = 0.6f; p.sync = false; p.rateHz = 9.0f; p.shape = Shape::sawUp;
        Engine e; e.prepare (48000.0, 512); e.setParams (p);
        // loud noise burst, then an impulse, then 3 s of silence
        auto x = noise (48000.0, 1.0, 4.0f, (unsigned) t + 3);
        Stereo tail { std::vector<float> (48000 * 3), std::vector<float> (48000 * 3) };
        tail.l[10] = tail.r[10] = 1.0f;
        x.l.insert (x.l.end(), tail.l.begin(), tail.l.end());
        x.r.insert (x.r.end(), tail.r.begin(), tail.r.end());
        run (e, x);
        const float pk = peak (x);
        const double end = rms (x.l, x.l.size() - 24000, x.l.size());
        std::snprintf (msg, sizeof msg, "%-7s reso 100 %% + drive 100 %%: peak %.2f (bounded), tail after 3 s %.1f dB (decays, no runaway)",
                       filterTypeNames[t], pk, 20.0 * std::log10 (end + 1e-12));
        CHECK (allFinite (x) && pk <= 4.01f && end < 1.0e-5, msg);
    }
    {
        // a max-resonance LP swept over the full range with a quiet input never runs away
        Params p; p.a.type = FilterType::lp12; p.a.reso = 1.0f; p.a.cutoff = 20.0f; p.lfoDepth = 1.0f; p.sync = false; p.rateHz = 20.0f; p.shape = Shape::square;
        Engine e; e.prepare (96000.0, 64); e.setParams (p);
        auto x = noise (96000.0, 5.0, 0.3f, 99);
        run (e, x, 64);
        std::snprintf (msg, sizeof msg, "reso 100 %% LP hammered by a 20 Hz square sweep (96 kHz): peak %.2f", peak (x));
        CHECK (allFinite (x) && peak (x) < 4.01f, msg);
    }

    // ---------------- mix / output / clicks ----------------
    std::printf ("-- mix, output, clicks\n");
    {
        Params p; p.a.type = FilterType::comb; p.a.reso = 0.9f; p.lfoDepth = 1.0f; p.mix = 0.0f; p.b.on = true;
        Engine e; e.prepare (48000.0, 333); e.setParams (p);
        auto x = noise (48000.0, 1.0, 0.5f);
        auto y = x;
        run (e, y, 333);
        bool exact = true;
        for (size_t i = 0; i < x.l.size(); ++i) if (x.l[i] != y.l[i] || x.r[i] != y.r[i]) { exact = false; break; }
        CHECK (exact, "mix 0 % = bit-exact dry while modulating");

        p.mix = 0.0f; p.outputDb = -6.0f;
        Engine e2; e2.prepare (48000.0, 512); e2.setParams (p);
        auto s = sine (48000.0, 0.5, 300.0, 0.5f);
        run (e2, s);
        CHECK (std::abs (peak (s, 12000) / 0.5f - 0.501f) < 0.01f, "output -6 dB scales the signal");
    }
    {
        // filter type change mid-signal: no click (largest sample step stays near the steady-state maximum)
        const double sr = 48000.0;
        Params p; p.a.type = FilterType::lp12; p.a.cutoff = 3000.0f; p.lfoDepth = 0.0f;
        Engine e; e.prepare (sr, 256); e.setParams (p);
        auto x = sine (sr, 256.0 * 187.0 / sr, 110.0, 0.5f);
        float maxStep = 0.0f, steady = 0.0f;
        float prev = 0.0f;
        for (size_t pos = 0; pos < x.l.size(); pos += 256)
        {
            if (pos == 24064) { p.a.type = FilterType::comb; e.setParams (p); }
            float* ch[2] = { x.l.data() + pos, x.r.data() + pos };
            e.process (ch, 2, 256, {});
            for (size_t i = pos; i < pos + 256; ++i)
            {
                const float d = std::abs (x.l[i] - prev);
                prev = x.l[i];
                if (i > 4800 && i < 24064) steady = std::max (steady, d);
                if (i >= 24064 && i < 24064 + 4800) maxStep = std::max (maxStep, d);
            }
        }
        std::snprintf (msg, sizeof msg, "type change LP -> COMB: max step %.4f vs steady %.4f", maxStep, steady);
        CHECK (maxStep < steady * 2.0f, msg);
    }

    // ---------------- robustness ----------------
    std::printf ("-- robustness\n");
    {
        std::mt19937 rng (5);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        bool finite = true; float maxPeak = 0.0f;
        for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            Engine e; e.prepare (sr, 4096);
            bool prevHuge = false;
            HostTime t; t.bpm = 140.0;
            for (int blk = 0; blk < 300; ++blk)
            {
                Params p;
                for (auto* f : { &p.a, &p.b })
                {
                    f->on = u (rng) > 0.4f; f->type = (FilterType) (int) (u (rng) * 7.99f); f->cutoff = 20.0f + u (rng) * 19980.0f;
                    f->reso = u (rng); f->drive = u (rng); f->mod = u (rng) * 2.0f - 1.0f;
                }
                p.a.on = true;
                p.routing = (Routing) (int) (u (rng) * 1.99f);
                p.sync = u (rng) > 0.5f; p.division = (int) (u (rng) * 15.99f); p.rateHz = 0.05f + u (rng) * 30.0f;
                p.shape = (Shape) (int) (u (rng) * 6.99f); p.lfoDepth = u (rng) * 2 - 1; p.stereo = u (rng); p.glide = u (rng); p.retrig = u (rng) > 0.5f;
                for (auto& s : p.pattern) s = u (rng);
                p.envDepth = u (rng) * 2 - 1; p.envSpeed = u (rng); p.trigger = (Trigger) (int) (u (rng) * 1.99f); p.sensitivity = u (rng);
                p.attackMs = u (rng) * 2000.0f; p.decayMs = u (rng) * 2000.0f; p.sustain = u (rng); p.releaseMs = u (rng) * 2000.0f; p.adsrDepth = u (rng) * 2 - 1;
                p.mix = u (rng); p.outputDb = -24.0f + 36.0f * u (rng);
                if (blk % 29 == 0) { p.a.cutoff = NAN; p.lfoDepth = INFINITY; p.rateHz = NAN; p.pattern[3] = NAN; }
                e.setParams (p);
                t.playing = u (rng) > 0.5f; t.hasPpq = t.playing; t.ppq += 3.7; t.bpm = blk % 31 == 0 ? NAN : 60.0 + 140.0 * u (rng);
                if (blk % 7 == 0) e.noteOn();
                if (blk % 11 == 0) e.noteOff();
                const int n = 1 + (int) (u (rng) * 4095.0f);
                std::vector<float> l ((size_t) n), r ((size_t) n);
                const int kind = blk % 5;
                for (int i = 0; i < n; ++i)
                {
                    float v = 0.0f;
                    if (kind == 0) v = (u (rng) * 2.0f - 1.0f) * 1000.0f;
                    else if (kind == 1) v = 0.0f;
                    else if (kind == 2) v = 1.0e-38f * (u (rng) - 0.5f);
                    else if (kind == 3) v = (i % 300 == 0) ? 1.0f : 0.0f;
                    else v = u (rng) * 2.0f - 1.0f;
                    l[(size_t) i] = v; r[(size_t) i] = v * 0.7f;
                }
                if (blk % 43 == 0) l[0] = NAN;
                float* ch[2] = { l.data(), r.data() };
                e.process (ch, (blk % 3 == 0) ? 1 : 2, n, t);
                for (int i = 0; i < n; ++i)
                {
                    if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                    if (kind != 0) maxPeak = std::max (maxPeak, std::abs (l[(size_t) i]));
                }
                prevHuge = kind == 0;
                (void) prevHuge;
            }
        }
        CHECK (finite, "fuzzing (random params incl. NaN/inf, NaN tempo, huge / denormal / silent / NaN input, 1..4096 blocks, mono+stereo): no NaN/inf");
        std::snprintf (msg, sizeof msg, "fuzzing: bounded output for unit-level input (peak %.1f <= safety 4 x +12 dB)", maxPeak);
        CHECK (maxPeak <= 4.0f * 4.0f + 0.01f, msg);
    }
    {
        const double sr = 48000.0;
        Params p; p.a.type = FilterType::lp24; p.a.reso = 1.0f; p.a.drive = 1.0f; p.b.on = true; p.b.type = FilterType::formant; p.b.reso = 1.0f;
        p.routing = Routing::parallel; p.lfoDepth = 1.0f; p.envDepth = -1.0f; p.adsrDepth = 1.0f; p.trigger = Trigger::transient; p.sensitivity = 1.0f;
        p.sync = true; p.division = 0; p.shape = Shape::sampleHold; p.stereo = 1.0f; p.outputDb = 12.0f;
        Engine e; e.prepare (sr, 512); e.setParams (p);
        auto x = noise (sr, 60.0, 1.0f, 77);
        for (size_t i = 0; i < x.l.size(); i += 6000) x.l[i] = x.r[i] = 1.0f;
        const auto t0 = std::chrono::steady_clock::now();
        HostTime t; t.playing = true; t.hasPpq = true; t.bpm = 140.0;
        run (e, x, 512, 2, t);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::snprintf (msg, sizeof msg, "60 s stress (both filters max reso, drive, all mods, S&H 1/32, +12 dB): finite, peak %.2f, %.0fx realtime", peak (x), 60.0 / secs);
        CHECK (allFinite (x) && peak (x) <= 16.01f, msg);
    }

    // ---------------- factory presets ----------------
    std::printf ("-- factory presets\n");
    {
        const auto& list = factoryPresets();
        bool ok = true; int idx = 0;
        for (const auto& pr : list)
        {
            Engine e; e.prepare (48000.0, 512); e.setParams (pr.params);
            HostTime t; t.playing = true; t.hasPpq = true; t.bpm = 140.0;
            // saw bass + drums
            auto x = drums (48000.0, 3.0, 0.43, 0.6f);
            for (size_t i = 0; i < x.l.size(); ++i) { const float s = 0.3f * (float) (2.0 * std::fmod ((double) i * 55.0 / 48000.0, 1.0) - 1.0); x.l[i] += s; x.r[i] += s; }
            const double inR = rms (x.l, 0, x.l.size());
            e.noteOn();
            run (e, x, 512, 2, t);
            const double outR = rms (x.l, 0, x.l.size());
            const double db = 20.0 * std::log10 (outR / inR + 1e-12);
            if (! allFinite (x) || peak (x) > 4.01f || db < -40.0 || db > 12.0)
            {
                ok = false;
                std::printf ("    preset %d '%s': level %+.1f dB, peak %.2f\n", idx, pr.name, db, peak (x));
            }
            ++idx;
        }
        std::snprintf (msg, sizeof msg, "%d presets render finite, bounded and audible", (int) list.size());
        CHECK (ok && list.size() >= 30, msg);
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
