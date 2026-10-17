/*
    TrapWin BEND DEMON
    LineEditor.h - the DRAW-mode editing tools for the Voice Field (2.0).

      PENCIL (P)   click = a note on that semitone; drag = freehand curve (Shift: semitones only).
                   The MAGNET pulls the drawn curve towards the scale (Scale Lock key/scale, or
                   semitones when Scale Lock is off) without flattening the glides.
      LINE   (L)   click-drag = straight glide from A to B; click again further right = add a knee;
                   Esc / Enter / right-click / another tool = finish. Alt = free pitch.
      SELECT (V)   click / Shift-click / box select; drag = move (time on the grid, pitch in
                   semitones, Ctrl = free); Alt-drag = duplicate; drag the ends = resize; drag a
                   point = reshape (Shift = semitone); double-click a line = add a point;
                   right-click = menu.
      ERASER (E)   click or sweep over lines.

      Ctrl+C / X / V / D, Delete, Ctrl+A, Ctrl+Z / Ctrl+Shift+Z (Ctrl+Y), Up/Down = +-1 semitone
      (Shift: octave), Left/Right = nudge by the grid, Q = quantize start, M = smooth.
      The space bar is never used (it belongs to the host).

    Every gesture is one undo step (PatternStore drag protocol); drags are audible while you drag.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/PatternTools.h"

namespace bd
{

class VoiceField;
class PatternStore;

class LineEditController
{
public:
    enum class Tool { pencil = 0, line, select, eraser };

    LineEditController (VoiceField&, PatternStore&, juce::AudioProcessorValueTreeState&,
                        std::function<EngineSettings()> gestureSettings, std::function<double()> tempo);

    void setTool (Tool);
    Tool getTool() const noexcept               { return tool; }
    std::function<void()> onToolChanged;
    std::function<void (const juce::String&)> onMessage;   // short status / toast text

    /** Grid in beats (0 = off). */
    void setSnap (double beats) noexcept        { snap = beats; }
    double getSnap() const noexcept             { return snap; }
    void setMagnet (float strength) noexcept    { magnet = juce::jlimit (0.0f, 1.0f, strength); }
    float getMagnet() const noexcept            { return magnet; }

    // input (forwarded by the VoiceField while it is in DRAW mode and flat)
    void mouseDown (const juce::MouseEvent&);
    void mouseDrag (const juce::MouseEvent&);
    void mouseUp (const juce::MouseEvent&);
    void mouseMove (const juce::MouseEvent&);
    void mouseExit (const juce::MouseEvent&);
    void mouseDoubleClick (const juce::MouseEvent&);
    bool keyPressed (const juce::KeyPress&);

    void paintOverlay (juce::Graphics&);

    // commands (toolbar / menus / keys)
    void copy();
    void cut();
    void paste();
    void duplicate();
    void deleteSelection();
    void selectAll();
    void smoothSelection();
    void quantizeSelection();
    void transposeSelection (float semitones);
    void nudgeSelection (int gridSteps);
    void toggleCurveType();
    void clearPattern();
    void setBars (int bars);
    void applyGesture (tools::Gesture);
    void finishLine()                           { lineInProgress = 0; }

    bool hasSelection() const;
    bool canPaste() const noexcept              { return ! clipboard().empty(); }
    uint32_t getHoverLine() const noexcept      { return hover.id; }

    void showContextMenu (juce::Point<float> where);

private:
    struct Hit
    {
        enum Part { none = 0, body, startEdge, endEdge, point };
        uint32_t id = 0;
        Part part = none;
        int pointIndex = -1;
    };

    enum class Drag { none, pencil, lineNew, lineKnee, box, move, resizeStart, resizeEnd, point, erase };

    Hit hitTest (juce::Point<float>) const;
    std::vector<uint32_t> linesInBox (juce::Rectangle<float>) const;
    double snapFloor (double beat, bool bypass) const noexcept;
    double snapRound (double beat, bool bypass) const noexcept;
    double gridOrDefault() const noexcept       { return snap > 0.0 ? snap : 0.25; }
    ScaleLock currentScale() const;
    void updateCursor();
    void eraseAt (juce::Point<float> from, juce::Point<float> to);
    void finishPencil (const juce::MouseEvent&);
    void message (const juce::String& m)        { if (onMessage) onMessage (m); }
    static std::vector<PatternLine>& clipboard();

    VoiceField& field;
    PatternStore& store;
    juce::AudioProcessorValueTreeState& apvts;
    std::function<EngineSettings()> getGestureSettings;
    std::function<double()> getTempo;

    Tool tool = Tool::pencil;
    double snap = 0.25;
    float magnet = 0.6f;

    // gesture state
    Drag drag = Drag::none;
    Hit downHit, hover;
    juce::Point<float> downPos, lastPos, mousePos;
    bool mouseInside = false;
    double downBeat = 0.0;
    float downPitch = 60.0f;
    bool moved = false;
    Pattern base;                              // pattern at the start of the drag (+ Alt duplicates)
    std::vector<uint32_t> baseSelection;
    Curve1D stroke;                            // pencil: absolute beats, pitch
    juce::Rectangle<float> box;
    uint32_t lineInProgress = 0;               // LINE tool: knees extend this line
    juce::Point<float> lineA;                  // LINE tool preview start (beat, pitch)
    double lineABeat = 0.0;
    float lineAPitch = 60.0f;
};

} // namespace bd
