#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

namespace
{
const juce::Colour cBg   { 0xff120d19 };
const juce::Colour cText { 0xffeee4f4 };
const juce::Colour cInk  { 0xff9686aa };
const juce::Colour cMag  { 0xffff3fa4 };
const juce::Colour cCyan { 0xff4ff0ff };
const juce::Colour cDeep { 0xff1d1427 };

struct Fonts
{
    juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    juce::Typeface::Ptr logo = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    static const Fonts& get() { static Fonts f; return f; }
};

juce::Font fnt (const juce::Typeface::Ptr& t, float h) { return juce::Font (juce::FontOptions (t).withHeight (h)); }

juce::String noteName (float hz)
{
    if (hz <= 0.0f) return "--";
    const int n = juce::roundToInt (vox::hzToMidi (hz));
    return juce::String (vox::keyNames[(size_t) ((n % 12 + 12) % 12)]) + juce::String (n / 12 - 1);
}

const juce::Point<float> knobCentres[] = { { 150, 232 }, { 670, 232 }, { 560, 418 }, { 660, 418 }, { 760, 418 } };
} // namespace

// ---------------- Look ----------------

VoxLook::VoxLook()
{
    setColour (juce::ComboBox::textColourId, cText);
    setColour (juce::PopupMenu::backgroundColourId, cDeep);
    setColour (juce::PopupMenu::textColourId, cText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cMag);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, cMag);
    knobBig = juce::ImageCache::getFromMemory (BinaryData::knob_big_png, BinaryData::knob_big_pngSize);
    knobSmall = juce::ImageCache::getFromMemory (BinaryData::knob_small_png, BinaryData::knob_small_pngSize);
}

void VoxLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto c = juce::Rectangle<int> (x, y, w, h).toFloat().getCentre();
    const float R = juce::jmin (w, h) * 0.5f;
    const bool big = R > 60.0f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const float angle = start + pos * (end - start);

    if (big)
    {
        // magenta value arc in the dark ring around the cap (from 0 for bipolar dials)
        const float arcR = R - 8.0f;
        const float a0 = bipolar ? (start + end) * 0.5f : start;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (a0, angle), juce::jmax (a0, angle), true);
        g.setColour (cMag.withAlpha (0.22f));
        g.strokePath (arc, juce::PathStrokeType (14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (cMag);
        g.strokePath (arc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        // LED dots (unlit ones are in the background)
        const int lit = juce::roundToInt (pos * 20.0f);
        for (int k = 0; k <= lit; ++k)
        {
            const float a = juce::degreesToRadians (225.0f - 270.0f * (float) k / 20.0f);
            const float lx = c.x + std::cos (a) * 38.0f, ly = c.y - std::sin (a) * 38.0f;
            g.setColour (cMag.withAlpha (0.25f));
            g.fillEllipse (lx - 4.0f, ly - 4.0f, 8.0f, 8.0f);
            g.setColour (cMag);
            g.fillEllipse (lx - 1.9f, ly - 1.9f, 3.8f, 3.8f);
        }
    }

    // photoreal knob (pre-rendered, lathe-brushed aluminium); only the pointer rotates
    const float body = big ? R - 4.0f : 30.0f;               // visible radius of the knob body
    const float imgHalf = body / 0.80f;                       // the image has shadow room around the body
    const auto& img = big ? knobBig : knobSmall;
    g.drawImage (img, juce::Rectangle<float> (imgHalf * 2.0f, imgHalf * 2.0f).withCentre (c), juce::RectanglePlacement::stretchToFit);
    const float topR = imgHalf * 0.62f;
    juce::Path p;
    p.addRoundedRectangle (-1.8f, -topR * 0.92f, 3.6f, topR * 0.42f, 1.8f);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c);
    g.setColour (cMag.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (3.0f), tr);
    g.setColour (s.isEnabled() ? cMag : cInk);
    g.fillPath (p, tr);
}

void VoxLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (juce::Colour (0xff4a3a5c));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colour (0xff0b0710));
    g.fillRoundedRectangle (r.reduced (1.0f), 4.5f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (cMag);
    g.fillPath (arrow);
}

void VoxLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, cText);
}

juce::Font VoxLook::getComboBoxFont (juce::ComboBox&) { return fnt (Fonts::get().semi, 17.0f); }
juce::Font VoxLook::getPopupMenuFont() { return fnt (Fonts::get().semi, 17.0f); }

void VoxLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    if (on)
    {
        g.setColour (cMag.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    }
    g.setGradientFill (juce::ColourGradient (on ? cMag.brighter (0.15f) : juce::Colour (down ? 0xff241a2e : 0xff342a40), 0, 0,
                                             on ? cMag.darker (0.35f) : juce::Colour (0xff1a1322), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? cMag.brighter (0.5f) : juce::Colour (over ? 0xff6a5a7e : 0xff4a3a5c));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void VoxLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colours::white : cText.withAlpha (0.85f));
    g.setFont (fnt (Fonts::get().label, 15.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void VoxLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.24f);
    if (b.getToggleState())
    {
        g.setColour (cMag.withAlpha (0.3f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (b.getToggleState() ? cMag : juce::Colour (0xff3a2e46));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (led, 1.0f);
    g.setColour (over ? cText : cText.withAlpha (0.85f));
    g.setFont (fnt (Fonts::get().label, 15.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6), juce::Justification::centredLeft);
}

// ---------------- KeyPicker ----------------

KeyPicker::KeyPicker (VoxProcessor& p)
    : proc (p), attachment (*p.apvts.getParameter (IDs::key), [this] (float) { repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> KeyPicker::keyRect (int pc) const
{
    static const int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const float blackPos[12] = { 0, 0.68f, 0, 1.70f, 0, 0, 3.66f, 0, 4.68f, 0, 5.70f, 0 };
    const auto b = getLocalBounds().toFloat();
    const float ww = b.getWidth() / 7.0f;
    if (whiteIndex[pc] >= 0)
        return { b.getX() + ww * (float) whiteIndex[pc], b.getY(), ww, b.getHeight() };
    return { b.getX() + ww * blackPos[pc], b.getY(), ww * 0.64f, b.getHeight() * 0.6f };
}

int KeyPicker::keyAt (juce::Point<float> pt) const
{
    for (int pc : { 1, 3, 6, 8, 10 })
        if (keyRect (pc).contains (pt)) return pc;
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 })
        if (keyRect (pc).contains (pt)) return pc;
    return -1;
}

void KeyPicker::mouseDown (const juce::MouseEvent& e)
{
    const int pc = keyAt (e.position);
    if (pc >= 0)
        attachment.setValueAsCompleteGesture ((float) pc);
}

void KeyPicker::paint (juce::Graphics& g)
{
    const int key = (int) proc.apvts.getRawParameterValue (IDs::key)->load();
    const int scale = (int) proc.apvts.getRawParameterValue (IDs::scale)->load();
    const bool scaleMode = (int) proc.apvts.getRawParameterValue (IDs::tune)->load() == 2;
    auto inScale = [&] (int pc)
    {
        const float snapped = vox::snapToScale ((float) (60 + pc), key, scale, false);
        return std::abs (snapped - (float) (60 + pc)) < 0.01f;
    };
    auto drawKey = [&] (int pc, bool black)
    {
        auto r = keyRect (pc).reduced (black ? 0.0f : 1.0f, 0.0f);
        juce::Colour col = black ? juce::Colour (0xff17101e) : juce::Colour (0xffe7dfee);
        if (scaleMode && inScale (pc)) col = black ? juce::Colour (0xff5a2a48) : juce::Colour (0xfff5c8e2);
        if (pc == key) col = cMag;
        g.setGradientFill (juce::ColourGradient (col.brighter (0.12f), r.getX(), r.getY(), col.darker (0.25f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        if (pc == key)
        {
            g.setColour (juce::Colours::white);
            g.setFont (fnt (Fonts::get().label, 13.0f));
            g.drawText (vox::keyNames[(size_t) pc], r.removeFromBottom (18.0f), juce::Justification::centred);
        }
    };
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) drawKey (pc, false);
    for (int pc : { 1, 3, 6, 8, 10 }) drawKey (pc, true);
}

// ---------------- Panel ----------------

VoxPanel::VoxPanel (VoxProcessor& p) : proc (p), keys (p)
{
    setLookAndFeel (&look);
    background = juce::ImageCache::getFromMemory (BinaryData::bg_png, BinaryData::bg_pngSize);

    const char* ids[] = { IDs::pitch, IDs::formant, IDs::retune, IDs::mix, IDs::output };
    const char* tips[] = { "Pitch shift in semitones (double-click = 0)", "Voice colour: down = bigger / darker, up = smaller / brighter (double-click = 0)",
                           "How fast the tuning grabs the note: HARD = robotic, slow = natural", "Dry / wet blend (50 % = layer under the original)", "Output level" };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.s.setTooltip (tips[i]);
        if (i < 2)
        {
            k.s.getProperties().set ("bipolar", true);
            k.s.setDoubleClickReturnValue (true, 0.0);
        }
        k.s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (k.s);
        k.a = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.s);
    }

    const char* tuneNames[] = { "OFF", "CHROMATIC", "SCALE" };
    auto* tuneParam = proc.apvts.getParameter (IDs::tune);
    tuneAttachment = std::make_unique<juce::ParameterAttachment> (*tuneParam, [this] (float v)
    {
        tuneMode = juce::roundToInt (v);
        refreshTuneButtons();
        keys.repaint();
    }, nullptr);
    for (int i = 0; i < 3; ++i)
    {
        auto& b = tuneButtons[(size_t) i];
        b.setButtonText (tuneNames[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { tuneAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    tuneAttachment->sendInitialUpdate();

    addAndMakeVisible (keys);
    for (int i = 0; i < vox::numScales; ++i)
        scaleBox.addItem (vox::scaleNames[(size_t) i], i + 1);
    addAndMakeVisible (scaleBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, IDs::scale, scaleBox);
    scaleBox.onChange = [this] { keys.repaint(); };

    midiToggle.setTooltip ("Hold MIDI notes on this track: the voice is tuned to the note you play");
    addAndMakeVisible (midiToggle);
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, IDs::midi, midiToggle);

    const auto& presets = vox::factoryPresets();
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
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getCurrentProgram()) proc.setCurrentProgram (idx);
    };
    addAndMakeVisible (presetBox);
    auto step = [this] (int dir)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + n) % n);
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    };
    prevButton.onClick = [step] { step (-1); };
    nextButton.onClick = [step] { step (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    startTimerHz (30);
}

VoxPanel::~VoxPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxPanel::refreshTuneButtons()
{
    for (int i = 0; i < 3; ++i)
        tuneButtons[(size_t) i].setToggleState (i == tuneMode, juce::dontSendNotification);
    const bool scale = tuneMode == 2;
    scaleBox.setEnabled (scale);
    knobs[2].s.setEnabled (tuneMode != 0 || midiToggle.getToggleState());
}

void VoxPanel::timerCallback()
{
    const float det = proc.uiDetected.load(), tgt = proc.uiTarget.load();
    shownDetected = det;
    shownTarget = tgt;
    if (det > 0.0f)
    {
        const float m = vox::hzToMidi (det);
        const float cents = (m - std::round (m)) * 100.0f;
        shownCents += 0.35f * (cents - shownCents);
    }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    refreshTuneButtons();
    repaint (300, 108, 220, 222);
}

void VoxPanel::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    // ---- note display
    const bool voiced = shownDetected > 0.0f;
    g.setFont (fnt (Fonts::get().mono, 44.0f));
    g.setColour (voiced ? cText : cInk.withAlpha (0.5f));
    g.drawText (noteName (shownDetected), juce::Rectangle<int> (310, 140, 100, 56), juce::Justification::centred);
    const bool tuning = tuneMode != 0 || midiToggle.getToggleState();
    g.setColour (voiced ? (tuning ? cMag : cInk) : cInk.withAlpha (0.5f));
    g.drawText (tuning ? noteName (shownTarget) : juce::String ("--"), juce::Rectangle<int> (410, 140, 100, 56), juce::Justification::centred);
    g.setFont (fnt (Fonts::get().mono, 14.0f));
    g.setColour (cInk);
    g.drawText (voiced ? juce::String (shownDetected, 1) + " Hz" : juce::String ("no pitch"), juce::Rectangle<int> (310, 196, 100, 18), juce::Justification::centred);
    g.drawText (juce::String ("->"), juce::Rectangle<int> (395, 156, 30, 24), juce::Justification::centred);

    // cents needle
    if (voiced)
    {
        const float x = 410.0f + juce::jlimit (-50.0f, 50.0f, shownCents) * 1.7f;
        const bool inTune = std::abs (shownCents) < 8.0f;
        const auto col = inTune ? cCyan : cMag;
        g.setColour (col.withAlpha (0.25f));
        g.fillRoundedRectangle (x - 6.0f, 226.0f, 12.0f, 34.0f, 4.0f);
        g.setColour (col);
        g.fillRoundedRectangle (x - 2.0f, 228.0f, 4.0f, 30.0f, 2.0f);
        g.setFont (fnt (Fonts::get().label, 13.0f));
        g.drawText (inTune ? "IN TUNE" : (shownCents > 0 ? "SHARP" : "FLAT"), juce::Rectangle<int> (360, 204, 100, 18), juce::Justification::centred);
    }

    // ---- MIDI hint
    if (midiToggle.getToggleState())
    {
        g.setColour (cInk);
        g.setFont (fnt (Fonts::get().semi, 13.0f));
        g.drawText ("play notes on this track", juce::Rectangle<int> (322, 446, 150, 16), juce::Justification::centredLeft);
    }
}

void VoxPanel::paintOverChildren (juce::Graphics& g)
{
    g.setFont (fnt (Fonts::get().mono, 15.0f));
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const auto c = knobCentres[i];
        auto& k = knobs[i];
        g.setColour (k.s.isEnabled() ? cText : cInk.withAlpha (0.5f));
        const float dy = i < 2 ? 98.0f : 50.0f;
        g.drawText (k.s.getTextFromValue (k.s.getValue()), juce::Rectangle<float> (110.0f, 18.0f).withCentre ({ c.x, c.y + dy }), juce::Justification::centred);
    }
}

void VoxPanel::resized()
{
    knobs[0].s.setBounds (juce::Rectangle<float> (172.0f, 172.0f).withCentre (knobCentres[0]).toNearestInt());
    knobs[1].s.setBounds (juce::Rectangle<float> (172.0f, 172.0f).withCentre (knobCentres[1]).toNearestInt());
    for (int i = 2; i < 5; ++i)
        knobs[(size_t) i].s.setBounds (juce::Rectangle<float> (90.0f, 90.0f).withCentre (knobCentres[i]).toNearestInt());

    for (int i = 0; i < 3; ++i)
        tuneButtons[(size_t) i].setBounds (28 + i * 92, 370, 88, 28);
    keys.setBounds (28, 420, 272, 56);
    scaleBox.setBounds (322, 370, 144, 30);
    midiToggle.setBounds (320, 416, 150, 26);

    prevButton.setBounds (470, 28, 30, 30);
    presetBox.setBounds (504, 28, 250, 30);
    nextButton.setBounds (758, 28, 30, 30);
}

// ---------------- Editor ----------------

VoxEditor::VoxEditor (VoxProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (VoxPanel::baseW * 3 / 4, VoxPanel::baseH * 3 / 4, VoxPanel::baseW * 2, VoxPanel::baseH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) VoxPanel::baseW / VoxPanel::baseH);
    setSize (VoxPanel::baseW, VoxPanel::baseH);
}

void VoxEditor::resized()
{
    panel.setBounds (0, 0, VoxPanel::baseW, VoxPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / VoxPanel::baseW));
}
