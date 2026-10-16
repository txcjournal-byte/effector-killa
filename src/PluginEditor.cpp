#include "PluginEditor.h"
#include "ui/Palette.h"
#include "ui/Fonts.h"

using namespace bd;

namespace
{
    constexpr float kTopFrac  = 0.095f;   // top bar height / editor height
    constexpr float kDockFrac = 0.33f;    // dock height / editor height
}

//==============================================================================
BendDemonEditor::BendDemonEditor (BendDemonProcessor& p)
    : AudioProcessorEditor (p), processor (p), topBar (p), voiceField (p.getVisualFifo())
{
    setLookAndFeel (&lnf);
    auto& s = p.getAPVTS();

    addAndMakeVisible (voiceField);
    addAndMakeVisible (topBar);

    // ---------------------------------------------------------------- gesture pads
    struct PadDef { const char* id; const char* title; GesturePad::Icon icon; juce::Colour col; Section section; };
    const PadDef padDefs[] = {
        { ids::glideOn,      "GLIDE",       GesturePad::Icon::glide,      pal::cyan,    Section::glide },
        { ids::chordGlideOn, "CHORD GLIDE", GesturePad::Icon::chordGlide, pal::violet,  Section::glide },
        { ids::vibratoOn,    "VIBRATO",     GesturePad::Icon::vibrato,    pal::magenta, Section::vibrato },
        { ids::diveOn,       "808 DIVE",    GesturePad::Icon::dive,       pal::amber,   Section::dive },
        { ids::riseOn,       "RISE",        GesturePad::Icon::rise,       pal::lime,    Section::dive },
        { ids::wobbleOn,     "WOBBLE",      GesturePad::Icon::wobble,     pal::violet,  Section::motion },
        { ids::shakeOn,      "SHAKE",       GesturePad::Icon::shake,      pal::magenta, Section::motion },
        { ids::trillOn,      "TRILL",       GesturePad::Icon::trill,      pal::cyan,    Section::motion },
    };

    for (auto& d : padDefs)
    {
        auto pad = std::make_unique<GesturePad> (s, d.id, d.title, d.icon, d.col);
        const auto section = d.section;
        auto* raw = pad.get();
        pad->onSelect = [this, section, raw]
        {
            showSection (section);
            for (auto& other : pads) other->setSelected (other.get() == raw);
        };
        addAndMakeVisible (*pad);
        pads.push_back (std::move (pad));
    }

    // ---------------------------------------------------------------- section controls
    auto knob = [&] (const char* id, const char* cap, juce::Colour c) -> juce::Component*
    {
        owned.push_back (std::make_unique<GlowKnob> (s, id, cap, c));
        addChildComponent (*owned.back());
        return owned.back().get();
    };
    auto choice = [&] (const char* id, const char* cap) -> juce::Component*
    {
        owned.push_back (std::make_unique<ChoiceBox> (s, id, cap));
        addChildComponent (*owned.back());
        return owned.back().get();
    };
    auto pill = [&] (const char* id, const char* text) -> juce::Component*
    {
        owned.push_back (std::make_unique<PillToggle> (s, id, text));
        addChildComponent (*owned.back());
        return owned.back().get();
    };

    sections.push_back ({ Section::glide, "GLIDE", {
        knob (ids::glideTime,   "Glide Time",   pal::cyan),
        knob (ids::glideWindow, "Glide Window", pal::cyan),
        knob (ids::chordWindow, "Chord Window", pal::violet),
        choice (ids::glideCurve, "Curve") } });

    sections.push_back ({ Section::vibrato, "VIBRATO", {
        knob (ids::vibRate,  "Rate",  pal::magenta),
        knob (ids::vibDepth, "Depth", pal::magenta),
        knob (ids::vibDelay, "Delay", pal::magenta),
        pill (ids::vibSync, "Sync"),
        choice (ids::vibDiv, "Division") } });

    sections.push_back ({ Section::dive, "808 DIVE / RISE", {
        knob (ids::diveAmount, "Dive Amount", pal::amber),
        knob (ids::diveTime,   "Dive Time",   pal::amber),
        pill (ids::diveSync, "Sync"),
        choice (ids::diveDiv,   "Division"),
        choice (ids::diveCurve, "Curve") } });

    sections.push_back ({ Section::motion, "WOBBLE / SHAKE / TRILL", {
        knob (ids::wobbleDepth, "Wobble",      pal::violet),
        choice (ids::wobbleDiv, "Wobble Rate"),
        knob (ids::shakeDepth,  "Shake",       pal::magenta),
        knob (ids::shakeRate,   "Shake Rate",  pal::magenta),
        choice (ids::trillDiv,  "Trill Rate") } });

    sections.push_back ({ Section::expression, "PRESSURE / TIMBRE", {
        knob (ids::pressure,      "Pressure",     pal::lime),
        knob (ids::pressureTime,  "Press Time",   pal::lime),
        choice (ids::pressureShape, "Press Shape"),
        knob (ids::velToPressure, "Vel>Press",    pal::lime),
        knob (ids::timbre,        "Timbre",       pal::amber),
        knob (ids::timbreTime,    "Timbre Time",  pal::amber),
        choice (ids::timbreShape, "Timbre Shape"),
        knob (ids::velToTimbre,   "Vel>Timbre",   pal::amber) } });

    // ---------------------------------------------------------------- always-visible
    scaleLock = std::make_unique<PillToggle> (s, ids::scaleLockOn, "Scale Lock");
    quantize  = std::make_unique<PillToggle> (s, ids::quantizeInput, "Quantize In");
    midiOut   = std::make_unique<PillToggle> (s, ids::midiOutMode, "MIDI Out");
    scaleKey  = std::make_unique<ChoiceBox>  (s, ids::scaleKey, "Key");
    scaleType = std::make_unique<ChoiceBox>  (s, ids::scaleType, "Scale");
    bendRange = std::make_unique<ChoiceBox>  (s, ids::bendRange, "Bend Range");
    voicesKnob = std::make_unique<GlowKnob>  (s, ids::voices, "Voices", pal::cyan);
    gainKnob   = std::make_unique<GlowKnob>  (s, ids::outputGain, "Output", pal::violet);

    midiOut->setTooltip ("Send the generated MPE to BEND DEMON's MIDI output instead of the hosted synth");
    quantize->setTooltip ("Snap incoming notes to the scale");

    for (juce::Component* c : std::initializer_list<juce::Component*> { scaleLock.get(), quantize.get(), midiOut.get(), scaleKey.get(),
                                 scaleType.get(), bendRange.get(), voicesKnob.get(), gainKnob.get() })
        addAndMakeVisible (c);

    // expression section toggle via a small button row: reuse pill toggles for pressure/timbre on/off
    {
        auto pOn = std::make_unique<PillToggle> (s, ids::pressureOn, "Pressure");
        auto tOn = std::make_unique<PillToggle> (s, ids::timbreOn, "Timbre");
        sections.back().items.insert (sections.back().items.begin(), { pOn.get(), tOn.get() });
        addChildComponent (*pOn); addChildComponent (*tOn);
        owned.push_back (std::move (pOn)); owned.push_back (std::move (tOn));
    }

    // ---------------------------------------------------------------- section tabs
    {
        const std::pair<const char*, Section> tabDefs[] = {
            { "GLIDE", Section::glide }, { "VIBRATO", Section::vibrato }, { "DIVE", Section::dive },
            { "MOTION", Section::motion }, { "EXPRESSION", Section::expression } };

        for (auto& [name, sec] : tabDefs)
        {
            auto* b = tabs.add (new juce::TextButton (name));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4711);
            b->setColour (juce::TextButton::textColourOnId, pal::ground0);
            const auto section = sec;
            b->onClick = [this, section]
            {
                showSection (section);
                for (auto& pad : pads) pad->setSelected (false);
            };
            addAndMakeVisible (b);
        }
    }

    // ---------------------------------------------------------------- top bar + browser
    topBar.onLoadSynth = [this] { showSynthBrowser (true); };
    topBar.onDimensionToggled = [this]
    {
        const bool on = topBar.getDimensionButton().getToggleState();
        voiceField.setDimension (on);
        processor.getUiState().setProperty ("dimension", on, nullptr);
    };

    browser = std::make_unique<SynthBrowser> (processor.getHost());
    browser->onClose = [this] { showSynthBrowser (false); };
    addChildComponent (*browser);

    processor.getHost().addChangeListener (this);

    // restore UI state (read the saved width before any resize callback can overwrite it)
    auto ui = processor.getUiState();
    const int savedWidth = (int) ui.getProperty ("width", defaultWidth);
    const bool dim = ui.getProperty ("dimension", false);
    topBar.getDimensionButton().setToggleState (dim, juce::dontSendNotification);
    voiceField.setDimension (dim);
    voiceField.setBars ((int) ui.getProperty ("bars", 4));

    pads.front()->setSelected (true);
    showSection (Section::glide);
    updateHint();

    // ---------------------------------------------------------------- size
   #if BEND_DEMON_USE_OPENGL
    glContext.setComponentPaintingEnabled (true);
    glContext.setContinuousRepainting (false);
    glContext.attachTo (*this);
   #endif

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 1920, 1200);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) defaultWidth / (double) defaultHeight);

    const int w = savedWidth;
    setSize (juce::jlimit (minWidth, 1920, w), juce::jlimit (minHeight, 1200, juce::roundToInt (w * (double) defaultHeight / defaultWidth)));

    sizeRestored = true;
    startMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

BendDemonEditor::~BendDemonEditor()
{
    stopTimer();
    processor.getHost().removeChangeListener (this);
    processor.getUiState().setProperty ("bars", voiceField.getBars(), nullptr);

   #if BEND_DEMON_USE_OPENGL
    glContext.detach();
   #endif

    setLookAndFeel (nullptr);
}

//==============================================================================
void BendDemonEditor::timerCallback()
{
    const float t = (float) ((juce::Time::getMillisecondCounterHiRes() - startMs) * 0.001);
    lastFrameTime = t;

    voiceField.tick (t);
    topBar.tick (t);

    // breathing phase for active pads (~0.6 Hz), staggered so they don't pulse in lockstep
    for (size_t i = 0; i < pads.size(); ++i)
        pads[i]->setPhase (t * 0.6f + (float) i * 0.13f);
}

void BendDemonEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    topBar.refresh();
    updateHint();
}

void BendDemonEditor::updateHint()
{
    auto& host = processor.getHost();
    if (processor.isMidiOutMode())
        voiceField.setHintText ("MIDI OUT MODE\nRoute BEND DEMON's MIDI output to an MPE synth on another channel.");
    else if (! host.hasPlugin())
        voiceField.setHintText ("LOAD A SYNTH TO BEGIN\nPick an MPE-capable synth (Serum 2, Vital, Pigments, Phase Plant ...),\nthen play or write a melody in the piano roll.");
    else
        voiceField.setHintText ("PLAY A MELODY\n" + host.getPluginName() + " is listening in MPE.");
}

void BendDemonEditor::showSynthBrowser (bool show)
{
    browser->setVisible (show);
    if (show)
    {
        browser->toFront (true);
        browser->grabKeyboardFocus();
    }
}

void BendDemonEditor::showSection (Section sec)
{
    currentSection = sec;
    const int idx = (int) sec;
    if (juce::isPositiveAndBelow (idx, tabs.size()))
        tabs[idx]->setToggleState (true, juce::dontSendNotification);
    for (auto& sc : sections)
        for (auto* c : sc.items)
            c->setVisible (sc.section == sec);
    resized();
    repaint();
}

//==============================================================================
void BendDemonEditor::resized()
{
    auto r = getLocalBounds();
    const int h = getHeight();

    topBar.setBounds (r.removeFromTop (juce::roundToInt (h * kTopFrac)));
    dockArea = r.removeFromBottom (juce::roundToInt (h * kDockFrac));
    voiceField.setBounds (r);
    browser->setBounds (getLocalBounds());

    layoutDock (dockArea);

    if (sizeRestored)
        processor.getUiState().setProperty ("width", getWidth(), nullptr);
}

void BendDemonEditor::layoutDock (juce::Rectangle<int> area)
{
    const float scale = (float) getWidth() / (float) defaultWidth;
    auto r = area.reduced (juce::roundToInt (16 * scale), juce::roundToInt (12 * scale));

    // row 1: pads
    auto padRow = r.removeFromTop (juce::roundToInt (r.getHeight() * 0.44f));
    const int gap = juce::roundToInt (8 * scale);
    const int padW = (padRow.getWidth() - gap * ((int) pads.size() - 1)) / (int) pads.size();
    for (auto& p : pads)
    {
        p->setBounds (padRow.removeFromLeft (padW));
        padRow.removeFromLeft (gap);
    }

    r.removeFromTop (juce::roundToInt (12 * scale));

    // row 2: [section controls ........][global controls]
    globalArea  = r.removeFromRight (juce::roundToInt (r.getWidth() * 0.42f));
    sectionArea = r.withTrimmedRight (juce::roundToInt (18 * scale));

    // tabs in the caption row
    {
        auto tabRow = sectionArea.withHeight (juce::roundToInt (20 * scale)).withTrimmedLeft (juce::roundToInt (170 * scale));
        const int tw = tabRow.getWidth() / juce::jmax (1, tabs.size());
        for (auto* t : tabs)
            t->setBounds (tabRow.removeFromLeft (tw).reduced (2, 0));
    }

    // section controls: knobs get square cells, choices/pills share a column
    for (auto& sc : sections)
    {
        if (sc.section != currentSection) continue;

        auto row = sectionArea.withTrimmedTop (juce::roundToInt (26 * scale));
        const int cellH = row.getHeight();
        int knobs = 0, others = 0;
        for (auto* c : sc.items) (dynamic_cast<GlowKnob*> (c) != nullptr ? knobs : others)++;

        const int sideW = others > 0 ? juce::roundToInt (cellH * 1.25f) : 0;
        const int knobW = knobs > 0 ? juce::jmin (cellH, (row.getWidth() - sideW - 8) / knobs) : 0;

        auto side = row.removeFromRight (sideW);
        const int sideItemH = others > 0 ? juce::jmin (juce::roundToInt (40 * scale), side.getHeight() / others) : 0;

        for (auto* c : sc.items)
        {
            if (dynamic_cast<GlowKnob*> (c) != nullptr)
                c->setBounds (row.removeFromLeft (knobW).reduced (2));
            else
            {
                auto cell = side.removeFromTop (sideItemH).reduced (0, 2);
                if (dynamic_cast<PillToggle*> (c) != nullptr)
                    cell = cell.withSizeKeepingCentre (cell.getWidth(), juce::jmin (cell.getHeight(), juce::roundToInt (24 * scale)));
                c->setBounds (cell);
            }
        }
    }

    // global controls: scale lock block + routing block
    auto g = globalArea.withTrimmedTop (juce::roundToInt (16 * scale));
    auto knobs = g.removeFromRight (juce::roundToInt (g.getHeight() * 1.6f));
    voicesKnob->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2).reduced (2));
    gainKnob->setBounds (knobs.reduced (2));
    g.removeFromRight (juce::roundToInt (10 * scale));

    const int rowH = g.getHeight() / 3;
    auto row1 = g.removeFromTop (rowH).reduced (0, 2);
    auto row2 = g.removeFromTop (rowH).reduced (0, 2);
    auto row3 = g.reduced (0, 2);
    const int pillH = juce::jmin (row1.getHeight(), juce::roundToInt (24 * scale));

    scaleLock->setBounds (row1.removeFromLeft (row1.getWidth() / 2 - 4).withSizeKeepingCentre (row1.getWidth() / 2 - 4, pillH));
    row1.removeFromLeft (8);
    quantize->setBounds (row1.withSizeKeepingCentre (row1.getWidth(), pillH));

    scaleKey->setBounds (row2.removeFromLeft (row2.getWidth() / 3));
    row2.removeFromLeft (6);
    scaleType->setBounds (row2);

    bendRange->setBounds (row3.removeFromLeft (row3.getWidth() / 2 - 4));
    row3.removeFromLeft (8);
    midiOut->setBounds (row3.withTrimmedTop (row3.getHeight() / 3).withSizeKeepingCentre (row3.getWidth(), pillH));
}

//==============================================================================
void BendDemonEditor::paint (juce::Graphics& g)
{
    g.fillAll (pal::ground0);

    // dock: glass panel with a glowing top edge
    auto dock = dockArea.toFloat();
    juce::ColourGradient bg (pal::ground1.withAlpha (0.9f), dock.getCentreX(), dock.getY(),
                             pal::ground0, dock.getCentreX(), dock.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (dock);

    juce::ColourGradient edge (pal::cyan.withAlpha (0.0f), dock.getX(), dock.getY(), pal::magenta.withAlpha (0.0f), dock.getRight(), dock.getY(), false);
    edge.addColour (0.25, pal::cyan.withAlpha (0.45f));
    edge.addColour (0.75, pal::magenta.withAlpha (0.45f));
    g.setGradientFill (edge);
    g.fillRect (dock.withHeight (1.0f));

    const float scale = (float) getWidth() / (float) defaultWidth;

    // section caption
    juce::String title;
    for (auto& sc : sections) if (sc.section == currentSection) title = sc.title;

    g.setFont (fonts::mono (11.0f * juce::jmax (0.85f, scale), true));
    g.setColour (pal::cyan);
    g.drawText (title, sectionArea.withHeight (juce::roundToInt (20 * scale)), juce::Justification::centredLeft);

    g.setColour (pal::textDim);
    g.drawText ("SCALE / MPE / OUTPUT", globalArea.withHeight (juce::roundToInt (14 * scale)), juce::Justification::topLeft);

    g.setColour (pal::hairline.withAlpha (0.5f));
    g.fillRect ((float) globalArea.getX() - 9.0f * scale, (float) globalArea.getY() + 4.0f, 1.0f, (float) globalArea.getHeight() - 8.0f);
}

void BendDemonEditor::paintOverChildren (juce::Graphics&) {}
