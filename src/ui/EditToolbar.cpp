#include "EditToolbar.h"
#include "VoiceField.h"
#include "Palette.h"
#include "Fonts.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../engine/PatternTools.h"
#include "../Parameters.h"

namespace bd
{

namespace
{
    struct SnapChoice { const char* name; double beats; };
    const SnapChoice snapChoices[] = {
        { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 },
        { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 }, { "OFF", 0.0 } };

    std::unique_ptr<juce::FileChooser>& chooser()
    {
        static std::unique_ptr<juce::FileChooser> c;
        return c;
    }
}

int EditToolbar::numSnapChoices()          { return (int) (sizeof (snapChoices) / sizeof (snapChoices[0])); }
double EditToolbar::snapBeatsFor (int i)    { return snapChoices[juce::jlimit (0, numSnapChoices() - 1, i)].beats; }

//==============================================================================
IconButton::IconButton (const juce::String& name, Icon i, juce::Colour acc)
    : juce::Button (name), icon (i), accent (acc)
{
}

void IconButton::mouseDrag (const juce::MouseEvent& e)
{
    juce::Button::mouseDrag (e);
    if (onDragOut && ! dragStarted && e.getDistanceFromDragStart() > 6)
    {
        dragStarted = true;
        onDragOut (e);
    }
}

juce::Path IconButton::makeIcon (juce::Rectangle<float> r) const
{
    using namespace juce;
    Path p;
    const float x0 = r.getX(), y0 = r.getY(), w = r.getWidth(), h = r.getHeight();
    const auto P = [&] (float fx, float fy) { return Point<float> (x0 + fx * w, y0 + fy * h); };

    switch (icon)
    {
        case Icon::pencil:
            p.startNewSubPath (P (0.12f, 0.88f));
            p.lineTo (P (0.18f, 0.62f));
            p.lineTo (P (0.70f, 0.10f));
            p.lineTo (P (0.90f, 0.30f));
            p.lineTo (P (0.38f, 0.82f));
            p.closeSubPath();
            p.startNewSubPath (P (0.18f, 0.62f));
            p.lineTo (P (0.38f, 0.82f));
            p.startNewSubPath (P (0.60f, 0.20f));
            p.lineTo (P (0.80f, 0.40f));
            break;

        case Icon::line:
            p.startNewSubPath (P (0.12f, 0.80f));
            p.lineTo (P (0.45f, 0.80f));
            p.lineTo (P (0.88f, 0.20f));
            p.addEllipse (Rectangle<float> (w * 0.16f, w * 0.16f).withCentre (P (0.12f, 0.80f)));
            p.addEllipse (Rectangle<float> (w * 0.16f, w * 0.16f).withCentre (P (0.88f, 0.20f)));
            break;

        case Icon::select:
            p.startNewSubPath (P (0.22f, 0.08f));
            p.lineTo (P (0.22f, 0.80f));
            p.lineTo (P (0.40f, 0.63f));
            p.lineTo (P (0.54f, 0.92f));
            p.lineTo (P (0.66f, 0.86f));
            p.lineTo (P (0.52f, 0.58f));
            p.lineTo (P (0.78f, 0.56f));
            p.closeSubPath();
            break;

        case Icon::eraser:
            p.startNewSubPath (P (0.10f, 0.62f));
            p.lineTo (P (0.52f, 0.18f));
            p.lineTo (P (0.90f, 0.52f));
            p.lineTo (P (0.56f, 0.88f));
            p.lineTo (P (0.34f, 0.88f));
            p.closeSubPath();
            p.startNewSubPath (P (0.32f, 0.40f));
            p.lineTo (P (0.70f, 0.74f));
            p.startNewSubPath (P (0.56f, 0.88f));
            p.lineTo (P (0.94f, 0.88f));
            break;

        case Icon::undo:
        case Icon::redo:
        {
            const bool u = icon == Icon::undo;
            auto X = [&] (float f) { return u ? f : 1.0f - f; };
            p.startNewSubPath (P (X (0.30f), 0.30f));
            p.cubicTo (P (X (0.75f), 0.30f), P (X (0.92f), 0.55f), P (X (0.70f), 0.82f));
            p.startNewSubPath (P (X (0.45f), 0.10f));
            p.lineTo (P (X (0.22f), 0.30f));
            p.lineTo (P (X (0.45f), 0.50f));
            break;
        }

        case Icon::play:
            p.addTriangle (P (0.25f, 0.15f), P (0.85f, 0.50f), P (0.25f, 0.85f));
            break;

        case Icon::stop:
            p.addRoundedRectangle (Rectangle<float> (w * 0.6f, h * 0.6f).withCentre (r.getCentre()), w * 0.08f);
            break;

        case Icon::record:
            p.addEllipse (Rectangle<float> (w * 0.62f, w * 0.62f).withCentre (r.getCentre()));
            break;

        case Icon::drag:
            p.startNewSubPath (P (0.50f, 0.08f));
            p.lineTo (P (0.50f, 0.62f));
            p.startNewSubPath (P (0.28f, 0.42f));
            p.lineTo (P (0.50f, 0.64f));
            p.lineTo (P (0.72f, 0.42f));
            p.startNewSubPath (P (0.12f, 0.66f));
            p.lineTo (P (0.12f, 0.90f));
            p.lineTo (P (0.88f, 0.90f));
            p.lineTo (P (0.88f, 0.66f));
            break;

        case Icon::lane:
            p.startNewSubPath (P (0.08f, 0.62f));
            p.cubicTo (P (0.30f, 0.20f), P (0.50f, 0.20f), P (0.62f, 0.48f));
            p.cubicTo (P (0.72f, 0.70f), P (0.82f, 0.40f), P (0.92f, 0.30f));
            p.startNewSubPath (P (0.08f, 0.86f));
            p.lineTo (P (0.92f, 0.86f));
            break;

        case Icon::dock:
            p.addRoundedRectangle (Rectangle<float> (x0 + w * 0.10f, y0 + h * 0.15f, w * 0.80f, h * 0.70f), w * 0.08f);
            p.startNewSubPath (P (0.10f, 0.60f));
            p.lineTo (P (0.90f, 0.60f));
            break;

        case Icon::gear:
        {
            const auto c = r.getCentre();
            const float ro = w * 0.46f, ri = w * 0.33f;
            for (int i = 0; i < 16; ++i)
            {
                const float a0 = MathConstants<float>::twoPi * (float) i / 16.0f;
                const float rad = (i % 2 == 0) ? ro : ri;
                const auto q = c.getPointOnCircumference (rad, a0);
                const auto q2 = c.getPointOnCircumference (rad, a0 + MathConstants<float>::twoPi / 16.0f);
                if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
                p.lineTo (q2);
            }
            p.closeSubPath();
            p.addEllipse (Rectangle<float> (w * 0.28f, w * 0.28f).withCentre (c));
            break;
        }

        case Icon::none:
            break;
    }
    return p;
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = pal::metric::corner;
    const bool on = getToggleState();
    const bool enabled = isEnabled();
    const bool hot = enabled && over;

    g.setColour (enabled && down ? pal::ground1 : (hot ? pal::raised : pal::panel));
    g.fillRoundedRectangle (r, corner);
    if (on)
    {
        g.setColour (accent.withAlpha (0.14f + 0.12f * pulse));
        g.fillRoundedRectangle (r, corner);
    }
    g.setColour (on ? accent.withAlpha (0.8f) : (hot ? pal::textFaint : pal::hairline));
    g.drawRoundedRectangle (r, corner, 1.0f);

    const auto col = ! enabled ? pal::textFaint : (on ? accent.interpolatedWith (pal::text, 0.35f) : (hot ? pal::text : pal::textDim));
    auto content = r.reduced (r.getHeight() * 0.24f);

    Rectangle<float> iconArea = content, textArea;
    if (label.isNotEmpty() && icon != Icon::none)
    {
        iconArea = content.removeFromLeft (content.getHeight());
        textArea = content.withTrimmedLeft (r.getHeight() * 0.16f);
    }
    else if (label.isNotEmpty())
        textArea = content;

    if (icon != Icon::none)
    {
        const float side = jmin (iconArea.getWidth(), iconArea.getHeight());
        const auto path = makeIcon (iconArea.withSizeKeepingCentre (side, side));
        const float sw = jmax (1.25f, r.getHeight() * 0.05f);
        g.setColour (icon == Icon::record ? (enabled ? accent.withAlpha (on ? 1.0f : 0.8f) : pal::textFaint) : col);
        if (icon == Icon::play || icon == Icon::stop || icon == Icon::record)
            g.fillPath (path);
        else
            g.strokePath (path, PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
    }

    if (label.isNotEmpty())
    {
        g.setColour (col);
        g.setFont (fonts::ui (jlimit (10.0f, 14.0f, r.getHeight() * 0.44f), true));
        g.drawFittedText (label, textArea.toNearestInt(), Justification::centredLeft, 1, 0.7f);
    }
}

//==============================================================================
PillSlider::PillSlider (const juce::String& c) : caption (c) { setRepaintsOnMouseActivity (true); }

void PillSlider::setValue (float v, bool notify)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - value) < 1.0e-6f) return;
    value = v;
    repaint();
    if (notify && onChange) onChange (value);
}

void PillSlider::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool hot = isMouseOverOrDragging() && isEnabled();
    g.setColour (hot ? pal::raised : pal::panel);
    g.fillRoundedRectangle (r, pal::metric::corner);

    auto fill = r.reduced (1.0f).withWidth ((r.getWidth() - 2.0f) * value);
    g.setColour (pal::cyan.withAlpha (isEnabled() ? 0.20f : 0.07f));
    g.fillRoundedRectangle (fill, pal::metric::corner - 1.0f);
    g.setColour (pal::cyan.withAlpha (isEnabled() ? 0.9f : 0.3f));
    g.fillRect (Rectangle<float> (fill.getRight() - 1.0f, fill.getY() + 2.0f, 2.0f, fill.getHeight() - 4.0f));

    g.setColour (hot ? pal::textFaint : pal::hairline);
    g.drawRoundedRectangle (r, pal::metric::corner, 1.0f);

    const float fs = jlimit (10.0f, 14.0f, r.getHeight() * 0.45f);
    auto t = r.reduced (7.0f, 0.0f).toNearestInt();
    g.setFont (fonts::mono (fs));
    g.setColour (isEnabled() ? pal::textDim : pal::textFaint);
    if (getWidth() > 70)
        g.drawFittedText (caption, t, Justification::centredLeft, 1, 0.7f);
    g.setFont (fonts::mono (fs, true));
    g.setColour (isEnabled() ? pal::text : pal::textFaint);
    g.drawFittedText (String (roundToInt (value * 100.0f)) + "%", t, getWidth() > 70 ? Justification::centredRight : Justification::centred, 1, 0.7f);
}

void PillSlider::mouseDown (const juce::MouseEvent&)            { dragStartValue = value; }
void PillSlider::mouseDrag (const juce::MouseEvent& e)
{
    const float d = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / juce::jmax (40.0f, (float) getWidth() * 1.5f);
    setValue (dragStartValue + d, true);
}
void PillSlider::mouseDoubleClick (const juce::MouseEvent&)     { setValue (0.6f, true); }

//==============================================================================
EditToolbar::EditToolbar (BendDemonProcessor& p, LineEditController& c, VoiceField& f)
    : processor (p), controller (c), field (f)
{
    struct T { IconButton* b; LineEditController::Tool tool; const char* tip; };
    const T tools[] = {
        { &pencil,   LineEditController::Tool::pencil, "PENCIL (P) - click to place a note, drag to draw a slide. Shift = whole semitones" },
        { &lineTool, LineEditController::Tool::line,   "LINE (L) - click and drag a straight slide from A to B, click again to add a corner" },
        { &select,   LineEditController::Tool::select, "SELECT (V) - click or drag a box to select, drag to move, Alt-drag to copy, drag the ends to resize" },
        { &eraser,   LineEditController::Tool::eraser, "ERASER (E) - click or sweep over notes to delete them" } };

    for (auto& t : tools)
    {
        t.b->setTooltip (t.tip);
        const auto tool = t.tool;
        t.b->onClick = [this, tool] { controller.setTool (tool); };
        addAndMakeVisible (*t.b);
    }

    undo.setTooltip ("UNDO (Ctrl+Z)");
    redo.setTooltip ("REDO (Ctrl+Shift+Z)");
    undo.onClick = [this] { processor.getPatternStore().undo(); };
    redo.onClick = [this] { processor.getPatternStore().redo(); };

    for (int i = 0; i < numSnapChoices(); ++i)
        snapBox.addItem (snapChoices[i].name, i + 1);
    snapBox.setSelectedItemIndex (4, juce::dontSendNotification);
    snapBox.setTooltip ("SNAP - the time grid notes start and end on (hold Ctrl to draw freely)");
    snapBox.onChange = [this] { controller.setSnap (getSnapBeats()); field.setGridDivision (getSnapBeats()); };

    auto& apvts = processor.getAPVTS();
    keyBox   = std::make_unique<ChoiceBox> (apvts, ids::scaleKey, "Key");
    scaleBox = std::make_unique<ChoiceBox> (apvts, ids::scaleType, "Scale");
    scaleLock = std::make_unique<PillToggle> (apvts, ids::scaleLockOn, "Lock");
    keyBox->setCaptionFraction (0.40f);
    scaleBox->setCaptionFraction (0.30f);
    keyBox->getCombo().setTooltip ("KEY - the key your beat is in");
    scaleBox->getCombo().setTooltip ("SCALE - with LOCK on, drawn notes and shapes land on this scale");
    scaleLock->setTooltip ("LOCK - drawn notes snap to the key / scale (off = any semitone)");
    addAndMakeVisible (*keyBox);
    addAndMakeVisible (*scaleBox);
    addAndMakeVisible (*scaleLock);

    magnet.setTooltip ("MAGNET - how hard hand-drawn lines are pulled onto the notes (slides stay smooth)");
    magnet.onChange = [this] (float v) { controller.setMagnet (v); };
    magnet.setValue (controller.getMagnet(), false);

    moreMenu.setTooltip ("MORE - capture from the piano roll, copy / paste, smooth, transpose, export, clear ...");
    moreMenu.onClick = [this] { showMoreMenu(); };

    dragMidiButton.setLabel ("MIDI");
    dragMidiButton.setTooltip ("MIDI - drag into FL Studio to get the pattern as an MPE MIDI file (Shift-drag: mono pitch-bend file, click: save as)");
    dragMidiButton.onDragOut = [this] (const juce::MouseEvent& e) { dragMidi (e.mods.isShiftDown(), e); };
    dragMidiButton.onClick = [this] { if (! dragMidiButton.consumeDragged()) saveMidi (false); };

    lane.setTooltip ("LANE - show / hide the lane for loudness, brightness and velocity");
    lane.onClick = [this] { setLaneOpen (! lane.getToggleState()); if (onLaneToggled) onLaneToggled (lane.getToggleState()); };

    for (juce::Component* comp : std::initializer_list<juce::Component*> { &undo, &redo, &snapBox, &magnet, &moreMenu, &dragMidiButton, &lane })
        addAndMakeVisible (comp);

    controller.onToolChanged = [this] { tick (time); };
    tick (0.0f);
}

double EditToolbar::getSnapBeats() const    { return snapBeatsFor (snapBox.getSelectedItemIndex()); }

void EditToolbar::setSnapIndex (int i)
{
    snapBox.setSelectedItemIndex (juce::jlimit (0, numSnapChoices() - 1, i), juce::sendNotificationSync);
}

void EditToolbar::setLaneOpen (bool b)       { lane.setToggleState (b, juce::dontSendNotification); }

void EditToolbar::setEditingEnabled (bool b)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &pencil, &lineTool, &select, &eraser, &snapBox, &magnet,
                                 &undo, &redo, &dragMidiButton, &lane })
        c->setEnabled (b);
}

void EditToolbar::tick (float t)
{
    time = t;
    using Tool = LineEditController::Tool;
    const auto tool = controller.getTool();
    pencil.setToggleState (tool == Tool::pencil, juce::dontSendNotification);
    lineTool.setToggleState (tool == Tool::line, juce::dontSendNotification);
    select.setToggleState (tool == Tool::select, juce::dontSendNotification);
    eraser.setToggleState (tool == Tool::eraser, juce::dontSendNotification);

    auto& um = processor.getPatternStore().getUndoManager();
    const bool editing = pencil.isEnabled();
    undo.setEnabled (editing && um.canUndo());
    redo.setEnabled (editing && um.canRedo());
    undo.setTooltip (um.canUndo() ? "UNDO " + um.getUndoDescription() + " (Ctrl+Z)" : juce::String ("UNDO (Ctrl+Z)"));
    redo.setTooltip (um.canRedo() ? "REDO " + um.getRedoDescription() + " (Ctrl+Shift+Z)" : juce::String ("REDO (Ctrl+Shift+Z)"));

    auto& cap = processor.getCaptureRecorder();
    moreMenu.setButtonText (cap.isRecording() ? "REC" : (cap.isArmed() ? "ARMED" : "MORE"));
    moreMenu.setToggleState (cap.isArmed(), juce::dontSendNotification);

    dragMidiButton.setEnabled (editing && ! processor.getPatternStore().get().lines.empty());
}

void EditToolbar::resized()
{
    const float s = (float) getWidth() / 1040.0f;
    auto r = getLocalBounds().reduced (juce::roundToInt (10 * s), juce::roundToInt (7 * s));
    const int h = r.getHeight();
    const int gap = juce::roundToInt (4 * s), group = juce::roundToInt (18 * s);
    separators.clear();

    auto take = [&] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); };
    auto sep = [&]
    {
        r.removeFromLeft (group / 2 - gap);
        separators.push_back (r.removeFromLeft (1).withSizeKeepingCentre (1, h - juce::roundToInt (8 * s)));
        r.removeFromLeft (group / 2);
    };

    for (auto* b : { &pencil, &lineTool, &select, &eraser }) take (*b, h);
    sep();
    snapCaption = r.removeFromLeft (juce::roundToInt (40 * s));
    take (snapBox, juce::roundToInt (72 * s));
    r.removeFromLeft (juce::roundToInt (6 * s));
    take (*keyBox, juce::roundToInt (92 * s));
    take (*scaleBox, juce::roundToInt (134 * s));
    take (*scaleLock, juce::roundToInt (54 * s));
    sep();
    take (magnet, juce::roundToInt (112 * s));

    auto right = r;
    lane.setBounds (right.removeFromRight (h));
    right.removeFromRight (gap);
    dragMidiButton.setBounds (right.removeFromRight (juce::roundToInt (72 * s)));
    right.removeFromRight (group / 2);
    separators.push_back (right.removeFromRight (1).withSizeKeepingCentre (1, h - juce::roundToInt (8 * s)));
    right.removeFromRight (group / 2);
    moreMenu.setBounds (right.removeFromRight (juce::roundToInt (68 * s)));
    right.removeFromRight (gap);
    redo.setBounds (right.removeFromRight (h));
    right.removeFromRight (gap);
    undo.setBounds (right.removeFromRight (h));
}

void EditToolbar::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (pal::ground1);
    g.setColour (pal::hairline);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    for (auto& r : separators) g.fillRect (r);

    const float s = (float) getWidth() / 1040.0f;
    g.setColour (pal::textDim);
    g.setFont (fonts::ui (jmax (10.0f, 13.0f * s)));
    g.drawFittedText ("Snap", snapCaption, Justification::centredLeft, 1, 0.8f);
}

//==============================================================================
void EditToolbar::showMoreMenu()
{
    auto& store = processor.getPatternStore();
    auto& um = store.getUndoManager();
    auto& cap = processor.getCaptureRecorder();
    const bool sel = controller.hasSelection();
    const bool any = ! store.get().lines.empty();
    const bool draw = processor.isDrawMode();
    const int bars = store.get().bars;

    juce::PopupMenu captureMenu;
    if (cap.isArmed()) captureMenu.addItem (50, "Cancel recording");
    else               captureMenu.addItem (51, "Record the next " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + " you play");
    captureMenu.addSeparator();
    for (int b : { 1, 2, 4, 8, 16 })
        captureMenu.addItem (60 + b, "Grab the last " + juce::String (b) + (b == 1 ? " bar" : " bars"), cap.numEvents() > 0);

    juce::PopupMenu m;
    m.addSectionHeader ("Capture");
    m.addSubMenu ("Capture from the piano roll", captureMenu);
    m.addSectionHeader ("Edit");
    m.addItem (1, um.canUndo() ? "Undo " + um.getUndoDescription() : juce::String ("Undo"), draw && um.canUndo());
    m.addItem (2, um.canRedo() ? "Redo " + um.getRedoDescription() : juce::String ("Redo"), draw && um.canRedo());
    m.addItem (3, "Cut", draw && sel);
    m.addItem (4, "Copy", draw && sel);
    m.addItem (5, "Paste", draw && controller.canPaste());
    m.addItem (6, "Duplicate", draw && sel);
    m.addItem (7, "Delete", draw && sel);
    m.addItem (8, "Select all", draw && any);
    m.addSeparator();
    m.addItem (10, "Smooth out hand-drawn curves", draw && sel);
    m.addItem (11, "Toggle smooth / straight", draw && sel);
    m.addItem (12, "Quantize note starts", draw && sel);
    m.addItem (13, "Octave up", draw && sel);
    m.addItem (14, "Octave down", draw && sel);
    m.addSectionHeader ("Pattern");
    m.addItem (22, "Open pattern or MIDI file...", draw);
    m.addItem (23, "Save pattern...", any);
    m.addItem (20, "Save as MIDI file (MPE)...", any);
    m.addItem (21, "Save as MIDI file (mono pitch bend)...", any);
    m.addItem (31, "Fit view", draw);
    m.addItem (40, "Clear pattern", draw && any);

    juce::Component::SafePointer<EditToolbar> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreMenu), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& c = safe->controller;
        auto& proc = safe->processor;
        switch (r)
        {
            case 1: proc.getPatternStore().undo(); break;
            case 2: proc.getPatternStore().redo(); break;
            case 3: c.cut(); break;
            case 4: c.copy(); break;
            case 5: c.paste(); break;
            case 6: c.duplicate(); break;
            case 7: c.deleteSelection(); break;
            case 8: c.selectAll(); break;
            case 10: c.smoothSelection(); break;
            case 11: c.toggleCurveType(); break;
            case 12: c.quantizeSelection(); break;
            case 13: c.transposeSelection (12.0f); break;
            case 14: c.transposeSelection (-12.0f); break;
            case 20: safe->saveMidi (false); break;
            case 22: safe->openFile(); break;
            case 23: safe->savePattern(); break;
            case 21: safe->saveMidi (true); break;
            case 31: safe->field.fitToPattern(); break;
            case 40: c.clearPattern(); break;
            case 50: proc.getCaptureRecorder().disarm(); break;
            case 51:
                proc.armCapture();
                if (safe->onMessage) safe->onMessage ("Recording armed - press play in FL Studio, the next pass becomes notes you can edit");
                break;
            default:
                if (r > 60)
                {
                    const int n = proc.captureLastBars (r - 60);
                    if (safe->onMessage)
                        safe->onMessage (n > 0 ? "Captured " + juce::String (n) + " notes. Mute the piano-roll notes so they don't play twice."
                                               : juce::String ("Nothing to capture yet - play some notes first"));
                }
                break;
        }
    });
}

juce::File EditToolbar::writeMidiFile (bool mono) const
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("TrapWin BEND DEMON");
    dir.createDirectory();
    const auto name = juce::File::createLegalFileName ("BEND DEMON " + processor.getPresets().getCurrentName()
                                                       + (mono ? " MONO" : " MPE") + ".mid");
    auto f = dir.getChildFile (name);
    f.deleteFile();

    auto midi = tools::exportMidi (processor.getPatternStore().get(), mono, processor.getBendRange(), processor.getTempo());
    juce::FileOutputStream os (f);
    if (os.openedOk())
        midi.writeTo (os, 1);
    os.flush();
    return f;
}

void EditToolbar::dragMidi (bool mono, const juce::MouseEvent&)
{
    if (processor.getPatternStore().get().lines.empty()) return;
    const auto f = writeMidiFile (mono);
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, &dragMidiButton);
}

void EditToolbar::saveMidi (bool mono)
{
    if (processor.getPatternStore().get().lines.empty()) return;
    const auto tmp = writeMidiFile (mono);

    chooser() = std::make_unique<juce::FileChooser> ("Save MIDI file",
                                                     juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (tmp.getFileName()),
                                                     "*.mid");
    chooser()->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                            [tmp] (const juce::FileChooser& fc)
                            {
                                const auto target = fc.getResult();
                                if (target != juce::File())
                                    tmp.copyFileTo (target.withFileExtension ("mid"));
                            });
}

void EditToolbar::savePattern()
{
    chooser() = std::make_unique<juce::FileChooser> ("Save pattern",
                                                     juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                         .getChildFile (juce::File::createLegalFileName (processor.getPresets().getCurrentName()) + ".bdpattern"),
                                                     "*.bdpattern");
    juce::Component::SafePointer<EditToolbar> safe (this);
    chooser()->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                            [safe] (const juce::FileChooser& fc)
                            {
                                const auto f = fc.getResult();
                                if (safe == nullptr || f == juce::File()) return;
                                if (auto xml = safe->processor.getPatternStore().createXml())
                                    xml->writeTo (f.withFileExtension ("bdpattern"));
                            });
}

void EditToolbar::openFile()
{
    chooser() = std::make_unique<juce::FileChooser> ("Open pattern or MIDI file",
                                                     juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                     "*.bdpattern;*.mid;*.midi");
    juce::Component::SafePointer<EditToolbar> safe (this);
    chooser()->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                            [safe] (const juce::FileChooser& fc)
                            {
                                const auto f = fc.getResult();
                                if (safe == nullptr || ! f.existsAsFile()) return;
                                auto& proc = safe->processor;
                                Pattern p;
                                bool ok = false;

                                if (f.hasFileExtension ("mid;midi"))
                                {
                                    juce::FileInputStream in (f);
                                    juce::MidiFile mf;
                                    ok = in.openedOk() && mf.readFrom (in) && tools::importMidi (mf, proc.getBendRange(), p);
                                }
                                else if (auto xml = juce::XmlDocument::parse (f); xml != nullptr && xml->hasTagName (Pattern::xmlTag))
                                {
                                    p = Pattern::fromXml (*xml);
                                    ok = true;
                                }

                                if (ok)
                                    proc.getPatternStore().perform (std::move (p), "Open " + f.getFileName(), true);
                                if (safe->onMessage)
                                    safe->onMessage (ok ? "Opened " + f.getFileName() : "Could not read " + f.getFileName());
                            });
}

} // namespace bd
