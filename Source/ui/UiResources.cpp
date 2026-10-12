#include "UiResources.h"
#include "EKBinaryData.h"

namespace ek::ui
{
using namespace juce;

namespace
{
// raw pointer only (no JUCE object): trivially destructible, safe at DLL unload
std::atomic<UiResources*> liveInstance { nullptr };
}

UiResources::UiResources() { liveInstance.store (this); }

UiResources::~UiResources()
{
    UiResources* expected = this;
    liveInstance.compare_exchange_strong (expected, nullptr);
}

UiResources* UiResources::current() noexcept { return liveInstance.load (std::memory_order_acquire); }

Typeface::Ptr UiResources::loadTypeface (const char* resourceName)
{
    int size = 0;
    if (const char* data = EKData::getNamedResource (resourceName, size))
        return Typeface::createSystemTypefaceFor (data, (size_t) size);
    return nullptr;
}

Image UiResources::loadLogo()
{
    // decoded directly (not through juce::ImageCache, which keeps its own copy and timer)
    return ImageFileFormat::loadFrom (EKData::trapwin_logo_png, (size_t) EKData::trapwin_logo_pngSize);
}

Typeface::Ptr UiResources::typeface (const char* resourceName)
{
    for (auto& f : faces)
        if (f.name == resourceName || std::strcmp (f.name, resourceName) == 0) return f.face;
    faces.push_back ({ resourceName, loadTypeface (resourceName) });   // names are string literals
    return faces.back().face;
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

Typeface::Ptr UiResources::getTypeface (const char* resourceName)
{
    if (auto* r = current()) return r->typeface (resourceName);
    return loadTypeface (resourceName);
}

Image UiResources::getLogo()
{
    if (auto* r = current()) return r->logo();
    return loadLogo();
}
} // namespace ek::ui
