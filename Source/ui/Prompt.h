#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ek::ui
{
// Implemented by the editor: a small modal "name" prompt (AlertWindow) that the editor owns, so it is
// closed with the window (a desktop window left behind by a closed plugin can hang the host on exit).
// onSave runs only while the editor is alive. Child components find it with
// findParentComponentOfClass<PromptOwner>().
class PromptOwner
{
public:
    virtual ~PromptOwner() = default;
    virtual void showNamePrompt (const juce::String& title, const juce::String& message, const juce::String& initialName,
                                 std::function<void (const juce::String& name)> onSave) = 0;
};
} // namespace ek::ui
