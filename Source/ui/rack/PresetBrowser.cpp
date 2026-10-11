#include "PresetBrowser.h"
#include "PerformerDraw.h"

namespace ek::ui
{
using namespace juce;

PresetBrowser::PresetBrowser (EffectorKillaAudioProcessor& p)
    : proc (p), list ("presets", this), closeBtn (String::fromUTF8 ("CLOSE \xe2\x9c\x95"), theme::accent)
{
    search.setTextToShowWhenEmpty ("SEARCH NAME, DESCRIPTION OR CHANNEL...", performer::iceText.withAlpha (0.35f));
    search.setFont (theme::monoLabel (13.0f));
    search.setIndents (12, 8);
    search.setColour (TextEditor::backgroundColourId, Colour (0xff020306));
    search.setColour (TextEditor::textColourId, performer::iceText);
    search.setColour (TextEditor::highlightColourId, performer::iceLine.withAlpha (0.3f));
    search.setColour (TextEditor::highlightedTextColourId, Colours::white);
    search.setColour (CaretComponent::caretColourId, performer::iceText);
    search.setColour (TextEditor::outlineColourId, performer::iceLine.withAlpha (0.25f));
    search.setColour (TextEditor::focusedOutlineColourId, performer::iceLine.withAlpha (0.8f));
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { if (! shown.empty()) { load (jmax (0, list.getSelectedRow())); if (onClose) onClose(); } };
    search.onEscapeKey = [this] { if (onClose) onClose(); };
    addAndMakeVisible (search);

    list.setRowHeight (34);
    list.getVerticalScrollBar().setColour (ScrollBar::thumbColourId, performer::iceLine.withAlpha (0.35f));
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);

    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeBtn);
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

Rectangle<int> PresetBrowser::categoryArea() const { return { 16, 60, 214, getHeight() - 76 }; }

int PresetBrowser::categoryRowHeight() const
{
    // 26 px per tab, squeezed when the browser is short
    const int n = jmax (1, categories.size());
    return jlimit (16, 26, (categoryArea().getHeight() - 8) / n);
}

int PresetBrowser::categoryAt (Point<int> p) const
{
    const auto a = categoryArea();
    if (! a.contains (p)) return -1;
    const int rh = categoryRowHeight();
    int y = p.y - a.getY();
    if (y > 3 * rh) y -= 8;   // gap after MY PRESETS
    const int row = y / rh;
    return row >= 0 && row < categories.size() ? row : -1;
}

Colour PresetBrowser::categoryLed (int i) const
{
    if (i == 0) return performer::iceLine;
    if (i == 1) return performer::amber;
    if (i == 2) return theme::ledGreen;
    const auto& chans = proc.bank.getChannels();
    if (i - 3 < (int) chans.size())
    {
        // channel colour, but bright enough to read as a lit LED (CH 12 is silver, CH 09 white)
        auto c = chans[(size_t) (i - 3)].colour;
        return c.getBrightness() < 0.75f ? c.withBrightness (jmax (0.75f, c.getBrightness())) : c;
    }
    return performer::iceLine;
}

void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (Colour (0xff010203));
    const auto outer = getLocalBounds().toFloat().reduced (4.0f);
    performer::drawBlackGlass (g, outer, 8.0f);

    // title bar: VFD title, preset count, thin ice line under it
    theme::drawGlowText (g, "PRESET BROWSER", { 22.0f, 14.0f, 260.0f, 30.0f }, theme::vfdFont (20.0f), performer::iceText,
                         Justification::centredLeft, 0.9f);
    const String count = String ((int) shown.size()) + ((int) shown.size() == 1 ? " PRESET" : " PRESETS");
    performer::drawSilkscreen (g, count, { 22.0f + 230.0f, 16.0f, 160.0f, 26.0f }, performer::iceText.withAlpha (0.62f), 10.0f,
                               Justification::centredLeft);
    g.setColour (performer::iceLine.withAlpha (0.18f));
    g.drawHorizontalLine (50, outer.getX() + 10.0f, outer.getRight() - 10.0f);

    // category tabs
    const auto a = categoryArea();
    const int rh = categoryRowHeight();
    int y = a.getY();
    for (int i = 0; i < categories.size(); ++i)
    {
        const auto r = Rectangle<int> (a.getX(), y, a.getWidth(), rh - 3).toFloat();
        performer::drawLedTab (g, r, categories[i], categoryLed (i), i == category, i == hoverCategory);
        y += rh;
        if (i == 2)
        {
            g.setColour (performer::iceLine.withAlpha (0.14f));
            g.drawHorizontalLine (y + 2, r.getX() + 6.0f, r.getRight() - 6.0f);
            y += 8;
        }
    }
    g.setColour (performer::iceLine.withAlpha (0.14f));
    g.drawVerticalLine (a.getRight() + 10, (float) a.getY(), (float) a.getBottom());
}

void PresetBrowser::resized()
{
    const int x0 = categoryArea().getRight() + 22;
    closeBtn.setBounds (getWidth() - 114, 15, 96, 28);
    search.setBounds (x0, 60, getWidth() - x0 - 18, 30);
    list.setBounds (x0, 100, getWidth() - x0 - 16, getHeight() - 116);
}

void PresetBrowser::mouseUp (const MouseEvent& e)
{
    const int c = categoryAt (e.getPosition());
    if (c >= 0 && c != category) { category = c; rebuild(); }
}

void PresetBrowser::mouseMove (const MouseEvent& e)
{
    const int c = categoryAt (e.getPosition());
    if (c != hoverCategory) { hoverCategory = c; repaint (categoryArea()); }
}

void PresetBrowser::mouseExit (const MouseEvent&)
{
    if (hoverCategory >= 0) { hoverCategory = -1; repaint (categoryArea()); }
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
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (2.0f, 1.5f);

    // rows are etched into the glass: very dark, alternating; the selected one is backlit
    if (selected)
    {
        ColourGradient lit (Colour (0xff12213a), r.getX(), r.getY(), Colour (0xff0a1322), r.getX(), r.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (performer::iceLine.withAlpha (0.65f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (performer::iceLine);
        g.fillRect (r.withWidth (2.5f).reduced (0.0f, 5.0f).translated (1.0f, 0.0f));
    }
    else
    {
        g.setColour (row % 2 == 0 ? Colour (0xff070a10) : Colour (0xff04060a));
        g.fillRoundedRectangle (r, 3.0f);
    }

    auto t = r.reduced (14.0f, 0.0f);
    // name: VFD-style ice blue, favourites get an amber star
    auto nameR = t.removeFromLeft (240.0f);
    if (e.favourite)
    {
        theme::drawGlowText (g, String::fromUTF8 ("\xe2\x98\x85"), nameR.removeFromLeft (18.0f), theme::monoLabel (13.0f), performer::amber,
                             Justification::centredLeft, selected ? 0.8f : 0.3f);
    }
    if (selected)
        theme::drawGlowText (g, e.name.toUpperCase(), nameR, theme::monoLabel (13.0f).boldened(), Colour (0xffd8f1ff), Justification::centredLeft, 0.7f);
    else
    {
        g.setFont (theme::monoLabel (13.0f).boldened());
        g.setColour (performer::iceText.withAlpha (0.85f));
        g.drawText (e.name.toUpperCase(), nameR, Justification::centredLeft, true);
    }
    // description and location
    g.setFont (theme::monoLabel (10.5f));
    g.setColour (performer::iceText.withAlpha (selected ? 0.75f : 0.45f));
    g.drawText (e.description, t.removeFromLeft (jmax (60.0f, t.getWidth() - 190.0f)), Justification::centredLeft, true);
    g.setFont (theme::monoLabel (9.5f));
    g.setColour (performer::iceLine.withAlpha (selected ? 0.9f : 0.5f));
    g.drawText (e.where, t, Justification::centredRight, true);
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
