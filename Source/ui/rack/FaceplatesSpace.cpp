// TrapWin effect cards of the "space" group: Doubles, Swirl, Ad-Lib Throw, Aura Room, Wide Body,
// Chopped and Red Line (Docs/TRAPWIN-STYLE.md). Dark card, chrome italic name,
// one glowing glass screen as the live element. No rack hardware, no materials.
//
// Drawing is split in two layers:
//  - a static layer (card, name, preset pill, empty screen) rendered once per
//    type / size / pixel scale into an image cache, so the per-frame cost stays tiny;
//  - the live screen content, drawn every frame from the State snapshot.
// knobArea() is left calm on purpose: RackView places the real knob components on top of it.
#include "Faceplates.h"
#include "CardStyle.h"
#include "../../engine/effects/AllEffects.h"
#include <map>

namespace ek::faceplate
{
namespace
{
using namespace juce;
namespace cs = ek::card;

constexpr float twoPi = MathConstants<float>::twoPi;

uint32 hashU (uint32 x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
float h01 (uint32 a, uint32 b = 0) noexcept { return (float) (hashU (a * 0x9E3779B1u ^ hashU (b + 0x51ed27u)) & 0xffffffu) / 16777215.0f; }

float real (EffectType t, const State& s, int i) { return effectInfo (t).params[(size_t) i].toReal (s.params[(size_t) i]); }
String fmt (EffectType t, const State& s, int i) { const auto& p = effectInfo (t).params[(size_t) i]; return p.format (p.toReal (s.params[(size_t) i])); }

// animation clock that freezes when the user asked for reduced motion
double animTime (const State& s) { return s.reduceMotion ? 0.0 : s.timeSec; }

// big screen readout (UX pass: >= 12 px bold Plex Mono, accent-tinted with a soft glow; shrinks to fit
// down to 12 px, never below)
void readout (Graphics& g, const String& s, Rectangle<float> r, float h, Colour c, Justification j = Justification::centredLeft)
{
    h = jmax (12.0f, h);
    auto f = cs::labelFont (h, true, 0.3f);
    const float w = GlyphArrangement::getStringWidth (f, s);
    if (w > r.getWidth() && h > 12.0f)
        f = cs::labelFont (jmax (12.0f, h * r.getWidth() / w), true, 0.3f);
    cs::glowText (g, s, r, f, c, j, 0.8f);
}


// ---------------------------------------------------------------------------------------------
// per-effect style
// ---------------------------------------------------------------------------------------------
struct Style
{
    Colour accent;
    const char* name;
};

Style style (EffectType t)
{
    switch (t)
    {
        case EffectType::Doubles:    return { Colour (0xff3fd0e0), "Doubles" };
        case EffectType::Swirl:      return { Colour (0xffb07cff), "Swirl" };
        case EffectType::AdLibThrow: return { Colour (0xff9fc3ff), "Ad-Lib Throw" };
        case EffectType::AuraRoom:   return { Colour (0xffd6b8ff), "Aura Room" };
        case EffectType::WideBody:   return { Colour (0xff5fe0b0), "Wide Body" };
        case EffectType::Chopped:    return { Colour (0xff8be04e), "Chopped" };
        case EffectType::RedLine:    return { Colour (0xffff2e3e), "Red Line" };
        default:                     return { cs::ice, "" };
    }
}

// geometry ----------------------------------------------------------------------------------
float screenCorner (bool mini) { return mini ? 6.0f : 10.0f; }

Rectangle<float> miniScreen (Rectangle<float> plate) { return miniGlyphArea (plate); }

// ---------------------------------------------------------------------------------------------
// static layer
// ---------------------------------------------------------------------------------------------
void paintStatic (Graphics& g, EffectType t, Rectangle<float> plate, bool mini)
{
    const auto st = style (t);
    cs::paintCard (g, plate, st.accent, mini);

    if (mini)
    {
        // library row: accent dot, chrome name (>= 12 px), small live glyph - no sub-text
        const auto scr = miniScreen (plate);
        const Point<float> dot (plate.getX() + 12.0f, plate.getCentreY());
        cs::dotGlow (g, dot, 2.6f, st.accent, 1.0f);
        const auto nameBox = Rectangle<float> (plate.getX() + 21.0f, plate.getY(), scr.getX() - plate.getX() - 26.0f, plate.getHeight());
        cs::paintChromeTitle (g, st.name, nameBox, jlimit (12.0f, 17.0f, plate.getHeight() * 0.5f), 0.7f, 0.8f);
        cs::paintScreen (g, scr, screenCorner (true), st.accent);
        return;
    }

    const bool narrow = plate.getWidth() < compactBelow;
    cs::paintChromeTitle (g, st.name, nameArea (plate), narrow ? 18.0f : 22.0f, 1.0f, 0.8f);
    cs::paintPill (g, presetLcdArea (plate), st.accent);
    cs::paintScreen (g, liveArea (t, plate), screenCorner (false), st.accent);
}

// cache of the static layer, keyed by type / size / pixel scale / miniature
const Image& staticLayer (EffectType t, Rectangle<float> plate, float scale, bool mini)
{
    static std::map<int64, Image> cache;
    const int w = roundToInt (plate.getWidth()), h = roundToInt (plate.getHeight());
    const int sc = roundToInt (scale * 100.0f);
    const int64 key = ((int64) t << 48) ^ ((int64) w << 32) ^ ((int64) h << 16) ^ ((int64) sc << 1) ^ (mini ? 1 : 0);
    auto it = cache.find (key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 96) cache.clear();

    Image img (Image::ARGB, jmax (1, roundToInt ((float) w * scale)), jmax (1, roundToInt ((float) h * scale)), true);
    {
        Graphics ig (img);
        ig.addTransform (AffineTransform::scale (scale));
        paintStatic (ig, t, { 0.0f, 0.0f, (float) w, (float) h }, mini);
    }
    return cache.emplace (key, std::move (img)).first->second;
}

// ---------------------------------------------------------------------------------------------
// live screens
// ---------------------------------------------------------------------------------------------
// a faint grid on the glass (screen texture, very low contrast)
void screenGrid (Graphics& g, Rectangle<float> r, Colour c, float step)
{
    g.setColour (c.withAlpha (0.045f));
    for (float x = r.getX() + step; x < r.getRight() - 1.0f; x += step) g.fillRect (x, r.getY() + 2.0f, 0.6f, r.getHeight() - 4.0f);
    for (float y = r.getY() + step; y < r.getBottom() - 1.0f; y += step) g.fillRect (r.getX() + 2.0f, y, r.getWidth() - 4.0f, 0.6f);
}

void liveDoubles (Graphics& g, const State& s, Rectangle<float> w, bool mini, Colour ac)
{
    constexpr auto T = EffectType::Doubles;
    const int voices = jlimit (1, 4, roundToInt (real (T, s, 2)));
    const float rate = real (T, s, 0), depth = real (T, s, 1), spread = real (T, s, 3);
    const double tm = animTime (s);

    auto in = w.reduced (mini ? 5.0f : 10.0f, mini ? 3.0f : 7.0f);
    // voice dots, each breathing with its own LFO phase (full card: on the top row next to the readout)
    const float dr = mini ? 1.8f : 3.0f;
    auto dots = mini ? in.removeFromLeft (26.0f) : in.removeFromTop (16.0f);
    if (! mini)
    {
        readout (g, String (voices) + (voices == 1 ? " VOICE" : " VOICES"), dots, 13.0f, ac.brighter (0.35f));
        in.removeFromTop (2.0f);
    }
    for (int i = 0; i < 4; ++i)
    {
        const float cx = mini ? dots.getX() + 3.0f + (float) i * 6.5f : dots.getRight() - 4.0f - (float) (3 - i) * 12.0f;
        const float cy = dots.getCentreY();
        const bool on = i < voices;
        const float wob = on ? 0.72f + 0.28f * std::sin ((float) (tm * twoPi * rate) + (float) i * 1.7f) : 0.0f;
        cs::dotGlow (g, { cx, cy }, dr, ac, wob);
    }
    if (mini) in.removeFromLeft (3.0f);

    // overlapped voice traces: same wave, each voice drifting in phase/time like a real double
    g.setColour (ac.withAlpha (0.12f));
    g.fillRect (in.getX(), in.getCentreY(), in.getWidth(), 0.6f);
    for (int v = voices - 1; v >= 0; --v)
    {
        Path p;
        const float drift = (0.25f + 1.4f * depth) * std::sin ((float) (tm * twoPi * rate) + (float) v * twoPi / (float) voices);
        const float amp = in.getHeight() * (0.40f - 0.04f * (float) v) * (0.8f + 0.2f * spread);
        const int n = mini ? 40 : 90;
        for (int i = 0; i <= n; ++i)
        {
            const float u = (float) i / (float) n;
            const float ph = u * twoPi * 2.0f + drift + (float) v * 0.35f * spread;
            const float y = in.getCentreY() - amp * (0.8f * std::sin (ph) + 0.2f * std::sin (ph * 2.0f + 0.7f));
            const float x = in.getX() + u * in.getWidth();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        const float a = v == 0 ? 1.0f : 0.5f - 0.08f * (float) v;
        cs::strokeGlow (g, p, (v == 0 ? ac.brighter (0.25f) : ac).withAlpha (a), mini ? 0.8f : 1.2f, v == 0 ? 1.0f : 0.5f);
    }
}

void liveSwirl (Graphics& g, const State& s, Rectangle<float> w, bool mini, Colour ac)
{
    constexpr auto T = EffectType::Swirl;
    const bool sync = real (T, s, 1) > 0.5f;
    const float hz = sync ? (float) (s.bpm / 60.0) / jmax (0.0625f, syncBeats (real (T, s, 6))) : real (T, s, 0);
    const int stages = jlimit (2, 12, roundToInt (real (T, s, 2)));
    const float depth = real (T, s, 4), fb = real (T, s, 3);
    const int arms = jlimit (1, 6, stages / 2);

    // visual rotation speed follows the LFO rate (clamped so fast rates stay readable)
    const float revPerSec = jlimit (0.03f, 2.5f, hz);
    const float angle = (float) std::fmod (animTime (s) * (double) revPerSec, 1.0) * twoPi + 0.4f;

    const float side = w.getHeight() - (mini ? 4.0f : 8.0f);
    const auto port = Rectangle<float> (side, side).withCentre ({ w.getX() + (mini ? 4.0f : 8.0f) + side * 0.5f, w.getCentreY() });
    const auto c = port.getCentre();
    const float R = side * 0.5f;

    // faint rings
    for (int k = 1; k <= 3; ++k)
    {
        g.setColour (ac.withAlpha (0.08f));
        g.drawEllipse (Rectangle<float> (R * 2.0f * (float) k / 3.0f, R * 2.0f * (float) k / 3.0f).withCentre (c), 0.6f);
    }
    const float twist = 2.4f + 3.5f * depth;   // how tightly the arms wind
    for (int a = 0; a < arms; ++a)
    {
        Path p;
        const int n = 48;
        for (int i = 0; i <= n; ++i)
        {
            const float u = (float) i / (float) n;
            const float rr = R * (0.08f + 0.9f * u);
            const float th = angle + (float) a * twoPi / (float) arms + u * twist;
            const Point<float> pt (c.x + rr * std::cos (th), c.y + rr * std::sin (th));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        cs::strokeGlow (g, p, ac.brighter (0.2f), mini ? 0.8f : 1.3f);
    }
    cs::dotGlow (g, c, mini ? 1.4f : 2.2f, ac.brighter (0.5f), 1.0f);

    if (mini) return;   // library glyph: the spiral alone

    // right of the spiral: the rate as the big readout, stage pips, feedback bar
    auto info = w.withLeft (port.getRight() + 10.0f).reduced (0.0f, 8.0f).withTrimmedRight (8.0f);
    const float rateHz = real (T, s, 0);
    const String rateTxt = sync ? fmt (T, s, 6) : (rateHz < 10.0f ? String (rateHz, 2) + " Hz" : fmt (T, s, 0));
    readout (g, rateTxt, info.removeFromTop (17.0f), 14.0f, ac.brighter (0.35f), Justification::centredRight);
    info.removeFromTop (6.0f);
    auto pips = info.removeFromTop (8.0f);
    const float pitch = pips.getWidth() / 12.0f;
    for (int i = 0; i < 12; ++i)
        cs::dotGlow (g, { pips.getX() + pitch * ((float) i + 0.5f), pips.getCentreY() }, 1.7f, ac, i < stages ? 0.85f : 0.0f);
    info.removeFromTop (6.0f);
    auto fbRow = info.removeFromTop (6.0f);
    auto bar = fbRow.withSizeKeepingCentre (fbRow.getWidth(), 2.5f);
    g.setColour (ac.withAlpha (0.12f));
    g.fillRoundedRectangle (bar, 1.0f);
    Path fp; fp.startNewSubPath (bar.getX(), bar.getCentreY()); fp.lineTo (bar.getX() + jmax (1.0f, bar.getWidth() * fb), bar.getCentreY());
    cs::strokeGlow (g, fp, ac, 2.0f, 0.8f);
}

void liveAdLib (Graphics& g, const State& s, Rectangle<float> w, bool mini, Colour ac)
{
    constexpr auto T = EffectType::AdLibThrow;
    const float idx = real (T, s, 0);
    const float beats = syncBeats (idx);
    const bool synced = real (T, s, 1) > 0.5f;
    const float fb = real (T, s, 2);
    const bool ping = real (T, s, 3) > 0.5f;
    const double bpm = synced ? jmax (20.0, s.bpm) : 120.0;   // free time uses the engine's 120 BPM mapping
    const float ms = (float) (beats * 60.0 / bpm * 1000.0);
    const String msTxt = ms >= 1000.0f ? String (ms / 1000.0f, 2) + " s" : String (roundToInt (ms)) + " ms";
    const String big = synced ? fmt (T, s, 0) : msTxt;

    auto in = w.reduced (mini ? 5.0f : 10.0f, mini ? 2.0f : 6.0f);
    if (! mini)
    {
        // the delay time as the one big readout (note value when synced, ms when free)
        auto disp = in.removeFromLeft (jlimit (60.0f, 84.0f, in.getWidth() * 0.5f));
        readout (g, big, disp, 20.0f, ac.brighter (0.35f));
        in.removeFromLeft (6.0f);
    }

    // echo taps as lollipops: decaying by FEEDBACK; the newest throw lights up in tempo.
    // Ping-pong: taps alternate above (L) and below (R) the centre line.
    const int taps = mini ? 6 : 8;
    const double pos = s.reduceMotion ? 0.0 : s.beats / jmax (0.0625, (double) beats);
    const int cur = (int) std::floor (pos) % taps;
    const float frac = (float) (pos - std::floor (pos));
    const float base = ping ? in.getCentreY() : in.getBottom() - 1.0f;
    const float maxH = ping ? in.getHeight() * 0.5f - 2.0f : in.getHeight() - 3.0f;
    g.setColour (ac.withAlpha (0.18f));
    g.fillRect (in.getX(), base - 0.3f, in.getWidth(), 0.6f);
    const float tw = in.getWidth() / (float) taps;
    for (int k = 0; k < taps; ++k)
    {
        const float level = std::pow (jmax (0.05f, fb), (float) k) * 0.92f + 0.08f;
        const float dir = (ping && (k & 1)) ? 1.0f : -1.0f;
        const float x = in.getX() + tw * ((float) k + 0.5f);
        const float tip = base + dir * maxH * level;
        const float flash = (! s.reduceMotion && k == cur) ? (1.0f - frac) : 0.0f;
        Path stem; stem.startNewSubPath (x, base); stem.lineTo (x, tip);
        cs::strokeGlow (g, stem, ac.withAlpha (0.45f + 0.45f * level), mini ? 0.8f : 1.1f, 0.6f);
        cs::dotGlow (g, { x, tip }, mini ? 1.3f : 2.0f, ac.interpolatedWith (Colours::white, 0.5f * flash), 0.55f + 0.45f * jmax (level, flash));
    }
}

void liveAura (Graphics& g, const State& s, Rectangle<float> w, bool mini, Colour ac)
{
    constexpr auto T = EffectType::AuraRoom;
    const float decay = real (T, s, 2), pre = real (T, s, 3), size = real (T, s, 1), damp = real (T, s, 4);
    const int type = roundToInt (real (T, s, 0));
    const double tm = animTime (s);
    // decay 0.2..20 s on a log scale -> tail length 18..100 % of the window
    const float lenN = jlimit (0.0f, 1.0f, std::log (decay / 0.2f) / std::log (100.0f));
    auto in = w.reduced (mini ? 4.0f : 10.0f, mini ? 3.0f : 6.0f);
    auto head = Rectangle<float>();
    if (! mini) head = in.removeFromTop (16.0f);
    const float x0 = in.getX() + in.getWidth() * 0.10f * (pre / 200.0f) + 1.0f;
    const float len = (in.getRight() - x0) * (0.18f + 0.82f * lenN);
    const float base = in.getBottom();
    const float H = in.getHeight() * 0.92f;
    const float energy = 0.75f + 0.25f * jlimit (0.0f, 1.0f, s.outLevel * 2.0f);
    const Colour hot = type == 3 ? ac.darker (0.25f) : ac.brighter (0.3f);

    // sparse ✦ stars in the space above the tail (twinkle; frozen with reduceMotion)
    {
        const int stars = mini ? 4 : 8;
        for (int i = 0; i < stars; ++i)
        {
            const float sx = in.getX() + in.getWidth() * (0.08f + 0.9f * h01 (91u, (uint32) i));
            const float sy = in.getY() + in.getHeight() * (0.05f + 0.55f * h01 (92u, (uint32) i));
            const float tw = 0.5f + 0.5f * std::sin ((float) tm * (1.1f + h01 (93u, (uint32) i)) + (float) i * 2.3f);
            const float big = h01 (94u, (uint32) i);
            if (big > 0.62f) cs::paintSparkle (g, { sx, sy }, (mini ? 1.6f : 2.6f) + big * 1.2f, Colours::white.withAlpha (0.35f + 0.5f * tw), 0.6f);
            else
            {
                g.setColour (ac.withAlpha (0.25f + 0.45f * tw));
                g.fillEllipse (Rectangle<float> (1.1f, 1.1f).withCentre ({ sx, sy }));
            }
        }
    }

    // early reflections (count by SIZE)
    const int refl = 3 + roundToInt (size * 6.0f);
    for (int i = 0; i < refl; ++i)
    {
        const float u = (float) (i + 1) / (float) (refl + 2) * 0.22f;
        const float x = x0 + len * u;
        const float hgt = H * (0.95f - u * 2.2f) * (0.6f + 0.4f * h01 (77u, (uint32) i));
        g.setColour (hot.withAlpha (0.45f * energy));
        g.fillRect (x, base - hgt, mini ? 0.6f : 0.8f, hgt);
    }
    // tail envelope: soft gradient fill + glowing outline
    Path tail, edge;
    tail.startNewSubPath (x0, base);
    const int n = mini ? 30 : 70;
    const float k = 3.2f + 2.0f * damp;
    for (int i = 0; i <= n; ++i)
    {
        const float u = (float) i / (float) n;
        const float shimmer = s.reduceMotion ? 0.0f : 0.05f * std::sin ((float) (tm * 2.1) + u * 23.0f) * (1.0f - u);
        const float env = std::exp (-k * u) * (1.0f - std::exp (-u * 40.0f)) + shimmer;
        const Point<float> pt (x0 + len * u, base - H * jmax (0.0f, env) * 0.92f);
        tail.lineTo (pt);
        if (i == 0) edge.startNewSubPath (pt); else edge.lineTo (pt);
    }
    tail.lineTo (x0 + len, base);
    tail.closeSubPath();
    ColourGradient fill (ac.withAlpha (0.5f * energy), x0, base - H, ac.withAlpha (0.0f), x0, base, false);
    g.setGradientFill (fill);
    g.fillPath (tail);
    cs::strokeGlow (g, edge, hot.withAlpha (0.95f), mini ? 0.8f : 1.2f);

    // floor line
    g.setColour (ac.withAlpha (0.25f));
    g.fillRect (in.getX(), base, in.getWidth(), 0.6f);

    if (! mini)
    {
        // two readouts: the room type and the decay time
        static const char* types[] = { "ROOM", "PLATE", "HALL", "DARK" };
        readout (g, types[jlimit (0, 3, type)], head, 13.0f, ac.brighter (0.35f));
        readout (g, fmt (T, s, 2), head, 12.0f, ac.brighter (0.15f), Justification::centredRight);
    }
}

void liveWideBody (Graphics& g, const State& s, Rectangle<float> w, bool mini, Colour ac)
{
    constexpr auto T = EffectType::WideBody;
    const float width = real (T, s, 0);   // 0..2
    const float side = w.getHeight() - (mini ? 4.0f : 8.0f);
    const auto scr = Rectangle<float> (side, side).withCentre ({ w.getX() + (mini ? 4.0f : 8.0f) + side * 0.5f, w.getCentreY() });
    const auto c = scr.getCentre();
    const float R = scr.getWidth() * 0.47f;

    // graticule: M vertical, S horizontal, L / R diagonals, ring
    g.setColour (ac.withAlpha (0.14f));
    g.drawLine (c.x, scr.getY(), c.x, scr.getBottom(), 0.6f);
    g.drawLine (scr.getX(), c.y, scr.getRight(), c.y, 0.6f);
    g.setColour (ac.withAlpha (0.08f));
    g.drawLine (c.x - R * 0.7f, c.y - R * 0.7f, c.x + R * 0.7f, c.y + R * 0.7f, 0.5f);
    g.drawLine (c.x + R * 0.7f, c.y - R * 0.7f, c.x - R * 0.7f, c.y + R * 0.7f, 0.5f);
    g.drawEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 0.6f);

    // points: (S, M) plot; real scope data when available, otherwise an animated synthetic signal
    double corrNum = 0.0, eL = 0.0, eR = 0.0;
    const float ps = mini ? 0.9f : 1.2f;
    auto plot = [&] (float l, float r, float alpha)
    {
        const float x = c.x + (r - l) * 0.7071f * R;
        const float y = c.y - (l + r) * 0.7071f * R;
        g.setColour (ac.withAlpha (alpha * 0.18f));
        g.fillEllipse (x - ps * 1.5f, y - ps * 1.5f, ps * 3.0f, ps * 3.0f);
        g.setColour (ac.brighter (0.4f).withAlpha (alpha));
        g.fillRect (x - ps * 0.5f, y - ps * 0.5f, ps, ps);
        corrNum += (double) l * r; eL += (double) l * l; eR += (double) r * r;
    };
    g.saveState();
    g.reduceClipRegion (scr.toNearestInt());
    if (s.scopeL != nullptr && s.scopeR != nullptr && s.scopeN > 1)
    {
        const int step = jmax (1, s.scopeN / (mini ? 160 : 400));
        for (int i = 0; i < s.scopeN; i += step)
            plot (jlimit (-1.4f, 1.4f, s.scopeL[i]), jlimit (-1.4f, 1.4f, s.scopeR[i]), 0.7f);
    }
    else
    {
        const double t0 = animTime (s);
        const int n = mini ? 140 : 360;
        const float sd0 = jlimit (0.0f, 1.6f, width * 0.55f);   // 0 = mono line, 1.1 at 200 %
        for (int i = 0; i < n; ++i)
        {
            const float u = (float) i / (float) n;
            const float tt = (float) t0 * 0.8f + u * 9.0f;
            const float m = 0.55f * std::sin (tt * 3.1f) + 0.25f * std::sin (tt * 7.3f + 1.0f) + 0.08f * std::sin (tt * 17.0f);
            const float sd = sd0 * (0.35f * std::sin (tt * 4.7f + 0.5f) + 0.18f * std::sin (tt * 11.3f));
            plot (0.7071f * (m - sd), 0.7071f * (m + sd), 0.3f + 0.6f * u);
        }
    }
    ColourGradient bloom (ac.withAlpha (0.10f), c.x, c.y, ac.withAlpha (0.0f), c.x + R, c.y, true);
    g.setGradientFill (bloom);
    g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    g.restoreState();

    if (mini) return;   // library glyph: the scope alone

    // right of the scope: WIDTH as the big readout, the correlation meter (-1 .. +1) under it
    auto info = w.withLeft (scr.getRight() + 10.0f).withTrimmedRight (8.0f).reduced (0.0f, 9.0f);
    const float corr = (eL > 1e-9 && eR > 1e-9) ? (float) (corrNum / std::sqrt (eL * eR)) : 1.0f;
    readout (g, fmt (T, s, 0), info.removeFromTop (18.0f), 15.0f, ac.brighter (0.35f), Justification::centredRight);
    info.removeFromTop (10.0f);
    auto meter = info.removeFromTop (10.0f);
    auto track = meter.withSizeKeepingCentre (meter.getWidth() - 6.0f, 2.5f);
    g.setColour (ac.withAlpha (0.14f));
    g.fillRoundedRectangle (track, 1.0f);
    g.setColour (ac.withAlpha (0.35f));
    g.fillRect (track.getCentreX() - 0.3f, track.getY() - 2.0f, 0.6f, 6.0f);
    const float nx = track.getX() + track.getWidth() * (0.5f + 0.5f * jlimit (-1.0f, 1.0f, corr));
    cs::dotGlow (g, { nx, track.getCentreY() }, 2.2f, corr < 0.0f ? Colour (0xffff4d5e) : ac, 1.0f);
}

void liveChopped (Graphics& g, const State& s, Rectangle<float> w, bool mini, Colour ac)
{
    constexpr auto T = EffectType::Chopped;
    const int mode = roundToInt (real (T, s, 0));
    const int patIdx = roundToInt (real (T, s, 1));
    const float stepBeats = jmax (0.0625f, syncBeats (real (T, s, 2)));
    const float depth = real (T, s, 3);
    const uint16 pat = choppedPattern (patIdx);
    const int cur = s.reduceMotion ? -1 : (int) std::floor (s.beats / (double) stepBeats) % 16;
    const Colour warm (0xffffd23f);

    auto in = w.reduced (mini ? 5.0f : 10.0f, mini ? 2.0f : 6.0f);
    if (! mini)
    {
        // mode (+ pattern number) left, step rate right - 12 px minimum
        static const char* modes[] = { "PATTERN", "PUMP", "GATE" };
        auto top = in.removeFromTop (16.0f);
        String head = modes[jlimit (0, 2, mode)];
        if (mode == 0) head = "PAT " + String (patIdx + 1).paddedLeft ('0', 2);
        readout (g, head, top, 13.0f, ac.brighter (0.35f));
        if (mode != 2) readout (g, fmt (T, s, 2), top, 12.0f, ac.brighter (0.15f), Justification::centredRight);
        in.removeFromTop (4.0f);
    }

    // noise-gate mode: the row turns into an input meter with a threshold marker
    const float thrDb = real (T, s, 5);
    const float inDb = s.inLevel > 0.0f ? Decibels::gainToDecibels (s.inLevel, -80.0f) : -80.0f;

    const float groupGap = mini ? 2.0f : 5.0f;
    const float pitch = (in.getWidth() - 3.0f * groupGap) / 16.0f;
    const float dr = mini ? jmin (1.7f, pitch * 0.28f) : 3.3f;
    const float cy = mini ? in.getCentreY() : in.getY() + jmin (12.0f, in.getHeight() * 0.4f);
    auto cxOf = [&] (int i) { return in.getX() + pitch * ((float) i + 0.5f) + (float) (i / 4) * groupGap; };
    for (int i = 0; i < 16; ++i)
    {
        const float cx = cxOf (i);
        float level = 0.0f;
        Colour col = ac;
        if (mode == 0)
        {
            const bool on = (pat >> (15 - i)) & 1;
            level = on ? 0.7f : 0.0f;
            if (i == cur) { level = on ? 1.0f : 0.0f; col = on ? ac.interpolatedWith (Colours::white, 0.45f) : ac; }
        }
        else if (mode == 1)
        {
            // pump: each beat ducks then swells back
            const float stepsPerBeat = jmax (1.0f, 1.0f / stepBeats);
            const float ph = std::fmod ((float) i / stepsPerBeat, 1.0f);
            level = 0.1f + 0.75f * ph * depth + 0.15f * (1.0f - depth);
            if (i == cur) level = jmin (1.0f, level + 0.35f);
        }
        else
        {
            const float db = -80.0f + 80.0f * (float) i / 15.0f;
            level = inDb >= db ? 0.9f : 0.0f;
            col = db >= thrDb ? ac : warm;
        }
        cs::dotGlow (g, { cx, cy }, dr, col, level);
        if (i == cur)
        {
            // playhead ring
            g.setColour (Colours::white.withAlpha (0.55f));
            g.drawEllipse (Rectangle<float> (dr * 3.4f, dr * 3.4f).withCentre ({ cx, cy }), mini ? 0.6f : 0.8f);
        }
    }
    if (! mini)
    {
        for (int b = 0; b < 4; ++b)
        {
            const float x0 = cxOf (b * 4) - dr, x1 = cxOf (b * 4 + 3) + dr;
            g.setColour (ac.withAlpha (b == (cur / 4) ? 0.35f : 0.14f));
            g.fillRect (x0, cy + 9.0f, x1 - x0, 0.8f);
        }
    }
    if (mode == 2 && ! mini)
    {
        const float tx = in.getX() + (in.getWidth() - 3.0f * groupGap) * (0.5f + 15.0f * (thrDb + 80.0f) / 80.0f) / 16.0f
                         + groupGap * 1.5f;
        g.setColour (warm.withAlpha (0.9f));
        Path tri; tri.addTriangle (tx - 3.0f, cy - 9.0f, tx + 3.0f, cy - 9.0f, tx, cy - 5.5f);
        g.fillPath (tri);
    }
}

void liveRedLine (Graphics& g, const State& s, Rectangle<float> w, bool mini, Colour ac)
{
    constexpr auto T = EffectType::RedLine;
    const float gr = jmax (0.0f, s.gainReductionDb);
    const Colour amber (0xffff8a3d);
    auto in = w.reduced (mini ? 5.0f : 10.0f, mini ? 3.0f : 6.0f);

    if (! mini)
    {
        // the gain reduction as the big readout; the ceiling and LOOKAHEAD are on the knob row
        auto top = in.removeFromTop (18.0f);
        readout (g, "-" + String (gr, 1) + " dB", top, 15.0f, ac.brighter (0.3f), Justification::centredLeft);
        readout (g, "GR", top, 12.0f, ac.brighter (0.1f), Justification::centredRight);
        in.removeFromTop (8.0f);
    }

    // limiter bar: 0..12 dB of gain reduction, glowing fill from the left
    static const float marks[] = { 0.0f, 1.0f, 2.0f, 3.0f, 6.0f, 9.0f, 12.0f };
    auto bar = mini ? in.withTrimmedLeft (10.0f).withSizeKeepingCentre (in.getWidth() - 10.0f, 5.0f)
                    : in.removeFromTop (8.0f);
    if (mini) bar.setX (in.getX() + 10.0f);
    const float frac = jlimit (0.0f, 1.0f, gr / 12.0f);
    g.setColour (ac.withAlpha (0.10f));
    g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
    g.setColour (cs::line);
    g.drawRoundedRectangle (bar, bar.getHeight() * 0.5f, 0.6f);
    if (frac > 0.0f)
    {
        auto f = bar.withWidth (jmax (bar.getHeight(), bar.getWidth() * frac));
        g.setColour (ac.withAlpha (0.18f));
        g.fillRoundedRectangle (f.expanded (2.5f), bar.getHeight());
        ColourGradient fill (amber, f.getX(), 0.0f, ac, bar.getX() + bar.getWidth() * 0.4f, 0.0f, false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (f, bar.getHeight() * 0.5f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.fillRoundedRectangle (f.withHeight (f.getHeight() * 0.35f).reduced (1.0f, 0.0f), 1.0f);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.fillRect (f.getRight() - 1.2f, f.getY() - 1.0f, 1.2f, f.getHeight() + 2.0f);   // hot tip
    }
    if (mini)
    {
        // LIMIT dot
        cs::dotGlow (g, { in.getX() + 3.0f, bar.getCentreY() }, 1.8f, ac, gr > 0.3f ? jmin (1.0f, 0.5f + gr / 6.0f) : 0.0f);
        return;
    }

    // scale ticks only (0 1 2 3 6 9 12 dB) - no numbers
    auto scale = in.removeFromTop (8.0f);
    for (float m : marks)
    {
        const float x = bar.getX() + bar.getWidth() * m / 12.0f;
        const bool major = roundToInt (m) % 6 == 0;   // 0 / 6 / 12 dB
        g.setColour (cs::ice.withAlpha (major ? 0.35f : 0.18f));
        g.fillRect (x - 0.35f, scale.getY() + 2.0f, 0.7f, major ? 4.0f : 2.5f);
    }
}

void paintLive (Graphics& g, EffectType t, const State& s, Rectangle<float> scr, bool mini)
{
    const auto ac = style (t).accent;
    const float corner = screenCorner (mini);
    Path clip;
    clip.addRoundedRectangle (scr.reduced (0.8f), corner - 0.8f);
    g.saveState();
    g.reduceClipRegion (clip);
    if (! mini) screenGrid (g, scr, ac, 10.0f);
    switch (t)
    {
        case EffectType::Doubles:    liveDoubles (g, s, scr, mini, ac); break;
        case EffectType::Swirl:      liveSwirl (g, s, scr, mini, ac); break;
        case EffectType::AdLibThrow: liveAdLib (g, s, scr, mini, ac); break;
        case EffectType::AuraRoom:   liveAura (g, s, scr, mini, ac); break;
        case EffectType::WideBody:   liveWideBody (g, s, scr, mini, ac); break;
        case EffectType::Chopped:    liveChopped (g, s, scr, mini, ac); break;
        case EffectType::RedLine:    liveRedLine (g, s, scr, mini, ac); break;
        default: break;
    }
    g.restoreState();
    cs::paintScreenGlass (g, scr, corner);
}
} // namespace

Palette paletteSpaceGroup (EffectType t)
{
    const auto st = style (t);
    // plate / plateDark / ink (labels) / accent (value arcs) / led
    return { cs::card, cs::screen, cs::label, st.accent, st.accent.brighter (0.25f) };
}

void paintSpaceGroup (Graphics& g, EffectType t, Rectangle<float> plate, const State& s, bool miniature)
{
    const float scale = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto& img = staticLayer (t, plate, scale, miniature);
    g.setOpacity (1.0f);   // drawImage uses the current colour's alpha
    g.drawImage (img, plate);
    paintLive (g, t, s, miniature ? miniScreen (plate) : liveArea (t, plate), miniature);
}
} // namespace ek::faceplate
