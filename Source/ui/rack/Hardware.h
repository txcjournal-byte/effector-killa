#pragma once

// Shared "hardware" drawing kit for the rack look (DESIGN-RACK.md): brushed metal, chrome
// cross-head screws, rack ears with oval holes, rack rails, procedural walnut, self-drawn
// 7-segment digits, LCD glass, square LED buttons, chicken-head knobs and vector icons.
// Everything here is pure drawing (plus two small reusable components: HwButton, HwKnob),
// so the faceplates, the gear thumbnails and the window frame all share one look.

#include "../Controls.h"

namespace ek::ui::hw
{
// ---- palette (DESIGN-RACK.md) ---------------------------------------------------------------
namespace col
{
    inline const juce::Colour ice { 0xff9fc3ff };        // ice-blue lines / LEDs
    inline const juce::Colour vfd { 0xff9fdcff };        // VFD text on LCD glass
    inline const juce::Colour seg { 0xff7fd0ff };        // 7-segment lit
    inline const juce::Colour segOff { 0xff0d2230 };     // 7-segment unlit
    inline const juce::Colour amber { 0xffffc861 };
    inline const juce::Colour green { 0xff3dff6e };
    inline const juce::Colour red { 0xffff2e3e };
    inline const juce::Colour shaft { 0xff07090d };      // dark rack shaft
    inline const juce::Colour walnutDark { 0xff3b2414 };
    inline const juce::Colour walnutLight { 0xff5a3a22 };
    inline const juce::Colour blackAlu { 0xff16181d };   // control unit
    inline const juce::Colour toolbarTop { 0xff1b1d22 };
    inline const juce::Colour toolbarBottom { 0xff121317 };
    inline const juce::Colour print { 0xffd9dee8 };      // white panel print
    inline const juce::Colour printDim { 0xff8d96a6 };
}

// ---- motion ----------------------------------------------------------------------------------
// "Reduce motion" (settings menu). A per-user preference, persisted next to the user presets.
// Animated elements (scan effects, blinking, spinning reels ...) should check it.
bool reduceMotion();
void setReduceMotion (bool on);
double animSeconds();   // monotonically increasing animation clock (seconds)

// ---- materials -------------------------------------------------------------------------------
// Fine horizontal brushing over a base colour (deterministic per seed).
void drawBrushedMetal (juce::Graphics&, juce::Rectangle<float> r, juce::Colour base, float corner = 0.0f, int seed = 1);
// Panel edge: light top / left bevel, dark bottom / right, thin black outline.
void drawBevel (juce::Graphics&, juce::Rectangle<float> r, float corner = 0.0f, float strength = 1.0f);
// Procedural walnut (sinusoidal grain + noise + pores). vertical = grain runs top to bottom.
void drawWalnut (juce::Graphics&, juce::Rectangle<float> r, bool vertical, int seed = 7);
// Chrome cross-head (Phillips) screw. angle rotates the cross.
void drawScrew (juce::Graphics&, juce::Point<float> c, float radius, float angle = 0.35f);
// Rack ear: a strip of the panel material with two oval holes (screws sit in them).
void drawRackEar (juce::Graphics&, juce::Rectangle<float> ear, juce::Colour base, bool withScrews = true, int seed = 3);
// Complete rack panel: brushed body, ears left / right (earWidth), bevel. Returns the inner area
// between the ears.
juce::Rectangle<float> drawRackPanel (juce::Graphics&, juce::Rectangle<float> r, juce::Colour base, float earWidth = 22.0f, int seed = 1);
// Vertical rack rail with the classic square-ish holes every `pitch` px.
void drawRackRail (juce::Graphics&, juce::Rectangle<float> rail, float pitch = 14.6f);

// ---- displays --------------------------------------------------------------------------------
void drawLcdGlass (juce::Graphics&, juce::Rectangle<float> r, float corner = 3.0f);
// Glass reflection on top of LCD content (call after drawing the text).
void drawGlassReflection (juce::Graphics&, juce::Rectangle<float> r, float corner = 3.0f);
// Self-drawn 7-segment text: digits, '-', ' ', and A b C d E F H L n o P r S t U. A '.' lights the
// decimal point of the previous digit. Unlit segments are drawn in `off`. Right-aligned in r when
// `cells` > the number of characters.
void drawSevenSegment (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text, juce::Colour on,
                       juce::Colour off, int cells = 0, float glow = 1.0f);
// Section header: light strip, ice-blue 1 px line under it, label in IBM Plex Mono.
void drawSectionHeader (juce::Graphics&, juce::Rectangle<float> r, const juce::String& label);
// Printed (silk-screen) label on a panel.
void drawPrint (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r, juce::Justification j,
                float size = 9.0f, juce::Colour c = col::printDim);

// ---- buttons ---------------------------------------------------------------------------------
// Square dark-metal cap (toolbar style), pressed = sunk.
void drawMetalCap (juce::Graphics&, juce::Rectangle<float> r, bool pressed, bool hover, float corner = 3.0f);
// Square LED button: metal cap, LED lens on the left (or on top when the cap is roughly square),
// label printed on the cap.
void drawLedButton (juce::Graphics&, juce::Rectangle<float> r, const juce::String& label, juce::Colour led,
                    bool lit, bool pressed, bool hover);
// Small rectangular LED lens (on / off).
void drawLedLens (juce::Graphics&, juce::Rectangle<float> r, juce::Colour led, bool lit, float glow = 1.0f);
// Round metal push button (TAP).
void drawRoundMetalButton (juce::Graphics&, juce::Point<float> c, float radius, bool pressed, bool hover);

// ---- knobs -----------------------------------------------------------------------------------
// Dark chicken-head knob with a chrome centre cap. angle: 0 = up, clockwise (theme::knobStart..End).
void drawChickenHead (juce::Graphics&, juce::Point<float> c, float radius, float angle, bool hot = false);
// Printed scale around a knob (ticks every 1/(ticks-1) of the travel).
void drawKnobScale (juce::Graphics&, juce::Point<float> c, float radius, int ticks, juce::Colour colour);
// Value arc around a knob (from..to as 0..1 travel), optional glow.
void drawValueArc (juce::Graphics&, juce::Point<float> c, float radius, float from, float to, juce::Colour colour,
                   float thickness, float glow = 0.0f);

// ---- icons (stroked in colour c, fitted into r) ----------------------------------------------
namespace icon
{
    void prev (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void next (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void save (juce::Graphics&, juce::Rectangle<float>, juce::Colour);    // floppy
    void revert (juce::Graphics&, juce::Rectangle<float>, juce::Colour);  // circular arrow
    void star (juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool filled);
    void undo (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void redo (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void cog (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void info (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void power (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
    void lock (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
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
// Hardware push / toggle button. Kinds:
//   Led   - LED lens + printed label (HOST, FREE, SMALL, SHOW GEAR ...)
//   Icon  - square metal cap with a vector icon (toolbar)
//   Text  - metal cap with a label and an LED edge when lit (BYPASS, A / B)
//   Round - round metal push button (TAP), lights its ring when flashed
class HwButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { Led, Icon, Text, Round };
    HwButton (Kind k, juce::String label = {}, juce::Colour led = col::ice);

    std::function<void()> onClick;
    std::function<void (bool down)> onPress;
    std::function<bool()> isLit, isEnabledFn;
    std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)> drawIcon;
    bool blinkWhenLit = false;
    void setLabel (const juce::String& l) { if (l != label) { label = l; repaint(); } }
    void setLedColour (juce::Colour c) { led = c; repaint(); }
    void flash() { flashUntil = juce::Time::getMillisecondCounter() + 110; repaint(); }
    void tick();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool litNow() const;
    Kind kind;
    juce::String label;
    juce::Colour led;
    bool down = false, lastLit = false, lastEnabled = true;
    juce::uint32 flashUntil = 0;
};

// Chicken-head knob (same behaviour as Knob: drag, Shift = fine, wheel, double-click = default).
class HwKnob : public Knob
{
public:
    HwKnob() : Knob (Knob::Style::Macro) {}
    juce::Colour ring { col::ice };   // value arc colour
    float ringGlow = 0.0f;            // 0..1 extra glow of the arc (RECYCLE warning)
    bool showArc = true;
    void paint (juce::Graphics&) override;
};
} // namespace ek::ui::hw
