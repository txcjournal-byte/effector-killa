/*
    TrapWin BEND DEMON
    SynthBrowser.h - overlay with a searchable list of scanned instrument plugins.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../host/HostedPluginManager.h"

namespace bd
{

class SynthBrowser final : public juce::Component,
                           private juce::ListBoxModel,
                           private juce::ChangeListener
{
public:
    explicit SynthBrowser (HostedPluginManager&);
    ~SynthBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // click outside the panel closes
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

    std::function<void()> onClose;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void refilter();
    void loadRow (int row);
    static bool isKnownMpeSynth (const juce::PluginDescription&);
    juce::Rectangle<int> panelBounds() const;

    HostedPluginManager& host;
    juce::Array<juce::PluginDescription> all, filtered;
    juce::TextEditor search;
    juce::ListBox list { "synths", this };
    juce::TextButton scanButton { "SCAN" }, rescanButton { "FULL RESCAN" }, loadButton { "LOAD" }, closeButton { "X" };
    juce::String status;
};

} // namespace bd
