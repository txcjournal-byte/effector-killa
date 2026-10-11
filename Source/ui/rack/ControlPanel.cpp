#include "ControlPanel.h"

using namespace juce;

namespace ek::ui
{
void ModFallback::paint (Graphics& g)
{
    background.draw (g, getLocalBounds(), [this] (Graphics& bg)
    {
        bg.fillAll (hw::col::shaft);
        auto inner = hw::drawRackPanel (bg, getLocalBounds().toFloat(), Colour (0xff121419), 22.0f, 31);
        hw::drawPrint (bg, "MOD", inner.reduced (14.0f, 0.0f), Justification::centredLeft, 12.0f, hw::col::print);
        hw::drawPrint (bg, String::fromUTF8 ("LFO 1  \xc2\xb7  LFO 2  \xc2\xb7  ENVELOPE  \xc2\xb7  STEP SEQ"), inner, Justification::centred, 9.0f);
    });
}
} // namespace ek::ui
