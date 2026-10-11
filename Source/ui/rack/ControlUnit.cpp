#include "ControlUnit.h"
#include "../Brand.h"

namespace ek::ui
{
using namespace juce;

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

String bpmText (double bpm)
{
    // 4 cells: "140.0", "85.5" (leading blank), "300" shown as "300.0"
    return String (jlimit (0.0, 999.9, bpm), 1);
}
} // namespace

// ============================================================================================
SevenSegBpm::SevenSegBpm (EffectorKillaAudioProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setTooltip ("Tempo (BPM). Drag up / down (Shift = fine), mouse wheel, double-click to type. Changing it switches to FREE tempo.");
}

void SevenSegBpm::setBpm (double v)
{
    setManualTempo (proc, true);
    if (auto* b = proc.getParam (ParamIDs::tempoBpm)) b->setValueNotifyingHost (b->convertTo0to1 ((float) jlimit (40.0, 300.0, v)));
}

void SevenSegBpm::tick()
{
    const auto s = bpmText (proc.getEffectiveBpm()) + (isManualTempo (proc) ? "F" : "H");
    if (s != shown) { shown = s; repaint(); }
}

void SevenSegBpm::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    hw::drawLcdGlass (g, r, 3.0f);
    auto digits = r.reduced (9.0f, 7.0f);
    hw::drawSevenSegment (g, digits, bpmText (proc.getEffectiveBpm()), hw::col::seg, hw::col::segOff, 4, 1.0f);
    hw::drawGlassReflection (g, r, 3.0f);
}

void SevenSegBpm::mouseDown (const MouseEvent&) { dragStartBpm = proc.getEffectiveBpm(); }

void SevenSegBpm::mouseDrag (const MouseEvent& e)
{
    const bool fine = e.mods.isShiftDown();
    const double perPixel = fine ? 0.05 : 0.5;
    const double q = fine ? 100.0 : 2.0;
    const double v = std::round ((dragStartBpm - e.getDistanceFromDragStartY() * perPixel) * q) / q;
    if (e.getDistanceFromDragStart() > 2) setBpm (v);
}

void SevenSegBpm::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const double step = e.mods.isShiftDown() ? 0.1 : 1.0;
    const double dir = w.deltaY > 0 ? 1.0 : (w.deltaY < 0 ? -1.0 : 0.0);
    if (dir != 0.0) setBpm (std::round ((proc.getEffectiveBpm() + dir * step * (w.isReversed ? -1.0 : 1.0)) * 10.0) / 10.0);
}

void SevenSegBpm::mouseDoubleClick (const MouseEvent&)
{
    editor = std::make_unique<TextEditor>();
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds().reduced (3));
    editor->setJustification (Justification::centred);
    editor->setFont (theme::vfdFont ((float) getHeight() * 0.6f));
    editor->setColour (TextEditor::backgroundColourId, Colour (0xff03060c));
    editor->setColour (TextEditor::textColourId, hw::col::seg);
    editor->setColour (TextEditor::outlineColourId, hw::col::ice);
    editor->setColour (TextEditor::focusedOutlineColourId, hw::col::ice);
    editor->setInputRestrictions (6, "0123456789.,");
    editor->setText (String (proc.getEffectiveBpm(), 1), false);
    editor->selectAll();
    editor->grabKeyboardFocus();
    SafePointer<SevenSegBpm> self (this);
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

// ============================================================================================
void LedIndicator::tick()
{
    const float v = level ? jlimit (0.0f, 1.0f, level()) : 0.0f;
    if (std::abs (v - shown) > 0.02f) { shown = v; repaint(); }
}

void LedIndicator::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float v = jmax (0.0f, shown);
    const float rad = jmin (r.getWidth(), r.getHeight()) * 0.5f / 2.0f;
    theme::drawLed (g, r.getCentre(), rad, colour, v > 0.05f, v);
}

// ============================================================================================
LevelKnob::LevelKnob (String n, String mn, String mx) : name (std::move (n)), minText (std::move (mn)), maxText (std::move (mx))
{
    addAndMakeVisible (knob);
    knob.onUserChange = [this] (float) { tick(); };
}

void LevelKnob::tick()
{
    knob.tick();
    const auto v = valueText ? valueText() : String();
    if (v != shownValue) { shownValue = v; repaint(); }
}

void LevelKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (12);
    r.removeFromBottom (13);
    knob.setBounds (r.withSizeKeepingCentre (r.getHeight(), r.getHeight()));
}

void LevelKnob::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    hw::drawPrint (g, name, r.removeFromTop (12.0f), Justification::centred, 9.5f, hw::col::print);
    auto v = r.removeFromBottom (13.0f);
    if (shownValue.isEmpty() && valueText) shownValue = valueText();
    g.setFont (theme::monoLabel (10.0f).boldened());
    g.setColour (hw::col::ice);
    g.drawFittedText (shownValue, v.toNearestInt(), Justification::centred, 1, 0.7f);
    // MIN / MAX printed under the ends of the scale
    const auto kb = knob.getBounds().toFloat();
    const float y = kb.getBottom() - 9.0f;
    hw::drawPrint (g, minText, { kb.getX() - 16.0f, y, 30.0f, 9.0f }, Justification::centredRight, 7.5f);
    hw::drawPrint (g, maxText, { kb.getRight() - 14.0f, y, 30.0f, 9.0f }, Justification::centredLeft, 7.5f);
}

// ============================================================================================
ControlUnit::ControlUnit (EffectorKillaAudioProcessor& p)
    : proc (p),
      tapBtn (hw::HwButton::Kind::Round, "TAP", hw::col::ice),
      hostBtn (hw::HwButton::Kind::Led, "HOST", hw::col::green),
      freeBtn (hw::HwButton::Kind::Led, "FREE", hw::col::ice),
      bpm (p), beatLed (hw::col::ice),
      inKnob ("INPUT", "MIN", "MAX"), outKnob ("OUTPUT", "MIN", "MAX"),
      recycleKnob ("RECYCLE", "MIN", "MAX"), mixKnob ("MIX", "DRY", "WET"),
      smallBtn (hw::HwButton::Kind::Led, "SMALL", hw::col::ice),
      fullBtn (hw::HwButton::Kind::Led, "FULL", hw::col::ice),
      autoBtn (hw::HwButton::Kind::Led, "AUTO", hw::col::ice),
      gearBtn (hw::HwButton::Kind::Led, "SHOW GEAR", hw::col::ice),
      autoGainBtn (hw::HwButton::Kind::Led, "AUTO GAIN", hw::col::green),
      phoneBtn (hw::HwButton::Kind::Led, "PHONE CHECK", hw::col::amber)
{
    setOpaque (true);
    setSize (width, height);

    // ---- tempo ------------------------------------------------------------------------------
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
    beatLed.level = [this]
    {
        const double ppq = proc.engine.meters.ppq.load();
        const double ph = ppq - std::floor (ppq);
        if (hw::reduceMotion()) return 0.35f;
        return ph < 0.16 ? 1.0f : 0.0f;
    };
    beatLed.setTooltip ("Beat");

    // ---- levels -----------------------------------------------------------------------------
    auto bind = [this] (LevelKnob& k, const String& id, std::function<String (float)> fmt, const String& tip)
    {
        if (auto* rp = proc.getParam (id))
        {
            k.knob.bindTo (*rp);
            k.valueText = [rp, fmt] { return fmt (rp->convertFrom0to1 (rp->getValue())); };
        }
        k.setTooltip (tip);
        addAndMakeVisible (k);
    };
    auto dbFmt = [] (float v) { return (v > 0.04f ? "+" : "") + String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB"; };
    auto pctFmt = [] (float v) { return String (roundToInt (v * 100.0f)) + " %"; };
    bind (inKnob, ParamIDs::inGain, dbFmt, "INPUT: level into the first unit");
    bind (outKnob, ParamIDs::outGain, dbFmt, "OUTPUT: level out of the rack");
    bind (recycleKnob, ParamIDs::recycle, pctFmt, "RECYCLE: feeds the rack output back into its input - repeats, extra drive, "
                                                  "self-oscillation at high settings. The amber ring warns how hot it is.");
    bind (mixKnob, ParamIDs::blend, pctFmt, "MIX: dry / wet of the whole rack (100 % for sends)");
    recycleKnob.knob.ring = hw::col::amber;
    recycleKnob.knob.getDisplayValue = nullptr;

    // ---- size -------------------------------------------------------------------------------
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

    // ---- toolbox ----------------------------------------------------------------------------
    gearBtn.isLit = [this] { return isGearShown && isGearShown(); };
    gearBtn.onClick = [this] { if (onToggleGear) onToggleGear(); };
    gearBtn.setTooltip ("SHOW GEAR: show / hide the effect list on the right (G)");
    auto toggle = [this] (hw::HwButton& b, const String& id, const String& tip)
    {
        b.isLit = [this, id] { auto* q = proc.getParam (id); return q != nullptr && q->getValue() > 0.5f; };
        b.onClick = [this, id] { if (auto* q = proc.getParam (id)) q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f); };
        b.setTooltip (tip);
    };
    toggle (autoGainBtn, ParamIDs::autoTracking, "AUTO GAIN: keeps the output as loud as the input");
    toggle (phoneBtn, ParamIDs::pocketTv, "PHONE CHECK: hear it like a phone speaker (monitoring only - switch it off before export!)");
    phoneBtn.blinkWhenLit = true;

    for (Component* c : std::initializer_list<Component*> { &tapBtn, &hostBtn, &freeBtn, &bpm, &beatLed,
                                                            &smallBtn, &fullBtn, &autoBtn, &gearBtn, &autoGainBtn, &phoneBtn })
        addAndMakeVisible (c);
}

void ControlUnit::tick()
{
    for (auto* b : { &tapBtn, &hostBtn, &freeBtn, &smallBtn, &fullBtn, &autoBtn, &gearBtn, &autoGainBtn, &phoneBtn }) b->tick();
    bpm.tick();
    beatLed.tick();
    for (auto* k : { &inKnob, &outKnob, &recycleKnob, &mixKnob }) k->tick();
    // RECYCLE warning ring glows more the higher it is
    const float rv = recycleKnob.knob.getNormValue();
    const float glow = rv <= 0.001f ? 0.0f : 0.25f + 0.75f * rv;
    if (std::abs (glow - recycleKnob.knob.ringGlow) > 0.01f) { recycleKnob.knob.ringGlow = glow; recycleKnob.knob.repaint(); }
}

void ControlUnit::resized()
{
    // TEMPO / SYNC
    {
        const auto s = tempoSection();
        tapBtn.setBounds ((int) s.getX() + 6, 84, 56, 56);
        bpm.setBounds ((int) s.getX() + 72, 82, 114, 44);
        beatLed.setBounds ((int) s.getX() + 72, 133, 16, 16);
        hostBtn.setBounds ((int) s.getRight() - 58, 82, 52, 24);
        freeBtn.setBounds ((int) s.getRight() - 58, 112, 52, 24);
    }
    // LEVELS
    {
        const auto s = levelsSection();
        const int kw = (int) (s.getWidth() / 4.0f);
        int x = (int) s.getX();
        for (auto* k : { &inKnob, &outKnob, &recycleKnob, &mixKnob }) { k->setBounds (x, 74, kw, 86); x += kw; }
    }
    // SIZE / TOOLBOX
    {
        const auto s = sizeSection();
        int y = 80;
        for (auto* b : { &smallBtn, &fullBtn, &autoBtn }) { b->setBounds ((int) s.getX() + 8, y, (int) s.getWidth() - 16, 22); y += 26; }
        const auto t = toolboxSection();
        y = 80;
        for (auto* b : { &gearBtn, &autoGainBtn, &phoneBtn }) { b->setBounds ((int) t.getX() + 8, y, (int) t.getWidth() - 16, 22); y += 26; }
    }
}

void ControlUnit::paint (Graphics& g)
{
    background.draw (g, getLocalBounds(), [this] (Graphics& bg) { paintStatic (bg); });
}

void ControlUnit::paintStatic (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.fillAll (hw::col::shaft);
    hw::drawRackPanel (g, r, hw::col::blackAlu, 22.0f, 11);

    // ---- header band: logo left, chrome EFFECTOR + RACK right, vents between ---------------
    const auto& logo = brand::logo();
    float logoRight = 32.0f;
    if (logo.isValid())
    {
        const float h = 44.0f, w = h * (float) logo.getWidth() / (float) logo.getHeight();
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (logo, Rectangle<float> (32.0f, 6.0f, w, h), RectanglePlacement::centred);
        logoRight = 32.0f + w;
    }
    // RACK tag (ice blue) and chrome title
    const auto rackTag = Rectangle<float> (852.0f, 20.0f, 48.0f, 18.0f);
    g.setColour (hw::col::ice.withAlpha (0.12f));
    g.fillRoundedRectangle (rackTag, 2.0f);
    g.setColour (hw::col::ice.withAlpha (0.85f));
    g.drawRoundedRectangle (rackTag.reduced (0.5f), 2.0f, 1.0f);
    theme::drawGlowText (g, "RACK", rackTag.translated (0.0f, 0.5f), theme::monoLabel (11.0f).boldened(), hw::col::ice, Justification::centred, 0.6f);
    const auto title = Rectangle<float> (606.0f, 9.0f, 238.0f, 40.0f);
    theme::drawChromeText (g, "EFFECTOR", title, theme::stencilFont (38.0f), Justification::centredRight, 0.7f);

    // ventilation slots
    {
        const float x0 = logoRight + 26.0f, x1 = 590.0f;
        const int n = jmax (0, (int) ((x1 - x0) / 13.0f));
        const float span = (float) n * 13.0f - 6.0f;
        float x = x0 + ((x1 - x0) - span) * 0.5f;
        for (int i = 0; i < n; ++i, x += 13.0f)
        {
            auto slot = Rectangle<float> (7.0f, 26.0f).withPosition (x, 15.0f);
            g.setColour (Colour (0xff030405));
            g.fillRoundedRectangle (slot, 3.5f);
            g.setColour (Colours::white.withAlpha (0.10f));
            g.drawLine (slot.getX() + 1.5f, slot.getBottom() + 0.6f, slot.getRight() - 1.5f, slot.getBottom() + 0.6f, 0.8f);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawLine (slot.getX() + 2.0f, slot.getY() + 1.0f, slot.getRight() - 2.0f, slot.getY() + 1.0f, 1.0f);
        }
    }

    // ---- sections ---------------------------------------------------------------------------
    auto section = [&] (Rectangle<float> s, const String& label)
    {
        g.setColour (Colours::black.withAlpha (0.22f));
        g.fillRect (s);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawRect (s, 1.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) s.getBottom(), s.getX(), s.getRight() + 1.0f);
        hw::drawSectionHeader (g, s.withHeight (16.0f).reduced (1.0f, 0.0f).withTrimmedTop (1.0f), label);
    };
    section (tempoSection(), "TEMPO / SYNC");
    section (levelsSection(), "LEVELS");
    section (sizeSection(), "SIZE");
    section (toolboxSection(), "TOOLBOX");

    // tempo prints
    const auto ts = tempoSection();
    hw::drawPrint (g, "BPM", { ts.getX() + 150.0f, 133.0f, 36.0f, 16.0f }, Justification::centredRight, 9.0f, hw::col::print);
    hw::drawPrint (g, "BEAT", { ts.getX() + 91.0f, 133.0f, 40.0f, 16.0f }, Justification::centredLeft, 8.0f);
    hw::drawPrint (g, "SYNC", { ts.getRight() - 58.0f, 140.0f, 52.0f, 14.0f }, Justification::centred, 8.0f);
    // thin divider lines between the level knobs
    const auto ls = levelsSection();
    g.setColour (Colours::black.withAlpha (0.4f));
    for (int i = 1; i < 4; ++i)
    {
        const float x = ls.getX() + ls.getWidth() * (float) i / 4.0f;
        g.drawVerticalLine ((int) x, ls.getY() + 24.0f, ls.getBottom() - 8.0f);
    }
}
} // namespace ek::ui
