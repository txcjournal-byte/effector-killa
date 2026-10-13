// Headless tests for the TrapWin SUB KILLA engine.
#include "../Source/DSP/SubEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace subkilla;
static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FAIL: %s\n", msg); ++failures; } else std::printf ("  ok:   %s\n", msg); } while (0)

struct Stereo { std::vector<float> l, r; };

static Params clean()
{
    Params p;
    p.mode = VoiceMode::legato;
    p.glideMs = 0.0f; p.tuneSemis = 0.0f; p.fineCents = 0.0f;
    p.punchSemis = 14.0f; p.punchMs = 45.0f; p.click = 0.0f; p.sub = 0.0f;
    p.drive = 0.0f; p.sat = SatType::tube; p.distMix = 1.0f;
    p.cutoffHz = 20000.0f; p.reso = 0.0f; p.fltEnv = 0.0f;
    p.attackMs = 1.0f; p.decayMs = 8000.0f; p.sustain = 0.0f; p.releaseMs = 200.0f;
    p.width = 0.0f; p.outputDb = 0.0f; p.limiter = true;
    return p;
}

struct TimedEvent { size_t at; Event e; };

static Event on (int note, float vel = 1.0f) { Event e; e.type = Event::noteOn; e.note = note; e.value = vel; return e; }
static Event off (int note) { Event e; e.type = Event::noteOff; e.note = note; return e; }
static Event bend (float v) { Event e; e.type = Event::pitchBend; e.value = v; return e; }

// renders n samples, events at absolute sample positions; calls probe(blockEndSample) after each block
template <typename Probe>
static Stereo render (Engine& eng, size_t n, std::vector<TimedEvent> ev, int block, Probe probe)
{
    Stereo out { std::vector<float> (n), std::vector<float> (n) };
    std::sort (ev.begin(), ev.end(), [] (auto& a, auto& b) { return a.at < b.at; });
    size_t ei = 0;
    std::vector<Event> local;
    for (size_t pos = 0; pos < n; pos += (size_t) block)
    {
        const int len = (int) std::min<size_t> ((size_t) block, n - pos);
        local.clear();
        while (ei < ev.size() && ev[ei].at < pos + (size_t) len)
        {
            Event e = ev[ei].e;
            e.offset = (int) (ev[ei].at - pos);
            local.push_back (e);
            ++ei;
        }
        eng.process (out.l.data() + pos, out.r.data() + pos, len, local.data(), (int) local.size());
        probe (pos + (size_t) len);
    }
    return out;
}

static Stereo render (Engine& eng, size_t n, std::vector<TimedEvent> ev, int block = 256)
{
    return render (eng, n, std::move (ev), block, [] (size_t) {});
}

// mean frequency from interpolated rising zero crossings in [from, to)
static double freqOf (const std::vector<float>& x, size_t from, size_t to, double sr)
{
    double first = -1, last = -1;
    int count = 0;
    for (size_t i = std::max<size_t> (from, 1); i < std::min (to, x.size()); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (double) (x[i - 1] - x[i]);
            if (first < 0) first = t;
            last = t;
            ++count;
        }
    if (count < 2) return 0.0;
    return (count - 1) * sr / (last - first);
}

static float peakIn (const std::vector<float>& v, size_t from, size_t to)
{
    float m = 0; for (size_t i = from; i < std::min (to, v.size()); ++i) m = std::max (m, std::abs (v[i])); return m;
}

int main()
{
    const double note36 = 65.40639;

    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("-- pitch @ %.0f Hz\n", sr);
        Engine e; e.prepare (sr, 512); e.setParams (clean());
        auto y = render (e, (size_t) (sr * 0.8), { { 0, on (36) } }, 512);
        const double f = freqOf (y.l, (size_t) (sr * 0.3), (size_t) (sr * 0.75), sr);
        char msg[160];
        std::snprintf (msg, sizeof msg, "note 36 -> %.3f Hz (expect 65.406 +-0.5 %%)", f);
        CHECK (std::abs (f / note36 - 1.0) < 0.005, msg);
        const double fEarly = freqOf (y.l, (size_t) (sr * 0.002), (size_t) (sr * 0.035), sr);
        std::snprintf (msg, sizeof msg, "punch: pitch envelope starts higher (%.1f Hz in the first 35 ms)", fEarly);
        CHECK (fEarly > note36 * 1.15 && fEarly < note36 * 2.3, msg);
    }

    const double SR = 48000.0;

    std::printf ("-- pitch survives saturation / tune\n");
    {
        auto p = clean(); p.drive = 0.7f; p.sat = SatType::hard; p.sub = 0.0f;
        Engine e; e.prepare (SR, 256); e.setParams (p);
        auto y = render (e, (size_t) (SR * 0.8), { { 0, on (36) } });
        const double f = freqOf (y.l, (size_t) (SR * 0.3), (size_t) (SR * 0.75), SR);
        CHECK (std::abs (f / note36 - 1.0) < 0.005, "hard-clipped note 36 keeps its fundamental");

        auto pt = clean(); pt.tuneSemis = 12.0f;
        Engine e2; e2.prepare (SR, 256); e2.setParams (pt);
        auto y2 = render (e2, (size_t) (SR * 0.8), { { 0, on (36) } });
        CHECK (std::abs (freqOf (y2.l, (size_t) (SR * 0.3), (size_t) (SR * 0.75), SR) / (note36 * 2) - 1.0) < 0.005, "tune +12 doubles the pitch");

        Engine e3; e3.prepare (SR, 256); e3.setParams (clean());
        auto y3 = render (e3, (size_t) (SR * 0.8), { { 0, on (36) }, { 4800, bend (1.0f) } });
        CHECK (std::abs (freqOf (y3.l, (size_t) (SR * 0.4), (size_t) (SR * 0.75), SR) / (note36 * 2) - 1.0) < 0.005, "pitch bend +max = +12 st");
    }

    std::printf ("-- glide (legato slide)\n");
    {
        auto p = clean(); p.glideMs = 200.0f; p.mode = VoiceMode::legato;
        Engine e; e.prepare (SR, 64); e.setParams (p);
        const size_t t2 = (size_t) (SR * 0.3);
        double reached = -1, halfNote = 0;
        int retrigBefore = 0;
        float envBefore = 0, envAfter = 0;
        auto y = render (e, (size_t) (SR * 1.0), { { 0, on (36) }, { t2, on (48) } }, 64, [&] (size_t endPos)
        {
            if (endPos == t2) { retrigBefore = e.getRetriggerCount(); envBefore = e.getAmpEnv(); }
            if (endPos == t2 + 64) envAfter = e.getAmpEnv();
            if (endPos == t2 + (size_t) (SR * 0.1)) halfNote = e.getCurrentNote();
            if (endPos > t2 && reached < 0 && e.getCurrentNote() >= 47.999f) reached = (double) (endPos - t2) / SR * 1000.0;
        });
        char msg[160];
        std::snprintf (msg, sizeof msg, "halfway after 100 ms (note %.2f, expect 42)", halfNote);
        CHECK (std::abs (halfNote - 42.0) < 0.3, msg);
        std::snprintf (msg, sizeof msg, "target reached after %.1f ms (glide 200 ms)", reached);
        CHECK (reached > 190.0 && reached < 212.0, msg);
        CHECK (e.getRetriggerCount() == retrigBefore, "legato: overlapping note does not retrigger");
        std::snprintf (msg, sizeof msg, "legato: amp env keeps decaying (%.4f -> %.4f)", envBefore, envAfter);
        CHECK (envAfter <= envBefore && envAfter > envBefore * 0.97f, msg);
        const double f = freqOf (y.l, (size_t) (SR * 0.6), (size_t) (SR * 0.95), SR);
        std::snprintf (msg, sizeof msg, "after the slide: %.2f Hz (expect 130.81)", f);
        CHECK (std::abs (f / (note36 * 2) - 1.0) < 0.005, msg);
    }
    {
        // releasing the upper note slides back to the held one
        auto p = clean(); p.glideMs = 50.0f;
        Engine e; e.prepare (SR, 64); e.setParams (p);
        render (e, (size_t) (SR * 0.6), { { 0, on (36) }, { 4800, on (43) }, { 14400, off (43) } }, 64);
        CHECK (std::abs (e.getCurrentNote() - 36.0f) < 1e-3f && e.getActiveVoices() == 1, "note-off of the top note slides back to the held note");
    }
    {
        auto p = clean(); p.glideMs = 100.0f; p.mode = VoiceMode::mono;
        Engine e; e.prepare (SR, 64); e.setParams (p);
        render (e, (size_t) (SR * 0.2), { { 0, on (36) }, { 4800, on (40) } }, 64);
        CHECK (e.getRetriggerCount() == 2, "mono mode: overlapping note retriggers (and still glides)");
        // non-overlapping notes do not glide
        auto pl = clean(); pl.glideMs = 300.0f; pl.releaseMs = 20.0f;
        Engine e2; e2.prepare (SR, 64); e2.setParams (pl);
        bool glided = false;
        render (e2, (size_t) (SR * 0.5), { { 0, on (36) }, { 4800, off (36) }, { 9600, on (48) } }, 64, [&] (size_t endPos)
        {
            if (endPos > 9600 && endPos < 9600 + 2000 && e2.getActiveVoices() > 0 && std::abs (e2.getCurrentNote() - 48.0f) > 0.01f) glided = true;
        });
        CHECK (! glided, "detached notes start on pitch (glide only on overlap)");
    }

    std::printf ("-- amp decay follows the parameter\n");
    for (float dms : { 400.0f, 1500.0f, 5000.0f })
    {
        auto p = clean(); p.decayMs = dms; p.punchSemis = 0.0f; p.attackMs = 0.5f;
        Engine e; e.prepare (SR, 256); e.setParams (p);
        const size_t n = (size_t) (SR * dms / 1000.0 * 1.6);
        auto y = render (e, n, { { 0, on (36) } });
        const float pk0 = peakIn (y.l, 0, (size_t) (SR * 0.03));
        size_t at = 0;
        const size_t win = (size_t) (SR * 0.02);
        for (size_t pos = 0; pos + win < n; pos += win / 2)
            if (peakIn (y.l, pos, pos + win) < pk0 * 0.001f) { at = pos + win / 2; break; }
        const double ms = at / SR * 1000.0;
        char msg[160];
        std::snprintf (msg, sizeof msg, "decay %.0f ms -> -60 dB after %.0f ms", dms, ms);
        CHECK (at > 0 && std::abs (ms / dms - 1.0) < 0.1, msg);
    }
    {
        auto p = clean(); p.sustain = 0.5f; p.punchSemis = 0.0f; p.decayMs = 200.0f; p.releaseMs = 300.0f;
        Engine e; e.prepare (SR, 256); e.setParams (p);
        auto y = render (e, (size_t) (SR * 2.0), { { 0, on (36) }, { (size_t) SR, off (36) } });
        const float pk0 = peakIn (y.l, 0, 2000), sus = peakIn (y.l, (size_t) (SR * 0.8), (size_t) (SR * 0.95));
        CHECK (std::abs (sus / pk0 - 0.5f) < 0.05f, "sustain level holds while the key is down");
        CHECK (peakIn (y.l, (size_t) (SR * 1.35), (size_t) (SR * 2.0)) < 1e-4f && e.getActiveVoices() == 0, "release fades to silence and frees the voice");
    }

    std::printf ("-- silence, timing\n");
    {
        Engine e; e.prepare (SR, 256); e.setParams (clean());
        auto y = render (e, (size_t) SR, {});
        CHECK (peakIn (y.l, 0, y.l.size()) == 0.0f && peakIn (y.r, 0, y.r.size()) == 0.0f, "silence when no notes are played");
        // sample-accurate note-on inside a block
        Engine e2; e2.prepare (SR, 512); e2.setParams (clean());
        auto y2 = render (e2, 2048, { { 300, on (36) } }, 512);
        CHECK (peakIn (y2.l, 0, 300) == 0.0f && peakIn (y2.l, 300, 360) > 1e-3f, "note starts exactly at its MIDI timestamp");
    }

    std::printf ("-- poly\n");
    {
        auto p = clean(); p.mode = VoiceMode::poly;
        Engine e; e.prepare (SR, 256); e.setParams (p);
        render (e, 4800, { { 0, on (36) }, { 10, on (40) }, { 20, on (43) } });
        CHECK (e.getActiveVoices() == 3, "3 notes -> 3 voices");
        render (e, 4800, { { 0, on (47) }, { 10, on (50) }, { 20, on (55) } });
        CHECK (e.getActiveVoices() == 4, "6 notes -> capped at 4 voices (oldest stolen)");
    }

    std::printf ("-- stereo width keeps the sub mono\n");
    {
        auto p = clean(); p.width = 1.0f; p.punchSemis = 0.0f;
        Engine e; e.prepare (SR, 256); e.setParams (p);
        auto y = render (e, (size_t) (SR * 0.5), { { 0, on (33) } });
        float side = 0, mid = 0;
        for (size_t i = (size_t) (SR * 0.1); i < y.l.size(); ++i) { side = std::max (side, std::abs (y.l[i] - y.r[i]) * 0.5f); mid = std::max (mid, std::abs (y.l[i] + y.r[i]) * 0.5f); }
        char msg[160];
        std::snprintf (msg, sizeof msg, "clean 55 Hz sub at width 100 %%: side %.1f dB below mid", 20 * std::log10 (std::max (1e-9f, side) / mid));
        CHECK (side < mid * 0.03f, msg);
        auto pd = p; pd.drive = 0.8f; pd.sat = SatType::hard;
        Engine e2; e2.prepare (SR, 256); e2.setParams (pd);
        auto y2 = render (e2, (size_t) (SR * 0.5), { { 0, on (33) } });
        float side2 = 0;
        for (size_t i = (size_t) (SR * 0.1); i < y2.l.size(); ++i) side2 = std::max (side2, std::abs (y2.l[i] - y2.r[i]) * 0.5f);
        CHECK (side2 > 0.02f, "distortion harmonics above 200 Hz get spread");
    }

    std::printf ("-- limiter & stability\n");
    {
        auto p = clean(); p.mode = VoiceMode::poly; p.outputDb = 12.0f; p.drive = 1.0f; p.sat = SatType::hard;
        p.sub = 1.0f; p.click = 1.0f; p.reso = 1.0f; p.width = 1.0f;
        for (bool lim : { true, false })
        {
            p.limiter = lim;
            Engine e; e.prepare (44100.0, 256); e.setParams (p);
            auto y = render (e, 44100, { { 0, on (30) }, { 0, on (37) }, { 0, on (42) }, { 0, on (49) } });
            const float pk = std::max (peakIn (y.l, 0, y.l.size()), peakIn (y.r, 0, y.r.size()));
            char msg[160];
            if (lim)
            {
                std::snprintf (msg, sizeof msg, "limiter on: peak %.4f <= 0 dBFS (ceiling %.4f)", pk, limiterCeiling);
                CHECK (pk <= limiterCeiling && pk > 0.5f, msg);
            }
            else
            {
                std::snprintf (msg, sizeof msg, "limiter off: +12 dB really is hot (peak %.2f)", pk);
                CHECK (pk > 1.0f, msg);
            }
        }
    }
    for (double sr : { 44100.0, 192000.0 })
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        Engine e; e.prepare (sr, 512);
        bool finite = true;
        float pk = 0.0f;
        const auto t0 = std::chrono::steady_clock::now();
        const int blocks = (int) (sr * 20.0 / 512);
        std::vector<float> l (512), r (512);
        for (int b = 0; b < blocks; ++b)
        {
            if (b % 40 == 0)
            {
                Params p;
                p.mode = (VoiceMode) (int) (u (rng) * 2.99f);
                p.glideMs = u (rng) * 2000; p.tuneSemis = u (rng) * 48 - 24; p.fineCents = u (rng) * 200 - 100;
                p.punchSemis = u (rng) * 48; p.punchMs = 5 + u (rng) * 395; p.click = u (rng); p.sub = u (rng);
                p.drive = u (rng); p.sat = (SatType) (int) (u (rng) * 3.99f); p.distMix = u (rng);
                p.cutoffHz = 30 + u (rng) * 19970; p.reso = u (rng); p.fltEnv = u (rng);
                p.attackMs = u (rng) * 200; p.decayMs = 30 + u (rng) * 11970; p.sustain = u (rng); p.releaseMs = 5 + u (rng) * 4995;
                p.width = u (rng); p.outputDb = u (rng) * 36 - 24; p.limiter = true;
                e.setParams (p);
            }
            Event ev[3];
            int ne = 0;
            if (b % 7 == 0) { ev[ne] = on (12 + (int) (u (rng) * 100), u (rng)); ev[ne].offset = (int) (u (rng) * 511); ++ne; }
            if (b % 11 == 0) { ev[ne] = off (12 + (int) (u (rng) * 100)); ev[ne].offset = 511; ++ne; }
            if (b % 13 == 0) { ev[ne] = bend (u (rng) * 2 - 1); ev[ne].offset = 511; ++ne; }
            e.process (l.data(), r.data(), 512, ev, ne);
            for (int i = 0; i < 512; ++i)
            {
                finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
                pk = std::max ({ pk, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
            }
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        char msg[160];
        std::snprintf (msg, sizeof msg, "random params/notes @ %.0f Hz: no NaN/inf, peak %.4f (%.2f s CPU for 20 s)", sr, pk, secs);
        CHECK (finite && pk <= limiterCeiling, msg);
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
