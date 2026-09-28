#include "Vcr.h"

namespace ek::ui
{
using namespace juce;

VideoRecorder::VideoRecorder (EffectorKillaAudioProcessor& p) : proc (p)
{
    origin = layout::vcrBody.getPosition();
}

VideoRecorder::Part VideoRecorder::partAt (Point<float> p) const
{
    if (local (layout::vcrDisplay).contains (p)) return Display;
    if (local (layout::rec).contains (p)) return Rec;
    if (local (layout::rewind).contains (p)) return Rewind;
    if (local (layout::fastFwd).contains (p)) return FastFwd;
    if (local (layout::eject).contains (p)) return Eject;
    if (local (layout::sideSwitch).expanded (6.0f, 26.0f).contains (p)) return Side;
    return None;
}

void VideoRecorder::tick()
{
    const auto name = proc.getMeta().name;
    if (name != shownName || proc.getSide() != shownSide || proc.canUndo() != shownUndo || proc.canRedo() != shownRedo)
        repaint();
}

static void drawVcrButton (Graphics& g, Rectangle<float> r, bool pressed, bool enabled, const String& label,
                           std::function<void (Graphics&, Rectangle<float>)> icon)
{
    theme::drawSocket (g, r, 4.0f);
    auto face = r.reduced (3.0f);
    if (pressed) face = face.translated (0.0f, 1.5f);
    ColourGradient grad (Colour (0xff34302b), face.getX(), face.getY(), Colour (0xff121110), face.getX(), face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 3.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (face.reduced (1.0f), 3.0f, 1.0f);
    const auto top = face.withHeight (face.getHeight() * 0.45f);
    const auto bottom = face.withTrimmedTop (face.getHeight() * 0.45f);
    g.setColour (theme::cream.withAlpha (enabled ? 0.95f : 0.35f));
    g.setFont (theme::stencilFont (19.0f));
    if (icon) { icon (g, top.reduced (4.0f)); g.drawText (label, bottom, Justification::centred); }
    else if (label.isNotEmpty()) g.drawText (label, face, Justification::centred);
}

void VideoRecorder::paint (Graphics& g)
{
    shownName = proc.getMeta().name;
    shownSide = proc.getSide();
    shownUndo = proc.canUndo();
    shownRedo = proc.canRedo();

    // VFD display window
    auto disp = local (layout::vcrDisplay);
    g.setColour (Colour (0xff040605));
    g.fillRoundedRectangle (disp, 5.0f);
    ColourGradient glass (Colour (0xff0e1a13), disp.getCentreX(), disp.getY(), Colour (0xff020302), disp.getCentreX(), disp.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (disp.reduced (4.0f), 4.0f);
    // VFD dot grid
    g.setColour (Colour (0xff1b2a20).withAlpha (0.5f));
    for (float x = disp.getX() + 8.0f; x < disp.getRight() - 6.0f; x += 4.0f)
        g.drawVerticalLine ((int) x, disp.getY() + 6.0f, disp.getBottom() - 6.0f);

    String text = shownName.toUpperCase();
    if (proc.isCurrentFavourite()) text = String::fromUTF8 ("\xe2\x98\x85 ") + text;
    theme::drawGlowText (g, text, local (layout::vcrText), theme::vfdFont (38.0f), theme::vfdGreen, Justification::centred, 1.0f);
    // small channel / side indicators
    g.setFont (theme::vfdFont (14.0f));
    g.setColour (theme::vfdGreen.withAlpha (0.7f));
    const int ch = proc.getCurrentChannel();
    g.drawText ("CH" + String (ch + 1).paddedLeft ('0', 2), disp.withWidth (64.0f).reduced (8.0f, 6.0f), Justification::topLeft);
    g.drawText (proc.getSide() == 0 ? "SIDE A" : "SIDE B", disp.withTrimmedLeft (disp.getWidth() - 80.0f).reduced (8.0f, 6.0f), Justification::topRight);
    g.setColour (theme::vfdGreen.withAlpha (0.35f));
    g.drawText (proc.getMeta().factory ? "FACTORY" : "USER", disp.withTrimmedLeft (disp.getWidth() - 80.0f).reduced (8.0f, 6.0f), Justification::bottomRight);

    // buttons
    drawVcrButton (g, local (layout::rec), down == Rec, true, String(), nullptr);
    drawVcrButton (g, local (layout::rewind), down == Rewind, shownUndo, "REWIND", [] (Graphics& gg, Rectangle<float> r)
    {
        Path p;
        const auto c = r.getCentre();
        const float s = r.getHeight() * 0.45f;
        p.addTriangle (c.x, c.y - s, c.x, c.y + s, c.x - s * 1.4f, c.y);
        p.addTriangle (c.x + s * 1.4f, c.y - s, c.x + s * 1.4f, c.y + s, c.x, c.y);
        gg.fillPath (p);
    });
    drawVcrButton (g, local (layout::fastFwd), down == FastFwd, shownRedo, "FAST FWD", [] (Graphics& gg, Rectangle<float> r)
    {
        Path p;
        const auto c = r.getCentre();
        const float s = r.getHeight() * 0.45f;
        p.addTriangle (c.x, c.y - s, c.x, c.y + s, c.x + s * 1.4f, c.y);
        p.addTriangle (c.x - s * 1.4f, c.y - s, c.x - s * 1.4f, c.y + s, c.x, c.y);
        gg.fillPath (p);
    });
    drawVcrButton (g, local (layout::eject), down == Eject, proc.selectedSlot >= 0, "EJECT", [] (Graphics& gg, Rectangle<float> r)
    {
        Path p;
        const auto c = r.getCentre().translated (0.0f, 2.0f);
        const float s = r.getHeight() * 0.42f;
        p.addTriangle (c.x - s * 1.2f, c.y + s * 0.2f, c.x + s * 1.2f, c.y + s * 0.2f, c.x, c.y - s);
        p.addRectangle (c.x - s * 1.2f, c.y + s * 0.45f, s * 2.4f, s * 0.35f);
        gg.fillPath (p);
    });

    // REC: label on top, red LED below (as on the reference)
    {
        auto r = local (layout::rec).reduced (3.0f).translated (0.0f, down == Rec ? 1.5f : 0.0f);
        g.setColour (theme::cream);
        g.setFont (theme::stencilFont (19.0f));
        g.drawText ("REC", r.withHeight (r.getHeight() * 0.45f).translated (0.0f, 3.0f), Justification::centred);
        theme::drawLed (g, { r.getCentreX(), r.getY() + r.getHeight() * 0.7f }, 8.0f, theme::killRed, true, 0.7f);
    }

    // SIDE A / SIDE B switch
    auto sw = local (layout::sideSwitch);
    auto labels = local (layout::sideLabels);
    g.setColour (Colour (0xff171410));
    g.fillRect (labels);
    g.setFont (theme::stencilFont (18.0f));
    g.setColour (theme::cream);
    g.drawText ("SIDE A", labels.withWidth (labels.getWidth() * 0.5f), Justification::centred);
    g.drawText ("SIDE B", labels.withTrimmedLeft (labels.getWidth() * 0.5f), Justification::centred);
    theme::drawTrack (g, sw);
    const bool b = proc.getSide() == 1;
    auto thumb = Rectangle<float> (sw.getHeight() * 0.9f, sw.getHeight() + 2.0f).withCentre ({ b ? sw.getRight() - sw.getHeight() * 0.55f : sw.getX() + sw.getHeight() * 0.55f, sw.getCentreY() });
    theme::drawSliderThumb (g, thumb, false);
    theme::drawLed (g, { b ? sw.getX() + 14.0f : sw.getRight() - 14.0f, sw.getCentreY() }, 5.5f, theme::amber, true);
}

void VideoRecorder::mouseDown (const MouseEvent& e)
{
    down = partAt (e.position);
    repaint();
}

void VideoRecorder::mouseUp (const MouseEvent& e)
{
    const auto p = partAt (e.position);
    const auto was = down;
    down = None;
    repaint();
    if (p != was) return;
    switch (p)
    {
        case Display: showPresetMenu(); break;
        case Rec: showSaveDialog(); break;
        case Rewind: proc.undo(); if (onMessage) onMessage ("<< REWIND"); break;
        case FastFwd: proc.redo(); if (onMessage) onMessage (">> FAST FWD"); break;
        case Eject:
            if (proc.selectedSlot >= 0 && ! proc.getSlotMeta (proc.selectedSlot).writeProtect)
            {
                proc.ejectSlot (proc.selectedSlot);
                proc.selectedSlot = -1;
                if (onEject) onEject();
                if (onMessage) onMessage ("EJECT");
            }
            break;
        case Side: proc.setSide (proc.getSide() == 0 ? 1 : 0); break;
        case None: default: break;
    }
}

void VideoRecorder::showPresetMenu()
{
    PopupMenu m;
    const int ch = proc.getCurrentChannel();
    const auto& info = proc.bank.getChannels();
    const auto& list = proc.bank.getChannelPresets (ch);
    if (ch >= 0 && ch < (int) info.size())
        m.addSectionHeader ("CH " + String (ch + 1).paddedLeft ('0', 2) + " " + info[(size_t) ch].name);
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const auto key = PresetBank::keyFor (list[(size_t) i]);
        m.addItem (1 + i, (proc.bank.isFavourite (key) ? String::fromUTF8 ("\xe2\x98\x85 ") : String()) + list[(size_t) i].name
                              + "   - " + list[(size_t) i].description,
                   true, proc.getMeta().factory && proc.getMeta().channel == ch && proc.getMeta().index == i);
    }

    PopupMenu chans;
    for (int c = 0; c < (int) info.size(); ++c)
    {
        PopupMenu sub;
        const auto& pl = proc.bank.getChannelPresets (c);
        for (int i = 0; i < (int) pl.size(); ++i) sub.addItem (1000 + c * 100 + i, pl[(size_t) i].name);
        chans.addSubMenu (String (c + 1).paddedLeft ('0', 2) + " " + info[(size_t) c].name, sub);
    }
    m.addSeparator();
    m.addSubMenu ("ALL CHANNELS", chans);

    PopupMenu favs;
    int favIndex = 0;
    std::vector<std::pair<int, int>> favTargets;
    for (int c = 0; c < (int) info.size(); ++c)
        for (int i = 0; i < (int) proc.bank.getChannelPresets (c).size(); ++i)
            if (proc.bank.isFavourite (PresetBank::keyFor (proc.bank.getChannelPresets (c)[(size_t) i])))
            {
                favs.addItem (5000 + favIndex++, proc.bank.getChannelPresets (c)[(size_t) i].name);
                favTargets.push_back ({ c, i });
            }
    const auto userFiles = proc.bank.getUserPresetFiles();
    for (int i = 0; i < userFiles.size(); ++i)
        if (proc.bank.isFavourite ("user:" + userFiles[i].getFileNameWithoutExtension()))
            favs.addItem (7000 + i, userFiles[i].getFileNameWithoutExtension());
    m.addSubMenu (String::fromUTF8 ("\xe2\x98\x85 STAFF PICKS"), favs, favs.getNumItems() > 0);

    PopupMenu user;
    for (int i = 0; i < userFiles.size(); ++i) user.addItem (6000 + i, userFiles[i].getFileNameWithoutExtension());
    m.addSubMenu ("USER TAPES", user, userFiles.size() > 0);

    SafePointer<VideoRecorder> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe, ch, favTargets, userFiles] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& p = safe->proc;
        if (r >= 7000) { p.loadUserPreset (userFiles[r - 7000]); return; }
        if (r >= 6000) { p.loadUserPreset (userFiles[r - 6000]); return; }
        if (r >= 5000) { auto t = favTargets[(size_t) (r - 5000)]; p.loadFactory (t.first, t.second); return; }
        if (r >= 1000) { p.loadFactory ((r - 1000) / 100, (r - 1000) % 100); return; }
        p.loadFactory (ch, r - 1);
    });
}

void VideoRecorder::showSaveDialog()
{
    auto* w = new AlertWindow ("REC - SAVE TAPE", "Preset name:", MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getMeta().factory ? proc.getMeta().name + " (mine)" : proc.getMeta().name);
    w->addButton ("REC", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<VideoRecorder> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w] (int res)
    {
        if (res != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        const bool ok = safe->proc.saveUserPreset (name);
        if (safe->onMessage) safe->onMessage (ok ? "REC OK" : "REC FAILED");
    }), true);
}

} // namespace ek::ui
