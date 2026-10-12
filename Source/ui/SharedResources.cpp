#include "SharedResources.h"
#include "EKBinaryData.h"

namespace ek::ui
{
using namespace juce;

namespace
{
// Plain pointer (not a JUCE object): set / cleared by the single live instance, message thread only.
UiResources* liveInstance = nullptr;

Typeface::Ptr loadTypeface (const char* resourceName)
{
    int size = 0;
    if (const char* data = EKData::getNamedResource (resourceName, size))
        return Typeface::createSystemTypefaceFor (data, (size_t) size);
    return nullptr;
}

Image loadLogo()
{
    // decoded directly (not through juce::ImageCache, whose singleton + timer would keep a reference)
    return ImageFileFormat::loadFrom (EKData::trapwin_logo_png, (size_t) EKData::trapwin_logo_pngSize);
}
} // namespace

UiResources::UiResources()
{
    jassert (liveInstance == nullptr);
    liveInstance = this;
}

UiResources::~UiResources()
{
    clear();
    if (liveInstance == this) liveInstance = nullptr;
}

UiResources* UiResources::get() noexcept { return liveInstance; }
int UiResources::getNumLiveInstances() noexcept { return liveInstance != nullptr ? 1 : 0; }

void UiResources::trackDialog (Component* dialog, const void* owner)
{
    if (dialog == nullptr) return;
    dialogs.erase (std::remove_if (dialogs.begin(), dialogs.end(), [] (const Dialog& d) { return d.window == nullptr; }),
                   dialogs.end());
    dialogs.push_back ({ owner, dialog });
}

void UiResources::closeDialogs (const void* owner)
{
    // detach first: deleting a window may run code that tracks / closes other dialogs
    std::vector<Component::SafePointer<Component>> toClose;
    for (auto it = dialogs.begin(); it != dialogs.end();)
    {
        if (owner == nullptr || it->owner == owner) { toClose.push_back (it->window); it = dialogs.erase (it); }
        else ++it;
    }
    // Deleting a modal component cancels its modal state; JUCE then runs its callback (res = 0)
    // asynchronously and does not delete it a second time.
    for (auto& w : toClose) w.deleteAndZero();
}

int UiResources::getNumOpenDialogs() const
{
    int n = 0;
    for (const auto& d : dialogs) n += d.window != nullptr ? 1 : 0;
    return n;
}

void UiResources::clear()
{
    closeDialogs (nullptr);
    imageCaches.clear();
    typefaces.clear();
    logoImage = {};
    logoLoaded = false;
}

Typeface::Ptr UiResources::typeface (const char* resourceName)
{
    const String key (resourceName);
    if (auto it = typefaces.find (key); it != typefaces.end()) return it->second;
    return typefaces.emplace (key, loadTypeface (resourceName)).first->second;   // nullptr cached too
}

Image UiResources::logo()
{
    if (! logoLoaded)
    {
        logoImage = loadLogo();
        logoLoaded = true;
    }
    return logoImage;
}

std::map<int64, Image>& UiResources::imageCache (const char* cacheName)
{
    return imageCaches[String (cacheName)];
}

void trackDialog (Component* dialog, const void* owner)
{
    if (auto* r = UiResources::get()) r->trackDialog (dialog, owner);
}

void closeEditorOverlays (const void* owner)
{
    // Menus without a target component (settings / info) would otherwise stay open after the editor
    // is gone. Their callbacks receive 0 and use SafePointers, so dismissing them is harmless.
    PopupMenu::dismissAllActiveMenus();
    if (auto* r = UiResources::get()) r->closeDialogs (owner);
}

Typeface::Ptr embeddedTypeface (const char* resourceName)
{
    if (auto* r = UiResources::get()) return r->typeface (resourceName);
    return loadTypeface (resourceName);
}

Image logoImage()
{
    if (auto* r = UiResources::get()) return r->logo();
    return loadLogo();
}
} // namespace ek::ui
