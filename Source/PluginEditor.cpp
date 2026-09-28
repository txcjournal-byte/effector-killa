#include "PluginEditor.h"
#include "EKBinaryData.h"

using namespace ek;

EffectorKillaAudioProcessorEditor::EffectorKillaAudioProcessorEditor (EffectorKillaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), cabinet (p)
{
    setOpaque (true);
    addAndMakeVisible (cabinet);
    cabinet.onLogoMenu = [this] (const juce::MouseEvent&) { showMainMenu(); };
    setWantsKeyboardFocus (true);
    processor.addChangeListener (this);

    gl = std::make_unique<ui::TvGl> (cabinet.getTv(), [this] { return cabinet.getTv().getLocalBounds(); });
    // the AV3 screensaver logo is the cabinet logo
    auto reference = juce::ImageCache::getFromMemory (EKData::reference_png, EKData::reference_pngSize);
    if (reference.isValid())
    {
        juce::Image logo (juce::Image::ARGB, 330, 195, true);
        juce::Graphics lg (logo);
        lg.drawImage (reference, 0, 0, 330, 195, 40, 60, 330, 195);
        // knock out the dark wood behind the letters
        for (int y = 0; y < logo.getHeight(); ++y)
            for (int x = 0; x < logo.getWidth(); ++x)
            {
                auto c = logo.getPixelAt (x, y);
                const float lum = c.getPerceivedBrightness();
                const float sat = c.getSaturation();
                const float a = juce::jlimit (0.0f, 1.0f, (lum - 0.28f) * 3.0f + (sat > 0.55f && c.getRed() > 150 ? 1.0f : 0.0f));
                logo.setPixelAt (x, y, c.withAlpha (a));
            }
        gl->setLogo (logo);
    }
    // EK_SOFTWARE_TV=1 forces the software TV (machines / VMs without a usable OpenGL driver)
    if (std::getenv ("EK_SOFTWARE_TV") != nullptr) processor.softwareTv = true;
    if (! processor.softwareTv) gl->attach();

    addMouseListener (this, true); // any click inside -> keyboard shortcuts work
    setResizable (false, false);
    setUiScale (processor.uiScale);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

EffectorKillaAudioProcessorEditor::~EffectorKillaAudioProcessorEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
    if (gl != nullptr) gl->detach();
}

void EffectorKillaAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (! cabinet.getTv().isGlActive()) g.fillAll (juce::Colours::black);
}

void EffectorKillaAudioProcessorEditor::resized()
{
    cabinet.setTransform (juce::AffineTransform::scale ((float) getWidth() / layout::kWidth, (float) getHeight() / layout::kHeight));
}

void EffectorKillaAudioProcessorEditor::setUiScale (float s)
{
    uiScale = juce::jlimit (1.0f, 1.5f, s);
    processor.uiScale = uiScale;
    setSize (juce::roundToInt (layout::kEditorWidth * uiScale), juce::roundToInt (layout::kEditorHeight * uiScale));
}

void EffectorKillaAudioProcessorEditor::setOpenGl (bool on)
{
    processor.softwareTv = ! on;
    if (on) gl->attach();
    else { gl->detach(); cabinet.getTv().setGlActive (false); cabinet.repaint(); }
}

void EffectorKillaAudioProcessorEditor::timerCallback()
{
    if (! focusGrabbed && isShowing()) { focusGrabbed = true; grabKeyboardFocus(); }
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;

    auto& tv = cabinet.getTv();
    if (gl != nullptr && ! processor.softwareTv)
    {
        if (gl->hasFailed())
        {
            // no usable OpenGL: detach for good and use the software TV
            gl->detach();
            processor.softwareTv = true;
            tv.setGlActive (false);
            cabinet.repaint();
        }
        else if (gl->isWorking() && ! tv.isGlActive()) { tv.setGlActive (true); cabinet.repaint(); }
    }
    cabinet.tick (dt);
    if (gl != nullptr && ! processor.softwareTv)
    {
        gl->setFrame (tv.getFrame());
        juce::Image osd;
        if (tv.renderOsdIfDirty (osd)) gl->setOsd (osd);
    }
}

void EffectorKillaAudioProcessorEditor::mouseDown (const juce::MouseEvent&)
{
    if (! hasKeyboardFocus (true)) grabKeyboardFocus();
}

void EffectorKillaAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*) { cabinet.refreshAll(); }

bool EffectorKillaAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    const auto code = k.getKeyCode();
    const auto mods = k.getModifiers();
    if (code == juce::KeyPress::spaceKey) { processor.kill(); return true; }
    if (code == juce::KeyPress::leftKey) { processor.stepPreset (-1); return true; }
    if (code == juce::KeyPress::rightKey) { processor.stepPreset (1); return true; }
    if (code == juce::KeyPress::upKey) { processor.stepChannel (1); return true; }
    if (code == juce::KeyPress::downKey) { processor.stepChannel (-1); return true; }
    if (code == juce::KeyPress::escapeKey) { cabinet.selectSlot (-1); return true; }
    if (mods.isCommandDown() && (code == 'Z' || code == 'z'))
    {
        if (mods.isShiftDown()) processor.redo(); else processor.undo();
        return true;
    }
    if (mods.isCommandDown() && (code == 'Y' || code == 'y')) { processor.redo(); return true; }
    if (! mods.isAnyModifierKeyDown() && code >= '1' && code <= '8')
    {
        const int slot = code - '1';
        cabinet.selectSlot (processor.selectedSlot == slot || processor.getSlotMeta (slot).isEmpty() ? -1 : slot);
        return true;
    }
    return false;
}

void EffectorKillaAudioProcessorEditor::showMainMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("EFFECTOR KILLA 1.0 - Every effect. One Killa.");
    juce::PopupMenu size;
    for (float s : { 1.0f, 1.25f, 1.5f })
        size.addItem ((int) (s * 100.0f), juce::String (juce::roundToInt (s * 100.0f)) + " %", true, std::abs (uiScale - s) < 0.01f);
    m.addSubMenu ("UI size", size);
    juce::PopupMenu os;
    for (int i = 0; i < 3; ++i) os.addItem (200 + i, juce::String (1 << i) + "x", true, processor.getOversampling() == i);
    m.addSubMenu ("Oversampling (saturation / distortion / crush / clipper)", os);
    m.addItem (300, "OpenGL TV (off = software)", true, ! processor.softwareTv);
    m.addSeparator();
    m.addItem (400, "Open user preset folder");
    juce::Component::SafePointer<EffectorKillaAudioProcessorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r >= 100 && r <= 150) safe->setUiScale ((float) r / 100.0f);
        else if (r >= 200 && r < 203) safe->processor.setOversampling (r - 200);
        else if (r == 300) safe->setOpenGl (safe->processor.softwareTv);
        else if (r == 400)
        {
            auto dir = PresetBank::getUserPresetDir();
            dir.createDirectory();
            dir.startAsProcess();
        }
    });
}
