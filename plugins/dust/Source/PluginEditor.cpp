#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

namespace
{
const juce::Colour cBg     { 0xff15110e };
const juce::Colour cPanel  { 0xff221b16 };
const juce::Colour cCream  { 0xffeadfc8 };
const juce::Colour cDim    { 0xff8c7d6a };
const juce::Colour cAmber  { 0xffe39b3c };
const juce::Colour cRust   { 0xffb4512d };
const juce::Colour cLcd    { 0xfff0b25a };

juce::Font fontOf (const juce::Typeface::Ptr& tf, float h)
{
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

// knob centres (base coordinates) – must match tools/make_background.py
juce::Point<float> knobCentre (int i)
{
    if (i == 0) return { 136.0f, 287.0f };
    const int idx = i - 1, col = idx % 4, row = idx / 4;
    return { 280.0f + (float) col * 118.0f + 59.0f, (row == 0 ? 172.0f : 296.0f) + 62.0f };
}

// classic VU ballistics: dB -> fraction of the sweep
float vuFraction (float db)
{
    static const float dbs[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    static const float ts[] = { 0.00f, 0.17f, 0.29f, 0.39f, 0.52f, 0.59f, 0.66f, 0.74f, 0.82f, 0.90f, 1.00f };
    if (db <= dbs[0]) return juce::jmax (-0.04f, (db + 26.0f) / 6.0f * 0.0f);
    for (int i = 1; i < 11; ++i)
        if (db <= dbs[i])
            return ts[i - 1] + (ts[i] - ts[i - 1]) * (db - dbs[i - 1]) / (dbs[i] - dbs[i - 1]);
    return 1.04f;
}
} // namespace

const DustFonts& DustFonts::get()
{
    static DustFonts f {
        juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize),
        juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize)
    };
    return f;
}

// ---------------- Look ----------------

DustLook::DustLook()
{
    setColour (juce::ComboBox::textColourId, cCream);
    setColour (juce::PopupMenu::backgroundColourId, cPanel);
    setColour (juce::PopupMenu::textColourId, cCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cAmber);
    setColour (juce::PopupMenu::highlightedTextColourId, cBg);
    setColour (juce::PopupMenu::headerTextColourId, cAmber);
    setColour (juce::TextButton::textColourOffId, cCream);
    setColour (juce::Label::textColourId, cCream);
}

void DustLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                 juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float seatR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float capR = seatR * 0.80f;

    // value glow inside the seat ring
    const float arcR = seatR - (seatR - capR) * 0.45f;
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, angle, true);
    const float thick = juce::jmax (2.5f, (seatR - capR) * 0.55f);
    g.setColour (cAmber.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (thick * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.isEnabled() ? cAmber : cDim);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - capR + 1.5f, c.y - capR + capR * 0.12f, capR * 2.0f, capR * 2.0f);

    // chrome skirt
    const float skirtR = capR;
    juce::ColourGradient chrome (juce::Colour (0xffd9d2c4), c.x - skirtR, c.y - skirtR,
                                 juce::Colour (0xff4a423a), c.x + skirtR, c.y + skirtR, false);
    chrome.addColour (0.45, juce::Colour (0xff8e857a));
    chrome.addColour (0.55, juce::Colour (0xffbdb4a6));
    g.setGradientFill (chrome);
    g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2, skirtR * 2);
    // skirt grooves
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine (c.x + std::cos (a) * skirtR * 0.80f, c.y + std::sin (a) * skirtR * 0.80f,
                    c.x + std::cos (a) * skirtR * 0.98f, c.y + std::sin (a) * skirtR * 0.98f, 0.8f);
    }

    // bakelite cap
    const float r = capR * 0.74f;
    juce::ColourGradient cap (cCream.brighter (0.25f), c.x - r * 0.6f, c.y - r * 0.8f,
                              juce::Colour (0xffa89a80), c.x + r * 0.7f, c.y + r, true);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
    // specular highlight
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), c.x - r * 0.35f, c.y - r * 0.55f,
                                             juce::Colours::white.withAlpha (0.0f), c.x - r * 0.05f, c.y - r * 0.05f, true));
    g.fillEllipse (c.x - r * 0.8f, c.y - r * 0.9f, r * 1.1f, r * 0.9f);

    // pointer
    juce::Path p;
    const float pw = juce::jmax (2.2f, r * 0.11f);
    p.addRoundedRectangle (-pw * 0.5f, -r * 0.96f, pw, r * 0.55f, pw * 0.5f);
    g.setColour (cRust.darker (0.15f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

void DustLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
    if (b.getToggleState())
    {
        g.setColour (cAmber.withAlpha (0.28f));
        g.fillEllipse (led.expanded (3.5f));
    }
    g.setGradientFill (juce::ColourGradient (b.getToggleState() ? cAmber.brighter (0.4f) : juce::Colour (0xff3a2f26), led.getX(), led.getY(),
                                             b.getToggleState() ? cAmber.darker (0.3f) : juce::Colour (0xff1a1410), led.getRight(), led.getBottom(), false));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (led, 1.0f);
    g.setColour (highlighted ? cCream : cCream.withAlpha (0.8f));
    g.setFont (fontOf (DustFonts::get().label, 13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4), juce::Justification::centredLeft);
}

void DustLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (juce::Colour (0xff6e5c4a));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff0d0a08));
    g.fillRoundedRectangle (r.reduced (1.0f), 3.5f);
    // amber LCD glow
    g.setGradientFill (juce::ColourGradient (cLcd.withAlpha (0.10f), 0, 0, cLcd.withAlpha (0.02f), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (2.0f), 3.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cLcd);
    g.fillPath (arrow);
}

void DustLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cLcd);
}

juce::Font DustLook::getComboBoxFont (juce::ComboBox&) { return fontOf (DustFonts::get().mono, 15.0f); }
juce::Font DustLook::getPopupMenuFont() { return fontOf (DustFonts::get().label, 16.0f); }

void DustLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (cPanel);
    g.setColour (cDim.withAlpha (0.4f));
    g.drawRect (0, 0, w, h);
}

void DustLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff2a221b : 0xff4a3e33), 0, 0,
                                             juce::Colour (down ? 0xff3a2f26 : 0xff221b16), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r.reduced (0.5f), 4.0f);
    g.setColour (juce::Colour (over ? 0xff9a8670 : 0xff6e5c4a));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

juce::Font DustLook::getTextButtonFont (juce::TextButton&, int) { return fontOf (DustFonts::get().labelBold, 16.0f); }

// ---------------- Panel ----------------

DustPanel::DustPanel (DustProcessor& p) : proc (p)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    const char* ids[]    = { IDs::age, IDs::tape, IDs::wobble, IDs::vinyl, IDs::crush, IDs::tone, IDs::width, IDs::mix, IDs::output };
    const char* labels[] = { "AGE", "TAPE", "WOBBLE", "VINYL", "CRUSH", "TONE", "WIDTH", "MIX", "OUTPUT" };
    const char* tips[]   = { "One knob that ages everything at once: more tape, wobble, vinyl and a darker tone",
                             "Warm tape saturation with low-end head bump", "Wow & flutter: pitch drift of a worn tape",
                             "Crackle and hiss", "Bit depth and sample-rate reduction", "Dark to bright",
                             "Stereo width of the processed sound", "Dry / wet blend", "Output level" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setTooltip (tips[i]);
        k.slider.setVelocityBasedMode (false);
        k.slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.slider);
        k.label.setText (labels[i], juce::dontSendNotification);
        k.label.setJustificationType (i == 0 ? juce::Justification::centredLeft : juce::Justification::centred);
        k.label.setFont (fontOf (DustFonts::get().labelBold, i == 0 ? 22.0f : 16.0f));
        k.label.setColour (juce::Label::textColourId, i == 0 ? cAmber : cCream);
        k.label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);
    }

    followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::follow, follow);
    follow.setTooltip ("Vinyl crackle and hiss only play while audio is playing");
    addAndMakeVisible (follow);

    const auto& presets = dust::factoryPresets();
    juce::String lastCat;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (lastCat != presets[(size_t) i].category)
        {
            lastCat = presets[(size_t) i].category;
            presetBox.addSectionHeading (lastCat.toUpperCase());
        }
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    }
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram())
            proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);

    auto step = [this] (int dir)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + n) % n);
        refreshPresetBox();
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);
    refreshPresetBox();

    startTimerHz (30);
}

DustPanel::~DustPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DustPanel::refreshPresetBox()
{
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void DustPanel::timerCallback()
{
    const float wob = proc.apvts.getRawParameterValue (IDs::wobble)->load();
    const float age = proc.apvts.getRawParameterValue (IDs::age)->load();
    const float speed = 0.05f * (1.0f - 0.35f * juce::jlimit (0.0f, 1.0f, wob + age * 0.5f) * (0.5f + 0.5f * std::sin (reelAngle * 0.7f)));
    reelAngle += speed;

    // VU ballistics (~300 ms)
    auto toward = [] (float cur, float lin)
    {
        const float target = vuFraction (juce::Decibels::gainToDecibels (lin, -60.0f) + 6.0f);
        return cur + (target - cur) * 0.22f;
    };
    needleIn = toward (needleIn, proc.meterIn.load());
    needleOut = toward (needleOut, proc.meterOut.load());

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        refreshPresetBox();
    repaint (320, 10, 460, 140);
}

void DustPanel::drawReel (juce::Graphics& g, juce::Point<float> c, float r, float angle) const
{
    // tape pack
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a3526), c.x, c.y, juce::Colour (0xff22170f), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (float rr = r * 0.45f; rr < r; rr += 3.0f)
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.5f);
    // white hub with 6 teeth
    const float hr = r * 0.40f;
    g.setColour (cCream);
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
    g.setColour (juce::Colour (0xff15110e));
    for (int i = 0; i < 6; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 6.0f;
        juce::Path tooth;
        tooth.addRoundedRectangle (-hr * 0.12f, -hr * 0.92f, hr * 0.24f, hr * 0.32f, 1.0f);
        g.fillPath (tooth, juce::AffineTransform::rotation (a).translated (c));
    }
    g.fillEllipse (c.x - hr * 0.35f, c.y - hr * 0.35f, hr * 0.7f, hr * 0.7f);
    g.setColour (cCream.withAlpha (0.6f));
    g.fillEllipse (c.x - hr * 0.12f, c.y - hr * 0.12f, hr * 0.24f, hr * 0.24f);
}

void DustPanel::drawNeedle (juce::Graphics& g, juce::Rectangle<float> face, float frac) const
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (face.toNearestInt());
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + 30.0f);
    const float a = juce::degreesToRadians (-48.0f + 96.0f * juce::jlimit (-0.05f, 1.05f, frac));
    const juce::Point<float> tip (pivot.x + std::sin (a) * 56.0f, pivot.y - std::cos (a) * 56.0f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (pivot.x + 2, pivot.y + 1, tip.x + 2, tip.y + 1, 1.4f);
    g.setColour (juce::Colour (0xff1a1410));
    g.drawLine ({ pivot, tip }, 1.3f);
}

void DustPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // reels in the cassette window (centres match the background)
    drawReel (g, { 402.0f, 88.0f }, 30.0f, reelAngle);
    drawReel (g, { 558.0f, 88.0f }, 30.0f, reelAngle * 1.07f);
    // tape strand visible in the middle window
    g.setColour (juce::Colour (0xff3a2a1e));
    g.fillRect (438.0f, 102.0f, 84.0f, 4.0f);

    drawNeedle (g, { 646.0f, 18.0f, 120.0f, 56.0f }, needleIn);
    drawNeedle (g, { 646.0f, 80.0f, 120.0f, 56.0f }, needleOut);
}

void DustPanel::paintOverChildren (juce::Graphics& g)
{
    // values in the little LCD windows
    g.setFont (fontOf (DustFonts::get().mono, 13.0f));
    for (auto& k : knobs)
    {
        g.setColour (cLcd.withAlpha (0.12f));
        g.fillRect (k.valueBox.toFloat().reduced (1.0f));
        g.setColour (k.slider.isEnabled() ? cLcd : cDim);
        g.drawText (k.slider.getTextFromValue (k.slider.getValue()), k.valueBox, juce::Justification::centred);
    }
}

void DustPanel::resized()
{
    prevButton.setBounds (31, 108, 26, 26);
    presetBox.setBounds (61, 108, 198, 26);
    nextButton.setBounds (263, 108, 26, 26);

    for (int i = 0; i < (int) knobs.size(); ++i)
    {
        const auto c = knobCentre (i);
        const float seat = i == 0 ? 84.0f : 36.0f;
        knobs[(size_t) i].slider.setBounds (juce::Rectangle<float> (seat * 2, seat * 2).withCentre (c).toNearestInt());
        if (i == 0)
        {
            knobs[0].label.setBounds (30, 170, 80, 24);
            knobs[0].valueBox = { 136 - 38, 386, 76, 18 };
        }
        else
        {
            knobs[(size_t) i].label.setBounds ((int) c.x - 55, (int) c.y - 62, 110, 18);
            knobs[(size_t) i].valueBox = { (int) c.x - 32, (int) c.y + 42, 64, 17 };
        }
    }
    follow.setBounds (130, 172, 120, 20);
}

// ---------------- Editor ----------------

DustEditor::DustEditor (DustProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (DustPanel::baseW * 3 / 4, DustPanel::baseH * 3 / 4, DustPanel::baseW * 2, DustPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) DustPanel::baseW / DustPanel::baseH);
    setSize (DustPanel::baseW, DustPanel::baseH);
}

void DustEditor::resized()
{
    panel.setBounds (0, 0, DustPanel::baseW, DustPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / DustPanel::baseW));
}
