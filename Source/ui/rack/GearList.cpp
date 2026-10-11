#include "GearList.h"
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

namespace
{
struct MiniatureCache : public DeletedAtShutdown
{
    ~MiniatureCache() override { clearSingletonInstance(); }
    std::map<std::tuple<int, int, int>, Image> images;
    JUCE_DECLARE_SINGLETON_INLINE (MiniatureCache, true)
};
} // namespace

Image miniature (EffectType t, int w, int h)
{
    auto* mc = MiniatureCache::getInstance();
    if (mc == nullptr) return {};
    auto& cache = mc->images;
    const auto key = std::make_tuple ((int) t, w, h);
    if (auto it = cache.find (key); it != cache.end()) return it->second;

    faceplate::State st;
    const auto& info = effectInfo (t);
    for (int i = 0; i < kNumParams; ++i) st.params[(size_t) i] = info.params[(size_t) i].defaultNorm();
    Image img (Image::ARGB, w * 2, h * 2, true);
    {
        Graphics g (img);
        g.addTransform (AffineTransform::scale (2.0f));
        faceplate::paintFaceplate (g, t, Rectangle<float> (0.0f, 0.0f, (float) w, (float) h), st, true);
    }
    cache[key] = img;
    return img;
}
} // namespace gear

// ============================================================================
// Scrolling list of group labels and miniatures (the drag source).
class GearList::Content : public Component, public TooltipClient
{
public:
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
        int y = 2;
        for (auto& r : rows)
        {
            if (r.header) { r.bounds = { 8, y + 4, w - 16, 16 }; y += 22; }
            else          { r.bounds = { 8, y, w - 16, itemH }; y += itemH + 6; }
        }
        idealHeight = y + 8;
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
             + "\nDrag into the rack, double-click = add at the end";
    }

    void paint (Graphics& g) override
    {
        if (rows.empty())
        {
            g.setFont (theme::monoLabel (10.0f));
            g.setColour (theme::silverDim.withAlpha (0.6f));
            g.drawText ("NO GEAR MATCHES", getLocalBounds().removeFromTop (50), Justification::centred, false);
            return;
        }
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto& r = rows[(size_t) i];
            auto rb = r.bounds.toFloat();
            if (! g.clipRegionIntersects (r.bounds.expanded (8))) continue;
            if (r.header)
            {
                g.setFont (theme::monoLabel (9.0f).boldened());
                g.setColour (theme::accent.withAlpha (0.85f));
                const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), r.text);
                g.drawText (r.text, rb, Justification::centredLeft, false);
                g.setColour (theme::accent.withAlpha (0.18f));
                g.fillRect (rb.getX() + tw + 8.0f, rb.getCentreY(), rb.getWidth() - tw - 8.0f, 1.0f);
                continue;
            }
            const bool hot = i == hover;
            const bool down = i == pressed;
            if (hot)
            {
                for (int k = 3; k >= 1; --k)
                {
                    g.setColour (theme::accent.withAlpha (0.07f * (float) (4 - k)));
                    g.fillRoundedRectangle (rb.expanded ((float) k * 2.0f), 4.0f + (float) k * 2.0f);
                }
            }
            g.setColour (Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (rb.translated (0.0f, 2.0f), 3.0f);
            const auto img = gear::miniature (r.type, r.bounds.getWidth(), r.bounds.getHeight());
            g.drawImage (img, rb.translated (0.0f, down ? 1.0f : 0.0f), RectanglePlacement::stretchToFit);
            if (! hot)
            {
                g.setColour (Colours::black.withAlpha (0.16f));
                g.fillRoundedRectangle (rb, 3.0f);
            }
            else
            {
                g.setColour (Colours::white.withAlpha (0.07f));
                g.fillRoundedRectangle (rb, 3.0f);
            }
            g.setColour (hot ? theme::accent.withAlpha (0.95f) : Colours::black.withAlpha (0.8f));
            g.drawRoundedRectangle (rb.reduced (0.5f), 3.0f, hot ? 1.4f : 1.0f);
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
                DropShadow (Colours::black.withAlpha (0.7f), m, { 0, m / 3 }).drawForRectangle (g, { m, m, w, h });
                g.setOpacity (0.92f);
                g.drawImage (gear::miniature (r.type, w, h), Rectangle<float> ((float) m, (float) m, (float) w, (float) h));
                g.setOpacity (1.0f);
                g.setColour (theme::accent);
                g.drawRoundedRectangle (Rectangle<float> ((float) m, (float) m, (float) w, (float) h).reduced (0.5f), 3.0f, 1.2f);
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
            owner.onMessage ("RACK FULL - DROP ON A UNIT TO REPLACE IT");
    }

    static constexpr int itemH = 41;

private:
    GearList& owner;
    std::vector<Row> rows;
    String filter;
    int hover = -1, pressed = -1, idealHeight = 60;
    bool dragging = false;
};

// ============================================================================
GearList::GearList (EffectorKillaAudioProcessor& p) : proc (p)
{
    content = std::make_unique<Content> (*this);
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (6);
    viewport.setScrollOnDragMode (Viewport::ScrollOnDragMode::never);
    viewport.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, theme::accent.withAlpha (0.3f));
    addAndMakeVisible (viewport);

    search.setTextToShowWhenEmpty ("SEARCH GEAR", theme::silverDim.withAlpha (0.45f));
    search.setFont (theme::monoLabel (11.0f));
    search.setColour (TextEditor::backgroundColourId, Colour (0xff050608));
    search.setColour (TextEditor::outlineColourId, Colour (0xff2a2d33));
    search.setColour (TextEditor::focusedOutlineColourId, theme::accent.withAlpha (0.6f));
    search.setColour (TextEditor::textColourId, theme::vfd);
    search.setColour (TextEditor::highlightColourId, theme::accent.withAlpha (0.3f));
    search.setColour (CaretComponent::caretColourId, theme::vfd);
    search.setIndents (24, 5);
    search.setEscapeAndReturnKeysConsumed (false);
    search.onTextChange = [this] { setFilter (search.getText()); };
    search.onEscapeKey = [this] { search.setText ({}, true); };
    search.setTooltip ("Filter the gear by name, group or description");
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
    auto r = getLocalBounds().reduced (6, 0);
    r.removeFromTop (46);
    search.setBounds (r.removeFromTop (26).reduced (2, 0));
    r.removeFromTop (8);
    viewport.setBounds (r.withTrimmedBottom (6).withX (0).withWidth (getWidth()));
    const int w = viewport.getWidth() - (content->getIdealHeight() > viewport.getHeight() ? viewport.getScrollBarThickness() : 0);
    content->setSize (w, content->getIdealHeight());
}

void GearList::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    ColourGradient bg (Colour (0xff0d0f13), 0.0f, 0.0f, Colour (0xff07080b), 0.0f, b.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (b);
    g.setColour (Colours::black);
    g.drawVerticalLine (0, 0.0f, b.getBottom());
    g.setColour (Colours::white.withAlpha (0.04f));
    g.drawVerticalLine (1, 0.0f, b.getBottom());

    // header
    auto head = b.removeFromTop (44.0f).reduced (14.0f, 0.0f);
    theme::drawGlowText (g, "GEAR", head.withTrimmedTop (6.0f).removeFromTop (26.0f), theme::stencilFont (24.0f), theme::silver,
                         Justification::centredLeft, 0.4f);
    g.setFont (theme::monoLabel (9.0f));
    g.setColour (theme::accent.withAlpha (0.75f));
    g.drawText ("DRAG INTO THE RACK", head.withTrimmedTop (10.0f).removeFromTop (20.0f), Justification::centredRight, false);
    g.setColour (theme::accent.withAlpha (0.35f));
    g.fillRect (14.0f, 42.0f, (float) getWidth() - 28.0f, 1.0f);
}

void GearList::paintOverChildren (Graphics& g)
{
    // the magnifier is drawn over the text editor background
    const auto sb = search.getBounds().toFloat();
    const auto c = Point<float> (sb.getX() + 12.0f, sb.getCentreY() - 1.0f);
    g.setColour (theme::silverDim.withAlpha (0.7f));
    g.drawEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (c), 1.4f);
    g.drawLine (c.x + 3.0f, c.y + 3.0f, c.x + 6.5f, c.y + 6.5f, 1.6f);

    if (removeHover)
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme::killRed.withAlpha (0.22f));
        g.fillRect (b);
        g.setColour (theme::killRed);
        g.drawRect (b.reduced (2.0f), 2.5f);
        auto tag = b.withSizeKeepingCentre (b.getWidth() - 40.0f, 70.0f);
        g.setColour (Colour (0xf0120507));
        g.fillRoundedRectangle (tag, 6.0f);
        g.setColour (theme::killRed);
        g.drawRoundedRectangle (tag, 6.0f, 1.5f);
        theme::drawGlowText (g, "DROP TO REMOVE", tag.removeFromTop (44.0f).withTrimmedTop (10.0f), theme::stencilFont (20.0f),
                             theme::killRed, Justification::centred, 0.8f);
        g.setFont (theme::monoLabel (9.5f));
        g.setColour (Colour (0xffffb3ba));
        g.drawText ("the unit leaves the rack", tag, Justification::centredTop, false);
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
    if (proc.getSlotMeta (s).writeProtect) { if (onMessage) onMessage ("UNIT IS LOCKED"); return; }
    proc.removeEffect (s);
}

} // namespace ek::ui
