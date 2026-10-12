#pragma once

// Preset browser overlay (TrapWin glass card #0A0F18 / 96 %, radius 20), designed for 752 x 608:
// category tabs on the left (170 px column: all, favourites, user, the 12 channels; active = ice-blue glow),
// a search pill and the preset rows (30 px: name 13 px, description 12 px, channel tag 11 px) on the right.
// Click = load and listen, double-click / Enter (in the search field) = load and close.
//
// Keyboard: the browser never takes keyboard focus itself. Only the search field does, and only after the user
// clicks into it; it gives the focus away again on Enter, Esc, close, hide and any click outside the field, so the
// space bar / arrows always reach the DAW otherwise. While typing, Up / Down move the selection.

#include "Widgets.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (EffectorKillaAudioProcessor& p);
    ~PresetBrowser() override;

    void open();
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int preferredWidth = 752, preferredHeight = 608;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    class SearchField;

private:
    struct Entry
    {
        juce::String name, description, where;
        int channel = -1, index = -1;   // factory
        juce::File file;                // user
        bool favourite = false;
    };
    void rebuild();
    void close();
    void releaseFocus();
    void moveSelection (int delta);
    void unfocusRows();
    void load (int row);
    juce::Rectangle<int> categoryArea() const;
    int categoryAt (juce::Point<int> p) const;
    int categoryRowHeight() const;
    juce::Colour categoryLed (int category) const;
    juce::Colour entryColour (const struct Entry&) const;

    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void selectedRowsChanged (int row) override;

    EffectorKillaAudioProcessor& proc;
    std::unique_ptr<SearchField> search;
    std::unique_ptr<juce::MouseListener> clickOutside;   // releases the search focus on clicks elsewhere
    juce::ListBox list;
    juce::Rectangle<int> closeArea() const;
    bool closeHover = false;
    std::vector<Entry> all, shown;
    juce::StringArray categories;
    int category = 0;   // 0 = all, 1 = favourites, 2 = user, 3.. = channels
    bool loadingFromSelection = false;
    int hoverCategory = -1;
};

} // namespace ek::ui
