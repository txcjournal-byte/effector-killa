/*
    TrapWin BEND DEMON
    PluginEditor.h - top bar, VOICE FIELD, gesture dock.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#if BEND_DEMON_USE_OPENGL
 #include <juce_opengl/juce_opengl.h>
#endif

#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/TopBar.h"
#include "ui/VoiceField.h"
#include "ui/GesturePad.h"
#include "ui/GlowKnob.h"
#include "ui/SynthBrowser.h"

class BendDemonEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer,
                              private juce::ChangeListener
{
public:
    explicit BendDemonEditor (BendDemonProcessor&);
    ~BendDemonEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth  = 1280;
    static constexpr int defaultHeight = 800;
    static constexpr int minWidth      = 960;
    static constexpr int minHeight     = 600;

private:
    enum class Section { glide, vibrato, dive, motion, expression };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showSection (Section);
    void layoutDock (juce::Rectangle<int>);
    void showSynthBrowser (bool);
    void updateHint();

    BendDemonProcessor& processor;
    bd::BendDemonLookAndFeel lnf;

   #if BEND_DEMON_USE_OPENGL
    juce::OpenGLContext glContext;
   #endif

    bd::TopBar topBar;
    bd::VoiceField voiceField;

    // gesture pads
    std::vector<std::unique_ptr<bd::GesturePad>> pads;

    // per-section controls
    struct SectionControls
    {
        Section section;
        juce::String title;
        std::vector<juce::Component*> items;
    };

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<SectionControls> sections;
    Section currentSection = Section::glide;

    // always-visible controls
    std::unique_ptr<bd::ChoiceBox> scaleKey, scaleType, bendRange;
    std::unique_ptr<bd::PillToggle> scaleLock, quantize, midiOut;
    std::unique_ptr<bd::GlowKnob> voicesKnob, gainKnob;

    juce::OwnedArray<juce::TextButton> tabs;   // GLIDE / VIBRATO / DIVE / MOTION / EXPRESSION
    std::unique_ptr<bd::SynthBrowser> browser;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Rectangle<int> dockArea, sectionArea, globalArea;
    double startMs = 0.0;
    float lastFrameTime = 0.0f;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BendDemonEditor)
};
