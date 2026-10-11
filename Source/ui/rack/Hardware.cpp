#include "Hardware.h"
#include "../../engine/Preset.h"

namespace ek::ui::hw
{
using namespace juce;

namespace
{
// small deterministic hash noise (0..1)
inline float hash01 (int a, int b = 0, int c = 0)
{
    uint32 h = (uint32) a * 374761393u + (uint32) b * 668265263u + (uint32) c * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffffff) / (float) 0xffffff;
}

inline Point<float> polar (Point<float> c, float r, float angle)
{
    return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
}

Font labelFont (float h) { return theme::monoLabel (h); }

// ---- reduce-motion preference ---------------------------------------------------------------
File settingsFile() { return PresetBank::getUserPresetDir().getParentDirectory().getChildFile ("ui-settings.xml"); }

std::atomic<int>& motionFlag()
{
    static std::atomic<int> flag { -1 };   // -1 = not loaded yet
    return flag;
}
} // namespace

bool reduceMotion()
{
    auto& f = motionFlag();
    int v = f.load();
    if (v < 0)
    {
        v = 0;
        if (auto xml = parseXML (settingsFile())) v = xml->getBoolAttribute ("reduceMotion", false) ? 1 : 0;
        f.store (v);
    }
    return v == 1;
}

void setReduceMotion (bool on)
{
    motionFlag().store (on ? 1 : 0);
    XmlElement xml ("TRAPWIN_UI");
    if (auto old = parseXML (settingsFile())) xml = *old;
    xml.setAttribute ("reduceMotion", on);
    auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
}

double animSeconds() { return Time::getMillisecondCounterHiRes() * 0.001; }

// ============================================================================================
// materials
void drawBrushedMetal (Graphics& g, Rectangle<float> r, Colour base, float corner, int seed)
{
    Graphics::ScopedSaveState s (g);
    Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);

    ColourGradient fill (base.brighter (0.10f), r.getX(), r.getY(), base.darker (0.22f), r.getX(), r.getBottom(), false);
    fill.addColour (0.45, base);
    g.setGradientFill (fill);
    g.fillRect (r);

    // brushing: one line per pixel row with random brightness, plus a few longer streaks
    const int rows = (int) std::ceil (r.getHeight());
    for (int i = 0; i < rows; ++i)
    {
        const float y = r.getY() + (float) i + 0.5f;
        const float n = hash01 (i, seed);
        const bool light = n > 0.5f;
        const float a = light ? (n - 0.5f) * 0.075f : (0.5f - n) * 0.11f;
        g.setColour ((light ? Colours::white : Colours::black).withAlpha (a));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        // streak
        const float sn = hash01 (i, seed, 7);
        if (sn > 0.82f)
        {
            const float x0 = r.getX() + hash01 (i, seed, 11) * r.getWidth();
            const float len = 40.0f + hash01 (i, seed, 13) * r.getWidth() * 0.5f;
            g.setColour (Colours::white.withAlpha (0.035f));
            g.drawHorizontalLine ((int) y, x0, jmin (r.getRight(), x0 + len));
        }
    }
    // soft diagonal sheen
    ColourGradient sheen (Colours::white.withAlpha (0.05f), r.getX() + r.getWidth() * 0.25f, r.getY(),
                          Colours::transparentWhite, r.getX() + r.getWidth() * 0.45f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRect (r);
}

void drawBevel (Graphics& g, Rectangle<float> r, float corner, float k)
{
    g.setColour (Colours::black.withAlpha (0.85f * k));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    // top / left light, bottom / right dark
    g.setColour (Colours::white.withAlpha (0.16f * k));
    g.drawLine (r.getX() + corner + 1.0f, r.getY() + 1.5f, r.getRight() - corner - 1.0f, r.getY() + 1.5f, 1.0f);
    g.setColour (Colours::white.withAlpha (0.07f * k));
    g.drawLine (r.getX() + 1.5f, r.getY() + corner + 1.0f, r.getX() + 1.5f, r.getBottom() - corner - 1.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.45f * k));
    g.drawLine (r.getX() + corner + 1.0f, r.getBottom() - 1.5f, r.getRight() - corner - 1.0f, r.getBottom() - 1.5f, 1.0f);
    g.drawLine (r.getRight() - 1.5f, r.getY() + corner + 1.0f, r.getRight() - 1.5f, r.getBottom() - corner - 1.0f, 1.0f);
}

void drawWalnut (Graphics& g, Rectangle<float> r, bool vertical, int seed)
{
    if (r.isEmpty()) return;
    Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r.toNearestIntEdges());

    // work in a frame where the grain runs along +x: len = along the grain, thick = across
    const float len = vertical ? r.getHeight() : r.getWidth();
    const float thick = vertical ? r.getWidth() : r.getHeight();
    const auto toWorld = vertical ? AffineTransform (0.0f, 1.0f, r.getX(), 1.0f, 0.0f, r.getY())   // (u,v) -> (x = v, y = u)
                                  : AffineTransform::translation (r.getX(), r.getY());

    // base: board shading across the thickness
    {
        const auto p0 = Point<float> (0.0f, 0.0f).transformedBy (toWorld);
        const auto p1 = Point<float> (0.0f, thick).transformedBy (toWorld);
        ColourGradient base (col::walnutLight.brighter (0.06f), p0, col::walnutDark.darker (0.35f), p1, false);
        base.addColour (0.18, col::walnutLight);
        base.addColour (0.6, col::walnutDark.interpolatedWith (col::walnutLight, 0.35f));
        g.setGradientFill (base);
        g.fillRect (r);
    }

    // grain lines
    const int lines = jmax (6, (int) (thick * 1.6f));
    for (int i = 0; i < lines; ++i)
    {
        const float v0 = (float) i / (float) lines * thick + hash01 (i, seed) * 1.2f;
        const float ph1 = hash01 (i, seed, 1) * MathConstants<float>::twoPi;
        const float ph2 = hash01 (i, seed, 2) * MathConstants<float>::twoPi;
        const float amp = 0.6f + hash01 (i, seed, 3) * 1.6f;
        Path p;
        for (float u = -4.0f; u <= len + 4.0f; u += 5.0f)
        {
            // slow waves + a "knot" bulge that all lines share
            const float knot = 3.2f * std::exp (-std::pow ((u - len * 0.37f) / (len * 0.06f + 20.0f), 2.0f));
            const float v = v0 + amp * std::sin (u * 0.011f + ph1) + 0.5f * std::sin (u * 0.047f + ph2)
                            + knot * std::sin ((v0 / thick) * MathConstants<float>::pi);
            const auto w = Point<float> (u, v).transformedBy (toWorld);
            if (u <= -4.0f) p.startNewSubPath (w); else p.lineTo (w);
        }
        const float n = hash01 (i, seed, 4);
        const Colour c = n > 0.7f ? Colour (0xff7a5233).withAlpha (0.22f) : Colour (0xff1c0f07).withAlpha (0.18f + n * 0.25f);
        g.setColour (c);
        g.strokePath (p, PathStrokeType (0.6f + hash01 (i, seed, 5) * 0.9f));
    }

    // pores
    const int pores = (int) (len * thick / 90.0f);
    g.setColour (Colour (0xff120904).withAlpha (0.35f));
    for (int i = 0; i < pores; ++i)
    {
        const float u = hash01 (i, seed, 21) * len, v = hash01 (i, seed, 22) * thick;
        const float l = 1.5f + hash01 (i, seed, 23) * 3.0f;
        const auto a = Point<float> (u, v).transformedBy (toWorld), b = Point<float> (u + l, v).transformedBy (toWorld);
        g.drawLine ({ a, b }, 0.7f);
    }

    // satin finish: soft gloss band near the "top" edge, dark edges
    {
        const auto e0 = Point<float> (0.0f, 0.0f).transformedBy (toWorld);
        const auto e1 = Point<float> (0.0f, thick).transformedBy (toWorld);
        ColourGradient gloss (Colours::white.withAlpha (0.10f), e0, Colours::transparentWhite, e0 + (e1 - e0) * 0.45f, false);
        g.setGradientFill (gloss);
        g.fillRect (r);
    }
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawRect (r, 1.0f);
}

void drawScrew (Graphics& g, Point<float> c, float radius, float angle)
{
    const auto disc = [c] (float rr) { return Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c); };
    // countersink shadow
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (disc (radius * 1.22f).translated (0.0f, radius * 0.1f));
    // chrome head
    ColourGradient head (Colour (0xfff4f6fa), c.x - radius * 0.45f, c.y - radius * 0.55f,
                         Colour (0xff4a505a), c.x + radius * 0.7f, c.y + radius * 0.8f, true);
    head.addColour (0.55, Colour (0xffa9b0bc));
    g.setGradientFill (head);
    g.fillEllipse (disc (radius));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (disc (radius), jmax (0.6f, radius * 0.08f));
    // cross (Phillips)
    const float w = radius * 0.26f, l = radius * 1.25f;
    for (int k = 0; k < 2; ++k)
    {
        Path slot;
        slot.addRoundedRectangle (-l * 0.5f, -w * 0.5f, l, w, w * 0.4f);
        const auto t = AffineTransform::rotation (angle + (float) k * MathConstants<float>::halfPi).translated (c);
        g.setColour (Colour (0xff15171b));
        g.fillPath (slot, t);
    }
    // highlight edge of the slots
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawLine ({ polar (c, l * 0.45f, angle + MathConstants<float>::halfPi + 0.25f),
                  polar (c, l * 0.18f, angle + MathConstants<float>::halfPi + 0.6f) }, 0.6f);
    // specular
    g.setColour (Colours::white.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (radius * 0.5f, radius * 0.32f).withCentre (c.translated (-radius * 0.35f, -radius * 0.5f)));
}

void drawRackEar (Graphics& g, Rectangle<float> ear, Colour base, bool withScrews, int seed)
{
    drawBrushedMetal (g, ear, base.brighter (0.04f), 0.0f, seed);
    const float hw = ear.getWidth() * 0.62f, hh = jmin (ear.getWidth() * 0.36f, 9.0f);
    const float inset = jmin (ear.getHeight() * 0.2f, 22.0f);
    for (float cy : { ear.getY() + inset, ear.getBottom() - inset })
    {
        const auto hole = Rectangle<float> (hw, hh).withCentre ({ ear.getCentreX(), cy });
        g.setColour (Colour (0xff030405));
        g.fillRoundedRectangle (hole, hh * 0.5f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawLine (hole.getX() + hh * 0.5f, hole.getBottom() + 0.5f, hole.getRight() - hh * 0.5f, hole.getBottom() + 0.5f, 0.8f);
        if (withScrews) drawScrew (g, hole.getCentre().translated (hw * 0.12f, 0.0f), hh * 0.62f, 0.3f + hash01 (seed, (int) cy) * 1.2f);
    }
}

Rectangle<float> drawRackPanel (Graphics& g, Rectangle<float> r, Colour base, float earWidth, int seed)
{
    // drop shadow onto the shaft
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRect (r.translated (0.0f, 2.0f).expanded (1.0f, 0.0f));
    auto body = r;
    const auto left = body.removeFromLeft (earWidth);
    const auto right = body.removeFromRight (earWidth);
    drawBrushedMetal (g, body, base, 0.0f, seed);
    drawRackEar (g, left, base, true, seed + 1);
    drawRackEar (g, right, base, true, seed + 2);
    // fold lines between ear and body
    for (float x : { left.getRight(), right.getX() })
    {
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawVerticalLine ((int) x + 1, r.getY(), r.getBottom());
    }
    drawBevel (g, r, 1.5f);
    return body;
}

void drawRackRail (Graphics& g, Rectangle<float> rail, float pitch)
{
    ColourGradient steel (Colour (0xff2c3038), rail.getX(), 0.0f, Colour (0xff15171c), rail.getRight(), 0.0f, false);
    steel.addColour (0.3, Colour (0xff3a3f48));
    g.setGradientFill (steel);
    g.fillRect (rail);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawVerticalLine ((int) rail.getX(), rail.getY(), rail.getBottom());
    g.drawVerticalLine ((int) rail.getRight() - 1, rail.getY(), rail.getBottom());
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawVerticalLine ((int) rail.getX() + 1, rail.getY(), rail.getBottom());

    const float hs = jmin (5.0f, rail.getWidth() * 0.42f);
    int i = 0;
    for (float y = rail.getY() + pitch * 0.5f; y < rail.getBottom() - hs; y += pitch, ++i)
    {
        // EIA pattern: holes in groups of three
        if (i % 3 == 1) continue;
        auto hole = Rectangle<float> (hs, hs).withCentre ({ rail.getCentreX(), y });
        g.setColour (Colour (0xff020304));
        g.fillRoundedRectangle (hole, 1.0f);
        g.setColour (Colours::white.withAlpha (0.14f));
        g.drawLine (hole.getX(), hole.getBottom() + 0.5f, hole.getRight(), hole.getBottom() + 0.5f, 0.7f);
    }
}

// ============================================================================================
// displays
void drawLcdGlass (Graphics& g, Rectangle<float> r, float corner)
{
    // bezel
    g.setColour (Colours::black);
    g.fillRoundedRectangle (r.expanded (1.0f), corner + 1.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawLine (r.getX() + corner, r.getBottom() + 1.0f, r.getRight() - corner, r.getBottom() + 1.0f, 0.8f);
    ColourGradient glass (Colour (0xff060a10), r.getX(), r.getY(), Colour (0xff010203), r.getX(), r.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (r, corner);
    // inner shadow
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.2f);
}

void drawGlassReflection (Graphics& g, Rectangle<float> r, float corner)
{
    Graphics::ScopedSaveState s (g);
    Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    Path refl;
    refl.startNewSubPath (r.getX(), r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
    refl.lineTo (r.getX() + r.getWidth() * 0.48f, r.getY() + r.getHeight() * 0.42f);
    refl.lineTo (r.getX(), r.getY() + r.getHeight() * 0.42f);
    refl.closeSubPath();
    ColourGradient gl (Colours::white.withAlpha (0.07f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.42f, false);
    g.setGradientFill (gl);
    g.fillPath (refl);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
}

namespace
{
int segMask (juce_wchar ch)
{
    switch (ch)
    {
        case '0': case 'O': return 0x3F;
        case '1': case 'I': return 0x06;
        case '2': return 0x5B;
        case '3': return 0x4F;
        case '4': return 0x66;
        case '5': case 'S': case 's': return 0x6D;
        case '6': return 0x7D;
        case '7': return 0x07;
        case '8': return 0x7F;
        case '9': return 0x6F;
        case '-': return 0x40;
        case '_': return 0x08;
        case 'A': case 'a': return 0x77;
        case 'b': case 'B': return 0x7C;
        case 'C': return 0x39;
        case 'c': return 0x58;
        case 'd': case 'D': return 0x5E;
        case 'E': case 'e': return 0x79;
        case 'F': case 'f': return 0x71;
        case 'H': return 0x76;
        case 'h': return 0x74;
        case 'L': case 'l': return 0x38;
        case 'n': case 'N': return 0x54;
        case 'o': return 0x5C;
        case 'P': case 'p': return 0x73;
        case 'r': case 'R': return 0x50;
        case 't': case 'T': return 0x78;
        case 'U': return 0x3E;
        case 'u': return 0x1C;
        case 'y': case 'Y': return 0x6E;
        default: return 0;
    }
}

// the seven segments of one cell (a..g) in local coords, skewed
std::array<Path, 7> segmentPaths (float x, float y, float w, float h, float t, float skew)
{
    const float gp = t * 0.14f, ht = t * 0.5f;
    auto sk = [=] (float px, float py) { return Point<float> (x + px + (h - py) * skew, y + py); };
    auto horiz = [&] (float yc)
    {
        const float x0 = ht + gp, x1 = w - ht - gp;
        Path p;
        p.startNewSubPath (sk (x0, yc));
        p.lineTo (sk (x0 + ht, yc - ht)); p.lineTo (sk (x1 - ht, yc - ht)); p.lineTo (sk (x1, yc));
        p.lineTo (sk (x1 - ht, yc + ht)); p.lineTo (sk (x0 + ht, yc + ht));
        p.closeSubPath();
        return p;
    };
    auto vert = [&] (float xc, float y0, float y1)
    {
        Path p;
        p.startNewSubPath (sk (xc, y0));
        p.lineTo (sk (xc + ht, y0 + ht)); p.lineTo (sk (xc + ht, y1 - ht)); p.lineTo (sk (xc, y1));
        p.lineTo (sk (xc - ht, y1 - ht)); p.lineTo (sk (xc - ht, y0 + ht));
        p.closeSubPath();
        return p;
    };
    const float m = h * 0.5f;
    return { horiz (ht),                                  // a
             vert (w - ht, ht + gp, m - gp),              // b
             vert (w - ht, m + gp, h - ht - gp),          // c
             horiz (h - ht),                              // d
             vert (ht, m + gp, h - ht - gp),              // e
             vert (ht, ht + gp, m - gp),                  // f
             horiz (m) };                                 // g
}
} // namespace

void drawSevenSegment (Graphics& g, Rectangle<float> r, const String& text, Colour on, Colour off, int cells, float glow)
{
    struct Cell { int mask = 0; bool dp = false; };
    std::vector<Cell> cs;
    for (auto ch : text)
    {
        if (ch == '.' || ch == ',') { if (cs.empty()) cs.push_back ({}); cs.back().dp = true; continue; }
        cs.push_back ({ segMask (ch), false });
    }
    while ((int) cs.size() < cells) cs.insert (cs.begin(), Cell {});
    if (cs.empty()) return;

    const int n = (int) cs.size();
    const float skew = 0.07f;
    float h = r.getHeight();
    float w = h * 0.5f, gap = h * 0.2f;
    float total = (float) n * w + (float) (n - 1) * gap + h * skew;
    if (total > r.getWidth()) { const float k = r.getWidth() / total; h *= k; w *= k; gap *= k; total = r.getWidth(); }
    const float t = jmax (1.2f, h * 0.13f);
    float x = r.getRight() - total;
    const float y = r.getCentreY() - h * 0.5f;

    Path lit, unlit;
    for (const auto& c : cs)
    {
        auto segs = segmentPaths (x, y, w, h, t, skew);
        for (int s = 0; s < 7; ++s) ((c.mask >> s) & 1 ? lit : unlit).addPath (segs[(size_t) s]);
        const auto dpRect = Rectangle<float> (t, t).withPosition (x + w + gap * 0.5f - t * 0.5f, y + h - t);
        (c.dp ? lit : unlit).addEllipse (dpRect);
        x += w + gap;
    }
    g.setColour (off.withMultipliedAlpha (0.8f));
    g.fillPath (unlit);
    if (glow > 0.0f)
    {
        g.setColour (on.withAlpha (0.10f * glow));
        g.strokePath (lit, PathStrokeType (t * 1.6f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (on.withAlpha (0.22f * glow));
        g.strokePath (lit, PathStrokeType (t * 0.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (on);
    g.fillPath (lit);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.strokePath (lit, PathStrokeType (0.5f));
}

void drawSectionHeader (Graphics& g, Rectangle<float> r, const String& label)
{
    ColourGradient strip (Colours::white.withAlpha (0.10f), r.getX(), r.getY(), Colours::white.withAlpha (0.04f), r.getX(), r.getBottom(), false);
    g.setGradientFill (strip);
    g.fillRect (r);
    g.setColour (col::ice.withAlpha (0.9f));
    g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    g.setColour (col::ice.withAlpha (0.18f));
    g.fillRect (r.getX(), r.getBottom(), r.getWidth(), 2.0f);
    g.setColour (col::ice);
    g.fillRect (Rectangle<float> (3.0f, r.getHeight() - 6.0f).withPosition (r.getX() + 5.0f, r.getY() + 2.5f));
    g.setFont (labelFont (jmin (10.5f, r.getHeight() * 0.72f)).boldened());
    g.setColour (col::print);
    g.drawText (label, r.withTrimmedLeft (14.0f).withTrimmedBottom (1.0f), Justification::centredLeft, false);
}

void drawPrint (Graphics& g, const String& text, Rectangle<float> r, Justification j, float size, Colour c)
{
    g.setFont (labelFont (size));
    g.setColour (c);
    g.drawText (text, r, j, false);
}

// ============================================================================================
// buttons
void drawMetalCap (Graphics& g, Rectangle<float> r, bool pressed, bool hover, float corner)
{
    // socket
    g.setColour (Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (r, corner + 1.0f);
    auto cap = r.reduced (1.0f);
    if (pressed) cap = cap.translated (0.0f, 0.8f);
    const float lift = hover ? 0.06f : 0.0f;
    ColourGradient body (Colour (0xff42464f).brighter (lift), cap.getX(), cap.getY(),
                         Colour (0xff1a1c21).brighter (lift), cap.getX(), cap.getBottom(), false);
    if (pressed) body = ColourGradient (Colour (0xff1c1e23), cap.getX(), cap.getY(), Colour (0xff30333a), cap.getX(), cap.getBottom(), false);
    else body.addColour (0.5, Colour (0xff2d3037).brighter (lift));
    g.setGradientFill (body);
    g.fillRoundedRectangle (cap, corner);
    if (! pressed)
    {
        g.setColour (Colours::white.withAlpha (0.22f));
        g.drawLine (cap.getX() + corner, cap.getY() + 0.7f, cap.getRight() - corner, cap.getY() + 0.7f, 1.0f);
    }
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (cap, corner, 0.8f);
}

void drawLedLens (Graphics& g, Rectangle<float> r, Colour led, bool lit, float glow)
{
    const float corner = jmin (r.getWidth(), r.getHeight()) * 0.35f;
    g.setColour (Colours::black);
    g.fillRoundedRectangle (r.expanded (1.0f), corner + 1.0f);
    if (lit)
    {
        g.setColour (led.withAlpha (0.22f * glow));
        g.fillRoundedRectangle (r.expanded (4.0f), corner + 4.0f);
        g.setColour (led.withAlpha (0.35f * glow));
        g.fillRoundedRectangle (r.expanded (2.0f), corner + 2.0f);
        ColourGradient body (led.brighter (0.8f), r.getCentreX(), r.getY(), led, r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, corner);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.38f).reduced (r.getWidth() * 0.18f, 0.3f).translated (0.0f, 0.6f), corner * 0.5f);
    }
    else
    {
        ColourGradient body (led.darker (2.2f).withMultipliedSaturation (0.55f), r.getCentreX(), r.getY(),
                             Colour (0xff060708), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, corner);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.35f).reduced (r.getWidth() * 0.2f, 0.3f).translated (0.0f, 0.6f), corner * 0.5f);
    }
}

void drawLedButton (Graphics& g, Rectangle<float> r, const String& label, Colour led, bool lit, bool pressed, bool hover)
{
    drawMetalCap (g, r, pressed, hover, 3.0f);
    auto cap = r.reduced (3.0f);
    if (pressed) cap = cap.translated (0.0f, 0.8f);
    const bool wide = r.getWidth() > r.getHeight() * 1.6f;
    Rectangle<float> lens, text;
    if (wide)
    {
        lens = Rectangle<float> (5.0f, jmin (12.0f, cap.getHeight() * 0.6f)).withPosition (cap.getX() + 4.0f, 0.0f).withCentre ({ cap.getX() + 6.5f, cap.getCentreY() });
        text = cap.withTrimmedLeft (14.0f);
    }
    else
    {
        lens = Rectangle<float> (jmin (18.0f, cap.getWidth() * 0.55f), 4.0f).withCentre ({ cap.getCentreX(), cap.getY() + 3.5f });
        text = cap.withTrimmedTop (7.0f);
    }
    drawLedLens (g, lens, led, lit);
    g.setFont (labelFont (jmin (10.0f, text.getHeight() * 0.62f)).boldened());
    g.setColour (lit ? col::print : col::print.withAlpha (0.78f));
    g.drawFittedText (label, text.toNearestInt(), wide ? Justification::centredLeft : Justification::centred, 1, 0.75f);
}

void drawRoundMetalButton (Graphics& g, Point<float> c, float radius, bool pressed, bool hover)
{
    const auto disc = [c] (float rr) { return Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c); };
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (disc (radius * 1.08f).translated (0.0f, 1.5f));
    // chrome bezel
    ColourGradient bezel (Colour (0xffe9edf3), c.x, c.y - radius, Colour (0xff2b2f36), c.x, c.y + radius, false);
    bezel.addColour (0.5, Colour (0xff8d939d));
    g.setGradientFill (bezel);
    g.fillEllipse (disc (radius));
    g.setColour (Colours::black);
    g.fillEllipse (disc (radius * 0.84f));
    // cap
    const auto cc = pressed ? c.translated (0.0f, 0.8f) : c;
    const float cr = radius * (pressed ? 0.76f : 0.79f);
    ColourGradient cap (Colour (0xff5a5f69).brighter (hover ? 0.1f : 0.0f), cc.x - cr * 0.3f, cc.y - cr * 0.6f,
                        Colour (0xff121418), cc.x + cr * 0.4f, cc.y + cr * 0.9f, true);
    if (pressed) cap = ColourGradient (Colour (0xff2a2d33), cc.x, cc.y - cr, Colour (0xff3f434b), cc.x, cc.y + cr, false);
    g.setGradientFill (cap);
    g.fillEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (cc));
    // machined rings
    g.setColour (Colours::white.withAlpha (0.05f));
    for (float k : { 0.35f, 0.55f, 0.75f })
        g.drawEllipse (Rectangle<float> (cr * 2.0f * k, cr * 2.0f * k).withCentre (cc), 0.6f);
    if (! pressed)
    {
        Path hl;
        hl.addCentredArc (cc.x, cc.y, cr * 0.9f, cr * 0.9f, 0.0f, -1.3f, 1.3f, true);
        g.setColour (Colours::white.withAlpha (0.25f));
        g.strokePath (hl, PathStrokeType (1.0f));
    }
}

// ============================================================================================
// knobs
void drawKnobScale (Graphics& g, Point<float> c, float radius, int ticks, Colour colour)
{
    g.setColour (colour);
    for (int i = 0; i < ticks; ++i)
    {
        const float t = ticks > 1 ? (float) i / (float) (ticks - 1) : 0.0f;
        const float a = theme::knobStart + t * (theme::knobEnd - theme::knobStart);
        const bool major = i == 0 || i == ticks - 1 || i == (ticks - 1) / 2;
        g.drawLine ({ polar (c, radius, a), polar (c, radius + (major ? 5.0f : 3.0f), a) }, major ? 1.4f : 1.0f);
    }
}

void drawValueArc (Graphics& g, Point<float> c, float radius, float from, float to, Colour colour, float thickness, float glow)
{
    if (to < from) std::swap (from, to);
    const float span = theme::knobEnd - theme::knobStart;
    const float a0 = theme::knobStart + from * span, a1 = theme::knobStart + to * span;
    if (a1 - a0 < 0.005f) return;
    Path arc;
    arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a1, true);
    if (glow > 0.0f)
    {
        g.setColour (colour.withAlpha (0.16f * glow));
        g.strokePath (arc, PathStrokeType (thickness * 4.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (colour.withAlpha (0.28f * glow));
        g.strokePath (arc, PathStrokeType (thickness * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (colour);
    g.strokePath (arc, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));
}

void drawChickenHead (Graphics& g, Point<float> c, float r, float angle, bool hot)
{
    const auto disc = [c] (float rr) { return Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c); };
    // shadow
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (disc (r * 1.02f).translated (r * 0.04f, r * 0.12f));
    // skirt with knurling
    ColourGradient skirt (Colour (0xff2a2c31), c.x - r * 0.6f, c.y - r * 0.8f, Colour (0xff050506), c.x + r * 0.6f, c.y + r, false);
    g.setGradientFill (skirt);
    g.fillEllipse (disc (r));
    g.setColour (Colours::black.withAlpha (0.55f));
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i / 48.0f * MathConstants<float>::twoPi;
        g.drawLine ({ polar (c, r * 0.86f, a), polar (c, r * 0.99f, a) }, 0.7f);
    }
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawEllipse (disc (r * 0.985f), 0.8f);
    if (hot)
    {
        g.setColour (col::ice.withAlpha (0.35f));
        g.drawEllipse (disc (r * 1.04f), 1.4f);
    }

    // the chicken head: pointer + tail, pointing up before rotation
    Path head;
    head.startNewSubPath (-r * 0.11f, -r * 0.94f);
    head.lineTo (r * 0.11f, -r * 0.94f);
    head.lineTo (r * 0.40f, -r * 0.05f);
    head.lineTo (r * 0.30f, r * 0.62f);
    head.lineTo (-r * 0.30f, r * 0.62f);
    head.lineTo (-r * 0.40f, -r * 0.05f);
    head.closeSubPath();
    head = head.createPathWithRoundedCorners (r * 0.14f);
    head.addEllipse (-r * 0.5f, -r * 0.5f, r, r);
    const auto rot = AffineTransform::rotation (angle).translated (c);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillPath (head, AffineTransform::rotation (angle).translated (c.translated (r * 0.03f, r * 0.08f)));
    ColourGradient body (Colour (0xff3a3d44), c.x - r * 0.5f, c.y - r * 0.6f, Colour (0xff0b0c0e), c.x + r * 0.5f, c.y + r * 0.7f, false);
    g.setGradientFill (body);
    g.fillPath (head, rot);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.strokePath (head, PathStrokeType (0.8f), rot);

    // pointer line
    g.setColour (Colour (0xffeef1f6));
    g.drawLine ({ polar (c, r * 0.52f, angle), polar (c, r * 0.88f, angle) }, jmax (1.4f, r * 0.07f));

    // chrome cap
    const float cr = r * 0.34f;
    ColourGradient cap (Colours::white, c.x - cr * 0.4f, c.y - cr * 0.5f, Colour (0xff3a3f48), c.x + cr * 0.6f, c.y + cr * 0.8f, true);
    cap.addColour (0.45, Colour (0xffb7bdc7));
    g.setGradientFill (cap);
    g.fillEllipse (disc (cr));
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawEllipse (disc (cr * 0.6f), 0.6f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawEllipse (disc (cr), 0.8f);
}

// ============================================================================================
// icons
namespace icon
{
static Rectangle<float> sq (Rectangle<float> r, float k = 0.62f)
{
    const float s = jmin (r.getWidth(), r.getHeight()) * k;
    return Rectangle<float> (s, s).withCentre (r.getCentre());
}
static float sw (Rectangle<float> b) { return jmax (1.3f, b.getWidth() * 0.12f); }

void prev (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.5f);
    Path p;
    p.addTriangle (b.getRight(), b.getY(), b.getRight(), b.getBottom(), b.getX(), b.getCentreY());
    g.setColour (c);
    g.fillPath (p);
}

void next (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.5f);
    Path p;
    p.addTriangle (b.getX(), b.getY(), b.getX(), b.getBottom(), b.getRight(), b.getCentreY());
    g.setColour (c);
    g.fillPath (p);
}

void save (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r);
    const float t = sw (b) * 0.8f;
    Path body;
    body.startNewSubPath (b.getX(), b.getY());
    body.lineTo (b.getRight() - b.getWidth() * 0.22f, b.getY());
    body.lineTo (b.getRight(), b.getY() + b.getHeight() * 0.22f);
    body.lineTo (b.getRight(), b.getBottom());
    body.lineTo (b.getX(), b.getBottom());
    body.closeSubPath();
    g.setColour (c);
    g.strokePath (body, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded));
    // shutter + label
    g.fillRect (Rectangle<float> (b.getWidth() * 0.42f, b.getHeight() * 0.3f).withPosition (b.getX() + b.getWidth() * 0.22f, b.getY()));
    g.drawRect (Rectangle<float> (b.getWidth() * 0.6f, b.getHeight() * 0.32f).withPosition (b.getX() + b.getWidth() * 0.2f, b.getBottom() - b.getHeight() * 0.42f), t * 0.8f);
}

void revert (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r);
    const float t = sw (b);
    const auto cc = b.getCentre();
    const float rad = b.getWidth() * 0.42f;
    Path arc;
    arc.addCentredArc (cc.x, cc.y, rad, rad, 0.0f, -2.2f, 2.0f, true);
    g.setColour (c);
    g.strokePath (arc, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded));
    // arrow head at the start of the arc (counter-clockwise arrow)
    const auto tip = polar (cc, rad, -2.2f);
    Path head;
    const float hs = b.getWidth() * 0.3f;
    head.addTriangle (tip.x - hs * 0.55f, tip.y - hs * 0.55f, tip.x + hs * 0.55f, tip.y - hs * 0.15f, tip.x - hs * 0.05f, tip.y + hs * 0.55f);
    g.fillPath (head);
}

void star (Graphics& g, Rectangle<float> r, Colour c, bool filled)
{
    auto b = sq (r, 0.66f);
    Path p;
    p.addStar (b.getCentre().translated (0.0f, b.getHeight() * 0.04f), 5, b.getWidth() * 0.22f, b.getWidth() * 0.52f);
    g.setColour (c);
    if (filled) g.fillPath (p);
    else g.strokePath (p, PathStrokeType (sw (b) * 0.8f, PathStrokeType::mitered));
}

static void curvedArrow (Graphics& g, Rectangle<float> r, Colour c, bool flip)
{
    auto b = sq (r);
    const float t = sw (b);
    Path p;
    p.startNewSubPath (b.getX() + b.getWidth() * 0.18f, b.getY() + b.getHeight() * 0.4f);
    p.lineTo (b.getX() + b.getWidth() * 0.62f, b.getY() + b.getHeight() * 0.4f);
    p.cubicTo (b.getRight() + b.getWidth() * 0.05f, b.getY() + b.getHeight() * 0.4f,
               b.getRight() + b.getWidth() * 0.05f, b.getBottom() - b.getHeight() * 0.05f,
               b.getX() + b.getWidth() * 0.55f, b.getBottom() - b.getHeight() * 0.05f);
    Path head;
    const float hy = b.getY() + b.getHeight() * 0.4f, hx = b.getX();
    head.addTriangle (hx, hy, hx + b.getWidth() * 0.32f, hy - b.getHeight() * 0.25f, hx + b.getWidth() * 0.32f, hy + b.getHeight() * 0.25f);
    const auto t2 = flip ? AffineTransform::scale (-1.0f, 1.0f, b.getCentreX(), b.getCentreY()) : AffineTransform();
    g.setColour (c);
    g.strokePath (p, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded), t2);
    g.fillPath (head, t2);
}

void undo (Graphics& g, Rectangle<float> r, Colour c) { curvedArrow (g, r, c, false); }
void redo (Graphics& g, Rectangle<float> r, Colour c) { curvedArrow (g, r, c, true); }

void cog (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.68f);
    const auto cc = b.getCentre();
    const float ro = b.getWidth() * 0.5f, ri = ro * 0.74f;
    Path p;
    const int teeth = 8;
    for (int i = 0; i < teeth * 4; ++i)
    {
        const float a = (float) i / (float) (teeth * 4) * MathConstants<float>::twoPi;
        const float rr = (i % 4 == 0 || i % 4 == 1) ? ro : ri;
        const auto pt = polar (cc, rr, a - 0.1f);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    p.addEllipse (Rectangle<float> (ro * 0.7f, ro * 0.7f).withCentre (cc));
    p.setUsingNonZeroWinding (false);
    g.setColour (c);
    g.fillPath (p);
}

void info (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.64f);
    g.setColour (c);
    g.drawEllipse (b, sw (b) * 0.8f);
    const float t = b.getWidth() * 0.13f;
    g.fillEllipse (Rectangle<float> (t * 1.2f, t * 1.2f).withCentre ({ b.getCentreX(), b.getY() + b.getHeight() * 0.28f }));
    g.fillRoundedRectangle (Rectangle<float> (t, b.getHeight() * 0.36f).withCentre ({ b.getCentreX(), b.getY() + b.getHeight() * 0.62f }), t * 0.3f);
}

void power (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.6f);
    const float t = sw (b);
    const auto cc = b.getCentre().translated (0.0f, b.getHeight() * 0.05f);
    Path arc;
    arc.addCentredArc (cc.x, cc.y, b.getWidth() * 0.42f, b.getWidth() * 0.42f, 0.0f, 0.6f, MathConstants<float>::twoPi - 0.6f, true);
    g.setColour (c);
    g.strokePath (arc, PathStrokeType (t, PathStrokeType::curved, PathStrokeType::rounded));
    g.drawLine (cc.x, b.getY() - b.getHeight() * 0.05f, cc.x, cc.y, t);
}

void lock (Graphics& g, Rectangle<float> r, Colour c)
{
    auto b = sq (r, 0.6f);
    const float t = sw (b);
    auto body = b.withTrimmedTop (b.getHeight() * 0.45f);
    g.setColour (c);
    g.fillRoundedRectangle (body, t * 0.6f);
    Path shackle;
    shackle.addCentredArc (b.getCentreX(), body.getY(), b.getWidth() * 0.28f, b.getHeight() * 0.32f, 0.0f,
                           -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
    g.strokePath (shackle, PathStrokeType (t));
}

void letter (Graphics& g, Rectangle<float> r, Colour c, const String& text)
{
    g.setColour (c);
    g.setFont (theme::stencilFont (r.getHeight() * 0.62f));
    g.drawText (text, r, Justification::centred, false);
}
} // namespace icon

// ============================================================================================
void BackgroundCache::draw (Graphics& g, Rectangle<int> area, const std::function<void (Graphics&)>& painter)
{
    const float scale = jlimit (1.0f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (! image.isValid() || area != cachedArea || std::abs (scale - cachedScale) > 0.01f)
    {
        cachedArea = area;
        cachedScale = scale;
        image = Image (Image::ARGB, jmax (1, (int) std::ceil ((float) area.getWidth() * scale)),
                       jmax (1, (int) std::ceil ((float) area.getHeight() * scale)), true);
        Graphics ig (image);
        ig.addTransform (AffineTransform::translation ((float) -area.getX(), (float) -area.getY()).scaled (scale));
        painter (ig);
    }
    g.drawImageTransformed (image, AffineTransform::scale (1.0f / cachedScale).translated ((float) area.getX(), (float) area.getY()));
}

// ============================================================================================
HwButton::HwButton (Kind k, String l, Colour c) : kind (k), label (std::move (l)), led (c)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool HwButton::litNow() const
{
    const bool lit = isLit && isLit();
    if (lit && blinkWhenLit && ! reduceMotion()) return (Time::getMillisecondCounter() / 420) % 2 == 0;
    return lit || Time::getMillisecondCounter() < flashUntil;
}

void HwButton::tick()
{
    const bool en = ! isEnabledFn || isEnabledFn();
    if (litNow() != lastLit || en != lastEnabled) repaint();
}

void HwButton::paint (Graphics& g)
{
    lastLit = litNow();
    lastEnabled = ! isEnabledFn || isEnabledFn();
    const bool hover = isMouseOver() && lastEnabled;
    auto r = getLocalBounds().toFloat();
    if (! lastEnabled) g.beginTransparencyLayer (0.4f);

    switch (kind)
    {
        case Kind::Led:
            drawLedButton (g, r, label, led, lastLit, down, hover);
            break;
        case Kind::Icon:
        case Kind::Text:
        {
            drawMetalCap (g, r, down, hover, 3.0f);
            auto inner = r.reduced (2.0f);
            if (down) inner = inner.translated (0.0f, 0.8f);
            if (lastLit)
            {
                g.setColour (led.withAlpha (0.16f));
                g.fillRoundedRectangle (inner, 2.0f);
                g.setColour (led.withAlpha (0.85f));
                g.drawRoundedRectangle (inner.reduced (0.5f), 2.0f, 1.0f);
                g.setColour (led.withAlpha (0.25f));
                g.drawRoundedRectangle (r.expanded (0.5f), 4.0f, 1.5f);
            }
            const Colour ink = lastLit ? led.brighter (0.35f) : (hover ? Colours::white : col::print.withAlpha (0.86f));
            if (kind == Kind::Icon && drawIcon) drawIcon (g, inner, ink);
            else
            {
                if (lastLit) theme::drawGlowText (g, label, inner, labelFont (jmin (11.5f, inner.getHeight() * 0.55f)).boldened(), ink, Justification::centred, 0.6f);
                else
                {
                    g.setFont (labelFont (jmin (11.5f, inner.getHeight() * 0.55f)).boldened());
                    g.setColour (ink);
                    g.drawFittedText (label, inner.toNearestInt(), Justification::centred, 1, 0.7f);
                }
            }
            break;
        }
        case Kind::Round:
        {
            const float rad = jmin (r.getWidth(), r.getHeight()) * 0.5f - 3.0f;
            const auto c = r.getCentre();
            if (lastLit)
            {
                g.setColour (led.withAlpha (0.35f));
                g.fillEllipse (Rectangle<float> ((rad + 3.0f) * 2.0f, (rad + 3.0f) * 2.0f).withCentre (c));
            }
            drawRoundMetalButton (g, c, rad, down, hover);
            g.setFont (labelFont (jmin (11.0f, rad * 0.5f)).boldened());
            g.setColour (lastLit ? led.brighter (0.4f) : col::print.withAlpha (0.9f));
            g.drawText (label, Rectangle<float> (rad * 2.0f, rad).withCentre (down ? c.translated (0.0f, 0.8f) : c), Justification::centred, false);
            break;
        }
    }
    if (! lastEnabled) g.endTransparencyLayer();
}

void HwButton::mouseDown (const MouseEvent& e)
{
    if (isEnabledFn && ! isEnabledFn()) return;
    if (e.mods.isPopupMenu() && ! onPress) { if (onClick) onClick(); return; }
    down = true;
    repaint();
    if (onPress) onPress (true);
}

void HwButton::mouseUp (const MouseEvent& e)
{
    if (! down) return;
    down = false;
    repaint();
    if (onPress) onPress (false);
    if (onClick && getLocalBounds().contains (e.getPosition())) onClick();
}

// ============================================================================================
void HwKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float s = jmin (b.getWidth(), b.getHeight());
    const float r = s * 0.5f * 0.70f;
    const float v = getDisplayValue ? getDisplayValue() : getNormValue();
    const float angle = theme::knobStart + v * (theme::knobEnd - theme::knobStart);
    drawKnobScale (g, c, r * 1.30f, 11, col::print.withAlpha (0.55f));
    if (showArc)
    {
        Path track;
        track.addCentredArc (c.x, c.y, r * 1.15f, r * 1.15f, 0.0f, theme::knobStart, theme::knobEnd, true);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        drawValueArc (g, c, r * 1.15f, 0.0f, v, ring.withAlpha (0.5f + 0.5f * ringGlow), 2.0f, ringGlow);
    }
    drawChickenHead (g, c, r, angle, isMouseButtonDown() || isMouseOver());
}
} // namespace ek::ui::hw
