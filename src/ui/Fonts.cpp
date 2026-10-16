#include "Fonts.h"
#include "BinaryData.h"

namespace bd::fonts
{

namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr unbMedium, unbBold, unbBlack, monoRegular, monoBold;

        Typefaces()
        {
            auto make = [] (const char* data, int size)
            {
                return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            };

            unbMedium   = make (BinaryData::UnboundedMedium_ttf,      BinaryData::UnboundedMedium_ttfSize);
            unbBold     = make (BinaryData::UnboundedBold_ttf,        BinaryData::UnboundedBold_ttfSize);
            unbBlack    = make (BinaryData::UnboundedBlack_ttf,       BinaryData::UnboundedBlack_ttfSize);
            monoRegular = make (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
            monoBold    = make (BinaryData::JetBrainsMonoBold_ttf,    BinaryData::JetBrainsMonoBold_ttfSize);
        }
    };

    const Typefaces& get()
    {
        static Typefaces t;   // created lazily on the message thread
        return t;
    }

    juce::Font fromTypeface (juce::Typeface::Ptr tf, float height, int fallbackStyle)
    {
        if (tf != nullptr)
            return juce::Font (juce::FontOptions (tf).withHeight (height));

        return juce::Font (juce::FontOptions (height, fallbackStyle));   // system sans fallback
    }
}

juce::Font display (float height, int weight)
{
    const auto& t = get();
    auto tf = weight <= 0 ? t.unbMedium : (weight == 1 ? t.unbBold : t.unbBlack);
    return fromTypeface (tf, height, juce::Font::bold);
}

juce::Font mono (float height, bool bold)
{
    const auto& t = get();
    return fromTypeface (bold ? t.monoBold : t.monoRegular, height, bold ? juce::Font::bold : juce::Font::plain);
}

} // namespace bd::fonts
