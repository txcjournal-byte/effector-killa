#pragma once

// Top bar of the window (40 px, DESIGN-RACK.md section 3):
//   < > [ PRESET LCD ] save revert fav  A B  undo redo  ........  settings info [ BYPASS ]
// The preset LCD also shows short status messages ("SAVED") and plays a scan animation when
// the preset changes. BYPASS = COMPARE (off_air): click toggles, holding listens while held.

#include "Hardware.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
class PresetLcd : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetLcd (EffectorKillaAudioProcessor& p);
    std::function<void()> onClick;
    void tick();
    void showMessage (const juce::String& m, int ms = 2200);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (onClick && getLocalBounds().contains (e.getPosition())) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    EffectorKillaAudioProcessor& proc;
    int lastProgramEvents = -1;
    juce::String lastName;
    bool lastFav = false;
    double scanStart = -10.0;
    juce::String message;
    juce::uint32 messageUntil = 0;
};

class Toolbar : public juce::Component
{
public:
    explicit Toolbar (EffectorKillaAudioProcessor& p);

    static constexpr int height = 40;

    std::function<void()> onBrowse, onSave, onSettings, onInfo;
    std::function<bool()> isBrowserShown;

    void tick();
    void refresh() { lcd.repaint(); }
    void showMessage (const juce::String& m) { lcd.showMessage (m); }
    void revertPreset();   // reloads the current factory / user preset

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EffectorKillaAudioProcessor& proc;
    PresetLcd lcd;
    hw::HwButton prevBtn, nextBtn, saveBtn, revertBtn, favBtn, aBtn, bBtn, undoBtn, redoBtn, settingsBtn, infoBtn, bypassBtn;
    juce::uint32 bypassDownAt = 0;
    bool bypassWasOn = false;
};
} // namespace ek::ui
