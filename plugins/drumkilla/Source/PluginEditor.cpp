#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
using namespace Look;

namespace
{
    const char* describeCategory (const String& category)
    {
        static const std::map<String, const char*> text = {
            { "808",            "Make the 808 hit on phones, keep the sub clean and mono." },
            { "KICK",           "Knock, click and punch that cut through any mix." },
            { "SNARE",          "Crack, body and slam for snares and rims." },
            { "CLAP",           "Wide, crushed and stacked claps." },
            { "HATS",           "Crisp, bitey hats that sit on top of the beat." },
            { "PERC",           "Pop and character for percs, shakers and FX." },
            { "DRUM BUS",       "Glue and loudness for the whole drum group." },
            { "FAVOURITES",     "Presets you starred." },
            { "USER",           "Your own saved presets." },
        };
        const auto it = text.find (category);
        return it != text.end() ? it->second : "";
    }

    // buttons created later must not take keyboard focus either (space bar = DAW play)
    template <typename C>
    C* noFocus (C* c)
    {
        c->setWantsKeyboardFocus (false);
        c->setMouseClickGrabsKeyboardFocus (false);
        return c;
    }
}

//==============================================================================
HeadView::HeadView (K808Processor& p)
    : processor (p),
      killParam (*p.apvts.getParameter (ParamIDs::driveAmount)),
      killAttachment (killParam, [this] (float v) { kill = v * 0.01f; repaint(); }, &p.undoManager),
      scopeIn (300, 0.0f), scopeOut (300, 0.0f)
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    jawImage = ImageCache::getFromMemory (BinaryData::jaw_png, BinaryData::jaw_pngSize);
    setOpaque (true);
    setTooltip ("Drag the jaw down = DRIVE: how hard the mid / high band hits the saturator (the sub stays clean). "
                "Double-click = default, Ctrl / Cmd + drag = fine. The mouth shows your drum: white line = in, red = out.");
    killAttachment.sendInitialUpdate();
}

bool HeadView::hitsJaw (Point<float> pos) const
{
    const auto o = Layout::head.getPosition().toFloat();
    const auto mouth = Layout::mouth.toFloat() - o;
    const auto open = mouth.withBottom (mouth.getBottom() + jawDrop());
    const auto jaw = (Layout::jaw.toFloat() - o).translated (0.0f, jawDrop()).reduced (10.0f, 0.0f);
    return open.contains (pos) || jaw.contains (pos);
}

void HeadView::paint (Graphics& g)
{
    const auto o = Layout::head.getPosition().toFloat();
    g.drawImage (background, 0, 0, getWidth(), getHeight(), Layout::head.getX(), Layout::head.getY(), getWidth(), getHeight());

    const auto drop = jawDrop();
    const auto mouth = Layout::mouth.toFloat() - o;
    const auto inner = mouth.withBottom (mouth.getBottom() + drop).reduced (2.0f);

    // mouth: dark throat with red heat that grows with KILL
    g.setColour (Colour (0xff050505));
    g.fillRoundedRectangle (inner, 10.0f);
    g.setGradientFill (ColourGradient (Palette::redGlow.withAlpha (jlimit (0.0f, 1.0f, 0.08f + 0.32f * kill + 0.2f * beat)),
                                       inner.getCentreX(), inner.getBottom(),
                                       Palette::redGlow.withAlpha (0.0f), inner.getCentreX(), inner.getY() + inner.getHeight() * 0.3f, false));
    g.fillRoundedRectangle (inner, 10.0f);

    // waveform: input grey behind, output red in front, clipped parts bright
    const auto wave = inner.reduced (12.0f, 8.0f).withTrimmedTop (36.0f).withTrimmedBottom (22.0f);
    const auto cy = wave.getCentreY(), half = wave.getHeight() * 0.5f;
    const auto ceiling = Decibels::decibelsToGain (processor.apvts.getRawParameterValue (ParamIDs::clipCeiling)->load());
    const auto n = (int) scopeOut.size();
    const auto colW = wave.getWidth() / (float) n;

    for (int i = 0; i < n; ++i)
    {
        const auto v = scopeOut[(size_t) i];
        const auto a = jmin (1.0f, v) * half;
        const auto x = wave.getX() + (float) i * colW;
        g.setColour (Palette::red.withAlpha (0.85f));
        g.fillRect (x, cy - a, colW + 0.4f, 2.0f * a);
        if (v >= ceiling * 0.97f)
        {
            // clipped: light up the flattened edges only
            g.setColour (Colour (0xffffa08a));
            g.fillRect (x, cy - a, colW + 0.4f, 3.0f);
            g.fillRect (x, cy + a - 3.0f, colW + 0.4f, 3.0f);
        }
    }

    // input as a light outline on top, so you see how much bigger the drum got
    Path in;
    for (int side = 0; side < 2; ++side)
    {
        const auto sign = side == 0 ? -1.0f : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            const Point<float> pt (wave.getX() + ((float) i + 0.5f) * colW, cy + sign * jmin (1.0f, scopeIn[(size_t) i]) * half);
            if (i == 0) in.startNewSubPath (pt); else in.lineTo (pt);
        }
    }
    g.setColour (Colour (0xe0e8e3d8));
    g.strokePath (in, PathStrokeType (1.6f));
    g.setColour (Palette::redBright.withAlpha (0.35f));
    g.fillRect (wave.getX(), cy - ceiling * half, wave.getWidth(), 1.2f);
    g.fillRect (wave.getX(), cy + ceiling * half, wave.getWidth(), 1.2f);

    // top of the mouth: KILL amount (or the phone check warning)
    const auto top = inner.withHeight (38.0f).translated (0.0f, 4.0f);
    if (phoneOn)
    {
        g.setColour (Palette::red);
        g.fillRoundedRectangle (top.reduced (40.0f, 3.0f), 3.0f);
        g.setColour (Colours::white);
        g.setFont (fonts->bold (24.0f));
        g.drawText ("PHONE CHECK ON", top, Justification::centred, false);
    }
    else
    {
        const auto mode = Choices::satModes[jlimit (0, 2, (int) processor.apvts.getRawParameterValue (ParamIDs::satMode)->load())];
        g.setColour (Palette::text);
        g.setFont (fonts->bold (32.0f));
        g.drawText ("DRIVE " + String (roundToInt (kill * 100.0f)) + "%  " + mode.toUpperCase(), top, Justification::centred, false);
    }

    // bottom of the mouth: output peak, clipper and ducking reduction
    g.setColour (Palette::textDim);
    g.setFont (fonts->mono (19.0f));
    g.drawText (status, inner.withTrimmedTop (inner.getHeight() - 26.0f).withTrimmedBottom (4.0f), Justification::centred, false);

    // jaw
    const auto jaw = (Layout::jaw.toFloat() - o).translated (0.0f, drop);
    g.drawImage (jawImage, jaw);

    if (hover && ! dragging)
    {
        const auto hint = Rectangle<float> (200.0f, 34.0f).withCentre ({ jaw.getCentreX(), jaw.getY() + 70.0f });
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (hint, 4.0f);
        g.setColour (Palette::text);
        g.setFont (fonts->bold (24.0f));
        g.drawText ("DRAG THE JAW", hint, Justification::centred, false);
    }

    // eyes glow with KILL and flash on every hit
    for (auto eye : { Point<float> (606.0f, 283.0f), Point<float> (852.0f, 283.0f) })
    {
        const auto c = eye - o;
        const auto r = 30.0f + 34.0f * kill + 12.0f * beat;
        const auto a = jlimit (0.0f, 1.0f, 0.1f + 0.5f * kill + 0.4f * beat);
        g.setGradientFill (ColourGradient (Palette::redGlow.withAlpha (a), c, Palette::redGlow.withAlpha (0.0f), c.translated (r, 0.0f), true));
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
        g.setColour (Colour (0xffffd6cc).withAlpha (jlimit (0.0f, 1.0f, 0.2f + 0.6f * kill + 0.3f * beat)));
        g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (c));
    }
}

void HeadView::tick (float outLevel, bool phone, const String& statusText)
{
    status = statusText;
    // the jaw "bites" on every new hit: level above its own slow average
    const auto hit = jlimit (0.0f, 1.0f, (outLevel - slowLevel) * 2.5f);
    slowLevel += (outLevel - slowLevel) * 0.12f;
    beat = jmax (hit, beat * 0.8f);
    phoneOn = phone;

    auto& e = processor.engine;
    const auto n = (int) scopeOut.size();
    const auto w = e.scopeWrite.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
    {
        const auto idx = (size_t) ((w - n + i + Engine::scopeSize) % Engine::scopeSize);
        scopeIn[(size_t) i] = e.scopeIn[idx].load (std::memory_order_relaxed);
        scopeOut[(size_t) i] = e.scopeOut[idx].load (std::memory_order_relaxed);
    }
    repaint();
}

void HeadView::mouseMove (const MouseEvent& e)
{
    const auto h = hitsJaw (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
        repaint();
    }
}

void HeadView::mouseExit (const MouseEvent&)
{
    hover = false;
    setMouseCursor (MouseCursor::NormalCursor);
    repaint();
}

void HeadView::mouseDown (const MouseEvent& e)
{
    if (! hitsJaw (e.position))
        return;
    dragging = true;
    dragStart = kill;
    killAttachment.beginGesture();
}

void HeadView::mouseDrag (const MouseEvent& e)
{
    if (! dragging)
        return;
    const auto fine = e.mods.isCtrlDown() || e.mods.isCommandDown();
    const auto v = jlimit (0.0f, 1.0f, dragStart + (float) e.getDistanceFromDragStartY() / (fine ? 1200.0f : 240.0f));
    killAttachment.setValueAsPartOfGesture (v * 100.0f);
}

void HeadView::mouseUp (const MouseEvent&)
{
    if (dragging)
        killAttachment.endGesture();
    dragging = false;
    repaint();
}

void HeadView::mouseDoubleClick (const MouseEvent& e)
{
    if (hitsJaw (e.position))
        killAttachment.setValueAsCompleteGesture (killParam.convertFrom0to1 (killParam.getDefaultValue()));
}

void HeadView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (hitsJaw (e.position))
        killAttachment.setValueAsCompleteGesture (100.0f * jlimit (0.0f, 1.0f, kill + wheel.deltaY * 0.15f));
}

//==============================================================================
SubPanel::SubPanel (K808Processor& p)
    : input     (p.apvts, ParamIDs::inputGain,     "INPUT",  "Input gain into the plugin.", true),
      crossover (p.apvts, ParamIDs::crossoverFreq, "XOVER",  "Where the sub band ends and the mid / high band starts (80 - 200 Hz)."),
      focus     (p.apvts, ParamIDs::midFocus,      "FOCUS",  "Boost before the drive at the FREQ below: 808 ~550 Hz, kick click ~3 kHz, snare body ~200 Hz, crack ~4 kHz."),
      focusFreq (p.apvts, ParamIDs::focusFreq,     "FREQ",   "Centre of the FOCUS boost (60 Hz - 8 kHz)."),
      subMono   (p.apvts, ParamIDs::subMono,       "MONO",   "The sub band summed to mono."),
      subCut    (p.apvts, ParamIDs::subCut,        "28 HZ",  "Removes rumble below 28 Hz for more headroom."),
      phase     (p.apvts, ParamIDs::phaseInvert,   "PHASE",  "Inverts the polarity (when the drum fights another layer)."),
      phone     (p.apvts, ParamIDs::phonePreview,  "PHONE",  "Listen like on a phone speaker (400 Hz - 3.5 kHz). Switch it off before you export!")
{
    UI::Knob* knobs[] = { &input, &crossover, &focus, &focusFreq };
    for (int i = 0; i < 4; ++i)
    {
        knobs[i]->setKnobArea ({ 24 + (i % 2) * 92, 66 + (i / 2) * 150, 60, 60 }, 22.0f);
        knobs[i]->setDark();
        addAndMakeVisible (*knobs[i]);
    }
    UI::LedToggle* toggles[] = { &subMono, &subCut, &phase, &phone };
    for (int i = 0; i < 4; ++i)
    {
        toggles[i]->big = true;
        toggles[i]->setBounds (6 + (i % 2) * 94, 404 + (i / 2) * 56, 92, 54);
        addAndMakeVisible (*toggles[i]);
    }
}

void SubPanel::paint (Graphics& g)
{
    g.setColour (Palette::text);
    g.setFont (fonts->bold (28.0f));
    g.drawText ("TONE / INPUT", Rectangle<int> (0, 2, getWidth(), 30), Justification::centred, false);
}

//==============================================================================
void PresetBrowser::Star::paintButton (Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (10.0f);
    Path star;
    star.addStar (r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f, 0.0f);
    if (on)
    {
        g.setColour (Palette::redBright);
        g.fillPath (star);
    }
    g.setColour (on ? Palette::redBright : Palette::textDim.withAlpha (over ? 1.0f : 0.6f));
    g.strokePath (star, PathStrokeType (1.6f));
}

namespace
{
    const Rectangle<int> browserPanel { 150, 100, 1236, 824 };
    constexpr int gridX = 500, gridY = 220, gridW = 862, gridBottom = 844;
}

PresetBrowser::PresetBrowser (K808Processor& p) : processor (p)
{
    categoryNames = PresetManager::categories();
    categoryNames.add ("FAVOURITES");
    categoryNames.add ("USER");

    for (int i = 0; i < categoryNames.size(); ++i)
    {
        auto* b = noFocus (categoryButtons.add (new UI::FlatButton (categoryNames[i])));
        b->textHeight = 25.0f;
        b->setBounds (browserPanel.getX() + 20, browserPanel.getY() + 80 + i * 50, 300, 46);
        b->onClick = [this, name = categoryNames[i]] { showCategory (name); };
        addAndMakeVisible (b);
    }

    const StringArray actions { "SAVE AS", "INIT", "UNDO", "REDO", "FOLDER" };
    for (int i = 0; i < actions.size(); ++i)
    {
        auto* b = noFocus (actionButtons.add (new UI::FlatButton (actions[i])));
        b->textHeight = 24.0f;
        b->setBounds (gridX + i * 174, browserPanel.getBottom() - 68, 162, 50);
        addAndMakeVisible (b);
    }
    actionButtons[0]->setTooltip ("Save the current sound as your own preset");
    actionButtons[1]->setTooltip ("Reset every control to its default");
    actionButtons[4]->setTooltip ("Open the folder with your presets");
    actionButtons[0]->onClick = [this] { setVisible (false); if (onSaveAs) onSaveAs(); };
    actionButtons[1]->onClick = [this] { processor.presets.init(); if (onChange) onChange(); rebuild(); };
    actionButtons[2]->onClick = [this] { processor.undoManager.undo(); if (onChange) onChange(); };
    actionButtons[3]->onClick = [this] { processor.undoManager.redo(); if (onChange) onChange(); };
    actionButtons[4]->onClick = [] { PresetManager::userFolder().createDirectory(); PresetManager::userFolder().startAsProcess(); };

    noFocus (&closeButton);
    closeButton.textHeight = 28.0f;
    closeButton.setBounds (browserPanel.getRight() - 72, browserPanel.getY() + 16, 56, 50);
    closeButton.onClick = [this] { setVisible (false); };
    addAndMakeVisible (closeButton);
}

void PresetBrowser::open()
{
    auto& pm = processor.presets;
    pm.rescan();
    const auto index = pm.getCurrentIndex();
    const auto& list = pm.getPresets();
    category = isPositiveAndBelow (index, list.size()) ? list.getReference (index).category : categoryNames[0];
    setVisible (true);
    toFront (false);
    rebuild();
}

void PresetBrowser::showCategory (const String& name)
{
    category = name;
    rebuild();
}

void PresetBrowser::rebuild()
{
    presetButtons.clear();
    stars.clear();

    auto& pm = processor.presets;
    const auto& list = pm.getPresets();
    const auto current = pm.getCurrentIndex();

    Array<int> shown;
    for (int i = 0; i < list.size(); ++i)
    {
        const auto& info = list.getReference (i);
        if (category == "FAVOURITES" ? pm.isFavourite (info.name) : info.category == category)
            shown.add (i);
    }

    const auto cols = shown.size() > 20 ? 3 : 2;
    const auto rows = (gridBottom - gridY) / 62;
    const auto colW = (gridW - (cols - 1) * 14) / cols;

    for (int k = 0; k < jmin (shown.size(), cols * rows); ++k)
    {
        const auto index = shown[k];
        const auto name = list.getReference (index).name;
        const auto x = gridX + (k / rows) * (colW + 14);
        const auto y = gridY + (k % rows) * 62;

        auto* b = noFocus (presetButtons.add (new UI::FlatButton (name)));
        b->textHeight = 26.0f;
        b->active = index == current;
        b->setBounds (x, y, colW - 56, 54);
        b->onClick = [this, index]
        {
            processor.presets.load (index);
            if (onChange) onChange();
            // rebuilding deletes the button that is still inside its click handler, so do it afterwards
            MessageManager::callAsync ([sp = SafePointer<PresetBrowser> (this)] { if (sp != nullptr) sp->rebuild(); });
        };
        addAndMakeVisible (b);

        auto* s = noFocus (stars.add (new Star()));
        s->on = pm.isFavourite (name);
        s->setTooltip ("Add to / remove from FAVOURITES");
        s->setBounds (x + colW - 52, y, 52, 54);
        s->onClick = [this, s, name]
        {
            processor.presets.toggleFavourite (name);
            s->on = processor.presets.isFavourite (name);
            s->repaint();
            if (category == "FAVOURITES")
                MessageManager::callAsync ([sp = SafePointer<PresetBrowser> (this)] { if (sp != nullptr) sp->rebuild(); });
        };
        addAndMakeVisible (s);
    }

    for (int i = 0; i < categoryButtons.size(); ++i)
    {
        categoryButtons[i]->active = categoryNames[i] == category;
        categoryButtons[i]->repaint();
    }
    repaint();
}

void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.72f));

    const auto panel = browserPanel.toFloat();
    g.setColour (Colour (0xf50b0b0a));
    g.fillRect (panel);
    g.setColour (Palette::red.withAlpha (0.8f));
    g.drawRect (panel, 2.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRect (panel.reduced (5.0f), 1.0f);

    g.setColour (Palette::text);
    g.setFont (fonts->bold (46.0f));
    g.drawText ("PRESETS", Rectangle<float> (panel.getX() + 22.0f, panel.getY() + 14.0f, 300.0f, 56.0f), Justification::centredLeft, false);

    g.setColour (Palette::redBright);
    g.setFont (fonts->bold (44.0f));
    g.drawText (category, Rectangle<float> ((float) gridX, panel.getY() + 14.0f, 700.0f, 56.0f), Justification::centredLeft, false);
    g.setColour (Palette::text);
    g.setFont (fonts->sans (26.0f));
    g.drawText (describeCategory (category), Rectangle<float> ((float) gridX, panel.getY() + 68.0f, (float) gridW, 40.0f),
                Justification::centredLeft, true);

    if (presetButtons.isEmpty())
    {
        g.setColour (Palette::textDim);
        g.setFont (fonts->sans (26.0f));
        g.drawText (category == "FAVOURITES" ? "Click the star next to a preset to add it here."
                                             : "Nothing here yet. Use SAVE AS to store your own sound.",
                    Rectangle<float> ((float) gridX, (float) gridY, (float) gridW, 60.0f), Justification::centredLeft, true);
    }
}

void PresetBrowser::mouseDown (const MouseEvent& e)
{
    if (! browserPanel.toFloat().contains (e.position))
        setVisible (false);
}

//==============================================================================
Canvas::Canvas()
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    setOpaque (true);
}

void Canvas::paint (Graphics& g)
{
    g.drawImageAt (background, 0, 0);
}

//==============================================================================
K808Editor::K808Editor (K808Processor& p)
    : AudioProcessorEditor (p), processor (p),
      head (p), subPanel (p),
      duck    (p.apvts, ParamIDs::duckDepth,   "DUCK",    "Ducks only the sub under the kick. Route the kick to the sidechain input."),
      release (p.apvts, ParamIDs::duckRelease, "RELEASE", "How fast the sub comes back after the kick."),
      clip    (p.apvts, ParamIDs::clipDrive,   "CLIP",    "Drive into the 4x oversampled soft clipper: louder and harder."),
      knee    (p.apvts, ParamIDs::clipKnee,    "KNEE",    "Where the clipper starts to bend: low = softer and earlier, high = harder."),
      ceiling (p.apvts, ParamIDs::clipCeiling, "CEILING", "Maximum level of the clipper."),
      output  (p.apvts, ParamIDs::outputGain,  "OUTPUT",  "Output trim. The final hard limit stays at -0.1 dBFS.", true),
      satMode (p.apvts, ParamIDs::satMode,     "Saturation type for the mid / high band: Tape (warm), Tube (asymmetric), Foldback (aggressive)."),
      hit     (p.apvts, ParamIDs::hit,         "HIT",     "How much the drive and the clipper follow the hit of each note. High = dirty, loud hit and a clean sub body "
                                                           "(like kit drums). 0 = the whole hit is driven the same."),
      browser (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);

    addAndMakeVisible (canvas);
    canvas.setBounds (0, 0, designWidth, designHeight);

    head.setBounds (Layout::head);
    canvas.addAndMakeVisible (head);

    subPanel.setBounds (Layout::leftPanel);
    canvas.addAndMakeVisible (subPanel);

    // right panel: 2 x 3 knobs
    UI::Knob* knobs[] = { &duck, &release, &clip, &knee, &ceiling, &output };
    for (int i = 0; i < 6; ++i)
    {
        const auto x = Layout::rightPanel.getX() + (i % 2) * 115 + 20;
        const auto y = Layout::rightPanel.getY() + (i / 2) * 172 + 36;
        knobs[i]->setKnobArea ({ x, y, 75, 75 }, 26.0f);
        knobs[i]->setDark();
        canvas.addAndMakeVisible (*knobs[i]);
    }

    // bottom bar: < preset > SAVE | TAPE TUBE FOLDBACK
    const auto bar = Layout::bottomBar;
    const auto y = bar.getY() + 10, h = bar.getHeight() - 20;
    prevButton.setBounds (bar.getX() + 8, y, 46, h);
    presetButton.setBounds (bar.getX() + 56, y, 200, h);
    nextButton.setBounds (bar.getX() + 258, y, 46, h);
    saveButton.setBounds (bar.getX() + 308, y, 70, h);
    satMode.setBounds (bar.getX() + 384, y, 206, h);
    canvas.addAndMakeVisible (satMode);

    // HIT: small knob at the end of the bar, label on the left of it
    hit.setKnobArea ({ bar.getX() + 628, y + 12, 40, 40 }, 16.0f);
    hit.setDark();
    canvas.addAndMakeVisible (hit);

    for (auto* b : { &prevButton, &presetButton, &nextButton, &saveButton })
    {
        b->textHeight = 25.0f;
        canvas.addAndMakeVisible (*b);
    }
    presetButton.textHeight = 23.0f;

    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    presetButton.setTooltip ("Open the preset table");
    saveButton.setTooltip ("Save the current sound as your own preset");

    prevButton.onClick = [this] { processor.presets.loadNext (-1); refreshPresetLabel(); };
    nextButton.onClick = [this] { processor.presets.loadNext (1); refreshPresetLabel(); };
    presetButton.onClick = [this] { browser.open(); };
    saveButton.onClick = [this] { savePresetAs(); };

    browser.setBounds (0, 0, designWidth, designHeight);
    browser.onChange = [this] { shownPreset = {}; refreshPresetLabel(); };
    browser.onSaveAs = [this] { savePresetAs(); };
    canvas.addChildComponent (browser);

    refreshPresetLabel();

    setResizable (true, true);
    setResizeLimits (designWidth * 2 / 5, designHeight * 2 / 5, designWidth * 5 / 4, designHeight * 5 / 4);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    // first open: half size; afterwards the size the user dragged it to
    const auto savedWidth = (int) processor.apvts.state.getProperty ("uiWidth", designWidth / 2);
    const auto width = jlimit (designWidth * 2 / 5, designWidth * 5 / 4, savedWidth);
    setSize (width, width * designHeight / designWidth);

    // never take keyboard focus: the space bar etc. must keep reaching the DAW (play / stop)
    setWantsKeyboardFocus (false);
    std::function<void (Component&)> clearFocus = [&clearFocus] (Component& c)
    {
        c.setWantsKeyboardFocus (false);
        c.setMouseClickGrabsKeyboardFocus (false);
        for (auto* child : c.getChildren())
            clearFocus (*child);
    };
    clearFocus (*this);

    startTimerHz (30);
}

K808Editor::~K808Editor()
{
    stopTimer();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void K808Editor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
}

void K808Editor::resized()
{
    canvas.setTransform (AffineTransform::scale ((float) getWidth() / (float) designWidth));
    processor.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}

// Direct2D can leave the window blank on some Windows GPUs/drivers inside hosts,
// so the editor always uses JUCE's software renderer.
void K808Editor::useSoftwareRenderer()
{
   #if JUCE_WINDOWS
    if (auto* peer = getPeer())
        if (peer->getCurrentRenderingEngine() != 0 && peer->getAvailableRenderingEngines().size() > 1)
            peer->setCurrentRenderingEngine (0);
   #endif
}

void K808Editor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();
    useSoftwareRenderer();
}

void K808Editor::visibilityChanged()
{
    AudioProcessorEditor::visibilityChanged();
    useSoftwareRenderer();
}

void K808Editor::refreshPresetLabel()
{
    const auto name = processor.presets.getCurrentName();
    const auto modified = processor.presets.isModified();
    if (name == shownPreset && modified == shownModified)
        return;

    shownPreset = name;
    shownModified = modified;

    const auto index = processor.presets.getCurrentIndex();
    const auto& list = processor.presets.getPresets();
    auto category = isPositiveAndBelow (index, list.size()) ? list.getReference (index).category : String();
    if (category.isEmpty() || category == name.toUpperCase())
        category = {};
    else
        category += " / ";
    presetButton.setButtonText (category + name.toUpperCase() + (modified ? " *" : ""));
}

void K808Editor::savePresetAs()
{
    saveDialog = std::make_unique<AlertWindow> ("SAVE PRESET", "Name your preset:", MessageBoxIconType::NoIcon);
    saveDialog->setLookAndFeel (&lnf);
    saveDialog->addTextEditor ("name", processor.presets.getCurrentName());
    saveDialog->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    saveDialog->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    saveDialog->enterModalState (true, ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveDialog != nullptr)
        {
            const auto name = saveDialog->getTextEditorContents ("name");
            if (! processor.presets.saveUser (name))
                AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "DRUM KILLA", "Could not save the preset.");
        }
        saveDialog.reset();
        shownPreset = {};
        refreshPresetLabel();
    }), false);
}

void K808Editor::timerCallback()
{
    useSoftwareRenderer();

    auto& e = processor.engine;
    const auto outPeak = e.outPeak.exchange (0.0f);
    e.inPeak.exchange (0.0f);

    const auto outDb = Decibels::gainToDecibels (outPeak, -100.0f);
    if (outDb >= peakHoldDb || --peakHoldTicks <= 0)
    {
        peakHoldDb = outDb;
        peakHoldTicks = 45;
    }
    const auto clipNow = e.clipDb.exchange (0.0f);
    if (clipNow >= clipHoldDb || --clipHoldTicks <= 0)
    {
        clipHoldDb = clipNow;
        clipHoldTicks = 30;
    }
    const auto duckNow = e.duckDb.exchange (0.0f);

    String status = "OUT " + (peakHoldDb <= -60.0f ? String ("-inf") : String (peakHoldDb, 1))
                  + "   CLIP -" + String (clipHoldDb, 1);
    if (e.sidechainActive.load())
        status << "   DUCK -" << String (duckNow, 1);

    const auto phoneOn = processor.apvts.getRawParameterValue (ParamIDs::phonePreview)->load() > 0.5f;
    head.tick (outPeak, phoneOn, status);

    refreshPresetLabel();
}
