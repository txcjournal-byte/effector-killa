#include "RackView.h"
#include "GearList.h"
#include "CardStyle.h"
#include "../SharedResources.h"

namespace ek::ui
{
using namespace juce;
namespace cs = ek::card;

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
const Colour soloGold { 0xffffd23f };
const Colour removeRed { 0xffff2e3e };

float macroAngle (float norm) { return theme::knobStart + jlimit (0.0f, 1.0f, norm) * (theme::knobEnd - theme::knobStart); }

float easeOut (float t) { t = jlimit (0.0f, 1.0f, t); return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); }

// geometry shared with Faceplates.cpp (presetLcdArea): icon pills and the ST/MID/SIDE pill
constexpr int iconPill = 22, iconGap = 5, msWidth = 84;

Path roundedCard (Rectangle<float> r, float rad)
{
    Path p;
    p.addRoundedRectangle (r, rad);
    return p;
}

// soft outer glow around a rounded rectangle (layered strokes, cheap enough for hover / drag)
void glowAround (Graphics& g, Rectangle<float> r, float rad, Colour c, float amount)
{
    for (int i = 4; i >= 1; --i)
    {
        const float w = (float) i * 3.0f;
        g.setColour (c.withAlpha (c.getFloatAlpha() * amount * 0.07f * (5.0f - (float) i) * 0.6f));
        g.drawRoundedRectangle (r.expanded (w * 0.5f), rad + w * 0.5f, w);
    }
}

// dark Plex Mono pill with a coloured border (status, REPLACE, MERGE ...)
void tagPill (Graphics& g, Rectangle<float> r, const String& text, Colour border, Colour textColour, float fontH, Colour dot = {})
{
    const float rad = r.getHeight() * 0.5f;
    g.setColour (cs::bg.withAlpha (0.94f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (border.withAlpha (0.10f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (border.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    auto tr = r;
    if (! dot.isTransparent())
    {
        cs::dotGlow (g, { r.getX() + rad + 1.0f, r.getCentreY() }, 2.0f, dot, 1.0f);
        tr = tr.withTrimmedLeft (rad + 6.0f);
    }
    g.setFont (cs::monoFont (fontH, true));
    g.setColour (textColour);
    g.drawFittedText (text, tr.reduced (6.0f, 0.0f).toNearestInt(), dot.isTransparent() ? Justification::centred : Justification::centredLeft, 1, 0.7f);
}

void dashedRound (Graphics& g, Rectangle<float> r, float rad, Colour c, float thickness)
{
    Path border, dashed;
    border.addRoundedRectangle (r, rad);
    const float dashes[] = { 6.0f, 5.0f };
    PathStrokeType (thickness).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (c);
    g.fillPath (dashed);
}

// centred chrome headline (CardStyle's paintChromeTitle draws left-aligned into its box)
void chromeCentred (Graphics& g, const String& s, Rectangle<float> r, float h, float glow)
{
    const Font f = cs::displayFont (h);
    const float w = GlyphArrangement::getStringWidth (f, s.toUpperCase()) + f.getAscent() * 0.72f * cs::shear + 2.0f;
    cs::paintChromeTitle (g, s, r.withSizeKeepingCentre (jmin (w, r.getWidth()), r.getHeight()), h, glow);
}
} // namespace

// ============================================================================
int UnitComponent::idealHeight (EffectType t, int w)
{
    if (w >= compactBelow) return height;
    const auto& info = effectInfo (t);
    float total = 0.0f;
    for (int i = 0; i < kNumParams; ++i)
        if (info.params[(size_t) i].used)
            total += (float) RackControl::preferredWidth (RackControl::kindFor (&info.params[(size_t) i]), &info.params[(size_t) i]);
    total += (float) RackControl::preferredWidth (RackControl::Kind::Rotary, nullptr);
    const float avail = (float) w - 24.0f - 14.0f;
    const int rows = total * 0.86f > avail ? 2 : 1;
    return 98 + rows * 82 + 8;
}

UnitComponent::UnitComponent (EffectorKillaAudioProcessor& p, RackView& o, int s)
    : proc (p), owner (o), slot (s), type (p.getSlotType (s)), pal (faceplate::palette (p.getSlotType (s))),
      menuBtn (SideButton::Icon::Menu, cs::ice),
      powerBtn (SideButton::Icon::Power, cs::ice),
      soloBtn (SideButton::Icon::Solo, soloGold),
      lockBtn (SideButton::Icon::Lock, removeRed),
      removeBtn (SideButton::Icon::Remove, removeRed)
{
    setOpaque (false);   // rounded card corners: the chain background (signal line) shows through
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
            // MACRO LEARN ranges (outside the mod rings): thin ice arc min..max with two drag handles
            const int nMod = (int) proc.findModAssignments (slot, param).size();
            int k = 0;
            for (const auto& mm : proc.getMacroAssignmentsFor (slot, param))
            {
                if (! mm.learned) continue;
                const auto c = face.getCentre();
                const float r = face.getWidth() * 0.5f + 3.0f + 4.0f * (float) (nMod + k++);
                const bool live = param == macroDragParam && mm.macro == macroDragMacro;
                const float a0 = macroAngle (live ? macroDragLo : mm.atZero), a1 = macroAngle (live ? macroDragHi : mm.atOne);
                Path arc;
                arc.addCentredArc (c.x, c.y, r, r, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
                cs::strokeGlow (g, arc, cs::ice.withAlpha (0.75f), 1.1f, 0.8f);
                const Point<float> p0 (c.x + r * std::sin (a0), c.y - r * std::cos (a0));
                const Point<float> p1 (c.x + r * std::sin (a1), c.y - r * std::cos (a1));
                g.setColour (cs::bg);
                g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (p0));
                g.setColour (cs::ice);
                g.drawEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (p0), 1.0f);
                cs::dotGlow (g, p1, 2.3f, cs::ice, 1.0f);
            }
        };
        // drag a modulation ring (drawn by the editor at face / 2 + 3 + 4 * ring) to change its depth,
        // or a MACRO LEARN handle (min = hollow, max = filled) to change the learned range
        c->hitRing = [this, param] (Point<float> rel, float faceR)
        {
            const float d = rel.getDistanceFromOrigin();
            const auto idx = proc.findModAssignments (slot, param);
            int k = (int) idx.size();
            for (const auto& mm : proc.getMacroAssignmentsFor (slot, param))
            {
                if (! mm.learned) continue;
                const float r = faceR + 3.0f + 4.0f * (float) k++;
                const bool live = param == macroDragParam && mm.macro == macroDragMacro;
                for (int end = 0; end < 2; ++end)
                {
                    const float a = macroAngle (end == 0 ? (live ? macroDragLo : mm.atZero) : (live ? macroDragHi : mm.atOne));
                    if (rel.getDistanceFrom ({ r * std::sin (a), -r * std::cos (a) }) < 5.5f) return 1000 + mm.macro * 2 + end;
                }
            }
            for (size_t i = 0; i < idx.size(); ++i)
                if (std::abs (d - (faceR + 3.0f + 4.0f * (float) i)) < 3.5f) return idx[i];
            return -1;
        };
        c->onRingDrag = [this, param] (int id, float delta)
        {
            if (id >= 1000)
            {
                const int end = (id - 1000) % 2;
                if (macroDragMacro < 0) return;
                if (end == 0) macroDragLo = jlimit (0.0f, 1.0f, macroDragLo + delta);
                else          macroDragHi = jlimit (0.0f, 1.0f, macroDragHi + delta);
                if (auto* ctl = findControl (param)) ctl->repaint();
                return;
            }
            const auto all = proc.getModAssignments();
            if (id < 0 || id >= (int) all.size()) return;
            proc.setModDepth (id, jlimit (-1.0f, 1.0f, all[(size_t) id].depth + delta));
            owner.repaintKnobs();
        };
        c->onRingDragStart = [this, param] (int id)
        {
            if (id < 1000) { proc.pushUndo(); return; }   // mod depth: one undo step per gesture
            const int macro = (id - 1000) / 2;
            for (const auto& mm : proc.getMacroAssignmentsFor (slot, param))
                if (mm.learned && mm.macro == macro)
                {
                    macroDragParam = param;
                    macroDragMacro = macro;
                    macroDragLo = mm.atZero;
                    macroDragHi = mm.atOne;
                }
        };
        c->onRingDragEnd = [this, param] (int)
        {
            if (macroDragMacro >= 0 && macroDragParam == param)
                proc.assignMacro (macroDragMacro, slot, param, macroDragLo, macroDragHi);   // one undo step
            macroDragParam = macroDragMacro = -1;
            owner.repaintKnobs();
        };
        if (param < kNumParams)
        {
            c->getEffectiveValue = [this, param]
            {
                const float e = proc.engine.effectiveParams[(size_t) slot][(size_t) param].load (std::memory_order_relaxed);
                const float b = proc.getSlotParam (slot, param);
                return (e == 0.0f && b != 0.0f) ? b : e;   // engine not running yet: show the base value
            };
            c->setTooltip (info.name + ": " + spec->name + "   (drag, Shift = fine, double-click = default, right-click = more;"
                           " drag a mod ring = depth)");
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
    menuBtn.setTooltip ("Unit menu: programmes, my unit presets, save, copy / paste, replace, reset, listen up to here, SPLIT, remove");

    powerBtn.isLit = [this] { return proc.getSlotParam (slot, 9) < 0.5f; };
    powerBtn.onClick = [this]
    {
        proc.pushUndo();
        proc.setSlotParam (slot, 9, proc.getSlotParam (slot, 9) > 0.5f ? 0.0f : 1.0f);
    };
    powerBtn.setTooltip ("POWER - unit on / off. Off keeps running in the background, so echoes and reverb tails continue.\n"
                         "Press and hold: LEVEL-MATCHED POWER - hear the chain without this unit at the same loudness.");
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

    removeBtn.isEnabledFn = [this] { return ! proc.getSlotMeta (slot).writeProtect; };
    removeBtn.dangerHover = true;
    removeBtn.onClick = [this]
    {
        if (proc.getSlotMeta (slot).writeProtect) return;
        auto& p = proc;
        const int s = slot;
        MessageManager::callAsync ([&p, s] { p.removeEffect (s); });   // the card is rebuilt afterwards
    };
    removeBtn.setTooltip ("Remove this effect from the chain");

    for (auto* b : { &menuBtn, &powerBtn, &soloBtn, &lockBtn, &removeBtn })
        addAndMakeVisible (b);

    lcd.accent = pal.accent;
    lcd.onClick = [this] { showPresetMenu (&lcd); };
    lcd.setTooltip ("Unit programmes and your saved unit presets");
    addAndMakeVisible (lcd);

    msBtn.pal = pal;
    msBtn.getMode = [this] { return (int) proc.getSlotMeta (slot).ms; };
    msBtn.onSelect = [this] (int m)
    {
        const auto mode = m == 1 ? MSMode::Mid : (m == 2 ? MSMode::Side : MSMode::Stereo);
        if (proc.getSlotMeta (slot).ms != mode) proc.setSlotMs (slot, mode);
    };
    msBtn.setTooltip ("Process the whole stereo signal (ST), only the MID or only the SIDE");
    addAndMakeVisible (msBtn);

    setSize (852 - RackView::gutter, height);
    tick (owner.clock());
}

UnitComponent::~UnitComponent() = default;

Rectangle<float> UnitComponent::plateBounds() const
{
    return getLocalBounds().toFloat();
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
    const auto r = Rectangle<float> ((float) m, (float) m, (float) getWidth(), (float) getHeight());
    const float rad = cs::radius;
    const auto shape = roundedCard (r, rad);
    DropShadow (Colours::black.withAlpha (0.85f), m, { 0, m / 2 }).drawForPath (g, shape);
    DropShadow (pal.accent.interpolatedWith (cs::iceDeep, 0.5f).withAlpha (0.55f), m, {}).drawForPath (g, shape);
    g.setOpacity (0.96f);
    g.drawImageAt (snap, m, m);
    g.setOpacity (1.0f);
    g.setColour (cs::ice.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.2f);
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
    for (auto* b : { &menuBtn, &powerBtn, &soloBtn, &lockBtn, &removeBtn }) b->tick();
    msBtn.tick();
    lcd.tick();
    const bool audition = proc.getLevelMatchedAudition() == slot;
    if (audition || audition != lastAudition) repaint (statusPillArea().expanded (4.0f).getSmallestIntegerContainer());
    lastAudition = audition;
    lcd.setText (currentProgrammeName(), ! firstTick);

    const bool dim = ! state.powered || state.soloDimmed;
    if (state.powered != lastPowered || state.soloDimmed != lastDimmed || state.locked != lastLocked || firstTick)
    {
        lastPowered = state.powered;
        lastDimmed = state.soloDimmed;
        lastLocked = state.locked;
        for (auto* c : controls) c->setAlpha (dim ? 0.45f : 1.0f);
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
    const auto& info = effectInfo (type);
    std::vector<float> w;
    for (auto* c : controls)
    {
        const ParamSpec* sp = c->param < kNumParams ? &info.params[(size_t) c->param] : nullptr;
        w.push_back ((float) RackControl::preferredWidth (c->kind, sp));
    }

    const int rows = isCompact() && idealHeight (type, getWidth()) > 200 ? 2 : 1;
    if (rows == 2)
    {
        // two rows on one grid: equal cells, DRY/WET in the last cell of the second row
        const int perRow = (n + 1) / 2;
        const float cell = area.getWidth() / (float) perRow;
        const float rowH = area.getHeight() / 2.0f;
        for (int i = 0; i < n; ++i)
        {
            const bool mix = i == n - 1;
            const int row = i < perRow ? 0 : 1;
            const int col = mix ? perRow - 1 : (row == 0 ? i : i - perRow);
            const float cw = jmin (w[(size_t) i], cell - 2.0f);
            auto r = Rectangle<float> (area.getX() + cell * (float) col, area.getY() + rowH * (float) row, cell, rowH);
            controls[i]->setBounds (r.withSizeKeepingCentre (cw, rowH).toNearestInt());
        }
        return;
    }

    constexpr float mixGap = 14.0f;
    float total = 0.0f;
    for (auto v : w) total += v;
    const float avail = area.getWidth() - mixGap;
    const float scale = total > avail ? avail / total : 1.0f;
    const float spacing = total < avail ? jmin (isCompact() ? 6.0f : 24.0f, (avail - total) / (float) jmax (1, n - 1)) : 0.0f;
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
    const auto lcdArea = faceplate::presetLcdArea (plate).toNearestInt();
    lcd.setBounds (lcdArea);
    int x;
    if (isCompact())
    {
        msBtn.setBounds (lcdArea.getRight() + 8, lcdArea.getY(), msWidth, lcdArea.getHeight());
        x = msBtn.getRight() + 8;
    }
    else
    {
        msBtn.setBounds (lcdArea.getX() - 8 - msWidth, lcdArea.getY(), msWidth, lcdArea.getHeight());
        x = lcdArea.getRight() + 10;
    }
    for (auto* b : { &menuBtn, &powerBtn, &soloBtn, &lockBtn, &removeBtn })
    {
        b->setBounds (x, lcdArea.getY(), iconPill, iconPill);
        x += iconPill + iconGap;
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

Rectangle<float> UnitComponent::statusPillArea() const
{
    const auto plate = plateBounds();
    if (isCompact())
    {
        const auto live = faceplate::liveArea (type, plate);
        return live.withSizeKeepingCentre (jmin (live.getWidth() - 8.0f, 236.0f), 22.0f);
    }
    const auto knobs = faceplate::knobArea (type, plate);
    const float x0 = knobs.getX();
    const float x1 = jmax (x0 + 140.0f, (float) msBtn.getX() - 10.0f);
    return { x0, (float) msBtn.getY(), jmin (300.0f, x1 - x0), (float) msBtn.getHeight() };
}

void UnitComponent::paint (Graphics& g)
{
    const auto plate = plateBounds();
    faceplate::paintFaceplate (g, type, plate, state, false);

    // SPECTRAL VIEW (hovered card only): subtle, inside the card behind the knobs
    {
        const auto ka = faceplate::knobArea (type, plate).expanded (4.0f, 0.0f);
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (roundedCard (ka, 12.0f));
        owner.paintSpectrum (g, slot, ka, 0.4f);
    }

    // hairline in front of DRY/WET
    if (controls.size() > 1 && ! (isCompact() && idealHeight (type, getWidth()) > 200))
    {
        const auto mb = controls.getLast()->getBounds().toFloat();
        const float x = mb.getX() - 7.0f;
        ColourGradient div (cs::ice.withAlpha (0.0f), 0.0f, mb.getY() + 8.0f, cs::ice.withAlpha (0.0f), 0.0f, mb.getBottom() - 8.0f, false);
        div.addColour (0.5, cs::ice.withAlpha (0.16f));
        g.setGradientFill (div);
        g.fillRect (x, mb.getY() + 8.0f, 1.0f, mb.getHeight() - 16.0f);
    }
}

void UnitComponent::paintOverChildren (Graphics& g)
{
    const auto plate = plateBounds();
    const float rad = cs::radius;

    // hover: the card's outline lights up in its accent (the outer glow is painted by the RackView)
    if (owner.getHoveredSlot() == slot && ! lifted)
    {
        g.setColour (pal.accent.withAlpha (0.42f));
        g.drawRoundedRectangle (plate.reduced (0.5f), rad - 0.5f, 1.0f);
    }

    // SPLIT branch tag (compact cards, left of the preset pill)
    if (const int br = proc.getSlotBranch (slot); br >= 0 && isCompact())
    {
        auto tag = Rectangle<float> (18.0f, (float) lcd.getY(), 22.0f, 22.0f);
        g.setColour (cs::bg);
        g.fillEllipse (tag);
        g.setColour (cs::iceDeep.withAlpha (0.45f));
        g.fillEllipse (tag.reduced (2.0f));
        g.setColour (cs::ice.withAlpha (0.8f));
        g.drawEllipse (tag.reduced (0.5f), 1.0f);
        g.setFont (cs::monoFont (10.0f, true));
        g.setColour (Colours::white);
        g.drawText (br == 0 ? "A" : "B", tag, Justification::centred, false);
    }

    // status: POWER off / dimmed by SOLO
    const bool audition = proc.getLevelMatchedAudition() == slot;
    if ((! state.powered || state.soloDimmed) && ! audition)
    {
        const bool off = ! state.powered;
        tagPill (g, statusPillArea(), off ? String::fromUTF8 ("POWER OFF  \xc2\xb7  TAILS KEEP RUNNING") : String ("MUTED BY SOLO"),
                 off ? cs::muted : soloGold, off ? cs::text2 : soloGold.interpolatedWith (Colours::white, 0.3f), 8.0f,
                 off ? removeRed : soloGold);
    }
    // LEVEL-MATCHED POWER readout while the POWER button is held
    if (audition)
    {
        const auto r = statusPillArea();
        glowAround (g, r, r.getHeight() * 0.5f, cs::ice, 1.0f);
        tagPill (g, r, "LEVEL-MATCHED BYPASS  " + String (proc.getAuditionGainDb(), 1) + " dB", cs::ice, Colours::white, 8.2f, cs::ice);
    }

    // KILL: an ice-blue glow sweeps across the card
    if (flash > 0.0f)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (roundedCard (plate, rad));
        const float t = 1.0f - flash;
        const float bandW = jmax (160.0f, plate.getWidth() * 0.32f);
        const float cx = plate.getX() - bandW + t * (plate.getWidth() + bandW * 2.0f);
        ColourGradient band (cs::ice.withAlpha (0.0f), cx - bandW * 0.5f, 0.0f, cs::ice.withAlpha (0.0f), cx + bandW * 0.5f, 0.0f, false);
        band.addColour (0.5, cs::ice.withAlpha (0.30f * jmin (1.0f, flash * 1.6f)));
        g.setGradientFill (band);
        g.fillRect (plate);
        g.setColour (cs::iceDeep.withAlpha (0.12f * flash));
        g.fillRect (plate);
        g.setColour (cs::ice.withAlpha (0.75f * flash));
        g.drawRoundedRectangle (plate.reduced (1.0f), rad - 1.0f, 1.5f);
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
                    if (res != 1) return;   // 0 also when the editor closed the dialog (w / p may be gone)
                    const auto name = w->getTextEditorContents ("name").trim();
                    if (name.isNotEmpty()) p.saveSlotPreset (s, name);
                }), true);
                ui::trackDialog (w, &p);   // closed with the editor (ui/SharedResources)
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
    setOpaque (true);
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
        if (auto* u = unitForSlot (s))
        {
            u->repaint();
            repaint (u->getBounds().expanded (16));   // outer hover glow
        }
}

void RackView::paintSpectrum (Graphics& g, int slot, Rectangle<float> area, float opacity) const
{
    if (spectrum != nullptr && slot >= 0 && spectrum->getSlot() == slot)
        spectrum->paint (g, area, opacity);
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

int RackView::unitX() const { return gutter; }
int RackView::unitWidth() const { return jmax (600, (getWidth() > 0 ? getWidth() : 852) - gutter - 2); }

float RackView::lineX (int branch) const
{
    if (branch == 1)
    {
        const auto L = computeLayout();
        return (float) (L.xB - gutter / 2);
    }
    return (float) gutter * 0.5f;
}

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
        gapRect = {};
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
        targetX.assign ((size_t) units.size(), unitX());
        targetW.assign ((size_t) units.size(), unitWidth());
        targetH.assign ((size_t) units.size(), UnitComponent::height);
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

RackView::Layout RackView::computeLayout() const
{
    Layout L;
    const int X = unitX(), W = unitWidth();
    int y = topMargin;
    if (! proc.hasSplit())
    {
        for (auto* u : units) y += UnitComponent::idealHeight (u->getType(), W) + gapY;
        L.bottom = y;
        return L;
    }
    for (auto* u : units)
        if (proc.getSlotBranch (u->getSlot()) < 0) y += UnitComponent::idealHeight (u->getType(), W) + gapY;
    L.splitY = y;
    y += SplitUnit::height + splitGap;
    L.colTop = y;
    L.colW = (W - gutter) / 2;
    L.xA = X;
    L.xB = X + L.colW + gutter;
    int yA = y, yB = y;
    for (auto* u : units)
    {
        const int br = proc.getSlotBranch (u->getSlot());
        if (br < 0) continue;
        (br == 0 ? yA : yB) += UnitComponent::idealHeight (u->getType(), L.colW) + gapY;
    }
    L.emptyA = yA == y;
    L.emptyB = yB == y;
    if (L.emptyA) yA = y + emptyBranchHeight + gapY;
    if (L.emptyB) yB = y + emptyBranchHeight + gapY;
    L.colBottom = jmax (yA, yB) - gapY;
    L.mergeY = L.colBottom + mergeGap / 2;
    L.bottom = L.colBottom + mergeGap;
    return L;
}

int RackView::contentBottom() const { return computeLayout().bottom; }

int RackView::branchColumnAt (Point<int> pos) const
{
    const auto L = computeLayout();
    return pos.x < L.xB - gutter / 2 ? 0 : 1;
}

int RackView::getIdealHeight() const
{
    return contentBottom() + (isFull() ? 64 : addZoneHeight) + 12;
}

Rectangle<int> RackView::addZone() const
{
    const int y = contentBottom();
    const int h = jmax (isFull() ? 56 : 90, jmin (isFull() ? 56 : addZoneHeight, getHeight() - y - 10));
    return { unitX(), y, unitWidth(), h };
}

void RackView::resized()
{
    layoutTargets (true);
}

void RackView::layoutTargets (bool snap)
{
    const int n = units.size();
    targetX.assign ((size_t) n, unitX());
    targetW.assign ((size_t) n, unitWidth());
    targetH.assign ((size_t) n, UnitComponent::height);
    if (targetY.size() != (size_t) n) { targetY.assign ((size_t) n, 0.0f); animY.assign ((size_t) n, 0.0f); }
    const auto L = computeLayout();

    if (proc.hasSplit())
    {
        // serial cards, then the SPLIT strip, then branch A (left) | branch B (right)
        int y = topMargin;
        for (int i = 0; i < n; ++i)
            if (proc.getSlotBranch (units[i]->getSlot()) < 0)
            {
                targetH[(size_t) i] = UnitComponent::idealHeight (units[i]->getType(), unitWidth());
                targetY[(size_t) i] = (float) y;
                y += targetH[(size_t) i] + gapY;
            }
        splitY = L.splitY;
        splitUnit->setBounds (unitX(), L.splitY, unitWidth(), SplitUnit::height);
        int yA = L.colTop, yB = L.colTop;
        for (int i = 0; i < n; ++i)
        {
            const int br = proc.getSlotBranch (units[i]->getSlot());
            if (br < 0) continue;
            targetW[(size_t) i] = L.colW;
            targetX[(size_t) i] = br == 0 ? L.xA : L.xB;
            targetH[(size_t) i] = UnitComponent::idealHeight (units[i]->getType(), L.colW);
            targetY[(size_t) i] = (float) (br == 0 ? yA : yB);
            (br == 0 ? yA : yB) += targetH[(size_t) i] + gapY;
        }
        gapRect = {};
        for (int i = 0; i < n; ++i)
        {
            if (snap || reduceMotion) animY[(size_t) i] = targetY[(size_t) i];
            units[i]->setBounds (targetX[(size_t) i], roundToInt (animY[(size_t) i]), targetW[(size_t) i], targetH[(size_t) i]);
        }
        return;
    }
    splitY = -1;

    int gapIndex = drop.kind == DropTarget::Move ? drop.gap : -1;
    int dragH = UnitComponent::height;
    if (draggingSlot >= 0)
    {
        int k = 0;
        for (auto* u : units)
        {
            if (u->getSlot() == draggingSlot)
            {
                dragH = UnitComponent::idealHeight (u->getType(), unitWidth());
                if (gapIndex < 0) gapIndex = k;   // no target yet: keep the gap where the card was
                break;
            }
            ++k;
        }
    }
    gapRect = {};
    int j = 0, y = topMargin;
    for (int i = 0; i < n; ++i)
    {
        auto* u = units[i];
        targetH[(size_t) i] = UnitComponent::idealHeight (u->getType(), unitWidth());
        if (u->getSlot() == draggingSlot) continue;
        if (draggingSlot >= 0 && j == gapIndex)
        {
            gapRect = { unitX(), y, unitWidth(), dragH };
            y += dragH + gapY;
        }
        targetY[(size_t) i] = (float) y;
        y += targetH[(size_t) i] + gapY;
        ++j;
    }
    if (draggingSlot >= 0 && gapRect.isEmpty())
        gapRect = { unitX(), y, unitWidth(), dragH };
    for (int i = 0; i < n; ++i)
    {
        if (snap || reduceMotion) animY[(size_t) i] = targetY[(size_t) i];
        units[i]->setBounds (unitX(), roundToInt (animY[(size_t) i]), unitWidth(), targetH[(size_t) i]);
    }
}

float RackView::signalLevel() const
{
    float l = 0.0f;
    for (auto* u : units) l = jmax (l, u->getLevel());
    return l;
}

void RackView::tick()
{
    const double now = clock();

    // KILL: cards get an ice-blue glow sweep and slide into the chain
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
            const float local = (t - 0.03f * (float) i) / 0.2f;
            const float e = easeOut (local);
            u->setTransform (AffineTransform::translation ((1.0f - e) * 56.0f, 0.0f));
            if (! u->isLifted()) u->setAlpha (0.3f + 0.7f * e);
            u->setFlash (1.0f - jlimit (0.0f, 1.0f, (t - 0.03f * (float) i) / 0.42f));
        }
        else if (! u->getTransform().isIdentity())
        {
            u->setTransform ({});
            if (! u->isLifted()) u->setAlpha (1.0f);
            u->setFlash (0.0f);
        }
    }
    if (killAnimStart >= 0.0 && now - killAnimStart > 0.03 * units.size() + 0.5)
        killAnimStart = -1.0;

    // unit drag finished somewhere else (dropped outside, Escape)
    if (draggingSlot >= 0)
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this); c == nullptr || ! c->isDragAndDropActive())
            endUnitDrag();

    // smooth movement towards the layout targets
    bool moving = false;
    for (int i = 0; i < units.size(); ++i)
    {
        float& y = animY[(size_t) i];
        const float tgt = targetY[(size_t) i];
        if (std::abs (tgt - y) > 0.5f && ! reduceMotion) { y += (tgt - y) * 0.36f; moving = true; }
        else y = tgt;
        const int iy = roundToInt (y);
        const int ix = i < (int) targetX.size() ? targetX[(size_t) i] : unitX();
        if (units[i]->getY() != iy || units[i]->getX() != ix) units[i]->setTopLeftPosition (ix, iy);
    }
    if (moving) repaint();   // the signal line nodes follow the cards

    // SPECTRAL VIEW: follow the hovered card (polled, so the knobs on top do not interrupt it)
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
            u->repaint (faceplate::knobArea (u->getType(), u->plateBounds()).expanded (4.0f).getSmallestIntegerContainer());

    if (splitUnit->isVisible()) splitUnit->tick();
    for (auto* u : units) u->tick (now);

    // SIGNAL LINE: pulse while audio flows (static brightness with reduce motion)
    const float lvl = signalLevel();
    const bool animate = lvl > 0.004f && ! reduceMotion;
    if (animate || lineAnimating || std::abs (lvl - shownLevel) > 0.05f)
    {
        shownLevel = lvl;
        repaint (0, 0, gutter, getHeight());
        if (proc.hasSplit())
        {
            const auto L = computeLayout();
            repaint (L.xB - gutter, L.splitY, gutter, L.bottom - L.splitY);
        }
    }
    lineAnimating = animate;
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
void RackView::paintSignalLine (Graphics& g, const Layout& L)
{
    const float lx = lineX (0);
    const float lvl = jlimit (0.0f, 1.0f, shownLevel * 2.5f);
    const auto zone = addZone().toFloat();
    const float yEnd = zone.getY() + 34.0f;
    const Colour lineCol = cs::ice.withAlpha (0.42f + 0.3f * lvl);

    struct Seg { float x, y0, y1; };
    std::vector<Seg> segs;
    Path line;
    float xb = 0.0f, forkY = 0.0f;
    if (L.splitY >= 0)
    {
        xb = (float) (L.xB - gutter / 2);
        forkY = (float) (L.splitY + SplitUnit::height) + splitGap * 0.5f;
        const float mergeY = (float) L.mergeY;
        const float rr = 8.0f;
        line.startNewSubPath (lx, 0.0f);
        line.lineTo (lx, yEnd);
        // branch B: fork right under the SPLIT strip, run down between the columns, merge back
        line.startNewSubPath (lx, forkY);
        line.lineTo (xb - rr, forkY);
        line.quadraticTo (xb, forkY, xb, forkY + rr);
        line.lineTo (xb, mergeY - rr);
        line.quadraticTo (xb, mergeY, xb - rr, mergeY);
        line.lineTo (lx, mergeY);
        segs.push_back ({ lx, 0.0f, yEnd });
        segs.push_back ({ xb, forkY + rr, mergeY - rr });
    }
    else
    {
        line.startNewSubPath (lx, 0.0f);
        line.lineTo (lx, yEnd);
        segs.push_back ({ lx, 0.0f, yEnd });
    }
    g.setColour (cs::iceDeep.withAlpha (0.16f + 0.18f * lvl));
    g.strokePath (line, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
    cs::strokeGlow (g, line, lineCol, 1.6f, 0.8f + 0.6f * lvl);

    // pulse travelling down (two blobs, half a period apart)
    if (lvl > 0.01f)
    {
        const float len = 150.0f;
        auto blob = [&] (float py, float a)
        {
            for (const auto& s : segs)
            {
                const float y0 = jmax (s.y0, py - len * 0.5f), y1 = jmin (s.y1, py + len * 0.5f);
                if (y1 <= y0) continue;
                Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (Rectangle<float> (s.x - 8.0f, y0, 16.0f, y1 - y0).getSmallestIntegerContainer());
                ColourGradient cg (cs::ice.withAlpha (0.0f), 0.0f, py - len * 0.5f, cs::ice.withAlpha (0.0f), 0.0f, py + len * 0.5f, false);
                cg.addColour (0.62, cs::ice.withAlpha (0.55f * a));
                cg.addColour (0.75, Colours::white.withAlpha (0.9f * a));
                cg.addColour (0.82, cs::ice.withAlpha (0.4f * a));
                g.setGradientFill (cg);
                ColourGradient halo (cg);
                halo.multiplyOpacity (0.25f);
                g.setGradientFill (halo);
                g.fillRoundedRectangle (Rectangle<float> (s.x - 6.0f, py - len * 0.5f, 12.0f, len), 6.0f);
                g.setGradientFill (cg);
                g.fillRoundedRectangle (Rectangle<float> (s.x - 1.5f, py - len * 0.5f, 3.0f, len), 1.5f);
            }
        };
        if (reduceMotion)
        {
            // static: the whole line simply glows brighter with the level (drawn above)
        }
        else
        {
            const float period = 1.7f;
            const float span = yEnd + len;
            const float ph = (float) std::fmod (clock() / period, 1.0);
            blob (-len * 0.5f + ph * span, lvl);
            blob (-len * 0.5f + std::fmod (ph + 0.5f, 1.0f) * span, lvl * 0.7f);
        }
    }

    // a node per card (ring + accent core), joined to the card's left edge
    auto node = [&] (Point<float> c, float r, Colour core, float lit, float connectTo)
    {
        if (connectTo > c.x + r)
        {
            ColourGradient cg (cs::ice.withAlpha (0.75f), c.x, c.y, cs::ice.withAlpha (0.25f), connectTo, c.y, false);
            g.setGradientFill (cg);
            g.fillRect (c.x + r, c.y - 0.5f, connectTo - c.x - r, 1.0f);
        }
        ColourGradient halo (cs::iceDeep.withAlpha (0.45f * (0.4f + lit)), c.x, c.y, cs::iceDeep.withAlpha (0.0f), c.x + r * 3.0f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (r * 6.0f, r * 6.0f).withCentre (c));
        g.setColour (cs::bg);
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        g.setColour (cs::ice.withAlpha (0.55f + 0.4f * lit));
        g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c).reduced (0.5f), 1.2f);
        if (! core.isTransparent()) cs::dotGlow (g, c, r * 0.42f, core, lit);
    };
    for (auto* u : units)
    {
        if (u->isLifted()) continue;
        const auto b = u->getBoundsInParent().toFloat();
        const int br = proc.getSlotBranch (u->getSlot());
        const float x = br == 1 ? xb : lx;
        const float y = b.getY() + (u->isCompact() ? 26.0f : 30.0f);
        node ({ x, y }, 5.0f, u->getAccent(), u->isPowered() ? 0.45f + 0.55f * jlimit (0.0f, 1.0f, u->getLevel() * 3.0f) : 0.0f, b.getX());
    }
    if (L.splitY >= 0)
    {
        node ({ lx, (float) L.splitY + SplitUnit::height * 0.5f }, 6.0f, cs::ice, 0.6f, (float) unitX());
        // fork + merge nodes
        node ({ lx, forkY }, 4.0f, {}, 0.6f, 0.0f);
        node ({ lx, (float) L.mergeY }, 6.5f, cs::ice, 0.7f, 0.0f);
        tagPill (g, Rectangle<float> (72.0f, 18.0f).withCentre ({ (lx + xb) * 0.5f, (float) L.mergeY }), "MERGE  A + B",
                 cs::ice, cs::text, 7.6f);
        // branch labels where the line enters each column
        for (int br = 0; br < 2; ++br)
        {
            const float x = br == 0 ? lx : xb;
            auto tag = Rectangle<float> (14.0f, 14.0f).withCentre ({ x, (float) L.colTop + 4.0f });
            g.setColour (cs::bg);
            g.fillEllipse (tag);
            g.setColour (cs::ice.withAlpha (0.7f));
            g.drawEllipse (tag.reduced (0.5f), 1.0f);
            g.setFont (cs::monoFont (7.5f, true));
            g.setColour (cs::text);
            g.drawText (br == 0 ? "A" : "B", tag, Justification::centred, false);
        }
    }
    // the chain's end: a sparkle on the line
    cs::paintSparkle (g, { lx, yEnd }, 6.0f, cs::ice, 1.0f);
}

void RackView::paintAddZone (Graphics& g)
{
    const auto z = addZone().toFloat();
    const bool full = isFull();
    const auto inner = z.reduced (0.0f, 4.0f);
    const bool hot = drop.kind == DropTarget::Append || (addHover && ! full);
    const float rad = cs::radius;
    if (hot) glowAround (g, inner, rad, cs::iceDeep, 1.0f);
    g.setColour (hot ? cs::iceDeep.withAlpha (0.10f) : cs::card.withAlpha (0.35f));
    g.fillRoundedRectangle (inner, rad);
    dashedRound (g, inner.reduced (0.5f), rad, hot ? cs::ice.withAlpha (0.85f) : cs::ice.withAlpha (full ? 0.14f : 0.26f), hot ? 1.4f : 1.1f);

    if (full)
    {
        g.setFont (cs::monoFont (9.5f, true));
        g.setColour (cs::muted);
        g.drawText (String::fromUTF8 ("CHAIN FULL  \xe2\x80\x93  DROP ON A CARD TO REPLACE"), inner, Justification::centred, false);
        return;
    }
    auto t = inner.withSizeKeepingCentre (inner.getWidth(), 50.0f);
    auto title = t.removeFromTop (30.0f);
    {
        Graphics::ScopedSaveState ss (g);
        if (! hot) g.setOpacity (0.72f);
        chromeCentred (g, "DROP EFFECTS HERE", title, 21.0f, hot ? 1.0f : 0.4f);
    }
    const Font f = cs::monoFont (8.8f);
    const String sub ("OR CLICK TO ADD");
    const float sw = GlyphArrangement::getStringWidth (f, sub);
    auto subR = t.withSizeKeepingCentre (sw + 22.0f, 16.0f).translated (0.0f, 2.0f);
    cs::paintSparkle (g, { subR.getX() + 5.0f, subR.getCentreY() }, 4.5f, hot ? cs::ice : cs::ice.withAlpha (0.6f), hot ? 1.0f : 0.4f);
    g.setFont (f);
    g.setColour (hot ? cs::text : cs::muted);
    g.drawText (sub, subR.withTrimmedLeft (16.0f), Justification::centredLeft, false);
}

void RackView::paint (Graphics& g)
{
    g.fillAll (cs::bg);
    // very subtle deep-blue haze behind the chain (the site's radial background)
    {
        const float w = (float) getWidth();
        ColourGradient haze (cs::iceDeep.withAlpha (0.07f), w * 0.55f, 0.0f, cs::iceDeep.withAlpha (0.0f), w * 0.55f, w * 0.6f, true);
        g.setGradientFill (haze);
        g.fillRect (getLocalBounds());
    }
    const auto L = computeLayout();

    // empty SPLIT branch placeholders
    if (L.splitY >= 0)
        for (int br = 0; br < 2; ++br)
            if (br == 0 ? L.emptyA : L.emptyB)
            {
                const auto r = Rectangle<float> ((float) (br == 0 ? L.xA : L.xB), (float) L.colTop, (float) L.colW, (float) emptyBranchHeight);
                g.setColour (cs::card.withAlpha (0.3f));
                g.fillRoundedRectangle (r, cs::radius);
                dashedRound (g, r.reduced (0.5f), cs::radius, cs::ice.withAlpha (0.22f), 1.0f);
                g.setFont (cs::monoFont (8.5f, true));
                g.setColour (cs::muted);
                g.drawText (String (br == 0 ? "BRANCH A" : "BRANCH B") + String::fromUTF8 ("  \xc2\xb7  DROP EFFECTS HERE"), r, Justification::centred, false);
            }

    if (g.clipRegionIntersects (addZone().expanded (16)))
        paintAddZone (g);
    paintSignalLine (g, L);

    // hovered card: soft outer glow in its accent
    if (auto* u = unitForSlot (hoverSlot); u != nullptr && ! u->isLifted() && draggingSlot < 0)
        glowAround (g, u->getBoundsInParent().toFloat(), cs::radius, u->getAccent().withAlpha (0.8f), 0.8f);

    // where a lifted card will land
    if (draggingSlot >= 0 && ! gapRect.isEmpty())
    {
        const auto r = gapRect.toFloat();
        if (drop.kind == DropTarget::None)
        {
            // dragged out of the chain: hint how to remove it
            glowAround (g, r, cs::radius, removeRed, 0.9f);
            g.setColour (removeRed.withAlpha (0.06f));
            g.fillRoundedRectangle (r, cs::radius);
            dashedRound (g, r.reduced (0.5f), cs::radius, removeRed.withAlpha (0.7f), 1.3f);
            g.setFont (cs::monoFont (11.0f, true));
            g.setColour (removeRed.interpolatedWith (Colours::white, 0.35f));
            g.drawText ("DROP ON THE FX LIBRARY TO REMOVE", r, Justification::centred, false);
        }
        else
        {
            glowAround (g, r, cs::radius, cs::iceDeep, 0.9f);
            g.setColour (cs::iceDeep.withAlpha (0.08f));
            g.fillRoundedRectangle (r, cs::radius);
            dashedRound (g, r.reduced (0.5f), cs::radius, cs::ice.withAlpha (0.6f), 1.2f);
        }
    }
}

void RackView::paintOverChildren (Graphics& g)
{
    const float ux = (float) unitX(), uw = (float) unitWidth();
    if (drop.kind == DropTarget::Insert)
    {
        const float x0 = drop.w > 0 ? (float) drop.x : ux;
        const float x1 = drop.w > 0 ? (float) (drop.x + drop.w) : ux + uw;
        const float lx = lineX (drop.branch == 1 ? 1 : 0);
        Path p;
        p.startNewSubPath (lx, drop.y);
        p.lineTo (x1 - 10.0f, drop.y);
        cs::strokeGlow (g, p, cs::ice, 2.4f, 1.6f);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.fillRoundedRectangle (Rectangle<float> (x0 + 12.0f, drop.y - 0.6f, x1 - x0 - 34.0f, 1.2f), 0.6f);
        // "+" node on the signal line
        const auto c = Point<float> (lx, drop.y);
        cs::dotGlow (g, c, 6.5f, cs::iceDeep, 1.0f);
        g.setColour (cs::bg);
        g.fillEllipse (Rectangle<float> (13.0f, 13.0f).withCentre (c));
        g.setColour (cs::ice);
        g.drawEllipse (Rectangle<float> (13.0f, 13.0f).withCentre (c), 1.3f);
        g.fillRect (Rectangle<float> (7.0f, 1.4f).withCentre (c));
        g.fillRect (Rectangle<float> (1.4f, 7.0f).withCentre (c));
    }
    else if ((drop.kind == DropTarget::Append || drop.kind == DropTarget::Move) && drop.branch >= 0 && drop.w > 0)
    {
        // SPLIT: highlight the target column
        const auto L = computeLayout();
        auto col = Rectangle<float> ((float) drop.x, (float) L.colTop, (float) drop.w, (float) (L.colBottom - L.colTop)).expanded (3.0f);
        glowAround (g, col, cs::radius, cs::iceDeep, 1.0f);
        g.setColour (cs::iceDeep.withAlpha (0.08f));
        g.fillRoundedRectangle (col, cs::radius);
        dashedRound (g, col.reduced (0.5f), cs::radius, cs::ice.withAlpha (0.8f), 1.3f);
        tagPill (g, Rectangle<float> (92.0f, 20.0f).withCentre ({ col.getCentreX(), col.getY() + 2.0f }),
                 drop.branch == 0 ? "BRANCH A" : "BRANCH B", cs::ice, Colours::white, 8.5f);
    }
    else if (drop.kind == DropTarget::Replace)
    {
        if (auto* u = unitForSlot (drop.slot))
        {
            const auto b = u->getBoundsInParent().toFloat();
            const auto acc = u->getAccent();
            glowAround (g, b, cs::radius, acc, 1.0f);
            g.setColour (acc.withAlpha (0.08f));
            g.fillRoundedRectangle (b, cs::radius);
            Path outline;
            outline.addRoundedRectangle (b.reduced (1.0f), cs::radius - 1.0f);
            cs::strokeGlow (g, outline, acc, 2.0f, 1.0f);
            tagPill (g, Rectangle<float> (104.0f, 24.0f).withCentre (b.getCentre()), "REPLACE", acc,
                     acc.interpolatedWith (Colours::white, 0.6f), 9.5f, acc);
        }
    }
}

void RackView::mouseMove (const MouseEvent& e)
{
    const bool h = ! isFull() && addZone().contains (e.getPosition());
    if (h != addHover) { addHover = h; repaint (addZone().expanded (14)); setMouseCursor (h ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor); }
}

void RackView::mouseExit (const MouseEvent&)
{
    if (addHover) { addHover = false; repaint (addZone().expanded (14)); setMouseCursor (MouseCursor::NormalCursor); }
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

void RackView::previewDropState (int kind, int slot, int gap)
{
    for (auto* u : units) u->setLifted (false);
    draggingSlot = -1;
    drop = {};
    if (auto* u = unitForSlot (slot))
    {
        const auto b = u->getBounds();
        switch (kind)
        {
            case 1: drop.kind = DropTarget::Insert; drop.slot = slot; drop.y = (float) b.getY() - gapY * 0.5f;
                    if (const int br = proc.getSlotBranch (slot); br >= 0) { drop.branch = br; drop.x = b.getX(); drop.w = b.getWidth(); }
                    break;
            case 2: drop.kind = DropTarget::Replace; drop.slot = slot; break;
            case 3: draggingSlot = slot; u->setLifted (true); drop.kind = DropTarget::Move; drop.gap = gap; break;
            case 4: draggingSlot = slot; u->setLifted (true); break;
            default: break;
        }
    }
    layoutTargets (true);
    repaint();
}

// ----------------------------------------------------------------------------
namespace { int grabY = 0; }

void RackView::beginUnitDrag (UnitComponent& u, const MouseEvent& e)
{
    auto* c = DragAndDropContainer::findParentDragContainerFor (this);
    if (c == nullptr || c->isDragAndDropActive()) return;
    constexpr int margin = 22;
    auto img = u.createLiftImage (margin);
    const auto grab = e.getMouseDownPosition();
    grabY = grab.y;
    Point<int> offset (-(grab.x + margin), -(grab.y + margin));
    draggingSlot = u.getSlot();
    drop = {};
    c->startDragging (dnd::unit (u.getSlot()), &u, ScaledImage (img, 1.0), true, &offset, &e.source);
    u.setLifted (true);
    drop.kind = DropTarget::Move;   // the card starts inside the chain: show its gap, not the remove hint
    drop.gap = -1;
    layoutTargets (false);
    repaint();
}

void RackView::endUnitDrag()
{
    for (auto* u : units) u->setLifted (false);
    draggingSlot = -1;
    gapRect = {};
    if (drop.kind == DropTarget::Move) drop = {};
    layoutTargets (false);
    repaint();
}

RackView::DropTarget RackView::dropTargetFor (Point<int> pos, bool fromUnit, int fromSlot) const
{
    DropTarget d;
    if (proc.hasSplit())
    {
        const auto L = computeLayout();
        // SPLIT layout: hit-test the cards (two columns), otherwise the column under the mouse
        for (auto* u : units)
        {
            if (u->getSlot() == fromSlot && fromUnit) continue;
            const auto b = u->getBounds();
            if (! b.expanded (0, gapY / 2).contains (pos)) continue;
            const int s = u->getSlot();
            const int br = proc.getSlotBranch (s);
            d.branch = br;
            if (br >= 0) { d.x = b.getX(); d.w = b.getWidth(); }
            if (fromUnit)
            {
                // drop a card on another one: take its place (same branch) / move into that branch
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
        if (pos.y >= L.colTop - splitGap / 2)
        {
            const int col = branchColumnAt (pos);
            d.kind = fromUnit ? DropTarget::Move : DropTarget::Append;
            d.branch = col;
            d.x = col == 0 ? L.xA : L.xB;
            d.w = L.colW;
            return d;
        }
        if (fromUnit) return d; // above the split: no reordering across it from here (use the unit menu)
        d.kind = DropTarget::Insert;
        d.slot = proc.getSplitPosition();
        d.y = (float) L.splitY - gapY * 0.5f;
        return d;
    }
    if (fromUnit)
    {
        // where would the lifted card land? use its centre, not the mouse
        int dragH = UnitComponent::height;
        for (auto* u : units) if (u->getSlot() == fromSlot) dragH = UnitComponent::idealHeight (u->getType(), unitWidth());
        const float centre = (float) (pos.y - grabY) + (float) dragH * 0.5f;
        d.kind = DropTarget::Move;
        int gap = 0, y = topMargin;
        for (auto* u : units)
        {
            if (u->getSlot() == fromSlot) continue;
            const int h = UnitComponent::idealHeight (u->getType(), unitWidth());
            if (centre > (float) y + (float) h * 0.5f) ++gap;
            y += h + gapY;
        }
        d.gap = gap;
        return d;
    }

    for (int i = 0; i < units.size(); ++i)
    {
        const auto b = units[i]->getBounds();
        if (pos.y > b.getBottom() + gapY / 2) continue;
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
                if (isFull()) { say ("CHAIN FULL - DROP ON A CARD TO REPLACE IT"); return; }
                if (target.branch >= 0) proc.insertEffectInBranchAt (target.slot, target.branch, t);
                else proc.insertEffect (target.slot, t);
                return;
            case DropTarget::Append:
                if (isFull()) { say ("CHAIN FULL - DROP ON A CARD TO REPLACE IT"); return; }
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
