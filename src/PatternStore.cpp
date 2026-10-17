#include "PatternStore.h"

namespace bd
{

struct PatternStore::EditAction final : public juce::UndoableAction
{
    EditAction (PatternStore& s, Pattern b, Pattern a) : store (s), before (std::move (b)), after (std::move (a)) {}

    bool perform() override  { store.apply (after);  return true; }
    bool undo() override     { store.apply (before); return true; }
    int getSizeInUnits() override { return (int) (before.lines.size() + after.lines.size()) * 8 + 1; }

    PatternStore& store;
    Pattern before, after;
};

PatternStore::PatternStore()
{
    exchange.publish (current);
}

void PatternStore::apply (const Pattern& p)
{
    {
        const juce::ScopedLock sl (stateLock);
        current = p;
    }
    exchange.publish (current);
    pruneSelection();
    ++version;
    sendChangeMessage();
}

void PatternStore::perform (Pattern p, const juce::String& name, bool isLoad)
{
    if (isLoad) ++loadCount;
    p.normalise();
    if (p == current)
        return;

    if (dragging)
        cancelDrag();

    undoManager.beginNewTransaction (name);
    undoManager.perform (new EditAction (*this, current, std::move (p)), name);
}

void PatternStore::beginDrag()
{
    dragStart = current;
    dragging = true;
}

void PatternStore::setLive (Pattern p)
{
    if (! dragging)
        beginDrag();
    apply (p);
}

void PatternStore::commitDrag (const juce::String& name)
{
    if (! dragging)
        return;

    dragging = false;
    auto finalPattern = current;
    finalPattern.normalise();

    if (finalPattern == dragStart)
    {
        apply (finalPattern);
        return;
    }

    // restore the start silently, then perform the whole drag as one undo step
    {
        const juce::ScopedLock sl (stateLock);
        current = dragStart;
    }
    undoManager.beginNewTransaction (name);
    undoManager.perform (new EditAction (*this, dragStart, std::move (finalPattern)), name);
}

void PatternStore::cancelDrag()
{
    if (! dragging) return;
    dragging = false;
    apply (dragStart);
}

void PatternStore::replaceAll (Pattern p, bool clearUndo)
{
    p.normalise();
    dragging = false;
    ++loadCount;
    if (clearUndo)
    {
        undoManager.clearUndoHistory();
        selection.clear();
        apply (p);
    }
    else
    {
        perform (std::move (p), "Load pattern");
    }
}

bool PatternStore::undo()
{
    if (dragging) cancelDrag();
    return undoManager.undo();
}

bool PatternStore::redo()
{
    if (dragging) cancelDrag();
    return undoManager.redo();
}

//==============================================================================
void PatternStore::pruneSelection()
{
    selection.erase (std::remove_if (selection.begin(), selection.end(),
                                     [this] (uint32_t id) { return current.find (id) == nullptr; }),
                     selection.end());
}

void PatternStore::setSelection (std::vector<uint32_t> s)
{
    std::sort (s.begin(), s.end());
    s.erase (std::unique (s.begin(), s.end()), s.end());
    if (s == selection) return;
    selection = std::move (s);
    pruneSelection();
    ++version;
    sendChangeMessage();
}

void PatternStore::select (uint32_t id, bool add)
{
    auto s = add ? selection : std::vector<uint32_t> {};
    s.push_back (id);
    setSelection (std::move (s));
}

void PatternStore::deselect (uint32_t id)
{
    auto s = selection;
    s.erase (std::remove (s.begin(), s.end(), id), s.end());
    setSelection (std::move (s));
}

bool PatternStore::isSelected (uint32_t id) const noexcept
{
    return std::binary_search (selection.begin(), selection.end(), id);
}

void PatternStore::selectAll()
{
    std::vector<uint32_t> s;
    for (auto& l : current.lines) s.push_back (l.id);
    setSelection (std::move (s));
}

//==============================================================================
std::unique_ptr<juce::XmlElement> PatternStore::createXml() const
{
    const juce::ScopedLock sl (stateLock);
    return current.toXml();
}

Pattern PatternStore::copyThreadSafe() const
{
    const juce::ScopedLock sl (stateLock);
    return current;
}

} // namespace bd
