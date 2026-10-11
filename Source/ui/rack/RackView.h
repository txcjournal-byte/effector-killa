#pragma once

// The rack: hardware units stacked top to bottom (signal flows downwards), DESIGN-RACK.md sections 6, 8, 9.
//
// Unit = 932 x 140: faceplate 898 x 140 (painted by ek::faceplate::paintFaceplate, live State filled from
// the processor every timer tick) + a 34 px side strip with four 26 x 26 buttons:
//   MENU (programmes, my unit presets, save, copy / paste, replace, reset, listen up to here, remove),
//   POWER (green; off keeps tails running), SOLO (yellow, blinks; other units dim), LOCK (red).
// The real controls sit on the plate inside faceplate::knobArea(): every used parameter + DRY/WET,
// label above, value below; switch parameters as LED rows / stepped selectors / LED toggles.
// ST / MID / SIDE switch next to the unit preset LCD (faceplate::presetLcdArea()).
//
// Drag & drop:
//   gear item  -> top / bottom edge of a unit = insert there (blue line)
//              -> centre of a unit            = replace it (yellow REPLACE frame)
//              -> empty shaft under the units = add at the end
//   unit plate -> drag anywhere outside a control: the unit lifts (shadow), a gap opens where it will land,
//                 the other units glide out of the way
//              -> back onto the gear list     = remove
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

#include "UnitControls.h"
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

    static constexpr int width = 932, height = 140, plateWidth = 898, stripWidth = 34;

    int getSlot() const noexcept { return slot; }
    ek::EffectType getType() const noexcept { return type; }
    void tick (double timeSec);
    void setReduceMotion (bool r);
    void setFlash (float amount);                 // KILL: ice-blue flash 0..1
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
    juce::String currentProgrammeName() const;

    EffectorKillaAudioProcessor& proc;
    RackView& owner;
    const int slot;
    const ek::EffectType type;
    const ek::faceplate::Palette pal;
    juce::OwnedArray<RackControl> controls;
    SideButton menuBtn, powerBtn, soloBtn, lockBtn;
    UnitLcd lcd;
    MsSwitch msBtn;

    ek::faceplate::State state;
    std::array<float, ek::UnitTaps::scopeSize> scopeL {}, scopeR {};
    int hitHold = 0;
    bool lastPowered = true, lastDimmed = false, lastLocked = false, reduceMotion = false, lifted = false;
    bool dragStarted = false, firstTick = true;
    float flash = 0.0f;
    int lastProgramEvents = -1;
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

    // ---- used by the units ----------------------------------------------------------------------
    void beginUnitDrag (UnitComponent& u, const juce::MouseEvent& e);
    double clock() const noexcept;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    // DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    std::function<void (const juce::String&)> onMessage;
    static void showAddMenu (EffectorKillaAudioProcessor& p, juce::Component* target, std::function<void (ek::EffectType)> picked);

private:
    struct DropTarget
    {
        enum Kind { None, Insert, Replace, Append, Move } kind = None;
        int slot = -1;     // slot to insert before / replace
        int gap = -1;      // Move: index in the compacted unit order where the dragged unit lands
        float y = 0.0f;    // indicator position
    };
    DropTarget dropTargetFor (juce::Point<int> pos, bool fromUnit, int fromSlot) const;
    juce::String signature() const;
    juce::Rectangle<int> addZone() const;
    int lastOccupied() const;
    bool isFull() const;
    int unitX() const;
    int unitWidth() const;
    void layoutTargets (bool snap);
    void endUnitDrag();
    UnitComponent* unitForSlot (int slot) const;

    EffectorKillaAudioProcessor& proc;
    juce::OwnedArray<UnitComponent> units;
    std::vector<float> animY, targetY;
    juce::String builtFor;
    DropTarget drop;
    int draggingSlot = -1;
    bool reduceMotion = false, addHover = false;
    int lastKillEvents = -1;
    double killAnimStart = -1.0;
    static constexpr int gapY = 4, topMargin = 0, addZoneHeight = 140;
};

} // namespace ek::ui
