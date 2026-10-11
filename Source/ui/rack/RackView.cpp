#include "RackView.h"
#include "GearList.h"

namespace ek::ui
{
using namespace juce;

// ============================================================================
namespace dnd
{
String fx (EffectType t) { return "fx:" + String ((int) t); }
String unit (int slot) { return "unit:" + String (slot); }

bool parseFx (const var& d, EffectType& t)
{
    const auto s = d.toString();
    if (! s.startsWith ("fx:")) return false;
    const int v = s.fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (v <= 0 || v > kNumEffectTypes) return false;
    t = (EffectType) v;
    return true;
}

bool parseUnit (const var& d, int& slot)
{
    const auto s = d.toString();
    if (! s.startsWith ("unit:")) return false;
    slot = s.fromFirstOccurrenceOf (":", false, false).getIntValue();
    return slot >= 0 && slot < kNumSlots;
}
} // namespace dnd

namespace
{
const Colour shaft { 0xff07090d };
const Colour soloYellow { 0xffffd23f };
const Colour powerGreen { 0xff3dff6e };
const Colour replaceYellow { 0xffffc861 };

float easeOut (float t) { t = jlimit (0.0f, 1.0f, t); return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

void drawRail (Graphics& g, Rectangle<float> r)
{
    ColourGradient metal (Colour (0xff23262c), r.getX(), 0.0f, Colour (0xff121418), r.getRight(), 0.0f, false);
    metal.addColour (0.35, Colour (0xff2e3138));
    g.setGradientFill (metal);
    g.fillRect (r);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawVerticalLine ((int) r.getRight() - 1, r.getY(), r.getBottom());
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawVerticalLine ((int) r.getX(), r.getY(), r.getBottom());
    // mounting holes (rack units are 3 holes per "U")
    const float pitch = 15.875f;
    const float first = std::ceil (r.getY() / pitch) * pitch;
    for (float y = first; y < r.getBottom(); y += pitch)
    {
        auto hole = Rectangle<float> (r.getWidth() * 0.42f, 6.0f).withCentre ({ r.getCentreX(), y });
        g.setColour (Colour (0xff030405));
        g.fillRoundedRectangle (hole, 1.5f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) hole.getBottom(), hole.getX() + 1.0f, hole.getRight() - 1.0f);
    }
}
} // namespace

// ============================================================================
UnitComponent::UnitComponent (EffectorKillaAudioProcessor& p, RackView& o, int s)
    : proc (p), owner (o), slot (s), type (p.getSlotType (s)), pal (faceplate::palette (p.getSlotType (s))),
      menuBtn (SideButton::Icon::Menu, theme::accent),
      powerBtn (SideButton::Icon::Power, powerGreen),
      soloBtn (SideButton::Icon::Solo, soloYellow),
      lockBtn (SideButton::Icon::Lock, theme::killRed)
{
    setOpaque (true);
    const auto& info = effectInfo (type);
    auto addControl = [this, &info] (int param)
    {
        const ParamSpec* spec = param < kNumParams ? &info.params[(size_t) param] : nullptr;
        const auto id = param < kNumParams ? ParamIDs::slotParam (slot, param) : ParamIDs::slotMix (slot);
        auto* c = controls.add (new RackControl (slot, param, spec, proc.getParam (id), pal));
        c->onGestureStart = [this] { proc.pushUndo(); };
        c->onRightClick = [this, param] (Point<int> screen)
        {
            if (! owner.onKnobRightClick) return false;
            owner.onKnobRightClick (slot, param, screen);
            return true;
        };
        c->paintOverlay = [this, param] (Graphics& g, Rectangle<float> face)
        {
            if (owner.paintKnobOverlay) owner.paintKnobOverlay (g, face, slot, param);
        };
        if (param < kNumParams)
        {
            c->getEffectiveValue = [this, param]
            {
                const float e = proc.engine.effectiveParams[(size_t) slot][(size_t) param].load (std::memory_order_relaxed);
                const float b = proc.getSlotParam (slot, param);
                return (e == 0.0f && b != 0.0f) ? b : e;   // engine not running yet: show the base value
            };
            c->setTooltip (info.name + ": " + spec->name + "   (drag, Shift = fine, double-click = default, right-click = more)");
        }
        else
        {
            c->setTooltip ("Unit dry / wet   (drag, double-click = 100 %)");
        }
        addAndMakeVisible (c);
    };
    for (int i = 0; i < kNumParams; ++i)
        if (info.params[(size_t) i].used) addControl (i);
    addControl (kNumParams);

    menuBtn.onClick = [this] { showUnitMenu (&menuBtn); };
    menuBtn.setTooltip ("Unit menu: programmes, my unit presets, save, copy / paste, replace, reset, listen up to here, remove");

    powerBtn.isLit = [this] { return proc.getSlotParam (slot, 9) < 0.5f; };
    powerBtn.onClick = [this]
    {
        proc.pushUndo();
        proc.setSlotParam (slot, 9, proc.getSlotParam (slot, 9) > 0.5f ? 0.0f : 1.0f);
    };
    powerBtn.setTooltip ("POWER - unit on / off. Off keeps running in the background, so echoes and reverb tails continue.\n"
                         "Press and hold: LEVEL-MATCHED POWER - hear the rack without this unit at the same loudness.");
    // LEVEL-MATCHED POWER: hold > 250 ms = audition the level-matched bypass while held
    powerBtn.holdMs = 250;
    powerBtn.onHoldStart = [this] { proc.setLevelMatchedAudition (slot); repaint(); };
    powerBtn.onHoldEnd = [this]
    {
        if (proc.getLevelMatchedAudition() == slot) proc.setLevelMatchedAudition (-1);
        repaint();
    };

    soloBtn.isLit = [this] { return proc.getSolo() == slot; };
    soloBtn.blinkWhenLit = true;
    soloBtn.onClick = [this] { proc.setSolo (proc.getSolo() == slot ? -1 : slot); };
    soloBtn.setTooltip ("SOLO - hear only this unit (the others are bypassed)");

    lockBtn.isLit = [this] { return proc.getSlotMeta (slot).writeProtect; };
    lockBtn.onClick = [this] { proc.setWriteProtect (slot, ! proc.getSlotMeta (slot).writeProtect); };
    lockBtn.setTooltip ("LOCK - KILL and preset changes keep this unit; it cannot be removed");

    for (auto* b : { &menuBtn, &powerBtn, &soloBtn, &lockBtn })
        addAndMakeVisible (b);

    lcd.onClick = [this] { showPresetMenu (&lcd); };
    lcd.setTooltip ("Unit programmes and your saved unit presets");
    addAndMakeVisible (lcd);

    msBtn.pal = pal;
    msBtn.getMode = [this] { return (int) proc.getSlotMeta (slot).ms; };
    msBtn.onClick = [this]
    {
        const auto m = proc.getSlotMeta (slot).ms;
        proc.setSlotMs (slot, m == MSMode::Stereo ? MSMode::Mid : (m == MSMode::Mid ? MSMode::Side : MSMode::Stereo));
    };
    msBtn.setTooltip ("Process the whole stereo signal (ST), only the MID or only the SIDE");
    addAndMakeVisible (msBtn);

    setSize (width, height);
    tick (owner.clock());
}

UnitComponent::~UnitComponent() = default;

Rectangle<float> UnitComponent::plateBounds() const
{
    return { 0.0f, 0.0f, (float) (getWidth() - stripWidth), (float) getHeight() };
}

String UnitComponent::currentProgrammeName() const
{
    const int t = proc.findTape (slot);
    if (t < 0) return "CUSTOM";
    return proc.bank.getTapes (type)[(size_t) t].name;
}

void UnitComponent::setReduceMotion (bool r)
{
    reduceMotion = r;
    lcd.setReduceMotion (r);
}

void UnitComponent::setFlash (float amount)
{
    amount = jlimit (0.0f, 1.0f, amount);
    if (std::abs (amount - flash) < 0.01f) return;
    flash = amount;
    repaint();
}

void UnitComponent::setLifted (bool l)
{
    if (l == lifted) return;
    lifted = l;
    setAlpha (l ? 0.0f : 1.0f);
}

Image UnitComponent::createLiftImage (int m)
{
    auto snap = createComponentSnapshot (getLocalBounds(), true, 1.0f);
    Image img (Image::ARGB, getWidth() + 2 * m, getHeight() + 2 * m, true);
    Graphics g (img);
    DropShadow (Colours::black.withAlpha (0.75f), m, { 0, m / 2 }).drawForRectangle (g, getLocalBounds().translated (m, m));
    g.setOpacity (0.94f);
    g.drawImageAt (snap, m, m);
    g.setOpacity (1.0f);
    g.setColour (theme::accent.withAlpha (0.55f));
    g.drawRect (Rectangle<int> (m, m, getWidth(), getHeight()), 1);
    return img;
}

void UnitComponent::fillState (double timeSec)
{
    auto& st = state;
    for (int p = 0; p < kNumParams; ++p)
    {
        const float e = proc.engine.effectiveParams[(size_t) slot][(size_t) p].load (std::memory_order_relaxed);
        const float b = proc.getSlotParam (slot, p);
        st.params[(size_t) p] = (e == 0.0f && b != 0.0f) ? b : e;
    }
    st.mix = proc.getSlotParam (slot, 8);
    st.powered = proc.getSlotParam (slot, 9) < 0.5f;
    st.soloDimmed = owner.isDimmed (slot);
    st.locked = proc.getSlotMeta (slot).writeProtect;

    auto& taps = proc.engine.getRack().getTaps();
    const float fall = reduceMotion ? 0.6f : 0.84f;
    st.inLevel = jmax (taps.takeInPeak (slot), st.inLevel * fall);
    st.outLevel = jmax (taps.takeOutPeak (slot), st.outLevel * fall);
    st.gainReductionDb = jmax (taps.takeGainReduction (slot), st.gainReductionDb * 0.82f);
    for (int b = 0; b < 3; ++b)
        st.bands[(size_t) b] = jmax (taps.takeActivity (slot, b), st.bands[(size_t) b] * 0.82f);
    if (type == EffectType::Slap && st.bands[0] > 0.5f && hitHold <= 0) hitHold = 4;
    st.transientHit = hitHold > 0;
    if (hitHold > 0) --hitHold;

    if (type == EffectType::WideBody)
    {
        taps.copyScope (slot, scopeL.data(), scopeR.data());
        st.scopeL = scopeL.data();
        st.scopeR = scopeR.data();
        st.scopeN = UnitTaps::scopeSize;
    }
    st.beats = proc.engine.meters.ppq.load (std::memory_order_relaxed);
    st.bpm = proc.getEffectiveBpm();
    st.timeSec = timeSec;
    st.reduceMotion = reduceMotion;
}

void UnitComponent::tick (double timeSec)
{
    fillState (timeSec);

    for (auto* c : controls) c->tick();
    for (auto* b : { &menuBtn, &powerBtn, &soloBtn, &lockBtn }) b->tick();
    msBtn.tick();
    lcd.tick();
    const bool audition = proc.getLevelMatchedAudition() == slot;
    if (audition || audition != lastAudition) repaint (plateBounds().removeFromTop (30.0f).getSmallestIntegerContainer());
    lastAudition = audition;
    lcd.setText (currentProgrammeName(), ! firstTick);

    const bool dim = ! state.powered || state.soloDimmed;
    if (state.powered != lastPowered || state.soloDimmed != lastDimmed || state.locked != lastLocked || firstTick)
    {
        lastPowered = state.powered;
        lastDimmed = state.soloDimmed;
        lastLocked = state.locked;
        for (auto* c : controls) c->setAlpha (dim ? 0.5f : 1.0f);
        lcd.setAlpha (dim ? 0.6f : 1.0f);
        msBtn.setAlpha (dim ? 0.6f : 1.0f);
        repaint();
    }
    else if (! reduceMotion || state.inLevel > 0.001f)
    {
        repaint (faceplate::liveArea (type, plateBounds()).expanded (3.0f).getSmallestIntegerContainer());
    }
    firstTick = false;
}

void UnitComponent::layoutControls()
{
    const auto area = faceplate::knobArea (type, plateBounds());
    const int n = controls.size();
    if (n == 0) return;
    constexpr float mixGap = 16.0f;
    float total = 0.0f;
    std::vector<float> w;
    const auto& info = effectInfo (type);
    for (auto* c : controls)
    {
        const ParamSpec* sp = c->param < kNumParams ? &info.params[(size_t) c->param] : nullptr;
        w.push_back ((float) RackControl::preferredWidth (c->kind, sp));
        total += w.back();
    }
    const float avail = area.getWidth() - mixGap;
    const float scale = total > avail ? avail / total : 1.0f;
    const float spacing = total < avail ? jmin (10.0f, (avail - total) / (float) jmax (1, n - 1)) : 0.0f;
    float x = area.getX();
    for (int i = 0; i < n - 1; ++i)
    {
        const float cw = w[(size_t) i] * scale;
        controls[i]->setBounds (Rectangle<float> (x, area.getY(), cw, area.getHeight()).toNearestInt());
        x += cw + spacing;
    }
    // DRY/WET sits apart at the right end of the knob area
    const float mw = w.back() * scale;
    controls[n - 1]->setBounds (Rectangle<float> (area.getRight() - mw, area.getY(), mw, area.getHeight()).toNearestInt());
}

void UnitComponent::resized()
{
    const auto plate = plateBounds();
    const auto lcdArea = faceplate::presetLcdArea (plate);
    lcd.setBounds (lcdArea.toNearestInt());
    msBtn.setBounds (Rectangle<float> (lcdArea.getX() - 54.0f, lcdArea.getY() + 1.0f, 46.0f, lcdArea.getHeight() - 2.0f).toNearestInt());

    const int sx = (int) plate.getRight() + (stripWidth - 26) / 2;
    const int top = (getHeight() - (4 * 26 + 3 * 7)) / 2;
    int y = top;
    for (auto* b : { &menuBtn, &powerBtn, &soloBtn, &lockBtn })
    {
        b->setBounds (sx, y, 26, 26);
        y += 26 + 7;
    }
    layoutControls();
}

RackControl* UnitComponent::controlAt (Point<int> local) const
{
    for (auto* c : controls)
        if (c->getBounds().contains (local)) return c;
    return nullptr;
}

RackControl* UnitComponent::findControl (int param) const
{
    for (auto* c : controls)
        if (c->param == param) return c;
    return nullptr;
}

void UnitComponent::repaintControls()
{
    for (auto* c : controls) c->repaint();
}

void UnitComponent::paint (Graphics& g)
{
    g.fillAll (shaft);
    const auto plate = plateBounds();
    faceplate::paintFaceplate (g, type, plate, state, false);
    owner.paintSpectrum (g, slot, faceplate::knobArea (type, plate)); // SPECTRAL VIEW (hovered unit only)

    // engraved divider in front of DRY/WET
    if (controls.size() > 1)
    {
        const auto mb = controls.getLast()->getBounds().toFloat();
        const float x = mb.getX() - 8.0f;
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillRect (x, mb.getY() + 6.0f, 1.0f, mb.getHeight() - 12.0f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillRect (x + 1.0f, mb.getY() + 6.0f, 1.0f, mb.getHeight() - 12.0f);
    }

    // side strip: dark anodised metal column with the four buttons
    auto strip = Rectangle<float> (plate.getRight(), 0.0f, (float) stripWidth, (float) getHeight());
    ColourGradient metal (Colour (0xff1d2025), strip.getX(), 0.0f, Colour (0xff101114), strip.getRight(), 0.0f, false);
    g.setGradientFill (metal);
    g.fillRect (strip.reduced (0.0f, 1.0f));
    g.setColour (Colours::black);
    g.drawVerticalLine ((int) strip.getX(), 0.0f, (float) getHeight());
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawVerticalLine ((int) strip.getX() + 1, 1.0f, (float) getHeight() - 1.0f);
    g.drawHorizontalLine (1, strip.getX() + 1.0f, strip.getRight());
}

void UnitComponent::paintOverChildren (Graphics& g)
{
    const auto plate = plateBounds();
    if (! state.powered || state.soloDimmed)
    {
        const String msg = ! state.powered ? "POWER OFF  -  TAILS KEEP RUNNING" : "MUTED BY SOLO";
        g.setFont (theme::monoLabel (9.5f).boldened());
        // a status strip over the plate's header row (between the name block and the ST switch), so
        // whatever small print the faceplate has there is covered completely
        const auto knobs = faceplate::knobArea (type, plate);
        const float x0 = knobs.getX();
        const float x1 = jmax (x0 + 120.0f, (float) msBtn.getX() - 5.0f);
        auto pill = Rectangle<float> (x0, (float) msBtn.getY(), x1 - x0, (float) msBtn.getHeight());
        g.setColour (Colour (0xe0050608));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour ((! state.powered ? replaceYellow : soloYellow).withAlpha (0.5f));
        g.drawRoundedRectangle (pill, 9.0f, 1.0f);
        theme::drawLed (g, { pill.getX() + 9.0f, pill.getCentreY() }, 2.4f, ! state.powered ? theme::killRed : soloYellow, false, 0.6f);
        g.setColour (! state.powered ? Colour (0xffffd98f) : soloYellow);
        g.drawText (msg, pill.withTrimmedLeft (16.0f), Justification::centredLeft, false);
    }
    // SPLIT branch tag
    if (const int br = proc.getSlotBranch (slot); br >= 0)
    {
        auto tag = Rectangle<float> (4.0f, 4.0f, 20.0f, 16.0f);
        g.setColour (Colour (0xe0050608));
        g.fillRoundedRectangle (tag, 4.0f);
        g.setColour (theme::accent);
        g.drawRoundedRectangle (tag, 4.0f, 1.0f);
        g.setFont (theme::monoLabel (10.0f).boldened());
        g.drawText (br == 0 ? "A" : "B", tag, Justification::centred, false);
    }
    // LEVEL-MATCHED POWER readout while the POWER button is held
    if (proc.getLevelMatchedAudition() == slot)
    {
        auto pill = Rectangle<float> (plate.getX() + 30.0f, 4.0f, 250.0f, 18.0f);
        g.setColour (Colour (0xf0050608));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (theme::accent);
        g.drawRoundedRectangle (pill, 9.0f, 1.0f);
        g.setFont (theme::monoLabel (9.5f).boldened());
        g.drawText ("LEVEL-MATCHED BYPASS  " + String (proc.getAuditionGainDb(), 1) + " dB", pill, Justification::centred, false);
    }
    if (flash > 0.0f)
    {
        g.setColour (theme::accent.withAlpha (0.55f * flash));
        g.fillRect (plate);
        g.setColour (Colours::white.withAlpha (0.5f * flash));
        g.drawRect (plate, 2.0f);
    }
}

void UnitComponent::mouseDown (const MouseEvent& e)
{
    dragStarted = false;
    if (e.mods.isPopupMenu()) showUnitMenu (nullptr);
}

void UnitComponent::mouseDrag (const MouseEvent& e)
{
    if (dragStarted || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 5) return;
    if (! plateBounds().contains (e.getMouseDownPosition().toFloat())) return;
    dragStarted = true;
    owner.beginUnitDrag (*this, e);
}

void UnitComponent::mouseUp (const MouseEvent&) { dragStarted = false; }

// ----------------------------------------------------------------------------
void UnitComponent::showPresetMenu (Component* target)
{
    showUnitMenu (target); // the LCD opens the same menu, programmes first
}

void UnitComponent::showUnitMenu (Component* target)
{
    const bool locked = proc.getSlotMeta (slot).writeProtect;
    const auto& tapes = proc.bank.getTapes (type);
    const int current = proc.findTape (slot);

    PopupMenu m;
    m.addSectionHeader ("S" + String (slot + 1) + "  " + effectInfo (type).name.toUpperCase());

    PopupMenu programmes;
    for (int i = 0; i < (int) tapes.size(); ++i)
        programmes.addItem (1000 + i, tapes[(size_t) i].name + "   - " + tapes[(size_t) i].description, ! locked, i == current);
    m.addSubMenu ("Programmes", programmes, ! tapes.empty());

    PopupMenu mine;
    std::vector<File> matching;
    for (auto& f : proc.bank.getSlotPresetFiles())
        if (auto s = proc.bank.loadSlotPreset (f); s.has_value() && s->type == type)
        {
            mine.addItem (3000 + (int) matching.size(), f.getFileNameWithoutExtension(), ! locked);
            matching.push_back (f);
        }
    m.addSubMenu ("My unit presets", mine, ! matching.empty());
    m.addItem (900, "Save unit preset...");
    m.addSeparator();
    m.addItem (901, "Copy unit");
    m.addItem (902, "Paste unit", proc.hasSlotClipboard() && ! locked);
    PopupMenu replace;
    for (const auto& grp : gear::groups())
    {
        replace.addSectionHeader (grp.name);
        for (auto t : grp.types) replace.addItem (100 + (int) t, effectInfo (t).name, ! locked, t == type);
    }
    m.addSubMenu ("Replace with effect", replace, ! locked);
    m.addItem (1, "Reset to default", ! locked);
    m.addSeparator();
    m.addItem (2, "Listen up to here (ignore the units below)", true, proc.getScreening() == slot);
    m.addItem (3, "Solo (hear only this unit)", true, proc.getSolo() == slot);
    m.addItem (6, "Lock", true, locked);
    m.addSeparator();
    // SPLIT / MERGE
    if (! proc.hasSplit())
    {
        m.addItem (800, "Insert SPLIT in front of this unit");
        m.addItem (801, "Insert SPLIT after this unit");
    }
    else
    {
        const int br = proc.getSlotBranch (slot);
        m.addItem (802, "Move to branch A", br != 0);
        m.addItem (803, "Move to branch B", br != 1);
        m.addItem (804, "Move SPLIT in front of this unit", proc.getSplitPosition() != slot);
        m.addItem (805, "Remove SPLIT");
    }
    m.addSeparator();
    m.addItem (7, "Remove", ! locked);

    auto& p = proc;
    const int s = slot;
    const auto t = type;
    auto opts = PopupMenu::Options();
    opts = target != nullptr ? opts.withTargetComponent (target) : opts.withTargetComponent (this).withMousePosition();
    m.showMenuAsync (opts, [&p, s, t, matching] (int r)
    {
        if (r <= 0) return;
        if (r >= 3000) { p.loadSlotPreset (s, matching[(size_t) (r - 3000)]); return; }
        if (r >= 1000) { p.playTape (s, t, r - 1000); return; }
        if (r >= 100 && r < 900) { const auto nt = (EffectType) (r - 100); if (nt != t) p.playTape (s, nt, 0); return; }
        switch (r)
        {
            case 900:
            {
                auto* w = new AlertWindow ("SAVE UNIT PRESET", effectInfo (t).name + " preset name:", MessageBoxIconType::NoIcon);
                w->addTextEditor ("name", "My " + effectInfo (t).name);
                w->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
                w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
                w->enterModalState (true, ModalCallbackFunction::create ([&p, s, w] (int res)
                {
                    const auto name = w->getTextEditorContents ("name").trim();
                    if (res == 1 && name.isNotEmpty()) p.saveSlotPreset (s, name);
                }), true);
                break;
            }
            case 901: p.copySlot (s); break;
            case 902: p.pasteSlot (s); break;
            case 1: p.pushUndo(); p.setSlotType (s, t); break;
            case 2: p.setScreening (p.getScreening() == s ? -1 : s); break;
            case 3: p.setSolo (p.getSolo() == s ? -1 : s); break;
            case 6: p.setWriteProtect (s, ! p.getSlotMeta (s).writeProtect); break;
            case 7: p.removeEffect (s); break;
            case 800: p.insertSplit (s); break;
            case 801: p.insertSplit (s + 1); break;
            case 802: p.setSlotBranch (s, 0); break;
            case 803: p.setSlotBranch (s, 1); break;
            case 804: p.insertSplit (s); break;
            case 805: p.removeSplit(); break;
            default: break;
        }
    });
}

// ============================================================================
RackView::RackView (EffectorKillaAudioProcessor& p) : proc (p)
{
    lastKillEvents = proc.killEvents.load();
    splitUnit = std::make_unique<SplitUnit> (proc);
    splitUnit->onRemoved = [this] { refresh(); };
    addChildComponent (*splitUnit);
    spectrum = std::make_unique<SpectrumOverlay> (proc.getSlotTap());
    refresh();
}

RackView::~RackView()
{
    spectrum.reset(); // switches the analyzer tap off
}

void RackView::setHoveredSlot (int slot)
{
    if (slot == hoverSlot) return;
    const int old = hoverSlot;
    hoverSlot = slot;
    if (spectrum != nullptr) spectrum->setSlot (slot);
    for (int s : { old, slot })
        if (auto* u = unitForSlot (s)) u->repaint();
}

void RackView::paintSpectrum (Graphics& g, int slot, Rectangle<float> area) const
{
    if (spectrum != nullptr && slot >= 0 && spectrum->getSlot() == slot)
        spectrum->paint (g, area);
}

double RackView::clock() const noexcept { return Time::getMillisecondCounterHiRes() * 0.001; }

String RackView::signature() const
{
    String s;
    for (int i = 0; i < kNumSlots; ++i) s << (int) proc.getSlotType (i) << ",";
    // SPLIT layout (position + branch of every unit)
    s << "|" << proc.getSplitPosition() << ":";
    for (int i = 0; i < kNumSlots; ++i) s << proc.getSlotBranch (i) << ",";
    return s;
}

int RackView::lastOccupied() const
{
    for (int i = kNumSlots - 1; i >= 0; --i)
        if (proc.getSlotType (i) != EffectType::None) return i;
    return -1;
}

bool RackView::isFull() const
{
    for (int i = 0; i < kNumSlots; ++i)
        if (proc.getSlotType (i) == EffectType::None) return false;
    return true;
}

bool RackView::isDimmed (int slot) const
{
    const int solo = proc.getSolo();
    const int screen = proc.getScreening();
    return (solo >= 0 && solo != slot) || (screen >= 0 && slot > screen);
}

void RackView::setReduceMotion (bool r)
{
    reduceMotion = r;
    for (auto* u : units) u->setReduceMotion (r);
}

int RackView::unitWidth() const { return jlimit (600, UnitComponent::width, getWidth() > 0 ? getWidth() : UnitComponent::width); }
int RackView::unitX() const { return jmax (0, (getWidth() - unitWidth()) / 2); }

UnitComponent* RackView::unitForSlot (int slot) const
{
    for (auto* u : units)
        if (u->getSlot() == slot) return u;
    return nullptr;
}

void RackView::refresh (bool repaintAll)
{
    const auto sig = signature();
    if (sig != builtFor)
    {
        builtFor = sig;
        draggingSlot = -1;
        drop = {};
        setHoveredSlot (-1);
        units.clear();
        splitUnit->setVisible (proc.hasSplit());
        splitUnit->refresh();
        for (int i = 0; i < kNumSlots; ++i)
            if (proc.getSlotType (i) != EffectType::None)
            {
                auto* u = units.add (new UnitComponent (proc, *this, i));
                u->setReduceMotion (reduceMotion);
                addAndMakeVisible (u);
            }
        animY.assign ((size_t) units.size(), 0.0f);
        targetY.assign ((size_t) units.size(), 0.0f);
        targetX.assign ((size_t) units.size(), 0);
        targetW.assign ((size_t) units.size(), UnitComponent::width);
        if (getWidth() > 0) setSize (getWidth(), getIdealHeight());
        resized();
        layoutTargets (true);
        tick();   // KILL: start the slide-in before the first paint
        repaint();
    }
    else if (repaintAll)
    {
        if (splitUnit->isVisible()) splitUnit->refresh();
        repaint();
    }
}

int RackView::contentBottom() const
{
    const int rowH = UnitComponent::height + gapY;
    if (! proc.hasSplit()) return topMargin + units.size() * rowH;
    int serial = 0, a = 0, b = 0;
    for (auto* u : units)
    {
        const int br = proc.getSlotBranch (u->getSlot());
        ++(br < 0 ? serial : (br == 0 ? a : b));
    }
    return topMargin + serial * rowH + SplitUnit::height + gapY + jmax (a, b, 1) * rowH;
}

int RackView::branchColumnAt (Point<int> pos) const
{
    return pos.x < unitX() + unitWidth() / 2 ? 0 : 1;
}

int RackView::getIdealHeight() const
{
    return contentBottom() + (isFull() ? 64 : addZoneHeight) + 8;
}

Rectangle<int> RackView::addZone() const
{
    const int y = contentBottom();
    return { unitX(), y, unitWidth(), jmax (isFull() ? 56 : 90, jmin (isFull() ? 56 : addZoneHeight, getHeight() - y - 6)) };
}

void RackView::resized()
{
    layoutTargets (true);
}

void RackView::layoutTargets (bool snap)
{
    targetX.assign ((size_t) units.size(), unitX());
    targetW.assign ((size_t) units.size(), unitWidth());
    if (targetY.size() != (size_t) units.size()) { targetY.assign ((size_t) units.size(), 0.0f); animY.assign ((size_t) units.size(), 0.0f); }

    if (proc.hasSplit())
    {
        // serial units, then the SPLIT strip, then branch A (left) | branch B (right)
        const int rowH = UnitComponent::height + gapY;
        int y = topMargin;
        for (int i = 0; i < units.size(); ++i)
            if (proc.getSlotBranch (units[i]->getSlot()) < 0) { targetY[(size_t) i] = (float) y; y += rowH; }
        splitY = y;
        splitUnit->setBounds (unitX(), y, unitWidth(), SplitUnit::height);
        y += SplitUnit::height + gapY;
        const int colW = (unitWidth() - gapY) / 2;
        int yA = y, yB = y;
        for (int i = 0; i < units.size(); ++i)
        {
            const int br = proc.getSlotBranch (units[i]->getSlot());
            if (br < 0) continue;
            targetW[(size_t) i] = colW;
            targetX[(size_t) i] = br == 0 ? unitX() : unitX() + colW + gapY;
            targetY[(size_t) i] = (float) (br == 0 ? yA : yB);
            (br == 0 ? yA : yB) += rowH;
        }
        for (int i = 0; i < units.size(); ++i)
        {
            if (snap || reduceMotion) animY[(size_t) i] = targetY[(size_t) i];
            units[i]->setBounds (targetX[(size_t) i], roundToInt (animY[(size_t) i]), targetW[(size_t) i], UnitComponent::height);
        }
        return;
    }
    splitY = -1;

    int gapIndex = drop.kind == DropTarget::Move ? drop.gap : -1;
    if (draggingSlot >= 0 && gapIndex < 0)
    {
        // no target yet: keep the gap where the unit was
        int k = 0;
        for (auto* u : units) { if (u->getSlot() == draggingSlot) { gapIndex = k; break; } ++k; }
    }
    int j = 0;
    for (int i = 0; i < units.size(); ++i)
    {
        auto* u = units[i];
        if (u->getSlot() == draggingSlot) continue;
        const int pos = (gapIndex >= 0 && j >= gapIndex) ? j + 1 : j;
        targetY[(size_t) i] = (float) (topMargin + pos * (UnitComponent::height + gapY));
        ++j;
    }
    for (int i = 0; i < units.size(); ++i)
    {
        if (snap || reduceMotion) animY[(size_t) i] = targetY[(size_t) i];
        units[i]->setBounds (unitX(), roundToInt (animY[(size_t) i]), unitWidth(), UnitComponent::height);
    }
}

void RackView::tick()
{
    const double now = clock();

    // KILL: units flash ice blue and slide into the rack
    const int ke = proc.killEvents.load();
    if (ke != lastKillEvents)
    {
        lastKillEvents = ke;
        killAnimStart = reduceMotion ? -1.0 : now;
    }
    for (int i = 0; i < units.size(); ++i)
    {
        auto* u = units[i];
        if (killAnimStart >= 0.0)
        {
            const float t = (float) (now - killAnimStart);
            const float local = (t - 0.025f * (float) i) / 0.17f;
            const float e = easeOut (local);
            u->setTransform (AffineTransform::translation ((1.0f - e) * 70.0f, 0.0f));
            if (! u->isLifted()) u->setAlpha (0.35f + 0.65f * e);
            u->setFlash (1.0f - jlimit (0.0f, 1.0f, t / 0.24f));
        }
        else if (! u->getTransform().isIdentity())
        {
            u->setTransform ({});
            if (! u->isLifted()) u->setAlpha (1.0f);
            u->setFlash (0.0f);
        }
    }
    if (killAnimStart >= 0.0 && now - killAnimStart > 0.025 * units.size() + 0.3)
        killAnimStart = -1.0;

    // unit drag finished somewhere else (dropped outside, Escape)
    if (draggingSlot >= 0)
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this); c == nullptr || ! c->isDragAndDropActive())
            endUnitDrag();

    // smooth movement towards the layout targets
    for (int i = 0; i < units.size(); ++i)
    {
        float& y = animY[(size_t) i];
        const float tgt = targetY[(size_t) i];
        if (std::abs (tgt - y) > 0.5f && ! reduceMotion) y += (tgt - y) * 0.38f;
        else y = tgt;
        const int iy = roundToInt (y);
        const int ix = i < (int) targetX.size() ? targetX[(size_t) i] : unitX();
        if (units[i]->getY() != iy || units[i]->getX() != ix) units[i]->setTopLeftPosition (ix, iy);
    }

    // SPECTRAL VIEW: follow the hovered unit (polled, so the knobs on top do not interrupt it)
    if (draggingSlot < 0 && isShowing() && isMouseOver (true))
    {
        int hovered = -1;
        const auto mp = getMouseXYRelative();
        for (auto* u : units)
            if (! u->isLifted() && u->getBounds().contains (mp)) { hovered = u->getSlot(); break; }
        setHoveredSlot (hovered);
        hoverFromMouse = true;
    }
    else if (hoverFromMouse)
    {
        setHoveredSlot (-1);
        hoverFromMouse = false;
    }
    if (spectrum != nullptr && spectrum->update() && hoverSlot >= 0)
        if (auto* u = unitForSlot (hoverSlot))
            u->repaint (faceplate::knobArea (u->getType(), u->plateBounds()).getSmallestIntegerContainer());

    if (splitUnit->isVisible()) splitUnit->tick();
    for (auto* u : units) u->tick (now);
}

// ----------------------------------------------------------------------------
RackView::KnobHit RackView::knobAt (Point<int> screenPos) const
{
    if (! isShowing()) return {};
    return knobAtLocal (getLocalPoint (nullptr, screenPos));
}

RackView::KnobHit RackView::knobAtLocal (Point<int> pos) const
{
    KnobHit h;
    for (auto* u : units)
    {
        if (u->isLifted() || ! u->getBounds().contains (pos)) continue;
        if (auto* c = u->controlAt (pos - u->getPosition()))
        {
            h.slot = c->slot;
            h.param = c->param;
            h.bounds = c->getBounds() + u->getPosition();
        }
        break;
    }
    return h;
}

Rectangle<int> RackView::getKnobBounds (int slot, int param) const
{
    if (auto* u = unitForSlot (slot))
        if (auto* c = u->findControl (param))
            return c->getBounds() + u->getPosition();
    return {};
}

void RackView::repaintKnobs()
{
    for (auto* u : units) u->repaintControls();
}

// ----------------------------------------------------------------------------
void RackView::paint (Graphics& g)
{
    g.fillAll (shaft);
    const float ux = (float) unitX(), uw = (float) unitWidth();
    const float railW = 16.0f;
    const float h = (float) getHeight();
    drawRail (g, { ux + 2.0f, 0.0f, railW, h });
    drawRail (g, { ux + uw - UnitComponent::stripWidth - railW - 2.0f, 0.0f, railW, h });

    const auto z = addZone().toFloat();
    const bool full = isFull();
    auto inner = z.reduced (railW + 10.0f, 8.0f).withTrimmedRight ((float) UnitComponent::stripWidth);
    const bool hot = drop.kind == DropTarget::Append || (addHover && ! full);
    g.setColour (hot ? theme::accent.withAlpha (0.07f) : Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (inner, 6.0f);
    Path border;
    border.addRoundedRectangle (inner.reduced (0.5f), 6.0f);
    Path dashed;
    const float dashes[] = { 7.0f, 5.0f };
    PathStrokeType (1.3f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (hot ? theme::accent : theme::accent.withAlpha (full ? 0.15f : 0.3f));
    g.fillPath (dashed);

    if (full)
    {
        g.setFont (theme::monoLabel (11.0f).boldened());
        g.setColour (theme::silverDim);
        g.drawText ("RACK FULL  -  DROP ON A UNIT TO REPLACE IT", inner, Justification::centred, false);
    }
    else
    {
        auto t = inner.withSizeKeepingCentre (inner.getWidth(), 46.0f);
        theme::drawGlowText (g, "DROP EFFECTS HERE", t.removeFromTop (28.0f),
                             theme::stencilFont (24.0f), hot ? theme::accent : theme::silverDim.withAlpha (0.8f), Justification::centred, hot ? 1.0f : 0.3f);
        g.setFont (theme::monoLabel (10.5f));
        g.setColour (theme::silverDim.withAlpha (0.7f));
        g.drawText ("or click to add", t, Justification::centred, false);
    }
}

void RackView::paintOverChildren (Graphics& g)
{
    const float ux = (float) unitX(), uw = (float) unitWidth();
    if (drop.kind == DropTarget::Insert)
    {
        auto line = drop.w > 0 ? Rectangle<float> ((float) drop.x + 6.0f, drop.y - 2.0f, (float) drop.w - 12.0f, 4.0f)
                               : Rectangle<float> (ux + 6.0f, drop.y - 2.0f, uw - 12.0f, 4.0f);
        g.setColour (theme::accent.withAlpha (0.22f));
        g.fillRoundedRectangle (line.expanded (0.0f, 5.0f), 6.0f);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (line, 2.0f);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillRoundedRectangle (line.reduced (0.0f, 1.3f), 1.0f);
        // arrow heads at both ends
        for (float x : { line.getX(), line.getRight() })
        {
            Path tri;
            const float d = x == line.getX() ? 1.0f : -1.0f;
            tri.addTriangle (x, drop.y - 7.0f, x, drop.y + 7.0f, x + d * 9.0f, drop.y);
            g.setColour (theme::accent);
            g.fillPath (tri);
        }
    }
    else if ((drop.kind == DropTarget::Append || drop.kind == DropTarget::Move) && drop.branch >= 0 && drop.w > 0)
    {
        // SPLIT: highlight the target column
        auto col = Rectangle<float> ((float) drop.x, (float) (splitY + SplitUnit::height), (float) drop.w,
                                     (float) (contentBottom() - splitY - SplitUnit::height));
        g.setColour (theme::accent.withAlpha (0.10f));
        g.fillRect (col);
        g.setColour (theme::accent);
        g.drawRect (col.reduced (1.0f), 2.0f);
        g.setFont (theme::monoLabel (11.0f).boldened());
        g.drawText (drop.branch == 0 ? "BRANCH A" : "BRANCH B", col.removeFromTop (24.0f), Justification::centred, false);
    }
    else if (drop.kind == DropTarget::Replace)
    {
        if (auto* u = unitForSlot (drop.slot))
        {
            auto b = u->getBounds().toFloat().withWidth ((float) (u->getWidth() - UnitComponent::stripWidth));
            g.setColour (replaceYellow.withAlpha (0.14f));
            g.fillRect (b);
            g.setColour (replaceYellow);
            g.drawRect (b.reduced (1.0f), 2.5f);
            auto tag = Rectangle<float> (92.0f, 22.0f).withCentre (b.getCentre());
            g.setColour (Colour (0xf0101114));
            g.fillRoundedRectangle (tag, 4.0f);
            g.setColour (replaceYellow);
            g.drawRoundedRectangle (tag, 4.0f, 1.2f);
            g.setFont (theme::monoLabel (11.0f).boldened());
            g.drawText ("REPLACE", tag, Justification::centred, false);
        }
    }
}

void RackView::mouseMove (const MouseEvent& e)
{
    const bool h = ! isFull() && addZone().contains (e.getPosition());
    if (h != addHover) { addHover = h; repaint (addZone()); setMouseCursor (h ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor); }
}

void RackView::mouseUp (const MouseEvent& e)
{
    if (! addZone().contains (e.getPosition()) || isFull()) return;
    SafePointer<RackView> safe (this);
    // add an effect (SPLIT: to branch A / B), or add a SPLIT at the end of the rack
    {
        PopupMenu m;
        if (proc.hasSplit())
        {
            for (int br = 0; br < 2; ++br)
            {
                PopupMenu sub;
                for (const auto& grp : gear::groups())
                {
                    sub.addSectionHeader (grp.name);
                    for (auto t : grp.types) sub.addItem (1000 * (br + 1) + (int) t, effectInfo (t).name);
                }
                m.addSubMenu (br == 0 ? "Add to branch A" : "Add to branch B", sub);
            }
        }
        else
        {
            for (const auto& grp : gear::groups())
            {
                m.addSectionHeader (grp.name);
                for (auto t : grp.types) m.addItem ((int) t, effectInfo (t).name);
            }
            m.addSeparator();
            m.addItem (999, "Add SPLIT (parallel branches A | B) here");
        }
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            auto& p = safe->proc;
            if (r == 999) { p.insertSplit (safe->lastOccupied() + 1); return; }
            if (r >= 1000) { p.insertEffectInBranch (r / 1000 - 1, (EffectType) (r % 1000)); return; }
            p.insertEffect (safe->lastOccupied() + 1, (EffectType) r);
        });
    }
}

void RackView::showAddMenu (EffectorKillaAudioProcessor&, Component* target, std::function<void (EffectType)> picked)
{
    PopupMenu m;
    for (const auto& grp : gear::groups())
    {
        m.addSectionHeader (grp.name);
        for (auto t : grp.types) m.addItem ((int) t, effectInfo (t).name);
    }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (target).withMousePosition(), [picked] (int r)
    {
        if (r > 0 && picked) picked ((EffectType) r);
    });
}

// ----------------------------------------------------------------------------
namespace { int grabY = 0; }

void RackView::beginUnitDrag (UnitComponent& u, const MouseEvent& e)
{
    auto* c = DragAndDropContainer::findParentDragContainerFor (this);
    if (c == nullptr || c->isDragAndDropActive()) return;
    constexpr int margin = 18;
    auto img = u.createLiftImage (margin);
    const auto grab = e.getMouseDownPosition();
    grabY = grab.y;
    Point<int> offset (-(grab.x + margin), -(grab.y + margin));
    draggingSlot = u.getSlot();
    drop = {};
    c->startDragging (dnd::unit (u.getSlot()), &u, ScaledImage (img, 1.0), true, &offset, &e.source);
    u.setLifted (true);
    layoutTargets (false);
}

void RackView::endUnitDrag()
{
    for (auto* u : units) u->setLifted (false);
    draggingSlot = -1;
    if (drop.kind == DropTarget::Move) drop = {};
    layoutTargets (false);
    repaint();
}

RackView::DropTarget RackView::dropTargetFor (Point<int> pos, bool fromUnit, int fromSlot) const
{
    DropTarget d;
    if (proc.hasSplit())
    {
        // SPLIT layout: hit-test the units (two columns), otherwise the column under the mouse
        for (auto* u : units)
        {
            if (u->getSlot() == fromSlot && fromUnit) continue;
            const auto b = u->getBounds();
            if (! b.expanded (0, gapY).contains (pos)) continue;
            const int s = u->getSlot();
            const int br = proc.getSlotBranch (s);
            d.branch = br;
            if (br >= 0) { d.x = b.getX(); d.w = b.getWidth(); }
            if (fromUnit)
            {
                // drop a unit on another one: take its place (same branch) / move into that branch
                d.kind = DropTarget::Move;
                d.slot = s;
                return d;
            }
            const float rel = (float) (pos.y - b.getY()) / (float) b.getHeight();
            if (rel > 0.25f && rel < 0.75f) { d.kind = DropTarget::Replace; d.slot = s; return d; }
            const bool before = rel < 0.5f;
            d.kind = DropTarget::Insert;
            d.slot = before ? s : s + 1;
            d.y = before ? (float) b.getY() - gapY * 0.5f : (float) b.getBottom() + gapY * 0.5f;
            return d;
        }
        if (pos.y >= splitY + SplitUnit::height)
        {
            const int col = branchColumnAt (pos);
            const int colW = (unitWidth() - gapY) / 2;
            d.kind = fromUnit ? DropTarget::Move : DropTarget::Append;
            d.branch = col;
            d.x = col == 0 ? unitX() : unitX() + colW + gapY;
            d.w = colW;
            return d;
        }
        if (fromUnit) return d; // above the split: no reordering across it from here (use the unit menu)
        d.kind = DropTarget::Insert;
        d.slot = proc.getSplitPosition();
        d.y = (float) splitY - gapY * 0.5f;
        return d;
    }
    if (fromUnit)
    {
        // where would the lifted panel land? use its centre, not the mouse
        const int m = units.size() - 1;
        const float centre = (float) (pos.y - grabY) + UnitComponent::height * 0.5f;
        d.kind = DropTarget::Move;
        d.gap = jlimit (0, jmax (0, m), (int) std::floor ((centre - (float) topMargin) / (float) (UnitComponent::height + gapY)));
        juce::ignoreUnused (fromSlot);
        return d;
    }

    for (int i = 0; i < units.size(); ++i)
    {
        const auto b = units[i]->getBounds();
        if (pos.y > b.getBottom() + gapY) continue;
        const float rel = (float) (pos.y - b.getY()) / (float) b.getHeight();
        const int s = units[i]->getSlot();
        if (rel > 0.25f && rel < 0.75f)
        {
            d.kind = DropTarget::Replace;
            d.slot = s;
            return d;
        }
        const bool before = rel < 0.5f;
        d.kind = DropTarget::Insert;
        d.slot = before ? s : s + 1;
        d.y = before ? (float) b.getY() - gapY * 0.5f : (float) b.getBottom() + gapY * 0.5f;
        return d;
    }
    d.kind = DropTarget::Append;
    return d;
}

bool RackView::isInterestedInDragSource (const SourceDetails& d)
{
    EffectType t; int s;
    return dnd::parseFx (d.description, t) || dnd::parseUnit (d.description, s);
}

void RackView::itemDragEnter (const SourceDetails& d) { itemDragMove (d); }

void RackView::itemDragMove (const SourceDetails& d)
{
    int from = -1;
    const bool fromUnit = dnd::parseUnit (d.description, from);
    const auto t = dropTargetFor (d.localPosition, fromUnit, from);
    if (t.kind != drop.kind || t.slot != drop.slot || t.y != drop.y || t.gap != drop.gap || t.branch != drop.branch)
    {
        drop = t;
        if (fromUnit) layoutTargets (false);
        repaint();
    }
    if (auto* vp = findParentComponentOfClass<Viewport>())
    {
        const auto inVp = vp->getLocalPoint (this, d.localPosition);
        vp->autoScroll (inVp.x, inVp.y, 40, 14);
    }
}

void RackView::itemDragExit (const SourceDetails&)
{
    drop = {};
    layoutTargets (false);   // a lifted unit: the gap goes back to where it came from
    repaint();
}

void RackView::itemDropped (const SourceDetails& d)
{
    const auto target = drop;
    drop = {};
    repaint();
    EffectType t;
    int from = -1;
    auto say = [this] (const String& m) { if (onMessage) onMessage (m); };

    if (dnd::parseFx (d.description, t))
    {
        switch (target.kind)
        {
            case DropTarget::Replace:
                if (proc.getSlotMeta (target.slot).writeProtect) { say ("UNIT IS LOCKED"); return; }
                proc.playTape (target.slot, t, 0);
                return;
            case DropTarget::Insert:
                if (isFull()) { say ("RACK FULL - DROP ON A UNIT TO REPLACE IT"); return; }
                if (target.branch >= 0) proc.insertEffectInBranchAt (target.slot, target.branch, t);
                else proc.insertEffect (target.slot, t);
                return;
            case DropTarget::Append:
                if (isFull()) { say ("RACK FULL - DROP ON A UNIT TO REPLACE IT"); return; }
                if (target.branch >= 0 && proc.hasSplit()) proc.insertEffectInBranch (target.branch, t);
                else proc.insertEffect (lastOccupied() + 1, t);
                return;
            case DropTarget::Move:
            case DropTarget::None: return;
        }
    }
    else if (dnd::parseUnit (d.description, from))
    {
        if (target.kind == DropTarget::Move && proc.hasSplit())
        {
            // SPLIT: into the other column = change branch; onto a unit of the same branch = take its place
            const int fromBranch = proc.getSlotBranch (from);
            if (target.branch >= 0 && target.branch != fromBranch) proc.setSlotBranch (from, target.branch);
            else if (target.slot >= 0 && target.slot != from && target.branch == fromBranch) proc.moveSlot (from, target.slot);
            endUnitDrag();
            refresh (false);
            return;
        }
        if (target.kind == DropTarget::Move)
        {
            std::vector<int> rest;
            for (auto* u : units) if (u->getSlot() != from) rest.push_back (u->getSlot());
            int to = -1;
            if (rest.empty()) to = from;
            else if (target.gap < (int) rest.size()) { const int ts = rest[(size_t) target.gap]; to = ts > from ? ts - 1 : ts; }
            else { const int last = rest.back(); to = last > from ? last : last + 1; }
            to = jlimit (0, kNumSlots - 1, to);
            // keep the gap layout while the processor rebuilds - it is already the final order
            if (to != from)
            {
                proc.moveSlot (from, to);
                refresh (false);   // rebuild now: the gap layout already shows the final order, so nothing jumps
            }
        }
        endUnitDrag();
    }
}

} // namespace ek::ui
