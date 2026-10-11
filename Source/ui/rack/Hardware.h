#pragma once

// Shared shell kit of the TrapWin look (Docs/TRAPWIN-STYLE.md): palette, "reduce motion", the
// cached-background helper, pill drawing, flat knob, vector icons and two small reusable
// components (HwButton = pill button, HwKnob = flat TrapWin knob).
// The old hardware materials (walnut, rack ears, screws, rails, brushed metal, chicken-head knobs,
// 7-segment LCDs) are gone on purpose - the plugin must not look like a vintage rack.

#include "../Controls.h"

namespace ek::ui::hw
{
// ---- palette (trapwin.com tokens) ------------------------------------------------------------
namespace col
{
    inline const juce::Colour ice { 0xff9fc3ff };        // accent-light: lines, active pills, glow
    inline const juce::Colour iceDeep { 0xff4676c4 };    // glow rgba(70,118,196,.5)
    inline const juce::Colour accent { 0xff7fa6e8 };
    inline const juce::Colour vfd { 0xff9fdcff };        // kept for older callers (= ice text)
    inline const juce::Colour amber { 0xffffc861 };
    inline const juce::Colour green { 0xff3ddc97 };
    inline const juce::Colour red { 0xffff2e3e };       // KILL / BYPASS / OVER only
    inline const juce::Colour bg { 0xff04060a };         // window background
    inline const juce::Colour shaft { 0xff04060a };      // (old name of the background)
    inline const juce::Colour card { 0xff0a0f18 };
    inline const juce::Colour card2 { 0xff0b111c };
    inline const juce::Colour screen { 0xff05080f };
    inline const juce::Colour line { 0x2e9fc3ff };       // rgba(159,195,255,.18)
    inline const juce::Colour print { 0xffe6ecf7 };      // text
    inline const juce::Colour text2 { 0xffa9b6cc };
    inline const juce::Colour printDim { 0xff8794ab };   // muted
}

// ---- motion ----------------------------------------------------------------------------------
// "Reduce motion" (settings menu). A per-user preference, persisted next to the user presets.
// Animated elements (scan effects, pulsing, glows ...) should check it.
bool reduceMotion();
void setReduceMotion (bool on);
double animSeconds();   // monotonically increasing animation clock (seconds)

// ---- fonts -------------------------------------------------------------------------------------
juce::Font mono (float height, bool bold = false);   // IBM Plex Mono (+1 px tracking)
juce::Font display (float height);                   // Archivo Black

// ---- surfaces ----------------------------------------------------------------------------------
// TrapWin card (radius 20 / 16, 1 px ice line, inner sheen). Returns the content area.
// (Kept under its old name for older callers; there are no rack ears any more.)
juce::Rectangle<float> drawRackPanel (juce::Graphics&, juce::Rectangle<float> r, juce::Colour base = col::card,
                                      float earWidth = 0.0f, int seed = 1);
void drawCard (juce::Graphics&, juce::Rectangle<float> r, float corner = 20.0f, float glow = 0.0f);
// Section label: Plex Mono muted caps + a hairline that fades out to the right.
void drawSectionHeader (juce::Graphics&, juce::Rectangle<float> r, const juce::String& label);
// Small printed label (Plex Mono).
void drawPrint (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
                float size = 9.0f, juce::Colour c = col::printDim);
// Soft glow (stacked translucent rounded rects, cheap) around r.
void drawGlow (juce::Graphics&, juce::Rectangle<float> r, float corner, juce::Colour c, float amount, float spread = 10.0f);

// ---- pills (buttons) ---------------------------------------------------------------------------
enum class PillStyle { Outline, Chrome };
// Pill: outline (secondary) or chrome (primary, dark text). `lit` = active: tinted fill in `accent`,
// accent border and soft glow. `dot` draws a small status dot on the left.
void drawPill (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text, juce::Colour accent, bool lit,
               bool pressed, bool hover, PillStyle style = PillStyle::Outline, bool dot = false, float fontSize = 0.0f);

// ---- knobs -------------------------------------------------------------------------------------
// Flat TrapWin knob (dark disc, thin line, accent value arc with glow, white pointer dot).
// `shown` drives the arc + pointer; `base` >= 0 adds a small white tick on the ring (unmodulated value).
void drawFlatKnob (juce::Graphics&, juce::Point<float> c, float r, float shown, juce::Colour accent,
                   float glow = 1.0f, float base = -1.0f, bool hot = false);

// ---- icons (stroked in colour c, fitted into r) ----------------------------------------------
namespace icon
{
    void prev (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void next (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void save (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void revert (juce::Graphics&, juce::Rectangle<float>, juce::Colour);  // circular arrow
    void star (juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool filled);   // TrapWin sparkle
    void undo (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void redo (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void cog (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void info (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void power (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void lock (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void library (juce::Graphics&, juce::Rectangle<float>, juce::Colour);   // sidebar panel
    void chevronDown (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void letter (juce::Graphics&, juce::Rectangle<float>, juce::Colour, const juce::String& text);
}

// ---- cached background -------------------------------------------------------------------------
// Renders an expensive static background once per size / scale and blits it afterwards.
class BackgroundCache
{
public:
    void draw (juce::Graphics& g, juce::Rectangle<int> area, const std::function<void (juce::Graphics&)>& painter);
    void invalidate() { image = {}; }

private:
    juce::Image image;
    juce::Rectangle<int> cachedArea;
    float cachedScale = 0.0f;
};

// ---- components ------------------------------------------------------------------------------
// Pill button. Kinds (names kept from the rack era):
//   Led   - outline pill with a status dot (HOST, FREE, AUTO GAIN ...)
//   Icon  - round icon pill (header)
//   Text  - outline pill with a label (A / B, BYPASS)
//   Round - TAP pill (flashes)
//   Chrome- primary chrome pill with dark text
class HwButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { Led, Icon, Text, Round, Chrome };
    HwButton (Kind k, juce::String label = {}, juce::Colour led = col::ice);

    std::function<void()> onClick;
    std::function<void (bool down)> onPress;
    std::function<bool()> isLit, isEnabledFn;
    std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)> drawIcon;
    bool blinkWhenLit = false;   // pulses (smoothly) while lit, unless reduce motion is on
    float fontSize = 0.0f;       // 0 = automatic
    void setLabel (const juce::String& l) { if (l != label) { label = l; repaint(); } }
    void setLedColour (juce::Colour c) { led = c; repaint(); }
    void flash() { flashUntil = juce::Time::getMillisecondCounter() + 140; repaintGlow(); }
    void tick();
    // The glow of a lit / hovered pill spills outside the bounds (painting is unclipped), so changes
    // repaint the parent area around the button.
    void repaintGlow();
    static constexpr int glowMargin = 10;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaintGlow(); }
    void mouseExit (const juce::MouseEvent&) override { repaintGlow(); }

private:
    bool litNow() const;
    float pulse() const;
    Kind kind;
    juce::String label;
    juce::Colour led;
    bool down = false, lastLit = false, lastEnabled = true;
    float lastPulse = -1.0f;
    juce::uint32 flashUntil = 0;
};

// Flat TrapWin knob (same behaviour as Knob: drag, Shift = fine, wheel, double-click = default).
class HwKnob : public Knob
{
public:
    HwKnob() : Knob (Knob::Style::Macro) {}
    juce::Colour ring { col::ice };   // value arc colour
    float ringGlow = 0.0f;            // 0..1 extra glow of the arc (RECYCLE warning)
    float radiusFactor = 0.62f;       // disc radius relative to half the component size
    bool showArc = true;
    void paint (juce::Graphics&) override;
};
} // namespace ek::ui::hw
