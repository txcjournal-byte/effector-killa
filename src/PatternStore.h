/*
    TrapWin BEND DEMON
    PatternStore.h - the message-thread owner of the line-sequencer pattern (2.0).

    * Holds the editable Pattern, the selection and a juce::UndoManager. Every edit goes through
      perform() (one undo step) or, for mouse drags, setLive() + commitDrag() (live preview while
      dragging, ONE undo step on mouse-up).
    * After every change the pattern is published to the audio thread through a lock-free
      PatternExchange (immutable snapshot, no locks or frees on the audio thread).
    * Lives in the processor, so the pattern, its undo history and the selection survive closing
      and reopening the editor.
*/
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>
#include "engine/Pattern.h"
#include "engine/PatternExchange.h"

namespace bd
{

class PatternStore final : public juce::ChangeBroadcaster
{
public:
    PatternStore();

    //==============================================================================
    /** Message thread. */
    const Pattern& get() const noexcept               { return current; }

    /** One undoable edit. Does nothing if `p` equals the current pattern. */
    void perform (Pattern p, const juce::String& actionName, bool isLoad = false);

    /** Counts whole-pattern loads (presets, capture, state restore): the editor fits its view. */
    uint32_t getLoadCount() const noexcept            { return loadCount; }

    /** Drag protocol: beginDrag() remembers the pattern; setLive() previews (audible, not
        undoable); commitDrag() records one undo step from the remembered pattern. */
    void beginDrag();
    void setLive (Pattern p);
    void commitDrag (const juce::String& actionName);
    void cancelDrag();
    bool isDragging() const noexcept                  { return dragging; }
    const Pattern& getDragStart() const noexcept      { return dragStart; }

    /** Replaces everything (state restore / preset load). Clears the undo history if asked. */
    void replaceAll (Pattern p, bool clearUndo);

    juce::UndoManager& getUndoManager() noexcept      { return undoManager; }
    bool undo();
    bool redo();

    //==============================================================================
    // Selection (line ids)
    const std::vector<uint32_t>& getSelection() const noexcept { return selection; }
    void setSelection (std::vector<uint32_t>);
    void select (uint32_t id, bool addToSelection);
    void deselect (uint32_t id);
    void clearSelection()                             { setSelection ({}); }
    bool isSelected (uint32_t id) const noexcept;
    void selectAll();

    /** Increments on every change (pattern or selection) - cheap "did anything change" check. */
    uint32_t getVersion() const noexcept              { return version; }

    //==============================================================================
    // State (any thread: guarded copy)
    std::unique_ptr<juce::XmlElement> createXml() const;
    Pattern copyThreadSafe() const;

    // Audio thread side
    PatternExchange& getExchange() noexcept           { return exchange; }
    void collectGarbage()                             { exchange.collectGarbage(); }

private:
    struct EditAction;
    void apply (const Pattern&);   // set + publish + notify
    void pruneSelection();

    Pattern current, dragStart;
    bool dragging = false;
    std::vector<uint32_t> selection;
    juce::UndoManager undoManager { 0, 200 };
    PatternExchange exchange;
    mutable juce::CriticalSection stateLock;   // only for getStateInformation from other threads
    uint32_t version = 1, loadCount = 0;
};

} // namespace bd
