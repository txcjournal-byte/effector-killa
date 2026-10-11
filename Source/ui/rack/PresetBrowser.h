#pragma once

// Preset browser overlay (TrapWin glass card #0A0F18 / 96 %, radius 20): category pills on the left (all, favourites,
// user, the 12 channels; active = ice-blue glow), a search pill and the preset rows (Plex Mono names, channel colour dot,
// sparkle for favourites) on the right. Click = load and listen, double-click / Enter = load and close.

#include "Widgets.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (EffectorKillaAudioProcessor& p);

    void open();
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Entry
    {
        juce::String name, description, where;
        int channel = -1, index = -1;   // factory
        juce::File file;                // user
        bool favourite = false;
    };
    void rebuild();
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
    juce::TextEditor search;
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
