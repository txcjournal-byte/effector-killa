#include "SplitUnit.h"

namespace ek::ui
{
using namespace juce;

namespace
{
const Colour amber { 0xffffc861 };
const Colour branchA { 0xff9fc3ff };  // ice blue
const Colour branchB { 0xffffc861 };  // amber

constexpr float earW = 16.0f;

void drawScrew (Graphics& g, Point<float> c, float r)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (Rectangle<float> (r * 2.4f, r * 2.4f).withCentre (c.translated (0.0f, 0.6f)));
    ColourGradient head (Colour (0xffb9c0cc), c.x - r, c.y - r, Colour (0xff3a404b), c.x + r, c.y + r, false);
    g.setGradientFill (head);
    g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
    g.setColour (Colour (0xff1a1d23));
    g.drawLine (c.x - r * 0.7f, c.y + r * 0.25f, c.x + r * 0.7f, c.y - r * 0.25f, r * 0.35f);
}

// Engraved metal name plate with four tiny rivets.
void drawPlate (Graphics& g, Rectangle<float> r)
{
    ColourGradient fill (Colour (0xffc7ccd4), r.getX(), r.getY(), Colour (0xff8a919c), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colours::white.withAlpha (0.5f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    for (auto p : { r.getTopLeft().translated (4.0f, 4.0f), r.getTopRight().translated (-4.0f, 4.0f),
                    r.getBottomLeft().translated (4.0f, -4.0f), r.getBottomRight().translated (-4.0f, -4.0f) })
    {
        g.setColour (Colour (0xff4a505a));
        g.fillEllipse (Rectangle<float> (2.6f, 2.6f).withCentre (p));
    }
}

void drawArrow (Graphics& g, Rectangle<float> r, Colour c)
{
    Path tri;
    tri.addTriangle (r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
    g.setColour (c);
    g.fillPath (tri);
}
} // namespace

// ============================================================================
// Mode button: small square push button with an LED above its printed label.
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
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        ColourGradient body (Colour (down ? 0xff0b0f18 : 0xff222a39), r.getX(), r.getY(),
                             Colour (down ? 0xff151c29 : 0xff0b0f18), r.getX(), r.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (lit ? amber.withAlpha (0.75f) : (isMouseOver() ? theme::accent.withAlpha (0.45f) : Colour (0x40a9b6cc)));
        g.drawRoundedRectangle (r, 4.0f, lit ? 1.3f : 1.0f);

        theme::drawLed (g, { r.getCentreX(), r.getY() + 8.0f }, 3.0f, amber, lit, 0.8f);
        g.setFont (theme::monoLabel (9.5f).boldened());
        g.setColour (lit ? amber.brighter (0.3f) : theme::silver.withAlpha (0.85f));
        g.drawFittedText (splitModeName (mode), r.withTrimmedTop (15.0f).toNearestInt(), Justification::centred, 1, 0.7f);
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
    setTooltip ("SPLIT: the units under it run in two parallel branches (A left, B right) and meet again at MERGE");

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

    auto gesture = [this] { proc.pushUndo(); };
    for (auto* k : { &xover, &balance, &levelA, &levelB })
    {
        k->knob.onGestureStart = gesture;
        addAndMakeVisible (*k);
    }

    xover.knob.setDefault (freqToNorm (SplitParams().freqHz));
    xover.knob.onUserChange = [this] (float v) { proc.setSplitFreq (normToFreq (v)); xover.tick(); };
    xover.valueText = [this] { return formatFreq (proc.getSplit().params.freqHz); };
    xover.setTooltip ("Crossover frequency of the LOW/HIGH split");

    balance.knob.setDefault (0.5f);
    balance.knob.onUserChange = [this] (float v) { proc.setSplitBalance (v); balance.tick(); };
    balance.valueText = [this] { return formatBalance (proc.getSplit().mode, proc.getSplit().params.balance); };
    balance.setTooltip ("MERGE balance: left = only branch A, centre = both, right = only branch B");

    levelA.setAccent (branchA);
    levelA.knob.setDefault (levelToNorm (0.0f));
    levelA.knob.onUserChange = [this] (float v) { proc.setSplitLevel (0, normToLevel (v)); levelA.tick(); };
    levelA.valueText = [this] { return String (proc.getSplit().params.levelADb, 1) + " dB"; };
    levelA.setTooltip ("Level of branch A at the MERGE");

    levelB.setAccent (branchB);
    levelB.knob.setDefault (levelToNorm (0.0f));
    levelB.knob.onUserChange = [this] (float v) { proc.setSplitLevel (1, normToLevel (v)); levelB.tick(); };
    levelB.valueText = [this] { return String (proc.getSplit().params.levelBDb, 1) + " dB"; };
    levelB.setTooltip ("Level of branch B at the MERGE");

    removeBtn.setTooltip ("Remove the split (the units stay, in series)");
    removeBtn.onClick = [this]
    {
        proc.removeSplit();
        if (onRemoved) onRemoved();
    };
    addAndMakeVisible (removeBtn);

    setSize (width, height); // after the children exist: resized() lays them out
    refresh();
}

SplitUnit::~SplitUnit() = default;

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
    xover.knob.setNormValue (freqToNorm (s.params.freqHz), dontSendNotification);
    balance.knob.setNormValue (s.params.balance, dontSendNotification);
    levelA.knob.setNormValue (levelToNorm (s.params.levelADb), dontSendNotification);
    levelB.knob.setNormValue (levelToNorm (s.params.levelBDb), dontSendNotification);
    const bool lowHigh = s.mode == SplitMode::LowHigh;
    xover.setAlpha (lowHigh ? 1.0f : 0.35f);
    xover.setEnabled (lowHigh);
    for (auto* k : { &xover, &balance, &levelA, &levelB }) k->tick();
    repaint();
}

void SplitUnit::tick()
{
    const auto& s = proc.getSplit();
    if (s.mode != shownMode) { refresh(); return; }
    for (auto* k : { &xover, &balance, &levelA, &levelB }) k->tick();
    float gA, gB;
    splitMergeGains (s.mode, s.params.balance, gA, gB);
    gA *= Decibels::decibelsToGain (s.params.levelADb);
    gB *= Decibels::decibelsToGain (s.params.levelBDb);
    if (std::abs (gA - shownGainA) > 0.01f || std::abs (gB - shownGainB) > 0.01f)
        repaint (mergePane().getSmallestIntegerContainer().expanded (4));
}

// ---- layout (x positions in a 932 px strip) -----------------------------------------------------
Rectangle<float> SplitUnit::platePane() const { return { 24.0f, 8.0f, 156.0f, 48.0f }; }
Rectangle<float> SplitUnit::mergePane() const { return { 768.0f, 8.0f, 112.0f, 48.0f }; }

void SplitUnit::resized()
{
    // mode buttons
    float x = 188.0f;
    for (auto& b : modeButtons)
    {
        b->setBounds (Rectangle<float> (x, 17.0f, 62.0f, 40.0f).toNearestInt());
        x += 65.0f;
    }
    xover.setBounds (516, 2, 62, 60);
    balance.setBounds (580, 2, 62, 60);
    levelA.setBounds (646, 2, 58, 60);
    levelB.setBounds (704, 2, 58, 60);
    removeBtn.setBounds (889, 20, 22, 22);
}

void SplitUnit::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // ---- chassis: gunmetal faceplate with brushed lines, rack ears with screws ------------------
    ColourGradient metal (Colour (0xff272d38), 0.0f, 0.0f, Colour (0xff12161e), 0.0f, b.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRoundedRectangle (b, 4.0f);
    Random rng (7);
    for (int y = 2; y < height - 2; y += 2)
    {
        g.setColour (Colours::white.withAlpha (0.012f + 0.018f * rng.nextFloat()));
        g.drawHorizontalLine (y, earW, b.getRight() - earW);
    }
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawHorizontalLine (1, 2.0f, b.getRight() - 2.0f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

    for (float ex : { 0.0f, b.getRight() - earW })
    {
        auto ear = Rectangle<float> (ex, 0.0f, earW, b.getHeight());
        ColourGradient ef (Colour (0xff353c48), ear.getX(), 0.0f, Colour (0xff1a1e26), ear.getRight(), 0.0f, false);
        g.setGradientFill (ef);
        g.fillRect (ear.reduced (0.0f, 1.0f));
        drawScrew (g, { ear.getCentreX(), 12.0f }, 3.6f);
        drawScrew (g, { ear.getCentreX(), b.getBottom() - 12.0f }, 3.6f);
    }

    const auto& s = proc.getSplit();

    // ---- SPLIT > A | B plate --------------------------------------------------------------------
    {
        auto r = platePane();
        drawPlate (g, r);
        auto top = r.reduced (9.0f, 4.0f).removeFromTop (24.0f);
        g.setFont (theme::stencilFont (19.0f));
        g.setColour (Colour (0xff15181d));
        g.drawText ("SPLIT", top.removeFromLeft (66.0f), Justification::centredLeft, false);
        drawArrow (g, top.removeFromLeft (14.0f).withSizeKeepingCentre (8.0f, 10.0f), Colour (0xff15181d));
        top.removeFromLeft (4.0f);
        g.setFont (theme::stencilFont (17.0f));
        g.setColour (branchA.darker (1.3f));
        g.drawText ("A", top.removeFromLeft (14.0f), Justification::centred, false);
        g.setColour (Colour (0xff15181d));
        g.drawText ("|", top.removeFromLeft (12.0f), Justification::centred, false);
        g.setColour (branchB.darker (1.1f));
        g.drawText ("B", top.removeFromLeft (14.0f), Justification::centred, false);

        auto sub = r.reduced (9.0f, 5.0f).removeFromBottom (14.0f);
        g.setFont (theme::monoLabel (9.0f).boldened());
        g.setColour (Colour (0xff2a2f38));
        g.drawFittedText ("A=" + String (splitBranchName (s.mode, 0)) + "   B=" + String (splitBranchName (s.mode, 1)),
                          sub.toNearestInt(), Justification::centredLeft, 1, 0.7f);
    }

    // ---- section captions ------------------------------------------------------------------------
    drawSmallLabel (g, "SPLIT MODE", { 188.0f, 3.0f, 322.0f, 13.0f }, Justification::centred, theme::silverDim, 9.0f);
    g.setColour (theme::line);
    g.drawVerticalLine (184, 8.0f, b.getBottom() - 8.0f);
    g.drawVerticalLine (512, 8.0f, b.getBottom() - 8.0f);
    g.drawVerticalLine (644, 8.0f, b.getBottom() - 8.0f);
    g.drawVerticalLine (764, 8.0f, b.getBottom() - 8.0f);
    g.drawVerticalLine (884, 8.0f, b.getBottom() - 8.0f);

    // ---- MERGE plate: two branch lines meet; LEDs glow with each branch's merge gain --------------
    {
        auto r = mergePane();
        drawPlate (g, r);
        g.setFont (theme::stencilFont (15.0f));
        g.setColour (Colour (0xff15181d));
        g.drawText ("MERGE", r.withTrimmedLeft (44.0f).withTrimmedTop (2.0f).withHeight (26.0f), Justification::centredLeft, false);

        float gA, gB;
        splitMergeGains (s.mode, s.params.balance, gA, gB);
        gA *= Decibels::decibelsToGain (s.params.levelADb);
        gB *= Decibels::decibelsToGain (s.params.levelBDb);
        shownGainA = gA;
        shownGainB = gB;

        // little patch diagram: A and B come in from the top and join into one output line
        const float x0 = r.getX() + 10.0f, y0 = r.getY() + 9.0f;
        Path pa, pb, out;
        pa.startNewSubPath (x0, y0); pa.quadraticTo (x0, y0 + 12.0f, x0 + 14.0f, y0 + 16.0f);
        pb.startNewSubPath (x0 + 28.0f, y0); pb.quadraticTo (x0 + 28.0f, y0 + 12.0f, x0 + 14.0f, y0 + 16.0f);
        out.startNewSubPath (x0 + 14.0f, y0 + 16.0f); out.lineTo (x0 + 14.0f, r.getBottom() - 5.0f);
        g.setColour (Colour (0xff15181d));
        g.strokePath (out, PathStrokeType (2.0f));
        g.setColour (branchA.darker (0.8f).interpolatedWith (Colour (0xff15181d), 1.0f - jlimit (0.0f, 1.0f, gA)));
        g.strokePath (pa, PathStrokeType (2.0f));
        g.setColour (branchB.darker (0.6f).interpolatedWith (Colour (0xff15181d), 1.0f - jlimit (0.0f, 1.0f, gB)));
        g.strokePath (pb, PathStrokeType (2.0f));

        // A / B LEDs with the effective gain
        auto leds = r.withTrimmedLeft (44.0f).withTrimmedTop (28.0f).withTrimmedBottom (4.0f);
        auto la = leds.removeFromLeft (32.0f), lb = leds.removeFromLeft (32.0f);
        theme::drawLed (g, { la.getX() + 5.0f, la.getCentreY() }, 3.0f, branchA, gA > 0.01f, jlimit (0.2f, 1.0f, gA));
        theme::drawLed (g, { lb.getX() + 5.0f, lb.getCentreY() }, 3.0f, branchB, gB > 0.01f, jlimit (0.2f, 1.0f, gB));
        g.setFont (theme::monoLabel (9.0f).boldened());
        g.setColour (Colour (0xff15181d));
        g.drawText ("A", la.withTrimmedLeft (11.0f), Justification::centredLeft, false);
        g.drawText ("B", lb.withTrimmedLeft (11.0f), Justification::centredLeft, false);
    }
}

void SplitUnit::mouseDrag (const MouseEvent& e)
{
    if (onPlateDrag && platePane().contains (e.mouseDownPosition))
        onPlateDrag (e);
}

} // namespace ek::ui
