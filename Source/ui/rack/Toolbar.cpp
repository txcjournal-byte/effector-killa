#include "Toolbar.h"
#include "CardStyle.h"
#include "../Brand.h"

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
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool hover = isMouseOver();
    const auto& m = proc.getMeta();

    if (hover) hw::drawGlow (g, r, rad, hw::col::iceDeep, 0.8f, 8.0f);
    g.setColour (hw::col::bg);
    g.fillRoundedRectangle (r, rad);
    ColourGradient inner (Colour (0xff0d1422), 0.0f, r.getY(), Colour (0xff070b12), 0.0f, r.getBottom(), false);
    g.setGradientFill (inner);
    g.fillRoundedRectangle (r, rad);
    g.setColour (hover ? hw::col::ice.withAlpha (0.5f) : hw::col::ice.withAlpha (0.26f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad - 0.5f, 1.0f);

    auto inner2 = r.reduced (rad * 0.75f, 0.0f);
    if (message.isNotEmpty())
    {
        card::glowText (g, message.toUpperCase(), inner2, hw::mono (12.5f, true), hw::col::ice, Justification::centred, 1.0f);
        return;
    }

    // right: source (CH 10 / USER / KILL) + chevron
    String sub;
    if (m.factory && m.channel >= 0) sub = "CH" + String (m.channel + 1).paddedLeft ('0', 2);
    else if (m.seed != 0) sub = "KILL";
    else sub = "USER";
    hw::icon::chevronDown (g, inner2.removeFromRight (14.0f), hover ? hw::col::print : hw::col::text2);
    inner2.removeFromRight (4.0f);
    auto subArea = inner2.removeFromRight (40.0f);
    hw::drawPrint (g, sub, subArea, Justification::centredRight, 9.5f, hw::col::printDim);
    inner2.removeFromRight (6.0f);

    // favourite sparkle
    auto nameArea = inner2;
    const bool fav = proc.isCurrentFavourite();
    auto sp = nameArea.removeFromLeft (16.0f);
    if (fav) card::paintSparkle (g, sp.getCentre(), 6.0f, hw::col::ice, 1.0f);
    else card::paintSparkle (g, sp.getCentre(), 3.0f, hw::col::ice.withAlpha (0.35f));
    nameArea.removeFromLeft (6.0f);

    const double t = (hw::animSeconds() - scanStart) / kScanSeconds;
    const bool scanning = t >= 0.0 && t < 1.0;
    const float revealX = scanning ? nameArea.getX() + (float) t * (r.getRight() - nameArea.getX()) : r.getRight();
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (Rectangle<float> (r.getX(), r.getY(), revealX - r.getX(), r.getHeight()).toNearestInt());
        auto text = m.name.toUpperCase();
        float fh = 13.5f;
        while (fh > 9.0f && GlyphArrangement::getStringWidth (hw::mono (fh, true), text) > nameArea.getWidth()) fh -= 0.5f;
        card::glowText (g, text, nameArea, hw::mono (fh, true), hw::col::print, Justification::centredLeft, 0.5f);
    }
    if (scanning)
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (r, rad);
        g.reduceClipRegion (clip);
        const float a = (float) (1.0 - t);
        ColourGradient bar (hw::col::ice.withAlpha (0.0f), revealX - 30.0f, 0.0f, hw::col::ice.withAlpha (0.35f * a + 0.1f), revealX, 0.0f, false);
        g.setGradientFill (bar);
        g.fillRect (Rectangle<float> (revealX - 30.0f, r.getY(), 30.0f, r.getHeight()));
        g.setColour (Colours::white.withAlpha (0.7f * a));
        g.fillRect (Rectangle<float> (revealX - 1.0f, r.getY() + 4.0f, 1.2f, r.getHeight() - 8.0f));
    }
}

// ============================================================================================
Toolbar::Toolbar (EffectorKillaAudioProcessor& p)
    : proc (p), lcd (p),
      libBtn (hw::HwButton::Kind::Icon), prevBtn (hw::HwButton::Kind::Icon), nextBtn (hw::HwButton::Kind::Icon), saveBtn (hw::HwButton::Kind::Icon),
      revertBtn (hw::HwButton::Kind::Icon), favBtn (hw::HwButton::Kind::Icon, {}, hw::col::ice),
      aBtn (hw::HwButton::Kind::Text, "A"), bBtn (hw::HwButton::Kind::Text, "B"),
      undoBtn (hw::HwButton::Kind::Icon), redoBtn (hw::HwButton::Kind::Icon),
      settingsBtn (hw::HwButton::Kind::Icon), infoBtn (hw::HwButton::Kind::Icon),
      bypassBtn (hw::HwButton::Kind::Text, "BYPASS", hw::col::red)
{
    addAndMakeVisible (lcd);
    lcd.onClick = [this] { if (onBrowse) onBrowse(); };

    libBtn.drawIcon = hw::icon::library;
    libBtn.isLit = [this] { return isLibraryShown && isLibraryShown(); };
    libBtn.onClick = [this] { if (onToggleLibrary) onToggleLibrary(); };
    libBtn.setTooltip ("FX LIBRARY: show / hide the effect library on the left (G) - the window gets narrower without it");
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
    favBtn.setTooltip ("Favourite (the sparkle in the preset pill)");
    aBtn.setTooltip ("A / B: two complete rack states to compare - this is A");
    bBtn.setTooltip ("A / B: two complete rack states to compare - this is B");
    undoBtn.setTooltip ("Undo (Ctrl/Cmd + Z)");
    redoBtn.setTooltip ("Redo (Ctrl/Cmd + Y)");
    settingsBtn.setTooltip ("Settings: window size, oversampling, reduce motion, preset folder, keys");
    infoBtn.setTooltip ("About TrapWin Effector / open the manual");
    bypassBtn.setTooltip ("BYPASS = COMPARE: the original signal, level-matched. Click = toggle, hold = listen while held.");

    bypassBtn.fontSize = 11.0f;
    aBtn.fontSize = bBtn.fontSize = 11.0f;
    for (auto* b : { &libBtn, &prevBtn, &nextBtn, &saveBtn, &revertBtn, &favBtn, &aBtn, &bBtn, &undoBtn, &redoBtn,
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
    for (auto* b : { &libBtn, &favBtn, &aBtn, &bBtn, &undoBtn, &redoBtn, &bypassBtn }) b->tick();
}

void Toolbar::resized()
{
    const int y = 13, h = 30, w = 30;
    // logo (painted)
    {
        const auto& logo = brand::logo();
        const float lh = 46.0f;
        const float lw = logo.isValid() ? lh * (float) logo.getWidth() / (float) logo.getHeight() : 99.0f;
        logoArea = { 16.0f, ((float) height - lh) * 0.5f + 1.0f, lw, lh };
    }
    // product name in chrome italic next to the (image) logo - the logo itself is never redrawn as text
    productArea = { logoArea.getRight() + 8.0f, 14.0f, 124.0f, 30.0f };
    int x = (int) productArea.getRight() + 16;
    auto place = [&] (Component& c, int width, int gapAfter = 6) { c.setBounds (x, y, width, h); x += width + gapAfter; };
    place (libBtn, w, 16);
    place (aBtn, 38, 4); place (bBtn, 38, 14);
    place (undoBtn, w); place (redoBtn, w, 0);
    const int leftEnd = x;

    int rx = getWidth() - 18;
    auto right = [&] (Component& c, int width, int gapBefore = 6) { rx -= width; c.setBounds (rx, y, width, h); rx -= gapBefore; };
    right (bypassBtn, 104, 12);
    right (infoBtn, w);
    right (settingsBtn, w);
    const int rightStart = rx;

    // centre group: < [preset] >  save revert fav, centred over the chain when there is room
    const int extra = 12 + 3 * w + 2 * 6;
    const int lcdW = jlimit (170, 300, rightStart - 36 - leftEnd - (w + 6 + 6 + w) - extra);
    const int core = w + 6 + lcdW + 6 + w;
    const int cx = centreX > 0 ? centreX : getWidth() / 2;
    x = jlimit (leftEnd + 18, jmax (leftEnd + 18, rightStart - 18 - core - extra), cx - core / 2);
    place (prevBtn, w);
    place (lcd, lcdW);
    place (nextBtn, w, 12);
    place (saveBtn, w); place (revertBtn, w); place (favBtn, w);
}

void Toolbar::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // hairline under the header, fading out at both ends
    ColourGradient lineG (hw::col::ice.withAlpha (0.0f), 0.0f, 0.0f, hw::col::ice.withAlpha (0.0f), r.getRight(), 0.0f, false);
    lineG.addColour (0.18, hw::col::ice.withAlpha (0.14f));
    lineG.addColour (0.5, hw::col::ice.withAlpha (0.22f));
    lineG.addColour (0.82, hw::col::ice.withAlpha (0.14f));
    g.setGradientFill (lineG);
    g.fillRect (Rectangle<float> (0.0f, r.getBottom() - 1.0f, r.getWidth(), 1.0f));

    const auto& logo = brand::logo();
    if (logo.isValid())
    {
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (logo, logoArea, RectanglePlacement::centred);
    }
    ek::card::paintChromeTitle (g, "EFFECTOR", productArea, 21.0f, 0.8f);
    // thin separators between the groups
    auto sep = [&] (const Component& left, const Component& right)
    {
        const float x = (float) (left.getRight() + right.getX()) * 0.5f;
        g.setColour (hw::col::line);
        g.fillRect (Rectangle<float> (x - 0.5f, 18.0f, 1.0f, r.getHeight() - 36.0f));
    };
    sep (libBtn, aBtn);
    sep (bBtn, undoBtn);
    sep (infoBtn, bypassBtn);
}
} // namespace ek::ui
