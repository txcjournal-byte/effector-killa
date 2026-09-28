#include "Shelf.h"

namespace ek::ui
{
using namespace juce;

CassetteShelf::CassetteShelf (EffectorKillaAudioProcessor& p) : proc (p) {}

Rectangle<float> CassetteShelf::cassetteRect (int slot) const
{
    return layout::cassette (slot).translated (-layout::shelf.getX(), -layout::shelf.getY());
}

int CassetteShelf::slotAt (Point<float> p) const
{
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto r = cassetteRect (i).expanded ((layout::cassetteStep - layout::cassetteW) * 0.5f, 0.0f);
        if (r.contains (p)) return i;
    }
    return -1;
}

int CassetteShelf::dropIndexFor (float x) const
{
    const float first = cassetteRect (0).getCentreX();
    return jlimit (0, kNumSlots - 1, (int) std::lround ((x - first) / layout::cassetteStep));
}

void CassetteShelf::tick()
{
    bool changed = false;
    for (int i = 0; i < kNumSlots; ++i)
    {
        const float target = (proc.selectedSlot == i) ? 1.0f : 0.0f;
        const float v = slide[(size_t) i] + (target - slide[(size_t) i]) * 0.35f;
        const float nv = std::abs (v - target) < 0.01f ? target : v;
        if (nv != slide[(size_t) i]) { slide[(size_t) i] = nv; changed = true; }
    }
    if (changed) repaint();
}

void CassetteShelf::drawCassette (Graphics& g, Rectangle<float> r, const SlotState& s, int slotIndex,
                                  bool selected, bool screening, float glow)
{
    if (s.isEmpty()) return;
    const auto& info = effectInfo (s.type);
    const bool paused = s.pause;

    // warm light behind a selected cassette
    if (glow > 0.01f)
    {
        ColourGradient light (theme::amber.withAlpha (0.45f * glow), r.getCentreX(), r.getBottom(),
                              theme::amber.withAlpha (0.0f), r.getCentreX(), r.getY() - 40.0f, false);
        g.setGradientFill (light);
        g.fillRect (r.expanded (22.0f, 10.0f));
    }

    // black VHS case (spine)
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (3.0f, 4.0f), 4.0f);
    ColourGradient body (Colour (0xff2a2724), r.getX(), r.getY(), Colour (0xff0d0c0b), r.getRight(), r.getY(), false);
    body.addColour (0.15, Colour (0xff3a3632));
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::white.withAlpha (0.13f));
    g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.2f);
    // moulded ridges top & bottom
    g.setColour (Colours::black.withAlpha (0.5f));
    for (float y : { r.getY() + 12.0f, r.getY() + 20.0f, r.getBottom() - 16.0f })
        g.drawLine (r.getX() + 6.0f, y, r.getRight() - 6.0f, y, 1.5f);

    // cream label
    auto label = Rectangle<float> (r.getX() + 7.0f, r.getY() + 30.0f, r.getWidth() - 14.0f, r.getHeight() - 58.0f);
    const Colour paper = paused ? Colour (0xff9a958c) : (selected ? Colour (0xfffff0c8) : theme::creamLabel);
    ColourGradient lab (paper.brighter (0.08f), label.getX(), label.getY(), paper.darker (0.12f), label.getRight(), label.getBottom(), false);
    g.setGradientFill (lab);
    g.fillRoundedRectangle (label, 2.0f);
    if (selected)
    {
        g.setColour (Colour (0xffffd98a).withAlpha (0.35f * glow));
        g.fillRoundedRectangle (label, 2.0f);
    }
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (label, 2.0f, 1.0f);

    // handwritten name, bottom-to-top
    {
        Graphics::ScopedSaveState ss (g);
        const auto c = label.getCentre().translated (0.0f, -8.0f);
        g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, c.x, c.y));
        auto textArea = Rectangle<float> (label.getHeight() - 30.0f, label.getWidth()).withCentre (c);
        g.setColour (paused ? theme::ink.withAlpha (0.45f) : theme::ink);
        g.setFont (theme::handFont (info.name.length() > 12 ? 20.0f : 23.0f));
        g.drawFittedText (info.name, textArea.toNearestInt(), Justification::centred, 1, 0.6f);
    }

    // coloured dot = effect type
    const Point<float> dot (label.getCentreX(), label.getBottom() - 13.0f);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (Rectangle<float> (15.0f, 15.0f).withCentre (dot.translated (0.5f, 1.0f)));
    g.setColour (paused ? info.colour.withSaturation (0.15f) : info.colour);
    g.fillEllipse (Rectangle<float> (13.0f, 13.0f).withCentre (dot));
    g.setColour (Colours::white.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre (dot.translated (-2.0f, -2.5f)));

    // M/S and SCREENING markers on the top edge
    if (s.ms != MSMode::Stereo || screening)
    {
        g.setFont (theme::stencilFont (13.0f));
        g.setColour (screening ? theme::amber : theme::cream.withAlpha (0.8f));
        const String t = screening ? "SCREEN" : (s.ms == MSMode::Mid ? "MID" : "SIDE");
        g.drawText (t, Rectangle<float> (r.getX(), r.getY() + 1.0f, r.getWidth(), 12.0f), Justification::centred);
    }

    // WRITE PROTECT tag
    if (s.writeProtect)
    {
        Graphics::ScopedSaveState ss (g);
        const Point<float> c (r.getX() + 16.0f, r.getY() + 24.0f);
        g.addTransform (AffineTransform::rotation (-0.18f, c.x, c.y));
        auto tag = Rectangle<float> (46.0f, 26.0f).withCentre (c);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRect (tag.translated (1.5f, 2.0f));
        g.setColour (Colour (0xffd81f2a));
        g.fillRect (tag);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.setFont (theme::stencilFont (11.5f));
        g.drawFittedText ("WRITE\nPROTECT", tag.reduced (2.0f).toNearestInt(), Justification::centred, 2, 0.6f);
    }

    if (paused)
    {
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::cream.withAlpha (0.7f));
        g.setFont (theme::stencilFont (13.0f));
        g.drawText ("PAUSE", Rectangle<float> (r.getX(), r.getBottom() - 16.0f, r.getWidth(), 14.0f), Justification::centred);
    }
    ignoreUnused (slotIndex);
}

void CassetteShelf::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    // shelf interior
    ColourGradient back (Colour (0xff1d130c), b.getCentreX(), b.getY(), Colour (0xff0a0705), b.getCentreX(), b.getBottom(), false);
    g.setGradientFill (back);
    g.fillRect (b);
    ColourGradient lamp (Colour (0xff6b3a14).withAlpha (0.35f), b.getCentreX(), b.getBottom(), Colours::transparentBlack, b.getCentreX(), b.getY(), false);
    g.setGradientFill (lamp);
    g.fillRect (b);
    // shelf floor
    auto floor = b.withTop (b.getBottom() - 14.0f);
    ColourGradient wood (Colour (0xff5a3316), floor.getX(), floor.getY(), Colour (0xff2a160a), floor.getX(), floor.getBottom(), false);
    g.setGradientFill (wood);
    g.fillRect (floor);
    g.setColour (Colour (0xffc98d4d).withAlpha (0.35f));
    g.drawLine (floor.getX(), floor.getY(), floor.getRight(), floor.getY(), 1.5f);

    // empty slot outlines (faint)
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (! proc.getSlotMeta (i).isEmpty()) continue;
        auto r = cassetteRect (i);
        g.setColour (Colours::white.withAlpha (hover == i ? 0.12f : 0.04f));
        g.drawRoundedRectangle (r.reduced (2.0f), 4.0f, 1.0f);
        if (hover == i)
        {
            g.setFont (theme::stencilFont (15.0f));
            g.setColour (theme::cream.withAlpha (0.4f));
            g.drawText ("+", r, Justification::centred);
        }
    }

    const int screening = proc.getScreening();
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (dragging && i == dragFrom) continue;
        const float s = slide[(size_t) i];
        auto r = cassetteRect (i);
        // slide out: forward (bigger) and down a little
        r = r.expanded (5.0f * s, 4.0f * s).translated (0.0f, 12.0f * s);
        drawCassette (g, r, proc.getSlotState (i), i, proc.selectedSlot == i, screening == i, s);
    }

    if (dragging && dragFrom >= 0)
    {
        const int target = dropIndexFor (dragX);
        auto marker = cassetteRect (target);
        g.setColour (theme::amber.withAlpha (0.5f));
        g.fillRect (Rectangle<float> (3.0f, marker.getHeight()).withCentre ({ marker.getX() - (layout::cassetteStep - layout::cassetteW) * 0.5f + (target > dragFrom ? layout::cassetteStep : 0.0f), marker.getCentreY() }));
        auto r = cassetteRect (dragFrom).withCentre ({ dragX - grabOffset, cassetteRect (dragFrom).getCentreY() + 10.0f }).expanded (4.0f);
        drawCassette (g, r, proc.getSlotState (dragFrom), dragFrom, true, false, 1.0f);
    }
}

void CassetteShelf::mouseMove (const MouseEvent& e)
{
    const int h = slotAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void CassetteShelf::mouseExit (const MouseEvent&) { if (hover != -1) { hover = -1; repaint(); } }

void CassetteShelf::mouseDown (const MouseEvent& e)
{
    pressed = slotAt (e.position);
    dragging = false;
    if (pressed < 0) return;
    if (e.mods.isPopupMenu())
    {
        showSlotMenu (pressed);
        pressed = -1;
        return;
    }
    grabOffset = e.position.x - cassetteRect (pressed).getCentreX();
    dragX = e.position.x;
}

void CassetteShelf::mouseDrag (const MouseEvent& e)
{
    if (pressed < 0 || proc.getSlotMeta (pressed).isEmpty()) return;
    if (! dragging && e.getDistanceFromDragStart() > 6)
    {
        if (proc.getSlotMeta (pressed).writeProtect)
        {
            if (onMessage) onMessage ("WRITE PROTECT");
            pressed = -1;
            return;
        }
        dragging = true;
        dragFrom = pressed;
    }
    if (dragging) { dragX = e.position.x; repaint(); }
}

void CassetteShelf::mouseUp (const MouseEvent& e)
{
    if (dragging)
    {
        const int to = dropIndexFor (dragX);
        dragging = false;
        const int from = dragFrom;
        dragFrom = -1;
        pressed = -1;
        if (to != from) proc.moveSlot (from, to);
        repaint();
        return;
    }
    if (pressed >= 0 && slotAt (e.position) == pressed)
    {
        if (proc.getSlotMeta (pressed).isEmpty()) showEffectMenu (pressed);
        else
        {
            const int sel = proc.selectedSlot == pressed ? -1 : pressed;
            proc.selectedSlot = sel;
            if (onSelect) onSelect (sel);
        }
    }
    pressed = -1;
    repaint();
}

void CassetteShelf::mouseDoubleClick (const MouseEvent& e)
{
    const int s = slotAt (e.position);
    if (s >= 0) showEffectMenu (s);
}

void CassetteShelf::showEffectMenu (int slot)
{
    PopupMenu m;
    m.addSectionHeader ("SLOT " + String (slot + 1) + " - EFFECT");
    for (auto t : allEffectTypes())
    {
        const auto& info = effectInfo (t);
        PopupMenu::Item it (info.name);
        it.itemID = (int) t;
        it.isTicked = proc.getSlotType (slot) == t;
        it.colour = info.colour.brighter (0.3f);
        m.addItem (it);
    }
    SafePointer<CassetteShelf> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe, slot] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        safe->proc.setSlotType (slot, (EffectType) r);
        safe->proc.selectedSlot = slot;
        if (safe->onSelect) safe->onSelect (slot);
    });
}

void CassetteShelf::showSlotMenu (int slot)
{
    const auto s = proc.getSlotState (slot);
    PopupMenu fxMenu;
    for (auto t : allEffectTypes())
        fxMenu.addItem (1000 + (int) t, effectInfo (t).name, true, s.type == t);

    PopupMenu msMenu;
    msMenu.addItem (2000, "STEREO", ! s.isEmpty(), s.ms == MSMode::Stereo);
    msMenu.addItem (2001, "MID", ! s.isEmpty(), s.ms == MSMode::Mid);
    msMenu.addItem (2002, "SIDE", ! s.isEmpty(), s.ms == MSMode::Side);

    PopupMenu loadMenu;
    const auto files = proc.bank.getSlotPresetFiles();
    for (int i = 0; i < files.size(); ++i) loadMenu.addItem (3000 + i, files[i].getFileNameWithoutExtension());

    PopupMenu m;
    m.addSectionHeader ("SLOT " + String (slot + 1) + (s.isEmpty() ? String (" - EMPTY") : " - " + effectInfo (s.type).name.toUpperCase()));
    m.addSubMenu (s.isEmpty() ? "Insert effect" : "Change effect", fxMenu, ! s.writeProtect);
    m.addSeparator();
    m.addItem (1, "Copy", ! s.isEmpty());
    m.addItem (2, "Paste", proc.hasSlotClipboard() && ! s.writeProtect);
    m.addItem (3, "EJECT", ! s.isEmpty() && ! s.writeProtect);
    m.addSeparator();
    m.addItem (4, "Save slot preset...", ! s.isEmpty());
    m.addSubMenu ("Load slot preset", loadMenu, files.size() > 0 && ! s.writeProtect);
    m.addSeparator();
    m.addItem (5, "WRITE PROTECT", ! s.isEmpty(), s.writeProtect);
    m.addItem (6, "PAUSE", ! s.isEmpty(), s.pause);
    m.addItem (7, "SCREENING (solo)", ! s.isEmpty(), proc.getScreening() == slot);
    m.addSubMenu ("M/S", msMenu, ! s.isEmpty());

    SafePointer<CassetteShelf> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe, slot, files] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& p = safe->proc;
        if (r >= 3000) { p.loadSlotPreset (slot, files[r - 3000]); return; }
        if (r >= 2000) { p.setSlotMs (slot, (MSMode) (r - 2000)); return; }
        if (r >= 1000)
        {
            p.setSlotType (slot, (EffectType) (r - 1000));
            p.selectedSlot = slot;
            if (safe->onSelect) safe->onSelect (slot);
            return;
        }
        switch (r)
        {
            case 1: p.copySlot (slot); break;
            case 2: p.pasteSlot (slot); break;
            case 3: p.ejectSlot (slot); if (p.selectedSlot == slot && safe->onSelect) { p.selectedSlot = -1; safe->onSelect (-1); } break;
            case 4:
            {
                auto* w = new AlertWindow ("SAVE SLOT PRESET", "Name:", MessageBoxIconType::NoIcon);
                w->addTextEditor ("name", effectInfo (p.getSlotType (slot)).name);
                w->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
                w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
                w->enterModalState (true, ModalCallbackFunction::create ([safe, w, slot] (int res)
                {
                    if (res == 1 && safe != nullptr)
                    {
                        const auto name = w->getTextEditorContents ("name");
                        const bool ok = safe->proc.saveSlotPreset (slot, name);
                        if (safe->onMessage) safe->onMessage (ok ? "SLOT SAVED" : "SAVE FAILED");
                    }
                }), true);
                break;
            }
            case 5: p.setWriteProtect (slot, ! p.getSlotMeta (slot).writeProtect); break;
            case 6: p.setSlotParam (slot, 9, p.getSlotState (slot).pause ? 0.0f : 1.0f); break;
            case 7: p.setScreening (p.getScreening() == slot ? -1 : slot); break;
            default: break;
        }
        safe->repaint();
    });
}

} // namespace ek::ui
