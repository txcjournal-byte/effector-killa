#include "PerformerUnit.h"
#include "PerformerDraw.h"
#include "../../engine/Macro.h"
#include <cmath>

namespace ek::ui
{
using namespace juce;
namespace pd = performer;

// layout of the unit at 1x (932 x 104)
namespace
{
constexpr float earW = 18.0f;
const Rectangle<int> killBounds { 28, 10, 206, 54 };
const Rectangle<int> ratingBounds { 28, 70, 134, 26 };
const Rectangle<int> bootlegBounds { 168, 70, 66, 26 };
constexpr int dividerA = 243;
constexpr int macroX = 250, macroW = 72, macroY = 6, macroH = 94;
constexpr int dividerB = 614;
const Rectangle<int> vuInBounds { 622, 7, 138, 90 };
const Rectangle<int> vuOutBounds { 768, 7, 138, 90 };
constexpr float vuRefDbfs = -14.0f;   // 0 VU = -14 dBFS RMS
} // namespace

// ============================================================================
class PerformerUnit::KillKey : public Component, public SettableTooltipClient
{
public:
    std::function<void()> onClick;
    float flash = 0.0f;

    KillKey()
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
        setTooltip ("KILL: a new, musical, level-matched chain (space bar). Locked units stay.");
    }
    void paint (Graphics& g) override
    {
        pd::drawKillButton (g, getLocalBounds().toFloat(), down, isMouseOver(), flash);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        down = true;
        repaint();
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (! down) return;
        down = false;
        repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }

private:
    bool down = false;
};

// ============================================================================
// PG · R · UNRATED: three LEDs in a recessed strip; click a position to select it.
class PerformerUnit::RatingSelector : public Component, public SettableTooltipClient
{
public:
    explicit RatingSelector (EffectorKillaAudioProcessor& p) : proc (p)
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setTooltip ("KILL rating - PG: same units, small changes / R: new chain of 3 - 5 units / UNRATED: 4 - 7 units, hotter settings");
    }
    void tick()
    {
        const int l = (int) proc.killLevel;
        if (l != shown) repaint();
    }
    void paint (Graphics& g) override
    {
        shown = (int) proc.killLevel;
        auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff03050a));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawLine (r.getX() + 4.0f, r.getBottom() - 0.5f, r.getRight() - 4.0f, r.getBottom() - 0.5f, 0.8f);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

        static const char* names[] = { "PG", "R", "UNRATED" };
        const Colour leds[] = { theme::ledGreen, pd::amber, theme::killRed };
        for (int i = 0; i < 3; ++i)
        {
            const auto z = zone (i).toFloat();
            const bool on = i == shown;
            const Point<float> c { z.getX() + 8.0f, z.getCentreY() };
            theme::drawLed (g, c, 3.0f, leds[i], on, 0.9f);
            pd::drawSilkscreen (g, names[i], z.withTrimmedLeft (14.0f), on ? theme::silver : theme::silverDim.withAlpha (0.7f), 9.5f,
                                Justification::centredLeft);
        }
        g.setColour (Colours::white.withAlpha (0.06f));
        for (int i = 1; i < 3; ++i)
            g.drawVerticalLine (zone (i).getX() - 1, r.getY() + 5.0f, r.getBottom() - 5.0f);
    }
    void mouseDown (const MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i)
            if (zone (i).contains (e.getPosition()))
            {
                proc.killLevel = (KillLevel) i;
                repaint();
            }
    }

private:
    Rectangle<int> zone (int i) const
    {
        // PG and R are short, UNRATED gets the rest
        const int w = getWidth();
        const int a = (int) std::round ((float) w * 0.26f), b = (int) std::round ((float) w * 0.475f);
        if (i == 0) return { 2, 0, a - 2, getHeight() };
        if (i == 1) return { a, 0, b - a, getHeight() };
        return { b, 0, w - b - 2, getHeight() };
    }
    EffectorKillaAudioProcessor& proc;
    int shown = -1;
};

// ============================================================================
class PerformerUnit::BootlegKey : public Component, public SettableTooltipClient
{
public:
    std::function<void()> onClick;
    float lit = 0.0f;

    BootlegKey()
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
        setTooltip ("BOOTLEG: subtle variation of every setting, same units");
    }
    void paint (Graphics& g) override
    {
        pd::drawLedKey (g, getLocalBounds().toFloat(), "BOOTLEG", pd::iceLine, jmax (lit, down ? 1.0f : 0.0f), down, isMouseOver());
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        down = true;
        repaint();
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (! down) return;
        down = false;
        repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }

private:
    bool down = false;
};

// ============================================================================
// One macro: coloured name, ring knob, value readout.
class PerformerUnit::MacroCell : public Component, public SettableTooltipClient
{
public:
    class RingKnob : public Knob, public SettableTooltipClient
    {
    public:
        explicit RingKnob (Colour c) : Knob (Knob::Style::Macro), ring (c) {}
        float shownValue = 0.5f;
        void paint (Graphics& g) override
        {
            pd::drawMacroKnob (g, getLocalBounds().toFloat().reduced (1.0f), ring, getNormValue(), shownValue, isMouseButtonDown());
        }
        Colour ring;
    };

    MacroCell (EffectorKillaAudioProcessor& p, int m) : proc (p), index (m), knob (pd::macroColour (m))
    {
        name = MacroEngine::macroName (m);
        if (auto* rp = proc.getParam (ParamIDs::macro (m))) knob.bindTo (*rp);
        knob.onGestureStart = [this] { proc.beginUiGesture (index); };
        knob.onGestureEnd = [this] { proc.endUiGesture (index); };
        knob.setMouseCursor (MouseCursor::UpDownResizeCursor);
        static const char* tips[] = { "VILLAIN ARC: darker - filters down, darker EQ and reverb",
                                      "CRASH OUT: dirt - more drive, fewer bits, harder clipping",
                                      "AURA: space - more reverb / delay, longer, wider",
                                      "DRIP: motion - faster, deeper modulation and wobble",
                                      "KNOCK: punch - transients, compression, clipper drive" };
        const String tip = String (tips[jlimit (0, 4, m)]) + ". The ring shows the live (modulated) value, the white tick the knob itself.";
        setTooltip (tip);
        knob.setTooltip (tip);
        addAndMakeVisible (knob);
        tick();
    }

    void tick()
    {
        const float v = jlimit (0.0f, 1.0f, proc.engine.meters.macroValue[(size_t) index].load());
        const float base = knob.getNormValue();
        if (std::abs (v - knob.shownValue) > 0.002f || std::abs (base - lastBase) > 0.002f)
        {
            knob.shownValue = v;
            lastBase = base;
            knob.repaint();
        }
        const int pct = roundToInt (v * 100.0f);
        if (pct != shownPct)
        {
            shownPct = pct;
            repaint (valueArea());
        }
    }

    void paint (Graphics& g) override
    {
        const auto col = pd::macroColour (index);
        auto r = getLocalBounds().toFloat();
        // name, printed in the macro colour
        g.setFont (theme::monoLabel (9.5f).boldened());
        g.setColour (col.interpolatedWith (theme::silver, 0.35f));
        g.drawFittedText (name, r.removeFromTop (14.0f).toNearestInt(), Justification::centred, 1, 0.75f);

        // value window
        const auto v = valueArea().toFloat();
        g.setColour (Colour (0xff03050a));
        g.fillRoundedRectangle (v, 3.0f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawLine (v.getX() + 3.0f, v.getBottom() + 0.5f, v.getRight() - 3.0f, v.getBottom() + 0.5f, 0.8f);
        theme::drawGlowText (g, String (shownPct), v.translated (0.0f, 0.5f), theme::vfdFont (12.0f), col.interpolatedWith (Colours::white, 0.25f),
                             Justification::centred, 0.5f);
    }

    void resized() override
    {
        const int k = 58;
        knob.setBounds ((getWidth() - k) / 2, 15, k, k);
    }

    RingKnob& getKnob() noexcept { return knob; }

private:
    Rectangle<int> valueArea() const { return { (getWidth() - 34) / 2, getHeight() - 17, 34, 15 }; }

    EffectorKillaAudioProcessor& proc;
    int index;
    RingKnob knob;
    String name;
    float lastBase = -1.0f;
    int shownPct = -1;
};

// ============================================================================
// Analogue VU meter with second-order needle ballistics (~300 ms rise, ~1.5 % overshoot)
// and an OVER LED under the face.
class PerformerUnit::VuMeter : public Component, public SettableTooltipClient
{
public:
    VuMeter (String nm, std::function<float()> rms, std::function<float()> peak, float overAt)
        : name (std::move (nm)), getRms (std::move (rms)), getPeak (std::move (peak)), overThreshold (overAt)
    {
        setTooltip (name + " level, VU ballistics (0 VU = -14 dBFS RMS). OVER lights when the peak reaches full scale.");
    }

    void tick (bool reduceMotion)
    {
        const float rmsLin = jmax (0.0f, getRms());
        const float vuDb = 20.0f * std::log10 (jmax (1.0e-5f, rmsLin)) - vuRefDbfs;
        // integrate in the "voltage" domain (that's what moves the needle coil)
        const float target = std::pow (10.0f, jlimit (-30.0f, 6.0f, vuDb) / 20.0f);
        if (reduceMotion)
        {
            pos = target;
            vel = 0.0f;
        }
        else
        {
            constexpr float wn = 17.0f, zeta = 0.8f, dt = 1.0f / 30.0f;
            constexpr int sub = 8;
            for (int i = 0; i < sub; ++i)
            {
                const float acc = wn * wn * (target - pos) - 2.0f * zeta * wn * vel;
                vel += acc * dt / (float) sub;
                pos += vel * dt / (float) sub;
            }
            pos = jlimit (0.0f, 1.6f, pos);
        }
        const float shownDb = 20.0f * std::log10 (jmax (1.0e-4f, pos));

        if (getPeak() >= overThreshold) overHold = 20;   // ~0.66 s
        else if (overHold > 0) --overHold;
        const bool over = overHold > 0;

        if (std::abs (shownDb - drawnDb) > 0.05f || over != drawnOver) repaint();
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float pos01 = pos;
        drawnDb = 20.0f * std::log10 (jmax (1.0e-4f, pos01));
        drawnOver = overHold > 0;

        auto face = r.removeFromTop (r.getHeight() - 17.0f);
        pd::drawVuMeter (g, face, drawnDb, name == "INPUT" ? "IN" : "OUT", 1.0f);

        r.removeFromTop (3.0f);
        auto row = r;
        pd::drawSilkscreen (g, name, row.withTrimmedLeft (2.0f), theme::silverDim, 9.5f, Justification::centredLeft);
        const Point<float> led { row.getRight() - 8.0f, row.getCentreY() };
        theme::drawLed (g, led, 3.2f, theme::killRed, drawnOver, 1.0f);
        pd::drawSilkscreen (g, "OVER", row.withTrimmedRight (17.0f), drawnOver ? theme::killRed.brighter (0.4f) : theme::silverDim.withAlpha (0.7f),
                            9.0f, Justification::centredRight);
    }

private:
    String name;
    std::function<float()> getRms, getPeak;
    float overThreshold;
    float pos = 0.0f, vel = 0.0f, drawnDb = -100.0f;
    int overHold = 0;
    bool drawnOver = false;
};

// ============================================================================
PerformerUnit::PerformerUnit (EffectorKillaAudioProcessor& p) : proc (p)
{
    setOpaque (true);
    lastKillEvents = proc.killEvents.load();

    killKey = std::make_unique<KillKey>();
    killKey->onClick = [this]
    {
        proc.kill();
        lastKillEvents = proc.killEvents.load();
        fireKillFlash();
    };
    addAndMakeVisible (*killKey);

    rating = std::make_unique<RatingSelector> (proc);
    addAndMakeVisible (*rating);

    bootleg = std::make_unique<BootlegKey>();
    bootleg->onClick = [this]
    {
        proc.bootleg();
        bootleg->lit = 1.0f;
        bootleg->repaint();
    };
    addAndMakeVisible (*bootleg);

    for (int m = 0; m < kNumMacros; ++m)
        addAndMakeVisible (macros.add (new MacroCell (proc, m)));

    auto& meters = proc.engine.meters;
    vuIn = std::make_unique<VuMeter> ("INPUT",
                                      [&meters] { return meters.inLevel.load() * 0.70710678f; },   // peak -> sine RMS estimate
                                      [&meters] { return meters.inLevel.load(); }, 0.999f);
    vuOut = std::make_unique<VuMeter> ("OUTPUT",
                                       [&meters] { return meters.outLevel.load(); },
                                       [&meters] { return meters.outPeak.load(); }, 0.989f);   // -0.1 dBFS
    addAndMakeVisible (*vuIn);
    addAndMakeVisible (*vuOut);

    setSize (unitWidth, unitHeight);
    startTimerHz (30);
}

PerformerUnit::~PerformerUnit()
{
    stopTimer();
}

void PerformerUnit::setReduceMotion (bool shouldReduce)
{
    reduceMotion = shouldReduce;
    if (reduceMotion)
    {
        killKey->flash = 0.0f;
        bootleg->lit = 0.0f;
        killKey->repaint();
        bootleg->repaint();
    }
}

void PerformerUnit::fireKillFlash()
{
    if (! reduceMotion)
    {
        killKey->flash = 1.0f;
        repaint (killBounds.expanded (20, 10));
    }
    if (onKillFlash) onKillFlash();
}

void PerformerUnit::tick()
{
    // KILL from anywhere (space bar, other UI) -> flash
    const int ke = proc.killEvents.load();
    if (ke != lastKillEvents)
    {
        lastKillEvents = ke;
        fireKillFlash();
    }
    if (killKey->flash > 0.0f)
    {
        killKey->flash = killKey->flash < 0.03f ? 0.0f : killKey->flash * 0.82f;
        repaint (killBounds.expanded (20, 10));   // key + the backlight spill on the metal
    }
    if (bootleg->lit > 0.0f)
    {
        bootleg->lit = (reduceMotion || bootleg->lit < 0.03f) ? 0.0f : bootleg->lit * 0.8f;
        bootleg->repaint();
    }

    rating->tick();
    for (auto* m : macros) m->tick();
    vuIn->tick (reduceMotion);
    vuOut->tick (reduceMotion);
}

void PerformerUnit::resized()
{
    // fixed 932 x 104 layout; the editor scales the whole window with a transform
    killKey->setBounds (killBounds);
    rating->setBounds (ratingBounds);
    bootleg->setBounds (bootlegBounds);
    for (int m = 0; m < macros.size(); ++m)
        macros[m]->setBounds (macroX + m * macroW, macroY, macroW, macroH);
    vuIn->setBounds (vuInBounds);
    vuOut->setBounds (vuOutBounds);
}

void PerformerUnit::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (Colour (0xff07090d));   // rack shaft behind the ears

    const auto plate = b.withTrimmedLeft (earW).withTrimmedRight (earW);
    pd::drawAnodisedPlate (g, plate);
    pd::drawRackEar (g, b.withWidth (earW), true);
    pd::drawRackEar (g, b.withLeft (b.getRight() - earW), false);

    // rack screws through the ear slots
    const float sy[] = { b.getY() + b.getHeight() * 0.2f, b.getY() + b.getHeight() * 0.8f };
    pd::drawScrew (g, { earW * 0.5f, sy[0] }, 5.0f, 0.35f);
    pd::drawScrew (g, { earW * 0.5f, sy[1] }, 5.0f, 1.1f);
    pd::drawScrew (g, { b.getRight() - earW * 0.5f, sy[0] }, 5.0f, 0.8f);
    pd::drawScrew (g, { b.getRight() - earW * 0.5f, sy[1] }, 5.0f, 0.15f);

    // section dividers
    pd::drawEngravedDivider (g, (float) dividerA, 10.0f, b.getBottom() - 10.0f);
    pd::drawEngravedDivider (g, (float) dividerB, 10.0f, b.getBottom() - 10.0f);

    // KILL section: soft red spill of the backlight onto the metal around the key
    const auto k = killBounds.toFloat();
    const float glow = 0.10f + 0.16f * killKey->flash;
    for (int i = 1; i <= 6; ++i)
    {
        g.setColour (theme::killRed.withAlpha (glow * (1.0f - (float) i / 7.0f)));
        g.drawRoundedRectangle (k.expanded ((float) i * 0.9f), 8.0f + (float) i * 0.6f, 1.2f);
    }
}
} // namespace ek::ui
