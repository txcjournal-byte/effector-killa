#include "LineEditor.h"
#include "VoiceField.h"
#include "Palette.h"
#include "Fonts.h"
#include "LookAndFeel.h"
#include "../PatternStore.h"
#include "../Parameters.h"
#include <cmath>

namespace bd
{

namespace
{
    constexpr double kMinLen = 1.0 / 64.0;

    juce::String toolName (LineEditController::Tool t)
    {
        switch (t)
        {
            case LineEditController::Tool::pencil: return "PENCIL";
            case LineEditController::Tool::line:   return "LINE";
            case LineEditController::Tool::select: return "SELECT";
            case LineEditController::Tool::eraser: return "ERASER";
        }
        return {};
    }
}

std::vector<PatternLine>& LineEditController::clipboard()
{
    static std::vector<PatternLine> clip;   // shared by every editor instance (copy between projects)
    return clip;
}

LineEditController::LineEditController (VoiceField& f, PatternStore& s, juce::AudioProcessorValueTreeState& state,
                                        std::function<EngineSettings()> gs, std::function<double()> tempo)
    : field (f), store (s), apvts (state), getGestureSettings (std::move (gs)), getTempo (std::move (tempo))
{
}

void LineEditController::setTool (Tool t)
{
    if (drag != Drag::none) return;
    lineInProgress = 0;
    if (tool == t) return;
    tool = t;
    updateCursor();
    if (onToolChanged) onToolChanged();
}

bool LineEditController::hasSelection() const
{
    return ! store.getSelection().empty();
}

ScaleLock LineEditController::currentScale() const
{
    auto get = [this] (const char* id) { auto* v = apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; };
    ScaleLock sl;
    const bool lockOn = get (ids::scaleLockOn) > 0.5f;
    sl.set ((int) std::lround (get (ids::scaleKey)), lockOn ? (int) std::lround (get (ids::scaleType)) : (int) ScaleLock::chromatic);
    return sl;
}

double LineEditController::snapFloor (double b, bool bypass) const noexcept
{
    if (snap <= 0.0 || bypass) return b;
    return std::floor (b / snap + 1.0e-6) * snap;
}

double LineEditController::snapRound (double b, bool bypass) const noexcept
{
    if (snap <= 0.0 || bypass) return b;
    return std::round (b / snap) * snap;
}

//==============================================================================
LineEditController::Hit LineEditController::hitTest (juce::Point<float> pos) const
{
    const auto& p = store.get();
    const float s = field.getUiScale();
    Hit h;

    // points of selected lines first
    const float pr = 6.5f * s;
    for (auto it = p.lines.rbegin(); it != p.lines.rend(); ++it)
    {
        if (! store.isSelected (it->id)) continue;
        for (int i = (int) it->pitch.size(); --i >= 0;)
        {
            const auto& pt = it->pitch[(size_t) i];
            const juce::Point<float> q (field.beatToX (it->start + pt.t), field.pitchToY (pt.v));
            if (q.getDistanceFrom (pos) <= pr)
                return { it->id, Hit::point, i };
        }
    }

    const float tolY = juce::jmax (6.0f * s, field.semitoneHeight() * 0.5f);
    for (auto it = p.lines.rbegin(); it != p.lines.rend(); ++it)
    {
        const auto& l = *it;
        const float x0 = field.beatToX (l.start), x1 = field.beatToX (l.end());
        if (pos.x < x0 - 4.0f * s || pos.x > x1 + 4.0f * s) continue;

        const double beat = juce::jlimit (l.start, l.end(), field.xToBeat (pos.x));
        const float y = field.pitchToY (l.pitchAt (beat - l.start));
        const float half = (1.2f + l.pressureAt (beat - l.start) * 9.0f) * s;
        if (std::abs (pos.y - y) > tolY + half) continue;

        h.id = l.id;
        h.part = Hit::body;
        // resize zones only on notes wide enough to also be grabbed in the middle
        if (x1 - x0 > 24.0f * s)
        {
            const float edge = juce::jmin (7.0f * s, (x1 - x0) * 0.25f);
            if (pos.x - x0 < edge)      h.part = Hit::startEdge;
            else if (x1 - pos.x < edge) h.part = Hit::endEdge;
        }
        return h;
    }

    return h;
}

std::vector<uint32_t> LineEditController::linesInBox (juce::Rectangle<float> r) const
{
    std::vector<uint32_t> out;
    for (auto& l : store.get().lines)
    {
        const float x0 = juce::jmax (r.getX(), field.beatToX (l.start));
        const float x1 = juce::jmin (r.getRight(), field.beatToX (l.end()));
        if (x1 < x0) continue;
        for (float x = x0; x <= x1 + 0.01f; x += 3.0f)
        {
            const double b = juce::jlimit (l.start, l.end(), field.xToBeat (x));
            if (r.contains (x, field.pitchToY (l.pitchAt (b - l.start))))
            {
                out.push_back (l.id);
                break;
            }
        }
    }
    return out;
}

void LineEditController::updateCursor()
{
    juce::MouseCursor c = juce::MouseCursor::NormalCursor;
    switch (tool)
    {
        case Tool::pencil:
        case Tool::line:   c = juce::MouseCursor::CrosshairCursor; break;
        case Tool::eraser: c = juce::MouseCursor::CrosshairCursor; break;
        case Tool::select:
            switch (hover.part)
            {
                case Hit::startEdge:
                case Hit::endEdge: c = juce::MouseCursor::LeftRightResizeCursor; break;
                case Hit::point:   c = juce::MouseCursor::PointingHandCursor; break;
                case Hit::body:    c = juce::MouseCursor::DraggingHandCursor; break;
                case Hit::none:    break;
            }
            break;
    }
    field.setMouseCursor (c);
}

//==============================================================================
void LineEditController::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    mouseInside = true;
    const auto h = hitTest (e.position);
    if (h.id != hover.id || h.part != hover.part || h.pointIndex != hover.pointIndex)
    {
        hover = h;
        updateCursor();
    }
}

void LineEditController::mouseExit (const juce::MouseEvent&)
{
    mouseInside = false;
    hover = {};
}

void LineEditController::mouseDown (const juce::MouseEvent& e)
{
    mousePos = downPos = lastPos = e.position;
    mouseInside = true;
    moved = false;
    downBeat = field.xToBeat (e.position.x);
    downPitch = field.yToPitch (e.position.y);
    downHit = hitTest (e.position);
    const bool noSnap = e.mods.isCommandDown();

    if (e.mods.isPopupMenu())
    {
        if (downHit.id != 0 && tool != Tool::select && downHit.part != Hit::point)
        {
            // right-click deletes with the drawing tools (piano-roll habit)
            auto p = store.get();
            p.remove (downHit.id);
            store.perform (std::move (p), "Delete line");
            return;
        }

        if (downHit.part == Hit::point && downHit.pointIndex > 0)
        {
            auto p = store.get();
            if (auto* l = p.find (downHit.id))
            {
                l->pitch.erase (l->pitch.begin() + downHit.pointIndex);
                store.perform (std::move (p), "Delete point");
            }
            return;
        }

        if (downHit.id != 0 && ! store.isSelected (downHit.id))
            store.select (downHit.id, false);
        lineInProgress = 0;
        showContextMenu (e.position);
        return;
    }

    base = store.get();

    switch (tool)
    {
        case Tool::pencil:
        {
            drag = Drag::pencil;
            stroke.clear();
            const double start = snapFloor (downBeat, noSnap);
            stroke.push_back ({ std::max (0.0, start), downPitch });
            if (downBeat - start > 1.0e-6)
                stroke.push_back ({ downBeat, downPitch });
            break;
        }

        case Tool::line:
        {
            const bool freePitch = e.mods.isAltDown();
            const float pitch = freePitch ? downPitch : (float) std::round (downPitch);
            const double beat = std::max (0.0, snapRound (downBeat, noSnap));

            if (const auto* l = base.find (lineInProgress); l != nullptr && beat > l->end() - 1.0e-6)
            {
                // knee: extend the line in progress to the new point, then drag that point
                auto p = base;
                auto* lp = p.find (lineInProgress);
                const double tRel = std::max (beat, lp->end() + kMinLen) - lp->start;
                lp->pitch.push_back ({ lp->length, lp->curvePitchAt (lp->length) });
                lp->pitch.push_back ({ tRel, pitch });
                lp->length = tRel;
                lp->normalise();
                store.beginDrag();
                store.setLive (p);
                base = p;
                drag = Drag::lineKnee;
            }
            else
            {
                lineInProgress = 0;
                lineABeat = beat;
                lineAPitch = pitch;
                drag = Drag::lineNew;
            }
            break;
        }

        case Tool::select:
        {
            if (downHit.id == 0)
            {
                drag = Drag::box;
                box = { e.position, e.position };
                baseSelection = e.mods.isShiftDown() ? store.getSelection() : std::vector<uint32_t> {};
                if (! e.mods.isShiftDown()) store.clearSelection();
                break;
            }

            if (e.mods.isShiftDown() || e.mods.isCommandDown())
            {
                if (store.isSelected (downHit.id) && downHit.part != Hit::point) { store.deselect (downHit.id); drag = Drag::none; return; }
                store.select (downHit.id, true);
            }
            else if (! store.isSelected (downHit.id))
            {
                store.select (downHit.id, false);
            }

            store.beginDrag();
            base = store.get();

            if (e.mods.isAltDown() && downHit.part == Hit::body)
            {
                // Alt-drag: duplicate the selection in place, then move the copies.
                // tools::duplicate copies in pattern order, so the clicked line's copy has the
                // same index among the copies as the clicked line among the selected lines.
                size_t anchorIndex = 0, k = 0;
                for (auto& l : base.lines)
                    if (store.isSelected (l.id)) { if (l.id == downHit.id) anchorIndex = k; ++k; }

                const auto ids = tools::duplicate (base, store.getSelection(), 0.0);
                store.setLive (base);
                store.setSelection (ids);
                if (anchorIndex < ids.size()) downHit.id = ids[anchorIndex];
            }

            drag = downHit.part == Hit::point     ? Drag::point
                 : downHit.part == Hit::startEdge ? Drag::resizeStart
                 : downHit.part == Hit::endEdge   ? Drag::resizeEnd
                                                   : Drag::move;
            break;
        }

        case Tool::eraser:
            drag = Drag::erase;
            store.beginDrag();
            eraseAt (e.position, e.position);
            break;
    }
}

void LineEditController::eraseAt (juce::Point<float> from, juce::Point<float> to)
{
    auto p = store.get();
    bool changed = false;
    const float dist = from.getDistanceFrom (to);
    const int steps = juce::jmax (1, (int) (dist / 3.0f));
    for (int i = 0; i <= steps; ++i)
    {
        const auto q = from + (to - from) * ((float) i / (float) steps);
        const auto h = hitTest (q);
        if (h.id != 0 && p.remove (h.id))
        {
            changed = true;
            store.setLive (p);
        }
    }
    juce::ignoreUnused (changed);
}

void LineEditController::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (e.position.getDistanceFrom (downPos) > 3.0f) moved = true;
    const bool noSnap = e.mods.isCommandDown();
    const double beat = field.xToBeat (e.position.x);
    const float pitch = field.yToPitch (e.position.y);

    switch (drag)
    {
        case Drag::pencil:
        {
            // time only moves forward; moving back erases the stroke tail
            while (stroke.size() > 1 && stroke.back().t > beat)
                stroke.pop_back();
            const double minStep = 1.0 / 512.0;
            if (beat > stroke.back().t + minStep)
                stroke.push_back ({ beat, e.mods.isShiftDown() ? (float) std::round (pitch) : pitch });
            break;
        }

        case Drag::lineNew:
            break;   // preview only (overlay)

        case Drag::lineKnee:
        {
            auto p = base;
            if (auto* l = p.find (lineInProgress))
            {
                const double prevT = l->pitch.size() >= 2 ? l->pitch[l->pitch.size() - 2].t : 0.0;
                const double tRel = std::max (prevT + kMinLen, snapRound (beat, noSnap) - l->start);
                l->pitch.back() = { tRel, e.mods.isAltDown() ? pitch : (float) std::round (pitch) };
                l->length = tRel;
                store.setLive (p);
            }
            break;
        }

        case Drag::box:
        {
            box = juce::Rectangle<float> (downPos, e.position);
            auto ids = linesInBox (box);
            for (auto id : baseSelection) ids.push_back (id);
            store.setSelection (ids);
            break;
        }

        case Drag::move:
        {
            if (! moved) break;
            const auto* anchor = base.find (downHit.id);
            if (anchor == nullptr) break;

            double dBeat = beat - downBeat;
            if (! noSnap && snap > 0.0)
                dBeat = snapRound (anchor->start + dBeat, false) - anchor->start;

            double minStart = 1.0e9;
            for (auto& l : base.lines) if (store.isSelected (l.id)) minStart = std::min (minStart, l.start);
            dBeat = std::max (dBeat, -minStart);

            float dPitch = pitch - downPitch;
            if (! noSnap) dPitch = std::round (dPitch);

            auto p = base;
            for (auto& l : p.lines)
                if (store.isSelected (l.id))
                {
                    l.start += dBeat;
                    for (auto& pt : l.pitch) pt.v = juce::jlimit (0.0f, 127.0f, pt.v + dPitch);
                }
            store.setLive (p);
            break;
        }

        case Drag::resizeStart:
        case Drag::resizeEnd:
        {
            const auto* anchor = base.find (downHit.id);
            if (anchor == nullptr) break;
            const double target = std::max (0.0, snapRound (beat, noSnap));
            const double delta = drag == Drag::resizeEnd ? target - anchor->end() : target - anchor->start;
            const double minLen = snap > 0.0 && ! noSnap ? snap : kMinLen;

            auto p = base;
            for (auto& l : p.lines)
            {
                if (! store.isSelected (l.id)) continue;
                const auto* b = base.find (l.id);
                if (drag == Drag::resizeEnd)
                    tools::retime (l, b->start, std::max (b->start + minLen, b->end() + delta));
                else
                    tools::retime (l, juce::jlimit (0.0, b->end() - minLen, b->start + delta), b->end());
            }
            store.setLive (p);
            break;
        }

        case Drag::point:
        {
            auto p = base;
            auto* l = p.find (downHit.id);
            if (l == nullptr || ! juce::isPositiveAndBelow (downHit.pointIndex, (int) l->pitch.size())) break;
            const auto i = (size_t) downHit.pointIndex;
            auto& pt = l->pitch[i];
            if (i > 0)
            {
                const double lo = l->pitch[i - 1].t + 1.0 / 256.0;
                const double hi = i + 1 < l->pitch.size() ? l->pitch[i + 1].t - 1.0 / 256.0 : l->length;
                pt.t = juce::jlimit (lo, std::max (lo, hi), beat - l->start);
            }
            pt.v = juce::jlimit (0.0f, 127.0f, e.mods.isShiftDown() ? (float) std::round (pitch) : pitch);
            store.setLive (p);
            break;
        }

        case Drag::erase:
            eraseAt (lastPos, e.position);
            break;

        case Drag::none:
            break;
    }

    lastPos = e.position;
}

void LineEditController::finishPencil (const juce::MouseEvent& e)
{
    if (stroke.empty()) return;
    const bool noSnap = e.mods.isCommandDown();
    const double start = stroke.front().t;
    double end = stroke.back().t;

    PatternLine l;
    l.start = start;
    l.smooth = true;

    if (! moved || end - start < 1.0 / 64.0)
    {
        // a click: one note on the clicked semitone, one grid step long
        l.length = gridOrDefault();
        l.pitch = { { 0.0, (float) std::round (stroke.front().v) } };
    }
    else
    {
        end = std::max (start + kMinLen, snap > 0.0 && ! noSnap ? std::max (start + snap, snapRound (end, false)) : end);
        l.length = end - start;

        Curve1D pts;
        for (auto& p : stroke) pts.push_back ({ p.t - start, p.v });
        pts = curves::simplify (pts, 0.06f);
        // stretch the curve's time to the (snapped) length
        const double drawn = std::max (1.0e-6, pts.back().t);
        for (auto& p : pts) p.t = p.t / drawn * l.length;

        if (e.mods.isShiftDown())
            for (auto& p : pts) p.v = (float) std::round (p.v);
        else
            tools::magnetize (pts, currentScale(), magnet);
        l.pitch = pts;
    }

    auto p = store.get();
    const auto id = p.add (std::move (l));
    store.perform (std::move (p), "Draw line");
    store.select (id, false);
}

void LineEditController::mouseUp (const juce::MouseEvent& e)
{
    const auto d = drag;
    drag = Drag::none;

    switch (d)
    {
        case Drag::pencil:
            finishPencil (e);
            stroke.clear();
            break;

        case Drag::lineNew:
        {
            const bool noSnap = e.mods.isCommandDown();
            double endBeat = std::max (0.0, snapRound (field.xToBeat (e.position.x), noSnap));
            const float endPitch = e.mods.isAltDown() ? field.yToPitch (e.position.y) : (float) std::round (field.yToPitch (e.position.y));

            PatternLine l;
            l.start = lineABeat;
            l.smooth = false;
            if (! moved || endBeat <= lineABeat + 1.0e-6)
            {
                l.length = gridOrDefault();
                l.pitch = { { 0.0, lineAPitch } };
            }
            else
            {
                endBeat = std::max (endBeat, lineABeat + kMinLen);
                l.length = endBeat - lineABeat;
                l.pitch = { { 0.0, lineAPitch }, { l.length, endPitch } };
            }

            auto p = store.get();
            lineInProgress = p.add (std::move (l));
            store.perform (std::move (p), "Draw glide");
            store.select (lineInProgress, false);
            message ("LINE: click further right to add a knee  |  Esc / Enter to finish");
            break;
        }

        case Drag::lineKnee:
            store.commitDrag ("Add knee");
            break;

        case Drag::move:
            if (moved) store.commitDrag ("Move");
            else
            {
                store.cancelDrag();
                // plain click on a selected line inside a multi-selection: select just that one
                if (! e.mods.isShiftDown() && ! e.mods.isCommandDown() && downHit.id != 0)
                    store.select (downHit.id, false);
            }
            break;

        case Drag::resizeStart:
        case Drag::resizeEnd:
            store.commitDrag ("Resize");
            break;

        case Drag::point:
            store.commitDrag ("Move point");
            break;

        case Drag::erase:
            store.commitDrag ("Erase");
            break;

        case Drag::box:
        case Drag::none:
            break;
    }

    mouseMove (e);
}

void LineEditController::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (tool == Tool::line)
    {
        lineInProgress = 0;
        return;
    }

    const auto h = hitTest (e.position);
    if (h.id == 0 || h.part == Hit::point) return;

    auto p = store.get();
    auto* l = p.find (h.id);
    if (l == nullptr) return;

    // remove the note a click of the pencil may just have created on top
    const double tRel = juce::jlimit (0.0, l->length, field.xToBeat (e.position.x) - l->start);
    if (tRel <= 1.0e-3) return;
    l->pitch.push_back ({ tRel, l->curvePitchAt (tRel) });
    l->normalise();
    store.perform (std::move (p), "Add point");
    store.select (h.id, false);
    if (tool != Tool::select) setTool (Tool::select);
}

bool LineEditController::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const bool cmd = mods.isCommandDown();
    const int code = k.getKeyCode();
    const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());

    if (cmd)
    {
        if (code == 'Z' || ch == 'z') { mods.isShiftDown() ? store.redo() : store.undo(); return true; }
        if (code == 'Y' || ch == 'y') { store.redo(); return true; }
        if (code == 'C' || ch == 'c') { copy(); return true; }
        if (code == 'X' || ch == 'x') { cut(); return true; }
        if (code == 'V' || ch == 'v') { paste(); return true; }
        if (code == 'D' || ch == 'd') { duplicate(); return true; }
        if (code == 'A' || ch == 'a') { selectAll(); return true; }
        return false;
    }

    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
    if (k == juce::KeyPress::escapeKey)
    {
        if (lineInProgress != 0) lineInProgress = 0;
        else store.clearSelection();
        return true;
    }
    if (k == juce::KeyPress::returnKey) { lineInProgress = 0; return true; }
    if (k == juce::KeyPress::upKey)    { transposeSelection (mods.isShiftDown() ? 12.0f : 1.0f); return true; }
    if (k == juce::KeyPress::downKey)  { transposeSelection (mods.isShiftDown() ? -12.0f : -1.0f); return true; }
    if (k == juce::KeyPress::leftKey)  { nudgeSelection (-1); return true; }
    if (k == juce::KeyPress::rightKey) { nudgeSelection (1); return true; }

    switch (ch)
    {
        case 'p': case 'b': case 'd': setTool (Tool::pencil); return true;
        case 'l':           setTool (Tool::line);   return true;
        case 'v': case 's': setTool (Tool::select); return true;
        case 'e':           setTool (Tool::eraser); return true;
        case 'q':           quantizeSelection();    return true;
        case 'm':           smoothSelection();      return true;
        default: break;
    }

    return false;
}

//==============================================================================
void LineEditController::copy()
{
    auto& clip = clipboard();
    clip.clear();
    for (auto& l : store.get().lines)
        if (store.isSelected (l.id)) clip.push_back (l);
    if (! clip.empty()) message ("Copied " + juce::String ((int) clip.size()) + (clip.size() == 1 ? " line" : " lines"));
}

void LineEditController::cut()
{
    copy();
    deleteSelection();
}

void LineEditController::paste()
{
    const auto& clip = clipboard();
    if (clip.empty()) return;

    double minStart = 1.0e9;
    for (auto& l : clip) minStart = std::min (minStart, l.start);

    double anchor = minStart;
    if (mouseInside && field.getFieldBounds().contains (mousePos))
        anchor = std::max (0.0, snapFloor (field.xToBeat (mousePos.x), false));

    auto p = store.get();
    std::vector<uint32_t> ids;
    for (auto l : clip)
    {
        l.start = l.start - minStart + anchor;
        ids.push_back (p.add (std::move (l)));
    }
    store.perform (std::move (p), "Paste");
    store.setSelection (ids);
}

void LineEditController::duplicate()
{
    const auto sel = store.getSelection();
    if (sel.empty()) return;

    double lo = 1.0e9, hi = -1.0e9;
    for (auto& l : store.get().lines)
        if (store.isSelected (l.id)) { lo = std::min (lo, l.start); hi = std::max (hi, l.end()); }

    const double span = hi - lo;
    const double bar = (double) store.get().beatsPerBar;
    const double unit = span >= bar - 1.0e-6 ? bar : gridOrDefault();
    const double offset = std::max (unit, std::ceil (span / unit - 1.0e-6) * unit);

    auto p = store.get();
    const auto ids = tools::duplicate (p, sel, offset);
    store.perform (std::move (p), "Duplicate");
    store.setSelection (ids);
}

void LineEditController::deleteSelection()
{
    if (! hasSelection()) return;
    auto p = store.get();
    for (auto id : store.getSelection()) p.remove (id);
    store.perform (std::move (p), "Delete");
}

void LineEditController::selectAll()           { store.selectAll(); }

void LineEditController::smoothSelection()
{
    auto p = store.get();
    if (tools::smooth (p, store.getSelection(), 0.5f))
        store.perform (std::move (p), "Smooth");
}

void LineEditController::quantizeSelection()
{
    auto p = store.get();
    if (tools::quantizeStarts (p, store.getSelection(), gridOrDefault()))
        store.perform (std::move (p), "Quantize start");
}

void LineEditController::transposeSelection (float semis)
{
    auto p = store.get();
    if (tools::transpose (p, store.getSelection(), semis))
        store.perform (std::move (p), semis > 0 ? "Transpose up" : "Transpose down");
}

void LineEditController::nudgeSelection (int steps)
{
    if (! hasSelection()) return;
    auto p = store.get();
    const double d = gridOrDefault() * steps;
    for (auto& l : p.lines)
        if (store.isSelected (l.id)) l.start = std::max (0.0, l.start + d);
    store.perform (std::move (p), "Nudge");
}

void LineEditController::toggleCurveType()
{
    if (! hasSelection()) return;
    auto p = store.get();
    bool anySmooth = false;
    for (auto& l : p.lines) if (store.isSelected (l.id) && l.smooth) anySmooth = true;
    for (auto& l : p.lines) if (store.isSelected (l.id)) l.smooth = ! anySmooth;
    store.perform (std::move (p), anySmooth ? "Straight segments" : "Smooth curve");
}

void LineEditController::clearPattern()
{
    auto p = store.get();
    p.lines.clear();
    store.perform (std::move (p), "Clear pattern");
}

void LineEditController::setBars (int bars)
{
    auto p = store.get();
    p.bars = juce::jlimit (Pattern::minBars, Pattern::maxBars, bars);
    store.perform (std::move (p), "Pattern length");
}

void LineEditController::applyGesture (tools::Gesture g)
{
    if (! hasSelection())
    {
        message ("Select lines first, then apply a gesture");
        return;
    }
    auto p = store.get();
    const auto ids = tools::bakeGesture (p, store.getSelection(), g, getGestureSettings(), getTempo());
    store.perform (std::move (p), juce::String ("Apply ") + tools::gestureName (g));
    store.setSelection (ids);
}

//==============================================================================
void LineEditController::showContextMenu (juce::Point<float>)
{
    const bool sel = hasSelection();
    juce::PopupMenu m;
    m.addSectionHeader (sel ? juce::String ((int) store.getSelection().size()) + " selected" : juce::String ("Pattern"));
    m.addItem (1, "Cut", sel);
    m.addItem (2, "Copy", sel);
    m.addItem (3, "Paste", canPaste());
    m.addItem (4, "Duplicate", sel);
    m.addItem (5, "Delete", sel);
    m.addSeparator();
    juce::PopupMenu gestures;
    for (int i = 0; i < (int) tools::Gesture::numGestures; ++i)
        gestures.addItem (100 + i, tools::gestureName ((tools::Gesture) i), sel);
    m.addSubMenu ("Apply gesture", gestures, sel);
    m.addItem (10, "Smooth curve", sel);
    m.addItem (11, "Straight / smooth segments", sel);
    m.addItem (12, "Quantize start", sel);
    m.addItem (13, "Transpose +1", sel);
    m.addItem (14, "Transpose -1", sel);
    m.addItem (15, "Octave up", sel);
    m.addItem (16, "Octave down", sel);
    m.addSeparator();
    m.addItem (20, "Select all", ! store.get().lines.empty());
    m.addItem (21, "Clear pattern", ! store.get().lines.empty());

    juce::Component::SafePointer<VoiceField> safe (&field);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&field).withMousePosition(),
                     [this, safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        switch (r)
        {
            case 1: cut(); break;
            case 2: copy(); break;
            case 3: paste(); break;
            case 4: duplicate(); break;
            case 5: deleteSelection(); break;
            case 10: smoothSelection(); break;
            case 11: toggleCurveType(); break;
            case 12: quantizeSelection(); break;
            case 13: transposeSelection (1.0f); break;
            case 14: transposeSelection (-1.0f); break;
            case 15: transposeSelection (12.0f); break;
            case 16: transposeSelection (-12.0f); break;
            case 20: selectAll(); break;
            case 21: clearPattern(); break;
            default:
                if (r >= 100 && r < 100 + (int) tools::Gesture::numGestures)
                    applyGesture ((tools::Gesture) (r - 100));
                break;
        }
    });
}

//==============================================================================
void LineEditController::paintOverlay (juce::Graphics& g)
{
    using namespace juce;
    const float s = field.getUiScale();
    const auto fb = field.getFieldBounds();

    g.saveState();
    g.reduceClipRegion (fb.toNearestInt());

    // hover highlight
    if (hover.id != 0 && drag == Drag::none)
        if (const auto* l = store.get().find (hover.id))
        {
            Path core;
            const int n = jmax (2, (int) ((field.beatToX (l->end()) - field.beatToX (l->start)) / 3.0f));
            for (int i = 0; i <= n; ++i)
            {
                const double t = l->length * (double) i / (double) n;
                const Point<float> q (field.beatToX (l->start + t), field.pitchToY (l->pitchAt (t)));
                if (i == 0) core.startNewSubPath (q); else core.lineTo (q);
            }
            g.setColour (Colours::white.withAlpha (0.25f));
            g.strokePath (core, PathStrokeType (3.0f * s, PathStrokeType::curved, PathStrokeType::rounded));
        }

    // pitch points of the selected lines
    for (auto& l : store.get().lines)
    {
        if (! store.isSelected (l.id)) continue;
        for (size_t i = 0; i < l.pitch.size(); ++i)
        {
            const auto& pt = l.pitch[i];
            const Point<float> q (field.beatToX (l.start + pt.t), field.pitchToY (pt.v));
            const bool hot = hover.id == l.id && hover.part == Hit::point && hover.pointIndex == (int) i;
            const float r = (hot ? 5.5f : 4.0f) * s;
            g.setColour (pal::ground0.withAlpha (0.8f));
            g.fillEllipse (Rectangle<float> (r * 2.0f + 2.0f, r * 2.0f + 2.0f).withCentre (q));
            g.setColour (i == 0 ? pal::lime : pal::amber);
            g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (q));
        }

        // resize handles
        for (double b : { l.start, l.end() })
        {
            const float x = field.beatToX (b);
            const float y = field.pitchToY (l.pitchAt (b - l.start));
            g.setColour (pal::text.withAlpha (0.55f));
            g.fillRoundedRectangle (Rectangle<float> (2.5f * s, 12.0f * s).withCentre ({ x, y }), 1.0f);
        }
    }

    // pencil stroke preview
    if (drag == Drag::pencil && stroke.size() >= 2)
    {
        Path p;
        for (size_t i = 0; i < stroke.size(); ++i)
        {
            const Point<float> q (field.beatToX (stroke[i].t), field.pitchToY (stroke[i].v));
            if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        BendDemonLookAndFeel::strokeGlow (g, p, pal::lime.withAlpha (0.9f), 1.6f * s, 10.0f * s, 4);
    }

    // line tool preview
    if (drag == Drag::lineNew)
    {
        const bool alt = juce::ModifierKeys::getCurrentModifiers().isAltDown();
        const float endPitch = alt ? field.yToPitch (mousePos.y) : (float) std::round (field.yToPitch (mousePos.y));
        const double endBeat = std::max (lineABeat, snapRound (field.xToBeat (mousePos.x), false));
        const Point<float> a (field.beatToX (lineABeat), field.pitchToY (lineAPitch));
        const Point<float> b (field.beatToX (endBeat), field.pitchToY (endPitch));
        Path p;
        p.startNewSubPath (a);
        p.lineTo (b);
        BendDemonLookAndFeel::strokeGlow (g, p, pal::cyan, 1.6f * s, 10.0f * s, 4);
        for (auto q : { a, b })
        {
            g.setColour (pal::lime);
            g.fillEllipse (Rectangle<float> (7.0f * s, 7.0f * s).withCentre (q));
        }
        g.setColour (pal::text);
        g.setFont (fonts::mono (10.0f * jmax (1.0f, s * 0.9f), true));
        const float semis = endPitch - lineAPitch;
        g.drawText ((semis >= 0 ? "+" : "") + String (semis, std::abs (semis - std::round (semis)) < 0.01f ? 0 : 2) + " st",
                    Rectangle<float> (b.x + 8.0f, b.y - 18.0f, 80.0f, 14.0f), Justification::left);
    }

    // line in progress: pulsing end node (next click adds a knee)
    if (tool == Tool::line && lineInProgress != 0 && drag == Drag::none)
        if (const auto* l = store.get().find (lineInProgress))
        {
            const Point<float> q (field.beatToX (l->end()), field.pitchToY (l->curvePitchAt (l->length)));
            const float pulse = 0.5f + 0.5f * std::sin ((float) Time::getMillisecondCounterHiRes() * 0.008f);
            g.setColour (pal::cyan.withAlpha (0.25f + 0.25f * pulse));
            g.fillEllipse (Rectangle<float> ((14.0f + 6.0f * pulse) * s, (14.0f + 6.0f * pulse) * s).withCentre (q));
            if (mouseInside && mousePos.x > q.x)
            {
                const Point<float> m (field.beatToX (std::max (l->end(), snapRound (field.xToBeat (mousePos.x), false))),
                                      field.pitchToY ((float) std::round (field.yToPitch (mousePos.y))));
                g.setColour (pal::cyan.withAlpha (0.45f));
                Path dash;
                dash.startNewSubPath (q);
                dash.lineTo (m);
                const float pattern[] = { 4.0f * s, 4.0f * s };
                PathStrokeType (1.2f * s).createDashedStroke (dash, dash, pattern, 2);
                g.fillPath (dash);
            }
        }

    // box select
    if (drag == Drag::box && ! box.isEmpty())
    {
        g.setColour (pal::cyan.withAlpha (0.08f));
        g.fillRect (box);
        g.setColour (pal::cyan.withAlpha (0.6f));
        g.drawRect (box, 1.0f);
    }

    // snap ghost for the drawing tools
    if ((tool == Tool::pencil || tool == Tool::line) && mouseInside && drag == Drag::none && fb.contains (mousePos))
    {
        const double b = tool == Tool::pencil ? snapFloor (field.xToBeat (mousePos.x), false) : snapRound (field.xToBeat (mousePos.x), false);
        const float x = field.beatToX (b);
        g.setColour (pal::lime.withAlpha (0.18f));
        g.fillRect (Rectangle<float> (x - 0.5f, fb.getY(), 1.0f, fb.getHeight()));
        const float y = field.pitchToY ((float) std::round (field.yToPitch (mousePos.y)));
        g.setColour (pal::lime.withAlpha (0.06f));
        g.fillRect (Rectangle<float> (fb.getX(), y - field.semitoneHeight() * 0.5f, fb.getWidth(), field.semitoneHeight()));
    }

    g.restoreState();
    ignoreUnused (toolName);
}

} // namespace bd
