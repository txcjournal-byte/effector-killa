#include "Inspector.h"
#include "LineEditor.h"
#include "ShapeInfo.h"
#include "Palette.h"
#include "Fonts.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../engine/InternalSynth.h"

namespace bd
{

namespace
{
    void drawCaption (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, float scale)
    {
        if (r.isEmpty()) return;
        g.setColour (pal::textDim);
        g.setFont (fonts::ui (juce::jmax (10.0f, 12.0f * scale), true));
        g.drawFittedText (text.toUpperCase(), r, juce::Justification::centredLeft, 1, 0.8f);
    }

    void styleBar (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setScrollWheelEnabled (true);
    }

    constexpr int kLiveFeel = 8;   // 9th tile: loudness / brightness envelopes of live notes
}

//==============================================================================
void ShapeButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool enabled = isEnabled();
    const bool hot = enabled && over;

    g.setColour (enabled && down ? pal::ground1 : (hot ? pal::raised : pal::panel));
    g.fillRoundedRectangle (r, pal::metric::corner);
    g.setColour (hot ? pal::textFaint : pal::hairline);
    g.drawRoundedRectangle (r, pal::metric::corner, 1.0f);

    auto c = r.reduced (r.getHeight() * 0.22f, r.getHeight() * 0.24f);
    auto iconArea = c.removeFromLeft (c.getHeight() * 1.7f);
    const auto path = GesturePad::iconPath (icon, iconArea);
    if (enabled)
    {
        ColourGradient grad (pal::cyan, iconArea.getX(), 0.0f, pal::magenta, iconArea.getRight(), 0.0f, false);
        g.setGradientFill (grad);
    }
    else
        g.setColour (pal::textFaint);
    g.strokePath (path, PathStrokeType (jmax (1.2f, r.getHeight() * 0.05f), PathStrokeType::curved, PathStrokeType::rounded));

    c.removeFromLeft (r.getHeight() * 0.25f);
    g.setColour (enabled ? (hot ? pal::text : pal::text.withAlpha (0.9f)) : pal::textFaint);
    g.setFont (fonts::ui (jlimit (10.0f, 13.5f, r.getHeight() * 0.40f), true));
    g.drawFittedText (getButtonText(), c.toNearestInt(), Justification::centredLeft, 1, 0.7f);
}

//==============================================================================
Inspector::Inspector (BendDemonProcessor& p, LineEditController& c) : processor (p), controller (c)
{
    auto& apvts = processor.getAPVTS();

    const char* tabNames[] = { "NOTE", "SHAPES", "SOUND" };
    const char* tabTips[] = { "The selected notes: velocity, vibrato, apply a shape",
                              "How each shape behaves, and whether it also bends your piano-roll notes live",
                              "The sound that plays: built-in sounds or your own plugin" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = tabButtons.add (new juce::TextButton (tabNames[i]));
        b->setTooltip (tabTips[i]);
        b->onClick = [this, i] { setTab ((Tab) i); if (onTabChanged) onTabChanged(); };
        addAndMakeVisible (b);
    }

    // ---------------------------------------------------------------- NOTE
    velocity = &makeSlider ("Velocity");
    velocity->setRange (1.0, 127.0, 1.0);
    velocity->textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)); };
    velocity->setTooltip ("VELOCITY - how hard the selected notes are hit");
    releaseVel = &makeSlider ("Release");
    releaseVel->setRange (0.0, 127.0, 1.0);
    releaseVel->textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)); };
    releaseVel->setTooltip ("RELEASE VELOCITY - how fast the notes are let go (synths that use it change their release)");
    vibDepth = &makeSlider ("Amount");
    vibDepth->setRange (0.0, 200.0, 1.0);
    vibDepth->setSkewFactorFromMidPoint (50.0);
    vibDepth->textFromValueFunction = [] (double v) { return v < 0.5 ? juce::String ("off") : juce::String (juce::roundToInt (v)) + " ct"; };
    vibDepth->setTooltip ("PITCH MOD AMOUNT - how far the pitch moves (100 ct = one semitone, 0 = off)");
    vibFade = &makeSlider ("Fade");
    vibFade->setRange (0.0, 4.0, 0.05);
    vibFade->textFromValueFunction = [] (double v) { return juce::String (v, 2) + " b"; };
    vibFade->setTooltip ("PITCH MOD FADE-IN - beats until the modulation is at full amount");

    velocity->onValueChange = [this] { const auto v = (float) (velocity->getValue() / 127.0);
                                       editSelected ([v] (PatternLine& l) { l.velocity = v; }, "Velocity", sliderDragging); };
    releaseVel->onValueChange = [this] { const auto v = (float) (releaseVel->getValue() / 127.0);
                                       editSelected ([v] (PatternLine& l) { l.releaseVelocity = v; }, "Release velocity", sliderDragging); };
    vibDepth->onValueChange = [this] { const auto v = (float) (vibDepth->getValue() * 0.01);
                                       editSelected ([v] (PatternLine& l) { l.vibDepth = v; }, "Pitch mod amount", sliderDragging); };
    vibFade->onValueChange  = [this] { const auto v = (float) vibFade->getValue();
                                       editSelected ([v] (PatternLine& l) { l.vibFade = v; }, "Pitch mod fade", sliderDragging); };

    const char* shapes[] = { "Sine", "Triangle", "Square", "Saw up", "Saw down" };
    for (int i = 0; i < 5; ++i) lfoShape.addItem (shapes[i], i + 1);
    lfoShape.setTooltip ("PITCH MOD SHAPE - the wave the pitch follows");
    lfoShape.onChange = [this]
    {
        const int sh = lfoShape.getSelectedItemIndex();
        if (sh >= 0) editSelected ([sh] (PatternLine& l) { l.vibShape = sh; if (l.vibDepth <= 0.0f) l.vibDepth = 0.3f; }, "Pitch mod shape", false);
    };
    for (int i = 0; i < numSyncDivisions; ++i) lfoSpeed.addItem (syncDivisionNames[i], i + 1);
    lfoSpeed.setTooltip ("PITCH MOD SPEED - one wave every ... (follows the tempo)");
    lfoSpeed.onChange = [this]
    {
        const int i = lfoSpeed.getSelectedItemIndex();
        if (i >= 0)
        {
            const auto rate = (float) (1.0 / syncDivisionBeats (i));
            editSelected ([rate] (PatternLine& l) { l.vibRate = rate; }, "Pitch mod speed", false);
        }
    };
    polarity.setTooltip ("BIPOLAR swings up and down, UNIPOLAR only goes up from the note");
    polarity.onClick = [this]
    {
        const bool uni = polarity.getButtonText() == "BIPOLAR";
        editSelected ([uni] (PatternLine& l) { l.vibUnipolar = uni; }, "Pitch mod polarity", false);
    };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &lfoShape, &lfoSpeed, &polarity })
        addAndMakeVisible (c);

    smoothCurve.setTooltip ("SMOOTH - the pitch flows in soft curves through the points");
    straightCurve.setTooltip ("STRAIGHT - the pitch moves in straight lines between the points");
    smoothCurve.onClick   = [this] { editSelected ([] (PatternLine& l) { l.smooth = true; },  "Smooth curve", false); };
    straightCurve.onClick = [this] { editSelected ([] (PatternLine& l) { l.smooth = false; }, "Straight segments", false); };
    addAndMakeVisible (smoothCurve);
    addAndMakeVisible (straightCurve);

    const std::pair<const char*, float> tr[] = { { "-12", -12.0f }, { "-1", -1.0f }, { "+1", 1.0f }, { "+12", 12.0f } };
    for (auto& [name, semis] : tr)
    {
        auto* b = transposeButtons.add (new juce::TextButton (name));
        const float st = semis;
        b->setTooltip (std::abs (st) > 11.0f ? "TRANSPOSE one octave (Shift + Up / Down)" : "TRANSPOSE one semitone (Up / Down)");
        b->onClick = [this, st] { controller.transposeSelection (st); };
        addAndMakeVisible (b);
    }

    for (int i = 0; i < 8; ++i)
    {
        const auto& info = shapeInfo (i);
        auto* b = applyButtons.add (new ShapeButton (info.name, info.icon));
        b->setTooltip (info.hint);
        b->onClick = [this, i] { controller.applyGesture ((tools::Gesture) i); };
        addAndMakeVisible (b);
    }

    const char* actions[] = { "SMOOTH OUT", "QUANTIZE", "DUPLICATE", "DELETE" };
    const char* actionTips[] = { "SMOOTH OUT - irons out shaky hand-drawn curves (M)", "QUANTIZE - moves the note starts onto the grid (Q)",
                                 "DUPLICATE - copies the notes right after themselves (Ctrl+D)", "DELETE the selected notes (Del)" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = actionButtons.add (new juce::TextButton (actions[i]));
        b->setTooltip (actionTips[i]);
        b->onClick = [this, i]
        {
            switch (i)
            {
                case 0: controller.smoothSelection(); break;
                case 1: controller.quantizeSelection(); break;
                case 2: controller.duplicate(); break;
                default: controller.deleteSelection(); break;
            }
        };
        addAndMakeVisible (b);
    }

    // ---------------------------------------------------------------- SHAPES
    for (int i = 0; i < 9; ++i)
    {
        const bool feel = i == kLiveFeel;
        const auto& info = shapeInfo (juce::jmin (i, 7));
        auto tile = std::make_unique<GesturePad> (apvts, feel ? ids::pressureOn : info.liveParam,
                                                  feel ? "Live feel" : info.name, feel ? GesturePad::Icon::rise : info.icon, pal::cyan);
        tile->setSelectOnly (true);
        tile->setTooltip (feel ? juce::String ("LIVE FEEL - loudness and brightness envelopes for piano-roll notes")
                               : juce::String (info.name).toUpperCase() + " settings  (dot = also used live on piano-roll notes)");
        tile->onSelect = [this, i] { showShape (i); };
        addAndMakeVisible (*tile);
        tiles.push_back (std::move (tile));
    }

    using P = std::vector<juce::Component*>;
    auto S = [this] (const char* id, const char* cap) { return static_cast<juce::Component*> (&makeParamSlider (id, cap)); };
    auto C = [this] (const char* id, const char* cap) { return makeChoice (id, cap); };
    auto T = [this] (const char* id, const char* cap) { return makePill (id, cap); };
    const char* live = "Use on piano-roll notes";

    shapeItems.push_back (P { T (ids::glideOn, live), S (ids::glideTime, "Slide time"), S (ids::glideWindow, "Max gap"), C (ids::glideCurve, "Curve") });
    shapeItems.push_back (P { T (ids::chordGlideOn, live), S (ids::glideTime, "Slide time"), S (ids::chordWindow, "Chord window"), C (ids::glideCurve, "Curve") });
    shapeItems.push_back (P { T (ids::vibratoOn, live), S (ids::vibRate, "Speed"), S (ids::vibDepth, "Depth"), S (ids::vibDelay, "Delay"),
                              T (ids::vibSync, "Sync to tempo"), C (ids::vibDiv, "Sync rate") });
    shapeItems.push_back (P { T (ids::diveOn, live), S (ids::diveAmount, "Dive amount"), S (ids::diveTime, "Dive time"),
                              T (ids::diveSync, "Sync to tempo"), C (ids::diveDiv, "Sync length"), C (ids::diveCurve, "Curve") });
    shapeItems.push_back (P { T (ids::riseOn, live), S (ids::diveAmount, "Rise amount"), S (ids::diveTime, "Rise time"),
                              T (ids::diveSync, "Sync to tempo"), C (ids::diveDiv, "Sync length"), C (ids::diveCurve, "Curve") });
    shapeItems.push_back (P { T (ids::wobbleOn, live), S (ids::wobbleDepth, "Depth"), C (ids::wobbleDiv, "Rate") });
    shapeItems.push_back (P { T (ids::shakeOn, live), S (ids::shakeDepth, "Depth"), S (ids::shakeRate, "Speed") });
    shapeItems.push_back (P { T (ids::trillOn, live), C (ids::trillDiv, "Rate") });
    shapeItems.push_back (P { T (ids::pressureOn, "Loudness envelope"), S (ids::pressure, "Loudness"), S (ids::pressureTime, "Loud. time"),
                              C (ids::pressureShape, "Loud. shape"), S (ids::velToPressure, "Vel > loud"),
                              T (ids::timbreOn, "Brightness envelope"), S (ids::timbre, "Brightness"), S (ids::timbreTime, "Bright. time"),
                              C (ids::timbreShape, "Bright. shape"), S (ids::velToTimbre, "Vel > bright"),
                              T (ids::quantizeInput, "Snap piano-roll notes to scale") });

    // ---------------------------------------------------------------- SOUND
    const char* soundTips[] = { "GLIDE LEAD - bright saw lead made for slides", "PLUCK - short, snappy pluck",
                                "808 - sine bass with drive that follows every slide", "PAD - soft wide pad",
                                "BELL - glassy FM bell", "SUB BASS - clean sub" };
    for (int i = 0; i < InternalSynth::numSounds; ++i)
    {
        auto* b = soundButtons.add (new juce::TextButton (juce::String (InternalSynth::getSoundName (i)).toUpperCase()));
        b->setTooltip (soundTips[i]);
        b->onClick = [this, i] { processor.setInternalSound (i); processor.setUseInternalSynth (true); };
        addAndMakeVisible (b);
    }
    loadSynth.setTooltip ("LOAD PLUGIN - play your own synth (Serum 2, Vital, Pigments ...) inside BEND DEMON");
    loadSynth.onClick = [this] { if (onLoadSynth) onLoadSynth(); };
    useHosted.setTooltip ("USE - play the loaded plugin instead of the built-in sound");
    useHosted.onClick = [this] { processor.setUseInternalSynth (processor.getHost().hasPlugin() && ! processor.isUsingInternalSynth()); };
    editHosted.setTooltip ("EDIT - open the plugin's own window to pick a patch");
    editHosted.onClick = [this] { processor.getHost().showEditorWindow(); };
    for (auto* b : { &loadSynth, &useHosted, &editHosted }) addAndMakeVisible (b);

    volume = &makeParamSlider (ids::outputGain, "Volume");
    volume->setTooltip ("VOLUME - output level of BEND DEMON");

    showShape (0);
    setTab (Tab::note);
}

Inspector::~Inspector()
{
    attachments.clear();
}

juce::Slider& Inspector::makeSlider (const juce::String& name)
{
    auto s = std::make_unique<juce::Slider> (name);
    styleBar (*s);
    s->onDragStart = [this] { sliderDragging = true; processor.getPatternStore().beginDrag(); };
    s->onDragEnd   = [this, raw = s.get()] { sliderDragging = false; processor.getPatternStore().commitDrag (raw->getName()); };
    addAndMakeVisible (*s);
    noteSliders.push_back (std::move (s));
    return *noteSliders.back();
}

juce::Slider& Inspector::makeParamSlider (const char* id, const juce::String& caption)
{
    auto s = std::make_unique<juce::Slider> (caption);
    styleBar (*s);
    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getAPVTS(), id, *s));
    if (auto* p = processor.getAPVTS().getParameter (id))
    {
        s->setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s->setTooltip (caption.toUpperCase() + " - " + p->getName (64) + " (double-click = default)");
    }
    addChildComponent (*s);
    auto* raw = s.get();
    owned.push_back (std::move (s));
    return *raw;
}

juce::Component* Inspector::makeChoice (const char* id, const juce::String& caption)
{
    auto c = std::make_unique<ChoiceBox> (processor.getAPVTS(), id, caption);
    c->setCaptionFraction (0.46f);
    addChildComponent (*c);
    owned.push_back (std::move (c));
    return owned.back().get();
}

juce::Component* Inspector::makePill (const char* id, const juce::String& caption)
{
    auto c = std::make_unique<PillToggle> (processor.getAPVTS(), id, caption);
    addChildComponent (*c);
    owned.push_back (std::move (c));
    return owned.back().get();
}

//==============================================================================
void Inspector::setTab (Tab t)
{
    tab = t;
    for (int i = 0; i < tabButtons.size(); ++i)
        tabButtons[i]->setToggleState (i == (int) t, juce::dontSendNotification);
    updateVisibility();
    resized();
    repaint();
}

void Inspector::showShape (int i)
{
    shape = juce::jlimit (0, kLiveFeel, i);
    for (size_t k = 0; k < tiles.size(); ++k)
        tiles[k]->setSelected ((int) k == shape);
    updateVisibility();
    resized();
    repaint();
}

void Inspector::updateVisibility()
{
    const bool N = tab == Tab::note, H = tab == Tab::shapes, S = tab == Tab::sound;

    for (auto& s : noteSliders) s->setVisible (N);
    for (auto* b : { &smoothCurve, &straightCurve }) b->setVisible (N);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &lfoShape, &lfoSpeed, &polarity }) c->setVisible (N);
    for (auto* b : transposeButtons) b->setVisible (N);
    for (auto* b : applyButtons) b->setVisible (N);
    for (auto* b : actionButtons) b->setVisible (false);   // in MORE / right-click / keys

    for (auto& t : tiles) t->setVisible (H);
    for (size_t k = 0; k < shapeItems.size(); ++k)
        for (auto* c : shapeItems[k])
            c->setVisible (H && (int) k == shape);

    for (auto* b : soundButtons) b->setVisible (S);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &loadSynth, &useHosted, &editHosted, volume })
        c->setVisible (S);
}

//==============================================================================
void Inspector::editSelected (std::function<void (PatternLine&)> fn, const juce::String& name, bool live)
{
    auto& store = processor.getPatternStore();
    if (store.getSelection().empty()) return;

    auto p = live && store.isDragging() ? store.getDragStart() : store.get();
    for (auto& l : p.lines)
        if (store.isSelected (l.id)) fn (l);

    if (live && store.isDragging()) store.setLive (std::move (p));
    else                            store.perform (std::move (p), name);
}

void Inspector::refreshNoteValues()
{
    auto& store = processor.getPatternStore();
    const PatternLine* first = nullptr;
    for (auto& l : store.get().lines)
        if (store.isSelected (l.id)) { first = &l; break; }

    const bool on = first != nullptr;
    for (auto& s : noteSliders) s->setEnabled (on);
    for (auto* b : { &smoothCurve, &straightCurve }) b->setEnabled (on);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &lfoShape, &lfoSpeed, &polarity }) c->setEnabled (on);
    for (auto* b : transposeButtons) b->setEnabled (on);
    for (auto* b : applyButtons) b->setEnabled (on);
    for (auto* b : actionButtons) b->setEnabled (on);

    if (! on) return;
    velocity->setValue (first->velocity * 127.0, juce::dontSendNotification);
    releaseVel->setValue (first->releaseVelocity * 127.0, juce::dontSendNotification);
    vibDepth->setValue (first->vibDepth * 100.0, juce::dontSendNotification);
    vibFade->setValue (first->vibFade, juce::dontSendNotification);
    lfoShape.setSelectedItemIndex (first->vibShape, juce::dontSendNotification);
    {
        int best = 0; double err = 1.0e9;
        for (int i = 0; i < numSyncDivisions; ++i)
        {
            const double e = std::abs (1.0 / syncDivisionBeats (i) - (double) first->vibRate);
            if (e < err) { err = e; best = i; }
        }
        lfoSpeed.setSelectedItemIndex (best, juce::dontSendNotification);
    }
    polarity.setButtonText (first->vibUnipolar ? "UNIPOLAR" : "BIPOLAR");
    smoothCurve.setToggleState (first->smooth, juce::dontSendNotification);
    straightCurve.setToggleState (! first->smooth, juce::dontSendNotification);
}

void Inspector::tick (float)
{
    auto& store = processor.getPatternStore();
    if (store.getVersion() != lastVersion && ! sliderDragging)
    {
        lastVersion = store.getVersion();
        refreshNoteValues();
        repaint (noteHeader);
    }

    if (tab == Tab::sound)
    {
        const bool internal = processor.isUsingInternalSynth();
        const int snd = processor.getInternalSound();
        for (int i = 0; i < soundButtons.size(); ++i)
            soundButtons[i]->setToggleState (internal && i == snd, juce::dontSendNotification);

        auto& host = processor.getHost();
        useHosted.setEnabled (host.hasPlugin());
        editHosted.setEnabled (host.hasPlugin());
        useHosted.setToggleState (host.hasPlugin() && ! internal, juce::dontSendNotification);
        const auto name = host.hasPlugin() ? host.getPluginName()
                                           : (host.isLoading() ? juce::String ("Loading...") : juce::String ("No plugin loaded"));
        if (name != hostedName) { hostedName = name; repaint (hostedNameArea); }
    }
}

//==============================================================================
void Inspector::resized()
{
    scale = juce::jmax (0.5f, (float) getParentWidth() / 1040.0f);
    const auto s = scale;
    auto r = getLocalBounds().reduced (juce::roundToInt (10 * s), juce::roundToInt (8 * s));
    r.removeFromLeft (1);

    auto tabsRow = r.removeFromTop (juce::roundToInt (26 * s));
    const int gap2 = juce::roundToInt (2 * s);
    const int tw = (tabsRow.getWidth() - 2 * gap2) / 3;
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i]->setBounds (i < 2 ? tabsRow.removeFromLeft (tw) : tabsRow);
        if (i < 2) tabsRow.removeFromLeft (gap2);
    }
    r.removeFromTop (juce::roundToInt (12 * s));

    switch (tab)
    {
        case Tab::note:   layoutNote (r); break;
        case Tab::shapes: layoutShapes (r); break;
        case Tab::sound:  layoutSound (r); break;
    }
}

void Inspector::layoutNote (juce::Rectangle<int> r)
{
    const auto s = scale;
    const int row = juce::roundToInt (28 * s), gap = juce::roundToInt (4 * s), block = juce::roundToInt (12 * s);
    const int capH = juce::roundToInt (20 * s);

    auto twoCols = [&] (juce::Component& a, juce::Component* b, int h)
    {
        auto rr = r.removeFromTop (h);
        a.setBounds (rr.removeFromLeft (rr.getWidth() / 2).withTrimmedRight (gap / 2));
        if (b != nullptr) b->setBounds (rr.withTrimmedLeft (gap / 2));
        r.removeFromTop (gap);
    };

    noteHeader = r.removeFromTop (capH);
    r.removeFromTop (gap);

    // the big one-click actions come first
    applyCaption = r.removeFromTop (capH);
    for (int i = 0; i < applyButtons.size(); i += 2)
        twoCols (*applyButtons[i], applyButtons[i + 1], juce::roundToInt (34 * s));
    r.removeFromTop (block - gap);

    twoCols (*velocity, releaseVel, row);
    twoCols (smoothCurve, &straightCurve, row);
    r.removeFromTop (block - gap);

    vibCaption = r.removeFromTop (capH);
    twoCols (lfoShape, &polarity, row);
    vibDepth->setBounds (r.removeFromTop (row));
    r.removeFromTop (gap);
    twoCols (lfoSpeed, vibFade, row);
    r.removeFromTop (block - gap);

    transposeCaption = r.removeFromTop (capH);
    {
        auto rr = r.removeFromTop (row);
        const int w = (rr.getWidth() - 3 * gap) / 4;
        for (auto* b : transposeButtons) { b->setBounds (rr.removeFromLeft (w)); rr.removeFromLeft (gap); }
    }
    actionCaption = {};
}

void Inspector::layoutShapes (juce::Rectangle<int> r)
{
    const auto s = scale;
    const int row = juce::roundToInt (28 * s), gap = juce::roundToInt (4 * s);
    const int capH = juce::roundToInt (18 * s);

    tilesCaption = r.removeFromTop (capH);
    for (int i = 0; i < 9; i += 2)
    {
        auto rr = r.removeFromTop (juce::roundToInt (30 * s));
        if (i + 1 < 9)
        {
            tiles[(size_t) i]->setBounds (rr.removeFromLeft ((rr.getWidth() - gap) / 2));
            rr.removeFromLeft (gap);
            tiles[(size_t) i + 1]->setBounds (rr);
        }
        else
            tiles[(size_t) i]->setBounds (rr.removeFromLeft ((rr.getWidth() - gap) / 2));
        r.removeFromTop (gap);
    }
    r.removeFromTop (juce::roundToInt (10 * s));

    shapeTitle = r.removeFromTop (juce::roundToInt (20 * s));
    shapeHint = r.removeFromTop (juce::roundToInt (36 * s));
    r.removeFromTop (gap);

    for (auto* c : shapeItems[(size_t) shape])
    {
        c->setBounds (r.removeFromTop (row));
        r.removeFromTop (gap);
    }
}

void Inspector::layoutSound (juce::Rectangle<int> r)
{
    const auto s = scale;
    const int row = juce::roundToInt (30 * s), gap = juce::roundToInt (4 * s), block = juce::roundToInt (12 * s);
    const int capH = juce::roundToInt (20 * s);

    internalCaption = r.removeFromTop (capH);
    for (int i = 0; i < soundButtons.size(); i += 2)
    {
        auto rr = r.removeFromTop (row);
        soundButtons[i]->setBounds (rr.removeFromLeft (rr.getWidth() / 2).withTrimmedRight (gap / 2));
        if (i + 1 < soundButtons.size()) soundButtons[i + 1]->setBounds (rr.withTrimmedLeft (gap / 2));
        r.removeFromTop (gap);
    }
    r.removeFromTop (block - gap);

    hostedCaption = r.removeFromTop (capH);
    hostedNameArea = r.removeFromTop (row);
    r.removeFromTop (gap);
    {
        auto rr = r.removeFromTop (row);
        const int w = (rr.getWidth() - 2 * gap) / 4;
        editHosted.setBounds (rr.removeFromRight (w));
        rr.removeFromRight (gap);
        useHosted.setBounds (rr.removeFromRight (w));
        rr.removeFromRight (gap);
        loadSynth.setBounds (rr);
    }
    r.removeFromTop (block);

    outputCaption = r.removeFromTop (capH);
    volume->setBounds (r.removeFromTop (row));
    r.removeFromTop (block);
    settingsHint = r.removeFromTop (juce::roundToInt (34 * s));
}

void Inspector::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (pal::ground1);
    g.setColour (pal::hairline);
    g.fillRect (0, 0, 1, getHeight());
    const float s = scale;

    switch (tab)
    {
        case Tab::note:
        {
            const auto n = (int) processor.getPatternStore().getSelection().size();
            g.setFont (fonts::ui (jmax (11.0f, 12.0f * s), true));
            g.setColour (n > 0 ? pal::text : pal::textDim);
            g.drawFittedText (n == 0 ? String ("NO NOTE SELECTED") : String (n) + (n == 1 ? " NOTE SELECTED" : " NOTES SELECTED"),
                              noteHeader, Justification::centredLeft, 1, 0.8f);
            drawCaption (g, vibCaption, "Pitch mod (vibrato)", s);
            drawCaption (g, transposeCaption, "Transpose", s);
            drawCaption (g, applyCaption, "Make the selected notes...", s);
            break;
        }
        case Tab::shapes:
        {
            drawCaption (g, tilesCaption, "Shapes", s);
            const bool feel = shape == kLiveFeel;
            g.setColour (pal::text);
            g.setFont (fonts::ui (jmax (11.0f, 12.5f * s), true));
            g.drawFittedText (feel ? String ("LIVE FEEL") : String (shapeInfo (shape).name).toUpperCase(), shapeTitle, Justification::centredLeft, 1, 0.8f);
            g.setColour (pal::textDim);
            g.setFont (fonts::ui (jmax (10.0f, 12.0f * s)));
            g.drawFittedText (feel ? String ("Loudness (pressure) and brightness (timbre) of the piano-roll notes")
                                   : String (shapeInfo (shape).liveHint) + ". APPLY bakes it into selected notes.",
                              shapeHint, Justification::topLeft, 2, 0.85f);
            break;
        }
        case Tab::sound:
        {
            drawCaption (g, internalCaption, "Built-in sound", s);
            drawCaption (g, hostedCaption, "Your plugin", s);
            drawCaption (g, outputCaption, "Output", s);
            auto r = hostedNameArea.toFloat();
            g.setColour (pal::ground0);
            g.fillRoundedRectangle (r, pal::metric::corner);
            g.setColour (pal::hairline);
            g.drawRoundedRectangle (r.reduced (0.5f), pal::metric::corner, 1.0f);
            g.setColour (processor.getHost().hasPlugin() ? pal::text : pal::textFaint);
            g.setFont (fonts::ui (jmax (10.0f, 12.0f * s)));
            g.drawFittedText (hostedName, hostedNameArea.reduced (8, 0), Justification::centredLeft, 1, 0.8f);
            g.setColour (pal::textDim);
            g.setFont (fonts::ui (jmax (10.0f, 12.0f * s)));
            g.drawFittedText ("MPE, bend range, voices, MIDI out: gear menu in the top bar", settingsHint, Justification::topLeft, 2, 0.85f);
            break;
        }
    }
}

} // namespace bd
