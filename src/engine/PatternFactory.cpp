/*
    TrapWin BEND DEMON
    PatternFactory.cpp - the factory patterns (4 bars, written for ~140 bpm trap, C minor).
*/
#include "PatternTools.h"
#include <cmath>

namespace bd::tools
{

namespace
{
    struct Step { double beat; float pitch; double glide; };

    /** One legato line: holds each pitch, glides into the next over `glide` beats. */
    PatternLine legato (double start, double end, std::initializer_list<Step> steps, float vel = 0.85f)
    {
        PatternLine l;
        l.start = start;
        l.length = end - start;
        l.velocity = vel;
        l.smooth = true;

        const Step* prev = nullptr;
        for (auto& s : steps)
        {
            const double t = s.beat - start;
            if (prev == nullptr)
                l.pitch.push_back ({ t, s.pitch });
            else
            {
                l.pitch.push_back ({ t, prev->pitch });
                l.pitch.push_back ({ t + std::max (1.0 / 128.0, s.glide), s.pitch });
            }
            prev = &s;
        }
        l.normalise();
        return l;
    }

    void addFall (PatternLine& l, double fromRel, float semis)
    {
        const float p = l.curvePitchAt (fromRel);
        Curve1D pts;
        for (auto& pt : l.pitch) if (pt.t < fromRel - 1.0e-6) pts.push_back (pt);
        pts.push_back ({ fromRel, p });
        pts.push_back ({ fromRel + (l.length - fromRel) * 0.35, p + semis * 0.25f });
        pts.push_back ({ l.length, p + semis });
        l.pitch = pts;
    }

    void addScoop (PatternLine& l, float below, double time)
    {
        const float p = l.curvePitchAt (0.0);
        l.pitch.front().v = p - below;
        l.pitch.insert (l.pitch.begin() + 1, { time, p });
        l.normalise();
    }

    void swell (PatternLine& l, float from, float to, double riseBeats, float endLevel)
    {
        l.pressure = { { 0.0, from }, { std::min (riseBeats, l.length * 0.8), to }, { l.length, endLevel } };
    }

    void brighten (PatternLine& l, float from, float to)
    {
        l.timbre = { { 0.0, from }, { l.length, to } };
    }

    void vibrato (PatternLine& l, float depth, float rate, float fade)
    {
        l.vibDepth = depth; l.vibRate = rate; l.vibFade = fade;
    }

    //==============================================================================
    Pattern trapGlideLead()
    {
        Pattern p;
        auto a = legato (0.0, 3.0, { { 0.0, 72, 0 }, { 0.75, 75, 0.12 }, { 1.5, 79, 0.15 } });
        vibrato (a, 0.28f, 2.6f, 1.0f); swell (a, 0.6f, 0.95f, 1.0, 0.8f); brighten (a, 0.35f, 0.75f);
        auto b = legato (3.0, 4.0, { { 3.0, 77, 0 }, { 3.5, 75, 0.15 } }, 0.75f);
        auto c = legato (4.0, 7.5, { { 4.0, 74, 0 }, { 4.5, 75, 0.1 }, { 5.0, 72, 0.12 } });
        vibrato (c, 0.3f, 2.6f, 1.2f); addFall (c, 3.0, -5.0f); swell (c, 0.7f, 1.0f, 1.5, 0.5f);
        auto d = legato (8.0, 11.0, { { 8.0, 72, 0 }, { 8.75, 75, 0.12 }, { 9.5, 79, 0.12 }, { 10.0, 82, 0.25 } });
        vibrato (d, 0.25f, 2.8f, 0.6f); brighten (d, 0.4f, 0.9f);
        auto e = makeNote (11.0, 0.5, 80, 0.7f);
        auto f = makeNote (11.5, 0.5, 79, 0.75f);
        auto g = legato (12.0, 16.0, { { 12.0, 77, 0 }, { 12.75, 75, 0.12 }, { 13.5, 74, 0.12 }, { 14.0, 72, 0.15 } });
        vibrato (g, 0.32f, 2.6f, 1.0f); addFall (g, 3.4, -12.0f); swell (g, 0.6f, 1.0f, 2.0, 0.4f);
        for (auto* l : { &a, &b, &c, &d, &e, &f, &g }) p.add (*l);
        return p;
    }

    Pattern slide808()
    {
        Pattern p;
        auto add = [&p] (PatternLine l, float vel = 0.95f) { l.velocity = vel; p.add (l); };
        add (makeNote (0.0, 1.5, 36));
        add (makeNote (1.5, 0.5, 36), 0.8f);
        add (legato (2.0, 4.0, { { 2.0, 36, 0 }, { 3.0, 48, 0.12 }, { 3.5, 43, 0.1 } }));
        add (makeNote (4.0, 1.0, 39));
        auto e = legato (5.0, 7.5, { { 5.0, 39, 0 }, { 6.0, 36, 0.2 } });
        addFall (e, 2.0, -7.0f);
        add (e);
        add (makeNote (8.0, 0.75, 36));
        add (makeNote (8.75, 0.75, 36), 0.8f);
        add (legato (9.5, 12.0, { { 9.5, 36, 0 }, { 10.5, 46, 0.1 }, { 11.0, 43, 0.15 } }));
        add (legato (12.0, 14.0, { { 12.0, 41, 0 }, { 13.0, 39, 0.25 } }));
        auto j = legato (14.0, 16.0, { { 14.0, 36, 0 } });
        addFall (j, 1.2, -12.0f);
        add (j);
        return p;
    }

    Pattern chordGlidePad()
    {
        Pattern p;
        const float voices[3][4] = { { 60, 60, 58, 58 }, { 63, 63, 63, 62 }, { 67, 68, 67, 65 } };
        for (auto& v : voices)
        {
            auto l = legato (0.0, 16.0, { { 0.0, v[0], 0 }, { 4.0, v[1], 0.5 }, { 8.0, v[2], 0.5 }, { 12.0, v[3], 0.6 } }, 0.7f);
            l.pressure.clear();
            for (int bar = 0; bar < 4; ++bar)
            {
                l.pressure.push_back ({ bar * 4.0 + 0.01, 0.35f });
                l.pressure.push_back ({ bar * 4.0 + 2.5, 0.95f });
            }
            l.pressure.push_back ({ 16.0, 0.4f });
            brighten (l, 0.25f, 0.8f);
            vibrato (l, 0.06f, 1.5f, 2.0f);
            p.add (l);
        }
        return p;
    }

    Pattern vocalLead()
    {
        Pattern p;
        auto voc = [&p] (PatternLine l, float vib = 0.35f)
        {
            addScoop (l, 1.2f, 0.25);
            vibrato (l, vib, 2.5f, 1.0f);
            swell (l, 0.45f, 1.0f, 0.8, 0.7f);
            brighten (l, 0.45f, 0.7f);
            p.add (l);
        };
        voc (legato (0.0, 3.5, { { 0.0, 75, 0 } }));
        voc (legato (4.0, 7.5, { { 4.0, 74, 0 }, { 5.5, 72, 0.3 } }));
        voc (legato (8.0, 11.5, { { 8.0, 70, 0 }, { 9.0, 72, 0.35 } }));
        voc (legato (12.0, 13.0, { { 12.0, 67, 0 } }), 0.15f);
        voc (legato (13.0, 14.0, { { 13.0, 70, 0 } }), 0.15f);
        auto last = legato (14.0, 16.0, { { 14.0, 72, 0 } });
        addFall (last, 1.6, -3.0f);
        voc (last);
        return p;
    }

    Pattern bellBends()
    {
        Pattern p;
        const float arps[4][8] = { { 60, 63, 67, 70, 74, 70, 67, 63 },
                                   { 60, 63, 67, 70, 75, 70, 67, 63 },
                                   { 56, 60, 63, 67, 72, 67, 63, 60 },
                                   { 58, 62, 65, 70, 74, 70, 65, 62 } };
        for (int bar = 0; bar < 4; ++bar)
            for (int i = 0; i < 8; ++i)
            {
                auto l = makeNote (bar * 4.0 + i * 0.5, 0.45, arps[bar][i] + 12.0f, i % 2 == 0 ? 0.85f : 0.65f);
                if (i % 4 == 3)
                    l.pitch = { { 0.0, arps[bar][i] + 12.0f }, { 0.2, arps[bar][i] + 12.0f }, { 0.45, arps[bar][i] + 14.0f } };
                brighten (l, 0.7f, 0.4f);
                p.add (l);
            }
        return p;
    }

    Pattern pluckRuns()
    {
        Pattern p;
        const float scale[] = { 60, 63, 65, 67, 70, 72, 75, 77, 79, 82, 84, 87 };
        const int roots[4] = { 0, 1, 2, 1 };
        for (int bar = 0; bar < 4; ++bar)
        {
            const double b0 = bar * 4.0;
            for (int i = 0; i < 6; ++i)
            {
                auto l = makeNote (b0 + i * 0.25, 0.22, scale[roots[bar] + i], 0.6f + 0.06f * (float) i);
                addScoop (l, 0.6f, 1.0 / 32.0);
                p.add (l);
            }
            auto held = makeNote (b0 + 1.5, 1.5, scale[roots[bar] + 6], 0.9f);
            vibrato (held, 0.2f, 3.0f, 0.75f);
            p.add (held);
            for (int i = 0; i < 4; ++i)
            {
                auto l = makeNote (b0 + 3.0 + i * 0.25, 0.22, scale[roots[bar] + 5 - i], 0.7f - 0.05f * (float) i);
                p.add (l);
            }
        }
        return p;
    }

    Pattern subWobble()
    {
        Pattern p;
        const float notes[4] = { 36, 39, 41, 34 };
        for (int bar = 0; bar < 4; ++bar)
        {
            PatternLine l = makeNote (bar * 4.0, 3.75, notes[bar], 0.9f);
            l.pitch.clear();
            for (int k = 0; k <= 30; ++k)
            {
                const double t = k * 0.125;
                const float w = k < 4 ? 0.0f : 0.45f * (float) std::sin (juce::MathConstants<double>::twoPi * t / 0.5);
                l.pitch.push_back ({ std::min (t, 3.75), notes[bar] + w });
            }
            brighten (l, 0.2f, 0.6f);
            p.add (l);
        }
        return p;
    }

    Pattern darkRise()
    {
        Pattern p;
        auto straight = [&p] (Curve1D pts, float vel)
        {
            PatternLine l;
            l.start = 0.0; l.length = 16.0; l.smooth = false; l.velocity = vel;
            l.pitch = std::move (pts);
            l.pressure = { { 0.0, 0.3f }, { 16.0, 1.0f } };
            l.timbre   = { { 0.0, 0.1f }, { 16.0, 1.0f } };
            p.add (l);
        };
        straight ({ { 0.0, 48 }, { 16.0, 60 } }, 0.8f);
        straight ({ { 0.0, 55 }, { 8.0, 58 }, { 16.0, 67 } }, 0.7f);
        straight ({ { 0.0, 60 }, { 12.0, 60 }, { 16.0, 72 } }, 0.7f);
        return p;
    }

    struct Entry { const char* name; Pattern (*make)(); };
    const Entry entries[] = {
        { "Trap Glide Lead",    trapGlideLead },
        { "808 Slide Bassline", slide808 },
        { "Chord Glide Pad",    chordGlidePad },
        { "Vocal Vibrato Lead", vocalLead },
        { "Bell Bends",         bellBends },
        { "Pluck Runs",         pluckRuns },
        { "Sub Wobble",         subWobble },
        { "Dark Rise",          darkRise },
    };
}

int numFactoryPatterns()    { return (int) (sizeof (entries) / sizeof (entries[0])); }

const char* factoryPatternName (int i)
{
    return juce::isPositiveAndBelow (i, numFactoryPatterns()) ? entries[i].name : "";
}

Pattern makeFactoryPattern (int i)
{
    if (! juce::isPositiveAndBelow (i, numFactoryPatterns()))
        return {};
    auto p = entries[i].make();
    p.bars = 4;
    p.normalise();
    return p;
}

} // namespace bd::tools
