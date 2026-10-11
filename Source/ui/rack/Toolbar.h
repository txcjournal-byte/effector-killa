#pragma once

// HEADER of the window (56 px, Docs/TRAPWIN-STYLE.md): no bar, just pills on the dark background.
//   [logo] (library)  A B  undo redo   ...   < [ preset pill ] >  save revert fav   ...   cog i [ BYPASS ]
// The official TrapWin logo is painted from Brand.h (never as text). The preset pill (IBM Plex Mono,
// sparkle = favourite) also shows short status messages ("SAVED") and plays a soft scan sweep when the
// preset changes; click = preset browser. BYPASS = COMPARE (off_air): click toggles, holding listens
// while held (red glow while on).

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

    static constexpr int height = 56;

    std::function<void()> onBrowse, onSave, onSettings, onInfo, onToggleLibrary;
    std::function<bool()> isBrowserShown, isLibraryShown;
    // x of the chain column centre (the preset group is centred over it when there is room)
    void setCentreX (int x) { if (x != centreX) { centreX = x; resized(); } }

    void tick();
    void refresh() { lcd.repaint(); }
    void showMessage (const juce::String& m) { lcd.showMessage (m); }
    void revertPreset();   // reloads the current factory / user preset

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EffectorKillaAudioProcessor& proc;
    PresetLcd lcd;
    hw::HwButton libBtn, prevBtn, nextBtn, saveBtn, revertBtn, favBtn, aBtn, bBtn, undoBtn, redoBtn, settingsBtn, infoBtn, bypassBtn;
    juce::Rectangle<float> logoArea;
    int centreX = -1;
    juce::uint32 bypassDownAt = 0;
    bool bypassWasOn = false;
};
} // namespace ek::ui
