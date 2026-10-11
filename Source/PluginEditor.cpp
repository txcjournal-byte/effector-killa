#include "PluginEditor.h"

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
        const bool gear = proc.showGear && getWidth() >= kWidth;
        bg.fillAll (hw::col::shaft);

        // dark rack shaft with rails left / right
        const auto shaft = shaftArea().toFloat();
        bg.setColour (hw::col::shaft);
        bg.fillRect (shaft);
        hw::drawRackRail (bg, { shaft.getX() + 1.0f, shaft.getY(), 14.0f, shaft.getHeight() });
        hw::drawRackRail (bg, { shaft.getRight() - 15.0f, shaft.getY(), 14.0f, shaft.getHeight() });
        // depth: shadow under the top board and along the sides
        juce::ColourGradient topShade (juce::Colours::black.withAlpha (0.8f), 0.0f, shaft.getY(), juce::Colours::transparentBlack, 0.0f, shaft.getY() + 14.0f, false);
        bg.setGradientFill (topShade);
        bg.fillRect (shaft.withHeight (14.0f));

        // gear bay
        if (gear)
        {
            const auto ga = gearArea().toFloat();
            bg.setColour (juce::Colour (0xff090b10));
            bg.fillRect (ga);
        }

        // walnut: side boards full height (under the toolbar), top / bottom rails between them
        const float top = 40.0f;
        hw::drawWalnut (bg, { 20.0f, top, W - 40.0f, 18.0f }, false, 3);
        hw::drawWalnut (bg, { 20.0f, H - 18.0f, W - 40.0f, 18.0f }, false, 5);
        hw::drawWalnut (bg, { 0.0f, top, 20.0f, H - top }, true, 7);
        hw::drawWalnut (bg, { W - 20.0f, top, 20.0f, H - top }, true, 9);
        if (gear) hw::drawWalnut (bg, { 960.0f, top + 18.0f, 20.0f, H - top - 36.0f }, true, 11);

        // joints + inner lip around the openings
        bg.setColour (juce::Colours::black.withAlpha (0.65f));
        for (auto x : { 20.0f, W - 20.0f }) bg.drawVerticalLine ((int) x, top, H);
        if (gear) for (auto x : { 960.0f, 980.0f }) bg.drawVerticalLine ((int) x, top + 18.0f, H - 18.0f);
        bg.drawHorizontalLine ((int) top + 18, 20.0f, W - 20.0f);
        bg.drawHorizontalLine ((int) H - 18, 20.0f, W - 20.0f);
        bg.setColour (juce::Colours::white.withAlpha (0.06f));
        bg.drawHorizontalLine ((int) top + 1, 0.0f, W);
    });
}

// ============================================================================================
EffectorKillaAudioProcessorEditor::EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), frame (p), toolbar (p), controlUnit (p), performer (p), modUnit (p),
      rackView (p), gearList (p), browser (p)
{
    setOpaque (true);
    addAndMakeVisible (content);
    content.addAndMakeVisible (frame);
    content.addAndMakeVisible (toolbar);
    content.addAndMakeVisible (controlUnit);
    content.addAndMakeVisible (performer);
    content.addAndMakeVisible (modUnit);

    rackViewport.setViewedComponent (&rackView, false);
    rackViewport.setScrollBarsShown (true, false);
    rackViewport.setScrollBarThickness (8);
    rackViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    content.addAndMakeVisible (rackViewport);
    content.addAndMakeVisible (gearList);
    content.addChildComponent (browser);

    toolbar.onBrowse = [this] { showBrowser (! browser.isVisible()); };
    toolbar.isBrowserShown = [this] { return browser.isVisible(); };
    toolbar.onSave = [this] { showSaveDialog(); };
    toolbar.onSettings = [this] { showSettingsMenu(); };
    toolbar.onInfo = [this] { showInfoMenu(); };
    controlUnit.onSetScale = [this] (float s) { setUiScale (s); };
    controlUnit.onToggleGear = [this] { setGearShown (! processor.showGear); };
    controlUnit.isGearShown = [this] { return processor.showGear; };
    browser.onClose = [this] { showBrowser (false); };
    auto say = [this] (const juce::String& m) { toolbar.showMessage (m); };
    rackView.onMessage = say;
    gearList.onMessage = say;

    getLookAndFeel().setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf00b0d12));
    getLookAndFeel().setColour (juce::TooltipWindow::textColourId, hw::col::print);
    getLookAndFeel().setColour (juce::TooltipWindow::outlineColourId, hw::col::ice.withAlpha (0.5f));
    getLookAndFeel().setColour (juce::ScrollBar::thumbColourId, hw::col::ice.withAlpha (0.35f));
    getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff111318));
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
}

void EffectorKillaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (hw::col::shaft);
}

float EffectorKillaAudioProcessorEditor::autoScale() const
{
    auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                    ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea
                    : juce::Rectangle<int> (1920, 1080);
    // always fit the wide (gear) layout so SHOW GEAR never pushes the window off the screen
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
    content.setBounds (0, 0, w, kHeight);
    frame.setBounds (0, 0, w, kHeight);
    frame.invalidate();
    toolbar.setBounds (toolbarArea (w));
    controlUnit.setBounds (controlUnitArea());
    performer.setBounds (performerArea());
    modUnit.setBounds (modUnitArea());

    const auto rack = rackArea();
    rackViewport.setBounds (rack);
    rackView.setSize (rack.getWidth() - rackViewport.getScrollBarThickness(), juce::jmax (rack.getHeight(), rackView.getIdealHeight()));
    gearList.setVisible (processor.showGear);
    gearList.setBounds (gearArea());
    browser.setBounds (20, performerArea().getY(), w - 40, rack.getBottom() - performerArea().getY());
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
    controlUnit.tick();
    tickIfPossible (performer, 0);
    tickIfPossible (modUnit, 0);
    rackView.tick();
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
    m.addItem (2, "Show gear list  (G)", true, processor.showGear);
    m.addItem (5, "Reduce motion", true, hw::reduceMotion());
    juce::PopupMenu os;
    for (int i = 0; i < 3; ++i) os.addItem (300 + i, juce::String (1 << i) + "x", true, processor.getOversampling() == i);
    m.addSubMenu ("Oversampling (saturation / distortion / crush / clipper)", os);
    m.addSeparator();
    m.addItem (4, "Open user preset folder");
    m.addSeparator();
    m.addSectionHeader ("Keys: Space KILL - B browser - G gear - T tap - 1-8 unit on/off");
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
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "TrapWin Effector",
                "Version " + juce::String (JucePlugin_VersionString) + "\nEvery effect. One win.\n\n"
                "15 hardware-style effects in one rack, KILL, macros, A / B and level-matched compare.\n"
                "(c) TrapWin - trapwin.com", "OK", safe.getComponent());
        else if (r == 2)
            juce::URL ("https://trapwin.com").launchInDefaultBrowser();
    });
}
