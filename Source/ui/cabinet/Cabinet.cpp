#include "Cabinet.h"
#include "EKBinaryData.h"

namespace ek::ui
{
using namespace juce;

// ============================================================================
void KillButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    // bezel
    ColourGradient bezel (Colour (0xff6d665f), b.getX(), b.getY(), Colour (0xff120f0d), b.getRight(), b.getBottom(), false);
    bezel.addColour (0.5, Colour (0xff2b2723));
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (b.reduced (9.0f), 9.0f);

    auto face = b.reduced (14.0f);
    if (down) face = face.translated (0.0f, 2.5f).reduced (1.5f);
    // glow
    g.setColour (theme::killRed.withAlpha (0.35f + 0.3f * flash));
    g.fillRoundedRectangle (face.expanded (6.0f), 12.0f);
    ColourGradient red (Colour (0xffff6a55).interpolatedWith (Colours::white, flash * 0.4f), face.getCentreX(), face.getCentreY() - 10.0f,
                        Colour (0xffb3121c), face.getRight(), face.getBottom(), true);
    red.addColour (0.55, theme::killRed);
    g.setGradientFill (red);
    g.fillRoundedRectangle (face, 8.0f);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (face.reduced (5.0f), 6.0f, 1.8f);
    ColourGradient gloss (Colours::white.withAlpha (0.3f), face.getX(), face.getY(), Colours::transparentWhite, face.getX(), face.getCentreY(), false);
    g.setGradientFill (gloss);
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.45f).reduced (8.0f, 5.0f), 6.0f);
    g.setColour (Colour (0xff3a0508));
    g.setFont (theme::stencilFont (58.0f).withHorizontalScale (0.85f));
    g.drawText ("KILL", face, Justification::centred);
}

void KillButton::mouseDown (const MouseEvent&) { down = true; repaint(); }
void KillButton::mouseUp (const MouseEvent& e)
{
    down = false;
    if (getLocalBounds().contains (e.getPosition()))
    {
        flash = 1.0f;
        if (onKill) onKill();
    }
    repaint();
}
void KillButton::tick()
{
    if (flash > 0.0f) { flash = std::max (0.0f, flash - 0.06f); repaint(); }
}

// ============================================================================
void StaffPickButton::paint (Graphics& g)
{
    const bool on = isOn ? isOn() : false;
    shown = on;
    auto b = getLocalBounds().toFloat();
    theme::drawSocket (g, b, 6.0f);
    auto face = b.reduced (6.0f);
    ColourGradient grad (Colour (0xff2a2520), face.getX(), face.getY(), Colour (0xff0d0b09), face.getX(), face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 4.0f);
    Path star;
    star.addStar (face.getCentre(), 5, face.getHeight() * 0.2f, face.getHeight() * 0.42f, 0.0f);
    if (on)
    {
        g.setColour (theme::amber.withAlpha (0.35f));
        g.fillEllipse (face.reduced (2.0f));
        g.setColour (Colour (0xffffd24a));
    }
    else g.setColour (Colour (0xff5a4a2a));
    g.fillPath (star);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.strokePath (star, PathStrokeType (1.0f));
}
void StaffPickButton::mouseUp (const MouseEvent& e) { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); repaint(); }
void StaffPickButton::refresh() { if ((isOn ? isOn() : false) != shown) repaint(); }

// ============================================================================
OffAirSwitch::OffAirSwitch (RangedAudioParameter& p) : param (p) {}

void OffAirSwitch::paint (Graphics& g)
{
    const bool on = param.getValue() > 0.5f;
    shown = on;
    const auto led = layout::offAirLed - getPosition().toFloat();
    theme::drawLed (g, led, 9.0f, theme::killRed, on);
    auto r = layout::offAirButton - getPosition().toFloat();
    theme::drawSocket (g, r, 6.0f);
    auto face = r.reduced (5.0f);
    if (down) face = face.translated (0.0f, 1.5f);
    ColourGradient grad (Colour (0xff3a3630), face.getX(), face.getY(), Colour (0xff171512), face.getX(), face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 4.0f);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (face.reduced (1.0f), 4.0f, 1.2f);
    g.setColour (on ? theme::killRed.brighter (0.3f) : theme::cream);
    g.setFont (theme::stencilFont (24.0f));
    g.drawText ("OFF AIR", face, Justification::centred);
}

void OffAirSwitch::mouseDown (const MouseEvent&)
{
    down = true;
    downTime = Time::getMillisecondCounter();
    wasOn = param.getValue() > 0.5f;
    param.beginChangeGesture();
    param.setValueNotifyingHost (wasOn ? 0.0f : 1.0f);
    repaint();
}

void OffAirSwitch::mouseUp (const MouseEvent&)
{
    down = false;
    // hold = listen to the original only while held
    if (Time::getMillisecondCounter() - downTime > 400)
        param.setValueNotifyingHost (wasOn ? 1.0f : 0.0f);
    param.endChangeGesture();
    repaint();
}

void OffAirSwitch::refresh() { if ((param.getValue() > 0.5f) != shown) repaint(); }

// ============================================================================
ChannelDial::ChannelDial (EffectorKillaAudioProcessor& p) : proc (p) {}

int ChannelDial::channelAt (Point<float> l) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto d = l - c;
    float a = std::atan2 (d.x, -d.y); // 0 = up, clockwise
    if (a < 0) a += MathConstants<float>::twoPi;
    const float deg = radiansToDegrees (a);
    return ((int) std::lround ((deg - 180.0f) / 30.0f) % 12 + 12) % 12;
}

void ChannelDial::paint (Graphics& g)
{
    const int ch = proc.getCurrentChannel();
    shown = ch;
    const auto c = getLocalBounds().toFloat().getCentre();
    // highlight the selected channel number
    const float a = angleForChannel (ch);
    const Point<float> np (c.x + layout::channelNumberRadius * std::sin (a), c.y - layout::channelNumberRadius * std::cos (a));
    ColourGradient glow (theme::amber.withAlpha (0.35f), np.x, np.y, theme::amber.withAlpha (0.0f), np.x + 22.0f, np.y, true);
    g.setGradientFill (glow);
    g.fillEllipse (Rectangle<float> (44.0f, 44.0f).withCentre (np));
    theme::drawChickenHeadKnob (g, c, layout::channelKnobRadius, dragging ? dragAngle : a);
}

void ChannelDial::mouseDown (const MouseEvent& e)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float dist = e.position.getDistanceFrom (c);
    if (dist > layout::channelKnobRadius * 1.1f)
    {
        // click on a number
        const int ch = channelAt (e.position);
        if (ch != proc.getCurrentChannel()) proc.loadFactory (ch, 0);
        return;
    }
    dragging = true;
    dragAngle = angleForChannel (proc.getCurrentChannel());
}

void ChannelDial::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    const auto c = getLocalBounds().toFloat().getCentre();
    const auto d = e.position - c;
    dragAngle = std::atan2 (d.x, -d.y);
    repaint();
}

void ChannelDial::mouseUp (const MouseEvent& e)
{
    if (! dragging) return;
    dragging = false;
    const int ch = channelAt (e.position);
    if (ch != proc.getCurrentChannel()) proc.loadFactory (ch, 0);
    repaint();
}

void ChannelDial::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    proc.stepChannel (w.deltaY > 0 ? 1 : -1);
}

void ChannelDial::refresh() { if (proc.getCurrentChannel() != shown) repaint(); }

// ============================================================================
CabinetView::CabinetView (EffectorKillaAudioProcessor& p)
    : proc (p),
      rating ({ layout::ratingLed[0].x - layout::ratingPanel.getX(), layout::ratingLed[1].x - layout::ratingPanel.getX(), layout::ratingLed[2].x - layout::ratingPanel.getX() },
              layout::ratingTrack.translated (-layout::ratingPanel.getX(), -layout::ratingPanel.getY()), 38.0f, false),
      cable (layout::cableTrack.translated (-layout::cableTrack.getX() + 4.0f, -layout::cableTrack.getY() + 6.0f),
             layout::premiumLed.translated (-layout::cableTrack.getX() + 4.0f, -layout::cableTrack.getY() + 6.0f)),
      tv (p),
      av ({ layout::avPositions[0] - 596.0f, layout::avPositions[1] - 596.0f, layout::avPositions[2] - 596.0f },
          layout::avTrack.translated (-596.0f, -575.0f), 46.0f, true),
      channel (p),
      shelf (p),
      vcr (p)
{
    setOpaque (true);
    background = ImageCache::getFromMemory (EKData::reference_png, EKData::reference_pngSize);

    // KILL
    addAndMakeVisible (kill);
    place (kill, layout::killBezel);
    kill.onKill = [this]
    {
        proc.kill();
    };

    // PG / R / UNRATED
    addAndMakeVisible (rating);
    place (rating, layout::ratingPanel);
    rating.showLeds = true;
    for (int i = 0; i < 3; ++i)
    {
        rating.leds[(size_t) i] = layout::ratingLed[i] - layout::ratingPanel.getPosition();
        rating.clickZones[(size_t) i] = Rectangle<float> (90.0f, 44.0f).withCentre (rating.leds[(size_t) i].translated (0.0f, -14.0f));
    }
    rating.getPosition = [this] { return (int) proc.killLevel; };
    rating.onChange = [this] (int i) { proc.killLevel = (KillLevel) i; tv.showMessage (i == 0 ? "RATED PG" : (i == 1 ? "RATED R" : "UNRATED")); };

    // BOOTLEG / POCKET TV / AUTO TRACKING
    for (auto* b : { &bootleg, &pocketTv, &autoTracking }) addAndMakeVisible (*b);
    place (bootleg, layout::bootleg);
    place (pocketTv, layout::pocketTv);
    place (autoTracking, layout::autoTracking);
    bootleg.onClick = [this] { proc.bootleg(); tv.showMessage ("BOOTLEG", 0.8); };
    auto boolParam = [this] (const String& id) { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; };
    auto toggleParam = [this] (const String& id)
    {
        if (auto* prm = proc.getParam (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
        }
    };
    pocketTv.isLit = [boolParam] { return boolParam (ParamIDs::pocketTv); };
    pocketTv.blinkWhenLit = true;
    pocketTv.onClick = [toggleParam] { toggleParam (ParamIDs::pocketTv); };
    autoTracking.isLit = [boolParam] { return boolParam (ParamIDs::autoTracking); };
    autoTracking.onClick = [toggleParam] { toggleParam (ParamIDs::autoTracking); };

    // BASIC / PREMIUM CABLE
    addAndMakeVisible (cable);
    place (cable, layout::cableTrack.expanded (4.0f, 6.0f).withRight (layout::premiumLed.x + 14.0f));
    cable.isOn = [this] { return proc.premiumCable; };
    cable.onChange = [this] (bool on)
    {
        proc.premiumCable = on;
        tv.showMessage (on ? "PREMIUM CABLE" : "BASIC CABLE", 1.0);
        refreshAll();
    };

    // STAFF PICK
    addAndMakeVisible (staffPick);
    place (staffPick, layout::staffPick);
    staffPick.isOn = [this] { return proc.isCurrentFavourite(); };
    staffPick.onClick = [this] { proc.toggleFavourite(); tv.showMessage (proc.isCurrentFavourite() ? "STAFF PICK" : "UNPICKED", 1.0); };

    // ANTENNA IN / RF OUT
    for (auto* k : { &antennaIn, &rfOut }) addAndMakeVisible (*k);
    placeCentred (antennaIn, layout::antennaIn, layout::smallKnobRadius * 2.9f);
    placeCentred (rfOut, layout::rfOut, layout::smallKnobRadius * 2.9f);
    antennaIn.bindTo (*proc.getParam (ParamIDs::inGain));
    rfOut.bindTo (*proc.getParam (ParamIDs::outGain));
    auto dbText = [this] (const String& id) { return String (proc.apvts.getRawParameterValue (id)->load(), 1) + " dB"; };
    antennaIn.onUserChange = [this, dbText] (float v) { tv.showValue ("ANTENNA IN", v, dbText (ParamIDs::inGain)); };
    rfOut.onUserChange = [this, dbText] (float v) { tv.showValue ("RF OUT", v, dbText (ParamIDs::outGain)); };

    // TV
    addAndMakeVisible (tv);
    place (tv, layout::tvScreen);
    tv.onDetailClosed = [this] { shelf.repaint(); };

    // AV1 / AV2 / AV3
    addAndMakeVisible (av);
    place (av, { 596.0f, 575.0f, 360.0f, 64.0f });
    for (int i = 0; i < 3; ++i) av.clickZones[(size_t) i] = layout::avLabels[i].translated (-596.0f, -575.0f);
    av.getPosition = [this] { return (int) proc.getMeta().mod.mode; };
    av.onChange = [this] (int i)
    {
        auto m = proc.getMeta().mod;
        m.mode = (AvMode) i;
        proc.setModState (m, true);
    };

    // OFF AIR
    offAir = std::make_unique<OffAirSwitch> (*proc.getParam (ParamIDs::offAir));
    addAndMakeVisible (*offAir);
    place (*offAir, layout::offAirButton.getUnion (Rectangle<float> (26.0f, 26.0f).withCentre (layout::offAirLed)).expanded (4.0f));

    // channel selector + PROGRAM
    addAndMakeVisible (channel);
    placeCentred (channel, layout::channelKnob, layout::channelNumberRadius * 2.0f + 40.0f);
    for (auto* b : { &programUp, &programDown }) addAndMakeVisible (*b);
    place (programUp, layout::programUp);
    place (programDown, layout::programDown);
    programUp.onClick = [this] { proc.stepPreset (-1); };
    programDown.onClick = [this] { proc.stepPreset (1); };

    // macros
    for (int m = 0; m < 5; ++m)
    {
        auto& k = macros[(size_t) m];
        addAndMakeVisible (k);
        placeCentred (k, layout::macroKnob[m], layout::macroKnobRadius * 2.6f);
        k.bindTo (*proc.getParam (ParamIDs::macro (m)));
        k.getDisplayValue = [this, m] { return proc.engine.meters.macroValue[(size_t) m].load(); };
        k.onGestureStart = [this, m] { proc.beginUiGesture (m); };
        k.onGestureEnd = [this, m] { proc.endUiGesture (m); };
        k.onUserChange = [this, m] (float v) { tv.showValue (MacroEngine::macroName (m), v, String (roundToInt (v * 100.0f))); };
    }

    // cassettes + video recorder
    addAndMakeVisible (shelf);
    place (shelf, layout::shelf);
    shelf.onSelect = [this] (int s) { selectSlot (s); };
    shelf.onMessage = [this] (const String& m) { tv.showMessage (m, 1.0); };
    addAndMakeVisible (vcr);
    place (vcr, layout::vcrBody);
    vcr.onMessage = [this] (const String& m) { tv.showMessage (m, 1.0); };
    vcr.onEject = [this] { selectSlot (-1); };

    setSize ((int) layout::kWidth, (int) layout::kHeight);
}

CabinetView::~CabinetView() = default;

void CabinetView::place (Component& c, Rectangle<float> r) { c.setBounds (r.getSmallestIntegerContainer()); }
void CabinetView::placeCentred (Component& c, Point<float> centre, float size)
{
    c.setBounds (Rectangle<float> (size, size).withCentre (centre).getSmallestIntegerContainer());
}

void CabinetView::paint (Graphics& g)
{
    if (tv.isGlActive())
    {
        Path hole;
        hole.addRoundedRectangle (layout::tvScreen, layout::tvCorner);
        g.excludeClipRegion (layout::tvScreen.reduced (layout::tvCorner * 0.3f).toNearestInt());
    }
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
    else g.fillAll (Colour (0xff3a2415));

    // focus macro of the preset: amber marker next to the label
    const int focus = proc.getMeta().focusMacro;
    shownFocus = focus;
    if (focus >= 0 && focus < 5)
    {
        const auto r = layout::macroLabel[focus];
        ColourGradient glow (theme::amber.withAlpha (0.5f), r.getRight() + 6.0f, r.getCentreY(), theme::amber.withAlpha (0.0f), r.getRight() + 20.0f, r.getCentreY(), true);
        g.setGradientFill (glow);
        g.fillEllipse (Rectangle<float> (30.0f, 30.0f).withCentre ({ r.getRight() + 7.0f, r.getCentreY() }));
        g.setColour (theme::amber);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getRight() + 7.0f, r.getCentreY() }));
    }
}

void CabinetView::mouseDown (const MouseEvent& e)
{
    if (layout::logo.contains (e.position) || e.mods.isPopupMenu())
    {
        if (onLogoMenu) onLogoMenu (e);
        return;
    }
    // click on the cabinet closes the cassette detail
    if (proc.selectedSlot >= 0) selectSlot (-1);
}

void CabinetView::selectSlot (int slot)
{
    proc.selectedSlot = slot;
    tv.setDetailSlot (slot);
    shelf.repaint();
    vcr.repaint();
    if (slot >= 0)
    {
        const auto t = proc.getSlotType (slot);
        if (t != EffectType::None && ! proc.premiumCable)
            tv.showMessage (String (slot + 1) + " " + effectInfo (t).name.toUpperCase(), 1.2);
    }
}

void CabinetView::tick (double dt)
{
    tv.tick (dt);
    kill.tick();
    rating.refresh();
    for (auto* b : { &bootleg, &pocketTv, &autoTracking }) b->refresh();
    cable.refresh();
    staffPick.refresh();
    av.refresh();
    offAir->refresh();
    channel.refresh();
    for (auto& k : macros) k.tick();
    shelf.tick();
    vcr.tick();
    if (proc.getMeta().focusMacro != shownFocus) repaint (layout::macroLabel[0].getUnion (layout::macroLabel[4]).expanded (30.0f).toNearestInt());
    if (tv.getDetailSlot() != proc.selectedSlot) tv.setDetailSlot (proc.selectedSlot);
}

void CabinetView::refreshAll()
{
    if (proc.selectedSlot >= 0 && proc.getSlotMeta (proc.selectedSlot).isEmpty()) selectSlot (-1);
    tv.setDetailSlot (proc.selectedSlot);
    repaint();
    for (auto* c : getChildren()) c->repaint();
}

} // namespace ek::ui
