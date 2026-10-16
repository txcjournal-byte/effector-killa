#include "TopBar.h"
#include "Palette.h"
#include "Fonts.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

namespace bd
{

namespace
{
    /** 4-point sparkle (concave star). */
    juce::Path sparkle (juce::Point<float> c, float r)
    {
        juce::Path p;
        const float k = r * 0.18f;
        p.startNewSubPath (c.x, c.y - r);
        p.quadraticTo (c.x + k, c.y - k, c.x + r, c.y);
        p.quadraticTo (c.x + k, c.y + k, c.x, c.y + r);
        p.quadraticTo (c.x - k, c.y + k, c.x - r, c.y);
        p.quadraticTo (c.x - k, c.y - k, c.x, c.y - r);
        p.closeSubPath();
        return p;
    }
}

TopBar::TopBar (BendDemonProcessor& p) : processor (p)
{
    synthButton.setButtonText ("LOAD SYNTH");
    synthButton.setTooltip ("Load the instrument plugin BEND DEMON plays (Serum 2, Vital, Pigments ...)");
    synthButton.onClick = [this]
    {
        if (processor.getHost().hasPlugin()) showSynthMenu();
        else if (onLoadSynth) onLoadSynth();
    };

    editorButton.setButtonText ("EDIT");
    editorButton.setTooltip ("Open the hosted synth's own window");
    editorButton.onClick = [this] { processor.getHost().showEditorWindow(); };

    presetButton.setTooltip ("Gesture presets (do not change the hosted synth patch)");
    presetButton.onClick = [this] { showPresetMenu(); };

    prevPreset.setButtonText ("<");
    nextPreset.setButtonText (">");
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

    dimensionButton.setButtonText ("DIMENSION");
    dimensionButton.setClickingTogglesState (true);
    dimensionButton.setTooltip ("Tilt the voice field into 3D");
    dimensionButton.onClick = [this] { if (onDimensionToggled) onDimensionToggled(); };

    panicButton.setButtonText ("!");
    panicButton.setTooltip ("Panic: release all voices and resend the MPE configuration");
    panicButton.onClick = [this] { processor.panic(); };

    for (auto* b : { &synthButton, &editorButton, &presetButton, &prevPreset, &nextPreset, &dimensionButton, &panicButton })
        addAndMakeVisible (b);

    refresh();
}

void TopBar::refresh()
{
    auto& host = processor.getHost();

    juce::String synthText;
    if (host.isLoading())                          synthText = "LOADING...";
    else if (host.hasPlugin())                     synthText = host.getPluginName();
    else if (host.getMissingPluginName().isNotEmpty()) synthText = "MISSING: " + host.getMissingPluginName();
    else                                           synthText = "+  LOAD SYNTH";

    synthButton.setButtonText (synthText);
    editorButton.setEnabled (host.hasPlugin());
    presetButton.setButtonText (processor.getPresets().getCurrentName());
    repaint();
}

void TopBar::tick (float t)
{
    time = t;
    repaint (titleArea.getSmallestIntegerContainer().expanded (4));

    const int v = processor.getActiveVoiceCount();
    if (v != lastVoices)
    {
        lastVoices = v;
        repaint (statusArea.getSmallestIntegerContainer().expanded (4));
    }
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (16, 0);
    const int h = getHeight();
    const int bh = juce::jlimit (24, 34, h / 2);

    wordmarkArea = r.removeFromLeft (juce::roundToInt (h * 2.7f)).toFloat();
    r.removeFromLeft (22);   // divider
    titleArea = r.removeFromLeft (juce::roundToInt (h * 2.75f)).toFloat();
    r.removeFromLeft (14);

    auto right = r.removeFromRight (juce::roundToInt (h * 2.9f));
    {
        auto row = right.withSizeKeepingCentre (right.getWidth(), bh);
        nextPreset.setBounds (row.removeFromRight (bh));
        row.removeFromRight (4);
        prevPreset.setBounds (row.removeFromLeft (bh));
        row.removeFromLeft (4);
        presetButton.setBounds (row);
    }

    r.removeFromRight (10);
    auto dimRow = r.removeFromRight (juce::roundToInt (h * 1.25f)).withSizeKeepingCentre (juce::roundToInt (h * 1.25f), bh);
    dimensionButton.setBounds (dimRow);
    r.removeFromRight (10);

    statusArea = r.removeFromRight (juce::roundToInt (h * 1.75f)).toFloat();
    r.removeFromRight (10);

    auto slot = r.withSizeKeepingCentre (r.getWidth(), bh);
    panicButton.setBounds (slot.removeFromRight (bh));
    slot.removeFromRight (4);
    editorButton.setBounds (slot.removeFromRight (juce::roundToInt (bh * 1.8f)));
    slot.removeFromRight (4);
    synthButton.setBounds (slot);
}

//==============================================================================
void TopBar::paint (juce::Graphics& g)
{
    // translucent strip with a bottom hairline that fades to the sides
    g.setColour (pal::ground0.withAlpha (0.55f));
    g.fillRect (getLocalBounds());

    juce::ColourGradient line (pal::cyan.withAlpha (0.0f), 0.0f, (float) getHeight() - 1.0f,
                               pal::cyan.withAlpha (0.0f), (float) getWidth(), (float) getHeight() - 1.0f, false);
    line.addColour (0.3, pal::cyan.withAlpha (0.35f));
    line.addColour (0.7, pal::magenta.withAlpha (0.35f));
    g.setGradientFill (line);
    g.fillRect (0.0f, (float) getHeight() - 1.0f, (float) getWidth(), 1.0f);

    drawWordmark (g, wordmarkArea);

    // divider
    const float dx = wordmarkArea.getRight() + 11.0f;
    g.setColour (pal::hairline.withAlpha (0.5f));
    g.fillRect (dx, (float) getHeight() * 0.22f, 1.0f, (float) getHeight() * 0.56f);

    drawTitle (g, titleArea);
    drawStatus (g, statusArea);
}

void TopBar::drawWordmark (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;

    auto main = r.withTrimmedBottom (r.getHeight() * 0.36f).withTrimmedTop (r.getHeight() * 0.10f);
    auto sub  = r.withTop (main.getBottom() + 1.0f).withHeight (r.getHeight() * 0.2f);

    // "TRAPWIN" as a path so it can be skewed and chrome-filled
    GlyphArrangement ga;
    const auto font = fonts::display (main.getHeight() * 0.95f, 2);
    ga.addFittedText (font, "TRAPWIN", main.getX(), main.getY(), main.getWidth() * 0.86f, main.getHeight(),
                      Justification::centredLeft, 1, 0.8f);
    Path text;
    ga.createPath (text);

    const auto tb = text.getBounds();
    text.applyTransform (AffineTransform::shear (-0.18f, 0.0f).translated (tb.getHeight() * 0.18f * (tb.getBottom() / tb.getHeight()), 0.0f));

    // chrome: bright top, dark band, bright bottom
    ColourGradient chrome (Colour (0xfff4f8ff), 0.0f, tb.getY(), Colour (0xff8d97b8), 0.0f, tb.getBottom(), false);
    chrome.addColour (0.45, Colour (0xffdfe7ff));
    chrome.addColour (0.52, Colour (0xff4a5375));
    chrome.addColour (0.70, Colour (0xffb9c4e6));
    g.setColour (pal::cyan.withAlpha (0.18f));
    g.strokePath (text, PathStrokeType (3.0f));
    g.setGradientFill (chrome);
    g.fillPath (text);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.strokePath (text, PathStrokeType (0.6f));

    // two sparkles
    const auto tb2 = text.getBounds();
    const float s1 = main.getHeight() * 0.28f, s2 = main.getHeight() * 0.17f;
    for (auto [c, rad] : { std::pair<Point<float>, float> { { tb2.getRight() + s1 * 0.9f, tb2.getY() + s1 * 0.4f }, s1 },
                           std::pair<Point<float>, float> { { tb2.getRight() + s1 * 2.2f, tb2.getY() + s1 * 1.6f }, s2 } })
    {
        g.setColour (pal::cyan.withAlpha (0.25f));
        g.fillEllipse (Rectangle<float> (rad * 1.6f, rad * 1.6f).withCentre (c));
        g.setColour (Colours::white);
        g.fillPath (sparkle (c, rad));
    }

    g.setColour (pal::textDim);
    g.setFont (fonts::mono (jmax (7.5f, sub.getHeight() * 0.95f), true));
    g.drawFittedText (String (CharPointer_UTF8 ("+ AUDIO \xc2\xb7 UNDERGROUND +")), sub.toNearestInt(), Justification::centredLeft, 1, 0.6f);
}

void TopBar::drawTitle (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;

    auto area = r.withSizeKeepingCentre (r.getWidth(), r.getHeight() * 0.55f);
    GlyphArrangement ga;
    ga.addFittedText (fonts::display (area.getHeight() * 0.95f, 2), "BEND DEMON",
                      area.getX(), area.getY(), area.getWidth(), area.getHeight(), Justification::centredLeft, 1, 0.7f);
    Path text;
    ga.createPath (text);
    const auto tb = text.getBounds();

    // animated shimmer: the gradient slides across, a white highlight band sweeps through
    const float w = tb.getWidth();
    const float shift = std::fmod (time * 0.25f, 1.0f);
    const float x0 = tb.getX() - w + shift * 2.0f * w;

    ColourGradient grad (pal::cyan, x0, 0.0f, pal::cyan, x0 + 2.0f * w, 0.0f, false);
    grad.addColour (0.25, pal::violet);
    grad.addColour (0.5,  pal::magenta);
    grad.addColour (0.75, pal::violet);

    g.setColour (pal::violet.withAlpha (0.20f));
    g.strokePath (text, PathStrokeType (4.0f));
    g.setGradientFill (grad);
    g.fillPath (text);

    const float sweep = std::fmod (time * 0.45f, 1.6f) - 0.3f;
    ColourGradient hi (Colours::white.withAlpha (0.0f), tb.getX() + sweep * w - 30.0f, 0.0f,
                       Colours::white.withAlpha (0.0f), tb.getX() + sweep * w + 30.0f, 0.0f, false);
    hi.addColour (0.5, Colours::white.withAlpha (0.55f));
    g.setGradientFill (hi);
    g.fillPath (text);

    g.setColour (pal::textDim);
    g.setFont (fonts::mono (jmax (8.0f, r.getHeight() * 0.15f)));
    g.drawText ("MPE GESTURE ENGINE", r.withTop (area.getBottom() + 1.0f).toNearestInt(), Justification::topLeft);
}

void TopBar::drawStatus (juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace juce;

    const int voices = processor.getActiveVoiceCount();
    const int maxV   = processor.getConfiguredVoices();
    const bool midiOut = processor.isMidiOutMode();
    const bool hosted  = processor.getHost().hasPlugin();

    auto top = r.withHeight (r.getHeight() * 0.5f).withY (r.getY() + r.getHeight() * 0.12f);
    auto bot = top.withY (top.getBottom());

    // voice meter: one dot per member channel
    const int n = jmax (1, maxV);
    const float dot = jmin (6.0f, (top.getWidth() - 4.0f) / (float) n - 1.5f);
    for (int i = 0; i < n; ++i)
    {
        auto c = Rectangle<float> (dot, dot).withCentre ({ top.getX() + 2.0f + (dot + 1.5f) * (float) i + dot * 0.5f, top.getCentreY() - 4.0f });
        const bool lit = i < voices;
        g.setColour (lit ? pal::ribbon ((float) i / (float) n) : pal::hairline);
        g.fillEllipse (c);
    }

    g.setColour (pal::text);
    g.setFont (fonts::mono (jmax (8.0f, top.getHeight() * 0.36f), true));
    g.drawText (String (voices) + "/" + String (maxV) + " VOICES", top.withTrimmedTop (top.getHeight() * 0.42f).toNearestInt(), Justification::centredLeft);

    const Colour statusCol = midiOut ? pal::amber : (hosted ? pal::lime : pal::textDim);
    g.setColour (statusCol);
    g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ bot.getX() + 4.0f, bot.getCentreY() }));
    g.setFont (fonts::mono (jmax (8.0f, bot.getHeight() * 0.36f)));
    const String s = midiOut ? String ("MPE > MIDI OUT")
                             : (hosted ? String (CharPointer_UTF8 ("MPE \xc2\xb1")) + String (processor.getBendRange()) + " LIVE"
                                       : String ("MPE READY"));
    g.drawText (s, bot.withTrimmedLeft (12.0f).toNearestInt(), Justification::centredLeft);
}

//==============================================================================
void TopBar::showPresetMenu()
{
    auto& pm = processor.getPresets();
    juce::PopupMenu menu;

    menu.addSectionHeader ("Factory");
    const auto factory = pm.getFactoryNames();
    for (int i = 0; i < factory.size(); ++i)
        menu.addItem (1 + i, factory[i], true, factory[i] == pm.getCurrentName());

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
                auto* aw = new juce::AlertWindow ("Save gesture preset", "Name:", juce::MessageBoxIconType::NoIcon);
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
    juce::PopupMenu menu;
    menu.addItem (1, "Open synth window");
    menu.addItem (2, "Change synth...");
    menu.addItem (3, "Resend MPE configuration");
    menu.addSeparator();
    menu.addItem (4, "Unload synth");

    juce::Component::SafePointer<TopBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&synthButton), [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& host = safe->processor.getHost();
        switch (r)
        {
            case 1: host.showEditorWindow(); break;
            case 2: if (safe->onLoadSynth) safe->onLoadSynth(); break;
            case 3: safe->processor.panic(); break;
            case 4: host.unloadPlugin(); break;
            default: break;
        }
        safe->refresh();
    });
}

} // namespace bd
