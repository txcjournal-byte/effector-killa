#include "PresetBrowser.h"
#include "CardStyle.h"

namespace ek::ui
{
using namespace juce;

PresetBrowser::PresetBrowser (EffectorKillaAudioProcessor& p)
    : proc (p), list ("presets", this)
{
    // the search field is drawn as a pill in paint(); the editor itself is transparent
    search.setTextToShowWhenEmpty ("SEARCH NAME, DESCRIPTION OR CHANNEL", card::muted.withAlpha (0.85f));
    search.setFont (card::monoFont (11.5f));
    search.setIndents (34, 7);
    search.setJustification (Justification::centredLeft);
    search.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
    search.setColour (TextEditor::textColourId, card::text);
    search.setColour (TextEditor::highlightColourId, card::ice.withAlpha (0.25f));
    search.setColour (TextEditor::highlightedTextColourId, Colours::white);
    search.setColour (CaretComponent::caretColourId, card::ice);
    search.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    search.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { if (! shown.empty()) { load (jmax (0, list.getSelectedRow())); if (onClose) onClose(); } };
    search.onEscapeKey = [this] { if (onClose) onClose(); };
    addAndMakeVisible (search);

    list.setRowHeight (36);
    list.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, card::ice.withAlpha (0.25f));
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);
    setRepaintsOnMouseActivity (false);
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
    search.clear();
    rebuild();
    search.grabKeyboardFocus();
}

void PresetBrowser::rebuild()
{
    shown.clear();
    const auto q = search.getText().trim().toLowerCase();
    for (auto& e : all)
    {
        if (category == 1 && ! e.favourite) continue;
        if (category == 2 && e.file == File()) continue;
        if (category >= 3 && e.channel != category - 3) continue;
        if (q.isNotEmpty() && ! (e.name + " " + e.description + " " + e.where).toLowerCase().contains (q)) continue;
        shown.push_back (e);
    }
    list.updateContent();
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

Rectangle<int> PresetBrowser::categoryArea() const { return { 22, 68, 196, getHeight() - 88 }; }
Rectangle<int> PresetBrowser::closeArea() const { return { getWidth() - 116, 20, 94, 28 }; }

int PresetBrowser::categoryRowHeight() const
{
    // 26 px per tab, squeezed when the browser is short
    const int n = jmax (1, categories.size());
    return jlimit (16, 28, (categoryArea().getHeight() - 10) / n);
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
        g.setColour (card::card.withAlpha (0.96f));
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

    // title: chrome italic + count pill
    const auto tb = card::paintChromeTitle (g, "PRESETS", { 26.0f, 18.0f, 220.0f, 32.0f }, 26.0f, 1.0f);
    card::paintSparkle (g, { tb.getRight() + 12.0f, tb.getY() + 4.0f }, 5.0f, card::ice, 1.0f);
    {
        const String count = String ((int) shown.size()) + ((int) shown.size() == 1 ? " PRESET" : " PRESETS");
        const auto f = card::monoFont (8.5f);
        const float w = GlyphArrangement::getStringWidth (f, count) + 20.0f;
        const auto badge = Rectangle<float> (tb.getRight() + 28.0f, 24.0f, w, 20.0f);
        g.setColour (card::bg);
        g.fillRoundedRectangle (badge, 10.0f);
        g.setColour (card::text.withAlpha (0.55f));
        g.drawRoundedRectangle (badge.reduced (0.5f), 9.5f, 1.0f);
        g.setFont (f);
        g.setColour (card::text);
        g.drawText (count, badge, Justification::centred, false);
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
        g.setColour (closeHover ? card::ice : card::text.withAlpha (0.45f));
        g.drawRoundedRectangle (cr.reduced (0.5f), cr.getHeight() * 0.5f - 0.5f, 1.0f);
        g.setFont (card::monoFont (9.5f));
        g.setColour (card::text);
        g.drawText (String::fromUTF8 ("CLOSE  \xe2\x9c\x95"), cr, Justification::centred, false);
    }

    // hairline under the header
    {
        ColourGradient hl (card::ice.withAlpha (0.0f), outer.getX() + 16.0f, 0.0f, card::ice.withAlpha (0.0f), outer.getRight() - 16.0f, 0.0f, false);
        hl.addColour (0.2, card::ice.withAlpha (0.16f));
        hl.addColour (0.8, card::ice.withAlpha (0.16f));
        g.setGradientFill (hl);
        g.fillRect (outer.getX() + 16.0f, 58.0f, outer.getWidth() - 32.0f, 1.0f);
    }

    // category pills
    const auto a = categoryArea();
    const int rh = categoryRowHeight();
    int y = a.getY();
    for (int i = 0; i < categories.size(); ++i)
    {
        const auto r = Rectangle<int> (a.getX(), y, a.getWidth(), rh - 4).toFloat();
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
            card::paintSparkle (g, { r.getX() + rad + 1.0f, r.getCentreY() }, 4.0f, led, active ? 1.0f : 0.5f);
        else
            card::dotGlow (g, { r.getX() + rad + 1.0f, r.getCentreY() }, 2.2f, led, active || hot ? 1.0f : 0.7f);
        String label = categories[i];
        if (i == 1) label = "FAVOURITES";
        g.setFont (card::monoFont (8.5f, active));
        g.setColour (active ? Colours::white : (hot ? card::text : card::text2));
        g.drawFittedText (label, r.withTrimmedLeft (rad + 10.0f).withTrimmedRight (8.0f).toNearestInt(), Justification::centredLeft, 1, 0.7f);
        y += rh;
        if (i == 2) y += 10;
    }
    {
        ColourGradient div (card::ice.withAlpha (0.0f), 0.0f, (float) a.getY(), card::ice.withAlpha (0.0f), 0.0f, (float) a.getBottom(), false);
        div.addColour (0.5, card::ice.withAlpha (0.14f));
        g.setGradientFill (div);
        g.fillRect ((float) a.getRight() + 12.0f, (float) a.getY(), 1.0f, (float) a.getHeight());
    }

    // search pill
    {
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
        const auto c = Point<float> (sb.getX() + 18.0f, sb.getCentreY() - 0.5f);
        g.setColour (card::text2);
        g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (c), 1.3f);
        g.drawLine (c.x + 3.4f, c.y + 3.4f, c.x + 6.6f, c.y + 6.6f, 1.5f);
    }
}

void PresetBrowser::resized()
{
    const int x0 = categoryArea().getRight() + 26;
    search.setBounds (x0, 68, getWidth() - x0 - 22, 32);
    list.setBounds (x0, 110, getWidth() - x0 - 18, getHeight() - 128);
}

void PresetBrowser::mouseUp (const MouseEvent& e)
{
    if (closeArea().contains (e.getPosition())) { if (onClose) onClose(); return; }
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

bool PresetBrowser::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

void PresetBrowser::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& e = shown[(size_t) row];
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (4.0f, 2.5f);
    const float rad = 12.0f;
    const auto col = entryColour (e);

    if (selected)
    {
        g.setColour (card::iceDeep.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (2.0f, 1.5f), rad + 2.0f);
        ColourGradient lit (Colour (0xff13203a), r.getX(), r.getY(), Colour (0xff0c1424), r.getX(), r.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (r, rad);
        ColourGradient side (col.withAlpha (0.16f), r.getX(), r.getCentreY(), col.withAlpha (0.0f), r.getX() + r.getWidth() * 0.35f, r.getCentreY(), false);
        g.setGradientFill (side);
        g.fillRoundedRectangle (r, rad);
        g.setColour (card::ice.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    }
    else
    {
        g.setColour (row % 2 == 0 ? card::card2 : card::card.withAlpha (0.6f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (card::line.withMultipliedAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);
    }

    auto t = r.reduced (14.0f, 0.0f);
    // channel colour dot
    card::dotGlow (g, { t.getX() + 3.0f, t.getCentreY() }, 2.6f, col, selected ? 1.0f : 0.75f);
    t.removeFromLeft (16.0f);
    // name in Plex Mono, favourite sparkle after it
    const auto nf = card::monoFont (12.0f, true);
    auto nameR = t.removeFromLeft (jmin (230.0f, t.getWidth() * 0.42f));
    const String name = e.name.toUpperCase();
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
        card::paintSparkle (g, { nameR.getX() + nw + 10.0f, nameR.getCentreY() }, 4.6f, Colour (0xffffd23f), selected ? 1.0f : 0.6f);
    }
    // where (right) and description (middle)
    const auto wf = card::monoFont (8.0f);
    const float ww = jmin (t.getWidth() * 0.45f, GlyphArrangement::getStringWidth (wf, e.where) + 8.0f);
    g.setFont (wf);
    g.setColour (selected ? card::ice : card::muted);
    g.drawText (e.where, t.removeFromRight (ww), Justification::centredRight, true);
    g.setFont (card::monoFont (9.5f));
    g.setColour (selected ? card::text2 : card::muted);
    g.drawText (e.description, t.withTrimmedRight (12.0f), Justification::centredLeft, true);
}

void PresetBrowser::load (int row)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& e = shown[(size_t) row];
    if (e.file != File()) proc.loadUserPreset (e.file);
    else proc.loadFactory (e.channel, e.index);
}

void PresetBrowser::listBoxItemClicked (int, const MouseEvent&) {} // selection change loads it
void PresetBrowser::listBoxItemDoubleClicked (int row, const MouseEvent&) { load (row); if (onClose) onClose(); }
void PresetBrowser::returnKeyPressed (int row) { load (row); if (onClose) onClose(); }
void PresetBrowser::selectedRowsChanged (int row)
{
    // click or arrow keys: load and listen right away
    if (! loadingFromSelection) load (row);
}

} // namespace ek::ui
