#include "SplitUnit.h"
#include "CardStyle.h"

namespace ek::ui
{
using namespace juce;

namespace
{
const Colour branchA { 0xff9fc3ff };  // ice blue
const Colour branchB { 0xffb07cff };  // violet
const Colour killRed { 0xffff2e3e };
constexpr float kCorner = 16.0f;
const Colour labelCol { 0xffc8d3e6 };   // readable label colour (Docs/TRAPWIN-STYLE.md)
Font smallFont (bool bold = false) { return card::monoFont (11.0f, bold).withExtraKerningFactor (0.02f); }
constexpr int kKnobW = 54, kKnobH = 60;
void noFocus (Component& c) { c.setWantsKeyboardFocus (false); c.setMouseClickGrabsKeyboardFocus (false); }
} // namespace

// ============================================================================
// Mode segment of the segmented pill (the pill itself is painted by SplitUnit).
class SplitUnit::ModeButton : public Component, public SettableTooltipClient
{
public:
    ModeButton (SplitMode m, std::function<void (SplitMode)> click) : mode (m), onClick (std::move (click))
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
    }

    void setLit (bool l) { if (l != lit) { lit = l; repaint(); } }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f, 2.5f);
        const float rad = r.getHeight() * 0.5f;
        if (lit)
        {
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (card::iceDeep.withAlpha (0.14f * (float) (4 - k)));
                g.fillRoundedRectangle (r.expanded ((float) k * 0.8f), rad + (float) k * 0.8f);
            }
            ColourGradient fill (card::ice.withAlpha (0.42f), 0.0f, r.getY(), card::iceDeep.withAlpha (0.38f), 0.0f, r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, rad);
            g.setColour (card::ice.withAlpha (0.9f));
            g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
        }
        else if (isMouseOver())
        {
            g.setColour (card::ice.withAlpha (down ? 0.14f : 0.07f));
            g.fillRoundedRectangle (r, rad);
        }
        g.setFont (smallFont (lit));
        g.setColour (lit ? Colours::white : (isMouseOver() ? card::text : labelCol));
        g.drawFittedText (splitModeName (mode), r.toNearestInt(), Justification::centred, 1, 0.8f);
    }

    void mouseDown (const MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        down = false;
        repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick (mode);
    }

private:
    SplitMode mode;
    std::function<void (SplitMode)> onClick;
    bool lit = false, down = false;
};

// ============================================================================
// Flat TrapWin knob: label above, value below (Plex Mono), drag / shift / double-click / wheel.
class SplitUnit::FlatKnob : public Component, public SettableTooltipClient
{
public:
    FlatKnob (String n, Colour a) : name (std::move (n)), accent (a)
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setRepaintsOnMouseActivity (true);
    }

    std::function<void()> onGestureStart;
    std::function<void (float)> onUserChange;
    std::function<String()> valueText;

    void setDefault (float d) { defaultValue = d; }
    void setNormValue (float v) { v = jlimit (0.0f, 1.0f, v); if (v != value) { value = v; repaint(); } }
    float getNormValue() const { return value; }
    void tick()
    {
        const auto t = valueText ? valueText() : String();
        if (t != shownText) { shownText = t; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool hot = isMouseOverOrDragging() && isEnabled();
        g.setFont (smallFont());
        g.setColour (hot ? card::text : labelCol);
        g.drawFittedText (name, getLocalBounds().withHeight (13), Justification::centred, 1, 0.85f);
        const Point<float> c (b.getCentreX(), 29.0f);
        const float r = 15.0f;
        if (hot)
        {
            ColourGradient halo (accent.withAlpha (0.22f), c.x, c.y, accent.withAlpha (0.0f), c.x + r * 2.2f, c.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (Rectangle<float> (r * 4.4f, r * 4.4f).withCentre (c));
        }
        card::paintKnob (g, c, r, value, accent);
        g.setFont (smallFont());
        g.setColour (card::text);
        g.drawFittedText (shownText, Rectangle<int> (0, 46, getWidth(), 14), Justification::centred, 1, 0.85f);
    }

    void mouseDown (const MouseEvent&) override { if (onGestureStart) onGestureStart(); start = value; }
    void mouseDrag (const MouseEvent& e) override
    {
        const float range = e.mods.isShiftDown() ? 800.0f : 160.0f;
        userSet (start - (float) e.getDistanceFromDragStartY() / range);
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onGestureStart) onGestureStart(); userSet (defaultValue); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        userSet (value + (w.deltaY > 0.0f ? 0.03f : -0.03f));
    }

private:
    void userSet (float v)
    {
        v = jlimit (0.0f, 1.0f, v);
        if (v == value) return;
        value = v;
        repaint();
        if (onUserChange) onUserChange (v);
    }
    String name, shownText;
    Colour accent;
    float value = 0.5f, defaultValue = 0.5f, start = 0.0f;
};

// ============================================================================
// Small round close pill (removes the split).
class SplitUnit::ClosePill : public Component, public SettableTooltipClient
{
public:
    ClosePill() { setMouseCursor (MouseCursor::PointingHandCursor); setRepaintsOnMouseActivity (true); }
    std::function<void()> onClick;

    void paint (Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        const bool hot = isMouseOver();
        if (hot)
        {
            ColourGradient halo (killRed.withAlpha (0.35f), r.getCentreX(), r.getCentreY(), killRed.withAlpha (0.0f),
                                 r.getCentreX() + r.getWidth(), r.getCentreY(), true);
            g.setGradientFill (halo);
            g.fillEllipse (r.expanded (r.getWidth() * 0.5f));
        }
        g.setColour (card::bg);
        g.fillEllipse (r);
        g.setColour (hot ? killRed : card::line.withMultipliedAlpha (1.5f));
        g.drawEllipse (r.reduced (0.5f), 1.0f);
        const auto c = r.getCentre();
        const float d = r.getWidth() * 0.17f;
        g.setColour (hot ? Colour (0xffff8a94) : card::text2);
        g.drawLine (c.x - d, c.y - d, c.x + d, c.y + d, 1.3f);
        g.drawLine (c.x - d, c.y + d, c.x + d, c.y - d, 1.3f);
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
};

// ============================================================================
float SplitUnit::freqToNorm (float hz) noexcept
{
    hz = jlimit (SplitParams::minFreq, SplitParams::maxFreq, hz);
    return std::log (hz / SplitParams::minFreq) / std::log (SplitParams::maxFreq / SplitParams::minFreq);
}

float SplitUnit::normToFreq (float norm) noexcept
{
    return SplitParams::minFreq * std::pow (SplitParams::maxFreq / SplitParams::minFreq, jlimit (0.0f, 1.0f, norm));
}

float SplitUnit::levelToNorm (float db) noexcept
{
    return (jlimit (SplitParams::minLevelDb, SplitParams::maxLevelDb, db) - SplitParams::minLevelDb)
         / (SplitParams::maxLevelDb - SplitParams::minLevelDb);
}

float SplitUnit::normToLevel (float norm) noexcept
{
    const float db = SplitParams::minLevelDb + jlimit (0.0f, 1.0f, norm) * (SplitParams::maxLevelDb - SplitParams::minLevelDb);
    return std::abs (db) < 0.25f ? 0.0f : db; // easy to hit 0 dB
}

String SplitUnit::formatFreq (float hz)
{
    return hz >= 1000.0f ? String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (hz)) + " Hz";
}

String SplitUnit::formatBalance (SplitMode mode, float b)
{
    float gA, gB;
    splitMergeGains (mode, b, gA, gB);
    if (std::abs (b - 0.5f) < 0.005f) return "CENTRE";
    return "A" + String (roundToInt (gA * 100.0f)) + " B" + String (roundToInt (gB * 100.0f));
}

// ============================================================================
SplitUnit::SplitUnit (EffectorKillaAudioProcessor& p) : proc (p)
{
    setTooltip ("SPLIT: the cards under it run in two parallel branches (A left, B right) and meet again at MERGE");

    static const char* tips[] = {
        "PARALLEL: both branches get the full signal; A/B crossfades between them",
        "L/R: branch A gets the left channel, branch B the right one",
        "M/S: branch A gets the mid (centre), branch B the side (stereo) part",
        "LOW/HIGH: Linkwitz-Riley crossover at XOVER; A = lows, B = highs (sums back flat)",
        "TRANSIENT/SUSTAIN: A gets the attacks, B the body and tails" };
    for (int m = 0; m < kNumSplitModes; ++m)
    {
        modeButtons[(size_t) m] = std::make_unique<ModeButton> ((SplitMode) m, [this] (SplitMode sm) { setMode (sm); });
        modeButtons[(size_t) m]->setTooltip (tips[m]);
        addAndMakeVisible (*modeButtons[(size_t) m]);
    }

    xover = std::make_unique<FlatKnob> ("XOVER", card::ice);
    balance = std::make_unique<FlatKnob> ("A / B", card::ice);
    levelA = std::make_unique<FlatKnob> ("LVL A", branchA);
    levelB = std::make_unique<FlatKnob> ("LVL B", branchB);

    auto gesture = [this] { proc.pushUndo(); };
    for (auto* k : allKnobs())
    {
        k->onGestureStart = gesture;
        addAndMakeVisible (*k);
    }

    xover->setDefault (freqToNorm (SplitParams().freqHz));
    xover->onUserChange = [this] (float v) { proc.setSplitFreq (normToFreq (v)); xover->tick(); };
    xover->valueText = [this] { return formatFreq (proc.getSplit().params.freqHz); };
    xover->setTooltip ("Crossover frequency of the LOW/HIGH split");

    balance->setDefault (0.5f);
    balance->onUserChange = [this] (float v) { proc.setSplitBalance (v); balance->tick(); };
    balance->valueText = [this] { return formatBalance (proc.getSplit().mode, proc.getSplit().params.balance); };
    balance->setTooltip ("MERGE balance: left = only branch A, centre = both, right = only branch B");

    levelA->setDefault (levelToNorm (0.0f));
    levelA->onUserChange = [this] (float v) { proc.setSplitLevel (0, normToLevel (v)); levelA->tick(); };
    levelA->valueText = [this] { return String (proc.getSplit().params.levelADb, 1) + "dB"; };
    levelA->setTooltip ("Level of branch A at the MERGE");

    levelB->setDefault (levelToNorm (0.0f));
    levelB->onUserChange = [this] (float v) { proc.setSplitLevel (1, normToLevel (v)); levelB->tick(); };
    levelB->valueText = [this] { return String (proc.getSplit().params.levelBDb, 1) + "dB"; };
    levelB->setTooltip ("Level of branch B at the MERGE");

    removeBtn = std::make_unique<ClosePill>();
    removeBtn->setTooltip ("Remove the split (the cards stay, in series)");
    removeBtn->onClick = [this]
    {
        proc.removeSplit();
        if (onRemoved) onRemoved();
    };
    addAndMakeVisible (*removeBtn);

    noFocus (*this);
    for (auto& b : modeButtons) noFocus (*b);
    for (auto* k : allKnobs()) noFocus (*k);
    noFocus (*removeBtn);

    setSize (width, height); // after the children exist: resized() lays them out
    refresh();
}

SplitUnit::~SplitUnit() = default;

std::array<SplitUnit::FlatKnob*, 4> SplitUnit::allKnobs() const { return { xover.get(), balance.get(), levelA.get(), levelB.get() }; }

void SplitUnit::setMode (SplitMode m)
{
    proc.setSplitMode (m);
    refresh();
}

void SplitUnit::refresh()
{
    const auto& s = proc.getSplit();
    shownMode = s.mode;
    for (int m = 0; m < kNumSplitModes; ++m)
        modeButtons[(size_t) m]->setLit ((int) s.mode == m);
    xover->setNormValue (freqToNorm (s.params.freqHz));
    balance->setNormValue (s.params.balance);
    levelA->setNormValue (levelToNorm (s.params.levelADb));
    levelB->setNormValue (levelToNorm (s.params.levelBDb));
    const bool lowHigh = s.mode == SplitMode::LowHigh;
    xover->setAlpha (lowHigh ? 1.0f : 0.35f);
    xover->setEnabled (lowHigh);
    for (auto* k : allKnobs()) k->tick();
    repaint();
}

void SplitUnit::tick()
{
    const auto& s = proc.getSplit();
    if (s.mode != shownMode) { refresh(); return; }
    for (auto* k : allKnobs()) k->tick();
}

// ---- layout (adapts to the width) ---------------------------------------------------------------
Rectangle<float> SplitUnit::platePane() const
{
    if (isWide()) return { 14.0f, 4.0f, 150.0f, (float) height - 8.0f };
    return { 14.0f, 4.0f, (float) getWidth() - 58.0f, 30.0f };
}

void SplitUnit::resized()
{
    const int w = getWidth();
    float left, right, pillY;
    if (isWide())
    {
        removeBtn->setBounds (Rectangle<int> (28, 28).withCentre ({ w - 24, height / 2 }));
        int kx = w - 44 - 4 * kKnobW;
        for (auto* k : allKnobs())
        {
            k->setBounds (kx, (height - kKnobH) / 2, kKnobW, kKnobH);
            kx += kKnobW;
        }
        // mode pill between the SPLIT title and the knobs, label above it
        left = platePane().getRight() + 8.0f;
        right = (float) (w - 44 - 4 * kKnobW) - 10.0f;
        pillY = 29.0f;
    }
    else
    {
        removeBtn->setBounds (Rectangle<int> (28, 28).withCentre ({ w - 24, 19 }));
        const float kw = jmin ((float) kKnobW + 16.0f, (float) (w - 28) / 4.0f);
        const float x0 = ((float) w - 4.0f * kw) * 0.5f;
        int i = 0;
        for (auto* k : allKnobs())
            k->setBounds (Rectangle<float> (x0 + kw * (float) i++ + (kw - (float) kKnobW) * 0.5f, 62.0f, (float) kKnobW, (float) kKnobH).toNearestInt());
        left = 14.0f;
        right = (float) w - 14.0f;
        pillY = 35.0f;
    }
    const float pw = jmax (150.0f, right - left);
    modeArea = Rectangle<float> (left, pillY, jmin (pw, 380.0f), 26.0f);
    modeArea.setX (left + (pw - modeArea.getWidth()) * 0.5f);
    // segment widths follow the label lengths, so the narrow strip keeps every label readable
    float tw[kNumSplitModes], sum = 0.0f;
    for (int m = 0; m < kNumSplitModes; ++m)
        sum += (tw[m] = GlyphArrangement::getStringWidth (smallFont (true), splitModeName ((SplitMode) m)) + 14.0f);
    float sx = modeArea.getX();
    for (int m = 0; m < kNumSplitModes; ++m)
    {
        const float sw = modeArea.getWidth() * tw[m] / sum;
        modeButtons[(size_t) m]->setBounds (Rectangle<float> (sx, modeArea.getY(), sw, modeArea.getHeight()).toNearestInt());
        sx += sw;
    }
}

void SplitUnit::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto& s = proc.getSplit();

    // ---- slim TrapWin card -----------------------------------------------------------------------
    {
        Path shape;
        shape.addRoundedRectangle (b, kCorner);
        ColourGradient body (card::card2, 0.0f, b.getY(), Colour (0xff080c14), 0.0f, b.getBottom(), false);
        body.addColour (0.5, card::card);
        g.setGradientFill (body);
        g.fillPath (shape);
        g.saveState();
        g.reduceClipRegion (shape);
        // the fork: A glows in from the left, B from the right
        ColourGradient ga (branchA.withAlpha (0.07f), b.getX(), b.getCentreY(), branchA.withAlpha (0.0f), b.getX() + b.getWidth() * 0.3f, b.getCentreY(), true);
        g.setGradientFill (ga);
        g.fillRect (b);
        ColourGradient gb (branchB.withAlpha (0.06f), b.getRight(), b.getCentreY(), branchB.withAlpha (0.0f), b.getRight() - b.getWidth() * 0.3f, b.getCentreY(), true);
        g.setGradientFill (gb);
        g.fillRect (b);
        g.restoreState();
        g.setColour (card::line);
        g.drawRoundedRectangle (b.reduced (0.5f), kCorner - 0.5f, 1.0f);
        ColourGradient top (card::ice.withAlpha (0.0f), b.getX() + kCorner, 0.0f, card::ice.withAlpha (0.0f), b.getRight() - kCorner, 0.0f, false);
        top.addColour (0.3, card::ice.withAlpha (0.16f));
        top.addColour (0.7, card::ice.withAlpha (0.10f));
        g.setGradientFill (top);
        g.fillRect (b.getX() + kCorner, b.getY() + 0.5f, b.getWidth() - 2.0f * kCorner, 0.8f);
        // accent edges: A on the left, B on the right
        for (int side = 0; side < 2; ++side)
        {
            const auto col = side == 0 ? branchA : branchB;
            const float y0 = b.getY() + kCorner * 0.6f, y1 = b.getBottom() - kCorner * 0.6f;
            ColourGradient edge (col.withAlpha (0.0f), 0.0f, y0, col.withAlpha (0.0f), 0.0f, y1, false);
            edge.addColour (0.5, col);
            g.setGradientFill (edge);
            g.fillRoundedRectangle (Rectangle<float> (side == 0 ? b.getX() + 1.0f : b.getRight() - 3.0f, y0, 2.0f, y1 - y0), 1.0f);
        }
    }

    // ---- SPLIT title + routing ------------------------------------------------------------------
    {
        const auto r = platePane();
        const bool wide = isWide();
        const auto tb = card::paintChromeTitle (g, "SPLIT", { r.getX() + 4.0f, r.getY() + 4.0f, 90.0f, 24.0f }, 19.0f, 0.9f);
        // little fork glyph after the title
        const float fx = tb.getRight() + 9.0f, fy = tb.getCentreY();
        Path pa, pb, in;
        in.startNewSubPath (fx, fy); in.lineTo (fx + 5.0f, fy);
        pa.startNewSubPath (fx + 5.0f, fy); pa.quadraticTo (fx + 9.0f, fy, fx + 14.0f, fy - 6.0f);
        pb.startNewSubPath (fx + 5.0f, fy); pb.quadraticTo (fx + 9.0f, fy, fx + 14.0f, fy + 6.0f);
        card::strokeGlow (g, in, card::ice, 1.2f, 0.8f);
        card::strokeGlow (g, pa, branchA, 1.2f, 0.8f);
        card::strokeGlow (g, pb, branchB, 1.2f, 0.8f);

        // routing: "A LOW   B HIGH" (under the title when wide, after it when narrow)
        auto sub = wide ? Rectangle<float> (r.getX() + 4.0f, r.getY() + 32.0f, r.getWidth() - 4.0f, 18.0f)
                        : Rectangle<float> (fx + 26.0f, r.getY() + 6.0f, jmin (240.0f, r.getRight() - fx - 26.0f), 20.0f);
        const float cellW = (sub.getWidth() - 6.0f) * 0.5f;
        auto drawBranch = [&] (int br, Colour col)
        {
            String bn = String (splitBranchName (s.mode, br)).toUpperCase();
            if (bn.length() <= 1) bn = "FULL";
            const String t = String (br == 0 ? "A " : "B ") + bn;
            auto cell = sub.removeFromLeft (cellW);
            card::dotGlow (g, { cell.getX() + 3.5f, cell.getCentreY() }, 2.4f, col, 0.9f);
            g.setFont (smallFont());
            g.setColour (labelCol);
            g.drawFittedText (t, cell.withTrimmedLeft (10.0f).toNearestInt(), Justification::centredLeft, 1, 0.8f);
            sub.removeFromLeft (6.0f);
        };
        drawBranch (0, branchA);
        drawBranch (1, branchB);
    }

    // ---- mode pill --------------------------------------------------------------------------------
    {
        if (isWide())
        {
            g.setFont (smallFont());
            g.setColour (labelCol);
            g.drawText ("SPLIT MODE", modeArea.withY (8.0f).withHeight (16.0f).withTrimmedLeft (12.0f), Justification::centredLeft, false);
        }
        const float rad = modeArea.getHeight() * 0.5f;
        g.setColour (card::bg);
        g.fillRoundedRectangle (modeArea, rad);
        g.setColour (card::line.withMultipliedAlpha (1.4f));
        g.drawRoundedRectangle (modeArea.reduced (0.5f), rad - 0.5f, 1.0f);
    }
}

void SplitUnit::mouseDrag (const MouseEvent& e)
{
    if (onPlateDrag && platePane().contains (e.mouseDownPosition))
        onPlateDrag (e);
}

} // namespace ek::ui
