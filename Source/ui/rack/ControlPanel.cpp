#include "ControlPanel.h"
#include "../Brand.h"
#include "../../engine/Macro.h"

namespace ek::ui
{
using namespace juce;

static const String kStar = String::fromUTF8 ("\xe2\x98\x85");
static const String kLeft = String::fromUTF8 ("\xe2\x97\x80");
static const String kRight = String::fromUTF8 ("\xe2\x96\xb6");
static const String kAmber = "amber";

// ============================================================================
BpmDisplay::BpmDisplay (EffectorKillaAudioProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setTooltip ("Tempo. Drag up / down (Shift = fine) or use the wheel, double-click to type. Changing it switches to MANUAL.");
}

bool BpmDisplay::manual() const
{
    auto* m = proc.getParam (ParamIDs::tempoMode);
    return m != nullptr && m->getValue() > 0.5f;
}

void BpmDisplay::setBpm (double bpm)
{
    if (auto* m = proc.getParam (ParamIDs::tempoMode)) if (m->getValue() < 0.5f) m->setValueNotifyingHost (1.0f);
    if (auto* b = proc.getParam (ParamIDs::tempoBpm)) b->setValueNotifyingHost (b->convertTo0to1 ((float) jlimit (40.0, 300.0, bpm)));
}

void BpmDisplay::tick()
{
    const double bpm = proc.getEffectiveBpm();
    const auto s = String (bpm, 1) + (manual() ? "M" : "H");
    if (s != shown) { shown = s; repaint(); }
}

void BpmDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff03060c));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    const double bpm = proc.getEffectiveBpm();
    theme::drawGlowText (g, String (bpm, 1), r.withTrimmedRight (34.0f), theme::vfdFont (r.getHeight() * 0.72f),
                         theme::vfd, Justification::centredRight, 0.8f);
    drawSmallLabel (g, "BPM", r.removeFromRight (32.0f), Justification::centred, theme::silverDim, 9.5f);
}

void BpmDisplay::mouseDown (const MouseEvent&)
{
    dragStartBpm = proc.getEffectiveBpm();
}

void BpmDisplay::mouseDrag (const MouseEvent& e)
{
    const double perPixel = e.mods.isShiftDown() ? 0.05 : 0.5;
    const double v = std::round ((dragStartBpm - e.getDistanceFromDragStartY() * perPixel) * (e.mods.isShiftDown() ? 100.0 : 2.0))
                     / (e.mods.isShiftDown() ? 100.0 : 2.0);
    if (e.getDistanceFromDragStart() > 2) setBpm (v);
}

void BpmDisplay::mouseUp (const MouseEvent&) {}

void BpmDisplay::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const double step = e.mods.isShiftDown() ? 0.1 : 1.0;
    const double dir = w.deltaY > 0 ? 1.0 : (w.deltaY < 0 ? -1.0 : 0.0);
    if (dir != 0.0) setBpm (std::round ((proc.getEffectiveBpm() + dir * step * (w.isReversed ? -1.0 : 1.0)) * 10.0) / 10.0);
}

void BpmDisplay::mouseDoubleClick (const MouseEvent&)
{
    editor = std::make_unique<TextEditor>();
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds().reduced (3));
    editor->setJustification (Justification::centred);
    editor->setFont (theme::vfdFont ((float) getHeight() * 0.6f));
    editor->setColour (TextEditor::backgroundColourId, Colour (0xff03060c));
    editor->setColour (TextEditor::textColourId, theme::vfd);
    editor->setColour (TextEditor::outlineColourId, theme::accent);
    editor->setInputRestrictions (6, "0123456789.,");
    editor->setText (String (proc.getEffectiveBpm(), 1), false);
    editor->selectAll();
    editor->grabKeyboardFocus();
    SafePointer<BpmDisplay> self (this);
    auto close = [this, self] (bool apply)
    {
        if (editor == nullptr) return;
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

// ============================================================================
void PresetDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff03060c));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (isMouseOver() ? theme::accent.withAlpha (0.6f) : theme::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto& m = proc.getMeta();
    auto inner = r.reduced (12.0f, 6.0f);
    const bool fav = proc.isCurrentFavourite();
    theme::drawGlowText (g, (fav ? kStar + " " : String()) + m.name.toUpperCase(), inner.removeFromTop (inner.getHeight() * 0.6f),
                         theme::vfdFont (22.0f), theme::vfd, Justification::centredLeft, 0.7f);
    String sub;
    if (m.factory && m.channel >= 0 && m.channel < proc.bank.getNumChannels())
        sub = "CH " + String (m.channel + 1).paddedLeft ('0', 2) + "  " + proc.bank.getChannels()[(size_t) m.channel].name + "  -  FACTORY";
    else if (m.seed != 0)
        sub = "KILL  #" + String::toHexString ((int64) (m.seed & 0xffffff)).toUpperCase();
    else
        sub = "USER";
    sub << "  -  SOURCE " << sourceName (m.source).toUpperCase();
    drawSmallLabel (g, sub, inner, Justification::centredLeft, theme::silverDim, 10.0f);
    drawSmallLabel (g, String::fromUTF8 ("BROWSE \xe2\x96\xbe"), r.reduced (10.0f, 6.0f).removeFromTop (16.0f), Justification::topRight,
                    theme::accent.withAlpha (isMouseOver() ? 1.0f : 0.5f), 9.0f);
}

// ============================================================================
ControlPanel::ControlPanel (EffectorKillaAudioProcessor& p)
    : proc (p), presetDisplay (p),
      prevBtn (kLeft, theme::accent, 11.0f), nextBtn (kRight, theme::accent, 11.0f),
      browseBtn ("BROWSE", theme::accent), saveBtn ("SAVE", theme::accent), favBtn (kStar, Colour (0xffffc861), 13.0f),
      aBtn ("A", theme::accent), bBtn ("B", theme::accent), undoBtn ("UNDO", theme::accent), redoBtn ("REDO", theme::accent),
      hostBtn ("HOST", theme::accent), manualBtn ("MANUAL", theme::accent), tapBtn ("TAP", theme::accent, 12.0f),
      bpm (p),
      inKnob ("Input"), recycleKnob ("Recycle"), mixKnob ("Mix"), outKnob ("Output"),
      killBtn ("KILL", theme::killRed, 24.0f), pgBtn ("PG", theme::accent, 10.0f), rBtn ("R", theme::accent, 10.0f),
      unratedBtn ("UNRATED", theme::accent, 9.0f), bootlegBtn ("BOOTLEG", theme::accent, 10.0f),
      autoGainBtn ("AUTO GAIN", theme::accent), phoneBtn ("PHONE CHECK", Colour (0xffffc861)),
      compareBtn ("COMPARE", theme::killRed), gearBtn ("GEAR", theme::accent), settingsBtn (String::fromUTF8 ("SIZE / SETUP \xe2\x96\xbe"), theme::accent)
{
    // ---- presets --------------------------------------------------------------------------
    addAndMakeVisible (presetDisplay);
    presetDisplay.onClick = [this] { if (onBrowse) onBrowse(); };
    presetDisplay.setTooltip ("Open the preset browser");
    prevBtn.onClick = [this] { proc.stepPreset (-1); };
    nextBtn.onClick = [this] { proc.stepPreset (1); };
    prevBtn.setTooltip ("Previous preset");
    nextBtn.setTooltip ("Next preset");
    browseBtn.onClick = [this] { if (onBrowse) onBrowse(); };
    browseBtn.isLit = [this] { return isBrowserShown && isBrowserShown(); };
    browseBtn.setTooltip ("Preset browser: channels, favourites, your presets, search");
    saveBtn.onClick = [this] { if (onSave) onSave(); };
    saveBtn.setTooltip ("Save the whole rack as a preset");
    favBtn.isLit = [this] { return proc.isCurrentFavourite(); };
    favBtn.onClick = [this] { proc.toggleFavourite(); presetDisplay.repaint(); };
    favBtn.setTooltip ("Favourite (STAFF PICK)");
    aBtn.isLit = [this] { return proc.getSide() == 0; };
    bBtn.isLit = [this] { return proc.getSide() == 1; };
    aBtn.onClick = [this] { proc.setSide (0); };
    bBtn.onClick = [this] { proc.setSide (1); };
    aBtn.setTooltip ("A / B: two complete rack states to compare");
    bBtn.setTooltip ("A / B: two complete rack states to compare");
    undoBtn.isEnabledFn = [this] { return proc.canUndo(); };
    redoBtn.isEnabledFn = [this] { return proc.canRedo(); };
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    undoBtn.setTooltip ("Undo (Ctrl/Cmd + Z)");
    redoBtn.setTooltip ("Redo (Ctrl/Cmd + Y)");

    // ---- tempo ----------------------------------------------------------------------------
    auto setMode = [this] (bool manual)
    {
        if (auto* m = proc.getParam (ParamIDs::tempoMode)) m->setValueNotifyingHost (manual ? 1.0f : 0.0f);
    };
    hostBtn.isLit = [this] { auto* m = proc.getParam (ParamIDs::tempoMode); return m == nullptr || m->getValue() < 0.5f; };
    manualBtn.isLit = [this] { auto* m = proc.getParam (ParamIDs::tempoMode); return m != nullptr && m->getValue() > 0.5f; };
    hostBtn.onClick = [setMode] { setMode (false); };
    manualBtn.onClick = [this, setMode]
    {
        // start the manual tempo from what is playing now
        if (auto* b = proc.getParam (ParamIDs::tempoBpm)) b->setValueNotifyingHost (b->convertTo0to1 ((float) proc.getEffectiveBpm()));
        setMode (true);
    };
    hostBtn.setTooltip ("Follow the DAW tempo");
    manualBtn.setTooltip ("Own tempo (drag / type the BPM or TAP)");
    tapBtn.onPress = [this] (bool down) { if (down) proc.tapTempo(); };
    tapBtn.setTooltip ("Tap the tempo (2 or more taps)");

    // ---- levels ---------------------------------------------------------------------------
    auto bind = [this] (LabeledKnob& k, const String& id, std::function<String (float)> fmt, const String& tip)
    {
        if (auto* rp = proc.getParam (id))
        {
            k.knob.bindTo (*rp);
            k.valueText = [rp, fmt] { return fmt (rp->convertFrom0to1 (rp->getValue())); };
        }
        k.setTooltip (tip);
        addAndMakeVisible (k);
    };
    auto dbFmt = [] (float v) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };
    auto pctFmt = [] (float v) { return String (roundToInt (v * 100.0f)) + " %"; };
    bind (inKnob, ParamIDs::inGain, dbFmt, "Level into the first unit");
    bind (recycleKnob, ParamIDs::recycle, pctFmt, "RECYCLE: feeds the rack output back into its input - repeats, extra drive, "
                                                  "self-oscillation at high settings. Careful with loud units.");
    bind (mixKnob, ParamIDs::blend, pctFmt, "Dry / wet of the whole rack (100 % for sends)");
    bind (outKnob, ParamIDs::outGain, dbFmt, "Level out of the rack");
    recycleKnob.setAccent (Colour (0xffffc861));
    inMeter.getPeak = [this] { return proc.engine.meters.inLevel.load(); };
    outMeter.getPeak = [this] { return proc.engine.meters.outPeak.load(); };
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // ---- KILL -----------------------------------------------------------------------------
    killBtn.solid = true;
    killBtn.onClick = [this] { proc.kill(); };
    killBtn.setTooltip ("KILL: a new, musical, level-matched chain (space bar). Locked units stay.");
    auto rating = [this] (TwButton& b, KillLevel l, const String& tip)
    {
        b.isLit = [this, l] { return proc.killLevel == l; };
        b.onClick = [this, l] { proc.killLevel = l; };
        b.setTooltip (tip);
    };
    rating (pgBtn, KillLevel::PG, "PG: same units, small changes");
    rating (rBtn, KillLevel::R, "R: new chain of 3 - 5 units");
    rating (unratedBtn, KillLevel::Unrated, "UNRATED: 4 - 7 units, hotter settings");
    bootlegBtn.onClick = [this] { proc.bootleg(); };
    bootlegBtn.setTooltip ("BOOTLEG: subtle variation of every setting, same units");

    // ---- macros ---------------------------------------------------------------------------
    for (int m = 0; m < kNumMacros; ++m)
    {
        auto* k = macros.add (new LabeledKnob (MacroEngine::macroName (m), Knob::Style::Macro));
        if (auto* rp = proc.getParam (ParamIDs::macro (m))) k->knob.bindTo (*rp);
        k->knob.getDisplayValue = [this, m] { return proc.engine.meters.macroValue[(size_t) m].load(); };
        k->valueText = [this, m] { return String (roundToInt (proc.engine.meters.macroValue[(size_t) m].load() * 100.0f)); };
        k->knob.onGestureStart = [this, m] { proc.beginUiGesture (m); };
        k->knob.onGestureEnd = [this, m] { proc.endUiGesture (m); };
        static const char* tips[] = { "VILLAIN ARC: darker - filters down, darker EQ and reverb",
                                      "CRASH OUT: dirt - more drive, fewer bits, harder clipping",
                                      "AURA: space - more reverb / delay, longer, wider",
                                      "DRIP: motion - faster, deeper modulation and wobble",
                                      "KNOCK: punch - transients, compression, clipper drive" };
        k->setTooltip (tips[m]);
        addAndMakeVisible (k);
    }

    // ---- global switches ------------------------------------------------------------------
    auto toggle = [this] (TwButton& b, const String& id, const String& tip)
    {
        b.isLit = [this, id] { auto* p = proc.getParam (id); return p != nullptr && p->getValue() > 0.5f; };
        b.onClick = [this, id] { if (auto* p = proc.getParam (id)) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); };
        b.setTooltip (tip);
    };
    toggle (autoGainBtn, ParamIDs::autoTracking, "AUTO GAIN: keeps the output as loud as the input");
    toggle (phoneBtn, ParamIDs::pocketTv, "PHONE CHECK: hear it like a phone speaker (monitoring only - switch off before export!)");
    phoneBtn.blinkWhenLit = true;
    compareBtn.isLit = [this] { auto* p = proc.getParam (ParamIDs::offAir); return p != nullptr && p->getValue() > 0.5f; };
    compareBtn.onClick = nullptr;
    compareBtn.onPress = [this] (bool down)
    {
        auto* p = proc.getParam (ParamIDs::offAir);
        if (p == nullptr) return;
        if (down)
        {
            compareDownAt = Time::getMillisecondCounter();
            compareWasOn = p->getValue() > 0.5f;
            p->setValueNotifyingHost (compareWasOn ? 0.0f : 1.0f);
        }
        else if (! compareWasOn && Time::getMillisecondCounter() - compareDownAt > 400)
        {
            p->setValueNotifyingHost (0.0f);   // held: momentary
        }
    };
    compareBtn.setTooltip ("COMPARE: the original, level-matched (click = toggle, hold = listen while held)");
    gearBtn.isLit = [this] { return isGearShown && isGearShown(); };
    gearBtn.onClick = [this] { if (onToggleGear) onToggleGear(); };
    gearBtn.setTooltip ("Show / hide the gear list");
    settingsBtn.onClick = [this] { if (onSettings) onSettings(); };
    settingsBtn.setTooltip ("Window size, oversampling, preset folder");

    for (auto* b : { &prevBtn, &nextBtn, &browseBtn, &saveBtn, &favBtn, &aBtn, &bBtn, &undoBtn, &redoBtn,
                     &hostBtn, &manualBtn, &tapBtn, &killBtn, &pgBtn, &rBtn, &unratedBtn, &bootlegBtn,
                     &autoGainBtn, &phoneBtn, &compareBtn, &gearBtn, &settingsBtn })
        addAndMakeVisible (b);
    addAndMakeVisible (bpm);
}

void ControlPanel::tick()
{
    for (auto* b : { &browseBtn, &favBtn, &aBtn, &bBtn, &undoBtn, &redoBtn, &hostBtn, &manualBtn,
                     &pgBtn, &rBtn, &unratedBtn, &autoGainBtn, &phoneBtn, &compareBtn, &gearBtn })
        b->tick();
    bpm.tick();
    for (auto* k : { &inKnob, &recycleKnob, &mixKnob, &outKnob }) k->tick();
    for (auto* k : macros) k->tick();
    inMeter.tick();
    outMeter.tick();
    if (messageUntil != 0 && Time::getMillisecondCounter() > messageUntil) { messageUntil = 0; message.clear(); repaint(); }
}

void ControlPanel::refresh() { presetDisplay.refresh(); }

void ControlPanel::showMessage (const String& m)
{
    message = m;
    messageUntil = Time::getMillisecondCounter() + 2600;
    repaint();
}

void ControlPanel::resized()
{
    // header cards
    presetDisplay.setBounds (254, 22, 346, 56);
    {
        int x = 254;
        auto place = [&] (Component& c, int w) { c.setBounds (x, 88, w, 28); x += w + 4; };
        place (prevBtn, 30); place (nextBtn, 30); place (browseBtn, 62); place (saveBtn, 46); place (favBtn, 30);
        x += 4; place (aBtn, 26); place (bBtn, 26); x += 4; place (undoBtn, 40); place (redoBtn, 40);
    }
    hostBtn.setBounds (632, 30, 74, 22);
    manualBtn.setBounds (710, 30, 76, 22);
    bpm.setBounds (632, 56, 154, 40);
    tapBtn.setBounds (632, 100, 154, 24);

    inMeter.setBounds (816, 26, 8, 96);
    const int kw = 66;
    int x = 828;
    for (auto* k : { &inKnob, &recycleKnob, &mixKnob, &outKnob }) { k->setBounds (x, 18, kw, 104); x += kw; }
    outMeter.setBounds (x + 2, 26, 8, 96);

    killBtn.setBounds (1130, 20, 130, 50);
    pgBtn.setBounds (1130, 76, 34, 22);
    rBtn.setBounds (1166, 76, 30, 22);
    unratedBtn.setBounds (1198, 76, 62, 22);
    bootlegBtn.setBounds (1130, 102, 130, 22);

    // macro strip
    const int sy = headerHeight + 8;
    x = 104;
    for (auto* k : macros) { k->setBounds (x, sy + 4, 92, stripHeight - 8); x += 96; }
    int bx = 1268 - 4;
    auto right = [&] (Component& c, int w) { bx -= w; c.setBounds (bx, sy + 20, w, 34); bx -= 6; };
    right (settingsBtn, 120); right (gearBtn, 64); right (compareBtn, 92); right (phoneBtn, 104); right (autoGainBtn, 96);
}

void ControlPanel::paint (Graphics& g)
{
    // header cards
    drawCard (g, { 10.0f, 10.0f, 224.0f, 120.0f });
    brand::paintLogoBlock (g, { 22.0f, 16.0f, 200.0f, 110.0f }, true);

    drawCard (g, { 242.0f, 10.0f, 370.0f, 120.0f });
    drawCard (g, { 620.0f, 10.0f, 178.0f, 120.0f });
    drawSmallLabel (g, "TEMPO", { 632.0f, 13.0f, 154.0f, 14.0f }, Justification::centredLeft, theme::accent, 9.5f);
    drawCard (g, { 806.0f, 10.0f, 306.0f, 120.0f });
    drawCard (g, { 1120.0f, 10.0f, 150.0f, 120.0f });

    // macro strip
    const float sy = (float) headerHeight + 8.0f;
    drawCard (g, { 10.0f, sy, 1260.0f, (float) stripHeight });
    g.setFont (theme::stencilFont (16.0f));
    g.setColour (theme::silver);
    g.drawText ("MACROS", Rectangle<float> (22.0f, sy + 14.0f, 80.0f, 22.0f), Justification::centredLeft, false);
    drawSmallLabel (g, "TURN ONE KNOB,", { 22.0f, sy + 36.0f, 80.0f, 12.0f }, Justification::centredLeft, theme::silverDim, 8.5f);
    drawSmallLabel (g, "MOVE THE RACK", { 22.0f, sy + 48.0f, 80.0f, 12.0f }, Justification::centredLeft, theme::silverDim, 8.5f);

    if (message.isNotEmpty())
        theme::drawGlowText (g, message, { 590.0f, sy + 8.0f, 160.0f, (float) stripHeight - 16.0f }, theme::monoLabel (12.0f).boldened(),
                             theme::accent, Justification::centred, 0.6f);
}

} // namespace ek::ui
