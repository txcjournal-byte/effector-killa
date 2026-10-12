#include "Brand.h"
#include "Theme.h"
#include "UiResources.h"

namespace ek::ui::brand
{
using namespace juce;

Image logo()
{
    return ek::ui::UiResources::getLogo();   // shared, freed with the last editor
}

void paintLogoBlock (Graphics& g, Rectangle<float> r, bool tagline)
{
    const auto img = logo();
    const float aspect = img.isValid() ? (float) img.getWidth() / (float) img.getHeight() : 2.74f;

    // never redraw the logo as text - always the official artwork
    auto logoArea = r.removeFromTop (jmin (r.getWidth() / aspect, r.getHeight() * 0.62f));
    if (img.isValid())
    {
        g.setOpacity (1.0f);   // drawImage uses the current colour's alpha
        g.drawImage (img, logoArea, RectanglePlacement::centred);
    }

    auto main = tagline ? r.removeFromTop (r.getHeight() * 0.72f) : r;
    theme::drawChromeText (g, "EFFECTOR", main, theme::stencilFont (main.getHeight() * 1.02f), Justification::centred, 1.0f);
    if (tagline)
    {
        g.setFont (theme::monoLabel (jmax (8.0f, r.getHeight() * 0.62f)));
        g.setColour (theme::accent);
        g.drawText ("EVERY EFFECT. ONE WIN.", r, Justification::centred, false);
    }
}
} // namespace ek::ui::brand
