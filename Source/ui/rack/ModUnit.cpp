#include "ModUnit.h"
#include "CardStyle.h"

namespace ek::ui
{
using namespace juce;

// ============================================================================
// Shared helpers
// ============================================================================
Colour modSourceColour (int source)
{
    switch (source)
    {
        case 0:  return Colour (0xff9fc3ff); // LFO 1 - ice blue
        case 1:  return Colour (0xffb07cff); // LFO 2 - violet
        case 2:  return Colour (0xff5fe0b0); // ENV   - mint
        case 3:  return Colour (0xffffd23f); // SEQ   - yellow
        default: return Colour (0xffe6ecf7);
    }
}

static Point<float> polarPt (Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }
static float normToAngle (float n) { return theme::knobStart + (theme::knobEnd - theme::knobStart) * jlimit (0.0f, 1.0f, n); }

void paintModRing (Graphics& g, Rectangle<float> knobBounds, float baseNorm, float depth, Colour colour,
                   bool bipolar, float currentNorm, int ringIndex)
{
    const auto c = knobBounds.getCentre();
    const float r = knobBounds.getWidth() * 0.5f + 3.0f + 4.0f * (float) jmax (0, ringIndex);
    const float thick = jlimit (1.4f, 2.4f, knobBounds.getWidth() * 0.05f);
    baseNorm = jlimit (0.0f, 1.0f, baseNorm);
    depth = jlimit (-1.0f, 1.0f, depth);

    // faint track of the whole knob range
    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, theme::knobStart, theme::knobEnd, true);
    g.setColour (colour.withAlpha (0.10f));
    g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    const float lo = jlimit (0.0f, 1.0f, bipolar ? baseNorm - std::abs (depth) : jmin (baseNorm, baseNorm + depth));
    const float hi = jlimit (0.0f, 1.0f, bipolar ? baseNorm + std::abs (depth) : jmax (baseNorm, baseNorm + depth));
    if (hi - lo > 1.0e-4f)
    {
        Path range;
        range.addCentredArc (c.x, c.y, r, r, 0.0f, normToAngle (lo), normToAngle (hi), true);
        card::strokeGlow (g, range, colour, thick, 1.2f);
        // the direction of a positive depth gets a bright end cap
        const float tip = bipolar ? (depth >= 0.0f ? hi : lo) : jlimit (0.0f, 1.0f, baseNorm + depth);
        card::dotGlow (g, polarPt (c, r, normToAngle (tip)), thick * 0.9f, colour, 0.9f);
    }
    // base marker: a small white tick across the ring
    g.setColour (Colours::white.withAlpha (0.8f));
    g.drawLine (Line<float> (polarPt (c, r - thick * 1.2f, normToAngle (baseNorm)), polarPt (c, r + thick * 1.2f, normToAngle (baseNorm))), 1.0f);

    if (currentNorm >= 0.0f)
        card::dotGlow (g, polarPt (c, r, normToAngle (currentNorm)), thick * 1.1f, colour, 1.0f);
}

void paintPatchCable (Graphics& g, Point<float> from, Point<float> to, Colour colour)
{
    const float dist = from.getDistanceFrom (to);
    const float sag = jlimit (10.0f, 80.0f, dist * 0.22f);
    Path cable;
    cable.startNewSubPath (from);
    cable.cubicTo (from.translated (0.0f, sag), to.translated (0.0f, sag), to);
    card::strokeGlow (g, cable, colour, 1.6f, 1.3f);
    g.setColour (Colours::white.withAlpha (0.45f));
    g.strokePath (cable, PathStrokeType (0.6f, PathStrokeType::curved, PathStrokeType::rounded));
    card::dotGlow (g, from, 3.0f, colour, 1.0f);
    card::dotGlow (g, to, 3.0f, colour, 1.0f);
}

namespace
{
const Colour labelCol { 0xffc8d3e6 };   // readable label colour (Docs/TRAPWIN-STYLE.md)
// 11 px Plex Mono with almost no extra tracking: the knob columns are narrow
Font smallFont (bool bold = false) { return card::monoFont (11.0f, bold).withExtraKerningFactor (0.02f); }
}

static String hzText (float hz)
{
    if (hz < 1.0f) return String (hz, 2) + "Hz";
    if (hz < 10.0f) return String (hz, 1) + "Hz";
    return String (roundToInt (hz)) + "Hz";
}

static String msText (float ms)
{
    if (ms < 10.0f) return String (ms, 1) + "ms";
    if (ms < 1000.0f) return String (roundToInt (ms)) + "ms";
    return String (ms * 0.001f, 1) + "s";
}

static float logNorm (float v, float lo, float hi) { return jlimit (0.0f, 1.0f, std::log (jmax (lo, v) / lo) / std::log (hi / lo)); }
static float logValue (float n, float lo, float hi) { return lo * std::pow (hi / lo, jlimit (0.0f, 1.0f, n)); }

// ============================================================================
// MiniKnob: small flat TrapWin knob with label + value, driven by callbacks
// ============================================================================
class ModUnit::MiniKnob : public Component, public SettableTooltipClient
{
public:
    MiniKnob (EffectorKillaAudioProcessor& p, String l, Colour c, std::function<float()> g, std::function<void (float)> s,
              std::function<String()> t, float def, std::function<int()> stepsFn = {})
        : proc (p), label (std::move (l)), colour (c), get (std::move (g)), set (std::move (s)), text (std::move (t)),
          defaultValue (def), steps (std::move (stepsFn))
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setRepaintsOnMouseActivity (true);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    static constexpr int height = 62;   // label 11 px, 30 px knob, value 11 px

    void tick()
    {
        const float v = get();
        const auto t = text();
        if (v != shown || t != shownText) { shown = v; shownText = t; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool hot = isMouseOverOrDragging();
        g.setFont (smallFont());
        g.setColour (hot ? card::text : labelCol);
        g.drawFittedText (label, getLocalBounds().withHeight (13), Justification::centred, 1, 0.85f);

        const auto c = Point<float> (b.getCentreX(), 29.0f);
        const float r = 15.0f;
        if (hot)
        {
            ColourGradient halo (colour.withAlpha (0.22f), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 2.2f, c.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (Rectangle<float> (r * 4.4f, r * 4.4f).withCentre (c));
        }
        card::paintKnob (g, c, r, get(), colour);

        g.setFont (smallFont());
        g.setColour (card::text);
        g.drawFittedText (text(), Rectangle<int> (0, 47, getWidth(), 14), Justification::centred, 1, 0.85f);
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        proc.pushUndo();
        startValue = get();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        const float range = e.mods.isShiftDown() ? 800.0f : 160.0f;
        apply (startValue - (float) e.getDistanceFromDragStartY() / range);
    }

    void mouseDoubleClick (const MouseEvent&) override { set (defaultValue); tick(); }

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        const int n = steps ? steps() : 0;
        const float step = n > 1 ? 1.0f / (float) (n - 1) : 0.03f;
        apply (get() + (w.deltaY > 0.0f ? step : -step));
    }

private:
    void apply (float v)
    {
        v = jlimit (0.0f, 1.0f, v);
        const int n = steps ? steps() : 0;
        if (n > 1) v = (float) roundToInt (v * (float) (n - 1)) / (float) (n - 1);
        if (v != get()) { set (v); tick(); }
    }

    EffectorKillaAudioProcessor& proc;
    String label;
    Colour colour;
    std::function<float()> get;
    std::function<void (float)> set;
    std::function<String()> text;
    float defaultValue, startValue = 0.0f, shown = -1.0f;
    std::function<int()> steps;
    String shownText;
};

// ============================================================================
// Layout: computed in resized() for any width >= kMinWidth
// ============================================================================
namespace
{
constexpr int kKnobsPerSection[kNumModSources] = { 3, 3, 3, 2 };
constexpr float kSectionWeight[kNumModSources] = { 1.04f, 1.04f, 1.0f, 0.92f };
constexpr int kMargin = 10;                // inner margin of the card
constexpr int kTitleW = 66;                // MOD title column
constexpr int kChipY = 6, kChipH = 24;     // source chips (both modes)
constexpr int kContentY = 36;              // expanded: screens + knobs below the chips
constexpr int kKnobW = 34;
constexpr float kCorner = 16.0f;           // card radius (small card)
constexpr float kJackR = 8.0f;
}

ModUnit::ModUnit (EffectorKillaAudioProcessor& p) : proc (p)
{
    static const char* titles[] = { "LFO 1", "LFO 2", "ENV", "SEQ" };
    for (int s = 0; s < kNumModSources; ++s)
    {
        sections[(size_t) s].source = s;
        sections[(size_t) s].title = titles[s];
    }

    // ---- knobs (positioned in resized()) -----------------------------------------------------
    auto addKnob = [this] (int, String label, int source, std::function<float()> g, std::function<void (float)> s,
                           std::function<String()> t, float def, std::function<int()> steps = {})
    {
        auto k = std::make_unique<MiniKnob> (proc, label, modSourceColour (source), std::move (g), std::move (s), std::move (t), def, std::move (steps));
        addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
    };
    const int nDiv = numModDivisions();
    for (int i = 0; i < 2; ++i)
    {
        const int ax = 0;
        auto lfo = [this, i] { return proc.getModSources().lfo[(size_t) i]; };
        addKnob (ax + 96, "SHAPE", i,
                 [lfo] { return (float) lfo().shape / (float) ((int) LfoShape::Count - 1); },
                 [this, lfo, i] (float v) { auto l = lfo(); l.shape = (LfoShape) roundToInt (v * (float) ((int) LfoShape::Count - 1)); proc.setLfo (i, l); },
                 [lfo] { return String (lfoShapeName (lfo().shape)); }, 0.0f, [] { return (int) LfoShape::Count; });
        addKnob (ax + 128, "RATE", i,
                 [lfo, nDiv] { auto l = lfo(); return l.sync ? (float) l.division / (float) (nDiv - 1) : logNorm (l.rateHz, LfoSettings::minHz, LfoSettings::maxHz); },
                 [this, lfo, i, nDiv] (float v)
                 {
                     auto l = lfo();
                     if (l.sync) l.division = roundToInt (v * (float) (nDiv - 1));
                     else l.rateHz = logValue (v, LfoSettings::minHz, LfoSettings::maxHz);
                     proc.setLfo (i, l);
                 },
                 [lfo] { auto l = lfo(); return l.sync ? String (modDivision (l.division).label) : hzText (l.rateHz); },
                 i == 0 ? logNorm (1.0f, LfoSettings::minHz, LfoSettings::maxHz) : 2.0f / (float) (nDiv - 1),
                 [lfo, nDiv] { return lfo().sync ? nDiv : 0; });
        addKnob (ax + 160, "PHASE", i,
                 [lfo] { return lfo().phase; },
                 [this, lfo, i] (float v) { auto l = lfo(); l.phase = v; proc.setLfo (i, l); },
                 [lfo] { return String (roundToInt (lfo().phase * 360.0f)) + String (CharPointer_UTF8 ("\xc2\xb0")); }, 0.0f);
    }

    {
        const int ax = 0;
        auto env = [this] { return proc.getModSources().env; };
        addKnob (ax + 82, "ATK", 2,
                 [env] { return logNorm (env().attackMs, 0.1f, 500.0f); },
                 [this, env] (float v) { auto e = env(); e.attackMs = logValue (v, 0.1f, 500.0f); proc.setEnvFollower (e); },
                 [env] { return msText (env().attackMs); }, logNorm (10.0f, 0.1f, 500.0f));
        addKnob (ax + 114, "REL", 2,
                 [env] { return logNorm (env().releaseMs, 5.0f, 5000.0f); },
                 [this, env] (float v) { auto e = env(); e.releaseMs = logValue (v, 5.0f, 5000.0f); proc.setEnvFollower (e); },
                 [env] { return msText (env().releaseMs); }, logNorm (150.0f, 5.0f, 5000.0f));
        addKnob (ax + 146, "SENS", 2,
                 [env] { return env().sensitivity; },
                 [this, env] (float v) { auto e = env(); e.sensitivity = v; proc.setEnvFollower (e); },
                 [env] { return "+" + String (roundToInt (EnvSettings::sensitivityGainDb (env().sensitivity))) + "dB"; }, 0.5f);
    }

    {
        const int ax = 0;
        auto seq = [this] { return proc.getModSources().seq; };
        addKnob (ax + 138, "RATE", 3,
                 [seq, nDiv] { return (float) seq().division / (float) (nDiv - 1); },
                 [this, seq, nDiv] (float v) { auto s = seq(); s.division = roundToInt (v * (float) (nDiv - 1)); proc.setStepSeq (s); },
                 [seq] { return String (modDivision (seq().division).label); }, 10.0f / (float) (nDiv - 1), [nDiv] { return nDiv; });
        addKnob (ax + 170, "GLIDE", 3,
                 [seq] { return seq().smooth; },
                 [this, seq] (float v) { auto s = seq(); s.smooth = v; proc.setStepSeq (s); },
                 [seq] { return String (roundToInt (seq().smooth * 100.0f)) + "%"; }, 0.0f);
    }

    setTooltip ("MOD SOURCES - drag a jack onto any knob in the chain to modulate it");
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    for (auto& k : knobs) k->setVisible (expanded);
    setSize (kWidth, collapsedHeight);
}

ModUnit::~ModUnit() = default;

void ModUnit::setExpanded (bool shouldBeExpanded)
{
    if (expanded == shouldBeExpanded) return;
    expanded = shouldBeExpanded;
    for (auto& k : knobs) k->setVisible (expanded);
    resized();
    repaint();
    if (onExpandedChanged) onExpandedChanged();
}

void ModUnit::resized()
{
    const int w = jmax (kMinWidth, getWidth());
    const int h = getCurrentHeight();
    titleArea = { kMargin, 0, kTitleW, h };

    if (! expanded)
    {
        // [MOD] [chip] [chip] [chip] [chip] [EDIT]
        editPill = { w - kMargin - 4 - 78, kChipY, 78, kChipH };
        const int x0 = titleArea.getRight() + 6, x1 = editPill.getX() - 12;
        const float cw = (float) (x1 - x0 + 8) / (float) kNumModSources;
        for (int s = 0; s < kNumModSources; ++s)
        {
            auto& sec = sections[(size_t) s];
            sec.chip = Rectangle<float> ((float) x0 + cw * (float) s, (float) kChipY, cw - 8.0f, (float) kChipH).toNearestInt();
            sec.area = sec.chip;
            sec.display = {};
            sec.jack = Rectangle<int> (22, 22).withCentre ({ sec.chip.getRight() - 13, sec.chip.getCentreY() });
        }
        for (int i = 0; i < 2; ++i) syncButtons[i] = {};
        return;
    }

    // expanded: title column (MOD / SOURCES / ACTIVE / CLOSE pill) + four weighted sections
    editPill = { titleArea.getX() + 2, h - 8 - 22, kTitleW - 4, 22 };
    const float avail = (float) (w - titleArea.getRight() - 8 - kMargin);
    float wsum = 0.0f;
    for (auto wt : kSectionWeight) wsum += wt;
    float x = (float) (titleArea.getRight() + 8);
    int k = 0;
    for (int s = 0; s < kNumModSources; ++s)
    {
        auto& sec = sections[(size_t) s];
        const float sw = avail * kSectionWeight[s] / wsum;
        sec.area = Rectangle<float> (x, 2.0f, sw, (float) h - 4.0f).toNearestInt();
        x += sw;
        const auto a = sec.area;
        sec.chip = { a.getX() + 5, kChipY, a.getWidth() - 10, kChipH };
        sec.jack = Rectangle<int> (22, 22).withCentre ({ sec.chip.getRight() - 13, sec.chip.getCentreY() });

        const int nk = kKnobsPerSection[s];
        const int knobsRight = a.getRight() - 4;
        const int knobsLeft = knobsRight - nk * kKnobW;
        const int screenX = a.getX() + 7;
        const int screenW = jmax (24, knobsLeft - 5 - screenX);
        if (s <= 1)
        {
            sec.display = { screenX, kContentY + 2, screenW, 36 };
            syncButtons[s] = { screenX, kContentY + 42, screenW, 20 };
        }
        else
            sec.display = { screenX, kContentY + 2, screenW, h - 8 - (kContentY + 2) };
        for (int i = 0; i < nk; ++i, ++k)
            knobs[(size_t) k]->setBounds (knobsLeft + i * kKnobW, kContentY, kKnobW, MiniKnob::height);
    }
}

Rectangle<int> ModUnit::getHandleBounds (int source) const
{
    return isPositiveAndBelow (source, kNumModSources) ? sections[(size_t) source].jack : Rectangle<int>();
}

Point<int> ModUnit::getHandleScreenPosition (int source) const
{
    return localPointToGlobal (getHandleBounds (source).getCentre());
}

String ModUnit::countText (int source) const
{
    int n = 0;
    for (auto& a : proc.getModAssignments()) if (a.source == source) ++n;
    return n == 0 ? String() : String (n) + String (CharPointer_UTF8 ("\xc3\x97")); // "3x"
}

void ModUnit::refresh()
{
    for (auto& k : knobs) k->tick();
    repaint();
}

void ModUnit::tick()
{
    // envelope history for the ENV screen
    envHistory[(size_t) envPos] = proc.getModSourceValue ((int) ModSourceId::Env);
    envPos = (envPos + 1) % (int) envHistory.size();

    if (expanded)
        for (auto& k : knobs) k->tick();
    for (int s = 0; s < kNumModSources; ++s)
    {
        const float v = proc.getModSourceValue (s);
        if (std::abs (v - shownValue[(size_t) s]) > 1.0e-3f || s == (int) ModSourceId::Env)
        {
            shownValue[(size_t) s] = v;
            if (expanded) repaint (sections[(size_t) s].display.expanded (4));
            repaint (sections[(size_t) s].chip.expanded (14));
        }
    }
    const int st = proc.getSeqStep();
    if (st != shownStep) { shownStep = st; if (expanded) repaint (sections[3].display.expanded (4)); }
    if (expanded) repaint (titleArea); // ACTIVE dot
}

// ============================================================================
// Painting
// ============================================================================
static void paintChevron (Graphics& g, Point<float> c, bool up, Colour col)
{
    Path p;
    const float s = 3.6f;
    if (up) { p.startNewSubPath (c.x - s, c.y + s * 0.5f); p.lineTo (c.x, c.y - s * 0.5f); p.lineTo (c.x + s, c.y + s * 0.5f); }
    else    { p.startNewSubPath (c.x - s, c.y - s * 0.5f); p.lineTo (c.x, c.y + s * 0.5f); p.lineTo (c.x + s, c.y - s * 0.5f); }
    g.setColour (col);
    g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
}

void ModUnit::paintEditPill (Graphics& g)
{
    const auto r = editPill.toFloat();
    const float rad = r.getHeight() * 0.5f;
    if (editHover)
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (card::iceDeep.withAlpha (0.12f * (float) (4 - k)));
            g.fillRoundedRectangle (r.expanded ((float) k * 1.3f), rad + (float) k * 1.3f);
        }
    if (expanded)
    {
        ColourGradient fill (card::ice.withAlpha (0.26f), 0.0f, r.getY(), card::iceDeep.withAlpha (0.26f), 0.0f, r.getBottom(), false);
        g.setGradientFill (fill);
    }
    else
        g.setColour (card::bg);
    g.fillRoundedRectangle (r, rad);
    g.setColour (editHover || expanded ? card::ice.withAlpha (0.85f) : card::text.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    const auto f = smallFont (true);
    const String t = expanded ? "CLOSE" : "EDIT";
    const float tw = GlyphArrangement::getStringWidth (f, t);
    const float x0 = r.getCentreX() - (tw + 12.0f) * 0.5f;
    g.setFont (f);
    g.setColour (card::text);
    g.drawText (t, Rectangle<float> (x0, r.getY(), tw + 2.0f, r.getHeight()), Justification::centredLeft, false);
    paintChevron (g, { x0 + tw + 8.0f, r.getCentreY() }, expanded, card::text);
}

void ModUnit::paintTitle (Graphics& g)
{
    const auto t = titleArea.toFloat();
    if (! expanded)
    {
        card::paintChromeTitle (g, "MOD", { t.getX() + 6.0f, 7.0f, t.getWidth() - 8.0f, 22.0f }, 19.0f, 1.0f);
    }
    else
    {
        card::paintChromeTitle (g, "MOD", { t.getX() + 6.0f, 9.0f, t.getWidth() - 8.0f, 24.0f }, 20.0f, 1.0f);
        g.setFont (smallFont());
        g.setColour (labelCol);
        g.drawText ("SOURCES", Rectangle<float> (t.getX() + 7.0f, 36.0f, t.getWidth() - 6.0f, 14.0f), Justification::centredLeft, false);

        const bool active = proc.engine.modMeters.active.load (std::memory_order_relaxed);
        card::dotGlow (g, { t.getX() + 10.0f, 60.0f }, 2.8f, card::ice, active ? 1.0f : 0.0f);
        g.setColour (active ? card::text : labelCol.withAlpha (0.75f));
        g.drawText ("ACTIVE", Rectangle<float> (t.getX() + 18.0f, 53.0f, t.getWidth() - 16.0f, 14.0f), Justification::centredLeft, false);
    }

    // divider between the title and the sources
    const float h = (float) getCurrentHeight();
    ColourGradient div (card::ice.withAlpha (0.0f), 0.0f, 6.0f, card::ice.withAlpha (0.0f), 0.0f, h - 6.0f, false);
    div.addColour (0.5, card::ice.withAlpha (0.18f));
    g.setGradientFill (div);
    g.fillRect (t.getRight() + 2.0f, 6.0f, 1.0f, h - 12.0f);
}

Rectangle<float> ModUnit::seqPlot() const
{
    return sections[3].display.toFloat().reduced (5.0f, 6.0f);
}

void ModUnit::paintDisplay (Graphics& g, const Section& sec)
{
    const auto d = sec.display.toFloat();
    const auto col = modSourceColour (sec.source);
    constexpr float corner = 7.0f;
    card::paintScreen (g, d, corner, col);

    Path clip;
    clip.addRoundedRectangle (d.reduced (1.0f), corner - 1.0f);
    g.saveState();
    g.reduceClipRegion (clip);

    // faint grid
    g.setColour (col.withAlpha (0.06f));
    for (int i = 1; i < 4; ++i) g.fillRect (d.getX() + 2.0f, d.getY() + d.getHeight() * (float) i / 4.0f, d.getWidth() - 4.0f, 1.0f);
    for (int i = 1; i < 6; ++i) g.fillRect (d.getX() + d.getWidth() * (float) i / 6.0f, d.getY() + 2.0f, 1.0f, d.getHeight() - 4.0f);

    const auto plot = d.reduced (5.0f, 6.0f);
    auto yOf = [&] (float v, bool bipolar) { return bipolar ? plot.getCentreY() - v * plot.getHeight() * 0.5f : plot.getBottom() - v * plot.getHeight(); };

    const auto& ms = proc.getModSources();
    if (sec.source <= 1)
    {
        const auto& l = ms.lfo[(size_t) sec.source];
        Path wave;
        const int n = 72;
        for (int i = 0; i <= n; ++i)
        {
            const double ph = (double) i / n;
            const float v = l.shape == LfoShape::SampleHold
                              ? ModSourceEngine::lfoShape (l.shape, 0.0, (juce::int64) (ph * 7.999), sec.source)
                              : ModSourceEngine::lfoShape (l.shape, ph, 0, sec.source);
            const Point<float> p (plot.getX() + plot.getWidth() * (float) ph, yOf (v * 0.9f, true));
            if (i == 0) wave.startNewSubPath (p); else wave.lineTo (p);
        }
        card::strokeGlow (g, wave, col.withAlpha (0.9f), 1.2f, 1.0f);
        const float ph = proc.getLfoPhase (sec.source);
        const float v = proc.getModSourceValue (sec.source);
        const Point<float> dot (plot.getX() + plot.getWidth() * ph, yOf (v * 0.9f, true));
        // playhead line + glowing dot
        g.setColour (col.withAlpha (0.18f));
        g.fillRect (dot.x - 0.5f, d.getY() + 2.0f, 1.0f, d.getHeight() - 4.0f);
        card::dotGlow (g, dot, 2.4f, col, 1.0f);
    }
    else if (sec.source == (int) ModSourceId::Env)
    {
        Path area, line;
        const int n = (int) envHistory.size();
        const auto p2 = plot.withTrimmedRight (8.0f);
        for (int i = 0; i < n; ++i)
        {
            const float v = envHistory[(size_t) ((envPos + i) % n)];
            const Point<float> p (p2.getX() + p2.getWidth() * (float) i / (float) (n - 1), yOf (jlimit (0.0f, 1.0f, v), false));
            if (i == 0) { line.startNewSubPath (p); area.startNewSubPath (p.x, plot.getBottom()); }
            else line.lineTo (p);
            area.lineTo (p);
        }
        area.lineTo (p2.getRight(), plot.getBottom());
        area.closeSubPath();
        g.setGradientFill (ColourGradient (col.withAlpha (0.28f), 0.0f, plot.getY(), col.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (area);
        card::strokeGlow (g, line, col, 1.2f, 1.0f);
        // live level: glowing dot at the head + a slim bar at the right edge
        const float v = jlimit (0.0f, 1.0f, proc.getModSourceValue ((int) ModSourceId::Env));
        card::dotGlow (g, { p2.getRight(), yOf (v, false) }, 2.2f, col, 1.0f);
        const auto bar = Rectangle<float> (plot.getRight() - 2.5f, plot.getY(), 2.5f, plot.getHeight());
        g.setColour (col.withAlpha (0.10f));
        g.fillRoundedRectangle (bar, 1.2f);
        g.setColour (col);
        g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * v), 1.2f);
    }
    else
    {
        const auto& sq = ms.seq;
        const int cur = proc.getSeqStep();
        const auto sp = seqPlot();
        const float w = sp.getWidth() / (float) kSeqSteps;
        const float barH = sp.getHeight() - 5.0f;
        for (int i = 0; i < kSeqSteps; ++i)
        {
            const float v = jlimit (0.0f, 1.0f, sq.steps[(size_t) i]);
            auto bar = Rectangle<float> (sp.getX() + w * (float) i + 0.8f, sp.getY(), jmax (1.0f, w - 1.6f), barH);
            g.setColour (col.withAlpha (0.06f));
            g.fillRoundedRectangle (bar, 1.0f);
            auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * v);
            if (i == cur)
            {
                g.setColour (col.withAlpha (0.25f));
                g.fillRoundedRectangle (fill.expanded (1.5f), 2.0f);
                g.setColour (col.brighter (0.5f));
            }
            else
                g.setColour (col.withAlpha (i % 4 == 0 ? 0.80f : 0.55f));
            g.fillRoundedRectangle (fill, 1.0f);
            // step dots
            const Point<float> dp (bar.getCentreX(), sp.getBottom() - 1.0f);
            if (i == cur) card::dotGlow (g, dp, 1.3f, col, 1.0f);
            else
            {
                g.setColour (col.withAlpha (0.22f));
                g.fillEllipse (Rectangle<float> (2.0f, 2.0f).withCentre (dp));
            }
        }
    }
    g.restoreState();
    card::paintScreenGlass (g, d, corner);
}

void ModUnit::paintJack (Graphics& g, const Section& sec)
{
    const auto col = modSourceColour (sec.source);
    const auto c = sec.jack.toFloat().getCentre();
    const bool hot = hoverJack == sec.source || dragSource == sec.source;
    int count = 0;
    for (auto& a : proc.getModAssignments()) if (a.source == sec.source) ++count;
    const float v = proc.getModSourceValue (sec.source);
    const float level = jlimit (0.0f, 1.0f, modSourceIsBipolar (sec.source) ? 0.5f + 0.5f * v : v);
    const float lit = hot ? 1.0f : (count > 0 ? 0.65f + 0.35f * level : 0.4f);
    const float R = kJackR;

    // halo
    ColourGradient halo (col.withAlpha (0.40f * lit), c.x, c.y, col.withAlpha (0.0f), c.x + R * 2.2f, c.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (Rectangle<float> (R * 4.4f, R * 4.4f).withCentre (c));
    // dark disc + glowing ring
    ColourGradient disc (Colour (0xff121a28), c.x, c.y - R, Colour (0xff060a11), c.x, c.y + R, false);
    g.setGradientFill (disc);
    g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));
    Path ring;
    ring.addEllipse (Rectangle<float> (R * 2.0f - 2.0f, R * 2.0f - 2.0f).withCentre (c));
    card::strokeGlow (g, ring, col.withAlpha (0.5f + 0.5f * lit), 1.5f, lit);
    // socket: the core lights up while the source is patched / hovered (empty while the cable is out)
    g.setColour (Colour (0xff020306));
    g.fillEllipse (Rectangle<float> (6.5f, 6.5f).withCentre (c));
    if (dragSource != sec.source)
        card::dotGlow (g, c, 2.0f, col, count > 0 || hot ? 0.6f + 0.4f * level : 0.3f);
}

// Source chip: pill in the source colour - live dot, name, patch count and the jack at the right end.
void ModUnit::paintChip (Graphics& g, const Section& sec)
{
    const auto r = sec.chip.toFloat();
    const auto col = modSourceColour (sec.source);
    const float rad = r.getHeight() * 0.5f;
    g.setColour (card::bg.withAlpha (0.85f));
    g.fillRoundedRectangle (r, rad);
    ColourGradient tint (col.withAlpha (0.12f), r.getX(), 0.0f, col.withAlpha (0.0f), r.getX() + r.getWidth() * 0.6f, 0.0f, false);
    g.setGradientFill (tint);
    g.fillRoundedRectangle (r, rad);
    g.setColour (col.withAlpha (hoverJack == sec.source ? 0.7f : 0.38f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);

    // live dot: brightness follows the source value
    const float v = proc.getModSourceValue (sec.source);
    const float level = jlimit (0.0f, 1.0f, modSourceIsBipolar (sec.source) ? 0.5f + 0.5f * v : v);
    card::dotGlow (g, { r.getX() + 12.0f, r.getCentreY() }, 3.0f, col, jlimit (0.12f, 1.0f, level));

    const auto f = smallFont (true);
    g.setFont (f);
    g.setColour (card::text);
    const float nameX = r.getX() + 21.0f;
    g.drawText (sec.title, Rectangle<float> (nameX, r.getY(), 60.0f, r.getHeight()), Justification::centredLeft, false);

    const String cnt = countText (sec.source);
    const auto right = Rectangle<float> (nameX + GlyphArrangement::getStringWidth (f, sec.title) + 6.0f, r.getY(),
                                         0.0f, r.getHeight()).withRight ((float) sec.jack.getX() - 2.0f);
    if (right.getWidth() > 14.0f)
    {
        g.setFont (smallFont());
        g.setColour (cnt.isNotEmpty() ? col : labelCol.withAlpha (0.8f));
        g.drawText (cnt.isNotEmpty() ? cnt : String ("OUT"), right, Justification::centredRight, false);
    }
    paintJack (g, sec);
}

void ModUnit::paintSection (Graphics& g, const Section& sec)
{
    const auto a = sec.area.toFloat();
    const auto col = modSourceColour (sec.source);

    // hairline divider on the left (not before the first section: the title divider is there)
    if (sec.source > 0)
    {
        ColourGradient div (card::ice.withAlpha (0.0f), 0.0f, a.getY() + 8.0f, card::ice.withAlpha (0.0f), 0.0f, a.getBottom() - 8.0f, false);
        div.addColour (0.5, card::ice.withAlpha (0.12f));
        g.setGradientFill (div);
        g.fillRect (a.getX(), a.getY() + 8.0f, 1.0f, a.getHeight() - 16.0f);
    }

    paintChip (g, sec);
    paintDisplay (g, sec);

    if (sec.source <= 1)
    {
        // FREE / SYNC toggle pill (click toggles)
        const bool sync = proc.getModSources().lfo[(size_t) sec.source].sync;
        const auto r = syncButtons[sec.source].toFloat();
        const float rad = r.getHeight() * 0.5f;
        if (sync)
        {
            g.setColour (col.withAlpha (0.16f));
            g.fillRoundedRectangle (r.expanded (1.0f), rad + 1.0f);
            g.setColour (col.withAlpha (0.30f));
            g.fillRoundedRectangle (r, rad);
            g.setColour (col.withAlpha (0.85f));
        }
        else
        {
            g.setColour (card::bg);
            g.fillRoundedRectangle (r, rad);
            g.setColour (card::line.withMultipliedAlpha (1.6f));
        }
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
        g.setFont (smallFont (sync));
        g.setColour (sync ? card::text : labelCol);
        g.drawFittedText (sync ? "SYNC" : "FREE", r.toNearestInt(), Justification::centred, 1, 0.85f);
    }
}

void ModUnit::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float corner = expanded ? kCorner : jmin (kCorner, b.getHeight() * 0.5f);
    // TrapWin card (the left accent edge in the source colours)
    {
        Path shape;
        shape.addRoundedRectangle (b, corner);
        ColourGradient body (card::card2, 0.0f, b.getY(), Colour (0xff080c14), 0.0f, b.getBottom(), false);
        body.addColour (0.45, card::card);
        g.setGradientFill (body);
        g.fillPath (shape);
        g.saveState();
        g.reduceClipRegion (shape);
        if (expanded)
            for (auto& s : sections)   // the four source colours breathe in very faintly from below each section
            {
                const auto a = s.area.toFloat();
                ColourGradient glow (modSourceColour (s.source).withAlpha (0.05f), a.getCentreX(), b.getBottom(),
                                     modSourceColour (s.source).withAlpha (0.0f), a.getCentreX(), b.getY(), true);
                g.setGradientFill (glow);
                g.fillRect (a.withY (b.getY()).withHeight (b.getHeight()));
            }
        ColourGradient sheen (Colours::white.withAlpha (0.035f), 0.0f, b.getY(), Colours::white.withAlpha (0.0f), 0.0f, b.getY() + 30.0f, false);
        g.setGradientFill (sheen);
        g.fillRect (b);
        g.restoreState();
        g.setColour (card::line);
        g.drawRoundedRectangle (b.reduced (0.5f), corner - 0.5f, 1.0f);
        ColourGradient top (card::ice.withAlpha (0.0f), b.getX() + corner, 0.0f, card::ice.withAlpha (0.0f), b.getRight() - corner, 0.0f, false);
        top.addColour (0.3, card::ice.withAlpha (0.16f));
        top.addColour (0.7, card::ice.withAlpha (0.10f));
        g.setGradientFill (top);
        g.fillRect (b.getX() + corner, b.getY() + 0.5f, b.getWidth() - 2.0f * corner, 0.8f);
        // 2 px accent edge on the left
        const float y0 = b.getY() + corner * 0.6f, y1 = b.getBottom() - corner * 0.6f;
        ColourGradient edge (card::ice.withAlpha (0.0f), 0.0f, y0, card::ice.withAlpha (0.0f), 0.0f, y1, false);
        edge.addColour (0.2, card::ice.withAlpha (0.9f));
        edge.addColour (0.5, Colour (0xffb07cff).withAlpha (0.9f));
        edge.addColour (0.8, Colour (0xff5fe0b0).withAlpha (0.9f));
        g.setGradientFill (edge);
        g.fillRoundedRectangle (Rectangle<float> (b.getX() + 1.0f, y0, 2.0f, y1 - y0), 1.0f);
    }
    paintTitle (g);
    if (expanded) for (auto& s : sections) paintSection (g, s);
    else          for (auto& s : sections) paintChip (g, s);
    paintEditPill (g);
}

// ============================================================================
// Mouse
// ============================================================================
int ModUnit::jackAt (Point<int> p) const
{
    for (auto& s : sections)
        if (s.jack.expanded (2).contains (p)) return s.source;
    return -1;
}

int ModUnit::seqStepAt (Point<int> p) const
{
    const auto d = seqPlot();
    if (! expanded || ! sections[3].display.contains (p)) return -1;
    return jlimit (0, kSeqSteps - 1, (int) ((float) (p.x - d.getX()) / d.getWidth() * (float) kSeqSteps));
}

void ModUnit::setSeqFromMouse (Point<int> p)
{
    const auto d = seqPlot();
    const int step = jlimit (0, kSeqSteps - 1, (int) ((float) (p.x - d.getX()) / d.getWidth() * (float) kSeqSteps));
    const float v = jlimit (0.0f, 1.0f, (d.getBottom() - 5.0f - (float) p.y) / (d.getHeight() - 5.0f));
    const float q = (float) roundToInt (v * 16.0f) / 16.0f;
    if (step != seqDragStep || q != lastSeqValue)
    {
        seqDragStep = step;
        lastSeqValue = q;
        proc.setSeqStep (step, q);
        repaint (sections[3].display.expanded (2));
    }
}

void ModUnit::mouseDown (const MouseEvent& e)
{
    const auto p = e.getPosition();
    if (const int j = jackAt (p); j >= 0)
    {
        if (e.mods.isPopupMenu()) { showAssignmentMenu (j); return; }
        dragSource = j;
        repaint();
        return;
    }
    if (editPill.contains (p)) { setExpanded (! expanded); return; }
    if (! expanded)
    {
        for (auto& s : sections)
            if (s.chip.contains (p)) { setExpanded (true); return; }
        return;
    }
    for (int i = 0; i < 2; ++i)
        if (syncButtons[i].contains (p))
        {
            proc.pushUndo();
            auto l = proc.getModSources().lfo[(size_t) i];
            l.sync = ! l.sync;
            proc.setLfo (i, l);
            refresh();
            return;
        }
    if (seqStepAt (p) >= 0)
    {
        proc.pushUndo();
        seqDragStep = -1;
        setSeqFromMouse (p);
    }
}

void ModUnit::mouseDrag (const MouseEvent& e)
{
    if (dragSource >= 0)
    {
        setMouseCursor (MouseCursor::DraggingHandCursor);
        if (onHandleDrag) onHandleDrag (dragSource, e.getScreenPosition());
        return;
    }
    if (seqDragStep >= 0) setSeqFromMouse (e.getPosition());
}

void ModUnit::mouseUp (const MouseEvent& e)
{
    if (dragSource >= 0)
    {
        const int src = dragSource;
        dragSource = -1;
        setMouseCursor (MouseCursor::NormalCursor);
        repaint();
        if (e.mouseWasDraggedSinceMouseDown() && onHandleDrop) onHandleDrop (src, e.getScreenPosition());
        return;
    }
    seqDragStep = -1;
}

void ModUnit::mouseMove (const MouseEvent& e)
{
    const bool eh = editPill.contains (e.getPosition());
    if (eh != editHover) { editHover = eh; repaint (editPill.expanded (6)); }
    const int j = jackAt (e.getPosition());
    bool overChip = false;
    if (! expanded) for (auto& s : sections) overChip = overChip || s.chip.contains (e.getPosition());
    setMouseCursor (j >= 0 || eh || overChip ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    if (j != hoverJack)
    {
        hoverJack = j;
        setTooltip (j >= 0 ? String (modSourceName (j)) + ": drag onto any knob to modulate it. Right-click: list / remove."
                           : String ("MOD SOURCES - drag a jack onto any knob in the chain to modulate it"));
        for (auto& s : sections) repaint (s.chip.expanded (16));
    }
}

void ModUnit::mouseExit (const MouseEvent&)
{
    if (hoverJack >= 0 || editHover) { hoverJack = -1; editHover = false; repaint(); }
}

void ModUnit::showAssignmentMenu (int source)
{
    PopupMenu m;
    m.addSectionHeader (String (modSourceName (source)) + " -> ");
    const auto list = proc.getModAssignments();
    int shown = 0;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto& a = list[(size_t) i];
        if (a.source != source) continue;
        String target;
        if (a.isMacro()) target = MacroEngine::macroName (a.param);
        else
        {
            const auto type = proc.getSlotType (a.slot);
            target = "S" + String (a.slot + 1) + " ";
            if (a.param == kNumParams) target << "MIX";
            else if (type != EffectType::None) target << effectInfo (type).name << " " << effectInfo (type).params[(size_t) a.param].name;
            else target << "P" << (a.param + 1);
        }
        const int pct = roundToInt (a.depth * 100.0f);
        m.addItem (1000 + i, "Remove  " + target + "  (" + (pct > 0 ? "+" : "") + String (pct) + " %)");
        ++shown;
    }
    if (shown == 0) m.addItem (-1, "Not patched - drag the jack onto a knob", false, false);
    else { m.addSeparator(); m.addItem (2000, "Remove all from " + String (modSourceName (source))); }

    SafePointer<ModUnit> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (sections[(size_t) source].jack)),
                     [safe, source] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         auto& p = safe->proc;
                         if (r == 2000)
                         {
                             auto list2 = p.getModAssignments();
                             auto st = p.getModSources();
                             for (int i = (int) list2.size() - 1; i >= 0; --i)
                                 if (list2[(size_t) i].source == source) st.remove (i);
                             p.setModSources (st, true);
                         }
                         else if (r >= 1000) p.removeModAssignment (r - 1000);
                         safe->refresh();
                     });
}

} // namespace ek::ui
