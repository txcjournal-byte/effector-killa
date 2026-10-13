#include "PluginEditor.h"
#include "Presets.h"

namespace
{
const juce::Colour cBg     { 0xff15110e };
const juce::Colour cPanel  { 0xff221b16 };
const juce::Colour cCream  { 0xffeadfc8 };
const juce::Colour cDim    { 0xff8c7d6a };
const juce::Colour cAmber  { 0xffe39b3c };
const juce::Colour cRust   { 0xffb4512d };
const juce::Colour cShadow { 0xaa000000 };

juce::Font font (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), h,
                                          bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

// ---------------- Look ----------------

DustLook::DustLook()
{
    setColour (juce::Slider::textBoxTextColourId, cCream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, cPanel);
    setColour (juce::ComboBox::textColourId, cCream);
    setColour (juce::ComboBox::outlineColourId, cDim.withAlpha (0.4f));
    setColour (juce::ComboBox::arrowColourId, cAmber);
    setColour (juce::PopupMenu::backgroundColourId, cPanel);
    setColour (juce::PopupMenu::textColourId, cCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cAmber.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, cBg);
    setColour (juce::PopupMenu::headerTextColourId, cAmber);
    setColour (juce::TextButton::buttonColourId, cPanel);
    setColour (juce::TextButton::textColourOffId, cCream);
    setColour (juce::Label::textColourId, cCream);
}

void DustLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                 juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const bool big = radius > 50.0f;
    const float angle = start + pos * (end - start);

    // value arc
    const float arcR = radius - 2.0f;
    const float thick = big ? 6.0f : 3.5f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (cDim.withAlpha (0.25f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, angle, true);
    g.setColour (s.isEnabled() ? cAmber : cDim);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body: worn cream bakelite
    const float kr = radius - thick - (big ? 8.0f : 5.0f);
    g.setColour (cShadow);
    g.fillEllipse (centre.x - kr + 2, centre.y - kr + 4, kr * 2, kr * 2);
    juce::ColourGradient body (cCream.brighter (0.15f), centre.x - kr * 0.5f, centre.y - kr * 0.7f,
                               cCream.darker (0.55f), centre.x + kr * 0.6f, centre.y + kr, true);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2, kr * 2);
    g.setColour (cBg.withAlpha (0.35f));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2, kr * 2, 1.2f);

    // grip ridges
    g.setColour (cBg.withAlpha (0.10f));
    for (int i = 0; i < 24; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
        g.drawLine (centre.x + std::cos (a) * kr * 0.82f, centre.y + std::sin (a) * kr * 0.82f,
                    centre.x + std::cos (a) * kr * 0.98f, centre.y + std::sin (a) * kr * 0.98f, 1.0f);
    }

    // pointer
    juce::Path p;
    const float pw = big ? 4.0f : 2.6f;
    p.addRoundedRectangle (-pw * 0.5f, -kr * 0.92f, pw, kr * 0.45f, pw * 0.5f);
    g.setColour (cRust);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centre));
}

void DustLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.28f);
    g.setColour (b.getToggleState() ? cAmber : cDim.withAlpha (0.35f));
    g.fillEllipse (led);
    if (b.getToggleState())
    {
        g.setColour (cAmber.withAlpha (0.25f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (highlighted ? cCream : cCream.withAlpha (0.8f));
    g.setFont (font (12.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (4), juce::Justification::centredLeft);
}

void DustLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (cPanel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (cDim.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (cAmber);
    g.fillPath (arrow);
}

juce::Font DustLook::getComboBoxFont (juce::ComboBox&) { return font (14.0f); }
juce::Font DustLook::getPopupMenuFont() { return font (14.0f, false); }

void DustLook::drawPopupMenuBackground (juce::Graphics& g, int, int)
{
    g.fillAll (cPanel);
}

// ---------------- Panel ----------------

DustPanel::DustPanel (DustProcessor& p) : proc (p)
{
    setLookAndFeel (&look);

    const char* ids[]    = { IDs::age, IDs::tape, IDs::wobble, IDs::vinyl, IDs::crush, IDs::tone, IDs::width, IDs::mix, IDs::output };
    const char* labels[] = { "AGE", "TAPE", "WOBBLE", "VINYL", "CRUSH", "TONE", "WIDTH", "MIX", "OUTPUT" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setDoubleClickReturnValue (false, 0.0);
        k.slider.setPopupDisplayEnabled (false, false, nullptr);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.slider.setColour (juce::Slider::textBoxTextColourId, cCream);
        k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (k.slider);
        k.label.setText (labels[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (font (i == 0 ? 18.0f : 13.0f));
        k.label.setColour (juce::Label::textColourId, i == 0 ? cAmber : cCream);
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

    // static film grain texture, generated once
    grain = juce::Image (juce::Image::ARGB, baseW, baseH, true);
    {
        juce::Random r (1234);
        juce::Image::BitmapData bd (grain, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < baseH; ++y)
            for (int x = 0; x < baseW; ++x)
            {
                const auto v = (juce::uint8) r.nextInt (256);
                bd.setPixelColour (x, y, juce::Colour (v, v, v, (juce::uint8) 14));
            }
    }

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
    const float wob = (float) proc.apvts.getRawParameterValue (IDs::wobble)->load();
    const float age = (float) proc.apvts.getRawParameterValue (IDs::age)->load();
    const float speed = 0.045f * (1.0f - 0.35f * (wob + age * 0.5f) * (0.5f + 0.5f * std::sin (reelAngle * 0.7f)));
    reelAngle += speed;
    meterIn = juce::jmax (proc.meterIn.load(), meterIn * 0.85f);
    meterOut = juce::jmax (proc.meterOut.load(), meterOut * 0.85f);

    const float vinyl = (float) proc.apvts.getRawParameterValue (IDs::vinyl)->load();
    if (random.nextFloat() < (vinyl + age * 0.5f) * 0.12f)
        crackleFlash = 1.0f;
    crackleFlash *= 0.75f;

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        refreshPresetBox();
    repaint (0, 0, baseW, 150);
}

void DustPanel::drawReel (juce::Graphics& g, juce::Point<float> c, float r, float angle) const
{
    g.setColour (cBg);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (cCream.withAlpha (0.85f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.0f);
    // tape pack
    g.setColour (juce::Colour (0xff3a2a1e));
    g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.78f, r * 1.56f, r * 1.56f);
    // hub with 3 spokes
    g.setColour (cCream);
    g.fillEllipse (c.x - r * 0.32f, c.y - r * 0.32f, r * 0.64f, r * 0.64f);
    g.setColour (cBg);
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 3.0f;
        juce::Path tooth;
        tooth.addRectangle (-r * 0.05f, -r * 0.30f, r * 0.10f, r * 0.14f);
        g.fillPath (tooth, juce::AffineTransform::rotation (a).translated (c));
    }
    g.fillEllipse (c.x - r * 0.08f, c.y - r * 0.08f, r * 0.16f, r * 0.16f);
}

void DustPanel::paint (juce::Graphics& g)
{
    g.fillAll (cBg);

    // header strip
    auto header = juce::Rectangle<float> (0, 0, (float) baseW, 150);
    juce::ColourGradient hg (cPanel.brighter (0.06f), 0, 0, cBg, 0, 150, false);
    g.setGradientFill (hg);
    g.fillRect (header);

    // logo
    g.setColour (cCream);
    g.setFont (font (64.0f));
    g.drawText ("DUST", 28, 18, 260, 70, juce::Justification::centredLeft);
    g.setColour (cAmber);
    g.setFont (font (12.0f));
    g.drawText ("TRAPWIN  /  LO-FI & VINTAGE COLOR", 32, 84, 300, 16, juce::Justification::centredLeft);

    // cassette window with two reels
    auto win = juce::Rectangle<float> (330, 22, 300, 108);
    g.setColour (cBg.withAlpha (0.9f));
    g.fillRoundedRectangle (win, 10.0f);
    g.setColour (cDim.withAlpha (0.5f));
    g.drawRoundedRectangle (win, 10.0f, 1.5f);
    drawReel (g, { win.getX() + 72, win.getCentreY() }, 40.0f, reelAngle);
    drawReel (g, { win.getRight() - 72, win.getCentreY() }, 40.0f, reelAngle * 1.07f);
    // tape between reels
    g.setColour (juce::Colour (0xff3a2a1e));
    g.fillRect (win.getX() + 72, win.getBottom() - 16, win.getWidth() - 144, 3.0f);
    if (crackleFlash > 0.05f)
    {
        g.setColour (cAmber.withAlpha (crackleFlash * 0.6f));
        g.fillEllipse (win.getCentreX() - 3, win.getBottom() - 19, 6, 6);
    }

    // in/out meters
    auto drawMeter = [&] (float x, float level, const char* text)
    {
        const float h = 96.0f, top = 26.0f;
        g.setColour (cBg);
        g.fillRoundedRectangle (x, top, 10, h, 3.0f);
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float fill = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * h;
        g.setColour (db > -1.0f ? cRust : cAmber);
        g.fillRoundedRectangle (x, top + h - fill, 10, fill, 3.0f);
        g.setColour (cDim);
        g.setFont (font (10.0f));
        g.drawText (text, (int) x - 8, (int) (top + h + 2), 26, 12, juce::Justification::centred);
    };
    drawMeter (668, meterIn, "IN");
    drawMeter (700, meterOut, "OUT");

    // knob panels
    g.setColour (cPanel);
    g.fillRoundedRectangle (16, 164, 240, 260, 12.0f);
    g.fillRoundedRectangle (268, 164, 496, 260, 12.0f);

    g.drawImageAt (grain, 0, 0);
}

void DustPanel::resized()
{
    // header controls
    prevButton.setBounds (32, 112, 26, 26);
    presetBox.setBounds (62, 112, 196, 26);
    nextButton.setBounds (262, 112, 26, 26);

    // big AGE knob
    knobs[0].label.setBounds (16, 172, 240, 24);
    knobs[0].slider.setBounds (36, 196, 200, 190);
    follow.setBounds (60, 392, 160, 24);

    // 2 rows of 4 small knobs: TAPE WOBBLE VINYL CRUSH / TONE WIDTH MIX OUTPUT
    const int x0 = 280, colW = 118, row1 = 172, row2 = 296;
    for (int i = 1; i <= 8; ++i)
    {
        const int idx = i - 1;
        const int col = idx % 4, row = idx / 4;
        const int x = x0 + col * colW, y = row == 0 ? row1 : row2;
        knobs[(size_t) i].label.setBounds (x, y, colW, 18);
        knobs[(size_t) i].slider.setBounds (x + 9, y + 18, colW - 18, 100);
    }
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
