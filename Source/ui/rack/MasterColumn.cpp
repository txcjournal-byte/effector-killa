#include "MasterColumn.h"
#include "CardStyle.h"
#include "PerformerDraw.h"
#include "../../engine/Macro.h"
#include <cmath>

namespace ek::ui
{
using namespace juce;
namespace pd = performer;

namespace
{
bool isManualTempo (EffectorKillaAudioProcessor& p)
{
    auto* m = p.getParam (ParamIDs::tempoMode);
    return m != nullptr && m->getValue() > 0.5f;
}

void setManualTempo (EffectorKillaAudioProcessor& p, bool manual)
{
    if (auto* m = p.getParam (ParamIDs::tempoMode)) m->setValueNotifyingHost (manual ? 1.0f : 0.0f);
}

String bpmText (double bpm) { return String (jlimit (0.0, 999.9, bpm), 1); }

const Colour kRed { 0xffff2e3e };
} // namespace

// ============================================================================================
// TEMPO: big glowing Plex Mono digits on a glass screen. Drag up / down (Shift = fine), wheel,
// double-click = type. Changing the tempo here switches to FREE.
class MasterColumn::BpmDisplay : public Component, public SettableTooltipClient
{
public:
    explicit BpmDisplay (EffectorKillaAudioProcessor& p) : proc (p)
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setTooltip ("Tempo (BPM). Drag up / down (Shift = fine), mouse wheel, double-click to type. Changing it switches to FREE tempo.");
    }

    void tick()
    {
        const auto s = bpmText (proc.getEffectiveBpm()) + (isManualTempo (proc) ? "F" : "H");
        if (s != shown) { shown = s; repaint(); }
    }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        card::paintScreen (g, r, 14.0f, hw::col::ice);
        // faint scanline grid
        g.setColour (hw::col::ice.withAlpha (0.025f));
        for (float y = r.getY() + 4.0f; y < r.getBottom() - 3.0f; y += 3.0f) g.drawHorizontalLine ((int) y, r.getX() + 8.0f, r.getRight() - 8.0f);

        auto digits = r.reduced (14.0f, 0.0f);
        auto side = digits.removeFromRight (40.0f);
        const auto text = bpmText (proc.getEffectiveBpm());
        {
            // soft backlight behind the digits
            ColourGradient halo (hw::col::iceDeep.withAlpha (0.28f), digits.getX() + 52.0f, digits.getCentreY(),
                                 hw::col::iceDeep.withAlpha (0.0f), digits.getX() + 140.0f, digits.getCentreY(), true);
            g.setGradientFill (halo);
            g.fillRect (r.reduced (2.0f));
        }
        card::glowText (g, text, digits.translated (0.0f, 1.0f), card::monoFont (36.0f, true), hw::col::ice.interpolatedWith (Colours::white, 0.35f),
                        Justification::centredLeft, 0.7f);
        hw::drawPrint (g, "BPM", side.withTrimmedTop (side.getHeight() * 0.48f).withTrimmedBottom (8.0f), Justification::bottomRight, 9.5f, hw::col::text2);
        hw::drawPrint (g, isManualTempo (proc) ? "FREE" : "HOST", side.withHeight (side.getHeight() * 0.5f).withTrimmedTop (9.0f),
                       Justification::topRight, 8.0f, hw::col::printDim);
        card::paintScreenGlass (g, r, 14.0f);
    }

    void mouseDown (const MouseEvent&) override { dragStartBpm = proc.getEffectiveBpm(); }

    void mouseDrag (const MouseEvent& e) override
    {
        const bool fine = e.mods.isShiftDown();
        const double perPixel = fine ? 0.05 : 0.5;
        const double q = fine ? 100.0 : 2.0;
        const double v = std::round ((dragStartBpm - e.getDistanceFromDragStartY() * perPixel) * q) / q;
        if (e.getDistanceFromDragStart() > 2) setBpm (v);
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const double step = e.mods.isShiftDown() ? 0.1 : 1.0;
        const double dir = w.deltaY > 0 ? 1.0 : (w.deltaY < 0 ? -1.0 : 0.0);
        if (dir != 0.0) setBpm (std::round ((proc.getEffectiveBpm() + dir * step * (w.isReversed ? -1.0 : 1.0)) * 10.0) / 10.0);
    }

    void mouseDoubleClick (const MouseEvent&) override
    {
        editor = std::make_unique<TextEditor>();
        addAndMakeVisible (*editor);
        editor->setBounds (getLocalBounds().reduced (6));
        editor->setJustification (Justification::centred);
        editor->setFont (card::monoFont ((float) getHeight() * 0.5f, true));
        editor->setColour (TextEditor::backgroundColourId, hw::col::screen);
        editor->setColour (TextEditor::textColourId, hw::col::ice);
        editor->setColour (TextEditor::outlineColourId, hw::col::ice.withAlpha (0.6f));
        editor->setColour (TextEditor::focusedOutlineColourId, hw::col::ice);
        editor->setInputRestrictions (6, "0123456789.,");
        editor->setText (String (proc.getEffectiveBpm(), 1), false);
        editor->selectAll();
        editor->grabKeyboardFocus();
        SafePointer<BpmDisplay> self (this);
        auto close = [this, self] (bool apply)
        {
            if (self == nullptr || editor == nullptr) return;
            if (apply)
            {
                const double v = editor->getText().replace (",", ".").getDoubleValue();
                if (v >= 40.0 && v <= 300.0) setBpm (v);
            }
            MessageManager::callAsync ([self] { if (self != nullptr) self->editor.reset(); });
        };
        editor->onReturnKey = [close] { close (true); };
        editor->onEscapeKey = [close] { close (false); };
        editor->onFocusLost = [close] { close (true); };
    }

private:
    void setBpm (double v)
    {
        setManualTempo (proc, true);
        if (auto* b = proc.getParam (ParamIDs::tempoBpm)) b->setValueNotifyingHost (b->convertTo0to1 ((float) jlimit (40.0, 300.0, v)));
    }

    EffectorKillaAudioProcessor& proc;
    double dragStartBpm = 120.0;
    String shown;
    std::unique_ptr<TextEditor> editor;
};

// ============================================================================================
// Beat dot next to TAP: lights on every beat of the (host or free) tempo.
class MasterColumn::BeatDot : public Component, public SettableTooltipClient
{
public:
    std::function<float()> level;
    BeatDot() { setTooltip ("Beat"); }
    void tick()
    {
        const float v = level ? jlimit (0.0f, 1.0f, level()) : 0.0f;
        if (std::abs (v - shown) > 0.02f) { shown = v; repaint(); }
    }
    void paint (Graphics& g) override
    {
        card::dotGlow (g, getLocalBounds().toFloat().getCentre(), 3.2f, hw::col::ice, jmax (0.0f, shown));
    }

private:
    float shown = -1.0f;
};

// ============================================================================================
// LEVELS knob: name above, flat knob, value below.
class MasterColumn::LevelKnob : public Component, public SettableTooltipClient
{
public:
    explicit LevelKnob (String n) : name (std::move (n))
    {
        addAndMakeVisible (knob);
        knob.radiusFactor = 0.56f;
        knob.onUserChange = [this] (float) { tick(); };
    }
    hw::HwKnob knob;
    std::function<String()> valueText;

    void tick()
    {
        knob.tick();
        const auto v = valueText ? valueText() : String();
        if (v != shownValue) { shownValue = v; repaint(); }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (14);
        r.removeFromBottom (16);
        knob.setBounds (r.withSizeKeepingCentre (r.getHeight(), r.getHeight()));
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        hw::drawPrint (g, name, r.removeFromTop (12.0f), Justification::centred, 8.5f, hw::col::text2);
        if (shownValue.isEmpty() && valueText) shownValue = valueText();
        g.setFont (card::monoFont (9.0f));
        g.setColour (hw::col::print.withAlpha (0.9f));
        g.drawFittedText (shownValue, r.removeFromBottom (13.0f).toNearestInt(), Justification::centred, 1, 0.7f);
    }

private:
    String name, shownValue;
};

// ============================================================================================
// KILL: big red pill with a red glow that flares after every KILL.
class MasterColumn::KillKey : public Component, public SettableTooltipClient
{
public:
    static constexpr int pad = 9;   // glow room around the pill
    std::function<void()> onClick;
    float flash = 0.0f;

    KillKey()
    {
        setMouseCursor (MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
        setTooltip ("KILL: a new, musical, level-matched chain (space bar). Locked units stay.");
    }
    bool hitTest (int x, int y) override { return getLocalBounds().reduced (pad).contains (x, y); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced ((float) pad);
        const bool hover = isMouseOver();
        if (down) r = r.reduced (1.0f);
        const float rad = r.getHeight() * 0.5f;
        hw::drawGlow (g, r, rad, kRed, 0.75f + (hover ? 0.35f : 0.0f) + 1.2f * flash, (float) pad);

        ColourGradient fill (Colour (0xffff5262).interpolatedWith (Colours::white, 0.25f * flash), 0.0f, r.getY(),
                             Colour (0xffc4142a), 0.0f, r.getBottom(), false);
        fill.addColour (0.5, kRed.interpolatedWith (Colours::white, 0.2f * flash));
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, rad);
        // glossy top half
        ColourGradient gloss (Colours::white.withAlpha (hover ? 0.30f : 0.22f), 0.0f, r.getY(), Colours::white.withAlpha (0.0f), 0.0f, r.getCentreY(), false);
        g.setGradientFill (gloss);
        g.fillRoundedRectangle (r.reduced (3.0f, 2.0f).withHeight (r.getHeight() * 0.5f), rad - 2.0f);
        g.setColour (Colour (0xffff9aa4).withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);

        // label: Archivo Black italic in white with a dark red drop
        Path p;
        {
            GlyphArrangement ga;
            const auto f = card::displayFont (r.getHeight() * 0.56f);
            const float w = GlyphArrangement::getStringWidth (f, "KILL");
            const float base = r.getCentreY() + f.getAscent() * 0.72f * 0.5f;
            ga.addLineOfText (f, "KILL", r.getCentreX() - w * 0.5f, base);
            ga.createPath (p);
            p.applyTransform (AffineTransform::shear (-card::shear, 0.0f).translated (card::shear * base, 0.0f));
        }
        g.setColour (Colour (0xff5a0610).withAlpha (0.55f));
        g.fillPath (p, AffineTransform::translation (0.0f, 1.5f));
        g.setColour (Colours::white);
        g.fillPath (p);
        // a sparkle on each side
        card::paintSparkle (g, { r.getX() + rad + 4.0f, r.getCentreY() }, 5.0f, Colours::white.withAlpha (0.85f));
        card::paintSparkle (g, { r.getRight() - rad - 4.0f, r.getCentreY() }, 5.0f, Colours::white.withAlpha (0.85f));
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

// ============================================================================================
// One macro: coloured name, flat knob whose arc follows the live (modulated) value, value readout.
class MasterColumn::MacroCell : public Component, public SettableTooltipClient
{
public:
    class RingKnob : public Knob, public SettableTooltipClient
    {
    public:
        explicit RingKnob (Colour c) : Knob (Knob::Style::Macro), ring (c) {}
        float shownValue = 0.5f;
        Colour ring;
        void paint (Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f * 0.6f;
            const bool hot = isMouseOver() || isMouseButtonDown();
            // the macro's own colour glows softly under the disc
            ColourGradient halo (ring.withAlpha (hot ? 0.22f : 0.12f), b.getCentreX(), b.getCentreY(), ring.withAlpha (0.0f),
                                 b.getCentreX() + r * 1.6f, b.getCentreY(), true);
            g.setGradientFill (halo);
            g.fillEllipse (Rectangle<float> (r * 3.2f, r * 3.2f).withCentre (b.getCentre()));
            hw::drawFlatKnob (g, b.getCentre(), r, shownValue, ring, 1.3f, getNormValue(), hot);
        }
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
        const String tip = String (tips[jlimit (0, 4, m)]) + ". The arc shows the live (modulated) value, the white tick the knob itself.";
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
        g.setFont (card::monoFont (8.5f, true));
        g.setColour (col.interpolatedWith (Colours::white, 0.3f));
        g.drawFittedText (name, r.removeFromTop (12.0f).toNearestInt(), Justification::centred, 1, 0.7f);
        card::glowText (g, String (shownPct), valueArea().toFloat(), card::monoFont (10.0f, true), col.interpolatedWith (Colours::white, 0.45f),
                        Justification::centred, 0.5f);
    }

    void resized() override
    {
        const int k = getHeight() - 12 - 14;
        knob.setBounds ((getWidth() - k) / 2, 12, k, k);
    }

private:
    Rectangle<int> valueArea() const { return { 0, getHeight() - 14, getWidth(), 13 }; }

    EffectorKillaAudioProcessor& proc;
    int index;
    RingKnob knob;
    String name;
    float lastBase = -1.0f;
    int shownPct = -1;
};

// ============================================================================================
// Slim vertical LED bar meter: ice blue -> white -> red segments, peak hold, OVER dot on top,
// name + peak readout under the bar.
class MasterColumn::LedMeter : public Component, public SettableTooltipClient
{
public:
    static constexpr float minDb = -48.0f, maxDb = 3.0f;

    LedMeter (String nm, std::function<float()> peak, float overAt)
        : name (std::move (nm)), getPeak (std::move (peak)), overThreshold (overAt)
    {
        setTooltip (name + " peak level (-48 .. +3 dBFS). The red dot lights when the signal reaches full scale.");
    }

    static float dbToPos (float db) { return jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); }
    Rectangle<float> barArea() const { return { (float) getWidth() * 0.5f - 7.0f, 16.0f, 14.0f, (float) getHeight() - 16.0f - 30.0f }; }

    void tick (bool reduceMotion)
    {
        const float pk = jmax (0.0f, getPeak());
        const float db = 20.0f * std::log10 (jmax (1.0e-5f, pk));
        if (reduceMotion || db >= level) level = db;
        else level = jmax (db, level - 1.3f);          // ~40 dB/s release
        if (level >= hold) { hold = level; holdFrames = 30; }
        else if (holdFrames > 0) --holdFrames;
        else hold = jmax (level, hold - 0.8f);
        if (pk >= overThreshold) overHold = 24; else if (overHold > 0) --overHold;
        const int segs = (int) std::round (dbToPos (level) * 200.0f) + (int) std::round (dbToPos (hold) * 200.0f) * 1000;
        const int readout = roundToInt (hold * 10.0f);
        if (segs != drawnSegs || (overHold > 0) != drawnOver || readout != drawnReadout) repaint();
    }

    void paint (Graphics& g) override
    {
        drawnSegs = (int) std::round (dbToPos (level) * 200.0f) + (int) std::round (dbToPos (hold) * 200.0f) * 1000;
        drawnOver = overHold > 0;
        drawnReadout = roundToInt (hold * 10.0f);
        const auto bar = barArea();

        // OVER dot
        card::dotGlow (g, { bar.getCentreX(), 6.0f }, 3.0f, kRed, drawnOver ? 1.0f : 0.0f);

        // track
        g.setColour (hw::col::screen);
        g.fillRoundedRectangle (bar.expanded (3.0f), 6.0f);
        g.setColour (hw::col::line);
        g.drawRoundedRectangle (bar.expanded (3.0f).reduced (0.5f), 5.5f, 1.0f);

        const float segH = 3.0f, gap = 1.6f;
        const int n = (int) ((bar.getHeight() + gap) / (segH + gap));
        const float lit = dbToPos (level), held = dbToPos (hold);
        const int heldSeg = jlimit (0, n - 1, (int) (held * (float) n));
        for (int i = 0; i < n; ++i)
        {
            const float t = ((float) i + 0.5f) / (float) n;
            const float db = minDb + t * (maxDb - minDb);
            const Colour c = segmentColour (db);
            const auto seg = Rectangle<float> (bar.getX(), bar.getBottom() - (float) (i + 1) * (segH + gap) + gap, bar.getWidth(), segH);
            const bool on = t <= lit && level > minDb + 0.5f;
            const bool peak = i == heldSeg && hold > minDb + 1.0f;
            if (on || peak)
            {
                g.setColour (c.withAlpha (0.18f));
                g.fillRoundedRectangle (seg.expanded (2.5f, 1.5f), 2.5f);
                g.setColour (peak && ! on ? c.withAlpha (0.85f) : c);
                g.fillRoundedRectangle (seg, 1.2f);
            }
            else
            {
                g.setColour (c.withAlpha (0.07f));
                g.fillRoundedRectangle (seg, 1.2f);
            }
        }

        auto txt = getLocalBounds().toFloat().removeFromBottom (26.0f);
        hw::drawPrint (g, name, txt.removeFromTop (12.0f), Justification::centred, 8.5f, hw::col::text2);
        const String v = hold <= minDb + 0.5f ? String ("-inf") : String (hold, 1);
        g.setFont (card::monoFont (9.0f, true));
        g.setColour (drawnOver ? kRed.brighter (0.3f) : hw::col::ice.withAlpha (0.85f));
        g.drawText (v, txt, Justification::centred, false);
    }

    static Colour segmentColour (float db)
    {
        const Colour iceBlue { 0xff9fc3ff }, deep { 0xff4676c4 };
        if (db < -18.0f) return deep.interpolatedWith (iceBlue, jlimit (0.0f, 1.0f, (db - minDb) / (minDb * -1.0f - 18.0f)));
        if (db < -3.0f) return iceBlue.interpolatedWith (Colour (0xfff4f7ff), (db + 18.0f) / 15.0f);
        if (db < -0.5f) return Colour (0xfff4f7ff);
        return kRed;
    }

private:
    String name;
    std::function<float()> getPeak;
    float overThreshold;
    float level = -100.0f, hold = -100.0f;
    int holdFrames = 0, overHold = 0;
    int drawnSegs = -1, drawnReadout = 0;
    bool drawnOver = false;
};

// ============================================================================================
MasterColumn::MasterColumn (EffectorKillaAudioProcessor& p)
    : proc (p),
      tapBtn (hw::HwButton::Kind::Round, "TAP", hw::col::ice),
      hostBtn (hw::HwButton::Kind::Text, "HOST", hw::col::ice),
      freeBtn (hw::HwButton::Kind::Text, "FREE", hw::col::ice),
      pgBtn (hw::HwButton::Kind::Text, "PG", hw::col::ice),
      rBtn (hw::HwButton::Kind::Text, "R", hw::col::ice),
      unratedBtn (hw::HwButton::Kind::Text, "UNRATED", kRed),
      bootlegBtn (hw::HwButton::Kind::Text, "BOOTLEG", hw::col::ice),
      phoneBtn (hw::HwButton::Kind::Led, "PHONE CHECK", hw::col::amber),
      autoGainBtn (hw::HwButton::Kind::Led, "AUTO GAIN", hw::col::green),
      smallBtn (hw::HwButton::Kind::Text, "SMALL", hw::col::ice),
      fullBtn (hw::HwButton::Kind::Text, "FULL", hw::col::ice),
      autoBtn (hw::HwButton::Kind::Text, "AUTO", hw::col::ice)
{
    setOpaque (false);
    lastKillEvents = proc.killEvents.load();

    // ---- tempo ------------------------------------------------------------------------------
    bpm = std::make_unique<BpmDisplay> (proc);
    addAndMakeVisible (*bpm);
    beat = std::make_unique<BeatDot>();
    beat->setInterceptsMouseClicks (false, false);
    beat->level = [this]
    {
        if (reduceMotion) return 0.35f;
        const double ppq = proc.engine.meters.ppq.load();
        return ppq - std::floor (ppq) < 0.16 ? 1.0f : 0.0f;
    };
    addAndMakeVisible (*beat);
    tapBtn.onPress = [this] (bool down) { if (down) { proc.tapTempo(); tapBtn.flash(); } };
    tapBtn.setTooltip ("TAP: tap the tempo (2 or more taps, T key) - switches to FREE tempo");
    hostBtn.isLit = [this] { return ! isManualTempo (proc); };
    freeBtn.isLit = [this] { return isManualTempo (proc); };
    hostBtn.onClick = [this] { setManualTempo (proc, false); };
    freeBtn.onClick = [this]
    {
        // start the free tempo from what is playing now
        if (auto* b = proc.getParam (ParamIDs::tempoBpm)) b->setValueNotifyingHost (b->convertTo0to1 ((float) proc.getEffectiveBpm()));
        setManualTempo (proc, true);
    };
    hostBtn.setTooltip ("HOST: follow the DAW tempo (sync)");
    freeBtn.setTooltip ("FREE: own tempo - drag / type the BPM or TAP");

    // ---- levels -----------------------------------------------------------------------------
    auto dbFmt = [] (float v) { return (v > 0.04f ? "+" : "") + String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB"; };
    auto pctFmt = [] (float v) { return String (roundToInt (v * 100.0f)) + " %"; };
    struct L { const char* name; String id; bool db; const char* tip; };
    const L ls[] = { { "IN", ParamIDs::inGain, true, "IN: level into the first unit" },
                     { "OUT", ParamIDs::outGain, true, "OUT: level out of the chain" },
                     { "RECYCLE", ParamIDs::recycle, false, "RECYCLE: feeds the chain output back into its input - repeats, extra drive, "
                                                            "self-oscillation at high settings. The amber arc warns how hot it is." },
                     { "MIX", ParamIDs::blend, false, "MIX: dry / wet of the whole chain (100 % for sends)" } };
    for (auto& l : ls)
    {
        auto* k = levels.add (new LevelKnob (l.name));
        if (auto* rp = proc.getParam (l.id))
        {
            k->knob.bindTo (*rp);
            std::function<String (float)> fmt = l.db ? std::function<String (float)> (dbFmt) : std::function<String (float)> (pctFmt);
            k->valueText = [rp, fmt] { return fmt (rp->convertFrom0to1 (rp->getValue())); };
        }
        k->setTooltip (l.tip);
        addAndMakeVisible (k);
    }
    levels[2]->knob.ring = hw::col::amber;
    levels[2]->knob.getDisplayValue = nullptr;

    // ---- kill -------------------------------------------------------------------------------
    killKey = std::make_unique<KillKey>();
    killKey->onClick = [this]
    {
        proc.kill();
        lastKillEvents = proc.killEvents.load();
        fireKillFlash();
    };
    addAndMakeVisible (*killKey);
    auto rating = [this] (hw::HwButton& b, KillLevel l)
    {
        b.isLit = [this, l] { return proc.killLevel == l; };
        b.onClick = [this, l] { proc.killLevel = l; };
        b.setTooltip ("KILL rating - PG: same units, small changes / R: new chain of 3 - 5 units / UNRATED: 4 - 7 units, hotter settings");
    };
    rating (pgBtn, KillLevel::PG);
    rating (rBtn, KillLevel::R);
    rating (unratedBtn, KillLevel::Unrated);
    bootlegBtn.isLit = [this] { return bootlegLit > 0.05f; };
    bootlegBtn.onClick = [this]
    {
        proc.bootleg();
        if (! reduceMotion) { bootlegLit = 1.0f; bootlegBtn.flash(); }
    };
    bootlegBtn.setTooltip ("BOOTLEG: subtle variation of every setting, same units");

    // ---- macros -----------------------------------------------------------------------------
    for (int m = 0; m < kNumMacros; ++m)
        addAndMakeVisible (macros.add (new MacroCell (proc, m)));

    // ---- meters -----------------------------------------------------------------------------
    auto& meters = proc.engine.meters;
    meterIn = std::make_unique<LedMeter> ("IN", [&meters] { return meters.inLevel.load(); }, 0.999f);
    meterOut = std::make_unique<LedMeter> ("OUT", [&meters] { return jmax (meters.outPeak.load(), meters.outLevel.load()); }, 0.989f);
    addAndMakeVisible (*meterIn);
    addAndMakeVisible (*meterOut);

    // ---- toolbox ----------------------------------------------------------------------------
    auto toggle = [this] (hw::HwButton& b, const String& id, const String& tip)
    {
        b.isLit = [this, id] { auto* q = proc.getParam (id); return q != nullptr && q->getValue() > 0.5f; };
        b.onClick = [this, id] { if (auto* q = proc.getParam (id)) q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f); };
        b.setTooltip (tip);
    };
    toggle (autoGainBtn, ParamIDs::autoTracking, "AUTO GAIN: keeps the output as loud as the input");
    toggle (phoneBtn, ParamIDs::pocketTv, "PHONE CHECK: hear it like a phone speaker (monitoring only - switch it off before export!)");
    phoneBtn.blinkWhenLit = true;
    auto sizeIs = [this] (float s) { return [this, s]
    {
        const float u = proc.uiScale;
        return s <= 0.0f ? u <= 0.0f : (u > 0.0f && std::abs (u - s) < 0.01f);
    }; };
    smallBtn.isLit = sizeIs (0.8f);
    fullBtn.isLit = sizeIs (1.0f);
    autoBtn.isLit = sizeIs (0.0f);
    smallBtn.onClick = [this] { if (onSetScale) onSetScale (0.8f); };
    fullBtn.onClick = [this] { if (onSetScale) onSetScale (1.0f); };
    autoBtn.onClick = [this] { if (onSetScale) onSetScale (0.0f); };
    smallBtn.setTooltip ("SMALL: window at 80 %");
    fullBtn.setTooltip ("FULL: window at 100 %");
    autoBtn.setTooltip ("AUTO: window size fits the screen (or drag the corner)");

    for (auto* b : { &tapBtn, &hostBtn, &freeBtn, &pgBtn, &rBtn, &unratedBtn, &bootlegBtn, &phoneBtn, &autoGainBtn,
                     &smallBtn, &fullBtn, &autoBtn })
    {
        b->fontSize = 9.5f;
        addAndMakeVisible (b);
    }
    bootlegBtn.fontSize = 10.0f;

    setSize (kWidth, kHeight);
    startTimerHz (30);
}

MasterColumn::~MasterColumn()
{
    stopTimer();
}

void MasterColumn::setReduceMotion (bool shouldReduce)
{
    reduceMotion = shouldReduce;
    if (reduceMotion)
    {
        killKey->flash = 0.0f;
        bootlegLit = 0.0f;
        killKey->repaint();
    }
}

void MasterColumn::fireKillFlash()
{
    if (! reduceMotion)
    {
        killKey->flash = 1.0f;
        killKey->repaint();
    }
    if (onKillFlash) onKillFlash();
}

void MasterColumn::tick()
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
        killKey->flash = killKey->flash < 0.03f ? 0.0f : killKey->flash * 0.84f;
        killKey->repaint();
    }
    if (bootlegLit > 0.0f) bootlegLit = (reduceMotion || bootlegLit < 0.05f) ? 0.0f : bootlegLit * 0.8f;

    bpm->tick();
    beat->tick();
    for (auto* b : { &tapBtn, &hostBtn, &freeBtn, &pgBtn, &rBtn, &unratedBtn, &bootlegBtn, &phoneBtn, &autoGainBtn,
                     &smallBtn, &fullBtn, &autoBtn })
        b->tick();
    for (auto* l : levels) l->tick();
    // RECYCLE warning arc glows more the higher it is
    auto& rk = levels[2]->knob;
    const float rv = rk.getNormValue();
    const float glow = rv <= 0.001f ? 0.0f : 0.25f + 0.75f * rv;
    if (std::abs (glow - rk.ringGlow) > 0.01f) { rk.ringGlow = glow; rk.repaint(); }
    for (auto* m : macros) m->tick();
    meterIn->tick (reduceMotion);
    meterOut->tick (reduceMotion);
}

void MasterColumn::resized()
{
    // TEMPO
    {
        const auto a = tempoArea();
        bpm->setBounds (a.getX(), a.getY() + 16, a.getWidth(), 58);
        const int y = a.getY() + 82;
        tapBtn.setBounds (a.getX(), y, 58, 24);
        beat->setBounds (a.getX() + 60, y, 16, 24);
        hostBtn.setBounds (a.getX() + 82, y, 55, 24);
        freeBtn.setBounds (a.getRight() - 55, y, 55, 24);
    }
    // LEVELS
    {
        const auto a = levelsArea();
        const int w = a.getWidth() / 4;
        for (int i = 0; i < levels.size(); ++i) levels[i]->setBounds (a.getX() + i * w, a.getY() + 16, w, 76);
    }
    // KILL
    {
        const auto a = killArea();
        const int pad = KillKey::pad;
        killKey->setBounds (a.getX() - pad, a.getY() + 16 - pad, a.getWidth() + 2 * pad, 44 + 2 * pad);
        int y = a.getY() + 68;
        pgBtn.setBounds (a.getX(), y, 46, 22);
        rBtn.setBounds (a.getX() + 50, y, 46, 22);
        unratedBtn.setBounds (a.getX() + 100, y, a.getWidth() - 100, 22);
        y += 26;
        bootlegBtn.setBounds (a.getX(), y, a.getWidth(), 22);
    }
    // MACROS: 3 + 2
    {
        const auto a = macrosArea();
        const int w = a.getWidth() / 3, h = 72;
        for (int m = 0; m < macros.size(); ++m)
        {
            if (m < 3) macros[m]->setBounds (a.getX() + m * w, a.getY() + 16, w, h);
            else macros[m]->setBounds (a.getX() + w / 2 + (m - 3) * w, a.getY() + 16 + h + 4, w, h);
        }
    }
    // METERS
    {
        const auto a = metersArea();
        meterIn->setBounds (a.getX() + 30, a.getY() + 14, 52, a.getHeight() - 14);
        meterOut->setBounds (a.getRight() - 82, a.getY() + 14, 52, a.getHeight() - 14);
    }
    // TOOLBOX
    {
        const auto a = toolboxArea();
        int y = a.getY() + 18;
        phoneBtn.setBounds (a.getX(), y, 108, 24);
        autoGainBtn.setBounds (a.getX() + 112, y, a.getWidth() - 112, 24);
        y += 32;
        smallBtn.setBounds (a.getX() + 40, y, 54, 22);
        fullBtn.setBounds (a.getX() + 98, y, 46, 22);
        autoBtn.setBounds (a.getX() + 148, y, a.getWidth() - 148, 22);
    }
}

void MasterColumn::paint (Graphics& g)
{
    background.draw (g, getLocalBounds(), [this] (Graphics& bg) { paintStatic (bg); });
}

void MasterColumn::paintStatic (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    hw::drawCard (g, r.reduced (0.5f), card::radius);
    // title
    card::paintChromeTitle (g, "MASTER", { 18.0f, 10.0f, 150.0f, 30.0f }, 21.0f, 0.9f);
    card::paintSparkle (g, { r.getRight() - 24.0f, 25.0f }, 6.0f, hw::col::ice, 1.0f);
    card::paintSparkle (g, { r.getRight() - 38.0f, 18.0f }, 2.5f, hw::col::ice.withAlpha (0.7f), 0.6f);

    auto header = [&] (Rectangle<int> a, const String& label)
    {
        hw::drawSectionHeader (g, a.toFloat().withHeight (12.0f), label);
    };
    header (tempoArea(), "TEMPO");
    header (levelsArea(), "LEVELS");
    header (killArea(), "KILL");
    header (macrosArea(), "MACROS");
    header (metersArea(), "METERS");
    header (toolboxArea(), "TOOLBOX");

    // meter scale between the two bars
    {
        const auto a = metersArea();
        const float barTop = (float) a.getY() + 14.0f + 16.0f;
        const float barH = (float) a.getHeight() - 14.0f - 16.0f - 30.0f;
        const float x0 = (float) a.getX() + 30.0f + 26.0f + 12.0f, x1 = (float) a.getRight() - 82.0f + 26.0f - 12.0f;
        for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float y = barTop + barH * (1.0f - LedMeter::dbToPos (db));
            g.setColour (hw::col::ice.withAlpha (0.18f));
            g.drawHorizontalLine ((int) y, x0, x0 + 6.0f);
            g.drawHorizontalLine ((int) y, x1 - 6.0f, x1);
            hw::drawPrint (g, db == 0.0f ? String ("0") : String ((int) db), { x0 + 6.0f, y - 5.0f, x1 - x0 - 12.0f, 10.0f },
                           Justification::centred, 8.0f, db == 0.0f ? hw::col::text2 : hw::col::printDim);
        }
    }
    // SIZE label
    {
        const auto a = toolboxArea();
        hw::drawPrint (g, "SIZE", { (float) a.getX(), (float) a.getY() + 50.0f, 38.0f, 22.0f }, Justification::centredLeft, 9.0f, hw::col::printDim);
    }
}

int MasterColumn::macroAt (Point<int> screenPos) const
{
    for (int m = 0; m < macros.size(); ++m)
        if (macros[m]->isShowing() && macros[m]->getScreenBounds().contains (screenPos)) return m;
    return -1;
}

Rectangle<int> MasterColumn::macroScreenBounds (int macro) const
{
    if (! isPositiveAndBelow (macro, macros.size())) return {};
    return macros[macro]->getScreenBounds();
}
} // namespace ek::ui
