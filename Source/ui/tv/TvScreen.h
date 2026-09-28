#pragma once

// The CRT television: the channel's "programme" (procedural scene), the playable AV layer
// (AV1 REMOTE / AV2 SCRIBBLE / AV3 SCREENSAVER), OSD messages, KILL / OFF AIR animations and the
// PREMIUM CABLE cassette detail panel. Rendered by OpenGL (TvGl) or, as a fallback, in software.

#include "../../PluginProcessor.h"
#include "../Theme.h"
#include "TvState.h"

namespace ek::ui
{
class TvScreen : public juce::Component
{
public:
    explicit TvScreen (EffectorKillaAudioProcessor& p);
    ~TvScreen() override;

    void tick (double dtSeconds);                      // animation clock (editor timer)
    void showValue (const juce::String& name, float norm, const juce::String& text);
    void showMessage (const juce::String& text, double seconds = 1.5);

    void setDetailSlot (int slot);                     // -1 closes the panel
    int getDetailSlot() const noexcept { return detailSlot; }
    std::function<void()> onDetailClosed;

    // rendering mode
    void setGlActive (bool on);
    bool isGlActive() const noexcept { return glActive; }
    TvFrame getFrame() const;                           // snapshot for the GL thread
    bool renderOsdIfDirty (juce::Image& target);        // paints the OSD layer into target (GL mode)

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static juce::String avName (int mode);

private:
    // OSD / geometry
    void paintOsd (juce::Graphics& g, bool includeDynamic);
    void paintDetailPanel (juce::Graphics& g);
    void paintSoftwareScene (juce::Graphics& g);
    void paintPlayLayer (juce::Graphics& g);   // dot / path / logo (software mode)
    juce::Rectangle<float> detailPanelArea() const;
    juce::Rectangle<float> detailCell (int index) const; // 0..7 = P1..P8, 8 = MIX
    juce::Rectangle<float> detailToggle (int index) const; // 0 PAUSE, 1 SCREEN, 2 M/S, 3 LOCK
    juce::Rectangle<float> axisLabel (int axis) const;     // 0 = X, 1 = Y
    juce::Rectangle<float> barSelector() const;
    juce::Rectangle<float> sourceLabel() const;
    juce::Rectangle<float> modeLabel() const;
    juce::String targetName (int target) const;
    void showTargetMenu (int axis);
    void showSourceMenu();
    int detailCellAt (juce::Point<float> p) const;          // returns the param (0..7, 8 = MIX)
    std::vector<int> detailParams() const;                  // visible params in panel order
    juce::Rectangle<float> detailRectFor (int param) const;
    void setDetailValue (int cell, float norm, bool asGesture);
    float getDetailValue (int cell) const;
    juce::Point<float> toNorm (juce::Point<float> local) const;   // 0..1, y up
    juce::Point<float> fromNorm (juce::Point<float> n) const;
    void setTargetValue (int target, float v, bool begin, bool end);
    float getTargetValue (int target) const;
    void markOsdDirty() { osdDirty = true; }

    EffectorKillaAudioProcessor& proc;
    bool glActive = false;
    double clock = 0.0;

    // animations / messages
    int lastKillEvents = 0, lastProgramEvents = 0, lastClipCount = 0, lastCornerHits = 0;
    double killStart = -10.0, switchStart = -10.0, overloadUntil = 0.0, nowPlayingUntil = 0.0;
    double valueUntil = 0.0, messageUntil = 0.0;
    juce::String valueName, valueText, message;
    float valueNorm = 0.0f;
    float power = 1.0f, pocket = 0.0f, level = 0.0f;
    double glitchFlash = 0.0;

    // AV interaction
    bool drawing = false, remoteDragging = false;
    int frameCounter = 0;
    std::vector<juce::Point<float>> stroke;

    // detail panel
    int detailSlot = -1, dragCell = -1, hoverCell = -1;
    float dragStartValue = 0.0f;

    bool osdDirty = true;
    juce::String lastOsdKey;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TvScreen)
};
} // namespace ek::ui
