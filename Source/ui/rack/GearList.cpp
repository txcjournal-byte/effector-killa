#include "GearList.h"
#include "CardStyle.h"
#include "../SharedResources.h"
#include <map>
#include <tuple>

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

String categoryName (EffectType t)
{
    static const char* names[] = { "SATURATION", "DISTORTION", "LO-FI", "FILTER", "EQ", "DYNAMICS",
                                   "MODULATION", "SPACE", "STEREO", "GATE", "CLIPPER" };
    return names[jlimit (0, 10, (int) effectInfo (t).category)];
}

Image miniature (EffectType t, int w, int h)
{
    // cached in the shared UI resources (released with the last editor; see ui/SharedResources.h)
    const int64 key = ((int64) t << 40) ^ ((int64) w << 20) ^ (int64) h;
    return ui::cachedImage ("gear.miniature", key, 256, [&]
    {
        faceplate::State st;
        const auto& info = effectInfo (t);
        for (int i = 0; i < kNumParams; ++i) st.params[(size_t) i] = info.params[(size_t) i].defaultNorm();
        Image img (Image::ARGB, w * 2, h * 2, true);
        {
            Graphics g (img);
            g.addTransform (AffineTransform::scale (2.0f));
            faceplate::paintFaceplate (g, t, Rectangle<float> (0.0f, 0.0f, (float) w, (float) h), st, true);
        }
        return img;
    });
}
} // namespace gear

// ============================================================================
// Scrolling list of group labels and miniature chips (the drag source).
class GearList::Content : public Component, public TooltipClient
{
public:
    static constexpr int miniW = 264, miniH = 41;   // designed chip size (faceplate miniature)
    static constexpr int sidePad = 12;              // chip inset from the content edge (220 px column -> 196 px chips)

    explicit Content (GearList& o) : owner (o) { rebuild(); }

    struct Row
    {
        bool header = false;
        String text;
        EffectType type = EffectType::None;
        Rectangle<int> bounds;
    };

    void setFilter (const String& f)
    {
        filter = f.trim();
        rebuild();
    }

    void rebuild()
    {
        rows.clear();
        for (const auto& grp : gear::groups())
        {
            std::vector<EffectType> match;
            for (auto t : grp.types)
                if (filter.isEmpty() || effectInfo (t).name.containsIgnoreCase (filter) || grp.name.containsIgnoreCase (filter)
                    || gear::describe (t).containsIgnoreCase (filter) || gear::categoryName (t).containsIgnoreCase (filter))
                    match.push_back (t);
            if (match.empty()) continue;
            rows.push_back ({ true, grp.name, EffectType::None, {} });
            for (auto t : match) rows.push_back ({ false, effectInfo (t).name, t, {} });
        }
        hover = pressed = -1;
        layoutRows();
        repaint();
    }

    void layoutRows()
    {
        const int w = jmax (100, getWidth());
        const int chipW = w - 2 * sidePad;
        // chips keep the miniature's 264 x 41 aspect
        const int itemH = jmax (22, roundToInt ((float) chipW * (float) miniH / (float) miniW));
        int y = 2;
        bool first = true;
        for (auto& r : rows)
        {
            if (r.header) { y += first ? 0 : 5; r.bounds = { sidePad, y + 2, chipW, 16 }; y += 21; first = false; }
            else          { r.bounds = { sidePad, y, chipW, itemH }; y += itemH + 5; }
        }
        idealHeight = y + 6;
    }

    int getIdealHeight() const { return rows.empty() ? 60 : idealHeight; }
    void resized() override { layoutRows(); }

    int rowAt (Point<int> p) const
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (! rows[(size_t) i].header && rows[(size_t) i].bounds.contains (p)) return i;
        return -1;
    }

    String getTooltip() override
    {
        const int i = rowAt (getMouseXYRelative());
        if (i < 0) return {};
        const auto t = rows[(size_t) i].type;
        return effectInfo (t).name.toUpperCase() + "  -  " + gear::categoryName (t) + "\n" + gear::describe (t)
             + "\nDrag into the chain, double-click = add at the end";
    }

    static float chipRadius (Rectangle<float> rb) { return card::radiusMini * rb.getWidth() / (float) miniW; }

    // soft outer glow around a chip (layered rounded rects, no blur)
    static void paintChipGlow (Graphics& g, Rectangle<float> rb, Colour c, float amount)
    {
        const float rad = chipRadius (rb);
        for (int k = 4; k >= 1; --k)
        {
            g.setColour (c.withAlpha (0.055f * amount * (float) (5 - k)));
            g.fillRoundedRectangle (rb.expanded ((float) k * 1.6f), rad + (float) k * 1.6f);
        }
    }

    void paint (Graphics& g) override
    {
        if (rows.empty())
        {
            g.setFont (card::monoFont (9.0f));
            g.setColour (card::muted);
            g.drawText ("NO EFFECT MATCHES", getLocalBounds().removeFromTop (50), Justification::centred, false);
            return;
        }
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto& r = rows[(size_t) i];
            auto rb = r.bounds.toFloat();
            if (! g.clipRegionIntersects (r.bounds.expanded (10))) continue;
            if (r.header)
            {
                // group label: tiny sparkle, Plex Mono muted, hairline fading out to the right
                card::paintSparkle (g, { rb.getX() + 3.5f, rb.getCentreY() }, 3.2f, card::ice.withAlpha (0.75f), 0.6f);
                const auto f = card::monoFont (8.5f);
                g.setFont (f);
                g.setColour (card::text2);
                const float tw = GlyphArrangement::getStringWidth (f, r.text);
                g.drawText (r.text, rb.withTrimmedLeft (11.0f), Justification::centredLeft, false);
                const float lx = rb.getX() + 11.0f + tw + 8.0f;
                if (lx < rb.getRight())
                {
                    ColourGradient hl (card::ice.withAlpha (0.20f), lx, 0.0f, card::ice.withAlpha (0.0f), rb.getRight(), 0.0f, false);
                    g.setGradientFill (hl);
                    g.fillRect (lx, rb.getCentreY(), rb.getRight() - lx, 1.0f);
                }
                continue;
            }
            const bool hot = i == hover;
            const bool down = i == pressed;
            const auto accent = faceplate::palette (r.type).accent;
            const float rad = chipRadius (rb);
            if (hot) paintChipGlow (g, rb, card::iceDeep.interpolatedWith (accent, 0.35f), 1.0f);

            const auto img = gear::miniature (r.type, miniW, miniH);   // the designed 264 x 41 chip, scaled to the row
            g.setOpacity (hot ? 1.0f : 0.92f);
            g.drawImage (img, rb.translated (0.0f, down ? 1.0f : 0.0f), RectanglePlacement::stretchToFit);
            g.setOpacity (1.0f);
            if (hot)
            {
                g.setColour (card::ice.withAlpha (0.55f));
                g.drawRoundedRectangle (rb.reduced (0.5f), rad, 1.0f);
            }
        }
    }

    void mouseMove (const MouseEvent& e) override
    {
        const int h = rowAt (e.getPosition());
        if (h != hover)
        {
            if (hover >= 0) repaint (rows[(size_t) hover].bounds.expanded (10));
            hover = h;
            if (hover >= 0) repaint (rows[(size_t) hover].bounds.expanded (10));
        }
        setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    }

    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }

    void mouseDown (const MouseEvent& e) override
    {
        pressed = rowAt (e.getPosition());
        dragging = false;
        repaint();
    }

    void mouseUp (const MouseEvent&) override { pressed = -1; repaint(); }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging || pressed < 0 || e.getDistanceFromDragStart() < 4) return;
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            const auto& r = rows[(size_t) pressed];
            const int m = 12;
            const int w = r.bounds.getWidth(), h = r.bounds.getHeight();
            Image img (Image::ARGB, w + 2 * m, h + 2 * m, true);
            {
                Graphics g (img);
                const auto rb = Rectangle<float> ((float) m, (float) m, (float) w, (float) h);
                paintChipGlow (g, rb, card::iceDeep, 1.2f);
                g.setOpacity (0.94f);
                g.drawImage (gear::miniature (r.type, miniW, miniH), rb);
                g.setOpacity (1.0f);
                g.setColour (card::ice.withAlpha (0.7f));
                g.drawRoundedRectangle (rb.reduced (0.5f), chipRadius (rb), 1.0f);
            }
            const auto grab = e.getMouseDownPosition() - r.bounds.getPosition();
            Point<int> offset (-(grab.x + m), -(grab.y + m));
            c->startDragging (dnd::fx (r.type), this, ScaledImage (img), true, &offset, &e.source);
        }
        pressed = -1;
        repaint();
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        const int i = rowAt (e.getPosition());
        if (i < 0) return;
        auto& proc = owner.proc;
        int last = -1;
        for (int s = kNumSlots - 1; s >= 0 && last < 0; --s)
            if (proc.getSlotType (s) != EffectType::None) last = s;
        if (proc.insertEffect (last + 1, rows[(size_t) i].type) < 0 && owner.onMessage)
            owner.onMessage ("CHAIN FULL - DROP ON A CARD TO REPLACE IT");
    }

    void setHoverForSnapshot (EffectType t)
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (! rows[(size_t) i].header && rows[(size_t) i].type == t) hover = i;
        repaint();
    }

private:
    GearList& owner;
    std::vector<Row> rows;
    String filter;
    int hover = -1, pressed = -1, idealHeight = 60;
    bool dragging = false;
};

// ============================================================================
namespace
{
constexpr int kHeadH = 64;      // title block inside the card
constexpr int kSearchH = 26;
constexpr float kCardInset = 0.0f;
}

GearList::GearList (EffectorKillaAudioProcessor& p) : proc (p)
{
    content = std::make_unique<Content> (*this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (5);
    viewport.setScrollOnDragMode (Viewport::ScrollOnDragMode::never);
    viewport.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, card::ice.withAlpha (0.25f));
    addAndMakeVisible (viewport);

    // the search field is drawn as a pill in paint(); the editor itself is transparent
    search.setTextToShowWhenEmpty ("SEARCH EFFECTS", card::muted.withAlpha (0.8f));
    search.setFont (card::monoFont (10.5f));
    search.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
    search.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    search.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
    search.setColour (TextEditor::textColourId, card::text);
    search.setColour (TextEditor::highlightColourId, card::ice.withAlpha (0.25f));
    search.setColour (CaretComponent::caretColourId, card::ice);
    search.setIndents (28, 6);
    search.setJustification (Justification::centredLeft);
    search.setEscapeAndReturnKeysConsumed (false);
    search.onTextChange = [this] { setFilter (search.getText()); };
    search.onEscapeKey = [this] { search.setText ({}, true); };
    search.onFocusLost = [this] { repaint(); };
    search.setTooltip ("Filter the effects by name, group or description");
    addAndMakeVisible (search);
}

GearList::~GearList() = default;

void GearList::setFilter (const String& text)
{
    if (search.getText() != text) search.setText (text, false);
    content->setFilter (text);
    resized();
}

void GearList::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kHeadH);
    search.setBounds (r.removeFromTop (kSearchH).reduced (12, 0));
    r.removeFromTop (12);
    viewport.setBounds (r.withTrimmedBottom (12));
    // the row height follows the width, so lay out at full width first, then decide on the scroll bar
    content->setSize (viewport.getWidth(), content->getHeight());
    const int w = viewport.getWidth() - (content->getIdealHeight() > viewport.getHeight() ? viewport.getScrollBarThickness() : 0);
    content->setSize (w, content->getHeight());
    content->setSize (w, content->getIdealHeight());
}

void GearList::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (kCardInset);
    card::paintCard (g, b, card::ice, false);

    // header: chrome italic title + Plex Mono hint
    card::paintChromeTitle (g, "FX LIBRARY", { b.getX() + 16.0f, b.getY() + 14.0f, b.getWidth() - 40.0f, 24.0f }, 19.0f, 0.9f);
    card::paintSparkle (g, { b.getRight() - 20.0f, b.getY() + 25.0f }, 4.5f, card::ice, 1.0f);
    g.setFont (card::monoFont (8.0f));
    g.setColour (card::muted);
    g.drawText ("DRAG INTO THE CHAIN", Rectangle<float> (b.getX() + 16.0f, b.getY() + 40.0f, b.getWidth() - 32.0f, 12.0f),
                Justification::centredLeft, false);

    // search pill
    const auto sb = search.getBounds().toFloat();
    const bool focus = search.hasKeyboardFocus (true);
    if (focus)
        for (int k = 3; k >= 1; --k)
        {
            g.setColour (card::iceDeep.withAlpha (0.10f * (float) (4 - k)));
            g.fillRoundedRectangle (sb.expanded ((float) k * 1.5f), sb.getHeight() * 0.5f + (float) k * 1.5f);
        }
    g.setColour (card::bg);
    g.fillRoundedRectangle (sb, sb.getHeight() * 0.5f);
    g.setColour (focus ? card::ice.withAlpha (0.6f) : card::line.withMultipliedAlpha (1.4f));
    g.drawRoundedRectangle (sb.reduced (0.5f), sb.getHeight() * 0.5f - 0.5f, 1.0f);
}

void GearList::paintOverChildren (Graphics& g)
{
    // magnifier inside the search pill
    const auto sb = search.getBounds().toFloat();
    const auto c = Point<float> (sb.getX() + 14.0f, sb.getCentreY() - 0.5f);
    g.setColour (card::text2.withAlpha (0.8f));
    g.drawEllipse (Rectangle<float> (7.5f, 7.5f).withCentre (c), 1.2f);
    g.drawLine (c.x + 2.9f, c.y + 2.9f, c.x + 5.6f, c.y + 5.6f, 1.4f);

    if (removeHover)
    {
        const Colour red (0xffff2e3e);
        const auto b = getLocalBounds().toFloat().reduced (kCardInset);
        Path shape;
        shape.addRoundedRectangle (b, card::radius);
        g.setColour (Colour (0xff04060a).withAlpha (0.72f));
        g.fillPath (shape);
        {
            g.saveState();
            g.reduceClipRegion (shape);
            ColourGradient wash (red.withAlpha (0.22f), b.getCentreX(), b.getCentreY(), red.withAlpha (0.0f), b.getCentreX(), b.getY(), true);
            g.setGradientFill (wash);
            g.fillRect (b);
            g.restoreState();
        }
        // glowing red outline
        for (int k = 4; k >= 1; --k)
        {
            g.setColour (red.withAlpha (0.08f * (float) (5 - k)));
            g.drawRoundedRectangle (b.reduced (0.5f + (float) k * 1.5f), card::radius - (float) k * 1.5f, 2.0f);
        }
        g.setColour (red);
        g.drawRoundedRectangle (b.reduced (1.0f), card::radius - 1.0f, 1.4f);

        auto tag = b.withSizeKeepingCentre (b.getWidth() - 32.0f, 56.0f);
        g.setColour (card::bg);
        g.fillRoundedRectangle (tag, 28.0f);
        g.setColour (red.withAlpha (0.25f));
        g.drawRoundedRectangle (tag.expanded (2.0f), 30.0f, 3.0f);
        g.setColour (red);
        g.drawRoundedRectangle (tag.reduced (0.5f), 27.5f, 1.2f);
        card::glowText (g, "DROP TO REMOVE", tag.withTrimmedBottom (18.0f).withTrimmedTop (6.0f), card::monoFont (11.0f, true),
                        Colour (0xffff6b78), Justification::centred, 1.2f);
        g.setFont (card::monoFont (7.5f));
        g.setColour (card::text2);
        g.drawText ("THE CARD LEAVES THE CHAIN", tag.withTrimmedTop (32.0f).withTrimmedBottom (10.0f), Justification::centred, false);
    }
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
    if (proc.getSlotMeta (s).writeProtect) { if (onMessage) onMessage ("CARD IS LOCKED"); return; }
    proc.removeEffect (s);
}

void GearList::setSnapshotState (bool showRemove, EffectType hoverType)
{
    removeHover = showRemove;
    content->setHoverForSnapshot (hoverType);
    repaint();
}

} // namespace ek::ui
