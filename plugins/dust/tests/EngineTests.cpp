// Headless tests + demo renderer for the DUST engine.
// Usage: dust_tests            -> run tests
//        dust_tests render DIR -> also write demo WAVs into DIR
#include "../Source/DSP/DustEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace dust;

static int failures = 0;
#define CHECK(cond, msg)                                                         \
    do {                                                                         \
        if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; }         \
        else          { std::printf ("  ok:   %s\n", msg); }                     \
    } while (0)

static constexpr double PI = 3.14159265358979323846;

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n = 0) : l (n), r (n) {}
    size_t size() const { return l.size(); }
};

static void run (Engine& eng, Stereo& buf, int block)
{
    for (size_t pos = 0; pos < buf.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, buf.size() - pos);
        float* chans[2] = { buf.l.data() + pos, buf.r.data() + pos };
        eng.process (chans, 2, n);
    }
}

static Params neutral()
{
    Params p;
    p.age = 0; p.tape = 0; p.wobble = 0; p.vinyl = 0; p.crush = 0;
    p.tone = 1.0f; p.width = 0.5f; p.mix = 1.0f; p.outputDb = 0;
    return p;
}

static Stereo sine (double sr, double seconds, double freq, float amp)
{
    Stereo s ((size_t) (sr * seconds));
    for (size_t i = 0; i < s.size(); ++i)
        s.l[i] = s.r[i] = amp * (float) std::sin (2 * PI * freq * (double) i / sr);
    return s;
}

static double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double acc = 0;
    for (size_t i = from; i < to; ++i) acc += (double) v[i] * v[i];
    return std::sqrt (acc / (double) (to - from));
}

static void testMixZeroIsDelayedDry()
{
    std::printf ("[mix 0 = dry, latency compensated]\n");
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Engine eng; eng.prepare (sr, 512);
        Params p = neutral(); p.tape = 0.8f; p.wobble = 0.8f; p.vinyl = 0.0f; p.crush = 0.5f; p.mix = 0.0f;
        eng.setParams (p);
        Stereo in = sine (sr, 0.5, 440.0, 0.5f), buf = in;
        run (eng, buf, 256);
        const int L = eng.getLatencySamples();
        double maxErr = 0;
        for (size_t i = (size_t) L; i < buf.size(); ++i)
            maxErr = std::max (maxErr, (double) std::abs (buf.l[i] - in.l[i - (size_t) L]));
        char msg[128]; std::snprintf (msg, sizeof msg, "sr %.0f latency %d max error %.2e", sr, L, maxErr);
        CHECK (maxErr < 1e-6, msg);
    }
}

static void testNeutralIsTransparent()
{
    std::printf ("[neutral settings ~ transparent and time-aligned]\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        Engine eng; eng.prepare (sr, 512);
        eng.setParams (neutral());
        Stereo in = sine (sr, 1.0, 1000.0, 0.25f), buf = in;
        run (eng, buf, 333);
        const int L = eng.getLatencySamples();
        // correlation with delayed input over the second half
        double num = 0, a2 = 0, b2 = 0;
        for (size_t i = buf.size() / 2; i < buf.size(); ++i)
        {
            const double a = buf.l[i], b = in.l[i - (size_t) L];
            num += a * b; a2 += a * a; b2 += b * b;
        }
        const double corr = num / std::sqrt (a2 * b2);
        const double gainDb = 20 * std::log10 (std::sqrt (a2 / b2));
        char msg[160]; std::snprintf (msg, sizeof msg, "sr %.0f corr %.5f gain %.2f dB", sr, corr, gainDb);
        CHECK (corr > 0.995 && std::abs (gainDb) < 0.5, msg);
    }
}

static void testStress()
{
    std::printf ("[random params / block sizes / sample rates: finite and bounded]\n");
    std::mt19937 gen (42);
    std::uniform_real_distribution<float> u (0.0f, 1.0f);
    bool allFinite = true;
    float worstPeak = 0;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        Engine eng; eng.prepare (sr, 4096);
        Stereo buf ((size_t) sr * 2);
        std::normal_distribution<float> nd (0.0f, 0.3f);
        for (size_t i = 0; i < buf.size(); ++i) { buf.l[i] = std::max (-1.0f, std::min (1.0f, nd (gen))); buf.r[i] = buf.l[i] * 0.7f; }
        size_t pos = 0;
        while (pos < buf.size())
        {
            Params p; p.age = u (gen); p.tape = u (gen); p.wobble = u (gen); p.vinyl = u (gen); p.crush = u (gen);
            p.tone = u (gen); p.width = u (gen); p.mix = u (gen); p.outputDb = 0; p.noiseFollow = u (gen) > 0.5f;
            eng.setParams (p);
            const int n = (int) std::min<size_t> (1 + (size_t) (u (gen) * 2047), buf.size() - pos);
            float* chans[2] = { buf.l.data() + pos, buf.r.data() + pos };
            eng.process (chans, 2, n);
            pos += (size_t) n;
        }
        for (size_t i = 0; i < buf.size(); ++i)
        {
            if (! std::isfinite (buf.l[i]) || ! std::isfinite (buf.r[i])) allFinite = false;
            worstPeak = std::max (worstPeak, std::max (std::abs (buf.l[i]), std::abs (buf.r[i])));
        }
    }
    CHECK (allFinite, "no NaN / Inf");
    char msg[96]; std::snprintf (msg, sizeof msg, "peak %.2f stays below 4.0 (+12 dBFS)", worstPeak);
    CHECK (worstPeak < 4.0f, msg);
}

static void testMono()
{
    std::printf ("[mono bus]\n");
    Engine eng; eng.prepare (48000, 512);
    Params p; p.age = 0.7f; eng.setParams (p);
    std::vector<float> m = sine (48000, 1.0, 220.0, 0.5f).l;
    float* chans[1] = { m.data() };
    for (size_t pos = 0; pos < m.size(); pos += 512) { chans[0] = m.data() + pos; eng.process (chans, 1, (int) std::min<size_t> (512, m.size() - pos)); }
    bool ok = true; for (float v : m) if (! std::isfinite (v)) ok = false;
    CHECK (ok && rms (m, m.size() / 2, m.size()) > 0.05, "mono processing works");
}

static void testNoiseFollow()
{
    std::printf ("[vinyl noise follows signal]\n");
    for (bool follow : { true, false })
    {
        Engine eng; eng.prepare (48000, 512);
        Params p = neutral(); p.vinyl = 1.0f; p.noiseFollow = follow; eng.setParams (p);
        Stereo buf ((size_t) 48000 * 2); // silence
        run (eng, buf, 512);
        const double level = 20 * std::log10 (rms (buf.l, buf.size() / 2, buf.size()) + 1e-12);
        char msg[96]; std::snprintf (msg, sizeof msg, "follow=%d silence -> noise %.1f dBFS", (int) follow, level);
        CHECK (follow ? level < -90.0 : level > -60.0, msg);
    }
}

static void testWobbleBendsPitch()
{
    std::printf ("[wobble actually modulates pitch]\n");
    const double sr = 48000;
    auto zeroCrossSpread = [&] (float wob)
    {
        Engine eng; eng.prepare (sr, 512);
        Params p = neutral(); p.wobble = wob; eng.setParams (p);
        Stereo buf = sine (sr, 4.0, 1000.0, 0.3f);
        run (eng, buf, 512);
        // measure local frequency via zero crossings in 50 ms windows
        std::vector<double> f;
        const size_t win = (size_t) (0.05 * sr);
        for (size_t s = (size_t) sr; s + win < buf.size(); s += win)
        {
            int zc = 0; for (size_t i = s + 1; i < s + win; ++i) if ((buf.l[i - 1] < 0) != (buf.l[i] < 0)) ++zc;
            f.push_back (zc / 2.0 / 0.05);
        }
        double mn = 1e9, mx = 0; for (double v : f) { mn = std::min (mn, v); mx = std::max (mx, v); }
        return mx - mn;
    };
    const double off = zeroCrossSpread (0.0f), on = zeroCrossSpread (1.0f);
    char msg[128]; std::snprintf (msg, sizeof msg, "pitch spread off %.1f Hz, full %.1f Hz", off, on);
    CHECK (off <= 20.0 && on > 25.0, msg);
}

static void benchmark()
{
    std::printf ("[CPU]\n");
    const double sr = 48000;
    Engine eng; eng.prepare (sr, 512);
    Params p; p.age = 0.5f; p.vinyl = 0.6f; p.crush = 0.3f; eng.setParams (p);
    Stereo buf = sine (sr, 10.0, 110.0, 0.5f);
    const auto t0 = std::chrono::steady_clock::now();
    run (eng, buf, 512);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    char msg[96]; std::snprintf (msg, sizeof msg, "%.2f %% of one core (stereo, 48 kHz)", 100.0 * secs / 10.0);
    CHECK (secs / 10.0 < 0.05, msg);
}

// ---------------- demo rendering ----------------

static void writeWav (const std::string& path, const Stereo& s, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) s.size(), dataBytes = n * 2 * 2;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
        for (float v : { s.l[i], s.r[i] })
        {
            const float c = std::max (-1.0f, std::min (1.0f, v));
            const int16_t q = (int16_t) std::lround (c * 32767.0f);
            std::fwrite (&q, 2, 1, f);
        }
    std::fclose (f);
}

// A dark trap-style bell / pluck melody (synthesised, no samples).
static Stereo makeMelody (int sr, double bpm, int bars)
{
    const double beat = 60.0 / bpm;
    const double len = bars * 4 * beat;
    Stereo s ((size_t) (len * sr) + (size_t) sr);
    // C# minor-ish phrase (MIDI notes) on 8th notes, -1 = rest
    const int phrase[16] = { 73, -1, 76, 73, 80, -1, 78, 76, 73, -1, 71, 73, 68, -1, 71, -1 };
    const int chordRoots[4] = { 49, 45, 52, 47 }; // C#, A, E, B
    auto mtof = [] (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); };
    auto addNote = [&] (double start, double dur, double freq, float amp, bool bell, float pan)
    {
        const size_t s0 = (size_t) (start * sr), n = (size_t) ((dur + 1.2) * sr);
        for (size_t i = 0; i < n && s0 + i < s.size(); ++i)
        {
            const double t = (double) i / sr;
            const double env = std::exp (-t * (bell ? 3.2 : 1.1)) * std::min (1.0, t * 400.0) * (t < dur + 0.05 ? 1.0 : std::exp (-(t - dur - 0.05) * 12));
            double v = std::sin (2 * PI * freq * t) + (bell ? 0.35 * std::sin (2 * PI * freq * 2.76 * t) * std::exp (-t * 6) + 0.2 * std::sin (2 * PI * freq * 5.4 * t) * std::exp (-t * 10)
                                                          : 0.3 * std::sin (2 * PI * freq * 2 * t) + 0.15 * std::sin (2 * PI * freq * 3 * t));
            const float x = (float) (v * env) * amp;
            s.l[s0 + i] += x * (1.0f - pan);
            s.r[s0 + i] += x * (1.0f + pan);
        }
    };
    for (int bar = 0; bar < bars; ++bar)
    {
        const double barStart = bar * 4 * beat;
        const int root = chordRoots[bar % 4];
        for (int k : { 0, 3, 7, 12 }) // minor-ish pad chord
            addNote (barStart, 4 * beat - 0.05, mtof (root + 12 + (k == 3 && (bar % 4 == 1 || bar % 4 == 2) ? 4 : k)), 0.06f, false, (k % 2 ? 0.2f : -0.2f));
        for (int i = 0; i < 8; ++i)
        {
            const int note = phrase[(bar % 2) * 8 + i];
            if (note > 0) addNote (barStart + i * beat / 2, beat / 2, mtof (note), 0.16f, true, 0.0f);
        }
    }
    return s;
}

struct Demo { const char* name; Params p; };

static void renderDemos (const std::string& dir)
{
    const int sr = 48000;
    const Stereo dry = makeMelody (sr, 140.0, 4);
    writeWav (dir + "/00_dry.wav", dry, sr);

    std::vector<Demo> demos;
    { Params p; p.age = 0.0f; p.tape = 0.45f; p.wobble = 0.35f; p.vinyl = 0.35f; p.crush = 0.0f; p.tone = 0.42f; demos.push_back ({ "01_dusty_cassette", p }); }
    { Params p; p.age = 0.0f; p.tape = 0.3f; p.wobble = 0.15f; p.vinyl = 0.6f; p.crush = 0.0f; p.tone = 0.35f; p.width = 0.7f; demos.push_back ({ "02_vinyl_loop", p }); }
    { Params p; p.age = 0.0f; p.tape = 0.6f; p.wobble = 0.8f; p.vinyl = 0.2f; p.crush = 0.0f; p.tone = 0.38f; demos.push_back ({ "03_warped_tape", p }); }
    { Params p; p.age = 0.0f; p.tape = 0.35f; p.wobble = 0.1f; p.vinyl = 0.1f; p.crush = 0.55f; p.tone = 0.5f; demos.push_back ({ "04_8bit_dream", p }); }
    { Params p; p.age = 0.85f; p.tape = 0.2f; p.wobble = 0.2f; p.vinyl = 0.2f; p.crush = 0.0f; p.tone = 0.5f; demos.push_back ({ "05_age_85", p }); }

    for (const auto& d : demos)
    {
        Engine eng; eng.prepare (sr, 512); eng.setParams (d.p);
        Stereo buf = dry;
        run (eng, buf, 512);
        writeWav (dir + "/" + d.name + ".wav", buf, sr);
        std::printf ("  rendered %s.wav\n", d.name);
    }
}

int main (int argc, char** argv)
{
    testMixZeroIsDelayedDry();
    testNeutralIsTransparent();
    testStress();
    testMono();
    testNoiseFollow();
    testWobbleBendsPitch();
    benchmark();

    if (argc >= 3 && std::strcmp (argv[1], "render") == 0)
    {
        std::printf ("[render demos]\n");
        renderDemos (argv[2]);
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
