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

static const String kArrow = String::fromUTF8 ("\xe2\x96\xbe");   // small down triangle

// ============================================================================
UnitComponent::UnitComponent (EffectorKillaAudioProcessor& p, RackView& o, int s)
    : proc (p), owner (o), slot (s), type (p.getSlotType (s)),
      presetBtn ("", theme::accent, 10.5f),
      powerBtn ("POWER", theme::accent, 10.5f),
      soloBtn ("SOLO", Colour (0xffffc861), 10.5f),
      msBtn ("ST", theme::accent, 10.5f),
      lockBtn ("LOCK", theme::killRed, 10.5f),
      removeBtn (String::fromUTF8 ("\xe2\x9c\x95"), theme::killRed, 12.0f),
      menuBtn (String::fromUTF8 ("\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2"), theme::accent, 12.0f)
{
    const auto& info = effectInfo (type);
    auto addKnob = [this, &info] (int param, const String& name)
    {
        auto* k = knobs.add (new LabeledKnob (name));
        knobParam.push_back (param);
        const auto id = param < kNumParams ? ParamIDs::slotParam (slot, param) : ParamIDs::slotMix (slot);
        auto* rp = proc.getParam (id);
        if (rp != nullptr) k->knob.bindTo (*rp);
        k->setAccent (info.colour.interpolatedWith (theme::accent, 0.55f));
        k->knob.onGestureStart = [this] { proc.pushUndo(); };
        if (param < kNumParams)
        {
            const auto spec = info.params[(size_t) param];
            k->knob.setDefault (spec.defaultNorm());
            k->valueText = [rp, spec] { return rp != nullptr ? spec.format (spec.toReal (rp->getValue())) : String(); };
            k->setTooltip (info.name + ": " + spec.name + "   (drag, Shift = fine, double-click = default)");
        }
        else
        {
            k->knob.setDefault (1.0f);
            k->valueText = [rp] { return rp != nullptr ? String (roundToInt (rp->getValue() * 100.0f)) + " %" : String(); };
            k->setTooltip ("Unit dry / wet");
        }
        addAndMakeVisible (k);
    };
    for (int i = 0; i < kNumParams; ++i)
        if (info.params[(size_t) i].used)
            addKnob (i, info.params[(size_t) i].name);
    addKnob (kNumParams, "Dry/Wet");

    presetBtn.onClick = [this] { showPresetMenu(); };
    presetBtn.setTooltip ("Unit presets: factory programmes, your saved units, copy / paste");

    powerBtn.isLit = [this] { return proc.getSlotParam (slot, 9) < 0.5f; };
    powerBtn.onClick = [this]
    {
        proc.pushUndo();
        proc.setSlotParam (slot, 9, proc.getSlotParam (slot, 9) > 0.5f ? 0.0f : 1.0f);
    };
    powerBtn.setTooltip ("Unit on / off. Off keeps running in the background, so tails continue when you switch it back on.");

    soloBtn.isLit = [this] { return proc.getSolo() == slot; };
    soloBtn.blinkWhenLit = true;
    soloBtn.onClick = [this] { proc.setSolo (proc.getSolo() == slot ? -1 : slot); };
    soloBtn.setTooltip ("Hear only this unit");

    msBtn.isLit = [this] { return proc.getSlotMeta (slot).ms != MSMode::Stereo; };
    msBtn.onClick = [this]
    {
        const auto m = proc.getSlotMeta (slot).ms;
        proc.setSlotMs (slot, m == MSMode::Stereo ? MSMode::Mid : (m == MSMode::Mid ? MSMode::Side : MSMode::Stereo));
    };
    msBtn.setTooltip ("Process the whole stereo signal, only the MID or only the SIDE");

    lockBtn.isLit = [this] { return proc.getSlotMeta (slot).writeProtect; };
    lockBtn.onClick = [this] { proc.setWriteProtect (slot, ! proc.getSlotMeta (slot).writeProtect); };
    lockBtn.setTooltip ("Lock: KILL and preset changes keep this unit");

    removeBtn.isEnabledFn = [this] { return ! proc.getSlotMeta (slot).writeProtect; };
    removeBtn.onClick = [this] { proc.removeEffect (slot); };
    removeBtn.setTooltip ("Remove from the rack (or drag it back to the gear list)");

    menuBtn.onClick = [this] { showUnitMenu(); };
    menuBtn.setTooltip ("More: replace, reset, listen up to here, copy / paste");

    for (auto* b : { &presetBtn, &powerBtn, &soloBtn, &msBtn, &lockBtn, &removeBtn, &menuBtn })
        addAndMakeVisible (b);
    tick();
}

String UnitComponent::currentProgrammeName() const
{
    const int t = proc.findTape (slot);
    if (t < 0) return "CUSTOM";
    return proc.bank.getTapes (type)[(size_t) t].name;
}

void UnitComponent::tick()
{
    for (auto* k : knobs) k->tick();
    for (auto* b : { &powerBtn, &soloBtn, &msBtn, &lockBtn, &removeBtn }) b->tick();
    const auto ms = proc.getSlotMeta (slot).ms;
    msBtn.setText (ms == MSMode::Stereo ? "ST" : (ms == MSMode::Mid ? "MID" : "SIDE"));
    const auto prog = currentProgrammeName();
    if (prog != shownProgramme) { shownProgramme = prog; presetBtn.setText (prog + "  " + kArrow); }
    const bool paused = proc.getSlotParam (slot, 9) > 0.5f;
    const bool dimmed = owner.isDimmed (slot);
    if (paused != lastPaused || dimmed != lastDimmed) { lastPaused = paused; lastDimmed = dimmed; repaint(); }
}

void UnitComponent::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (36);                         // grip
    auto name = r.removeFromLeft (180).reduced (8, 8);
    presetBtn.setBounds (name.removeFromBottom (26));

    auto right = r.removeFromRight (100).reduced (6, 8);
    const int rowH = (right.getHeight() - 9) / 4;
    auto row = [&] { auto x = right.removeFromTop (rowH); right.removeFromTop (3); return x; };
    powerBtn.setBounds (row());
    { auto x = row(); soloBtn.setBounds (x.removeFromLeft (x.getWidth() / 2 - 1)); x.removeFromLeft (2); msBtn.setBounds (x); }
    { auto x = row(); lockBtn.setBounds (x.removeFromLeft (x.getWidth() / 2 - 1)); x.removeFromLeft (2); removeBtn.setBounds (x); }
    menuBtn.setBounds (row());

    auto area = r.reduced (4, 6);
    const int n = knobs.size();
    const int mixGap = 10;
    const int w = n > 0 ? jmin (84, (area.getWidth() - mixGap) / n) : 0;
    for (int i = 0; i < n; ++i)
    {
        if (i == n - 1) area.removeFromLeft (mixGap);   // MIX sits apart from the effect controls
        knobs[i]->setBounds (area.removeFromLeft (w));
    }
}

void UnitComponent::paint (Graphics& g)
{
    const auto& info = effectInfo (type);
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    drawCard (g, b, 9.0f);

    // grip with slot number
    auto grip = gripArea().toFloat().reduced (5.0f, 8.0f);
    g.setColour (info.colour);
    g.fillRoundedRectangle (grip.removeFromLeft (4.0f), 2.0f);
    drawSmallLabel (g, "S" + String (slot + 1), grip.removeFromTop (18.0f), Justification::centred, theme::silver, 11.0f);
    g.setColour (theme::silverDim.withAlpha (0.55f));
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (grip.getCentreX() - 4.0f + j * 6.0f, grip.getCentreY() - 14.0f + i * 6.0f, 2.6f, 2.6f);

    // name block
    auto name = Rectangle<float> (40.0f, 8.0f, 172.0f, 70.0f);
    g.setFont (theme::stencilFont (21.0f));
    g.setColour (theme::silver);
    g.drawFittedText (info.name.toUpperCase(), name.removeFromTop (30.0f).toNearestInt(), Justification::centredLeft, 1, 0.6f);
    static const char* catNames[] = { "SATURATION", "DISTORTION", "LO-FI", "FILTER", "EQ", "DYNAMICS",
                                      "MODULATION", "SPACE", "STEREO", "GATE", "CLIPPER" };
    drawSmallLabel (g, catNames[(int) info.category], name.removeFromTop (14.0f), Justification::centredLeft, info.colour.brighter (0.3f), 9.5f);

    // separators
    g.setColour (theme::line);
    g.drawVerticalLine (36, 8.0f, (float) getHeight() - 8.0f);
    g.drawVerticalLine (216, 8.0f, (float) getHeight() - 8.0f);
    g.drawVerticalLine (getWidth() - 100, 8.0f, (float) getHeight() - 8.0f);
}

void UnitComponent::paintOverChildren (Graphics& g)
{
    if (lastPaused || lastDimmed)
    {
        auto area = getLocalBounds().toFloat().withTrimmedLeft (216.0f).withTrimmedRight (100.0f).reduced (1.0f, 2.0f);
        g.setColour (Colour (0xff04060a).withAlpha (0.62f));
        g.fillRect (area);
        drawSmallLabel (g, lastPaused ? "POWER OFF  -  TAILS KEEP RUNNING" : "MUTED BY SOLO", area, Justification::centred,
                        theme::silverDim, 11.0f);
    }
}

void UnitComponent::mouseDown (const MouseEvent& e)
{
    dragStarted = false;
    if (e.mods.isPopupMenu()) showUnitMenu();
}

void UnitComponent::mouseDrag (const MouseEvent& e)
{
    if (dragStarted || ! gripArea().contains (e.getMouseDownPosition())) return;
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        auto img = createComponentSnapshot (getLocalBounds(), true, 0.6f);
        img.multiplyAllAlphas (0.8f);
        c->startDragging (dnd::unit (slot), this, ScaledImage (img, 0.6), true);
    }
}

void UnitComponent::mouseUp (const MouseEvent&) { dragStarted = false; }

void UnitComponent::showPresetMenu()
{
    PopupMenu m;
    const auto& tapes = proc.bank.getTapes (type);
    const int current = proc.findTape (slot);
    m.addSectionHeader (effectInfo (type).name.toUpperCase() + " PRESETS");
    for (int i = 0; i < (int) tapes.size(); ++i)
        m.addItem (1 + i, tapes[(size_t) i].name + "   - " + tapes[(size_t) i].description, true, i == current);

    const auto files = proc.bank.getSlotPresetFiles();
    PopupMenu mine;
    std::vector<File> matching;
    for (auto& f : files)
        if (auto s = proc.bank.loadSlotPreset (f); s.has_value() && s->type == type)
        {
            mine.addItem (1000 + (int) matching.size(), f.getFileNameWithoutExtension());
            matching.push_back (f);
        }
    m.addSeparator();
    m.addSubMenu ("My unit presets", mine, ! matching.empty());
    m.addItem (900, "Save unit preset...");
    m.addSeparator();
    m.addItem (901, "Copy unit");
    m.addItem (902, "Paste unit", proc.hasSlotClipboard() && ! proc.getSlotMeta (slot).writeProtect);

    SafePointer<UnitComponent> safe (this);
    auto& p = proc;
    const int s = slot;
    const auto t = type;
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBtn), [safe, &p, s, t, matching] (int r)
    {
        if (r <= 0) return;
        if (r >= 1000) { p.loadSlotPreset (s, matching[(size_t) (r - 1000)]); return; }
        if (r == 901) { p.copySlot (s); return; }
        if (r == 902) { p.pasteSlot (s); return; }
        if (r == 900)
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
            return;
        }
        p.playTape (s, t, r - 1);
        juce::ignoreUnused (safe);
    });
}

void UnitComponent::showUnitMenu()
{
    PopupMenu m;
    const bool locked = proc.getSlotMeta (slot).writeProtect;
    m.addSectionHeader ("S" + String (slot + 1) + "  " + effectInfo (type).name.toUpperCase());
    PopupMenu replace;
    for (const auto& grp : gear::groups())
    {
        replace.addSectionHeader (grp.name);
        for (auto t : grp.types) replace.addItem (100 + (int) t, effectInfo (t).name, ! locked, t == type);
    }
    m.addSubMenu ("Replace with", replace, ! locked);
    m.addItem (1, "Reset to default", ! locked);
    m.addItem (2, "Listen up to here (ignore the units below)", true, proc.getScreening() == slot);
    m.addItem (3, "Solo (hear only this unit)", true, proc.getSolo() == slot);
    m.addSeparator();
    m.addItem (4, "Copy unit");
    m.addItem (5, "Paste unit", proc.hasSlotClipboard() && ! locked);
    m.addItem (6, "Lock", true, locked);
    m.addSeparator();
    m.addItem (7, "Remove", ! locked);

    auto& p = proc;
    const int s = slot;
    const auto t = type;
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&menuBtn), [&p, s, t] (int r)
    {
        if (r <= 0) return;
        if (r >= 100) { const auto nt = (EffectType) (r - 100); if (nt != t) p.playTape (s, nt, 0); return; }
        switch (r)
        {
            case 1: p.setSlotType (s, t); break;
            case 2: p.setScreening (p.getScreening() == s ? -1 : s); break;
            case 3: p.setSolo (p.getSolo() == s ? -1 : s); break;
            case 4: p.copySlot (s); break;
            case 5: p.pasteSlot (s); break;
            case 6: p.setWriteProtect (s, ! p.getSlotMeta (s).writeProtect); break;
            case 7: p.removeEffect (s); break;
            default: break;
        }
    });
}

// ============================================================================
RackView::RackView (EffectorKillaAudioProcessor& p) : proc (p) { refresh(); }

String RackView::signature() const
{
    String s;
    for (int i = 0; i < kNumSlots; ++i) s << (int) proc.getSlotType (i) << ",";
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

void RackView::refresh (bool repaintAll)
{
    const auto sig = signature();
    if (sig != builtFor)
    {
        builtFor = sig;
        units.clear();
        for (int i = 0; i < kNumSlots; ++i)
            if (proc.getSlotType (i) != EffectType::None)
                addAndMakeVisible (units.add (new UnitComponent (proc, *this, i)));
        if (getWidth() > 0) setSize (getWidth(), getIdealHeight());
        resized();
        repaint();
    }
    else if (repaintAll)
    {
        repaint();
    }
}

void RackView::tick()
{
    for (auto* u : units) u->tick();
}

int RackView::getIdealHeight() const
{
    return 8 + units.size() * (UnitComponent::height + 8) + 86;
}

Rectangle<int> RackView::addZone() const
{
    const int y = 8 + units.size() * (UnitComponent::height + 8);
    return { 8, y, getWidth() - 16, 70 };
}

void RackView::resized()
{
    int y = 8;
    for (auto* u : units)
    {
        u->setBounds (8, y, getWidth() - 16, UnitComponent::height);
        y += UnitComponent::height + 8;
    }
}

void RackView::paint (Graphics& g)
{
    // signal flow rail behind the units
    g.setColour (theme::line);
    g.fillRect (getWidth() / 2 - 1, 0, 2, getHeight());

    const auto z = addZone().toFloat();
    const bool full = isFull();
    g.setColour (Colour (0xff070b13));
    g.fillRoundedRectangle (z, 9.0f);
    Path border;
    border.addRoundedRectangle (z.reduced (1.0f), 9.0f);
    Path dashed;
    const float dashes[] = { 6.0f, 5.0f };
    PathStrokeType (1.2f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (drop.kind == DropTarget::Append ? theme::accent : theme::accent.withAlpha (0.35f));
    g.fillPath (dashed);
    drawSmallLabel (g, full ? "RACK FULL  -  8 / 8 UNITS  (DROP ON A UNIT TO REPLACE IT)"
                            : (units.isEmpty() ? "+   DRAG AN EFFECT FROM THE GEAR LIST   -   OR CLICK TO ADD"
                                               : "+   ADD A UNIT   -   DRAG FROM THE GEAR LIST OR CLICK"),
                    z, Justification::centred, full ? theme::silverDim : theme::accent, 11.0f);
}

void RackView::paintOverChildren (Graphics& g)
{
    if (drop.kind == DropTarget::Insert)
    {
        g.setColour (theme::accent);
        g.fillRoundedRectangle (10.0f, drop.y - 2.0f, (float) getWidth() - 20.0f, 4.0f, 2.0f);
        g.setColour (theme::accent.withAlpha (0.25f));
        g.fillRoundedRectangle (10.0f, drop.y - 6.0f, (float) getWidth() - 20.0f, 12.0f, 6.0f);
    }
    else if (drop.kind == DropTarget::Replace)
    {
        for (auto* u : units)
            if (u->getSlot() == drop.slot)
            {
                auto b = u->getBounds().toFloat();
                g.setColour (Colour (0xffffc861).withAlpha (0.16f));
                g.fillRoundedRectangle (b, 9.0f);
                g.setColour (Colour (0xffffc861));
                g.drawRoundedRectangle (b.reduced (1.0f), 9.0f, 2.0f);
                drawSmallLabel (g, "REPLACE", b.removeFromTop (20.0f).withTrimmedRight (110.0f), Justification::centredRight, Colour (0xffffc861), 11.0f);
            }
    }
}

void RackView::mouseUp (const MouseEvent& e)
{
    if (! addZone().contains (e.getPosition()) || isFull()) return;
    SafePointer<RackView> safe (this);
    showAddMenu (proc, this, [safe] (EffectType t)
    {
        if (safe == nullptr) return;
        safe->proc.insertEffect (safe->lastOccupied() + 1, t);
    });
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

RackView::DropTarget RackView::dropTargetFor (Point<int> pos, bool fromUnit, int fromSlot) const
{
    DropTarget d;
    for (int i = 0; i < units.size(); ++i)
    {
        const auto b = units[i]->getBounds();
        if (pos.y > b.getBottom() + 4) continue;
        const float rel = (float) (pos.y - b.getY()) / (float) b.getHeight();
        const int s = units[i]->getSlot();
        if (! fromUnit && rel > 0.28f && rel < 0.72f)
        {
            d.kind = DropTarget::Replace;
            d.slot = s;
            return d;
        }
        const bool before = rel < 0.5f;
        d.kind = DropTarget::Insert;
        d.slot = before ? s : s + 1;
        d.y = before ? (float) b.getY() - 4.0f : (float) b.getBottom() + 4.0f;
        if (fromUnit && (d.slot == fromSlot || d.slot == fromSlot + 1)) d.kind = DropTarget::None; // no move
        return d;
    }
    d.kind = DropTarget::Append;
    if (fromUnit && fromSlot == lastOccupied()) d.kind = DropTarget::None;
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
    if (t.kind != drop.kind || t.slot != drop.slot || t.y != drop.y) { drop = t; repaint(); }
    if (auto* vp = findParentComponentOfClass<Viewport>())
    {
        const auto inVp = vp->getLocalPoint (this, d.localPosition);
        vp->autoScroll (inVp.x, inVp.y, 40, 14);
    }
}

void RackView::itemDragExit (const SourceDetails&)
{
    drop = {};
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
                proc.insertEffect (target.slot, t);
                return;
            case DropTarget::Append:
                if (isFull()) { say ("RACK FULL - DROP ON A UNIT TO REPLACE IT"); return; }
                proc.insertEffect (lastOccupied() + 1, t);
                return;
            case DropTarget::None: return;
        }
    }
    else if (dnd::parseUnit (d.description, from))
    {
        int to = -1;
        if (target.kind == DropTarget::Insert) to = target.slot > from ? target.slot - 1 : target.slot;
        else if (target.kind == DropTarget::Append) to = lastOccupied();
        if (to >= 0 && to != from) proc.moveSlot (from, jlimit (0, kNumSlots - 1, to));
    }
}

} // namespace ek::ui
