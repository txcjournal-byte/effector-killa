#include "PresetBrowser.h"
#include "CardStyle.h"

namespace ek::ui
{
using namespace juce;

namespace
{
const Colour labelCol { 0xffc8d3e6 };   // readable label colour (Docs/TRAPWIN-STYLE.md)
// layout of the 752 x 608 overlay
constexpr int kPad = 20, kTitleH = 54, kCatW = 170, kTop = 68, kSearchH = 34, kRowH = 30;

// While the field has the keyboard focus, a mouse-down anywhere outside it (also outside this component, e.g. on the
// chain or the header) gives the focus away, so the space bar / arrows reach the DAW again. Registered as a global
// mouse listener for the lifetime of the owner; it does nothing while the field is not focused.
struct BrowserClickOutside : public MouseListener
{
    explicit BrowserClickOutside (TextEditor& e) : ed (e) { Desktop::getInstance().addGlobalMouseListener (this); }
    ~BrowserClickOutside() override { Desktop::getInstance().removeGlobalMouseListener (this); }
    void mouseDown (const MouseEvent& e) override
    {
        if (! ed.hasKeyboardFocus (true)) return;
        auto* c = e.eventComponent;
        if (c != &ed && ! ed.isParentOf (c)) ed.giveAwayKeyboardFocus();
    }
    TextEditor& ed;
};
}

// Single-line search field: Up / Down move the list selection while typing, it never keeps the focus by itself.
class PresetBrowser::SearchField : public TextEditor
{
public:
    std::function<void (int)> onMove;
    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::upKey)   { if (onMove) onMove (-1); return true; }
        if (k == KeyPress::downKey) { if (onMove) onMove (1); return true; }
        return TextEditor::keyPressed (k);
    }
};

PresetBrowser::PresetBrowser (EffectorKillaAudioProcessor& p)
    : proc (p), search (std::make_unique<SearchField>()), list ("presets", this)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);

    // the search field is drawn as a pill in paint(); the editor itself is transparent
    search->setTextToShowWhenEmpty ("SEARCH NAME, DESCRIPTION OR CHANNEL", labelCol.withAlpha (0.7f));
    search->setFont (card::monoFont (13.0f));
    search->setIndents (36, 8);
    search->setJustification (Justification::centredLeft);
    search->setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
    search->setColour (TextEditor::textColourId, card::text);
    search->setColour (TextEditor::highlightColourId, card::ice.withAlpha (0.25f));
    search->setColour (TextEditor::highlightedTextColourId, Colours::white);
    search->setColour (CaretComponent::caretColourId, card::ice);
    search->setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    search->setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
    search->setEscapeAndReturnKeysConsumed (true);
    search->onTextChange = [this] { rebuild(); };
    search->onReturnKey = [this]
    {
        if (! shown.empty()) load (jmax (0, list.getSelectedRow()));
        close();
    };
    search->onEscapeKey = [this] { close(); };
    search->onFocusLost = [this] { repaint (search->getBounds().expanded (8)); };
    search->onMove = [this] (int d) { moveSelection (d); };
    addAndMakeVisible (*search);
    clickOutside = std::make_unique<BrowserClickOutside> (*search);

    list.setRowHeight (kRowH);
    list.setWantsKeyboardFocus (false);
    list.setMouseClickGrabsKeyboardFocus (false);
    list.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, card::ice.withAlpha (0.30f));
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);
    setRepaintsOnMouseActivity (false);
    setSize (preferredWidth, preferredHeight);
}

PresetBrowser::~PresetBrowser()
{
    clickOutside.reset();   // before the search field it watches
}

void PresetBrowser::releaseFocus()
{
    if (search->hasKeyboardFocus (true)) search->giveAwayKeyboardFocus();
}

void PresetBrowser::close()
{
    releaseFocus();
    if (onClose) onClose();
}

void PresetBrowser::visibilityChanged()
{
    if (! isVisible()) releaseFocus();
}

// the ListBox rows are created by JUCE; a click on them must not pull the keyboard focus anywhere
void PresetBrowser::unfocusRows()
{
    std::function<void (Component&)> walk = [&] (Component& c)
    {
        c.setWantsKeyboardFocus (false);
        c.setMouseClickGrabsKeyboardFocus (false);
        for (auto* ch : c.getChildren()) walk (*ch);
    };
    walk (list);
}

void PresetBrowser::moveSelection (int delta)
{
    if (shown.empty()) return;
    const int cur = list.getSelectedRow();
    const int next = jlimit (0, (int) shown.size() - 1, cur < 0 ? 0 : cur + delta);
    list.selectRow (next);   // selectedRowsChanged loads it
    list.scrollToEnsureRowIsOnscreen (next);
}

void PresetBrowser::open()
{
    all.clear();
    categories = { "ALL PRESETS", String::fromUTF8 ("\xe2\x98\x85 FAVOURITES"), "MY PRESETS" };
    const auto& chans = proc.bank.getChannels();
    for (int c = 0; c < (int) chans.size(); ++c)
    {
        categories.add (String (c + 1).paddedLeft ('0', 2) + "  " + chans[(size_t) c].name);
        const auto& pl = proc.bank.getChannelPresets (c);
        for (int i = 0; i < (int) pl.size(); ++i)
        {
            Entry e;
            e.name = pl[(size_t) i].name;
            e.description = pl[(size_t) i].description;
            e.where = "CH " + String (c + 1).paddedLeft ('0', 2) + " " + chans[(size_t) c].name;
            e.channel = c;
            e.index = i;
            e.favourite = proc.bank.isFavourite (PresetBank::keyFor (pl[(size_t) i]));
            all.push_back (e);
        }
    }
    for (auto& f : proc.bank.getUserPresetFiles())
    {
        Entry e;
        e.name = f.getFileNameWithoutExtension();
        e.description = "your preset";
        e.where = "MY PRESETS";
        e.file = f;
        e.favourite = proc.bank.isFavourite ("user:" + e.name);
        all.push_back (e);
    }
    const auto& m = proc.getMeta();
    category = m.factory && m.channel >= 0 ? 3 + m.channel : 0;
    search->clear();
    rebuild();
    unfocusRows();
    // no grabKeyboardFocus(): the space bar must keep reaching the DAW until the user clicks into the search
}

void PresetBrowser::rebuild()
{
    shown.clear();
    const auto q = search->getText().trim().toLowerCase();
    for (auto& e : all)
    {
        if (category == 1 && ! e.favourite) continue;
        if (category == 2 && e.file == File()) continue;
        if (category >= 3 && e.channel != category - 3) continue;
        if (q.isNotEmpty() && ! (e.name + " " + e.description + " " + e.where).toLowerCase().contains (q)) continue;
        shown.push_back (e);
    }
    list.updateContent();
    unfocusRows();
    // select the preset that is loaded now
    const auto& m = proc.getMeta();
    loadingFromSelection = true;
    list.deselectAllRows();
    for (int i = 0; i < (int) shown.size(); ++i)
        if ((m.factory && shown[(size_t) i].channel == m.channel && shown[(size_t) i].index == m.index)
            || (! m.factory && shown[(size_t) i].file != File() && shown[(size_t) i].name == m.name))
        {
            list.selectRow (i);
            break;
        }
    loadingFromSelection = false;
    repaint();
}

Rectangle<int> PresetBrowser::categoryArea() const { return { kPad, kTop, kCatW, getHeight() - kTop - kPad }; }
Rectangle<int> PresetBrowser::closeArea() const { return { getWidth() - kPad - 104, 15, 104, 30 }; }

int PresetBrowser::categoryRowHeight() const
{
    // 26 px tab + 5 px gap, squeezed (never below a 22 px tab) when the browser is short
    const int n = jmax (1, categories.size());
    return jlimit (26, 31, (categoryArea().getHeight() - 10) / n);
}

int PresetBrowser::categoryAt (Point<int> p) const
{
    const auto a = categoryArea();
    if (! a.contains (p)) return -1;
    const int rh = categoryRowHeight();
    int y = p.y - a.getY();
    if (y > 3 * rh) y -= 10;  // gap after MY PRESETS
    const int row = y / rh;
    return row >= 0 && row < categories.size() ? row : -1;
}

Colour PresetBrowser::categoryLed (int i) const
{
    if (i == 0) return card::ice;
    if (i == 1) return Colour (0xffffd23f);
    if (i == 2) return Colour (0xff5fe0b0);
    const auto& chans = proc.bank.getChannels();
    if (i - 3 < (int) chans.size())
    {
        // channel colour, but bright enough to read as a lit LED (CH 12 is silver, CH 09 white)
        auto c = chans[(size_t) (i - 3)].colour;
        return c.getBrightness() < 0.75f ? c.withBrightness (jmax (0.75f, c.getBrightness())) : c;
    }
    return card::ice;
}

Colour PresetBrowser::entryColour (const Entry& e) const
{
    if (e.channel >= 0) return categoryLed (3 + e.channel);
    return categoryLed (2);
}

void PresetBrowser::paint (Graphics& g)
{
    const auto outer = getLocalBounds().toFloat().reduced (4.0f);
    // soft shadow + ice glow around the glass card
    for (int k = 5; k >= 1; --k)
    {
        g.setColour (card::iceDeep.withAlpha (0.035f * (float) (6 - k)));
        g.fillRoundedRectangle (outer.expanded ((float) k * 0.8f), card::radius + (float) k * 0.8f);
    }
    {
        Path shape;
        shape.addRoundedRectangle (outer, card::radius);
        g.setColour (card::card.withAlpha (0.97f));
        g.fillPath (shape);
        g.saveState();
        g.reduceClipRegion (shape);
        ColourGradient haze (card::iceDeep.withAlpha (0.10f), outer.getRight(), outer.getY(), card::iceDeep.withAlpha (0.0f),
                             outer.getRight() - outer.getWidth() * 0.5f, outer.getBottom(), true);
        g.setGradientFill (haze);
        g.fillRect (outer);
        ColourGradient sheen (Colours::white.withAlpha (0.04f), 0.0f, outer.getY(), Colours::white.withAlpha (0.0f), 0.0f, outer.getY() + 60.0f, false);
        g.setGradientFill (sheen);
        g.fillRect (outer);
        g.restoreState();
        g.setColour (card::line);
        g.drawRoundedRectangle (outer.reduced (0.5f), card::radius - 0.5f, 1.0f);
        ColourGradient top (card::ice.withAlpha (0.0f), outer.getX() + 30.0f, 0.0f, card::ice.withAlpha (0.0f), outer.getRight() - 30.0f, 0.0f, false);
        top.addColour (0.3, card::ice.withAlpha (0.22f));
        top.addColour (0.7, card::ice.withAlpha (0.12f));
        g.setGradientFill (top);
        g.fillRect (outer.getX() + 30.0f, outer.getY() + 0.5f, outer.getWidth() - 60.0f, 0.8f);
    }

    // title: chrome italic (~20 px) + count badge
    const auto tb = card::paintChromeTitle (g, "PRESETS", { (float) kPad + 4.0f, 16.0f, 180.0f, 28.0f }, 21.0f, 1.0f);
    card::paintSparkle (g, { tb.getRight() + 11.0f, tb.getY() + 4.0f }, 4.5f, card::ice, 1.0f);
    {
        const String count = String ((int) shown.size()) + ((int) shown.size() == 1 ? " PRESET" : " PRESETS");
        const auto f = card::monoFont (11.0f, true);
        const float w = GlyphArrangement::getStringWidth (f, count) + 22.0f;
        const auto badge = Rectangle<float> (tb.getRight() + 26.0f, 18.0f, w, 24.0f);
        g.setColour (card::bg);
        g.fillRoundedRectangle (badge, 12.0f);
        g.setColour (card::text.withAlpha (0.6f));
        g.drawRoundedRectangle (badge.reduced (0.5f), 11.5f, 1.0f);
        g.setFont (f);
        g.setColour (card::text);
        g.drawText (count, badge, Justification::centred, false);
    }
    // hint between badge and close
    {
        g.setFont (card::monoFont (11.0f));
        g.setColour (labelCol);
        g.drawText (String::fromUTF8 ("CLICK = LISTEN  \xc2\xb7  DOUBLE-CLICK = LOAD"), Rectangle<int> (closeArea().getX() - 330, 15, 316, 30),
                    Justification::centredRight, false);
    }

    // close pill (secondary: outline)
    {
        const auto cr = closeArea().toFloat();
        if (closeHover)
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (card::iceDeep.withAlpha (0.12f * (float) (4 - k)));
                g.fillRoundedRectangle (cr.expanded ((float) k * 1.5f), cr.getHeight() * 0.5f + (float) k * 1.5f);
            }
        g.setColour (card::bg);
        g.fillRoundedRectangle (cr, cr.getHeight() * 0.5f);
        g.setColour (closeHover ? card::ice : card::text.withAlpha (0.55f));
        g.drawRoundedRectangle (cr.reduced (0.5f), cr.getHeight() * 0.5f - 0.5f, 1.0f);
        g.setFont (card::monoFont (12.0f, true));
        g.setColour (card::text);
        g.drawText (String::fromUTF8 ("CLOSE  \xe2\x9c\x95"), cr, Justification::centred, false);
    }

    // hairline under the header
    {
        ColourGradient hl (card::ice.withAlpha (0.0f), outer.getX() + 16.0f, 0.0f, card::ice.withAlpha (0.0f), outer.getRight() - 16.0f, 0.0f, false);
        hl.addColour (0.2, card::ice.withAlpha (0.18f));
        hl.addColour (0.8, card::ice.withAlpha (0.18f));
        g.setGradientFill (hl);
        g.fillRect (outer.getX() + 16.0f, (float) kTitleH + 2.0f, outer.getWidth() - 32.0f, 1.0f);
    }

    // category tabs
    const auto a = categoryArea();
    const int rh = categoryRowHeight();
    const int tabH = jmin (rh - 4, 28);
    int y = a.getY();
    for (int i = 0; i < categories.size(); ++i)
    {
        const auto r = Rectangle<int> (a.getX(), y, a.getWidth(), tabH).toFloat();
        const bool active = i == category, hot = i == hoverCategory;
        const float rad = r.getHeight() * 0.5f;
        const auto led = categoryLed (i);
        if (active)
        {
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (card::iceDeep.withAlpha (0.13f * (float) (4 - k)));
                g.fillRoundedRectangle (r.expanded ((float) k * 1.2f), rad + (float) k * 1.2f);
            }
            ColourGradient fill (card::ice.withAlpha (0.30f), 0.0f, r.getY(), card::iceDeep.withAlpha (0.30f), 0.0f, r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, rad);
            g.setColour (card::ice.withAlpha (0.85f));
            g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
        }
        else
        {
            g.setColour (hot ? card::card2.brighter (0.15f) : card::bg.withAlpha (0.7f));
            g.fillRoundedRectangle (r, rad);
            g.setColour (hot ? card::ice.withAlpha (0.45f) : card::line);
            g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
        }
        if (i == 1)
            card::paintSparkle (g, { r.getX() + 14.0f, r.getCentreY() }, 4.5f, led, active ? 1.0f : 0.6f);
        else
            card::dotGlow (g, { r.getX() + 14.0f, r.getCentreY() }, 2.8f, led, active || hot ? 1.0f : 0.75f);
        String label = categories[i];
        if (i == 1) label = "FAVOURITES";
        const auto lf = card::monoFont (12.0f, active).withExtraKerningFactor (0.03f);
        g.setFont (lf);
        g.setColour (active ? Colours::white : (hot ? card::text : labelCol));
        g.drawFittedText (label, r.withTrimmedLeft (25.0f).withTrimmedRight (8.0f).toNearestInt(), Justification::centredLeft, 1, 0.85f);
        y += rh;
        if (i == 2) y += 8;
    }
    {
        ColourGradient div (card::ice.withAlpha (0.0f), 0.0f, (float) a.getY(), card::ice.withAlpha (0.0f), 0.0f, (float) a.getBottom(), false);
        div.addColour (0.5, card::ice.withAlpha (0.16f));
        g.setGradientFill (div);
        g.fillRect ((float) a.getRight() + 10.0f, (float) a.getY(), 1.0f, (float) a.getHeight());
    }

    // search pill
    {
        const auto sb = search->getBounds().toFloat();
        const bool focus = search->hasKeyboardFocus (true);
        if (focus)
            for (int k = 3; k >= 1; --k)
            {
                g.setColour (card::iceDeep.withAlpha (0.10f * (float) (4 - k)));
                g.fillRoundedRectangle (sb.expanded ((float) k * 1.5f), sb.getHeight() * 0.5f + (float) k * 1.5f);
            }
        g.setColour (card::bg);
        g.fillRoundedRectangle (sb, sb.getHeight() * 0.5f);
        g.setColour (focus ? card::ice.withAlpha (0.7f) : card::line.withMultipliedAlpha (1.6f));
        g.drawRoundedRectangle (sb.reduced (0.5f), sb.getHeight() * 0.5f - 0.5f, 1.0f);
        const auto c = Point<float> (sb.getX() + 19.0f, sb.getCentreY() - 0.5f);
        g.setColour (labelCol);
        g.drawEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (c), 1.4f);
        g.drawLine (c.x + 3.8f, c.y + 3.8f, c.x + 7.2f, c.y + 7.2f, 1.6f);
    }
}

void PresetBrowser::resized()
{
    const int x0 = categoryArea().getRight() + 22;
    search->setBounds (x0, kTop, getWidth() - x0 - kPad, kSearchH);
    list.setBounds (x0, kTop + kSearchH + 10, getWidth() - x0 - kPad + 6, getHeight() - (kTop + kSearchH + 10) - kPad + 4);
    unfocusRows();
}

void PresetBrowser::mouseDown (const MouseEvent&)
{
    // any click outside the search field ends typing (the field itself gets its own mouse events)
    releaseFocus();
}

void PresetBrowser::mouseUp (const MouseEvent& e)
{
    if (closeArea().contains (e.getPosition())) { close(); return; }
    const int c = categoryAt (e.getPosition());
    if (c >= 0 && c != category) { category = c; rebuild(); }
}

void PresetBrowser::mouseMove (const MouseEvent& e)
{
    const int c = categoryAt (e.getPosition());
    if (c != hoverCategory) { hoverCategory = c; repaint (categoryArea().expanded (6)); }
    const bool ch = closeArea().contains (e.getPosition());
    if (ch != closeHover) { closeHover = ch; repaint (closeArea().expanded (8)); }
    setMouseCursor (c >= 0 || ch ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void PresetBrowser::mouseExit (const MouseEvent&)
{
    if (hoverCategory >= 0 || closeHover) { hoverCategory = -1; closeHover = false; repaint(); }
}

void PresetBrowser::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& e = shown[(size_t) row];
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (3.0f, 2.0f);
    const float rad = r.getHeight() * 0.5f;
    const auto col = entryColour (e);

    if (selected)
    {
        g.setColour (card::iceDeep.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (2.0f, 1.5f), rad + 2.0f);
        ColourGradient lit (Colour (0xff15233f), r.getX(), r.getY(), Colour (0xff0c1424), r.getX(), r.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (r, rad);
        ColourGradient side (col.withAlpha (0.18f), r.getX(), r.getCentreY(), col.withAlpha (0.0f), r.getX() + r.getWidth() * 0.35f, r.getCentreY(), false);
        g.setGradientFill (side);
        g.fillRoundedRectangle (r, rad);
        g.setColour (card::ice.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    }
    else
    {
        g.setColour (row % 2 == 0 ? card::card2.brighter (0.06f) : card::card.withAlpha (0.6f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (card::line.withMultipliedAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    }

    auto t = r.reduced (12.0f, 0.0f);
    // channel colour dot
    card::dotGlow (g, { t.getX() + 3.0f, t.getCentreY() }, 3.0f, col, selected ? 1.0f : 0.8f);
    t.removeFromLeft (16.0f);

    // channel tag pill on the right (11 px)
    {
        const auto tf = card::monoFont (11.0f).withExtraKerningFactor (0.02f);
        const float tw = jmin (t.getWidth() * 0.32f, GlyphArrangement::getStringWidth (tf, e.where) + 16.0f);
        auto tag = t.removeFromRight (tw).withSizeKeepingCentre (tw, 20.0f);
        g.setColour (card::bg.withAlpha (0.8f));
        g.fillRoundedRectangle (tag, 10.0f);
        g.setColour (col.withAlpha (selected ? 0.75f : 0.45f));
        g.drawRoundedRectangle (tag.reduced (0.5f), 9.5f, 1.0f);
        g.setFont (tf);
        g.setColour (selected ? card::text : labelCol);
        g.drawFittedText (e.where, tag.reduced (6.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.9f);
        t.removeFromRight (10.0f);
    }

    // name (13 px), favourite sparkle after it
    const auto nf = card::monoFont (13.0f, true).withExtraKerningFactor (0.03f);
    const String name = e.name.toUpperCase();
    const float nameW = jmin (t.getWidth() * 0.5f, jmax (150.0f, GlyphArrangement::getStringWidth (nf, name) + 22.0f));
    auto nameR = t.removeFromLeft (nameW);
    if (selected) card::glowText (g, name, nameR, nf, Colours::white, Justification::centredLeft, 0.35f);
    else
    {
        g.setFont (nf);
        g.setColour (card::text);
        g.drawText (name, nameR, Justification::centredLeft, true);
    }
    if (e.favourite)
    {
        const float nw = jmin (nameR.getWidth() - 10.0f, GlyphArrangement::getStringWidth (nf, name));
        card::paintSparkle (g, { nameR.getX() + nw + 10.0f, nameR.getCentreY() }, 5.0f, Colour (0xffffd23f), selected ? 1.0f : 0.7f);
    }
    // description (12 px)
    g.setFont (card::monoFont (12.0f).withExtraKerningFactor (0.0f));
    g.setColour (selected ? card::text : labelCol);
    g.drawText (e.description, t.withTrimmedLeft (4.0f), Justification::centredLeft, true);
}

void PresetBrowser::load (int row)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& e = shown[(size_t) row];
    if (e.file != File()) proc.loadUserPreset (e.file);
    else proc.loadFactory (e.channel, e.index);
}

void PresetBrowser::listBoxItemClicked (int, const MouseEvent&) { releaseFocus(); } // selection change loads it
void PresetBrowser::listBoxItemDoubleClicked (int row, const MouseEvent&) { load (row); close(); }
void PresetBrowser::returnKeyPressed (int row) { load (row); close(); }
void PresetBrowser::selectedRowsChanged (int row)
{
    // click or arrow keys: load and listen right away
    if (! loadingFromSelection) load (row);
}

} // namespace ek::ui
