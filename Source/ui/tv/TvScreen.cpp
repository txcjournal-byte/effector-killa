#include "TvScreen.h"
#include "../cabinet/Layout.h"

namespace ek::ui
{
using namespace juce;

namespace
{
    constexpr float W = 843.0f, H = 471.0f;
    const char* sourceNames[] = { "MELODY", "VOX", "808", "DRUMS", "FX", "BUS" };

    void drawBlocks (Graphics& g, Rectangle<float> r, float norm, int blocks, Colour c)
    {
        const float gap = 3.0f;
        const float bw = (r.getWidth() - gap * (float) (blocks - 1)) / (float) blocks;
        const int filled = roundToInt (norm * (float) blocks);
        for (int i = 0; i < blocks; ++i)
        {
            auto b = Rectangle<float> (r.getX() + (float) i * (bw + gap), r.getY(), bw, r.getHeight());
            if (i < filled) { g.setColour (c); g.fillRect (b); }
            else { g.setColour (c.withAlpha (0.8f)); g.drawRect (b, 1.6f); }
        }
    }

    void osdText (Graphics& g, const String& t, Rectangle<float> r, float size, Justification j, Colour c = theme::osdWhite)
    {
        g.setFont (theme::osdFont (size));
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawText (t, r.translated (2.0f, 2.0f), j, false);
        g.setColour (c.withAlpha (0.25f));
        g.drawText (t, r.translated (-1.0f, 0.0f), j, false);
        g.drawText (t, r.translated (1.0f, 0.0f), j, false);
        g.setColour (c);
        g.drawText (t, r, j, false);
    }
}

TvScreen::TvScreen (EffectorKillaAudioProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (false);
    lastKillEvents = proc.killEvents.load();
    lastProgramEvents = proc.programEvents.load();
    lastClipCount = proc.engine.meters.clipCount.load();
    lastCornerHits = proc.engine.meters.cornerHits.load();
}

TvScreen::~TvScreen() = default;

String TvScreen::avName (int mode)
{
    switch (mode)
    {
        case 0: return "AV1 REMOTE";
        case 1: return "AV2 SCRIBBLE";
        default: return "AV3 SCREENSAVER";
    }
}

// ============================================================================
// clock / state
// ============================================================================
void TvScreen::tick (double dt)
{
    clock += dt;
    auto& m = proc.engine.meters;

    const int ke = proc.killEvents.load();
    const int pe = proc.programEvents.load();
    if (ke != lastKillEvents)
    {
        lastKillEvents = ke;
        lastProgramEvents = pe;
        killStart = clock;
        nowPlayingUntil = clock + 0.7 + 2.0;
    }
    else if (pe != lastProgramEvents)
    {
        lastProgramEvents = pe;
        switchStart = clock;
    }
    const int cc = m.clipCount.load();
    if (cc != lastClipCount) { lastClipCount = cc; overloadUntil = clock + 0.6; }
    const int ch = m.cornerHits.load();
    if (ch != lastCornerHits) { lastCornerHits = ch; glitchFlash = clock; }

    const bool offAir = proc.apvts.getRawParameterValue (ParamIDs::offAir)->load() > 0.5f;
    const bool pocketOn = proc.apvts.getRawParameterValue (ParamIDs::pocketTv)->load() > 0.5f;
    const float step = (float) dt / 0.35f;
    power = offAir ? std::max (0.0f, power - step) : std::min (1.0f, power + step);
    pocket = pocketOn ? std::min (1.0f, pocket + (float) dt / 0.25f) : std::max (0.0f, pocket - (float) dt / 0.25f);
    const float lv = std::min (1.0f, m.outLevel.load() * 3.0f);
    level += (lv - level) * (lv > level ? 0.5f : 0.08f);

    // OSD content key: repaint the (GL) OSD texture only when it really changes
    String key;
    key << proc.getMeta().name << "|" << proc.getCurrentChannel() << "|" << (int) proc.getMeta().mod.mode
        << "|" << proc.getMeta().mod.targetX << "|" << proc.getMeta().mod.targetY << "|" << proc.getMeta().mod.scribbleBars
        << "|" << proc.getMeta().mod.screensaverSpeed << "|" << (int) proc.getMeta().source << "|" << detailSlot << "|" << hoverCell
        << "|" << (clock < valueUntil ? valueName + valueText : String()) << "|" << (clock < messageUntil ? message : String())
        << "|" << (clock < overloadUntil ? 1 : 0) << "|" << (clock < nowPlayingUntil ? 1 : 0) << "|" << power << "|" << (proc.premiumCable ? 1 : 0)
        << "|" << (int) ((clock - killStart) * 10.0);
    if (detailSlot >= 0)
    {
        for (int c = 0; c < 9; ++c) key << "|" << roundToInt (getDetailValue (c) * 1000.0f);
        const auto s = proc.getSlotState (detailSlot);
        key << (s.pause ? 1 : 0) << (int) s.ms << (s.writeProtect ? 1 : 0) << proc.getScreening() << (int) s.type;
    }
    if (key != lastOsdKey) { lastOsdKey = key; osdDirty = true; }

    // software fallback: 30 fps
    if (! glActive && (++frameCounter % 2) == 0) repaint();
}

void TvScreen::showValue (const String& name, float norm, const String& text)
{
    valueName = name;
    valueNorm = norm;
    valueText = text;
    valueUntil = clock + 1.5;
    osdDirty = true;
}

void TvScreen::showMessage (const String& text, double seconds)
{
    message = text;
    messageUntil = clock + seconds;
    osdDirty = true;
}

void TvScreen::setDetailSlot (int slot)
{
    const int s = (slot >= 0 && slot < kNumSlots && ! proc.getSlotMeta (slot).isEmpty()) ? slot : -1;
    if (s == detailSlot) return;
    detailSlot = s;
    dragCell = hoverCell = -1;
    osdDirty = true;
    repaint();
}

void TvScreen::setGlActive (bool on)
{
    glActive = on;
    setOpaque (! on);
    osdDirty = true;
    repaint();
}

TvFrame TvScreen::getFrame() const
{
    TvFrame f;
    auto& m = proc.engine.meters;
    f.time = (float) clock;
    const double bpm = m.bpm.load();
    f.beat = m.playing.load() ? (float) m.ppq.load() : (float) (clock * bpm / 60.0);
    f.channel = jlimit (0, 11, proc.getCurrentChannel());
    const auto& chans = proc.bank.getChannels();
    if (f.channel < (int) chans.size()) f.colour = chans[(size_t) f.channel].colour;
    f.level = level;
    f.transient = jlimit (0.0f, 1.0f, m.transient.load() * 4.0f);
    for (int i = 0; i < kNumMacros; ++i) f.macros[(size_t) i] = m.macroValue[(size_t) i].load();

    const double kt = clock - killStart;
    float glitch = 0.0f;
    if (kt >= 0.0 && kt < 0.3) glitch = 1.0f;
    const double beatDur = 60.0 / std::max (40.0, bpm);
    const double gt = clock - glitchFlash;
    if (gt >= 0.0 && gt < beatDur) glitch = std::max (glitch, (float) (1.0 - gt / beatDur) * 0.8f);
    glitch = std::max (glitch, m.glitch.load() * 0.6f);
    f.glitch = glitch;
    f.blue = (kt >= 0.3 && kt < 0.55) ? 1.0f : 0.0f;
    f.snow = (kt >= 0.55 && kt < 0.7) || (clock - switchStart >= 0.0 && clock - switchStart < 0.15) ? 1.0f : 0.0f;
    f.power = power;
    f.pocket = pocket;
    f.calm = proc.premiumCable ? 0.0f : 1.0f;

    const auto& mod = proc.getMeta().mod;
    f.avMode = (int) mod.mode;
    if (mod.mode == AvMode::Remote)
    {
        f.dotVisible = true;
        f.dot = { getTargetValue (mod.targetX), getTargetValue (mod.targetY) };
    }
    else if (mod.mode == AvMode::Scribble)
    {
        f.path = drawing ? stroke : mod.path;
        f.dotVisible = ! drawing && mod.path.size() > 1;
        f.dot = { m.dotX.load(), m.dotY.load() };
    }
    else
    {
        f.logoVisible = true;
        f.logo = { m.dotX.load(), m.dotY.load() };
    }
    f.osdDirty = osdDirty;
    return f;
}

// ============================================================================
// geometry
// ============================================================================
Rectangle<float> TvScreen::detailPanelArea() const { return { 26.0f, 214.0f, W - 52.0f, 236.0f }; }

Rectangle<float> TvScreen::detailCell (int index) const
{
    const auto a = detailPanelArea();
    const float top = a.getY() + 52.0f;
    const float cw = (a.getWidth() - 36.0f) / 3.0f, ch = 58.0f;
    const int col = index % 3, row = index / 3;
    return { a.getX() + 18.0f + (float) col * cw, top + (float) row * ch, cw - 18.0f, ch - 8.0f };
}

Rectangle<float> TvScreen::detailToggle (int index) const
{
    const auto a = detailPanelArea();
    const float w = 92.0f;
    return { a.getRight() - 16.0f - (float) (4 - index) * (w + 8.0f), a.getY() + 12.0f, w, 28.0f };
}

Rectangle<float> TvScreen::axisLabel (int axis) const
{
    return axis == 0 ? Rectangle<float> (40.0f, 438.0f, 190.0f, 22.0f) : Rectangle<float> (40.0f, 60.0f, 190.0f, 22.0f);
}
Rectangle<float> TvScreen::barSelector() const { return { 410.0f, 402.0f, 400.0f, 58.0f }; }
Rectangle<float> TvScreen::sourceLabel() const { return { W - 250.0f, 58.0f, 210.0f, 22.0f }; }
Rectangle<float> TvScreen::modeLabel() const { return { 40.0f, 18.0f, 380.0f, 36.0f }; }

Point<float> TvScreen::toNorm (Point<float> l) const
{
    const float m = 30.0f;
    return { jlimit (0.0f, 1.0f, (l.x - m) / (W - 2 * m)), jlimit (0.0f, 1.0f, 1.0f - (l.y - m) / (H - 2 * m)) };
}
Point<float> TvScreen::fromNorm (Point<float> n) const
{
    const float m = 30.0f;
    return { m + n.x * (W - 2 * m), m + (1.0f - n.y) * (H - 2 * m) };
}

String TvScreen::targetName (int target) const
{
    if (ModTarget::isMacro (target)) return MacroEngine::macroName (target);
    if (ModTarget::isSlotParam (target))
    {
        const int s = ModTarget::slotOf (target), p = ModTarget::paramOf (target);
        const auto t = proc.getSlotType (s);
        if (t == EffectType::None) return "SLOT " + String (s + 1);
        return String (s + 1) + ":" + (p == 8 ? String ("MIX") : effectInfo (t).params[(size_t) jlimit (0, 7, p)].name);
    }
    return "-";
}

float TvScreen::getTargetValue (int target) const
{
    if (ModTarget::isMacro (target)) return proc.apvts.getRawParameterValue (ParamIDs::macro (target))->load();
    if (ModTarget::isSlotParam (target)) return proc.getSlotParam (ModTarget::slotOf (target), ModTarget::paramOf (target));
    return 0.5f;
}

void TvScreen::setTargetValue (int target, float v, bool begin, bool end)
{
    RangedAudioParameter* p = nullptr;
    if (ModTarget::isMacro (target)) p = proc.getParam (ParamIDs::macro (target));
    else if (ModTarget::isSlotParam (target))
    {
        const int s = ModTarget::slotOf (target), k = ModTarget::paramOf (target);
        p = proc.getParam (k == 8 ? ParamIDs::slotMix (s) : ParamIDs::slotParam (s, k));
    }
    if (p == nullptr) return;
    if (begin) { if (ModTarget::isMacro (target)) proc.beginUiGesture (target); p->beginChangeGesture(); }
    p->setValueNotifyingHost (jlimit (0.0f, 1.0f, v));
    if (end) { p->endChangeGesture(); if (ModTarget::isMacro (target)) proc.endUiGesture (target); }
}

// ============================================================================
// detail panel values
// ============================================================================
float TvScreen::getDetailValue (int cell) const
{
    if (detailSlot < 0) return 0.0f;
    return proc.getSlotParam (detailSlot, cell == 8 ? 8 : cell);
}

void TvScreen::setDetailValue (int cell, float norm, bool asGesture)
{
    if (detailSlot < 0) return;
    const auto id = cell == 8 ? ParamIDs::slotMix (detailSlot) : ParamIDs::slotParam (detailSlot, cell);
    if (auto* p = proc.getParam (id))
    {
        if (cell < 8)
        {
            const auto& spec = effectInfo (proc.getSlotType (detailSlot)).params[(size_t) cell];
            if (spec.isDiscrete() && spec.numSteps() > 1)
            {
                const int steps = spec.numSteps() - 1;
                norm = (float) roundToInt (norm * (float) steps) / (float) steps;
            }
        }
        if (asGesture) p->setValueNotifyingHost (jlimit (0.0f, 1.0f, norm));
        else { p->beginChangeGesture(); p->setValueNotifyingHost (jlimit (0.0f, 1.0f, norm)); p->endChangeGesture(); }
    }
    osdDirty = true;
}

std::vector<int> TvScreen::detailParams() const
{
    std::vector<int> v;
    if (detailSlot < 0) return v;
    const auto& info = effectInfo (proc.getSlotType (detailSlot));
    for (int c = 0; c < kNumParams; ++c)
        if (info.params[(size_t) c].used) v.push_back (c);
    v.push_back (8);
    return v;
}

Rectangle<float> TvScreen::detailRectFor (int param) const
{
    const auto v = detailParams();
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] == param) return detailCell ((int) i);
    return {};
}

int TvScreen::detailCellAt (Point<float> p) const
{
    const auto v = detailParams();
    for (size_t i = 0; i < v.size(); ++i)
        if (detailCell ((int) i).expanded (4.0f).contains (p)) return v[i];
    return -1;
}

// ============================================================================
// painting
// ============================================================================
void TvScreen::paint (Graphics& g)
{
    if (glActive) return; // the OpenGL renderer draws the picture

    Path screen;
    screen.addRoundedRectangle (getLocalBounds().toFloat(), layout::tvCorner);
    g.reduceClipRegion (screen);
    g.fillAll (Colours::black);

    const auto f = getFrame();
    const auto full = getLocalBounds().toFloat();

    if (f.power <= 0.001f)
    {
        g.fillAll (Colour (0xff050605));
        return;
    }

    // picture area (POCKET TV shrinks it, OFF AIR squashes it)
    auto pic = full;
    if (f.pocket > 0.0f) pic = pic.withSizeKeepingCentre (W * (1.0f - 0.38f * f.pocket), H * (1.0f - 0.38f * f.pocket));
    if (f.power < 1.0f)
    {
        const float p = f.power;
        const float sy = p > 0.4f ? jmap (p, 0.4f, 1.0f, 0.012f, 1.0f) : 0.012f;
        const float sx = p > 0.4f ? 1.0f : jmap (p, 0.0f, 0.4f, 0.01f, 1.0f);
        pic = pic.withSizeKeepingCentre (pic.getWidth() * sx, pic.getHeight() * sy);
    }

    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (pic.toNearestInt());
        g.addTransform (AffineTransform::scale (pic.getWidth() / W, pic.getHeight() / H).translated (pic.getX(), pic.getY()));
        paintSoftwareScene (g);
        paintPlayLayer (g);
        if (f.power >= 1.0f && f.pocket < 0.5f) paintOsd (g, true);
        if (f.power < 1.0f)
        {
            g.setColour (Colours::white.withAlpha (jlimit (0.0f, 1.0f, (1.0f - f.power) * 1.4f)));
            g.fillAll();
        }
    }
    if (f.pocket > 0.0f && f.power >= 1.0f)
    {
        g.setColour (Colours::black.withAlpha (0.25f * f.pocket));
        g.fillRect (pic);
    }

    // scanlines + vignette + glass
    g.setColour (Colours::black.withAlpha (0.16f));
    for (float y = 0.0f; y < H; y += 3.0f) g.fillRect (0.0f, y, W, 1.2f);
    ColourGradient vig (Colours::transparentBlack, W * 0.5f, H * 0.5f, Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (vig);
    g.fillAll();
    ColourGradient glass (Colours::white.withAlpha (0.07f), 0.0f, 0.0f, Colours::transparentWhite, W * 0.45f, H * 0.5f, false);
    g.setGradientFill (glass);
    g.fillAll();
}

void TvScreen::paintSoftwareScene (Graphics& g)
{
    const auto f = getFrame();
    const Colour c = f.colour;
    const float t = f.time * (f.calm > 0.5f ? 0.35f : 1.0f);
    const float beat = f.beat;
    const float bf = beat - std::floor (beat); // beat fraction
    const float pulse = std::pow (1.0f - bf, 3.0f) * (1.0f - f.calm * 0.7f);
    const float lv = f.level;

    ColourGradient bg (c.darker (2.6f), 0.0f, 0.0f, c.darker (1.1f).withMultipliedSaturation (0.7f), 0.0f, H, false);
    g.setGradientFill (bg);
    g.fillRect (0.0f, 0.0f, W, H);

    switch (f.channel)
    {
        case 9: // DRIFT – night highway
        {
            const float horizon = H * 0.46f;
            ColourGradient sky (Colour (0xff05060e), 0, 0, Colour (0xff2a1d2c), 0, horizon, false);
            g.setGradientFill (sky); g.fillRect (0.0f, 0.0f, W, horizon);
            // city
            Random r (7);
            for (int i = 0; i < 40; ++i)
            {
                const float bw = 12.0f + r.nextFloat() * 30.0f, bh = 10.0f + r.nextFloat() * 50.0f;
                const float x = r.nextFloat() * W;
                g.setColour (Colour (0xff0b0a10)); g.fillRect (x, horizon - bh, bw, bh);
                g.setColour (Colour (0xffffb54a).withAlpha (0.5f));
                if (r.nextBool()) g.fillRect (x + 3.0f, horizon - bh + 4.0f, 2.0f, 2.0f);
            }
            // road
            Path road;
            road.addTriangle (W * 0.5f, horizon, -W * 0.3f, H, W * 1.3f, H);
            g.setColour (Colour (0xff121014)); g.fillPath (road);
            ColourGradient wet (Colour (0xffff9a3c).withAlpha (0.25f), W * 0.5f, H, Colours::transparentBlack, W * 0.5f, horizon, false);
            g.setGradientFill (wet); g.fillPath (road);
            // lane dashes moving towards the viewer
            for (int i = 0; i < 12; ++i)
            {
                float z = std::fmod ((float) i / 12.0f + t * 0.35f, 1.0f);
                z = z * z;
                const float y = horizon + (H - horizon) * z;
                const float w2 = 2.0f + 10.0f * z, h2 = 3.0f + 26.0f * z;
                for (float lane : { -0.33f, 0.33f })
                {
                    const float x = W * 0.5f + lane * W * 0.9f * z;
                    g.setColour (Colour (0xffffc46b).withAlpha (0.8f));
                    g.fillRect (x - w2 * 0.5f, y, w2, h2);
                }
            }
            // lamps
            for (int i = 0; i < 6; ++i)
            {
                float z = std::fmod ((float) i / 6.0f + t * 0.12f, 1.0f);
                z = z * z;
                const float y = horizon - 10.0f - 180.0f * z;
                for (float side : { -1.0f, 1.0f })
                {
                    const float x = W * 0.5f + side * (40.0f + W * 0.62f * z);
                    const float r2 = 3.0f + 14.0f * z;
                    ColourGradient lamp (Colour (0xffffb347).withAlpha (0.9f), x, y, Colour (0xffffb347).withAlpha (0.0f), x + r2 * 3.0f, y, true);
                    g.setGradientFill (lamp);
                    g.fillEllipse (x - r2 * 3.0f, y - r2 * 3.0f, r2 * 6.0f, r2 * 6.0f);
                }
            }
            break;
        }
        case 2: // SVBTERRA – speaker cone
        {
            const auto ctr = Point<float> (W * 0.5f, H * 0.5f);
            const float breathe = 1.0f + 0.08f * pulse + 0.25f * lv;
            for (int i = 6; i >= 1; --i)
            {
                const float r = (float) i * 34.0f * breathe;
                g.setColour (i % 2 ? c.withAlpha (0.35f) : Colours::black.withAlpha (0.5f));
                g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (ctr));
            }
            g.setColour (c.brighter (0.6f));
            g.fillEllipse (Rectangle<float> (40.0f * breathe, 40.0f * breathe).withCentre (ctr));
            Path wave;
            for (int i = 0; i <= 80; ++i)
            {
                const float x = W * (float) i / 80.0f;
                const float y = H * 0.86f + std::sin ((float) i * 0.25f + t * 6.0f) * (6.0f + 30.0f * lv);
                if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            g.setColour (c.brighter (0.8f).withAlpha (0.8f));
            g.strokePath (wave, PathStrokeType (4.0f));
            break;
        }
        case 3: // DRVM CVLT – pads lighting on the beat
        {
            const int step = ((int) std::floor (beat * 4.0f)) % 16;
            for (int i = 0; i < 16; ++i)
            {
                const auto r = Rectangle<float> (W * 0.18f + (float) (i % 4) * W * 0.16f, H * 0.12f + (float) (i / 4) * H * 0.2f, W * 0.14f, H * 0.17f);
                g.setColour (i == step ? c.brighter (0.4f) : c.darker (1.5f).withAlpha (0.8f));
                g.fillRoundedRectangle (r, 6.0f);
            }
            break;
        }
        case 4: // RAGE ENGINE – strobe
        {
            g.setColour (c.withAlpha (0.2f + 0.8f * pulse));
            g.fillRect (0.0f, 0.0f, W, H);
            g.setColour (Colours::black.withAlpha (0.6f));
            for (int i = 0; i < 8; ++i)
            {
                const float x = std::fmod ((float) i * 140.0f + t * 400.0f, W + 200.0f) - 100.0f;
                Path p; p.addQuadrilateral (x, 0, x + 40, 0, x - 80, H, x - 120, H);
                g.fillPath (p);
            }
            break;
        }
        case 5: // DREAMCORE – clouds
        {
            ColourGradient sky (c.brighter (0.2f), 0, 0, Colour (0xff8fb4ff), 0, H, false);
            g.setGradientFill (sky); g.fillRect (0.0f, 0.0f, W, H);
            Random r (3);
            for (int i = 0; i < 14; ++i)
            {
                const float x = std::fmod (r.nextFloat() * W + t * (8.0f + (float) i), W + 300.0f) - 150.0f;
                const float y = r.nextFloat() * H * 0.9f;
                const float s = 60.0f + r.nextFloat() * 120.0f;
                g.setColour (Colours::white.withAlpha (0.35f));
                g.fillEllipse (x, y, s * 1.8f, s * 0.6f);
                g.fillEllipse (x + s * 0.4f, y - s * 0.25f, s, s * 0.6f);
            }
            break;
        }
        case 6: // VHS – colour bars + rolling noise band
        {
            const Colour bars[] = { Colour (0xffc0c0c0), Colour (0xffc0c000), Colour (0xff00c0c0), Colour (0xff00c000),
                                    Colour (0xffc000c0), Colour (0xffc00000), Colour (0xff0000c0) };
            for (int i = 0; i < 7; ++i) { g.setColour (bars[i].withMultipliedBrightness (0.8f)); g.fillRect (W * (float) i / 7.0f, 0.0f, W / 7.0f + 1.0f, H * 0.72f); }
            g.setColour (Colour (0xff101010)); g.fillRect (0.0f, H * 0.72f, W, H * 0.28f);
            const float band = std::fmod (t * 60.0f, H + 60.0f) - 30.0f;
            Random r ((int64) (t * 30.0f));
            for (int i = 0; i < 60; ++i)
            {
                g.setColour (Colours::white.withAlpha (r.nextFloat() * 0.5f));
                g.fillRect (r.nextFloat() * W, band + r.nextFloat() * 30.0f, 4.0f + r.nextFloat() * 40.0f, 1.5f);
            }
            break;
        }
        case 7: // MVTANT – toxic blobs
        {
            for (int i = 0; i < 6; ++i)
            {
                const float x = W * (0.5f + 0.35f * std::sin (t * (0.3f + (float) i * 0.13f) + (float) i));
                const float y = H * (0.5f + 0.35f * std::cos (t * (0.4f + (float) i * 0.07f) + (float) i * 2.0f));
                const float r = 50.0f + 40.0f * lv + 20.0f * pulse;
                ColourGradient blob (c.withAlpha (0.8f), x, y, c.withAlpha (0.0f), x + r, y, true);
                g.setGradientFill (blob);
                g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
            }
            break;
        }
        case 8: // NO FILTER – raw scope
        {
            g.fillAll (Colours::black);
            Path p;
            Random r ((int64) (t * 24.0f));
            for (int i = 0; i <= 120; ++i)
            {
                const float x = W * (float) i / 120.0f;
                const float y = H * 0.5f + (r.nextFloat() - 0.5f) * (40.0f + 300.0f * lv);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (Colours::white.withAlpha (0.9f));
            g.strokePath (p, PathStrokeType (2.5f));
            break;
        }
        case 10: // HYPER – zooming squares
        {
            for (int i = 0; i < 10; ++i)
            {
                const float z = std::fmod ((float) i / 10.0f + t * 0.25f, 1.0f);
                const float s = z * z * W * 1.4f;
                g.setColour ((i % 2 ? c : Colour (0xffff3ec8)).withAlpha (0.25f + 0.5f * z));
                g.drawRect (Rectangle<float> (s, s * 0.56f).withCentre ({ W * 0.5f, H * 0.5f }), 3.0f + 6.0f * z);
            }
            break;
        }
        case 11: // FINAL BOSS – VU bars
        {
            for (int i = 0; i < 16; ++i)
            {
                const float h = H * (0.15f + 0.6f * lv * (0.6f + 0.4f * std::sin (t * 3.0f + (float) i * 0.9f)));
                ColourGradient bar (c.brighter (0.6f), 0, H - h, c.darker (1.0f), 0, H, false);
                g.setGradientFill (bar);
                g.fillRect (W * 0.08f + (float) i * W * 0.053f, H - h - 30.0f, W * 0.04f, h);
            }
            break;
        }
        case 1: // VOX DEI – rings
        {
            for (int i = 0; i < 8; ++i)
            {
                const float z = std::fmod ((float) i / 8.0f + t * 0.3f, 1.0f);
                const float r = z * W * 0.6f * (1.0f + lv * 0.3f);
                g.setColour (c.withAlpha (0.9f * (1.0f - z)));
                g.drawEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ W * 0.5f, H * 0.5f }), 3.0f);
            }
            break;
        }
        case 0: // MELODIA – melody lines
        default:
        {
            for (int k = 0; k < 4; ++k)
            {
                Path p;
                for (int i = 0; i <= 100; ++i)
                {
                    const float x = W * (float) i / 100.0f;
                    const float y = H * (0.3f + 0.13f * (float) k) + std::sin ((float) i * 0.12f * (1.0f + (float) k * 0.3f) + t * (1.0f + (float) k * 0.4f)) * (20.0f + 40.0f * lv);
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (c.brighter (0.5f).withAlpha (0.8f - 0.15f * (float) k));
                g.strokePath (p, PathStrokeType (3.0f));
            }
            break;
        }
    }

    // macro colouring: VILLAIN ARC darker, KNOCK flash, AURA haze, CRASH OUT glitch
    const float villain = f.macros[0], crash = f.macros[1], aura = f.macros[2];
    if (villain > 0.5f) { g.setColour (Colours::black.withAlpha ((villain - 0.5f) * 1.2f)); g.fillRect (0.0f, 0.0f, W, H); }
    if (aura > 0.5f) { g.setColour (c.brighter (0.8f).withAlpha ((aura - 0.5f) * 0.3f)); g.fillRect (0.0f, 0.0f, W, H); }
    g.setColour (Colours::white.withAlpha (0.35f * f.transient * f.macros[4]));
    g.fillRect (0.0f, 0.0f, W, H);

    const float glitch = std::max (f.glitch, std::max (0.0f, crash - 0.6f) * 0.8f * (1.0f - f.calm));
    if (glitch > 0.01f)
    {
        Random r ((int64) (f.time * 40.0f));
        for (int i = 0; i < (int) (glitch * 14.0f); ++i)
        {
            const float y = r.nextFloat() * H, h = 4.0f + r.nextFloat() * 30.0f;
            g.setColour ((r.nextBool() ? c : Colours::white).withAlpha (0.25f + 0.4f * r.nextFloat()));
            g.fillRect (r.nextFloat() * W * 0.3f - 40.0f, y, W * (0.4f + r.nextFloat()), h);
        }
    }
    if (f.snow > 0.0f)
    {
        Random r ((int64) (f.time * 1000.0f));
        g.setColour (Colour (0xff202020)); g.fillRect (0.0f, 0.0f, W, H);
        for (int i = 0; i < 900; ++i)
        {
            g.setColour (Colours::white.withAlpha (r.nextFloat()));
            g.fillRect (r.nextFloat() * W, r.nextFloat() * H, 4.0f, 3.0f);
        }
    }
    if (f.blue > 0.0f) { g.setColour (Colour (0xff1030c8)); g.fillRect (0.0f, 0.0f, W, H); }
}

void TvScreen::paintPlayLayer (Graphics& g)
{
    const auto f = getFrame();
    if (f.blue > 0.0f || f.snow > 0.0f) return;
    const Colour glow = Colour (0xfffff3d6);
    if (f.path.size() > 1)
    {
        Path p;
        for (size_t i = 0; i < f.path.size(); ++i)
        {
            const auto pt = fromNorm (f.path[i]);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        if (! drawing) p.closeSubPath();
        g.setColour (Colour (0xffffb238).withAlpha (0.35f));
        g.strokePath (p, PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (glow);
        g.strokePath (p, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    if (f.dotVisible)
    {
        const auto pt = fromNorm (f.dot);
        ColourGradient halo (Colours::white, pt.x, pt.y, Colour (0xffffb238).withAlpha (0.0f), pt.x + 34.0f, pt.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (Rectangle<float> (68.0f, 68.0f).withCentre (pt));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (pt));
    }
    if (f.logoVisible)
    {
        const float lw = 230.0f, lh = 64.0f;
        const Point<float> c (lw * 0.5f + f.logo.x * (W - lw), lh * 0.5f + (1.0f - f.logo.y) * (H - lh));
        auto r = Rectangle<float> (lw, lh).withCentre (c);
        g.setFont (theme::stencilFont (30.0f).italicised());
        g.setColour (Colour (0xffd8d8d8));
        g.drawText ("EFFECTOR", r.withHeight (lh * 0.5f), Justification::centred);
        g.setFont (theme::stencilFont (38.0f).italicised());
        g.setColour (theme::killRed);
        g.drawText ("KILLA", r.withTrimmedTop (lh * 0.42f), Justification::centred);
    }
}

void TvScreen::paintOsd (Graphics& g, bool)
{
    const auto& meta = proc.getMeta();
    const int ch = proc.getCurrentChannel();
    const auto& chans = proc.bank.getChannels();
    const String chName = ch < (int) chans.size() ? chans[(size_t) ch].name : String();
    const double kt = clock - killStart;

    if (kt >= 0.3 && kt < 0.55)
    {
        osdText (g, "NO SIGNAL", { 0.0f, H * 0.5f - 30.0f, W, 60.0f }, 46.0f, Justification::centred);
        return;
    }

    // corners
    {
        Path play;
        const auto r = modeLabel();
        play.addTriangle (r.getX() + 8.0f, r.getY() + 7.0f, r.getX() + 8.0f, r.getBottom() - 7.0f, r.getX() + 28.0f, r.getCentreY());
        g.setColour (theme::osdWhite);
        g.fillPath (play);
        osdText (g, avName ((int) meta.mod.mode), r.withTrimmedLeft (46.0f), 30.0f, Justification::centredLeft);
    }
    osdText (g, "CH " + String (ch + 1).paddedLeft ('0', 2) + " " + chName, { W - 440.0f, 18.0f, 400.0f, 36.0f }, 30.0f, Justification::centredRight);
    osdText (g, sourceNames[jlimit (0, 5, (int) meta.source)] + String (" SOURCE"), sourceLabel(), 17.0f, Justification::centredRight,
             theme::osdWhite.withAlpha (0.75f));
    if (clock < overloadUntil)
        osdText (g, "OVERLOAD", { W - 250.0f, 84.0f, 210.0f, 26.0f }, 22.0f, Justification::centredRight, theme::killRed);

    const bool detail = detailSlot >= 0 && proc.premiumCable;
    if (detail)
    {
        paintDetailPanel (g);
    }
    else
    {
        // knob OSD bar
        if (clock < valueUntil)
        {
            const auto r = Rectangle<float> (40.0f, 398.0f, 360.0f, 30.0f);
            osdText (g, valueName.toUpperCase(), r, 24.0f, Justification::centredLeft);
            g.setFont (theme::osdFont (24.0f));
            const float nameW = GlyphArrangement::getStringWidth (g.getCurrentFont(), valueName.toUpperCase() + " ");
            drawBlocks (g, { r.getX() + nameW, r.getY() + 5.0f, 132.0f, 20.0f }, valueNorm, 8, theme::osdWhite);
            osdText (g, valueText, { r.getX() + nameW + 142.0f, r.getY(), 140.0f, 30.0f }, 24.0f, Justification::centredLeft);
        }
        // axis labels (click to change the target)
        {
            osdText (g, "X " + targetName (meta.mod.targetX), axisLabel (0), 16.0f, Justification::centredLeft, theme::osdWhite.withAlpha (0.7f));
            osdText (g, "Y " + targetName (meta.mod.targetY), axisLabel (1), 16.0f, Justification::centredLeft, theme::osdWhite.withAlpha (0.7f));
        }
        // loop length ruler (AV2) / crossing speed (AV3)
        if (meta.mod.mode != AvMode::Remote)
        {
            const auto r = barSelector();
            g.setColour (theme::osdWhite.withAlpha (0.8f));
            const float y = r.getY() + 18.0f;
            g.drawLine (r.getX(), y, r.getRight(), y, 2.0f);
            g.drawLine (r.getX(), y - 12.0f, r.getX(), y + 12.0f, 3.0f);
            g.drawLine (r.getRight(), y - 12.0f, r.getRight(), y + 12.0f, 3.0f);
            const int ticks = 16;
            for (int i = 1; i < ticks; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) ticks;
                g.drawLine (x, y - (i % 4 == 0 ? 9.0f : 5.0f), x, y + (i % 4 == 0 ? 9.0f : 0.0f), 1.5f);
            }
            String label;
            if (meta.mod.mode == AvMode::Scribble) label = String (meta.mod.scribbleBars) + (meta.mod.scribbleBars == 1 ? " BAR" : " BARS");
            else
            {
                const float b = screensaverBarsForIndex (meta.mod.screensaverSpeed);
                label = (b < 1.0f ? "1/" + String (roundToInt (1.0f / b)) : String (roundToInt (b))) + (b > 1.0f ? " BARS" : " BAR");
            }
            osdText (g, label, { r.getX(), y + 12.0f, r.getWidth(), 24.0f }, 18.0f, Justification::centred);
        }
    }

    if (clock < messageUntil)
        osdText (g, message, { 0.0f, 120.0f, W, 44.0f }, 34.0f, Justification::centred, theme::amber);

    if (clock < nowPlayingUntil && kt >= 0.7)
    {
        auto card = Rectangle<float> (W * 0.5f - 210.0f, 80.0f, 420.0f, 300.0f);
        g.setColour (Colours::black.withAlpha (0.72f));
        g.fillRoundedRectangle (card, 6.0f);
        g.setColour (theme::osdWhite.withAlpha (0.8f));
        g.drawRoundedRectangle (card, 6.0f, 2.0f);
        osdText (g, "NOW PLAYING", card.withHeight (44.0f), 26.0f, Justification::centred, theme::amber);
        int row = 0;
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto st = proc.getSlotMeta (s);
            if (st.isEmpty()) continue;
            osdText (g, String (s + 1) + "  " + effectInfo (st.type).name.toUpperCase(),
                     { card.getX() + 40.0f, card.getY() + 50.0f + (float) row * 30.0f, card.getWidth() - 60.0f, 28.0f }, 20.0f, Justification::centredLeft);
            ++row;
        }
        osdText (g, meta.name.toUpperCase(), { card.getX(), card.getBottom() - 36.0f, card.getWidth(), 28.0f }, 18.0f, Justification::centred,
                 theme::osdWhite.withAlpha (0.7f));
    }
}

void TvScreen::paintDetailPanel (Graphics& g)
{
    const auto a = detailPanelArea();
    const auto st = proc.getSlotState (detailSlot);
    const auto& info = effectInfo (st.type);
    g.setColour (Colours::black.withAlpha (0.74f));
    g.fillRoundedRectangle (a, 8.0f);
    g.setColour (info.colour.withAlpha (0.9f));
    g.drawRoundedRectangle (a, 8.0f, 2.0f);
    osdText (g, String (detailSlot + 1) + " " + info.name.toUpperCase(), { a.getX() + 18.0f, a.getY() + 10.0f, 360.0f, 30.0f }, 24.0f,
             Justification::centredLeft, info.colour.brighter (0.6f));

    const String toggles[4] = { "PAUSE", "SCREEN", st.ms == MSMode::Stereo ? "ST" : (st.ms == MSMode::Mid ? "MID" : "SIDE"), "LOCK" };
    const bool on[4] = { st.pause, proc.getScreening() == detailSlot, st.ms != MSMode::Stereo, st.writeProtect };
    for (int i = 0; i < 4; ++i)
    {
        const auto r = detailToggle (i);
        g.setColour (on[i] ? (i == 3 ? theme::killRed : theme::amber) : theme::osdWhite.withAlpha (0.12f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme::osdWhite.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
        g.setColour (on[i] ? Colours::black : theme::osdWhite);
        g.setFont (theme::osdFont (16.0f));
        g.drawText (toggles[i], r, Justification::centred);
    }

    const auto visible = detailParams();
    for (size_t vi = 0; vi < visible.size(); ++vi)
    {
        const int c = visible[vi];
        const auto r = detailCell ((int) vi);
        const String name = c == 8 ? String ("SLOT MIX") : info.params[(size_t) c].name;
        const float v = getDetailValue (c);
        const bool hot = hoverCell == c || dragCell == c;
        osdText (g, name, r.withHeight (20.0f), 16.0f, Justification::centredLeft, hot ? theme::amber : theme::osdWhite);
        drawBlocks (g, { r.getX(), r.getY() + 24.0f, r.getWidth() * 0.62f, 18.0f }, v, 10, hot ? theme::amber : theme::osdWhite);
        String valueStr;
        if (c == 8) valueStr = String (roundToInt (v * 100.0f)) + " %";
        else { const auto& spec = info.params[(size_t) c]; valueStr = spec.format (spec.toReal (v)); }
        if (hot || info.params[(size_t) jlimit (0, 7, c)].isDiscrete())
            osdText (g, valueStr, { r.getX() + r.getWidth() * 0.64f, r.getY() + 20.0f, r.getWidth() * 0.36f + 10.0f, 26.0f }, 15.0f,
                     Justification::centredLeft, hot ? theme::amber : theme::osdWhite.withAlpha (0.8f));
    }
}

bool TvScreen::renderOsdIfDirty (Image& target)
{
    if (! osdDirty) return false;
    osdDirty = false;
    if (target.isNull() || target.getWidth() != (int) W * 2 || target.getHeight() != (int) H * 2)
        target = Image (Image::ARGB, (int) W * 2, (int) H * 2, true);
    else
        target.clear (target.getBounds());
    Graphics g (target);
    g.addTransform (AffineTransform::scale (2.0f));
    paintOsd (g, true);
    return true;
}

// ============================================================================
// mouse
// ============================================================================
void TvScreen::mouseMove (const MouseEvent& e)
{
    const int c = detailSlot >= 0 && proc.premiumCable ? detailCellAt (e.position) : -1;
    if (c != hoverCell) { hoverCell = c; osdDirty = true; }
}

void TvScreen::mouseExit (const MouseEvent&)
{
    if (hoverCell != -1) { hoverCell = -1; osdDirty = true; }
}

void TvScreen::mouseDown (const MouseEvent& e)
{
    const auto pos = e.position;
    auto mod = proc.getMeta().mod;

    // detail panel
    if (detailSlot >= 0 && proc.premiumCable)
    {
        if (detailPanelArea().contains (pos))
        {
            for (int i = 0; i < 4; ++i)
                if (detailToggle (i).contains (pos))
                {
                    const auto st = proc.getSlotState (detailSlot);
                    switch (i)
                    {
                        case 0: proc.setSlotParam (detailSlot, 9, st.pause ? 0.0f : 1.0f); break;
                        case 1: proc.setScreening (proc.getScreening() == detailSlot ? -1 : detailSlot); break;
                        case 2: proc.setSlotMs (detailSlot, (MSMode) (((int) st.ms + 1) % 3)); break;
                        case 3: proc.setWriteProtect (detailSlot, ! st.writeProtect); break;
                        default: break;
                    }
                    osdDirty = true;
                    return;
                }
            dragCell = detailCellAt (pos);
            if (dragCell >= 0)
            {
                proc.pushUndo();
                dragStartValue = getDetailValue (dragCell);
                const auto id = dragCell == 8 ? ParamIDs::slotMix (detailSlot) : ParamIDs::slotParam (detailSlot, dragCell);
                if (auto* p = proc.getParam (id)) p->beginChangeGesture();
            }
            return;
        }
        // click outside the panel closes it
        proc.selectedSlot = -1;
        setDetailSlot (-1);
        if (onDetailClosed) onDetailClosed();
        return;
    }

    // OSD hot spots
    if (axisLabel (0).contains (pos)) { showTargetMenu (0); return; }
    if (axisLabel (1).contains (pos)) { showTargetMenu (1); return; }
    if (sourceLabel().contains (pos)) { showSourceMenu(); return; }
    if (mod.mode != AvMode::Remote && barSelector().contains (pos))
    {
        if (mod.mode == AvMode::Scribble) mod.scribbleBars = mod.scribbleBars == 1 ? 2 : (mod.scribbleBars == 2 ? 4 : 1);
        else mod.screensaverSpeed = (mod.screensaverSpeed + 1) % 5;
        proc.setModState (mod, false);
        return;
    }

    switch (mod.mode)
    {
        case AvMode::Remote:
        {
            remoteDragging = true;
            const auto n = toNorm (pos);
            setTargetValue (mod.targetX, n.x, true, false);
            setTargetValue (mod.targetY, n.y, true, false);
            break;
        }
        case AvMode::Scribble:
            if (e.mods.isPopupMenu())
            {
                mod.path.clear();
                proc.setModState (mod, true);
                showMessage ("ERASED", 0.8);
                return;
            }
            drawing = true;
            stroke.clear();
            stroke.push_back (toNorm (pos));
            break;
        case AvMode::Screensaver:
        default:
            break;
    }
}

void TvScreen::mouseDrag (const MouseEvent& e)
{
    if (dragCell >= 0)
    {
        const float range = detailRectFor (dragCell).getWidth() * (e.mods.isShiftDown() ? 6.0f : 1.0f);
        setDetailValue (dragCell, jlimit (0.0f, 1.0f, dragStartValue + (float) e.getDistanceFromDragStartX() / range), true);
        return;
    }
    const auto mod = proc.getMeta().mod;
    if (remoteDragging)
    {
        const auto n = toNorm (e.position);
        setTargetValue (mod.targetX, n.x, false, false);
        setTargetValue (mod.targetY, n.y, false, false);
        const float xv = n.x;
        showValue (targetName (mod.targetX), xv, String (roundToInt (xv * 100.0f)));
    }
    else if (drawing)
    {
        const auto n = toNorm (e.position);
        if (stroke.empty() || stroke.back().getDistanceFrom (n) > 0.006f) stroke.push_back (n);
    }
}

void TvScreen::mouseUp (const MouseEvent&)
{
    if (dragCell >= 0)
    {
        const auto id = dragCell == 8 ? ParamIDs::slotMix (detailSlot) : ParamIDs::slotParam (detailSlot, dragCell);
        if (auto* p = proc.getParam (id)) p->endChangeGesture();
        dragCell = -1;
        osdDirty = true;
        return;
    }
    auto mod = proc.getMeta().mod;
    if (remoteDragging)
    {
        remoteDragging = false;
        setTargetValue (mod.targetX, getTargetValue (mod.targetX), false, true);
        setTargetValue (mod.targetY, getTargetValue (mod.targetY), false, true);
    }
    if (drawing)
    {
        drawing = false;
        if (stroke.size() > 3)
        {
            mod.path = stroke;           // a new drawing replaces the old one
            proc.setModState (mod, true);
        }
        stroke.clear();
    }
}

void TvScreen::mouseDoubleClick (const MouseEvent& e)
{
    if (dragCell >= 0 || (detailSlot >= 0 && proc.premiumCable && detailPanelArea().contains (e.position)))
    {
        const int c = detailCellAt (e.position);
        if (c >= 0)
        {
            const float def = c == 8 ? 1.0f : effectInfo (proc.getSlotType (detailSlot)).params[(size_t) c].defaultNorm();
            setDetailValue (c, def, false);
        }
        return;
    }
    const auto mod = proc.getMeta().mod;
    if (mod.mode == AvMode::Remote)
    {
        // back to the defaults (0.5 for macros, spec default for slot params)
        for (int t : { mod.targetX, mod.targetY })
        {
            float def = 0.5f;
            if (ModTarget::isSlotParam (t))
            {
                const int s = ModTarget::slotOf (t), k = ModTarget::paramOf (t);
                def = k == 8 ? 1.0f : effectInfo (proc.getSlotType (s)).params[(size_t) jlimit (0, 7, k)].defaultNorm();
            }
            setTargetValue (t, def, true, true);
        }
    }
}

void TvScreen::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (detailSlot < 0 || ! proc.premiumCable) return;
    const int c = detailCellAt (e.position);
    if (c < 0) return;
    float step = e.mods.isShiftDown() ? 0.005f : 0.02f;
    if (c < 8)
    {
        const auto& spec = effectInfo (proc.getSlotType (detailSlot)).params[(size_t) c];
        if (spec.isDiscrete() && spec.numSteps() > 1) step = 1.0f / (float) (spec.numSteps() - 1);
    }
    const float dir = (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f);
    setDetailValue (c, jlimit (0.0f, 1.0f, getDetailValue (c) + step * dir), false);
}

bool TvScreen::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey && detailSlot >= 0)
    {
        proc.selectedSlot = -1;
        setDetailSlot (-1);
        if (onDetailClosed) onDetailClosed();
        return true;
    }
    return false;
}

void TvScreen::showTargetMenu (int axis)
{
    PopupMenu m;
    auto mod = proc.getMeta().mod;
    const int current = axis == 0 ? mod.targetX : mod.targetY;
    m.addSectionHeader (axis == 0 ? "X TARGET" : "Y TARGET");
    for (int i = 0; i < kNumMacros; ++i) m.addItem (1 + i, MacroEngine::macroName (i), true, current == i);
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto t = proc.getSlotType (s);
        if (t == EffectType::None) continue;
        PopupMenu sub;
        const auto& info = effectInfo (t);
        for (int k = 0; k < kNumParams; ++k)
            if (info.params[(size_t) k].used && ! info.params[(size_t) k].isDiscrete())
                sub.addItem (1000 + ModTarget::slotParam (s, k), info.params[(size_t) k].name, true, current == ModTarget::slotParam (s, k));
        sub.addItem (1000 + ModTarget::slotParam (s, 8), "SLOT MIX", true, current == ModTarget::slotParam (s, 8));
        m.addSubMenu (String (s + 1) + " " + info.name.toUpperCase(), sub);
    }
    SafePointer<TvScreen> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe, axis] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto mod2 = safe->proc.getMeta().mod;
        const int target = r >= 1000 ? r - 1000 : r - 1;
        (axis == 0 ? mod2.targetX : mod2.targetY) = target;
        safe->proc.setModState (mod2, true);
    });
}

void TvScreen::showSourceMenu()
{
    PopupMenu m;
    m.addSectionHeader ("SOURCE (KILL rules, macro ranges)");
    for (int i = 0; i < 6; ++i) m.addItem (1 + i, sourceNames[i], true, (int) proc.getMeta().source == i);
    SafePointer<TvScreen> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe != nullptr && r > 0) safe->proc.setSource ((Source) (r - 1));
    });
}

} // namespace ek::ui
