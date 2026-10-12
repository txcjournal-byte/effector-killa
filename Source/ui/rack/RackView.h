#pragma once

// The CHAIN view: TrapWin effect cards stacked top to bottom (signal flows downwards), DESIGN-RACK.md
// sections 6, 8, 9 with the look of Docs/TRAPWIN-STYLE.md.
//
// Geometry: a 26 px gutter on the left carries the glowing ice-blue SIGNAL LINE (a node per card, a pulse
// travelling down while audio is present - static when reduce motion is on). Cards fill the rest of the
// width (any width >= ~630; 752 in the 1000 x 680 editor -> 724 x 120 cards; SPLIT columns ~349 wide
// use the compact variant): faceplate painted by
// ek::faceplate::paintFaceplate into the whole card (no side strip), real controls on top inside
// faceplate::knobArea(): every used parameter + DRY/WET, label above, value below; switch parameters as
// segmented pills / pill toggles. Top-right row (from faceplate::presetLcdArea()):
//   [ST|MID|SIDE] [ preset name v ] (gear) (POWER) (SOLO) (LOCK) (x)
//   gear = unit menu (programmes, my unit presets, save, copy / paste, replace, reset, listen up to here,
//   SPLIT items, remove), POWER (off keeps tails running; hold = LEVEL-MATCHED POWER), SOLO (other cards
//   dim), LOCK, x = remove.
// SPLIT: the line forks under the SplitUnit strip into two columns A | B (compact half-width cards:
// title + live screen on top, pill row, knobs in one or two rows) and merges again (MERGE node).
//
// Drag & drop:
//   FX chip    -> top / bottom edge of a card = insert there (glowing ice-blue line)
//              -> centre of a card            = replace it (accent outline + REPLACE pill)
//              -> the dashed "DROP EFFECTS HERE" card under the chain = add at the end
//   card       -> drag anywhere outside a control: the card lifts (shadow + glow), a gap opens where it
//                 will land, the other cards glide out of the way
//              -> outside the chain: red "DROP ON THE FX LIBRARY TO REMOVE" hint in the gap
//              -> back onto the FX library    = remove
//
// ------------------------------------------------------------------------------------------------
// KNOB HOOKS (for modulation drag & drop and macro learn, DESIGN-RACK.md section 12)
//   Every control knows (slot, param): param 0..7 = P1..P8, param 8 = unit DRY/WET.
//   * onKnobRightClick (slot, param, screenPos)  - set it to replace the default right-click menu
//     ("Reset to default") of every control, e.g. with "Assign to VILLAIN ARC ..." / modulation list.
//   * paintKnobOverlay (g, faceBounds, slot, param) - called at the end of every control's paint();
//     faceBounds is the knob face (or LED block) in the control's own coordinates. Draw modulation
//     rings / learn highlights here. Call repaintKnobs() when what you draw changes.
//   * knobAt (screenPos) -> KnobHit {slot, param, bounds-in-RackView}; slot = -1 when there is no
//     control under the point (screen coordinates, so any component - e.g. a MOD source being dragged -
//     can hit-test without converting). knobAtLocal() does the same for RackView coordinates.
//   * getKnobBounds (slot, param) - bounds of a control in RackView coordinates (empty if not shown).
// ------------------------------------------------------------------------------------------------
// FEATURE WIRING (keep when restyling):
//   * SPLIT / MERGE: when proc.hasSplit(), layoutTargets() puts the serial units first, then the SplitUnit
//     strip, then branch A (left column) and branch B (right column). Unit menu: insert / move / remove
//     SPLIT, move to branch A / B. Add zone menu: add to branch A / B, add SPLIT. Gear drops into a
//     column go to that branch (insertEffectInBranch / insertEffectInBranchAt); a unit dragged into
//     the other column changes branch (setSlotBranch).
//   * SPECTRAL VIEW: tick() polls the hovered unit -> spectrum->setSlot() (= proc SlotTap
//     setAnalyzedSlot) and UnitComponent::paint() draws it behind the knobs via paintSpectrum().
//   * LEVEL-MATCHED POWER: POWER SideButton onHoldStart / onHoldEnd (> 250 ms) ->
//     proc.setLevelMatchedAudition (slot / -1); a short click still toggles POWER.
// ------------------------------------------------------------------------------------------------

#include "UnitControls.h"
#include "SplitUnit.h"
#include "SpectrumOverlay.h"
#include "../../PluginProcessor.h"

namespace ek::ui
{
namespace dnd
{
    juce::String fx (ek::EffectType t);
    juce::String unit (int slot);
    bool parseFx (const juce::var& d, ek::EffectType& t);
    bool parseUnit (const juce::var& d, int& slot);
}

class RackView;

class UnitComponent : public juce::Component
{
public:
    UnitComponent (EffectorKillaAudioProcessor& p, RackView& owner, int slot);
    ~UnitComponent() override;

    static constexpr int height = ek::faceplate::fullHeight;                  // full-width card (120)
    static constexpr int compactBelow = (int) ek::faceplate::compactBelow;    // narrower cards use the compact layout (SPLIT branches)
    // card height for a width (full: 120, compact: 186 with one knob row, 260 with two)
    static int idealHeight (ek::EffectType type, int width);

    int getSlot() const noexcept { return slot; }
    ek::EffectType getType() const noexcept { return type; }
    juce::Colour getAccent() const noexcept { return pal.accent; }
    bool isCompact() const noexcept { return getWidth() < compactBelow; }
    float getLevel() const noexcept { return juce::jmax (state.inLevel, state.outLevel); }
    bool isPowered() const noexcept { return state.powered; }
    void tick (double timeSec);
    void setReduceMotion (bool r);
    void setFlash (float amount);                 // KILL: ice-blue glow sweep 0..1 (1 = start)
    void setLifted (bool l);                      // hidden in place while it is being dragged
    bool isLifted() const noexcept { return lifted; }
    juce::Image createLiftImage (int shadowMargin);

    juce::Rectangle<float> plateBounds() const;
    RackControl* controlAt (juce::Point<int> local) const;
    RackControl* findControl (int param) const;
    const juce::OwnedArray<RackControl>& getControls() const noexcept { return controls; }
    void repaintControls();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void fillState (double timeSec);
    void showPresetMenu (juce::Component* target);
    void showUnitMenu (juce::Component* target);
    void layoutControls();
    juce::Rectangle<float> statusPillArea() const;
    juce::String currentProgrammeName() const;

    EffectorKillaAudioProcessor& proc;
    RackView& owner;
    const int slot;
    const ek::EffectType type;
    const ek::faceplate::Palette pal;
    juce::OwnedArray<RackControl> controls;
    SideButton menuBtn, powerBtn, soloBtn, lockBtn, removeBtn;
    UnitLcd lcd;
    MsSwitch msBtn;

    ek::faceplate::State state;
    std::array<float, ek::UnitTaps::scopeSize> scopeL {}, scopeR {};
    int hitHold = 0;
    bool lastPowered = true, lastDimmed = false, lastLocked = false, reduceMotion = false, lifted = false;
    bool dragStarted = false, firstTick = true;
    float flash = 0.0f;
    int lastProgramEvents = -1;
    bool lastAudition = false;
    // MACRO LEARN range drag in progress (committed with one assignMacro() on mouse-up)
    int macroDragParam = -1, macroDragMacro = -1;
    float macroDragLo = 0.0f, macroDragHi = 1.0f;
};

class RackView : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit RackView (EffectorKillaAudioProcessor& p);
    ~RackView() override;

    void refresh (bool repaintAll = true);   // rebuilds when the rack structure changed
    void tick();                              // 30 Hz from the editor: live data + animations
    int getIdealHeight() const;
    bool isDimmed (int slot) const;          // another unit is soloed / screened away

    void setReduceMotion (bool r);
    bool getReduceMotion() const noexcept { return reduceMotion; }

    // ---- knob hooks (see the header comment) ----------------------------------------------------
    struct KnobHit
    {
        int slot = -1, param = -1;
        juce::Rectangle<int> bounds;   // in RackView coordinates
    };
    std::function<void (int slot, int param, juce::Point<int> screenPos)> onKnobRightClick;
    std::function<void (juce::Graphics&, juce::Rectangle<float> face, int slot, int param)> paintKnobOverlay;
    KnobHit knobAt (juce::Point<int> screenPos) const;
    KnobHit knobAtLocal (juce::Point<int> posInRackView) const;
    juce::Rectangle<int> getKnobBounds (int slot, int param) const;
    void repaintKnobs();

    // ---- SPLIT / SPECTRAL VIEW ------------------------------------------------------------------
    SplitUnit* getSplitUnit() const noexcept { return splitUnit.get(); }
    int getHoveredSlot() const noexcept { return hoverSlot; }
    void setHoveredSlot (int slot);                       // -1 = none; drives the SPECTRAL VIEW tap
    void paintSpectrum (juce::Graphics& g, int slot, juce::Rectangle<float> area, float opacity = 1.0f) const;

    // ---- used by the units ----------------------------------------------------------------------
    void beginUnitDrag (UnitComponent& u, const juce::MouseEvent& e);
    double clock() const noexcept;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (const juce::String&)> onMessage;
    static void showAddMenu (EffectorKillaAudioProcessor& p, juce::Component* target, std::function<void (ek::EffectType)> picked);

    static constexpr int gutter = 26;     // left gutter with the signal line

    // Snapshot tool / tests only: show a drop state without a real drag.
    //   kind 1 = insert line in front of `slot`, 2 = REPLACE on `slot`, 3 = `slot` lifted with its gap at
    //   compacted index `gap`, 4 = `slot` lifted and dragged out of the chain (remove hint), 0 = clear.
    void previewDropState (int kind, int slot, int gap = -1);

private:
    struct DropTarget
    {
        enum Kind { None, Insert, Replace, Append, Move } kind = None;
        int slot = -1;     // slot to insert before / replace
        int gap = -1;      // Move: index in the compacted unit order where the dragged unit lands
        float y = 0.0f;    // indicator position
        int branch = -1;   // SPLIT: branch (0 = A, 1 = B) the drop goes to, -1 = serial
        int x = -1, w = -1; // SPLIT: indicator column (-1 = full width)
    };
    // vertical structure of the chain (independent of animations / drags)
    struct Layout
    {
        int splitY = -1, colTop = -1, colBottom = -1, mergeY = -1, bottom = 0;
        int colW = 0, xA = 0, xB = 0;
        bool emptyA = false, emptyB = false;
    };
    Layout computeLayout() const;
    DropTarget dropTargetFor (juce::Point<int> pos, bool fromUnit, int fromSlot) const;
    juce::String signature() const;
    juce::Rectangle<int> addZone() const;
    int lastOccupied() const;
    bool isFull() const;
    int unitX() const;
    int unitWidth() const;
    int contentBottom() const;                 // bottom of the units (and the split columns + merge)
    int branchColumnAt (juce::Point<int> pos) const; // SPLIT: 0 = A (left), 1 = B (right)
    float lineX (int branch) const;            // x of the signal line (branch 1 = column B)
    float signalLevel() const;
    void layoutTargets (bool snap);
    void endUnitDrag();
    void paintSignalLine (juce::Graphics&, const Layout&);
    void paintAddZone (juce::Graphics&);
    UnitComponent* unitForSlot (int slot) const;

    EffectorKillaAudioProcessor& proc;
    juce::OwnedArray<UnitComponent> units;
    std::vector<float> animY, targetY;
    std::vector<int> targetX, targetW, targetH;
    std::unique_ptr<SplitUnit> splitUnit;
    std::unique_ptr<ek::SpectrumOverlay> spectrum;
    int hoverSlot = -1, splitY = -1;
    bool hoverFromMouse = false;
    juce::String builtFor;
    DropTarget drop;
    int draggingSlot = -1;
    juce::Rectangle<int> gapRect;              // where the lifted card will land
    bool reduceMotion = false, addHover = false;
    int lastKillEvents = -1;
    double killAnimStart = -1.0;
    float shownLevel = 0.0f;
    bool lineAnimating = false;
    static constexpr int gapY = 12, topMargin = 10, addZoneHeight = 128;
    static constexpr int splitGap = 24, mergeGap = 36, emptyBranchHeight = 120;
};

} // namespace ek::ui
