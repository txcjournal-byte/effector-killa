#pragma once

// UI resources shared by every open editor of this plugin (fonts, the logo, pre-rendered image layers).
//
// Why: function-local statics holding juce::Typeface / juce::Image survive until the plugin binary is
// unloaded, long after JUCE's GUI shutdown. On Windows those objects wrap DirectWrite / Direct2D COM
// resources; releasing them from DllMain at DLL_PROCESS_DETACH (under the loader lock, after JUCE has torn
// down its factories) can hang or crash the host - e.g. FL Studio freezing on exit with the editor open.
//
// Rule: no JUCE object in a static. Everything cached lives in ONE UiResources instance, held by a
// UiResourceHolder (juce::SharedResourcePointer) in every editor. When the last editor closes, the
// instance and everything it cached is released - while JUCE is still fully alive.
//
// Code that paints outside an editor's lifetime (offline tools, tests) still works: without a live
// instance the helpers simply load / render uncached.
//
// Message thread only.

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

namespace ek::ui
{
class UiResources
{
public:
    UiResources();
    ~UiResources();

    // The live instance while at least one UiResourceHolder exists, nullptr otherwise.
    static UiResources* get() noexcept;

    // Embedded font from the binary data ("osd_ttf", "vfd_ttf", ...), created once per instance.
    juce::Typeface::Ptr typeface (const char* resourceName);

    // Official logo artwork (decoded once per instance).
    juce::Image logo();

    // Named image cache (e.g. the static layers of the faceplates), keyed by a caller-defined int64.
    std::map<juce::int64, juce::Image>& imageCache (const char* cacheName);

    void clear();   // releases everything (also done by the destructor)

    // Number of live instances (0 or 1) - used by the lifecycle tests.
    static int getNumLiveInstances() noexcept;

    // Modal dialogs launched by an editor, closed when that editor is destroyed (see closeEditorOverlays).
    void trackDialog (juce::Component* dialog, const void* owner);
    void closeDialogs (const void* owner);
    int getNumOpenDialogs() const;

private:
    struct Dialog { const void* owner; juce::Component::SafePointer<juce::Component> window; };
    std::vector<Dialog> dialogs;
    std::map<juce::String, juce::Typeface::Ptr> typefaces;
    std::map<juce::String, std::map<juce::int64, juce::Image>> imageCaches;
    juce::Image logoImage;
    bool logoLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UiResources)
};

// Keep one of these alive for as long as UI may be painted (a member of the editor).
using UiResourceHolder = juce::SharedResourcePointer<UiResources>;

// ---- helpers (work with or without a live instance) --------------------------------------------

// Registers a modal dialog (e.g. an AlertWindow in enterModalState) that belongs to the editor of
// `owner` (the processor). Its modal callback must cope with res == 0 after the editor is gone.
void trackDialog (juce::Component* dialog, const void* owner);

// Call from the editor's destructor: dismisses open popup menus and deletes the dialogs of `owner`,
// so nothing outlives the editor (an orphaned window or a callback into a dead editor / processor).
void closeEditorOverlays (const void* owner);

juce::Typeface::Ptr embeddedTypeface (const char* resourceName);
juce::Image logoImage();

// Returns the cached image for `key` in cache `cacheName`, rendering it with make() on a miss.
// The cache is cleared when it grows beyond maxEntries. Without a live UiResources: make() every time.
template <typename MakeFn>
juce::Image cachedImage (const char* cacheName, juce::int64 key, size_t maxEntries, MakeFn&& make)
{
    auto* res = UiResources::get();
    if (res == nullptr) return make();
    auto& cache = res->imageCache (cacheName);
    if (auto it = cache.find (key); it != cache.end()) return it->second;
    if (cache.size() > maxEntries) cache.clear();
    return cache.emplace (key, make()).first->second;
}
} // namespace ek::ui
