#include "TopBar.h"
#include "Palette.h"
#include "Fonts.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../engine/InternalSynth.h"

namespace bd
{

TopBar::TopBar (BendDemonProcessor& p) : processor (p)
{
    // mode switch (bound to the "mode" parameter)
    if (auto* param = processor.getAPVTS().getParameter (ids::mode))
    {
        modeAttachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            modeDraw.setToggleState (v > 0.5f, juce::dontSendNotification);
            modeLive.setToggleState (v <= 0.5f, juce::dontSendNotification);
        });
        modeAttachment->sendInitialUpdate();
    }
    modeDraw.setTooltip ("DRAW: the drawn pattern plays in sync with the host; piano-roll notes still play live");
    modeLive.setTooltip ("LIVE: 1.x behaviour - piano-roll notes are turned into bends by the gestures");
    modeDraw.onClick = [this] { if (modeAttachment) modeAttachment->setValueAsCompleteGesture (1.0f); };
    modeLive.onClick = [this] { if (modeAttachment) modeAttachment->setValueAsCompleteGesture (0.0f); };

    synthButton.setTooltip ("SOUND - pick a built-in sound or load your own synth plugin");
    synthButton.onClick = [this] { showSynthMenu(); };

    for (int b : { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 })
        barsBox.addItem (juce::String (b) + (b == 1 ? " bar" : " bars"), b);
    barsBox.setTooltip ("LOOP - how many bars the pattern has before it repeats");
    barsBox.onChange = [this] { if (onBarsChanged && barsBox.getSelectedId() > 0) onBarsChanged (barsBox.getSelectedId()); };

    playButton.setTooltip ("PLAY - hear the pattern right now (when FL Studio is stopped). Press play in FL and it follows FL.");
    playButton.setLabel ("PLAY");
    playButton.onClick = [this] { processor.setPreviewPlaying (! processor.isPreviewPlaying()); };

    presetButton.setTooltip ("PRESETS - ready-made patterns with a sound, or shape settings");
    presetButton.onClick = [this] { showPresetMenu(); };

    prevPreset.setButtonText ("<");
    nextPreset.setButtonText (">");
    prevPreset.setVisible (false);
    nextPreset.setVisible (false);
    auto step = [this] (int d)
    {
        auto& pm = processor.getPresets();
        const auto names = pm.getFactoryNames();
        int idx = names.indexOf (pm.getCurrentName());
        idx = (idx < 0 ? 0 : (idx + d + names.size()) % names.size());
        pm.loadFactory (idx);
        refresh();
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (+1); };
    prevPreset.setTooltip ("Previous factory preset");
    nextPreset.setTooltip ("Next factory preset");

    dimensionButton.setButtonText ("3D");   // placed by the editor in the canvas corner
    dimensionButton.setClickingTogglesState (true);
    dimensionButton.setTooltip ("DIMENSION: tilt the field into 3D (view only)");
    dimensionButton.onClick = [this] { if (onDimensionToggled) onDimensionToggled(); };

    settingsButton.setTooltip ("SETTINGS - MPE, pitch-bend range, voices, MIDI out, panic, window size");
    settingsButton.onClick = [this] { showSettingsMenu(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &modeDraw, &modeLive, &synthButton, &barsBox, &playButton,
                                 &presetButton, &settingsButton })
        addAndMakeVisible (c);

    setTooltip ("Right-click for window size, or drag the bottom-right corner");
    refresh();
}

TopBar::~TopBar() = default;

void TopBar::refresh()
{
    auto& host = processor.getHost();
    juce::String slot;
    if (host.isLoading())                                   slot = "LOADING...";
    else if (! processor.isUsingInternalSynth())            slot = host.getPluginName();
    else if (host.getMissingPluginName().isNotEmpty())      slot = "MISSING: " + host.getMissingPluginName();
    else                                                    slot = processor.getSynthSlotText();

    lastSlot = slot;
    synthButton.setButtonText (slot);
    presetButton.setButtonText (processor.getPresets().getCurrentName());
    repaint();
}

void TopBar::tick (float t)
{
    time = t;

    if (processor.getSynthSlotText() != lastSlot && ! processor.getHost().isLoading())
        refresh();

    const int bars = processor.getPatternStore().get().bars;
    if (bars != lastBars)
    {
        lastBars = bars;
        if (barsBox.indexOfItemId (bars) >= 0) barsBox.setSelectedId (bars, juce::dontSendNotification);
        else                                   barsBox.setText (juce::String (bars) + " bars", juce::dontSendNotification);
    }

    const bool hostPlaying = processor.getVisualFifo().getTransportNow().playing && ! processor.isPreviewPlaying();
    const bool previewing = processor.isPreviewPlaying();
    playButton.setIcon (previewing ? IconButton::Icon::stop : IconButton::Icon::play);
    playButton.setToggleState (previewing || hostPlaying, juce::dontSendNotification);
    playButton.setEnabled (! hostPlaying && processor.isDrawMode());
    barsBox.setEnabled (processor.isDrawMode());

    const auto tempo = hostPlaying ? juce::String ("FL ") + juce::String (juce::roundToInt (processor.getTempo()))
                                   : (previewing ? juce::String ("STOP ") : juce::String ("PLAY ")) + juce::String (juce::roundToInt (processor.getTempo()));
    if (tempo != lastTempo)
    {
        lastTempo = tempo;
        playButton.setLabel (tempo + " BPM");
    }
}

void TopBar::resized()
{
    const float s = (float) getWidth() / 1040.0f;
    auto r = getLocalBounds().reduced (juce::roundToInt (12 * s), 0);
    const int bh = juce::roundToInt (32 * s);
    auto row = [&] (juce::Rectangle<int> a) { return a.withSizeKeepingCentre (a.getWidth(), bh); };
    const int gap = juce::roundToInt (4 * s), group = juce::roundToInt (16 * s);

    wordmarkArea = r.removeFromLeft (juce::roundToInt (196 * s)).toFloat();
    r.removeFromLeft (group);

    modeDraw.setBounds (row (r.removeFromLeft (juce::roundToInt (60 * s))));
    r.removeFromLeft (gap / 2);
    modeLive.setBounds (row (r.removeFromLeft (juce::roundToInt (56 * s))));
    r.removeFromLeft (group);

    settingsButton.setBounds (row (r.removeFromRight (bh)));
    r.removeFromRight (gap * 2);
    presetButton.setBounds (row (r.removeFromRight (juce::roundToInt (156 * s))));
    r.removeFromRight (group);
    playButton.setBounds (row (r.removeFromRight (juce::roundToInt (112 * s))));
    r.removeFromRight (group);
    barsBox.setBounds (row (r.removeFromRight (juce::roundToInt (84 * s))));
    barsCaption = r.removeFromRight (juce::roundToInt (40 * s)).toFloat();
    r.removeFromRight (group);

    soundCaption = r.removeFromLeft (juce::roundToInt (48 * s)).toFloat();
    synthButton.setBounds (row (r));
}

//==============================================================================
void TopBar::paint (juce::Graphics& g)
{
    using namespace juce;
    const float s = (float) getWidth() / 1040.0f;
    g.fillAll (pal::ground0);
    g.setColour (pal::hairline);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    drawWordmark (g, wordmarkArea);

    g.setColour (pal::textDim);
    g.setFont (fonts::ui (jmax (10.0f, 13.0f * s)));
    g.drawText ("Sound", soundCaption.toNearestInt(), Justification::centredLeft);
    g.drawText ("Loop", barsCaption.toNearestInt(), Justification::centredLeft);
}

void TopBar::drawWordmark (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;
    const float h = r.getHeight();

    // TRAPWIN (brand) - small, chrome
    auto brand = r.withWidth (r.getWidth() * 0.38f).withSizeKeepingCentre (r.getWidth() * 0.38f, h * 0.30f);
    GlyphArrangement ga;
    ga.addFittedText (fonts::display (brand.getHeight(), 2), "TRAPWIN", brand.getX(), brand.getY(), brand.getWidth(), brand.getHeight(),
                      Justification::centredLeft, 1, 0.8f);
    Path text;
    ga.createPath (text);
    const auto tb = text.getBounds();
    text.applyTransform (AffineTransform::shear (-0.16f, 0.0f).translated (tb.getHeight() * 0.16f * (tb.getBottom() / jmax (1.0f, tb.getHeight())), 0.0f));
    ColourGradient chrome (Colour (0xffeef2fa), 0.0f, tb.getY(), Colour (0xff8a93a8), 0.0f, tb.getBottom(), false);
    chrome.addColour (0.5, Colour (0xffc9d1e3));
    g.setGradientFill (chrome);
    g.fillPath (text);

    // divider
    const float dx = text.getBounds().getRight() + 10.0f;
    g.setColour (pal::hairline);
    g.fillRect (dx, h * 0.28f, 1.0f, h * 0.44f);

    // BEND DEMON - product name with the accent gradient
    auto name = Rectangle<float> (dx + 10.0f, r.getY(), r.getRight() - dx - 10.0f, h).withSizeKeepingCentre (r.getRight() - dx - 10.0f, h * 0.30f);
    GlyphArrangement ga2;
    ga2.addFittedText (fonts::display (name.getHeight(), 1), "BEND DEMON", name.getX(), name.getY(), name.getWidth(), name.getHeight(),
                       Justification::centredLeft, 1, 0.5f);
    Path t2;
    ga2.createPath (t2);
    const auto nb = t2.getBounds();
    ColourGradient grad (pal::cyan, nb.getX(), 0.0f, pal::magenta, nb.getRight(), 0.0f, false);
    grad.addColour (0.5, pal::violet);
    g.setGradientFill (grad);
    g.fillPath (t2);
}

//==============================================================================
void TopBar::showPresetMenu()
{
    auto& pm = processor.getPresets();
    juce::PopupMenu menu;

    const auto factory = pm.getFactoryNames();
    const int numPat = PresetManager::numPatternPresets();
    menu.addSectionHeader ("Patterns");
    for (int i = 0; i < factory.size(); ++i)
    {
        if (i == numPat) menu.addSectionHeader ("Gestures (keep the pattern)");
        menu.addItem (1 + i, factory[i], true, factory[i] == pm.getCurrentName());
    }

    const auto user = pm.getUserNames();
    if (! user.isEmpty())
    {
        menu.addSectionHeader ("User");
        for (int i = 0; i < user.size(); ++i)
            menu.addItem (1000 + i, user[i], true, user[i] == pm.getCurrentName());
    }

    menu.addSeparator();
    menu.addItem (5000, "Save preset as...");
    menu.addItem (5001, "Open preset folder");

    juce::Component::SafePointer<TopBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton),
        [safe, factory, user] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pm2 = safe->processor.getPresets();

            if (r >= 1 && r <= factory.size())          pm2.loadFactory (r - 1);
            else if (r >= 1000 && r < 1000 + user.size()) pm2.loadUser (user[r - 1000]);
            else if (r == 5001)                          PresetManager::getUserPresetDirectory().startAsProcess();
            else if (r == 5000)
            {
                auto* aw = new juce::AlertWindow ("Save preset", "Name (stores the pattern and all settings):", juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", pm2.getCurrentName());
                aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->setLookAndFeel (&safe->getLookAndFeel());
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int result)
                {
                    if (safe != nullptr && result == 1)
                    {
                        safe->processor.getPresets().saveUser (aw->getTextEditorContents ("name"));
                        safe->refresh();
                    }
                }), true);
            }

            safe->refresh();
        });
}

void TopBar::showSynthMenu()
{
    auto& host = processor.getHost();
    const bool internal = processor.isUsingInternalSynth();

    juce::PopupMenu menu;
    menu.addSectionHeader ("Built-in sounds");
    for (int i = 0; i < InternalSynth::numSounds; ++i)
        menu.addItem (1 + i, InternalSynth::getSoundName (i), true, internal && processor.getInternalSound() == i);

    menu.addSectionHeader ("Your synth plugin");
    if (host.hasPlugin())
    {
        menu.addItem (20, "Use " + host.getPluginName(), true, ! internal);
        menu.addItem (21, "Open plugin window");
        menu.addItem (22, "Change plugin...");
        menu.addItem (23, "Unload plugin");
    }
    else
    {
        menu.addItem (22, "Load plugin...");
    }
    juce::Component::SafePointer<TopBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&synthButton), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& proc = safe->processor;
        if (r >= 1 && r <= InternalSynth::numSounds)
        {
            proc.setInternalSound (r - 1);
            proc.setUseInternalSynth (true);
        }
        else switch (r)
        {
            case 20: proc.setUseInternalSynth (false); break;
            case 21: proc.getHost().showEditorWindow(); break;
            case 22: if (safe->onLoadSynth) safe->onLoadSynth(); break;
            case 23: proc.getHost().unloadPlugin(); break;
            case 30: proc.panic(); break;
            default: break;
        }
        safe->refresh();
    });
}

void TopBar::showSettingsMenu()
{
    auto& apvts = processor.getAPVTS();
    auto get = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    juce::PopupMenu bend, voices, size;
    const int curRange = (int) std::lround (get (ids::bendRange));
    for (int i = 0; i < numBendRanges; ++i)
        bend.addItem (100 + i, juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (bendRanges[i]) + " semitones" + (bendRanges[i] == 48 ? "  (MPE standard)" : ""),
                      true, i == curRange);
    const int curVoices = (int) std::lround (get (ids::voices));
    for (int v : { 1, 2, 4, 6, 8, 10, 12, 15 })
        voices.addItem (200 + v, juce::String (v) + (v == 1 ? " voice" : " voices"), true, v == curVoices);

    juce::PopupMenu m;
    m.addSectionHeader ("Synth connection");
    m.addItem (1, "Mono slides (for synths without MPE, e.g. Serum 1)", true, get (ids::seqMono) > 0.5f);
    m.addSubMenu ("Pitch-bend range (set the same in your synth)", bend);
    m.addSubMenu ("Voices (MPE channels)", voices);
    m.addItem (2, "Send MIDI out instead of playing a synth", true, get (ids::midiOutMode) > 0.5f);
    m.addItem (3, "Snap piano-roll notes to the scale", true, get (ids::quantizeInput) > 0.5f);
    m.addSeparator();
    m.addItem (4, "Panic: stop all notes + resend MPE setup");
    m.addItem (5, "Window size...");

    auto toggle = [&apvts] (const char* id)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
    };
    auto setPlain = [&apvts] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };

    juce::Component::SafePointer<TopBar> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [safe, toggle, setPlain] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) toggle (ids::seqMono);
        else if (r == 2) toggle (ids::midiOutMode);
        else if (r == 3) toggle (ids::quantizeInput);
        else if (r == 4) safe->processor.panic();
        else if (r == 5) { if (safe->onWindowSizeMenu) safe->onWindowSizeMenu(); }
        else if (r >= 100 && r < 100 + numBendRanges) setPlain (ids::bendRange, (float) (r - 100));
        else if (r > 200) setPlain (ids::voices, (float) (r - 200));
    });
}

} // namespace bd
