// Headless tests + demo renderer for the PUMP engine.
#include "../Source/DSP/PumpEngine.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace pump;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)
static constexpr double PI = 3.14159265358979323846;

struct St { std::vector<float> l, r; explicit St (size_t n = 0) : l (n), r (n) {} size_t size() const { return l.size(); } };

static St sine (double sr, double sec, double f, float a)
{
    St s ((size_t) (sr * sec));
    for (size_t i = 0; i < s.size(); ++i) s.l[i] = s.r[i] = a * (float) std::sin (2 * PI * f * (double) i / sr);
    return s;
}

static double rms (const std::vector<float>& v, size_t a, size_t b)
{
    double acc = 0; for (size_t i = a; i < b; ++i) acc += (double) v[i] * v[i];
    return std::sqrt (acc / (double) (b - a));
}

static void run (Engine& e, St& s, int block, HostTime t, double sr, const St* sc = nullptr)
{
    for (size_t pos = 0; pos < s.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, s.size() - pos);
        float* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        const float* scp[2] = { sc ? sc->l.data() + pos : nullptr, sc ? sc->r.data() + pos : nullptr };
        e.process (ch, 2, sc ? scp : nullptr, sc ? 2 : 0, n, t);
        if (t.playing) t.ppq += n * t.bpm / 60.0 / sr;
    }
}

static void testCurve()
{
    std::printf ("[curve]\n");
    Curve c = Curve::preset (0); // Classic Duck: 0 -> 1 at 0.5, hold, wrap to 0
    CHECK (std::abs (c.evaluate (0.0f)) < 1e-6f, "starts at 0");
    CHECK (std::abs (c.evaluate (0.5f) - 1.0f) < 1e-4f, "reaches 1 at 0.5");
    CHECK (c.evaluate (0.25f) > 0.5f, "tension -0.55 recovers fast (value at 0.25 > 0.5)");
    CHECK (std::abs (c.evaluate (0.999f) - 1.0f) < 1e-6f, "holds full level until the next cycle");
    bool mono = true; float prev = -1;
    for (int i = 0; i <= 500; ++i) { float v = c.evaluate (i / 1000.0f); if (v + 1e-6f < prev) mono = false; prev = v; }
    CHECK (mono, "duck segment is monotonic");
    for (int p = 0; p < Curve::numPresets(); ++p)
    {
        Curve q = Curve::preset (p); bool ok = true;
        for (int i = 0; i < 1000; ++i) { float v = q.evaluate (i / 1000.0f); if (! (v >= 0 && v <= 1)) ok = false; }
        char m[96]; std::snprintf (m, sizeof m, "preset '%s' stays in 0..1", Curve::presetName (p)); CHECK (ok, m);
    }
}

static void testSync()
{
    std::printf ("[sync to host]\n");
    const double sr = 48000;
    Engine e; e.prepare (sr, 512);
    Params p; p.rate = 3; p.depth = 1.0f; p.smoothMs = 0.5f; e.setParams (p); e.setCurve (Curve::preset (0));
    St s = sine (sr, 2.0, 1000.0, 0.5f);
    HostTime t { true, 0.0, 120.0 };
    run (e, s, 256, t, sr);
    // at 120 bpm a quarter = 24000 samples. Right after each beat the gain is ~0, at mid-beat ~1.
    const size_t beat = 24000;
    const double atBeat = rms (s.l, beat + 80, beat + 400);
    const double mid = rms (s.l, beat + 13000, beat + 14000);
    char m[128]; std::snprintf (m, sizeof m, "level right after the beat %.3f, mid-beat %.3f", atBeat, mid);
    CHECK (atBeat < 0.05 && mid > 0.3, m);
}

static void testDepthZeroTransparent()
{
    std::printf ("[depth 0 = transparent, also with band split]\n");
    const double sr = 44100;
    for (int band = 0; band < 3; ++band)
    {
        Engine e; e.prepare (sr, 512);
        Params p; p.depth = 0.0f; p.band = (Band) band; p.crossoverHz = 120.0f; e.setParams (p);
        double worst = 0;
        for (double f : { 40.0, 120.0, 1000.0, 8000.0 })
        {
            e.reset();
            St s = sine (sr, 1.0, f, 0.5f), in = s;
            run (e, s, 333, HostTime { false, 0, 120 }, sr);
            const double g = 20 * std::log10 (rms (s.l, s.size() / 2, s.size()) / rms (in.l, in.size() / 2, in.size()));
            worst = std::max (worst, std::abs (g));
        }
        char m[96]; std::snprintf (m, sizeof m, "band %d: worst level change %.3f dB", band, worst);
        CHECK (worst < 0.1, m);
    }
}

static void testLowBandOnly()
{
    std::printf ("[LOW band only ducks the low end]\n");
    const double sr = 48000;
    auto measure = [&] (double f)
    {
        Engine e; e.prepare (sr, 512);
        Params p; p.depth = 1.0f; p.band = Band::low; p.crossoverHz = 120.0f; p.rate = 3; e.setParams (p);
        e.setCurve (Curve::preset (0));
        St s = sine (sr, 2.0, f, 0.5f);
        run (e, s, 512, HostTime { true, 0.0, 120.0 }, sr);
        return rms (s.l, 24000 + 100, 24000 + 2000); // just after a beat
    };
    const double low = measure (40.0), high = measure (3000.0);
    char m[96]; std::snprintf (m, sizeof m, "after beat: 40 Hz rms %.3f, 3 kHz rms %.3f", low, high);
    CHECK (low < 0.1 && high > 0.3, m);
}

static void testMidiTrigger()
{
    std::printf ("[MIDI retrigger]\n");
    const double sr = 48000;
    Engine e; e.prepare (sr, 480);
    Params p; p.trigger = Trigger::midi; p.depth = 1.0f; p.smoothMs = 0.5f; p.rate = 3; e.setParams (p); e.setCurve (Curve::preset (0));
    St s = sine (sr, 1.0, 1000.0, 0.5f);
    const int block = 480;
    for (size_t pos = 0; pos < s.size(); pos += block)
    {
        if (pos == 9600) e.addNoteOn (100); // note at sample 9700
        float* ch[2] = { s.l.data() + pos, s.r.data() + pos };
        e.process (ch, 2, nullptr, 0, block, HostTime { false, 0, 120 });
    }
    const double before = rms (s.l, 8000, 9600), after = rms (s.l, 9750, 10000), later = rms (s.l, 40000, 44000);
    char m[128]; std::snprintf (m, sizeof m, "before %.3f, right after note %.3f, after recovery %.3f", before, after, later);
    CHECK (before > 0.3 && after < 0.05 && later > 0.3, m);
}

static void testAudioTrigger()
{
    std::printf ("[sidechain audio trigger]\n");
    const double sr = 48000;
    Engine e; e.prepare (sr, 512);
    Params p; p.trigger = Trigger::audio; p.depth = 1.0f; p.threshold = 0.2f; p.smoothMs = 0.5f; p.rate = 4; e.setParams (p);
    e.setCurve (Curve::preset (0));
    St s = sine (sr, 2.0, 1000.0, 0.5f);
    St kick (s.size());
    for (size_t k : { (size_t) 12000, (size_t) 36000, (size_t) 60000 }) // three kicks
        for (size_t i = 0; i < 4000 && k + i < kick.size(); ++i)
            kick.l[k + i] = kick.r[k + i] = 0.9f * (float) std::exp (-(double) i / 600.0) * (float) std::sin (2 * PI * 55 * (double) i / sr);
    run (e, s, 512, HostTime { false, 0, 120 }, sr, &kick);
    const double k1 = rms (s.l, 12100, 12500), k2 = rms (s.l, 36100, 36500), between = rms (s.l, 30000, 34000);
    char m[128]; std::snprintf (m, sizeof m, "after kick1 %.3f, kick2 %.3f, between %.3f", k1, k2, between);
    CHECK (k1 < 0.1 && k2 < 0.1 && between > 0.3, m);
}

static void testStress()
{
    std::printf ("[random params, finite output]\n");
    std::mt19937 g (7); std::uniform_real_distribution<float> u (0, 1);
    bool ok = true; float peak = 0;
    for (double sr : { 44100.0, 96000.0, 192000.0 })
    {
        Engine e; e.prepare (sr, 4096);
        St s ((size_t) sr * 2); std::normal_distribution<float> nd (0, 0.3f);
        for (size_t i = 0; i < s.size(); ++i) { s.l[i] = nd (g); s.r[i] = nd (g); }
        size_t pos = 0; double ppq = 0;
        while (pos < s.size())
        {
            Params p; p.rate = (int) (u (g) * 9) % 9; p.trigger = (Trigger) ((int) (u (g) * 3) % 3); p.band = (Band) ((int) (u (g) * 3) % 3);
            p.crossoverHz = 40 + u (g) * 900; p.depth = u (g); p.smoothMs = u (g) * 20; p.threshold = u (g); p.mix = u (g);
            e.setParams (p); e.setCurve (Curve::preset ((int) (u (g) * 8)));
            if (u (g) < 0.3f) e.addNoteOn ((int) (u (g) * 100));
            const int n = (int) std::min<size_t> (1 + (size_t) (u (g) * 2000), s.size() - pos);
            float* ch[2] = { s.l.data() + pos, s.r.data() + pos };
            e.process (ch, 2, nullptr, 0, n, HostTime { u (g) > 0.3f, ppq, 60 + u (g) * 140 });
            ppq += 0.1; pos += (size_t) n;
        }
        for (size_t i = 0; i < s.size(); ++i) { if (! std::isfinite (s.l[i])) ok = false; peak = std::max (peak, std::abs (s.l[i])); }
    }
    CHECK (ok, "no NaN / Inf");
    char m[64]; std::snprintf (m, sizeof m, "peak %.2f bounded", peak); CHECK (peak < 6.0f, m);
}

// ---------- demo ----------
static void writeWav (const std::string& path, const St& s, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    const uint32_t n = (uint32_t) s.size(), bytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); }; auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (uint32_t i = 0; i < n; ++i) for (float v : { s.l[i], s.r[i] }) { int16_t q = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, v)) * 32767.0f); std::fwrite (&q, 2, 1, f); }
    std::fclose (f);
}

static void renderDemos (const std::string& dir)
{
    const int sr = 48000; const double bpm = 140, beat = 60.0 / bpm; const int bars = 4;
    const size_t len = (size_t) (bars * 4 * beat * sr);
    St pad (len), kick (len);
    // sustained chord pad (the thing being pumped)
    for (size_t i = 0; i < len; ++i)
    {
        const double t = (double) i / sr; double v = 0;
        for (double f : { 138.6, 164.8, 207.7, 277.2 }) v += std::sin (2 * PI * f * t) + 0.3 * std::sin (2 * PI * f * 2.003 * t);
        pad.l[i] = (float) (v * 0.07); pad.r[i] = (float) (v * 0.07);
    }
    // four-on-the-floor kicks (sidechain source + audible)
    for (int b = 0; b < bars * 4; ++b)
    {
        const size_t k = (size_t) (b * beat * sr);
        for (size_t i = 0; i < (size_t) (0.35 * sr) && k + i < len; ++i)
        {
            const double t = (double) i / sr, f = 50 + 120 * std::exp (-t * 30);
            const float v = (float) (0.8 * std::exp (-t * 9) * std::sin (2 * PI * f * t));
            kick.l[k + i] += v; kick.r[k + i] += v;
        }
    }
    auto mixWith = [&] (const St& a) { St o (len); for (size_t i = 0; i < len; ++i) { o.l[i] = a.l[i] + kick.l[i]; o.r[i] = a.r[i] + kick.r[i]; } return o; };
    writeWav (dir + "/00_no_pump.wav", mixWith (pad), sr);
    struct D { const char* name; int preset; int rate; Trigger trig; };
    for (D d : { D { "01_classic_duck_1-4", 0, 3, Trigger::sync }, D { "02_hard_pump_1-4", 2, 3, Trigger::sync },
                 D { "03_chop_1-16", 4, 5, Trigger::sync }, D { "04_kick_follow_audio", 1, 3, Trigger::audio } })
    {
        Engine e; e.prepare (sr, 512);
        Params p; p.rate = d.rate; p.trigger = d.trig; p.depth = 0.9f; p.smoothMs = 1.5f; p.threshold = 0.3f; e.setParams (p);
        e.setCurve (Curve::preset (d.preset));
        St s = pad;
        run (e, s, 512, HostTime { true, 0.0, bpm }, sr, &kick);
        writeWav (dir + "/" + d.name + ".wav", mixWith (s), sr);
        std::printf ("  rendered %s.wav\n", d.name);
    }
}

int main (int argc, char** argv)
{
    testCurve(); testSync(); testDepthZeroTransparent(); testLowBandOnly(); testMidiTrigger(); testAudioTrigger(); testStress();
    if (argc >= 3 && std::strcmp (argv[1], "render") == 0) { std::printf ("[render demos]\n"); renderDemos (argv[2]); }
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
