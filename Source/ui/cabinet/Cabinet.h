#pragma once

// The whole TV cabinet in cabinet space (1630 x 965). Every visible element is a control.

#include "Controls.h"
#include "Shelf.h"
#include "Vcr.h"
#include "../tv/TvScreen.h"

namespace ek::ui
{
class KillButton : public juce::Component
{
public:
    std::function<void()> onKill;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
private:
    bool down = false;
    float flash = 0.0f;
};

class StaffPickButton : public juce::Component
{
public:
    std::function<bool()> isOn;
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    bool shown = false;
};

class OffAirSwitch : public juce::Component
{
public:
    explicit OffAirSwitch (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    juce::RangedAudioParameter& param;
    juce::uint32 downTime = 0;
    bool wasOn = false, down = false, shown = false;
};

class ChannelDial : public juce::Component
{
public:
    explicit ChannelDial (EffectorKillaAudioProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void refresh();
    static float angleForChannel (int ch) { return juce::degreesToRadians (180.0f + (float) ch * 30.0f); }
private:
    int channelAt (juce::Point<float> local) const;
    EffectorKillaAudioProcessor& proc;
    float dragAngle = 0.0f;
    bool dragging = false;
    int shown = -1;
};

class CabinetView : public juce::Component
{
public:
    explicit CabinetView (EffectorKillaAudioProcessor& p);
    ~CabinetView() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick (double dt);
    void refreshAll();
    void selectSlot (int slot);

    TvScreen& getTv() noexcept { return tv; }
    std::function<void (const juce::MouseEvent&)> onLogoMenu;

private:
    void placeCentred (juce::Component& c, juce::Point<float> centre, float size);
    void place (juce::Component& c, juce::Rectangle<float> r);

    EffectorKillaAudioProcessor& proc;
    juce::Image background;

    KillButton kill;
    ThreeWaySwitch rating;
    LitButton bootleg { "BOOTLEG", juce::Colour (0xffff8a2a), 19.0f };
    LitButton pocketTv { "POCKET\nTV", juce::Colour (0xffffc23a), 19.0f };
    LitButton autoTracking { "AUTO\nTRACKING", juce::Colour (0xffffc23a), 18.0f };
    CableToggle cable;
    StaffPickButton staffPick;
    Knob antennaIn { Knob::Style::Small }, rfOut { Knob::Style::Small };

    TvScreen tv;
    ThreeWaySwitch av;
    std::unique_ptr<OffAirSwitch> offAir;

    ChannelDial channel;
    ArrowButton programUp { true }, programDown { false };
    std::array<Knob, 5> macros;

    CassetteShelf shelf;
    VideoRecorder vcr;
    int shownFocus = -2;
};
} // namespace ek::ui
