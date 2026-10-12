#include "PluginEditor.h"
#include "ui/rack/CardStyle.h"
#include "ui/rack/CableStyle.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "1.0.0"   // snapshot tool (not a plugin target)
#endif

using namespace ek;
namespace hw = ek::ui::hw;

namespace
{
// tick() is optional on the standalone units
template <typename T> auto tickIfPossible (T& t, int) -> decltype (t.tick(), void()) { t.tick(); }
template <typename T> void tickIfPossible (T&, long) {}
} // namespace

// ============================================================================================
void EffectorKillaAudioProcessorEditor::Frame::paint (juce::Graphics& g)
{
    cache.draw (g, getLocalBounds(), [this] (juce::Graphics& bg)
    {
        const float W = (float) getWidth(), H = (float) getHeight();
        bg.fillAll (hw::col::bg);

        // very subtle deep-blue radial glow (trapwin.com background): a wide one from the top centre,
        // a fainter one rising from the lower right
        {
            juce::ColourGradient top (juce::Colour (0xff4676c4).withAlpha (0.16f), W * 0.5f, -H * 0.18f,
                                      juce::Colour (0xff4676c4).withAlpha (0.0f), W * 0.5f + W * 0.62f, -H * 0.18f, true);
            top.addColour (0.45, juce::Colour (0xff4676c4).withAlpha (0.06f));
            bg.setGradientFill (top);
            bg.fillRect (getLocalBounds().toFloat());
            juce::ColourGradient low (juce::Colour (0xff2b4d8f).withAlpha (0.10f), W * 0.86f, H * 1.05f,
                                      juce::Colour (0xff2b4d8f).withAlpha (0.0f), W * 0.86f - W * 0.45f, H * 1.05f, true);
            bg.setGradientFill (low);
            bg.fillRect (getLocalBounds().toFloat());
        }

        // sparse sparkles + star dust (deterministic, so snapshots are stable)
        juce::Random rnd (0x7a11);
        for (int i = 0; i < 90; ++i)
        {
            const juce::Point<float> p (rnd.nextFloat() * W, rnd.nextFloat() * H);
            bg.setColour (juce::Colour (0xffc9d8f5).withAlpha (0.06f + 0.16f * rnd.nextFloat()));
            const float d = 0.8f + rnd.nextFloat() * 1.0f;
            bg.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
        }
        for (int i = 0; i < 26; ++i)
        {
            const juce::Point<float> p (rnd.nextFloat() * W, rnd.nextFloat() * H);
            const float big = rnd.nextFloat();
            const float r = big > 0.85f ? 5.0f + 3.0f * rnd.nextFloat() : 2.0f + 2.2f * rnd.nextFloat();
            ek::card::paintSparkle (bg, p, r, hw::col::ice.withAlpha (big > 0.85f ? 0.55f : 0.22f + 0.2f * rnd.nextFloat()),
                                    big > 0.85f ? 0.7f : 0.0f);
        }
    });
}

// ============================================================================================
void EffectorKillaAudioProcessorEditor::CableOverlay::paint (juce::Graphics& g)
{
    if (source < 0) return;
    ek::ui::cable::paintOverlay (g, ek::ui::modSourceColour (source), from, to, target);   // ui/rack/CableStyle
}

// ============================================================================================
void EffectorKillaAudioProcessorEditor::KillFlash::trigger()
{
    if (hw::reduceMotion()) return;
    startMs = juce::Time::getMillisecondCounterHiRes();
    tick();
}

void EffectorKillaAudioProcessorEditor::KillFlash::tick()
{
    const double t = (juce::Time::getMillisecondCounterHiRes() - startMs) / 220.0;
    const float a = t >= 1.0 || t < 0.0 ? 0.0f : (float) (1.0 - t) * (float) (1.0 - t);
    if (std::abs (a - shown) < 0.002f) return;
    shown = a;
    setVisible (a > 0.0f);
    repaint();
}

void EffectorKillaAudioProcessorEditor::KillFlash::paint (juce::Graphics& g)
{
    if (shown <= 0.0f) return;
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (hw::col::ice.withAlpha (0.12f * shown));
    g.fillRoundedRectangle (r, 20.0f);
    g.setColour (hw::col::ice.withAlpha (0.6f * shown));
    g.drawRoundedRectangle (r.reduced (1.0f), 19.0f, 1.5f);
}

// ============================================================================================
EffectorKillaAudioProcessorEditor::EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), toolbar (p), master (p), modUnit (p),
      rackView (p), gearList (p), browser (p)
{
    setOpaque (true);
    addAndMakeVisible (content);
    content.addAndMakeVisible (frame);
    content.addAndMakeVisible (toolbar);
    content.addAndMakeVisible (master);
    content.addAndMakeVisible (modUnit);

    rackViewport.setViewedComponent (&rackView, false);
    rackViewport.setScrollBarsShown (true, false);
    rackViewport.setScrollBarThickness (8);
    rackViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    content.addAndMakeVisible (rackViewport);
    content.addChildComponent (killFlash);
    content.addAndMakeVisible (gearList);
    content.addChildComponent (browser);
    content.addAndMakeVisible (cableOverlay);   // topmost

    toolbar.onBrowse = [this] { showBrowser (! browser.isVisible()); };
    toolbar.isBrowserShown = [this] { return browser.isVisible(); };
    toolbar.onSave = [this] { showSaveDialog(); };
    toolbar.onSettings = [this] { showSettingsMenu(); };
    toolbar.onInfo = [this] { showInfoMenu(); };
    toolbar.onToggleLibrary = [this] { setGearShown (! processor.showGear); };
    toolbar.isLibraryShown = [this] { return processor.showGear; };
    master.onKillFlash = [this] { killFlash.trigger(); };
    master.setReduceMotion (hw::reduceMotion());
    master.onSetScale = [this] (float s) { setUiScale (s); };
    browser.onClose = [this] { showBrowser (false); };
    auto say = [this] (const juce::String& m) { toolbar.showMessage (m); };
    rackView.onMessage = say;
    // MOD patching + knob right-click (modulation list, MACRO LEARN) + modulation rings
    modUnit.onHandleDrag = [this] (int src, juce::Point<int> screen) { modHandleDragged (src, screen, false); };
    modUnit.onHandleDrop = [this] (int src, juce::Point<int> screen) { modHandleDragged (src, screen, true); };
    rackView.onKnobRightClick = [this] (int slot, int param, juce::Point<int>) { showKnobMenu (slot, param); };
    rackView.paintKnobOverlay = [this] (juce::Graphics& g, juce::Rectangle<float> face, int slot, int param)
    {
        paintKnobOverlay (g, face, slot, param);
    };
    gearList.onMessage = say;

    getLookAndFeel().setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf20a0f18));
    getLookAndFeel().setColour (juce::TooltipWindow::textColourId, hw::col::print);
    getLookAndFeel().setColour (juce::TooltipWindow::outlineColourId, hw::col::ice.withAlpha (0.35f));
    getLookAndFeel().setColour (juce::ScrollBar::thumbColourId, hw::col::ice.withAlpha (0.3f));
    getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0a0f18));
    getLookAndFeel().setColour (juce::PopupMenu::textColourId, hw::col::print);
    getLookAndFeel().setColour (juce::PopupMenu::headerTextColourId, hw::col::ice);
    getLookAndFeel().setColour (juce::PopupMenu::highlightedBackgroundColourId, hw::col::ice.withAlpha (0.22f));
    getLookAndFeel().setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setWantsKeyboardFocus (true);
    addMouseListener (this, true);
    processor.addChangeListener (this);

    setResizable (true, true);
    updateConstrainer();
    layoutBody();
    setUiScale (processor.uiScale);
    startTimerHz (30);
}

EffectorKillaAudioProcessorEditor::~EffectorKillaAudioProcessorEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
    removeMouseListener (this);
    ek::ui::closeEditorOverlays (&processor);   // open menus + dialogs (ui/SharedResources)
}

void EffectorKillaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (hw::col::bg);
}

float EffectorKillaAudioProcessorEditor::autoScale() const
{
    auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                    ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea
                    : juce::Rectangle<int> (1920, 1080);
    // always fit the wide (library) layout so showing the library never pushes the window off the screen
    const float s = juce::jmin ((float) area.getWidth() * 0.86f / kWidth, (float) area.getHeight() * 0.86f / kHeight);
    return juce::jlimit (0.6f, 1.5f, std::floor (s * 20.0f) / 20.0f);
}

void EffectorKillaAudioProcessorEditor::updateConstrainer()
{
    if (auto* c = getConstrainer())
    {
        const int w = getBaseWidth();
        c->setFixedAspectRatio ((double) w / (double) kHeight);
        c->setSizeLimits (juce::roundToInt (w * 0.6), juce::roundToInt (kHeight * 0.6), w * 2, kHeight * 2);
    }
}

void EffectorKillaAudioProcessorEditor::setUiScale (float s)
{
    processor.uiScale = s;
    const float scale = s <= 0.0f ? autoScale() : juce::jlimit (0.6f, 2.0f, s);
    settingSize = true;
    setSize (juce::roundToInt ((float) getBaseWidth() * scale), juce::roundToInt ((float) kHeight * scale));
    settingSize = false;
}

void EffectorKillaAudioProcessorEditor::resized()
{
    const float scale = currentScale();
    if (scale <= 0.0f) return;
    content.setBounds (0, 0, getBaseWidth(), kHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    if (! settingSize && isShowing()) processor.uiScale = scale; // dragged the corner: remember that size
}

void EffectorKillaAudioProcessorEditor::layoutBody()
{
    const int w = getBaseWidth();
    const bool lib = libraryShown();
    content.setBounds (0, 0, w, kHeight);
    frame.setBounds (0, 0, w, kHeight);
    frame.invalidate();
    toolbar.setBounds (toolbarArea (w));
    toolbar.setCentreX (rackArea (lib).getCentreX());
    master.setBounds (masterArea (lib));
    modUnit.setBounds (modUnitArea (lib));

    const auto rack = rackArea (lib);
    rackViewport.setBounds (rack);
    killFlash.setBounds (rack);
    rackView.setSize (rack.getWidth() - rackViewport.getScrollBarThickness(), juce::jmax (rack.getHeight(), rackView.getIdealHeight()));
    gearList.setVisible (lib);
    gearList.setBounds (gearArea());
    // the browser covers the centre column (MOD card + chain); library and MASTER stay visible
    browser.setBounds (browserArea (lib));
    cableOverlay.setBounds (0, 0, w, kHeight);
    cableOverlay.toFront (false);
}

void EffectorKillaAudioProcessorEditor::setGearShown (bool shown)
{
    if (processor.showGear == shown) return;
    const float scale = getHeight() > 0 ? currentScale() : 1.0f;
    processor.showGear = shown;
    updateConstrainer();
    layoutBody();
    settingSize = true;
    setSize (juce::roundToInt ((float) getBaseWidth() * scale), juce::roundToInt ((float) kHeight * scale));
    settingSize = false;
    resized();
}

void EffectorKillaAudioProcessorEditor::showBrowser (bool shown)
{
    if (shown == browser.isVisible()) return;
    browser.setVisible (shown);
    if (shown) { browser.toFront (true); browser.open(); }
    else grabKeyboardFocus();
}

void EffectorKillaAudioProcessorEditor::tickAll()
{
    // structure changes normally arrive through the change message; checking here too keeps the
    // rack right even when the host delays messages (cheap: compares the slot types only)
    rackView.refresh (false);
    const int h = rackArea().getHeight();
    const int want = juce::jmax (h, rackView.getIdealHeight());
    if (rackView.getHeight() != want) rackView.setSize (rackView.getWidth(), want);
    toolbar.tick();
    tickIfPossible (modUnit, 0);   // (MasterColumn runs its own timer)
    rackView.tick();
    killFlash.tick();
}

void EffectorKillaAudioProcessorEditor::setReduceMotion (bool on)
{
    hw::setReduceMotion (on);
    master.setReduceMotion (on);
}

void EffectorKillaAudioProcessorEditor::timerCallback()
{
    if (! focusGrabbed && isShowing()) { focusGrabbed = true; grabKeyboardFocus(); }
    tickAll();
}

void EffectorKillaAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rackView.refresh();
    rackView.setSize (rackView.getWidth(), juce::jmax (rackArea().getHeight(), rackView.getIdealHeight()));
    toolbar.refresh();
}

void EffectorKillaAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent != nullptr && dynamic_cast<juce::TextEditor*> (e.eventComponent) != nullptr) return;
    if (! hasKeyboardFocus (true)) grabKeyboardFocus();
}

bool EffectorKillaAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    const auto code = k.getKeyCode();
    const auto mods = k.getModifiers();
    if (code == juce::KeyPress::escapeKey && browser.isVisible()) { showBrowser (false); return true; }
    if (code == juce::KeyPress::spaceKey) { processor.kill(); return true; }
    if (code == juce::KeyPress::leftKey) { processor.stepPreset (-1); return true; }
    if (code == juce::KeyPress::rightKey) { processor.stepPreset (1); return true; }
    if (code == juce::KeyPress::upKey) { processor.stepChannel (1); return true; }
    if (code == juce::KeyPress::downKey) { processor.stepChannel (-1); return true; }
    if (mods.isCommandDown() && (code == 'Z' || code == 'z'))
    {
        if (mods.isShiftDown()) processor.redo(); else processor.undo();
        return true;
    }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y')) { processor.redo(); return true; }
    if (mods.isCommandDown() && (code == 'S' || code == 's')) { showSaveDialog(); return true; }
    if (! mods.isAnyModifierKeyDown() && (code == 'B' || code == 'b')) { showBrowser (! browser.isVisible()); return true; }
    if (! mods.isAnyModifierKeyDown() && (code == 'G' || code == 'g')) { setGearShown (! processor.showGear); return true; }
    if (! mods.isAnyModifierKeyDown() && (code == 'T' || code == 't')) { processor.tapTempo(); return true; }
    if (! mods.isAnyModifierKeyDown() && code >= '1' && code <= '8')
    {
        // number keys: power a unit on / off
        const int slot = code - '1';
        if (processor.getSlotType (slot) != EffectType::None)
            processor.setSlotParam (slot, 9, processor.getSlotParam (slot, 9) > 0.5f ? 0.0f : 1.0f);
        return true;
    }
    return false;
}

void EffectorKillaAudioProcessorEditor::showSaveDialog()
{
    auto* w = new juce::AlertWindow ("SAVE PRESET", "Preset name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", processor.getMeta().factory ? processor.getMeta().name + " (mine)" : processor.getMeta().name);
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int res)
    {
        if (res != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        const bool ok = safe->processor.saveUserPreset (name);
        safe->toolbar.showMessage (ok ? "SAVED" : "SAVE FAILED");
    }), true);
    ek::ui::trackDialog (w, &processor);   // closed with the editor
}

void EffectorKillaAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("TRAPWIN EFFECTOR 1.0 - Every effect. One win.");
    juce::PopupMenu size;
    size.addItem (100, "Auto (fits the screen)", true, processor.uiScale <= 0.0f);
    for (int pct : { 80, 90, 100, 110, 125, 150 })
        size.addItem (100 + pct, juce::String (pct) + " %" + (pct == 80 ? "  (SMALL)" : pct == 100 ? "  (FULL)" : ""), true,
                      processor.uiScale > 0.0f && std::abs (processor.uiScale * 100.0f - (float) pct) < 0.5f);
    m.addSubMenu ("Window size  (or drag the corner)", size);
    m.addItem (2, "Show FX library  (G)", true, processor.showGear);
    m.addItem (5, "Reduce motion", true, hw::reduceMotion());
    juce::PopupMenu os;
    for (int i = 0; i < 3; ++i) os.addItem (300 + i, juce::String (1 << i) + "x", true, processor.getOversampling() == i);
    m.addSubMenu ("Oversampling (saturation / distortion / crush / clipper)", os);
    m.addSeparator();
    m.addItem (4, "Open user preset folder");
    m.addSeparator();
    m.addSectionHeader ("Keys: Space KILL - B browser - G library - T tap - 1-8 unit on/off");
    m.addSectionHeader ("Left/Right preset - Up/Down channel - Ctrl+Z / Ctrl+Y undo / redo - Ctrl+S save");
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 100) safe->setUiScale (0.0f);
        else if (r > 100 && r <= 250) safe->setUiScale ((float) (r - 100) / 100.0f);
        else if (r == 2) safe->setGearShown (! safe->processor.showGear);
        else if (r == 5) safe->setReduceMotion (! hw::reduceMotion());
        else if (r >= 300 && r < 303) safe->processor.setOversampling (r - 300);
        else if (r == 4)
        {
            auto dir = PresetBank::getUserPresetDir();
            dir.createDirectory();
            dir.startAsProcess();
        }
    });
}

void EffectorKillaAudioProcessorEditor::showInfoMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("TRAPWIN EFFECTOR " + juce::String (JucePlugin_VersionString));
    m.addItem (1, "About TrapWin Effector");
    m.addItem (2, "Open the manual (trapwin.com)");
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1)
        {
            auto* w = new juce::AlertWindow ("TrapWin Effector",
                "Version " + juce::String (JucePlugin_VersionString) + "\nEvery effect. One win.\n\n"
                "15 effects in one chain, KILL, macros, MOD, A / B and level-matched compare.\n"
                "(c) TrapWin - trapwin.com", juce::MessageBoxIconType::NoIcon, safe.getComponent());
            w->addButton ("OK", 0, juce::KeyPress (juce::KeyPress::returnKey));
            w->enterModalState (true, nullptr, true);
            ek::ui::trackDialog (w, &safe->processor);   // closed with the editor
        }
        else if (r == 2)
            juce::URL ("https://trapwin.com").launchInDefaultBrowser();
    });
}

// ============================================================================================
// MOD patching / knob menu / modulation rings
// ============================================================================================
void EffectorKillaAudioProcessorEditor::modHandleDragged (int source, juce::Point<int> screen, bool dropped)
{
    ek::ui::RackView::KnobHit hit;
    if (rackViewport.isShowing() && rackViewport.getScreenBounds().contains (screen))
        hit = rackView.knobAt (screen);
    const int macro = hit.slot < 0 ? master.macroAt (screen) : -1;

    if (! dropped)
    {
        cableOverlay.source = source;
        cableOverlay.from = content.getLocalPoint (nullptr, modUnit.getHandleScreenPosition (source)).toFloat();
        cableOverlay.to = content.getLocalPoint (nullptr, screen).toFloat();
        cableOverlay.target = hit.slot >= 0 ? content.getLocalArea (&rackView, hit.bounds)
                            : macro >= 0 ? content.getLocalArea (nullptr, master.macroScreenBounds (macro)).reduced (8, 4)
                                         : juce::Rectangle<int>();
        cableOverlay.repaint();
        return;
    }

    cableOverlay.source = -1;
    cableOverlay.target = {};
    cableOverlay.repaint();
    const juce::String src (ek::modSourceName (source));
    if (hit.slot >= 0)
    {
        const int idx = processor.addModAssignment (source, hit.slot, hit.param, 0.5f);
        toolbar.showMessage (idx >= 0 ? src + " -> S" + juce::String (hit.slot + 1) + (hit.param < kNumParams ? " P" + juce::String (hit.param + 1) : juce::String (" DRY/WET"))
                                      : juce::String ("MODULATION FULL (16 MAX)"));
        rackView.repaintKnobs();
    }
    else if (macro >= 0)
    {
        const int idx = processor.addMacroModAssignment (source, macro, 0.5f);
        toolbar.showMessage (idx >= 0 ? src + " -> " + MacroEngine::macroName (macro) : juce::String ("MODULATION FULL (16 MAX)"));
    }
}

void EffectorKillaAudioProcessorEditor::paintKnobOverlay (juce::Graphics& g, juce::Rectangle<float> face, int slot, int param)
{
    const auto idx = processor.findModAssignments (slot, param);
    if (! idx.empty())
    {
        const auto all = processor.getModAssignments();
        const float base = processor.getSlotParam (slot, param);
        const float cur = processor.getEffectiveSlotParam (slot, param);
        int ring = 0;
        for (int i : idx)
        {
            if (i < 0 || i >= (int) all.size()) continue;
            const auto& a = all[(size_t) i];
            const bool bipolar = a.source == (int) ModSourceId::Lfo1 || a.source == (int) ModSourceId::Lfo2;
            ek::ui::paintModRing (g, face, base, a.depth, ek::ui::modSourceColour (a.source), bipolar, cur, ring++);
        }
    }
    // MACRO LEARN: a small dot per learned macro assignment
    int k = 0;
    for (const auto& mm : processor.getMacroAssignmentsFor (slot, param))
        if (mm.learned)
        {
            g.setColour (hw::col::ice);
            g.fillEllipse (face.getX() + 1.0f + 6.0f * (float) k++, face.getY() + 1.0f, 4.0f, 4.0f);
        }
}

void EffectorKillaAudioProcessorEditor::showKnobMenu (int slot, int param)
{
    const auto type = processor.getSlotType (slot);
    if (type == EffectType::None) return;
    const auto& info = effectInfo (type);
    const juce::String pname = param < kNumParams ? info.params[(size_t) param].name : juce::String ("DRY/WET");
    const float defaultValue = param < kNumParams ? info.params[(size_t) param].defaultNorm() : 1.0f;
    static constexpr float depths[] = { 1.0f, 0.5f, 0.25f, -0.25f, -0.5f, -1.0f };

    juce::PopupMenu m;
    m.addSectionHeader ("S" + juce::String (slot + 1) + "  " + info.name.toUpperCase() + "  -  " + pname.toUpperCase());
    m.addItem (1, "Reset to default");

    m.addSeparator();
    m.addSectionHeader ("MODULATION");
    const auto all = processor.getModAssignments();
    const auto idx = processor.findModAssignments (slot, param);
    if (idx.empty()) m.addItem (2, "(drag a MOD jack onto this knob)", false);
    for (int i : idx)
    {
        if (i < 0 || i >= (int) all.size()) continue;
        const auto& a = all[(size_t) i];
        juce::PopupMenu sub;
        for (int d = 0; d < 6; ++d)
            sub.addItem (2000 + i * 10 + d, "Depth " + juce::String (juce::roundToInt (depths[d] * 100.0f)) + " %", true,
                         std::abs (a.depth - depths[d]) < 0.001f);
        sub.addSeparator();
        sub.addItem (1000 + i, "Remove");
        m.addSubMenu (juce::String (ek::modSourceName (a.source)) + "   depth " + juce::String (juce::roundToInt (a.depth * 100.0f)) + " %", sub);
    }
    juce::PopupMenu add;
    for (int src = 0; src < kNumModSources; ++src) add.addItem (3000 + src, ek::modSourceName (src));
    m.addSubMenu ("Add modulation (depth 50 %)", add);
    if (! idx.empty()) m.addItem (3, "Remove all modulation of this knob");

    m.addSeparator();
    m.addSectionHeader ("MACRO LEARN");
    const auto maps = processor.getMacroAssignmentsFor (slot, param);
    for (int mc = 0; mc < kNumMacros; ++mc)
    {
        const bool learned = std::any_of (maps.begin(), maps.end(), [mc] (const MacroMapping& x) { return x.macro == mc && x.learned; });
        juce::PopupMenu sub;
        sub.addItem (4000 + mc * 10 + 0, "Range: full (macro 0 = 0 %, 100 = 100 %)");
        sub.addItem (4000 + mc * 10 + 1, "Range: inverted (macro 0 = 100 %, 100 = 0 %)");
        sub.addItem (4000 + mc * 10 + 2, "Range: subtle (knob +- 25 %)");
        m.addSubMenu (juce::String ("Assign to ") + MacroEngine::macroName (mc) + (learned ? "   (assigned)" : ""), sub);
        if (learned) m.addItem (5000 + mc, juce::String ("Remove from ") + MacroEngine::macroName (mc));
    }

    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, slot, param, defaultValue] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& p = safe->processor;
        if (r == 1) { p.pushUndo(); p.setSlotParam (slot, param, defaultValue); }
        else if (r == 3) p.removeModAssignmentsFor (slot, param);
        else if (r >= 1000 && r < 2000) p.removeModAssignment (r - 1000);
        else if (r >= 2000 && r < 3000) { p.pushUndo(); p.setModDepth ((r - 2000) / 10, depths[(r - 2000) % 10]); }
        else if (r >= 3000 && r < 4000) p.addModAssignment (r - 3000, slot, param, 0.5f);
        else if (r >= 4000 && r < 5000)
        {
            const int mc = (r - 4000) / 10, kind = (r - 4000) % 10;
            const float v = p.getSlotParam (slot, param);
            float lo = 0.0f, hi = 1.0f;
            if (kind == 1) { lo = 1.0f; hi = 0.0f; }
            else if (kind == 2) { lo = juce::jlimit (0.0f, 1.0f, v - 0.25f); hi = juce::jlimit (0.0f, 1.0f, v + 0.25f); }
            p.assignMacro (mc, slot, param, lo, hi);
        }
        else if (r >= 5000) p.removeMacroAssignment (r - 5000, slot, param);
        safe->rackView.repaintKnobs();
    });
}
