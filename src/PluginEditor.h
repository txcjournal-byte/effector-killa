/*
    TrapWin BEND DEMON
    PluginEditor.h - the 2.0 editor:

        +--------------------------------------------------------------------+
        | top bar: brand, DRAW/LIVE, synth slot, bars, play, presets, 3D     |
        | tool bar: tools, snap, magnet, key/scale, undo, edit, capture, .mid|
        +----------------------------------------------------+---------------+
        |  canvas (Voice Field): piano keys | ruler | lines   |  INSPECTOR    |
        |                                                    |  line /       |
        +----------------------------------------------------+  gestures /   |
        |  expression lane (collapsible)                     |  sound        |
        +----------------------------------------------------+---------------+
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
#include "ui/LineEditor.h"
#include "ui/EditToolbar.h"
#include "ui/ExpressionLane.h"
#include "ui/Inspector.h"
#include "ui/SynthBrowser.h"
#include "ui/StatusBar.h"

class BendDemonEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer,
                              private juce::ChangeListener
{
public:
    explicit BendDemonEditor (BendDemonProcessor&);
    ~BendDemonEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth  = 1040;
    static constexpr int defaultHeight = 650;
    static constexpr int minWidth      = 640;
    static constexpr int minHeight     = 400;

    void mouseDown (const juce::MouseEvent&) override;

    // used by the snapshot tool
    bd::LineEditController& getController() noexcept { return *controller; }
    bd::Inspector& getInspector() noexcept           { return *inspector; }
    bd::VoiceField& getVoiceField() noexcept         { return voiceField; }
    void setLaneOpen (bool);
    void setInspectorOpen (bool);

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void showSynthBrowser (bool);
    void showWindowSizeMenu();
    void updateStatus();
    void updateHint();
    void applyMode (bool draw);
    void saveUiState();
    int  fitWidthToScreen (int wantedWidth) const;
    void setEditorWidth (int width);
    static void releaseKeys (juce::Component&);

    BendDemonProcessor& processor;
    bd::BendDemonLookAndFeel lnf;

   #if BEND_DEMON_USE_OPENGL
    juce::OpenGLContext glContext;
   #endif

    bd::VoiceField voiceField;
    std::unique_ptr<bd::LineEditController> controller;
    bd::TopBar topBar;
    std::unique_ptr<bd::EditToolbar> toolbar;
    std::unique_ptr<bd::ExpressionLane> lane;
    std::unique_ptr<bd::Inspector> inspector;
    std::unique_ptr<bd::SynthBrowser> browser;
    bd::StatusBar statusBar;
    bd::NoticeBanner banner;
    juce::TooltipWindow tooltips { this, 600 };

    bool laneOpen = true, inspectorOpen = true, drawModeShown = true;
    uint32_t lastLoadCount = 0;
    juce::String lastLaneKey;
    double startMs = 0.0;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BendDemonEditor)
};
