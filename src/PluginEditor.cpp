#include "PluginEditor.h"
#include "ui/HostKeys.h"
#include "ui/Palette.h"
#include "ui/Fonts.h"

using namespace bd;

namespace
{
    constexpr float kTop = 52.0f, kTool = 44.0f, kLane = 104.0f, kInspector = 272.0f, kStatus = 26.0f;   // at 1040 px wide
}

//==============================================================================
BendDemonEditor::BendDemonEditor (BendDemonProcessor& p)
    : AudioProcessorEditor (p), processor (p), voiceField (p.getVisualFifo()), topBar (p)
{
    setLookAndFeel (&lnf);
    auto& store = p.getPatternStore();

    controller = std::make_unique<LineEditController> (voiceField, store, p.getAPVTS(),
                                                       [&p] { return p.getGestureSettings(); },
                                                       [&p] { return p.getTempo(); });
    voiceField.setPatternStore (&store);
    voiceField.setEditController (controller.get());

    toolbar   = std::make_unique<EditToolbar> (p, *controller, voiceField);
    lane      = std::make_unique<ExpressionLane> (voiceField, store);
    inspector = std::make_unique<Inspector> (p, *controller);

    addAndMakeVisible (voiceField);
    addAndMakeVisible (*lane);
    addAndMakeVisible (*inspector);
    addAndMakeVisible (*toolbar);
    addAndMakeVisible (topBar);
    addAndMakeVisible (statusBar);
    addAndMakeVisible (topBar.getDimensionButton());
    addChildComponent (banner);

    auto toast = [this] (const juce::String& m) { voiceField.showToast (m); };
    controller->onMessage = toast;
    toolbar->onMessage = toast;
    processor.onCaptureComplete = [this] (int n)
    {
        voiceField.showToast (n > 0 ? "Captured " + juce::String (n) + " lines. Mute the piano-roll notes so they don't double."
                                    : juce::String ("Capture finished: nothing was played"));
    };

    toolbar->onLaneToggled = [this] (bool b) { setLaneOpen (b); };
    inspector->onLoadSynth = [this] { showSynthBrowser (true); };
    inspector->onTabChanged = [this] { saveUiState(); };
    lane->onModeChanged = [this] { saveUiState(); };

    topBar.onLoadSynth = [this] { showSynthBrowser (true); };
    topBar.onWindowSizeMenu = [this] { showWindowSizeMenu(); };
    topBar.onBarsChanged = [this] (int b) { if (b != processor.getPatternStore().get().bars) controller->setBars (b); };
    topBar.onDimensionToggled = [this]
    {
        const bool on = topBar.getDimensionButton().getToggleState();
        voiceField.setDimension (on);
        processor.getUiState().setProperty ("dimension", on, nullptr);
    };

    browser = std::make_unique<SynthBrowser> (processor.getHost());
    browser->onClose = [this] { showSynthBrowser (false); };
    addChildComponent (*browser);

    processor.getHost().addChangeListener (this);

    // ---------------------------------------------------------------- restore UI state
    auto ui = processor.getUiState();
    const int savedWidth = (int) ui.getProperty ("width", 1040);
    const bool dim = ui.getProperty ("dimension", false);
    topBar.getDimensionButton().setToggleState (dim, juce::dontSendNotification);
    voiceField.setDimension (dim);
    voiceField.setBars ((int) ui.getProperty ("bars", 4));
    laneOpen      = ui.getProperty ("laneOpen", true);
    inspectorOpen = ui.getProperty ("inspectorOpen", true);
    toolbar->setLaneOpen (laneOpen);
    toolbar->setSnapIndex ((int) ui.getProperty ("snap", 4));
    toolbar->getMagnet().setValue ((float) (double) ui.getProperty ("magnet", 0.6), true);
    controller->setMagnet (toolbar->getMagnet().getValue());
    controller->setTool ((LineEditController::Tool) juce::jlimit (0, 3, (int) ui.getProperty ("tool", 0)));
    inspector->setTab ((Inspector::Tab) juce::jlimit (0, 2, (int) ui.getProperty ("inspectorTab", 0)));
    lane->setMode ((ExpressionLane::Mode) juce::jlimit (0, 3, (int) ui.getProperty ("laneMode", 0)));

    voiceField.setDrawMode (true);
    if (ui.hasProperty ("viewBeats"))
    {
        VoiceField::PatternView v;
        v.start = ui.getProperty ("viewStart", 0.0);
        v.beats = ui.getProperty ("viewBeats", 16.0);
        v.lo = (float) (double) ui.getProperty ("viewLo", 48.0);
        v.hi = (float) (double) ui.getProperty ("viewHi", 84.0);
        voiceField.setPatternView (v);
    }
    else
        voiceField.fitToPattern();
    lastLoadCount = store.getLoadCount();

    // ---------------------------------------------------------------- size
   #if BEND_DEMON_USE_OPENGL
    glContext.setComponentPaintingEnabled (true);
    glContext.setContinuousRepainting (false);
    glContext.attachTo (*this);
   #endif

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 2560, 1600);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) defaultWidth / (double) defaultHeight);

    const bool hasSaved = ui.hasProperty ("width");
    setEditorWidth (fitWidthToScreen (hasSaved ? savedWidth : 1040));

    topBar.addMouseListener (this, false);

    // Keys belong to the host: buttons, sliders and menus never take keyboard focus (space =
    // FL Studio play / stop). Only the canvas takes keys, after a click inside it.
    setWantsKeyboardFocus (false);
    for (auto* child : getChildren())
        if (child != browser.get())
            releaseKeys (*child);

    drawModeShown = ! processor.isDrawMode();   // force applyMode
    applyMode (processor.isDrawMode());

    sizeRestored = true;
    startMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

BendDemonEditor::~BendDemonEditor()
{
    if (keyRoot != nullptr) keyRoot->removeKeyListener (this);
    stopTimer();
    saveUiState();
    processor.onCaptureComplete = nullptr;
    processor.getHost().removeChangeListener (this);

   #if BEND_DEMON_USE_OPENGL
    glContext.detach();
   #endif

    setLookAndFeel (nullptr);
}

void BendDemonEditor::saveUiState()
{
    auto ui = processor.getUiState();
    ui.setProperty ("bars", voiceField.getBars(), nullptr);
    ui.setProperty ("laneOpen", laneOpen, nullptr);
    ui.setProperty ("inspectorOpen", inspectorOpen, nullptr);
    ui.setProperty ("snap", toolbar->getSnapIndex(), nullptr);
    ui.setProperty ("magnet", (double) controller->getMagnet(), nullptr);
    ui.setProperty ("tool", (int) controller->getTool(), nullptr);
    ui.setProperty ("inspectorTab", (int) inspector->getTab(), nullptr);
    ui.setProperty ("laneMode", (int) lane->getMode(), nullptr);
    const auto v = voiceField.getPatternView();
    ui.setProperty ("viewStart", v.start, nullptr);
    ui.setProperty ("viewBeats", v.beats, nullptr);
    ui.setProperty ("viewLo", (double) v.lo, nullptr);
    ui.setProperty ("viewHi", (double) v.hi, nullptr);
}

//==============================================================================
void BendDemonEditor::applyMode (bool draw)
{
    if (draw == drawModeShown) return;
    drawModeShown = draw;

    voiceField.setDrawMode (draw);
    voiceField.setWantsKeyboardFocus (draw);
    toolbar->setEditingEnabled (draw);
    if (! draw && inspector->getTab() == Inspector::Tab::note)
        inspector->setTab (Inspector::Tab::shapes);
    updateHint();
    resized();
}

void BendDemonEditor::timerCallback()
{
    const float t = (float) ((juce::Time::getMillisecondCounterHiRes() - startMs) * 0.001);

    applyMode (processor.isDrawMode());

    auto& store = processor.getPatternStore();
    if (store.getLoadCount() != lastLoadCount)
    {
        lastLoadCount = store.getLoadCount();
        voiceField.fitToPattern();
    }

    auto& apvts = processor.getAPVTS();
    auto val = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    voiceField.setScale ((int) std::lround (val (ids::scaleKey)), (int) std::lround (val (ids::scaleType)), val (ids::scaleLockOn) > 0.5f);
    const auto tv = processor.getVisualFifo().getTransportNow();
    voiceField.setSequencerPosition (tv.ppq, processor.isSequencerRunning());
    voiceField.setGridDivision (controller->getSnap());

    // a synth without MPE was loaded: say what happened and offer the fix
    if (processor.hasNonMpeNotice() && ! banner.isVisible())
    {
        banner.show ("This synth doesn't support MPE - BEND DEMON switched to mono slides. "
                     "For slides on every note use Serum 2, Vital or the built-in sound.",
                     "USE BUILT-IN SOUND",
                     [this] { processor.setUseInternalSynth (true); processor.dismissNonMpeNotice(); banner.setVisible (false);
                              if (auto* p = processor.getAPVTS().getParameter (ids::seqMono)) p->setValueNotifyingHost (0.0f); },
                     [this] { processor.dismissNonMpeNotice(); banner.setVisible (false); });
        banner.toFront (false);
    }
    else if (! processor.hasNonMpeNotice() && banner.isVisible())
        banner.setVisible (false);

    updateStatus();

    voiceField.tick (t);
    topBar.tick (t);
    toolbar->tick (t);
    inspector->tick (t);
    if (lane->isVisible())
    {
        // the lane only changes with the pattern, the selection or the view
        const auto v = voiceField.getPatternView();
        const auto key = juce::String ((int) store.getVersion()) + "|" + juce::String (v.start, 4) + "|" + juce::String (v.beats, 4);
        if (key != lastLaneKey) { lastLaneKey = key; lane->repaint(); }
    }
}

void BendDemonEditor::updateStatus()
{
    // left: what the thing under the mouse does (its tooltip, in plain words)
    juce::String hint;
    auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    if (c == &voiceField || (c != nullptr && voiceField.isParentOf (c)))
    {
        if (! processor.isDrawMode())
            hint = "LIVE - play notes in FL Studio: the shapes that are switched on bend them (SHAPES tab)";
        else if (! voiceField.canEdit())
            hint = "3D VIEW - turn 3D off to draw";
        else switch (controller->getTool())
        {
            case LineEditController::Tool::pencil: hint = "PENCIL - click to place a note, drag to draw a slide. Shift = whole semitones, right-click = delete"; break;
            case LineEditController::Tool::line:   hint = "LINE - click and drag a straight slide, click further right to add a corner, Esc to finish"; break;
            case LineEditController::Tool::select: hint = "SELECT - drag to move, Alt-drag to copy, drag the ends to resize, drag a dot to reshape"; break;
            case LineEditController::Tool::eraser: hint = "ERASER - click or sweep over notes to delete them"; break;
        }
    }
    else
    {
        for (auto* p = c; p != nullptr && p != this; p = p->getParentComponent())
            if (auto* tc = dynamic_cast<juce::TooltipClient*> (p))
            {
                hint = tc->getTooltip();
                if (hint.isNotEmpty()) break;
            }
    }
    auto& cap = processor.getCaptureRecorder();
    if (cap.isArmed())
        hint = cap.isRecording() ? "RECORDING - playing your piano-roll notes into the pattern ..."
                                 : "RECORDING ARMED - starts at the next loop / bar line when FL plays";

    // right: what is loaded
    const auto& pat = processor.getPatternStore().get();
    const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    juce::String info = processor.getPresets().getCurrentName() + dot + juce::String (pat.bars) + (pat.bars == 1 ? " bar" : " bars")
                        + dot + juce::String ((int) pat.lines.size()) + " notes";
    if (processor.getAPVTS().getRawParameterValue (ids::seqMono)->load() > 0.5f)
        info << dot << "mono slides";
    statusBar.setTexts (hint, info);
}

void BendDemonEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    topBar.refresh();
    updateHint();
}

void BendDemonEditor::updateHint()
{
    auto& host = processor.getHost();
    if (processor.isDrawMode())
        voiceField.setHintText (juce::String (juce::CharPointer_UTF8 ("Draw a line with the pencil \xe2\x80\x93 it plays as one note that can slide anywhere.\n"))
                                + "Then press PLAY (or play in FL Studio). Or pick a pattern from the presets.");
    else if (processor.isMidiOutMode())
        voiceField.setHintText ("MIDI OUT MODE\nRoute BEND DEMON's MIDI output to an MPE synth on another channel.");
    else if (! host.hasPlugin() || processor.isUsingInternalSynth())
        voiceField.setHintText ("LIVE MODE\nPlay or write a melody in the piano roll: the gestures bend it.\n"
                                "Sound: " + processor.getSynthSlotText());
    else
        voiceField.setHintText ("PLAY A MELODY\n" + host.getPluginName() + " is listening in MPE.");
}

void BendDemonEditor::parentHierarchyChanged()
{
    // Listen on the top-level component of the plugin window: when nothing has focus JUCE sends
    // keys there, and when the canvas has focus unused keys bubble up to it.
    auto* top = getTopLevelComponent();
    if (top == keyRoot.getComponent()) return;
    if (keyRoot != nullptr) keyRoot->removeKeyListener (this);
    keyRoot = top;
    if (keyRoot != nullptr) keyRoot->addKeyListener (this);
}

bool BendDemonEditor::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // typing in a text field (synth search, preset name) keeps every key
    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    // Space = play / stop in FL Studio. Never keep it: hand the keyboard back to the host and
    // replay the key there so it works wherever the mouse is in the plugin.
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
        return bd::hostkeys::forwardToHost (*this, key);

    return false;
}

void BendDemonEditor::releaseKeys (juce::Component& c)
{
    if (dynamic_cast<juce::TextEditor*> (&c) != nullptr)
        return;   // text fields (preset name, synth search) still need typing

    c.setWantsKeyboardFocus (false);
    c.setMouseClickGrabsKeyboardFocus (false);
    for (auto* child : c.getChildren())
        releaseKeys (*child);
}

void BendDemonEditor::showSynthBrowser (bool show)
{
    browser->setVisible (show);
    if (show)
    {
        browser->toFront (true);
        browser->grabKeyboardFocus();
    }
    else
    {
        browser->giveAwayKeyboardFocus();
        juce::Component::unfocusAllComponents();
    }
}

void BendDemonEditor::setLaneOpen (bool b)
{
    laneOpen = b;
    toolbar->setLaneOpen (b);
    resized();
    saveUiState();
}

void BendDemonEditor::setInspectorOpen (bool b)
{
    inspectorOpen = b;
    resized();
    saveUiState();
}

//==============================================================================
int BendDemonEditor::fitWidthToScreen (int wanted) const
{
    juce::Rectangle<int> area (1366, 728);
    auto& displays = juce::Desktop::getInstance().getDisplays();
    if (auto* d = (isShowing() ? displays.getDisplayForRect (getScreenBounds()) : displays.getPrimaryDisplay()))
        area = d->userArea;

    const int maxW = juce::jmin (juce::roundToInt (area.getWidth() * 0.85),
                                 juce::roundToInt (area.getHeight() * 0.80 * defaultWidth / (double) defaultHeight));
    return juce::jlimit (minWidth, juce::jmax (minWidth, maxW), wanted);
}

void BendDemonEditor::setEditorWidth (int w)
{
    w = juce::jlimit (minWidth, 2560, w);
    setSize (w, juce::roundToInt (w * (double) defaultHeight / defaultWidth));
}

void BendDemonEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showWindowSizeMenu();
}

void BendDemonEditor::showWindowSizeMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Window size");
    const int widths[] = { 640, 800, 1040, 1280, 1440, 1680 };
    const char* names[] = { "62 %", "77 %", "100 %", "123 %", "138 %", "162 %" };
    for (int i = 0; i < 6; ++i)
        m.addItem (i + 1, names[i], true, std::abs (getWidth() - widths[i]) < 8);
    m.addSeparator();
    m.addItem (100, "Fit to screen");

    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                     [safe = juce::Component::SafePointer<BendDemonEditor> (this), widths] (int r)
                     {
                         if (safe == nullptr || r == 0) return;
                         if (r == 100) safe->setEditorWidth (safe->fitWidthToScreen (2560));
                         else          safe->setEditorWidth (widths[r - 1]);
                     });
}

//==============================================================================
void BendDemonEditor::resized()
{
    const float s = (float) getWidth() / (float) defaultWidth;
    auto r = getLocalBounds();

    topBar.setBounds (r.removeFromTop (juce::roundToInt (kTop * s)));
    statusBar.setBounds (r.removeFromBottom (juce::roundToInt (kStatus * s)));
    toolbar->setBounds (r.removeFromTop (juce::roundToInt (kTool * s)));

    inspector->setVisible (inspectorOpen);
    if (inspectorOpen)
        inspector->setBounds (r.removeFromRight (juce::roundToInt (kInspector * s)));

    const bool showLane = laneOpen && drawModeShown;
    lane->setVisible (showLane);
    if (showLane)
        lane->setBounds (r.removeFromBottom (juce::roundToInt (kLane * s)));

    voiceField.setBounds (r);
    {
        // 3D view toggle in the canvas corner (above the piano keys)
        topBar.getDimensionButton().setBounds (juce::Rectangle<int> (r.getX() + 3, r.getY() + 3,
                                                                      juce::roundToInt (42 * s), juce::roundToInt (18 * s)));
        topBar.getDimensionButton().toFront (false);
    }
    banner.setBounds (r.reduced (juce::roundToInt (40 * s), 0).withHeight (juce::roundToInt (46 * s))
                       .translated (0, juce::roundToInt (26 * s)));
    browser->setBounds (getLocalBounds());

    if (sizeRestored)
        processor.getUiState().setProperty ("width", getWidth(), nullptr);
}

void BendDemonEditor::paint (juce::Graphics& g)
{
    g.fillAll (pal::ground0);
}
