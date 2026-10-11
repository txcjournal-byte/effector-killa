#include "CabinetBackground.h"
#include "Layout.h"
#include "../Theme.h"
#include "EKBinaryData.h"

namespace ek::ui
{
using namespace juce;
namespace L = ek::layout;

static void label (Graphics& g, const String& t, Rectangle<float> r, float size, Justification j = Justification::centred,
                   Colour c = theme::silverDim)
{
    g.setFont (theme::monoLabel (size));
    g.setColour (c);
    g.drawText (t.toUpperCase(), r, j, false);
}

static void screw (Graphics& g, Point<float> c)
{
    ColourGradient m (Colour (0xffcfd6e3), c.x - 4, c.y - 4, Colour (0xff3a4152), c.x + 4, c.y + 4, false);
    g.setGradientFill (m);
    g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (c));
    g.setColour (Colour (0xff1a1f2a));
    g.drawLine (c.x - 3.0f, c.y + 1.0f, c.x + 3.0f, c.y - 1.0f, 1.3f);
}

static void chromeFrame (Graphics& g, Rectangle<float> r, float corner, float width)
{
    ColourGradient chrome (Colour (0xff9aa3b6), r.getX(), r.getY(), Colour (0xffe6eaf2), r.getX(), r.getBottom(), false);
    chrome.addColour (0.30, Colour (0xfff5f7fc));
    chrome.addColour (0.48, Colour (0xff6f7a90));
    chrome.addColour (0.50, Colour (0xff151b29));
    chrome.addColour (0.70, Colour (0xff8e98ab));
    g.setGradientFill (chrome);
    Path p;
    p.addRoundedRectangle (r, corner);
    g.strokePath (p, PathStrokeType (width));
}

// The official TrapWin logo (chrome TRAPWIN, four sparkle stars, "AUDIO · UNDERGROUND"),
// the same artwork the other TrapWin plugins use. Never redraw it as text.
static const Image& brandLogo()
{
    static const Image img = ImageCache::getFromMemory (EKData::trapwin_logo_png, EKData::trapwin_logo_pngSize);
    return img;
}

void CabinetBackground::paintLogo (Graphics& g, Rectangle<float> r, bool tagline)
{
    const auto& logo = brandLogo();
    const float aspect = logo.isValid() ? (float) logo.getWidth() / (float) logo.getHeight() : 2.74f;

    // logo on top at full width, product name under it
    auto logoArea = r.removeFromTop (jmin (r.getWidth() / aspect, r.getHeight() * 0.62f));
    if (logo.isValid())
        g.drawImage (logo, logoArea, RectanglePlacement::centred);

    auto main = tagline ? r.removeFromTop (r.getHeight() * 0.74f) : r;
    theme::drawChromeText (g, "EFFECTOR", main, theme::stencilFont (main.getHeight() * 1.02f), Justification::centred, 1.2f);
    if (tagline)
    {
        g.setFont (theme::monoLabel (r.getHeight() * 0.55f));
        g.setColour (theme::accent);
        g.drawText ("EVERY EFFECT. ONE WIN.", r, Justification::centred, false);
    }
}

Image CabinetBackground::renderLogo (int w, int h)
{
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);
    paintLogo (g, Rectangle<float> ((float) w, (float) h).reduced ((float) w * 0.04f, (float) h * 0.06f), false);
    return img;
}

void CabinetBackground::paint (Graphics& g)
{
    const Rectangle<float> all (0.0f, 0.0f, L::kWidth, L::kHeight);

    // ---- body: near-black with a faint brushed texture and blue ambient light ---------------
    g.setColour (theme::bg);
    g.fillRect (all);
    Random rnd (11);
    for (int i = 0; i < 900; ++i)
    {
        const float y = rnd.nextFloat() * L::kHeight;
        g.setColour (Colours::white.withAlpha (0.006f + rnd.nextFloat() * 0.012f));
        g.fillRect (0.0f, y, L::kWidth, 1.0f);
    }
    ColourGradient ambient (theme::accentDeep.withAlpha (0.16f), L::tvScreen.getCentreX(), L::tvScreen.getCentreY(),
                            Colours::transparentBlack, L::tvScreen.getCentreX() + 900.0f, L::tvScreen.getCentreY(), true);
    g.setGradientFill (ambient);
    g.fillRect (all);

    chromeFrame (g, all.reduced (7.0f), 22.0f, 5.0f);
    g.setColour (theme::line);
    g.drawRoundedRectangle (all.reduced (16.0f), 16.0f, 1.2f);

    // ---- left column --------------------------------------------------------------------------
    const Rectangle<float> logoPanel (34.0f, 40.0f, 342.0f, 222.0f);
    const Rectangle<float> killPanel (34.0f, 270.0f, 342.0f, 176.0f);
    const Rectangle<float> ratingPanel (34.0f, 452.0f, 342.0f, 110.0f);
    const Rectangle<float> buttonPanel (34.0f, 570.0f, 342.0f, 106.0f);
    const Rectangle<float> cablePanel (34.0f, 684.0f, 342.0f, 84.0f);
    const Rectangle<float> staffPanel (34.0f, 776.0f, 116.0f, 132.0f);
    const Rectangle<float> knobPanel (158.0f, 776.0f, 218.0f, 132.0f);
    for (auto r : { logoPanel, killPanel, ratingPanel, buttonPanel, cablePanel, staffPanel, knobPanel })
        theme::drawPanel (g, r, 14.0f);

    paintLogo (g, logoPanel.reduced (22.0f, 26.0f), true);

    label (g, "PG", { L::ratingLed[0].x - 40.0f, 466.0f, 80.0f, 18.0f }, 15.0f);
    label (g, "R", { L::ratingLed[1].x - 40.0f, 466.0f, 80.0f, 18.0f }, 15.0f);
    label (g, "UNRATED", { L::ratingLed[2].x - 50.0f, 466.0f, 100.0f, 18.0f }, 15.0f);
    label (g, "KILL RATING", { 48.0f, 544.0f, 140.0f, 14.0f }, 11.0f, Justification::centredLeft, theme::accentSoft);

    g.setFont (theme::monoLabel (15.0f));
    g.setColour (theme::silverDim);
    g.drawFittedText ("BASIC\nCABLE", Rectangle<int> (46, 700, 92, 50), Justification::centred, 2);
    g.drawFittedText ("PREMIUM\nCABLE", Rectangle<int> (288, 700, 84, 50), Justification::centred, 2);

    g.drawFittedText ("STAFF\nPICK", Rectangle<int> (40, 856, 110, 44), Justification::centred, 2);
    label (g, "ANTENNA IN", { L::antennaIn.x - 60.0f, 878.0f, 120.0f, 18.0f }, 14.0f);
    label (g, "RF OUT", { L::rfOut.x - 50.0f, 878.0f, 100.0f, 18.0f }, 14.0f);

    // ---- TV --------------------------------------------------------------------------------------
    const auto bezel = L::tvScreen.expanded (34.0f, 30.0f);
    {
        ColourGradient plastic (Colour (0xff151b26), bezel.getX(), bezel.getY(), Colour (0xff05070b), bezel.getX(), bezel.getBottom(), false);
        g.setGradientFill (plastic);
        g.fillRoundedRectangle (bezel, 34.0f);
        chromeFrame (g, bezel, 34.0f, 3.0f);
        // inner shadow ring around the glass
        g.setColour (Colours::black);
        g.fillRoundedRectangle (L::tvScreen.expanded (8.0f), L::tvCorner + 6.0f);
        g.setColour (theme::accent.withAlpha (0.25f));
        g.drawRoundedRectangle (L::tvScreen.expanded (8.0f), L::tvCorner + 6.0f, 1.5f);
        for (auto c : { bezel.getTopLeft().translated (16, 16), bezel.getTopRight().translated (-16, 16),
                        bezel.getBottomLeft().translated (16, -16), bezel.getBottomRight().translated (-16, -16) })
            screw (g, c);
        label (g, "TRAPWIN  TRINITRAP", { bezel.getCentreX() - 150.0f, bezel.getBottom() - 24.0f, 300.0f, 16.0f }, 12.0f,
               Justification::centred, theme::silverDim.withAlpha (0.7f));
    }

    const Rectangle<float> avPanel (404.0f, 570.0f, 648.0f, 80.0f);
    const Rectangle<float> offAirPanel (1060.0f, 570.0f, 266.0f, 80.0f);
    theme::drawPanel (g, avPanel, 14.0f);
    theme::drawPanel (g, offAirPanel, 14.0f);
    label (g, "AV1 REMOTE", L::avLabels[0], 14.0f);
    label (g, "AV2 SCRIBBLE", L::avLabels[1], 14.0f);
    label (g, "AV3 SCREENSAVER", L::avLabels[2], 14.0f);
    label (g, "BYPASS", { 1072.0f, 578.0f, 80.0f, 14.0f }, 11.0f, Justification::centredLeft, theme::accentSoft);

    // ---- right column -----------------------------------------------------------------------------
    const Rectangle<float> chanPanel (1342.0f, 30.0f, 258.0f, 250.0f);
    const Rectangle<float> macroPanel (1342.0f, 286.0f, 258.0f, 364.0f);
    theme::drawPanel (g, chanPanel, 14.0f);
    theme::drawPanel (g, macroPanel, 14.0f);
    g.setFont (theme::monoLabel (19.0f));
    for (int ch = 0; ch < 12; ++ch)
    {
        const float a = degreesToRadians (180.0f + (float) ch * 30.0f);
        const Point<float> c (L::channelKnob.x + L::channelNumberRadius * std::sin (a), L::channelKnob.y - L::channelNumberRadius * std::cos (a));
        g.setColour (theme::silver);
        g.drawText (String (ch + 1).paddedLeft ('0', 2), Rectangle<float> (40.0f, 24.0f).withCentre (c), Justification::centred);
    }
    label (g, "PROGRAM", L::programLabel, 15.0f, Justification::centred, theme::silver);

    static const char* macroNames[] = { "VILLAIN ARC", "CRASH OUT", "AURA", "DRIP", "KNOCK" };
    for (int m = 0; m < 5; ++m)
    {
        // chrome plates like the trapwin.com buttons
        auto r = L::macroLabel[m].reduced (2.0f, 5.0f);
        ColourGradient plate (Colour (0xfff4f6fb), 0.0f, r.getY(), Colour (0xffe6eaf2), 0.0f, r.getBottom(), false);
        plate.addColour (0.48, Colour (0xffc9d0dd));
        plate.addColour (0.52, Colour (0xff8e98ab));
        g.setColour (theme::accentDeep.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (4.0f), r.getHeight() * 0.5f + 4.0f);
        g.setGradientFill (plate);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (theme::ink);
        g.setFont (theme::stencilFont (19.0f).withHorizontalScale (0.78f));
        g.drawText (macroNames[m], r, Justification::centred, false);
    }

    // ---- shelf + video recorder -----------------------------------------------------------------
    theme::drawPanel (g, L::shelf.expanded (10.0f, 10.0f), 14.0f);
    label (g, "TAPES", { L::shelf.getX(), L::shelf.getY() - 8.0f, 80.0f, 12.0f }, 10.0f, Justification::centredLeft, theme::accentSoft);
    const Rectangle<float> vcrPanel (1030.0f, 664.0f, 570.0f, 244.0f);
    theme::drawPanel (g, vcrPanel, 14.0f);
    {
        auto body = L::vcrBody;
        ColourGradient metal (Colour (0xff1c2331), body.getX(), body.getY(), Colour (0xff0a0e16), body.getX(), body.getBottom(), false);
        g.setGradientFill (metal);
        g.fillRoundedRectangle (body, 8.0f);
        chromeFrame (g, body, 8.0f, 2.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        for (float y = body.getY() + 4.0f; y < body.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, body.getX() + 4.0f, body.getRight() - 4.0f);
        label (g, "TRAPWIN  HQ-VHS", { body.getX() + 14.0f, body.getY() + 6.0f, 200.0f, 16.0f }, 11.0f, Justification::centredLeft, theme::silverDim);
    }
}

} // namespace ek::ui
