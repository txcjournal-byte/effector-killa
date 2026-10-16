/*
    TrapWin BEND DEMON
    VoiceField.h - the live "voice field" visualisation.

    Each MPE voice is drawn as a translucent ribbon: thickness = pressure, colour = timbre / time,
    a bright core line = the actual pitch including every bend. Glowing nodes mark note starts and
    glide/bend points, sparks travel along live voices, onsets bloom, and a light-beam playhead
    sweeps across the last N bars in sync with the host. DIMENSION tilts the whole field into a
    projective pseudo-3D space with one depth layer per voice.

    Data arrives from the audio thread through bd::VisualFifo (lock-free); this component only
    ever reads it on the message thread inside tick().
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../VisualFifo.h"
#include <vector>

namespace bd
{

class VoiceField final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    explicit VoiceField (VisualFifo&);

    /** Pull new events, advance animations, repaint. Called by the editor's frame timer. */
    void tick (float timeSeconds);

    void setDimension (bool on)          { dimensionTarget = on ? 1.0f : 0.0f; }
    bool getDimension() const noexcept   { return dimensionTarget > 0.5f; }
    void setBars (int bars)              { numBars = juce::jlimit (1, 16, bars); }
    int  getBars() const noexcept        { return numBars; }
    void setHintText (juce::String s)    { hint = std::move (s); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    // All positions are stored in BEATS (quarter notes), converted once when the event arrives,
    // so tempo changes or transport jumps never re-shape what has already been drawn.
    struct Pt    { double beat; float pitch, pressure, timbre; };
    struct Node  { double beat; float pitch; int kind; };          // 0 = onset, 1 = glide, 2 = key up
    struct Stroke
    {
        int channel = 0;
        int note = 0;
        int velocity = 0;
        double startBeat = 0.0, endBeat = -1.0;   // endBeat < 0 while sounding
        float createdAt = 0.0f;                   // frame clock seconds, for the onset bloom
        float seed = 0.0f;
        std::vector<Pt> pts;
        std::vector<Node> nodes;
    };

    struct View
    {
        double nowClock = 0.0, sampleRate = 44100.0, bpm = 120.0, beatNow = 0.0;
        double viewStart = 0.0, viewBeats = 16.0;   // in beats
        double playheadBeat = 0.0;
        bool   pageMode = false;
        float  pitchLo = 36.0f, pitchHi = 84.0f;
        int    beatsPerBar = 4;
    };

    void consume (const VisualEvent&);
    void prune();
    double beatOf (int64_t t) const noexcept;
    void   updateView();

    /** Field coordinates -> screen. u: 0..1 left->right, v: 0..1 bottom->top, z: 0..1 depth. */
    juce::Point<float> project (float u, float v, float z) const noexcept;
    juce::Point<float> projectRaw (float u, float v, float z) const noexcept;   // unit space, before fitting
    void updateProjectionFit();
    float depthScale (float v, float z) const noexcept;
    float uOf (double beat, float& alpha) const noexcept;
    float vOf (float pitch) const noexcept { return (pitch - view.pitchLo) / juce::jmax (1.0f, view.pitchHi - view.pitchLo); }
    static float zOf (int channel) noexcept { return (float) (channel - 2) / 14.0f; }

    void drawBackground (juce::Graphics&);
    void drawGrid (juce::Graphics&);
    void drawStroke (juce::Graphics&, const Stroke&);
    void drawPlayhead (juce::Graphics&);
    void renderBackgroundImage();

    VisualFifo& fifo;
    std::vector<Stroke> strokes;
    int activeStroke[17];      // channel -> index in strokes, -1 = none

    View view;
    juce::Rectangle<float> field;
    juce::Rectangle<float> rawBounds { -0.5f, -0.5f, 1.0f, 1.0f };   // bounds of the projected unit cube
    juce::Image bgImage;

    float time = 0.0f;
    float dimension = 0.0f, dimensionTarget = 0.0f;
    float pitchLoSmoothed = 36.0f, pitchHiSmoothed = 84.0f;
    int   numBars = 4;
    juce::String hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceField)
};

} // namespace bd
