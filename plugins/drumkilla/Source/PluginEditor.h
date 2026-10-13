#pragma once

#include "PluginProcessor.h"
#include "UI/Look.h"
#include "UI/Widgets.h"

//==============================================================================
// Where things sit on the artwork (design space 1536 x 1024, Resources/background.jpg)
namespace Layout
{
    inline const juce::Rectangle<int> leftPanel  { 56, 320, 198, 518 };
    inline const juce::Rectangle<int> rightPanel { 1250, 320, 230, 518 };
    inline const juce::Rectangle<int> bottomBar  { 396, 852, 688, 80 };
    inline const juce::Rectangle<int> head       { 470, 180, 540, 668 };   // animated part of the artwork
    inline const juce::Rectangle<int> mouth      { 568, 402, 330, 258 };
    inline const juce::Rectangle<int> jaw        { 552, 650, 368, 162 };   // Resources/jaw.png
}

//==============================================================================
// The head: drag the jaw down = KILL, the mouth shows the waveform (input grey, output red)
class HeadView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HeadView (K808Processor&);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // called by the editor timer
    void tick (float outLevel, bool phoneOn, const juce::String& status);

private:
    bool hitsJaw (juce::Point<float>) const;
    float jawDrop() const noexcept { return kill * 44.0f + beat * 7.0f; }

    K808Processor& processor;
    juce::RangedAudioParameter& killParam;
    juce::ParameterAttachment killAttachment;
    juce::Image background, jawImage;

    float kill = 0.0f, beat = 0.0f, slowLevel = 0.0f, dragStart = 0.0f;
    bool hover = false, dragging = false, phoneOn = false;
    std::vector<float> scopeIn, scopeOut;
    juce::String status;
    juce::SharedResourcePointer<Look::Fonts> fonts;
};

//==============================================================================
// Left panel: input, crossover, focus and the sub switches
class SubPanel : public juce::Component
{
public:
    explicit SubPanel (K808Processor&);
    void paint (juce::Graphics&) override;

private:
    UI::Knob input, crossover, focus, focusFreq;
    UI::LedToggle subMono, subCut, phase, phone;
    juce::SharedResourcePointer<Look::Fonts> fonts;
};

//==============================================================================
// Preset table: categories on the left, presets on the right, stars for favourites
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (K808Processor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void open();
    void showCategory (const juce::String& category);
    std::function<void()> onChange, onSaveAs;

private:
    struct Star : juce::Button
    {
        Star() : Button ("star") {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
        bool on = false;
    };

    void rebuild();

    K808Processor& processor;
    juce::String category;
    juce::StringArray categoryNames;
    juce::OwnedArray<UI::FlatButton> categoryButtons, presetButtons, actionButtons;
    juce::OwnedArray<Star> stars;
    UI::FlatButton closeButton { "X" };
    juce::SharedResourcePointer<Look::Fonts> fonts;
};

//==============================================================================
class Canvas : public juce::Component
{
public:
    Canvas();
    void paint (juce::Graphics&) override;

private:
    juce::Image background;
};

//==============================================================================
class K808Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit K808Editor (K808Processor&);
    ~K808Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

    static constexpr int designWidth = 1536;
    static constexpr int designHeight = 1024;

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void savePresetAs();
    void refreshPresetLabel();

    K808Processor& processor;
    Look::LookAndFeel lnf;
    juce::TooltipWindow tooltips { nullptr, 600 };
    Canvas canvas;

    HeadView head;
    SubPanel subPanel;
    UI::Knob duck, release, clip, knee, ceiling, output;
    UI::ChoiceSelector satMode;
    UI::Knob hit;
    UI::FlatButton prevButton { "<" }, nextButton { ">" }, presetButton { "" }, saveButton { "SAVE" };
    PresetBrowser browser;

    float peakHoldDb = -100.0f, clipHoldDb = 0.0f;
    int peakHoldTicks = 0, clipHoldTicks = 0;
    juce::String shownPreset;
    bool shownModified = false;

    std::unique_ptr<juce::AlertWindow> saveDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (K808Editor)
};
