/*
    TrapWin BEND DEMON
    Parameters.h - parameter IDs, the APVTS layout and the snapshot into bd::EngineSettings.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/EngineTypes.h"

namespace bd::ids
{
    // gesture toggles
    inline constexpr auto glideOn       = "glideOn";
    inline constexpr auto chordGlideOn  = "chordGlideOn";
    inline constexpr auto vibratoOn     = "vibratoOn";
    inline constexpr auto diveOn        = "diveOn";
    inline constexpr auto riseOn        = "riseOn";
    inline constexpr auto wobbleOn      = "wobbleOn";
    inline constexpr auto shakeOn       = "shakeOn";
    inline constexpr auto trillOn       = "trillOn";

    // glide
    inline constexpr auto glideTime     = "glideTime";
    inline constexpr auto glideWindow   = "glideWindow";
    inline constexpr auto glideCurve    = "glideCurve";
    inline constexpr auto chordWindow   = "chordWindow";

    // vibrato
    inline constexpr auto vibRate       = "vibRate";
    inline constexpr auto vibSync       = "vibSync";
    inline constexpr auto vibDiv        = "vibDiv";
    inline constexpr auto vibDepth      = "vibDepth";
    inline constexpr auto vibDelay      = "vibDelay";

    // dive / rise
    inline constexpr auto diveAmount    = "diveAmount";
    inline constexpr auto diveTime      = "diveTime";
    inline constexpr auto diveSync      = "diveSync";
    inline constexpr auto diveDiv       = "diveDiv";
    inline constexpr auto diveCurve     = "diveCurve";

    // wobble / shake / trill
    inline constexpr auto wobbleDepth   = "wobbleDepth";
    inline constexpr auto wobbleDiv     = "wobbleDiv";
    inline constexpr auto shakeDepth    = "shakeDepth";
    inline constexpr auto shakeRate     = "shakeRate";
    inline constexpr auto trillDiv      = "trillDiv";

    // scale
    inline constexpr auto scaleLockOn   = "scaleLockOn";
    inline constexpr auto scaleKey      = "scaleKey";
    inline constexpr auto scaleType     = "scaleType";
    inline constexpr auto quantizeInput = "quantizeInput";

    // pressure / timbre
    inline constexpr auto pressureOn    = "pressureOn";
    inline constexpr auto pressure      = "pressure";
    inline constexpr auto pressureTime  = "pressureTime";
    inline constexpr auto pressureShape = "pressureShape";
    inline constexpr auto velToPressure = "velToPressure";
    inline constexpr auto timbreOn      = "timbreOn";
    inline constexpr auto timbre        = "timbre";
    inline constexpr auto timbreTime    = "timbreTime";
    inline constexpr auto timbreShape   = "timbreShape";
    inline constexpr auto velToTimbre   = "velToTimbre";

    // MPE / routing
    inline constexpr auto bendRange     = "bendRange";
    inline constexpr auto voices        = "voices";
    inline constexpr auto midiOutMode   = "midiOutMode";
    inline constexpr auto bypassGestures= "bypassGestures";
    inline constexpr auto outputGain    = "outputGain";

    // 2.0 line sequencer + internal synth
    inline constexpr auto mode          = "mode";            // 0 = LIVE (1.x), 1 = DRAW
    inline constexpr auto seqMono       = "seqMono";         // Mono/Legacy output for non-MPE synths
    inline constexpr auto internalSound = "internalSound";
}

namespace bd
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Caches raw parameter pointers so the audio thread can snapshot them without lookups. */
class ParameterSnapshot
{
public:
    explicit ParameterSnapshot (juce::AudioProcessorValueTreeState&);

    EngineSettings read() const noexcept;
    bool  midiOutMode() const noexcept    { return p.midiOut->load() > 0.5f; }
    float outputGainDb() const noexcept   { return p.gain->load(); }
    bool  drawMode() const noexcept       { return p.mode->load() > 0.5f; }
    bool  monoMode() const noexcept       { return p.seqMono->load() > 0.5f; }
    int   internalSound() const noexcept  { return (int) std::lround (p.sound->load()); }

private:
    struct Ptrs
    {
        std::atomic<float>* glideOn{}, *chordGlideOn{}, *vibratoOn{}, *diveOn{}, *riseOn{}, *wobbleOn{}, *shakeOn{}, *trillOn{};
        std::atomic<float>* glideTime{}, *glideWindow{}, *glideCurve{}, *chordWindow{};
        std::atomic<float>* vibRate{}, *vibSync{}, *vibDiv{}, *vibDepth{}, *vibDelay{};
        std::atomic<float>* diveAmount{}, *diveTime{}, *diveSync{}, *diveDiv{}, *diveCurve{};
        std::atomic<float>* wobbleDepth{}, *wobbleDiv{}, *shakeDepth{}, *shakeRate{}, *trillDiv{};
        std::atomic<float>* scaleLockOn{}, *scaleKey{}, *scaleType{}, *quantizeInput{};
        std::atomic<float>* pressureOn{}, *pressure{}, *pressureTime{}, *pressureShape{}, *velToPressure{};
        std::atomic<float>* timbreOn{}, *timbre{}, *timbreTime{}, *timbreShape{}, *velToTimbre{};
        std::atomic<float>* bendRange{}, *voices{}, *midiOut{}, *bypass{}, *gain{};
        std::atomic<float>* mode{}, *seqMono{}, *sound{};
    } p;
};

} // namespace bd
