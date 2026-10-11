#include "ModUnit.h"

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
        case 1:  return Colour (0xffc58bff); // LFO 2 - violet
        case 2:  return Colour (0xffffc861); // ENV   - amber
        case 3:  return Colour (0xff3dff6e); // SEQ   - green
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
    const float thick = jmax (2.0f, knobBounds.getWidth() * 0.07f);
    baseNorm = jlimit (0.0f, 1.0f, baseNorm);
    depth = jlimit (-1.0f, 1.0f, depth);

    // faint track of the whole knob range
    Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, theme::knobStart, theme::knobEnd, true);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.strokePath (track, PathStrokeType (thick + 1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (colour.withAlpha (0.12f));
    g.strokePath (track, PathStrokeType (thick * 0.6f, PathStrokeType::curved, PathStrokeType::rounded));

    const float lo = jlimit (0.0f, 1.0f, bipolar ? baseNorm - std::abs (depth) : jmin (baseNorm, baseNorm + depth));
    const float hi = jlimit (0.0f, 1.0f, bipolar ? baseNorm + std::abs (depth) : jmax (baseNorm, baseNorm + depth));
    if (hi - lo > 1.0e-4f)
    {
        Path range;
        range.addCentredArc (c.x, c.y, r, r, 0.0f, normToAngle (lo), normToAngle (hi), true);
        g.setColour (colour.withAlpha (0.28f));                          // glow
        g.strokePath (range, PathStrokeType (thick * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (range, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
        // the direction of a positive depth gets the brighter end cap
        const float tip = bipolar ? (depth >= 0.0f ? hi : lo) : jlimit (0.0f, 1.0f, baseNorm + depth);
        g.setColour (colour.brighter (0.8f));
        g.fillEllipse (Rectangle<float> (thick * 1.7f, thick * 1.7f).withCentre (polarPt (c, r, normToAngle (tip))));
    }
    // base marker
    g.setColour (Colours::white.withAlpha (0.85f));
    g.drawLine (Line<float> (polarPt (c, r - thick, normToAngle (baseNorm)), polarPt (c, r + thick, normToAngle (baseNorm))), 1.2f);

    if (currentNorm >= 0.0f)
    {
        const auto p = polarPt (c, r, normToAngle (currentNorm));
        g.setColour (colour.withAlpha (0.45f));
        g.fillEllipse (Rectangle<float> (thick * 3.2f, thick * 3.2f).withCentre (p));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (thick * 1.4f, thick * 1.4f).withCentre (p));
    }
}

void paintPatchCable (Graphics& g, Point<float> from, Point<float> to, Colour colour)
{
    const float dist = from.getDistanceFrom (to);
    const float sag = jlimit (10.0f, 90.0f, dist * 0.25f);
    Path cable;
    cable.startNewSubPath (from);
    cable.cubicTo (from.translated (0.0f, sag), to.translated (0.0f, sag), to);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.strokePath (cable, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (1.5f, 3.0f));
    g.setColour (colour.darker (0.9f));
    g.strokePath (cable, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (colour);
    g.strokePath (cable, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.strokePath (cable, PathStrokeType (1.0f, PathStrokeType::curved, PathStrokeType::rounded), AffineTransform::translation (-0.6f, -0.8f));
    // plug at the free end
    for (auto p : { from, to })
    {
        ColourGradient plug (Colour (0xffeef2f8), p.x - 4.0f, p.y - 4.0f, Colour (0xff4a505c), p.x + 4.0f, p.y + 4.0f, false);
        g.setGradientFill (plug);
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (p));
        g.setColour (colour);
        g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (p), 1.2f);
    }
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
// MiniKnob: small rotary knob with label + value, driven by callbacks
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
    }

    void tick()
    {
        const float v = get();
        const auto t = text();
        if (v != shown || t != shownText) { shown = v; shownText = t; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setFont (theme::monoLabel (8.5f));
        g.setColour (theme::silverDim);
        g.drawFittedText (label, getLocalBounds().withHeight (10).reduced (2, 0), Justification::centred, 1, 0.6f);

        const auto c = Point<float> (b.getCentreX(), 26.0f);
        const float r = 10.0f;
        // value arc
        Path track;
        track.addCentredArc (c.x, c.y, r + 3.5f, r + 3.5f, 0.0f, theme::knobStart, theme::knobEnd, true);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.strokePath (track, PathStrokeType (2.6f));
        Path val;
        val.addCentredArc (c.x, c.y, r + 3.5f, r + 3.5f, 0.0f, theme::knobStart, normToAngle (get()), true);
        g.setColour (colour.withAlpha (isMouseOverOrDragging() ? 1.0f : 0.75f));
        g.strokePath (val, PathStrokeType (1.6f));
        theme::drawKnob (g, c, r, normToAngle (get()), isMouseOverOrDragging());

        g.setFont (theme::monoLabel (8.5f));
        g.setColour (colour.interpolatedWith (theme::silver, 0.45f));
        g.drawFittedText (text(), Rectangle<int> (0, 42, getWidth(), 10), Justification::centred, 1, 0.7f);
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
// Layout (local coordinates of the 932 x 96 unit)
// ============================================================================
namespace
{
constexpr int kEar = 18;
constexpr int kTitleW = 60;
constexpr int kSecX = kEar + kTitleW;                     // 78
constexpr int kSecW[kNumModSources] = { 210, 210, 196, 220 }; // 78 + 836 + 18 = 932
constexpr int kKnobW = 32, kKnobTop = 22, kKnobH = 56;
}

ModUnit::ModUnit (EffectorKillaAudioProcessor& p) : proc (p)
{
    setSize (kWidth, kHeight);
    setOpaque (true);
    static const char* titles[] = { "LFO 1", "LFO 2", "ENV FOLLOW", "STEP SEQ" };

    int x = kSecX;
    for (int s = 0; s < kNumModSources; ++s)
    {
        auto& sec = sections[(size_t) s];
        sec.source = s;
        sec.title = titles[s];
        sec.area = { x, 4, kSecW[s], kHeight - 8 };
        x += kSecW[s];
    }

    // ---- display / jack geometry -------------------------------------------------------------
    for (int i = 0; i < 2; ++i)
    {
        auto& sec = sections[(size_t) i];
        sec.display = { sec.area.getX() + 8, 24, 68, 40 };
        sec.jack = Rectangle<int> (26, 26).withCentre ({ sec.area.getX() + 193, 47 });
        syncButtons[i] = { sec.area.getX() + 8, 68, 68, 14 };
    }
    sections[2].display = { sections[2].area.getX() + 8, 24, 54, 58 };
    sections[2].jack = Rectangle<int> (26, 26).withCentre ({ sections[2].area.getX() + 179, 47 });
    sections[3].display = { sections[3].area.getX() + 8, 24, 110, 58 };
    sections[3].jack = Rectangle<int> (26, 26).withCentre ({ sections[3].area.getX() + 203, 47 });

    // ---- knobs -------------------------------------------------------------------------------
    auto addKnob = [this] (int cx, String label, int source, std::function<float()> g, std::function<void (float)> s,
                           std::function<String()> t, float def, std::function<int()> steps = {})
    {
        auto k = std::make_unique<MiniKnob> (proc, label, modSourceColour (source), std::move (g), std::move (s), std::move (t), def, std::move (steps));
        k->setBounds (cx - kKnobW / 2, kKnobTop, kKnobW, kKnobH);
        addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
    };

    const int nDiv = numModDivisions();
    for (int i = 0; i < 2; ++i)
    {
        const int ax = sections[(size_t) i].area.getX();
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
        const int ax = sections[2].area.getX();
        auto env = [this] { return proc.getModSources().env; };
        addKnob (ax + 82, "ATTACK", 2,
                 [env] { return logNorm (env().attackMs, 0.1f, 500.0f); },
                 [this, env] (float v) { auto e = env(); e.attackMs = logValue (v, 0.1f, 500.0f); proc.setEnvFollower (e); },
                 [env] { return msText (env().attackMs); }, logNorm (10.0f, 0.1f, 500.0f));
        addKnob (ax + 114, "RELEASE", 2,
                 [env] { return logNorm (env().releaseMs, 5.0f, 5000.0f); },
                 [this, env] (float v) { auto e = env(); e.releaseMs = logValue (v, 5.0f, 5000.0f); proc.setEnvFollower (e); },
                 [env] { return msText (env().releaseMs); }, logNorm (150.0f, 5.0f, 5000.0f));
        addKnob (ax + 146, "SENS", 2,
                 [env] { return env().sensitivity; },
                 [this, env] (float v) { auto e = env(); e.sensitivity = v; proc.setEnvFollower (e); },
                 [env] { return "+" + String (roundToInt (EnvSettings::sensitivityGainDb (env().sensitivity))) + "dB"; }, 0.5f);
    }

    {
        const int ax = sections[3].area.getX();
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

    setTooltip ("MOD SOURCES - drag a jack onto any knob in the rack to modulate it");
}

ModUnit::~ModUnit() = default;

void ModUnit::resized() {}

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
    // envelope history for the ENV display
    envHistory[(size_t) envPos] = proc.getModSourceValue ((int) ModSourceId::Env);
    envPos = (envPos + 1) % (int) envHistory.size();

    for (auto& k : knobs) k->tick();
    for (int s = 0; s < kNumModSources; ++s)
    {
        const float v = proc.getModSourceValue (s);
        if (std::abs (v - shownValue[(size_t) s]) > 1.0e-3f || s == (int) ModSourceId::Env)
        {
            shownValue[(size_t) s] = v;
            repaint (sections[(size_t) s].display.expanded (2));
            repaint (sections[(size_t) s].area.withHeight (20));
        }
    }
    const int st = proc.getSeqStep();
    if (st != shownStep) { shownStep = st; repaint (sections[3].display.expanded (2)); }
    repaint (Rectangle<int> (kEar, 60, kTitleW, 30)); // ACTIVE LED
}

// ============================================================================
// Painting
// ============================================================================
static void drawScrew (Graphics& g, Point<float> c, float r, float angle)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (r * 2.4f, r * 2.4f).withCentre (c.translated (0.4f, 0.8f)));
    ColourGradient head (Colour (0xffe9edf3), c.x - r, c.y - r, Colour (0xff3c424d), c.x + r, c.y + r, false);
    g.setGradientFill (head);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 0.6f);
    g.setColour (Colour (0xff20242b));
    for (float a : { angle, angle + MathConstants<float>::halfPi })
        g.drawLine (Line<float> (polarPt (c, r * 0.72f, a), polarPt (c, r * 0.72f, a + MathConstants<float>::pi)), r * 0.32f);
}

void ModUnit::paintChassis (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (Colour (0xff07090d));

    // rack ears (lighter, rougher metal with oval slots)
    for (auto ear : { b.withWidth ((float) kEar), b.withLeft (b.getRight() - (float) kEar) })
    {
        ColourGradient eg (Colour (0xff2a2e36), ear.getX(), 0.0f, Colour (0xff15181d), ear.getRight(), 0.0f, false);
        g.setGradientFill (eg);
        g.fillRect (ear);
        for (float y : { 20.0f, 76.0f })
        {
            auto slot = Rectangle<float> (7.0f, 13.0f).withCentre ({ ear.getCentreX(), y });
            g.setColour (Colour (0xff030406));
            g.fillRoundedRectangle (slot, 3.5f);
            g.setColour (Colours::white.withAlpha (0.10f));
            g.drawRoundedRectangle (slot.translated (0.0f, 0.6f), 3.5f, 0.6f);
        }
    }

    // faceplate: black anodised, brushed
    auto face = b.withTrimmedLeft ((float) kEar).withTrimmedRight ((float) kEar);
    ColourGradient fg (Colour (0xff1b1e25), 0.0f, face.getY(), Colour (0xff0d0f13), 0.0f, face.getBottom(), false);
    g.setGradientFill (fg);
    g.fillRect (face);
    Random rnd (0x6d6f64);
    for (float y = face.getY() + 1.0f; y < face.getBottom(); y += 1.5f)
    {
        g.setColour (Colours::white.withAlpha (0.012f + 0.018f * rnd.nextFloat()));
        g.drawHorizontalLine ((int) y, face.getX(), face.getRight());
    }
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawHorizontalLine (0, face.getX(), face.getRight());
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawHorizontalLine (getHeight() - 1, face.getX(), face.getRight());

    // screws in the corners
    for (auto c : { Point<float> (face.getX() + 9.0f, 10.0f), Point<float> (face.getX() + 9.0f, b.getBottom() - 10.0f),
                    Point<float> (face.getRight() - 9.0f, 10.0f), Point<float> (face.getRight() - 9.0f, b.getBottom() - 10.0f) })
        drawScrew (g, c, 4.2f, 0.35f + c.x * 0.01f);

    // title block
    const auto title = Rectangle<float> ((float) kEar + 6.0f, 20.0f, (float) kTitleW - 8.0f, 30.0f);
    theme::drawChromeText (g, "MOD", title, theme::stencilFont (22.0f), Justification::centredLeft, 0.6f);
    g.setFont (theme::monoLabel (9.0f));
    g.setColour (theme::accent);
    g.drawText ("SOURCES", title.translated (0.0f, 26.0f).withHeight (11.0f), Justification::centredLeft, false);
    const bool active = proc.engine.modMeters.active.load (std::memory_order_relaxed);
    theme::drawLed (g, { (float) kEar + 13.0f, 76.0f }, 3.2f, theme::ledGreen, active, 0.8f);
    g.setFont (theme::monoLabel (8.0f));
    g.setColour (theme::silverDim);
    g.drawText ("ACTIVE", Rectangle<float> ((float) kEar + 21.0f, 71.0f, 50.0f, 10.0f), Justification::centredLeft, false);
}

void ModUnit::paintDisplay (Graphics& g, const Section& sec)
{
    const auto d = sec.display.toFloat();
    const auto col = modSourceColour (sec.source);

    // black glass with a bevel
    theme::drawSocket (g, d.expanded (2.0f), 4.0f);
    g.setColour (Colour (0xff03060a));
    g.fillRoundedRectangle (d, 3.0f);
    g.setColour (col.withAlpha (0.07f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (d.getY() + d.getHeight() * (float) i / 4.0f), d.getX() + 2.0f, d.getRight() - 2.0f);
    for (int i = 1; i < 6; ++i) g.drawVerticalLine ((int) (d.getX() + d.getWidth() * (float) i / 6.0f), d.getY() + 2.0f, d.getBottom() - 2.0f);

    const auto plot = d.reduced (4.0f, 5.0f);
    auto yOf = [&] (float v, bool bipolar) { return bipolar ? plot.getCentreY() - v * plot.getHeight() * 0.5f : plot.getBottom() - v * plot.getHeight(); };
    auto glowStroke = [&] (const Path& p)
    {
        g.setColour (col.withAlpha (0.22f));
        g.strokePath (p, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (p, PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
    };

    const auto& ms = proc.getModSources();
    g.setFont (theme::monoLabel (7.5f));
    if (sec.source <= 1)
    {
        const auto& l = ms.lfo[(size_t) sec.source];
        Path wave;
        const int n = 64;
        for (int i = 0; i <= n; ++i)
        {
            const double ph = (double) i / n;
            const float v = l.shape == LfoShape::SampleHold
                              ? ModSourceEngine::lfoShape (l.shape, 0.0, (juce::int64) (ph * 7.999), sec.source)
                              : ModSourceEngine::lfoShape (l.shape, ph, 0, sec.source);
            const Point<float> p (plot.getX() + plot.getWidth() * (float) ph, yOf (v * 0.92f, true));
            if (i == 0) wave.startNewSubPath (p); else wave.lineTo (p);
        }
        glowStroke (wave);
        const float ph = proc.getLfoPhase (sec.source);
        const float v = proc.getModSourceValue (sec.source);
        const Point<float> dot (plot.getX() + plot.getWidth() * ph, yOf (v * 0.92f, true));
        g.setColour (col.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (dot));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (3.6f, 3.6f).withCentre (dot));
        g.setColour (col.withAlpha (0.8f));
        g.drawText (lfoShapeName (l.shape), d.reduced (3.0f, 1.0f).withHeight (9.0f), Justification::centredLeft, false);
    }
    else if (sec.source == (int) ModSourceId::Env)
    {
        Path area, line;
        const int n = (int) envHistory.size();
        for (int i = 0; i < n; ++i)
        {
            const float v = envHistory[(size_t) ((envPos + i) % n)];
            const Point<float> p (plot.getX() + plot.getWidth() * (float) i / (float) (n - 1), yOf (jlimit (0.0f, 1.0f, v), false));
            if (i == 0) { line.startNewSubPath (p); area.startNewSubPath (p.x, plot.getBottom()); }
            else line.lineTo (p);
            area.lineTo (p);
        }
        area.lineTo (plot.getRight(), plot.getBottom());
        area.closeSubPath();
        g.setColour (col.withAlpha (0.18f));
        g.fillPath (area);
        glowStroke (line);
        // level LED column at the right edge
        const float v = proc.getModSourceValue ((int) ModSourceId::Env);
        for (int k = 0; k < 6; ++k)
        {
            const auto led = Rectangle<float> (3.0f, 5.0f).withCentre ({ d.getRight() - 4.0f, d.getBottom() - 6.0f - (float) k * 8.0f });
            g.setColour (v * 6.0f > (float) k ? col : col.withAlpha (0.12f));
            g.fillRect (led);
        }
    }
    else
    {
        const auto& sq = ms.seq;
        const int cur = proc.getSeqStep();
        const float w = plot.getWidth() / (float) kSeqSteps;
        for (int i = 0; i < kSeqSteps; ++i)
        {
            const float v = jlimit (0.0f, 1.0f, sq.steps[(size_t) i]);
            auto bar = Rectangle<float> (plot.getX() + w * (float) i + 0.8f, plot.getY(), w - 1.6f, plot.getHeight() - 5.0f);
            g.setColour (col.withAlpha (0.08f));
            g.fillRect (bar);
            auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * v);
            g.setColour (i == cur ? col.brighter (0.6f) : col.withAlpha (i % 4 == 0 ? 0.75f : 0.55f));
            g.fillRect (fill);
            // step LEDs
            g.setColour (i == cur ? Colours::white : col.withAlpha (0.18f));
            g.fillRect (Rectangle<float> (w - 2.6f, 2.0f).withCentre ({ bar.getCentreX(), plot.getBottom() - 1.0f }));
        }
    }

    // glass reflection
    ColourGradient gloss (Colours::white.withAlpha (0.08f), d.getX(), d.getY(), Colours::white.withAlpha (0.0f), d.getX(), d.getCentreY(), false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (d.withHeight (d.getHeight() * 0.5f), 3.0f);
}

void ModUnit::paintJack (Graphics& g, const Section& sec)
{
    const auto col = modSourceColour (sec.source);
    const auto c = sec.jack.toFloat().getCentre();
    const bool hot = hoverJack == sec.source || dragSource == sec.source;
    int count = 0;
    for (auto& a : proc.getModAssignments()) if (a.source == sec.source) ++count;

    // hex nut
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = polarPt (c, 12.0f, (float) i * MathConstants<float>::pi / 3.0f + 0.52f);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillPath (hex, AffineTransform::translation (0.6f, 1.4f));
    ColourGradient nut (Colour (0xffdfe5ee), c.x - 10.0f, c.y - 10.0f, Colour (0xff3a404b), c.x + 10.0f, c.y + 10.0f, false);
    g.setGradientFill (nut);
    g.fillPath (hex);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.strokePath (hex, PathStrokeType (0.8f));

    // coloured LED ring (lit when patched / hovered)
    const float lit = hot ? 1.0f : (count > 0 ? 0.85f : 0.25f);
    if (lit > 0.5f)
    {
        ColourGradient halo (col.withAlpha (0.45f * lit), c.x, c.y, col.withAlpha (0.0f), c.x + 20.0f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (40.0f, 40.0f).withCentre (c));
    }
    g.setColour (Colour (0xff0b0d11));
    g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (c));
    g.setColour (col.withAlpha (lit));
    g.drawEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (c), 2.2f);

    // the socket (empty while the cable is being dragged out)
    g.setColour (Colour (0xff010203));
    g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c));
    if (dragSource != sec.source)
    {
        g.setColour (Colours::white.withAlpha (0.25f));
        g.fillEllipse (Rectangle<float> (2.0f, 2.0f).withCentre (c.translated (-1.2f, -1.2f)));
    }

    g.setFont (theme::monoLabel (8.0f));
    g.setColour (count > 0 ? col : theme::silverDim);
    g.drawText (countText (sec.source), Rectangle<float> (c.x - 14.0f, c.y + 15.0f, 28.0f, 10.0f), Justification::centred, false);
    g.setColour (theme::silverDim.withAlpha (0.8f));
    g.drawText ("OUT", Rectangle<float> (c.x - 14.0f, c.y - 25.0f, 28.0f, 9.0f), Justification::centred, false);
}

void ModUnit::paintSection (Graphics& g, const Section& sec)
{
    const auto a = sec.area.toFloat();
    const auto col = modSourceColour (sec.source);

    // engraved separator on the left
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawVerticalLine ((int) a.getX(), a.getY() + 2.0f, a.getBottom() - 2.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawVerticalLine ((int) a.getX() + 1, a.getY() + 2.0f, a.getBottom() - 2.0f);

    // header: label + LED + ice-blue rule
    const float v = proc.getModSourceValue (sec.source);
    const float level = modSourceIsBipolar (sec.source) ? 0.5f + 0.5f * v : v;
    theme::drawLed (g, { a.getX() + 12.0f, a.getY() + 7.5f }, 2.6f, col, level > 0.02f, jlimit (0.0f, 1.0f, level));
    g.setFont (theme::monoLabel (9.5f));
    g.setColour (theme::silver);
    g.drawText (sec.title, Rectangle<float> (a.getX() + 20.0f, a.getY() + 1.0f, 90.0f, 13.0f), Justification::centredLeft, false);
    const float textW = GlyphArrangement::getStringWidth (theme::monoLabel (9.5f), sec.title);
    g.setColour (theme::accent.withAlpha (0.55f));
    g.fillRect (Rectangle<float> (a.getX() + 26.0f + textW, a.getY() + 7.0f, a.getRight() - (a.getX() + 34.0f + textW), 1.0f));

    paintDisplay (g, sec);
    paintJack (g, sec);

    if (sec.source <= 1)
    {
        const bool sync = proc.getModSources().lfo[(size_t) sec.source].sync;
        const auto r = syncButtons[sec.source].toFloat();
        auto half = r.withWidth (r.getWidth() * 0.5f - 1.0f);
        for (int k = 0; k < 2; ++k)
        {
            const bool on = (k == 1) == sync;
            const auto bb = k == 0 ? half : half.withX (r.getCentreX() + 1.0f);
            g.setColour (Colour (0xff07090c));
            g.fillRoundedRectangle (bb, 2.5f);
            g.setColour (on ? col.withAlpha (0.22f) : Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (bb.reduced (1.0f), 2.0f);
            g.setColour (on ? col : Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (bb.reduced (0.5f), 2.5f, 0.8f);
            g.setFont (theme::monoLabel (8.0f));
            g.setColour (on ? theme::silver : theme::silverDim.withAlpha (0.6f));
            g.drawText (k == 0 ? "FREE" : "SYNC", bb, Justification::centred, false);
        }
    }
}

void ModUnit::paint (Graphics& g)
{
    paintChassis (g);
    for (auto& s : sections) paintSection (g, s);
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawVerticalLine (kWidth - kEar - 1, 6.0f, (float) kHeight - 6.0f);
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
    const auto d = sections[3].display.toFloat().reduced (4.0f, 5.0f);
    if (! sections[3].display.contains (p)) return -1;
    return jlimit (0, kSeqSteps - 1, (int) ((float) (p.x - d.getX()) / d.getWidth() * (float) kSeqSteps));
}

void ModUnit::setSeqFromMouse (Point<int> p)
{
    const auto d = sections[3].display.toFloat().reduced (4.0f, 5.0f);
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
    for (int i = 0; i < 2; ++i)
        if (syncButtons[i].contains (p))
        {
            proc.pushUndo();
            auto l = proc.getModSources().lfo[(size_t) i];
            l.sync = p.x >= syncButtons[i].getCentreX();
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
    const int j = jackAt (e.getPosition());
    if (j != hoverJack)
    {
        hoverJack = j;
        setMouseCursor (j >= 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
        setTooltip (j >= 0 ? String (modSourceName (j)) + ": drag onto any knob to modulate it. Right-click: list / remove."
                           : String ("MOD SOURCES - drag a jack onto any knob in the rack to modulate it"));
        for (auto& s : sections) repaint (s.jack.expanded (16));
    }
}

void ModUnit::mouseExit (const MouseEvent&)
{
    if (hoverJack >= 0) { hoverJack = -1; repaint(); }
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
