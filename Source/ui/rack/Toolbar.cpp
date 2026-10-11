#include "Toolbar.h"

namespace ek::ui
{
using namespace juce;

namespace
{
constexpr double kScanSeconds = 0.42;
const String kStar = String::fromUTF8 ("\xe2\x98\x85");
const String kDot = String::fromUTF8 (" \xc2\xb7 ");
} // namespace

// ============================================================================================
PresetLcd::PresetLcd (EffectorKillaAudioProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setTooltip ("Preset - click to open the preset browser (B)");
}

void PresetLcd::showMessage (const String& m, int ms)
{
    message = m;
    messageUntil = Time::getMillisecondCounter() + (uint32) ms;
    repaint();
}

void PresetLcd::tick()
{
    const int ev = proc.programEvents.load();
    const auto& name = proc.getMeta().name;
    const bool fav = proc.isCurrentFavourite();
    if (ev != lastProgramEvents || name != lastName)
    {
        // the very first update (editor opened) does not scan
        if (lastProgramEvents >= 0 && ! hw::reduceMotion()) scanStart = hw::animSeconds();
        lastProgramEvents = ev;
        lastName = name;
        repaint();
    }
    if (fav != lastFav) { lastFav = fav; repaint(); }
    if (hw::animSeconds() - scanStart < kScanSeconds + 0.1) repaint();
    if (messageUntil != 0 && Time::getMillisecondCounter() > messageUntil) { messageUntil = 0; message.clear(); repaint(); }
}

void PresetLcd::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    hw::drawLcdGlass (g, r, 3.0f);
    auto inner = r.reduced (9.0f, 2.0f);

    // faint pixel grid of a VFD
    g.setColour (hw::col::vfd.withAlpha (0.035f));
    for (float x = inner.getX(); x < inner.getRight(); x += 3.0f) g.drawVerticalLine ((int) x, inner.getY() + 2.0f, inner.getBottom() - 2.0f);

    const auto& m = proc.getMeta();
    const bool hover = isMouseOver();

    if (message.isNotEmpty())
    {
        theme::drawGlowText (g, message.toUpperCase(), inner, theme::vfdFont (15.0f), hw::col::amber, Justification::centred, 0.8f);
        hw::drawGlassReflection (g, r, 3.0f);
        return;
    }

    // right: source of the preset (CH 10 / USER / KILL) + browse arrow
    String sub;
    if (m.factory && m.channel >= 0) sub = "CH" + String (m.channel + 1).paddedLeft ('0', 2);
    else if (m.seed != 0) sub = "KILL";
    else sub = "USER";
    auto arrow = inner.removeFromRight (12.0f);
    {
        Path tri;
        const auto c = arrow.getCentre();
        tri.addTriangle (c.x - 4.0f, c.y - 2.0f, c.x + 4.0f, c.y - 2.0f, c.x, c.y + 3.0f);
        g.setColour (hw::col::vfd.withAlpha (hover ? 1.0f : 0.55f));
        g.fillPath (tri);
    }
    auto subArea = inner.removeFromRight (50.0f);
    g.setFont (theme::monoLabel (9.5f));
    g.setColour (hw::col::vfd.withAlpha (0.5f));
    g.drawText (sub, subArea, Justification::centredRight, false);

    // name (with the favourite star)
    const bool fav = proc.isCurrentFavourite();
    auto nameArea = inner;
    if (fav)
    {
        hw::icon::star (g, nameArea.removeFromLeft (16.0f), hw::col::amber, true);
        nameArea.removeFromLeft (4.0f);
    }

    const double t = (hw::animSeconds() - scanStart) / kScanSeconds;
    const bool scanning = t >= 0.0 && t < 1.0;
    const float revealX = scanning ? nameArea.getX() + (float) t * (r.getRight() - nameArea.getX()) : r.getRight();
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (Rectangle<float> (r.getX(), r.getY(), revealX - r.getX(), r.getHeight()).toNearestInt());
        auto text = m.name.toUpperCase();
        // shrink long names instead of clipping them
        float fh = 16.0f;
        while (fh > 10.0f && GlyphArrangement::getStringWidth (theme::vfdFont (fh), text) > nameArea.getWidth()) fh -= 0.5f;
        theme::drawGlowText (g, text, nameArea, theme::vfdFont (fh), hw::col::vfd, Justification::centredLeft, 0.9f);
    }
    if (scanning)
    {
        // bright scan bar
        const float a = (float) (1.0 - t);
        ColourGradient bar (hw::col::vfd.withAlpha (0.0f), revealX - 18.0f, 0.0f, hw::col::vfd.withAlpha (0.55f * a + 0.2f), revealX, 0.0f, false);
        g.setGradientFill (bar);
        g.fillRect (Rectangle<float> (revealX - 18.0f, r.getY() + 2.0f, 18.0f, r.getHeight() - 4.0f));
        g.setColour (Colours::white.withAlpha (0.8f));
        g.fillRect (Rectangle<float> (revealX - 1.0f, r.getY() + 2.0f, 1.5f, r.getHeight() - 4.0f));
    }
    if (hover)
    {
        g.setColour (hw::col::ice.withAlpha (0.35f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
    hw::drawGlassReflection (g, r, 3.0f);
}

// ============================================================================================
Toolbar::Toolbar (EffectorKillaAudioProcessor& p)
    : proc (p), lcd (p),
      prevBtn (hw::HwButton::Kind::Icon), nextBtn (hw::HwButton::Kind::Icon), saveBtn (hw::HwButton::Kind::Icon),
      revertBtn (hw::HwButton::Kind::Icon), favBtn (hw::HwButton::Kind::Icon, {}, hw::col::amber),
      aBtn (hw::HwButton::Kind::Text, "A"), bBtn (hw::HwButton::Kind::Text, "B"),
      undoBtn (hw::HwButton::Kind::Icon), redoBtn (hw::HwButton::Kind::Icon),
      settingsBtn (hw::HwButton::Kind::Icon), infoBtn (hw::HwButton::Kind::Icon),
      bypassBtn (hw::HwButton::Kind::Text, "BYPASS", hw::col::red)
{
    addAndMakeVisible (lcd);
    lcd.onClick = [this] { if (onBrowse) onBrowse(); };

    prevBtn.drawIcon = hw::icon::prev;
    nextBtn.drawIcon = hw::icon::next;
    saveBtn.drawIcon = hw::icon::save;
    revertBtn.drawIcon = hw::icon::revert;
    favBtn.drawIcon = [this] (Graphics& g, Rectangle<float> r, Colour c) { hw::icon::star (g, r, c, proc.isCurrentFavourite()); };
    undoBtn.drawIcon = hw::icon::undo;
    redoBtn.drawIcon = hw::icon::redo;
    settingsBtn.drawIcon = hw::icon::cog;
    infoBtn.drawIcon = hw::icon::info;

    prevBtn.onClick = [this] { proc.stepPreset (-1); };
    nextBtn.onClick = [this] { proc.stepPreset (1); };
    saveBtn.onClick = [this] { if (onSave) onSave(); };
    revertBtn.onClick = [this] { revertPreset(); };
    favBtn.isLit = [this] { return proc.isCurrentFavourite(); };
    favBtn.onClick = [this] { proc.toggleFavourite(); lcd.repaint(); };
    aBtn.isLit = [this] { return proc.getSide() == 0; };
    bBtn.isLit = [this] { return proc.getSide() == 1; };
    aBtn.onClick = [this] { proc.setSide (0); };
    bBtn.onClick = [this] { proc.setSide (1); };
    undoBtn.isEnabledFn = [this] { return proc.canUndo(); };
    redoBtn.isEnabledFn = [this] { return proc.canRedo(); };
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    settingsBtn.onClick = [this] { if (onSettings) onSettings(); };
    infoBtn.onClick = [this] { if (onInfo) onInfo(); };

    bypassBtn.isLit = [this] { auto* q = proc.getParam (ParamIDs::offAir); return q != nullptr && q->getValue() > 0.5f; };
    bypassBtn.onPress = [this] (bool down)
    {
        auto* q = proc.getParam (ParamIDs::offAir);
        if (q == nullptr) return;
        if (down)
        {
            bypassDownAt = Time::getMillisecondCounter();
            bypassWasOn = q->getValue() > 0.5f;
            q->setValueNotifyingHost (bypassWasOn ? 0.0f : 1.0f);
        }
        else if (! bypassWasOn && Time::getMillisecondCounter() - bypassDownAt > 400)
        {
            q->setValueNotifyingHost (0.0f);   // held: momentary
        }
    };

    prevBtn.setTooltip ("Previous preset (Left arrow)");
    nextBtn.setTooltip ("Next preset (Right arrow)");
    saveBtn.setTooltip ("Save the whole rack as a preset (Ctrl/Cmd + S)");
    revertBtn.setTooltip ("Revert: reload the current preset and drop your changes");
    favBtn.setTooltip ("Favourite (STAFF PICK)");
    aBtn.setTooltip ("A / B: two complete rack states to compare - this is A");
    bBtn.setTooltip ("A / B: two complete rack states to compare - this is B");
    undoBtn.setTooltip ("Undo (Ctrl/Cmd + Z)");
    redoBtn.setTooltip ("Redo (Ctrl/Cmd + Y)");
    settingsBtn.setTooltip ("Settings: window size, oversampling, reduce motion, preset folder, keys");
    infoBtn.setTooltip ("About TrapWin Effector / open the manual");
    bypassBtn.setTooltip ("BYPASS = COMPARE: the original signal, level-matched. Click = toggle, hold = listen while held.");

    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &revertBtn, &favBtn, &aBtn, &bBtn, &undoBtn, &redoBtn,
                     &settingsBtn, &infoBtn, &bypassBtn })
        addAndMakeVisible (b);
}

void Toolbar::revertPreset()
{
    const auto& m = proc.getMeta();
    if (m.factory && m.channel >= 0 && m.index >= 0)
    {
        proc.loadFactory (m.channel, m.index);
        lcd.showMessage ("REVERTED", 1200);
        return;
    }
    if (! m.factory && m.seed == 0)
    {
        const auto wanted = m.name;
        for (auto& f : proc.bank.getUserPresetFiles())
        {
            if (auto pr = proc.bank.loadUserPreset (f))
            {
                if (pr->name == wanted)
                {
                    proc.loadUserPreset (f);
                    lcd.showMessage ("REVERTED", 1200);
                    return;
                }
            }
        }
    }
    lcd.showMessage ("NOTHING TO REVERT", 1600);
}

void Toolbar::tick()
{
    lcd.tick();
    for (auto* b : { &favBtn, &aBtn, &bBtn, &undoBtn, &redoBtn, &bypassBtn }) b->tick();
}

void Toolbar::resized()
{
    const int y = 7, h = 26, w = 28;
    int x = 12;
    auto place = [&] (Component& c, int width, int gapAfter = 4) { c.setBounds (x, y, width, h); x += width + gapAfter; };
    place (prevBtn, w); place (nextBtn, w, 8);
    place (lcd, 300, 8);
    place (saveBtn, w); place (revertBtn, w); place (favBtn, w, 14);
    place (aBtn, w); place (bBtn, w, 14);
    place (undoBtn, w); place (redoBtn, w);

    int rx = getWidth() - 12;
    auto right = [&] (Component& c, int width, int gapBefore = 4) { rx -= width; c.setBounds (rx, y, width, h); rx -= gapBefore; };
    right (bypassBtn, 96, 14);
    right (infoBtn, w);
    right (settingsBtn, w);
}

void Toolbar::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    ColourGradient metal (hw::col::toolbarTop, 0.0f, 0.0f, hw::col::toolbarBottom, 0.0f, r.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRect (r);
    // fine brushing
    for (int i = 0; i < (int) r.getHeight(); i += 2)
    {
        g.setColour (Colours::white.withAlpha (0.012f + 0.01f * (float) ((i * 7) % 3)));
        g.drawHorizontalLine (i, 0.0f, r.getRight());
    }
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (0, 0.0f, r.getRight());
    g.setColour (Colours::black);
    g.drawHorizontalLine ((int) r.getBottom() - 1, 0.0f, r.getRight());
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getBottom() - 2, 0.0f, r.getRight());

    // engraved separators between groups
    auto sep = [&] (const Component& left, const Component& right)
    {
        const float x = (float) (left.getRight() + right.getX()) * 0.5f;
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, 9.0f, r.getBottom() - 9.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x + 1, 9.0f, r.getBottom() - 9.0f);
    };
    sep (favBtn, aBtn);
    sep (bBtn, undoBtn);
    sep (infoBtn, bypassBtn);
}
} // namespace ek::ui
