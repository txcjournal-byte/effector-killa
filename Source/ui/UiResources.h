#pragma once

// Process-wide UI resources (embedded typefaces, the logo, faceplate / miniature image caches).
//
// Lifetime rule: nothing JUCE-owned may outlive the last editor. Function-local statics holding
// Images / Typefaces were destroyed at DLL unload, after JUCE had shut down (Direct2D / DirectWrite
// objects released under the Windows loader lock -> the host froze on exit). Everything is now kept
// in one reference-counted holder: every editor (and the snapshot tool) owns a UiResourceHolder, so
// the shared UiResources object is created with the first editor and freed with the last one.
//
// Painting code calls UiResources::current(): the live instance, or nullptr when no holder exists
// (then the helpers create uncached objects - same pixels, just slower). Message thread only.

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <tuple>
#include <vector>
#include <cstring>

namespace ek::ui
{
class UiResources
{
public:
    UiResources();
    ~UiResources();

    // the live shared instance, nullptr when no holder is alive (message thread)
    static UiResources* current() noexcept;

    // embedded typeface by binary resource name ("osd_ttf", "vfd_ttf", "hand_ttf", "display_ttf"),
    // created lazily on first use; nullptr when the font is not embedded
    juce::Typeface::Ptr typeface (const char* resourceName);

    // the TrapWin logo (decoded lazily)
    juce::Image logo();

    // image caches (owned here so they die with the last editor)
    std::map<juce::int64, juce::Image> colorLayers, spaceLayers;   // faceplate static layers
    std::map<std::tuple<int, int, int>, juce::Image> miniatures;    // FX LIBRARY chips

    // helpers that work with or without a live instance
    static juce::Typeface::Ptr getTypeface (const char* resourceName);
    static juce::Image getLogo();
    static juce::Typeface::Ptr loadTypeface (const char* resourceName);   // uncached
    static juce::Image loadLogo();                                         // uncached

private:
    struct Face { const char* name = nullptr; juce::Typeface::Ptr face; };
    std::vector<Face> faces;   // a handful of fonts: linear search, no allocation per lookup
    juce::Image logoImage;
    bool logoLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UiResources)
};

// Hold one of these for as long as UI painting may happen (editor member, snapshot tool main()).
using UiResourceHolder = juce::SharedResourcePointer<UiResources>;
} // namespace ek::ui
