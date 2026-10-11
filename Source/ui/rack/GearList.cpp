#include "GearList.h"

namespace ek::ui
{
using namespace juce;

namespace gear
{
const std::vector<Group>& groups()
{
    using E = EffectType;
    static const std::vector<Group> g {
        { "COLOR",        { E::CassettePlug, E::Menace, E::Brainrot } },
        { "FILTER & EQ",  { E::ThroughTheWall, E::ToneUp } },
        { "DYNAMICS",     { E::Squeeze, E::Overcooked, E::Slap, E::RedLine } },
        { "MODULATION",   { E::Doubles, E::Swirl, E::Chopped } },
        { "SPACE",        { E::AdLibThrow, E::AuraRoom } },
        { "STEREO",       { E::WideBody } },
    };
    return g;
}

String describe (EffectType t)
{
    switch (t)
    {
        case EffectType::CassettePlug:   return "Tape / tube saturation with wow, hiss and low keep";
        case EffectType::Menace:         return "Fuzz, clip, foldback and rectify distortion";
        case EffectType::Brainrot:       return "Bitcrusher and lo-fi: bits, rate, jitter, noise, crackle";
        case EffectType::ThroughTheWall: return "Filter LP / HP / BP / notch / comb with tempo LFO and envelope";
        case EffectType::ToneUp:         return "Musical EQ: low cut, shelves, two mids and tilt";
        case EffectType::Squeeze:        return "Compressor with parallel mix";
        case EffectType::Overcooked:     return "3-band upward / downward compression";
        case EffectType::Slap:           return "Transient shaper: attack, sustain, clip";
        case EffectType::Doubles:        return "Chorus / doubler with up to 4 voices";
        case EffectType::Swirl:          return "Phaser, free or tempo synced";
        case EffectType::AdLibThrow:     return "Delay: tempo sync, ping-pong, filters, ducking";
        case EffectType::AuraRoom:       return "Reverb: room, plate, hall, dark";
        case EffectType::WideBody:       return "Stereo width, Haas and mono bass";
        case EffectType::Chopped:        return "Pattern gate, tempo pump, noise gate";
        case EffectType::RedLine:        return "Clipper / limiter with lookahead";
        default:                         return {};
    }
}
} // namespace gear

GearList::GearList (EffectorKillaAudioProcessor& p) : proc (p)
{
    for (const auto& grp : gear::groups())
    {
        rows.push_back ({ true, grp.name, EffectType::None, {} });
        for (auto t : grp.types) rows.push_back ({ false, effectInfo (t).name, t, {} });
    }
    layoutRows();
}

void GearList::layoutRows()
{
    int y = 44;
    for (auto& r : rows)
    {
        const int h = r.header ? 20 : 25;
        if (r.header) y += 2;
        r.bounds = { 8, y, 224, h };
        y += h + 1;
    }
}

int GearList::rowAt (Point<int> p) const
{
    for (int i = 0; i < (int) rows.size(); ++i)
        if (! rows[(size_t) i].header && rows[(size_t) i].bounds.withWidth (getWidth() - 16).contains (p)) return i;
    return -1;
}

void GearList::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    drawCard (g, b, 10.0f);
    g.setFont (theme::stencilFont (17.0f));
    g.setColour (theme::silver);
    g.drawText ("GEAR", Rectangle<float> (14.0f, 10.0f, 120.0f, 24.0f), Justification::centredLeft, false);
    drawSmallLabel (g, "DRAG INTO THE RACK", { 90.0f, 12.0f, b.getWidth() - 100.0f, 20.0f }, Justification::centredRight, theme::silverDim, 9.0f);

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& r = rows[(size_t) i];
        auto rb = r.bounds.withWidth (getWidth() - 16).toFloat();
        if (r.header)
        {
            drawSmallLabel (g, r.text, rb.withTrimmedLeft (4.0f), Justification::centredLeft, theme::accent, 9.5f);
            continue;
        }
        const auto& info = effectInfo (r.type);
        const bool hot = i == hover || i == pressed;
        g.setColour (hot ? Colour (0xff17223a) : Colour (0xff0b111d));
        g.fillRoundedRectangle (rb, 5.0f);
        g.setColour (hot ? theme::accent.withAlpha (0.6f) : Colour (0x22a9b6cc));
        g.drawRoundedRectangle (rb.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (info.colour);
        g.fillEllipse (rb.getX() + 9.0f, rb.getCentreY() - 4.5f, 9.0f, 9.0f);
        g.setFont (theme::monoLabel (12.0f).boldened());
        g.setColour (theme::silver);
        g.drawText (r.text, rb.withTrimmedLeft (26.0f), Justification::centredLeft, false);
        // grip hint
        g.setColour (theme::silverDim.withAlpha (hot ? 0.8f : 0.3f));
        for (int k = 0; k < 3; ++k)
            g.fillRect (rb.getRight() - 16.0f, rb.getCentreY() - 4.0f + (float) k * 3.5f, 8.0f, 1.3f);
    }

    if (removeHover)
    {
        g.setColour (theme::killRed.withAlpha (0.16f));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (theme::killRed);
        g.drawRoundedRectangle (b.reduced (1.0f), 10.0f, 2.0f);
        drawSmallLabel (g, "DROP HERE TO REMOVE", b.withHeight (b.getHeight() - 20.0f).removeFromBottom (30.0f),
                        Justification::centred, theme::killRed, 12.0f);
    }
}

String GearList::getTooltipFor (Point<int> p) const
{
    const int i = rowAt (p);
    if (i < 0) return {};
    return gear::describe (rows[(size_t) i].type) + "   (drag into the rack, double-click = add at the end)";
}

void GearList::mouseMove (const MouseEvent& e)
{
    const int h = rowAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
}

void GearList::mouseExit (const MouseEvent&) { hover = -1; repaint(); }

void GearList::mouseDown (const MouseEvent& e)
{
    pressed = rowAt (e.getPosition());
    dragging = false;
    repaint();
}

void GearList::mouseDrag (const MouseEvent& e)
{
    if (dragging || pressed < 0 || e.getDistanceFromDragStart() < 4) return;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        const auto& r = rows[(size_t) pressed];
        // drag image: a small chip with the effect name
        Image img (Image::ARGB, 200, 30, true);
        {
            Graphics g (img);
            g.setColour (Colour (0xe0101a2c));
            g.fillRoundedRectangle (0.0f, 0.0f, 200.0f, 30.0f, 6.0f);
            g.setColour (theme::accent);
            g.drawRoundedRectangle (0.5f, 0.5f, 199.0f, 29.0f, 6.0f, 1.2f);
            g.setColour (effectInfo (r.type).colour);
            g.fillEllipse (10.0f, 10.5f, 9.0f, 9.0f);
            g.setFont (theme::monoLabel (12.0f).boldened());
            g.setColour (theme::silver);
            g.drawText (r.text, 28, 0, 170, 30, Justification::centredLeft, false);
        }
        c->startDragging (dnd::fx (r.type), this, ScaledImage (img), true);
    }
    pressed = -1;
    repaint();
}

void GearList::mouseDoubleClick (const MouseEvent& e)
{
    const int i = rowAt (e.getPosition());
    if (i < 0) return;
    int last = -1;
    for (int s = kNumSlots - 1; s >= 0 && last < 0; --s)
        if (proc.getSlotType (s) != EffectType::None) last = s;
    if (proc.insertEffect (last + 1, rows[(size_t) i].type) < 0 && onMessage)
        onMessage ("RACK FULL - DROP ON A UNIT TO REPLACE IT");
}

bool GearList::isInterestedInDragSource (const SourceDetails& d)
{
    int s;
    return dnd::parseUnit (d.description, s);
}

void GearList::itemDragEnter (const SourceDetails&) { removeHover = true; repaint(); }
void GearList::itemDragExit (const SourceDetails&) { removeHover = false; repaint(); }

void GearList::itemDropped (const SourceDetails& d)
{
    removeHover = false;
    repaint();
    int s;
    if (! dnd::parseUnit (d.description, s)) return;
    if (proc.getSlotMeta (s).writeProtect) { if (onMessage) onMessage ("UNIT IS LOCKED"); return; }
    proc.removeEffect (s);
}

} // namespace ek::ui
