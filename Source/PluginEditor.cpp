#include "PluginEditor.h"

using namespace ek;

namespace
{
constexpr int bodyTop = ui::ControlPanel::headerHeight + 8 + ui::ControlPanel::stripHeight + 8;
constexpr int gearWidth = 240;
}

EffectorKillaAudioProcessorEditor::EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), controls (p), rackView (p), gearList (p), browser (p)
{
    setOpaque (true);
    addAndMakeVisible (content);
    content.setBounds (0, 0, kWidth, kHeight);
    content.addAndMakeVisible (controls);
    controls.setBounds (0, 0, kWidth, bodyTop);

    rackViewport.setViewedComponent (&rackView, false);
    rackViewport.setScrollBarsShown (true, false);
    rackViewport.setScrollBarThickness (8);
    rackViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    content.addAndMakeVisible (rackViewport);
    content.addAndMakeVisible (gearList);
    content.addChildComponent (browser);

    controls.onBrowse = [this] { showBrowser (! browser.isVisible()); };
    controls.isBrowserShown = [this] { return browser.isVisible(); };
    controls.onSave = [this] { showSaveDialog(); };
    controls.onToggleGear = [this] { setGearShown (! processor.showGear); };
    controls.isGearShown = [this] { return processor.showGear; };
    controls.onSettings = [this] { showSettingsMenu(); };
    browser.onClose = [this] { showBrowser (false); };
    auto say = [this] (const juce::String& m) { controls.showMessage (m); };
    rackView.onMessage = say;
    gearList.onMessage = say;

    getLookAndFeel().setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf00b111c));
    getLookAndFeel().setColour (juce::TooltipWindow::textColourId, ek::theme::silver);
    getLookAndFeel().setColour (juce::TooltipWindow::outlineColourId, ek::theme::accent.withAlpha (0.5f));
    getLookAndFeel().setColour (juce::ScrollBar::thumbColourId, ek::theme::accent.withAlpha (0.35f));

    setWantsKeyboardFocus (true);
    addMouseListener (this, true);
    processor.addChangeListener (this);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) kWidth / (double) kHeight);
        c->setSizeLimits (juce::roundToInt (kWidth * 0.6), juce::roundToInt (kHeight * 0.6), kWidth * 2, kHeight * 2);
    }
    setUiScale (processor.uiScale);
    layoutBody();
    startTimerHz (30);
}

EffectorKillaAudioProcessorEditor::~EffectorKillaAudioProcessorEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
}

void EffectorKillaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ek::theme::bg);
}

float EffectorKillaAudioProcessorEditor::autoScale() const
{
    auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                    ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea
                    : juce::Rectangle<int> (1920, 1080);
    const float s = juce::jmin ((float) area.getWidth() * 0.86f / kWidth, (float) area.getHeight() * 0.80f / kHeight);
    return juce::jlimit (0.6f, 1.5f, std::floor (s * 20.0f) / 20.0f);
}

void EffectorKillaAudioProcessorEditor::setUiScale (float s)
{
    processor.uiScale = s;
    const float scale = s <= 0.0f ? autoScale() : juce::jlimit (0.6f, 2.0f, s);
    settingSize = true;
    setSize (juce::roundToInt (kWidth * scale), juce::roundToInt (kHeight * scale));
    settingSize = false;
}

void EffectorKillaAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) kWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    if (! settingSize && isShowing()) processor.uiScale = scale; // dragged the corner: remember that size
}

void EffectorKillaAudioProcessorEditor::layoutBody()
{
    const int h = kHeight - bodyTop - 10;
    const int rackW = processor.showGear ? kWidth - 20 - gearWidth - 8 : kWidth - 20;
    rackViewport.setBounds (10, bodyTop, rackW, h);
    rackView.setSize (rackW - rackViewport.getScrollBarThickness() - 2, juce::jmax (h, rackView.getIdealHeight()));
    gearList.setVisible (processor.showGear);
    gearList.setBounds (kWidth - 10 - gearWidth, bodyTop, gearWidth, h);
    browser.setBounds (10, bodyTop, kWidth - 20, h);
}

void EffectorKillaAudioProcessorEditor::setGearShown (bool shown)
{
    processor.showGear = shown;
    layoutBody();
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
    const int h = kHeight - bodyTop - 10;
    const int want = juce::jmax (h, rackView.getIdealHeight());
    if (rackView.getHeight() != want) rackView.setSize (rackView.getWidth(), want);
    controls.tick();
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
    const int h = kHeight - bodyTop - 10;
    rackView.setSize (rackView.getWidth(), juce::jmax (h, rackView.getIdealHeight()));
    controls.refresh();
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
        safe->controls.showMessage (ok ? "SAVED" : "SAVE FAILED");
    }), true);
}

void EffectorKillaAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("TRAPWIN EFFECTOR 1.0 - Every effect. One win.");
    juce::PopupMenu size;
    size.addItem (100, "Auto (fits the screen)", true, processor.uiScale <= 0.0f);
    for (int pct : { 80, 90, 100, 110, 125, 150 })
        size.addItem (100 + pct, juce::String (pct) + " %", true, processor.uiScale > 0.0f && std::abs (processor.uiScale * 100.0f - (float) pct) < 0.5f);
    m.addSubMenu ("Window size  (or drag the corner)", size);
    m.addItem (2, "Show gear list  (G)", true, processor.showGear);
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
        else if (r >= 300 && r < 303) safe->processor.setOversampling (r - 300);
        else if (r == 4)
        {
            auto dir = PresetBank::getUserPresetDir();
            dir.createDirectory();
            dir.startAsProcess();
        }
    });
}
