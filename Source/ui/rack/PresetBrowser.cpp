#include "PresetBrowser.h"

namespace ek::ui
{
using namespace juce;

PresetBrowser::PresetBrowser (EffectorKillaAudioProcessor& p)
    : proc (p), list ("presets", this), closeBtn (String::fromUTF8 ("CLOSE \xe2\x9c\x95"), theme::accent)
{
    search.setTextToShowWhenEmpty ("SEARCH NAME, DESCRIPTION OR CHANNEL...", theme::silverDim.withAlpha (0.6f));
    search.setFont (theme::monoLabel (13.0f));
    search.setColour (TextEditor::backgroundColourId, Colour (0xff03060c));
    search.setColour (TextEditor::textColourId, theme::silver);
    search.setColour (TextEditor::outlineColourId, theme::line);
    search.setColour (TextEditor::focusedOutlineColourId, theme::accent);
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { if (! shown.empty()) { load (jmax (0, list.getSelectedRow())); if (onClose) onClose(); } };
    search.onEscapeKey = [this] { if (onClose) onClose(); };
    addAndMakeVisible (search);

    list.setRowHeight (34);
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

Rectangle<int> PresetBrowser::categoryArea() const { return { 16, 56, 220, getHeight() - 72 }; }

int PresetBrowser::categoryAt (Point<int> p) const
{
    const auto a = categoryArea();
    if (! a.contains (p)) return -1;
    const int row = (p.y - a.getY()) / 26;
    return row >= 0 && row < categories.size() ? row : -1;
}

void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (Colour (0xff040609));
    drawCard (g, getLocalBounds().toFloat().reduced (4.0f), 10.0f);
    g.setFont (theme::stencilFont (18.0f));
    g.setColour (theme::silver);
    g.drawText ("PRESET BROWSER", 18, 14, 260, 28, Justification::centredLeft, false);
    drawSmallLabel (g, String ((int) shown.size()) + " PRESETS", { 252.0f, 16.0f, 160.0f, 24.0f }, Justification::centredLeft, theme::silverDim, 10.0f);

    const auto a = categoryArea();
    for (int i = 0; i < categories.size(); ++i)
    {
        auto r = Rectangle<int> (a.getX(), a.getY() + i * 26, a.getWidth(), 24).toFloat();
        if (i == category)
        {
            g.setColour (theme::accent.withAlpha (0.16f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (theme::accent.withAlpha (0.7f));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        }
        if (i >= 3)
        {
            g.setColour (proc.bank.getChannels()[(size_t) (i - 3)].colour);
            g.fillEllipse (r.getX() + 8.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
        }
        drawSmallLabel (g, categories[i], r.withTrimmedLeft (i >= 3 ? 24.0f : 10.0f), Justification::centredLeft,
                        i == category ? theme::silver : theme::silverDim, 11.0f);
        if (i == 2)
        {
            g.setColour (theme::line);
            g.drawHorizontalLine ((int) r.getBottom() + 1, r.getX(), r.getRight());
        }
    }
    g.setColour (theme::line);
    g.drawVerticalLine (a.getRight() + 10, (float) a.getY(), (float) a.getBottom());
}

void PresetBrowser::resized()
{
    const int x0 = categoryArea().getRight() + 22;
    search.setBounds (x0, 52, getWidth() - x0 - 120, 30);
    closeBtn.setBounds (getWidth() - 112, 52, 96, 30);
    list.setBounds (x0, 92, getWidth() - x0 - 16, getHeight() - 108);
}

void PresetBrowser::mouseUp (const MouseEvent& e)
{
    const int c = categoryAt (e.getPosition());
    if (c >= 0 && c != category) { category = c; rebuild(); }
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
    auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (2.0f, 2.0f);
    g.setColour (selected ? Colour (0xff17223a) : (row % 2 == 0 ? Colour (0xff0a0f19) : Colour (0xff080c15)));
    g.fillRoundedRectangle (r, 5.0f);
    if (selected)
    {
        g.setColour (theme::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }
    auto t = r.reduced (12.0f, 0.0f);
    g.setFont (theme::monoLabel (13.0f).boldened());
    g.setColour (e.favourite ? Colour (0xffffc861) : theme::silver);
    g.drawText ((e.favourite ? String::fromUTF8 ("\xe2\x98\x85 ") : String()) + e.name, t.removeFromLeft (230.0f), Justification::centredLeft, true);
    drawSmallLabel (g, e.description, t.removeFromLeft (jmax (60.0f, t.getWidth() - 190.0f)), Justification::centredLeft, theme::silverDim, 10.5f);
    drawSmallLabel (g, e.where, t, Justification::centredRight, theme::accent.withAlpha (0.7f), 9.5f);
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
