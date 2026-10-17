#include "Parameters.h"
#include "engine/ScaleLock.h"
#include "engine/InternalSynth.h"

namespace bd
{

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using API = juce::AudioParameterInt;
    using NR  = juce::NormalisableRange<float>;

    juce::StringArray syncNames()
    {
        juce::StringArray s;
        for (auto* n : syncDivisionNames) s.add (n);
        return s;
    }

    juce::StringArray fromNullTerminated (const char* const* names)
    {
        juce::StringArray s;
        for (int i = 0; names[i] != nullptr; ++i) s.add (names[i]);
        return s;
    }

    NR skewed (float lo, float hi, float centre)
    {
        NR r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::AudioParameterFloatAttributes msAttr()
    {
        return juce::AudioParameterFloatAttributes()
                 .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })
                 .withLabel ("ms");
    }

    juce::AudioParameterFloatAttributes pctAttr()
    {
        return juce::AudioParameterFloatAttributes()
                 .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    }

    juce::ParameterID pid (const char* id) { return { id, 1 }; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto curves = juce::StringArray { "Linear", "Ease", "S-Curve" };
    const auto shapes = juce::StringArray { "Attack", "Swell", "Decay" };

    // --- gesture pads
    layout.add (std::make_unique<APB> (pid (ids::glideOn),      "Glide",       true));
    layout.add (std::make_unique<APB> (pid (ids::chordGlideOn), "Chord Glide", false));
    layout.add (std::make_unique<APB> (pid (ids::vibratoOn),    "Vibrato",     false));
    layout.add (std::make_unique<APB> (pid (ids::diveOn),       "Dive",        false));
    layout.add (std::make_unique<APB> (pid (ids::riseOn),       "Rise",        false));
    layout.add (std::make_unique<APB> (pid (ids::wobbleOn),     "Wobble",      false));
    layout.add (std::make_unique<APB> (pid (ids::shakeOn),      "Shake",       false));
    layout.add (std::make_unique<APB> (pid (ids::trillOn),      "Trill",       false));

    // --- glide
    layout.add (std::make_unique<APF> (pid (ids::glideTime),   "Glide Time",   skewed (5.0f, 2000.0f, 150.0f), 120.0f, msAttr()));
    layout.add (std::make_unique<APF> (pid (ids::glideWindow), "Glide Window", NR (0.0f, 250.0f, 1.0f), 60.0f, msAttr()));
    layout.add (std::make_unique<APC> (pid (ids::glideCurve),  "Glide Curve",  curves, 1));
    layout.add (std::make_unique<APF> (pid (ids::chordWindow), "Chord Window", NR (2.0f, 40.0f, 1.0f), 12.0f, msAttr()));

    // --- vibrato
    layout.add (std::make_unique<APF> (pid (ids::vibRate),  "Vibrato Rate",  skewed (0.5f, 12.0f, 5.0f), 5.5f,
                                       juce::AudioParameterFloatAttributes().withLabel ("Hz")
                                           .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })));
    layout.add (std::make_unique<APB> (pid (ids::vibSync),  "Vibrato Sync",  false));
    layout.add (std::make_unique<APC> (pid (ids::vibDiv),   "Vibrato Division", syncNames(), 9));
    layout.add (std::make_unique<APF> (pid (ids::vibDepth), "Vibrato Depth", NR (0.0f, 200.0f, 1.0f), 35.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("ct")
                                           .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ct"; })));
    layout.add (std::make_unique<APF> (pid (ids::vibDelay), "Vibrato Delay", skewed (0.0f, 2000.0f, 400.0f), 350.0f, msAttr()));

    // --- dive / rise
    layout.add (std::make_unique<APF> (pid (ids::diveAmount), "Dive Amount", NR (0.0f, 48.0f, 0.01f), 12.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("st")
                                           .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " st"; })));
    layout.add (std::make_unique<APF> (pid (ids::diveTime),   "Dive Time",   skewed (20.0f, 4000.0f, 400.0f), 400.0f, msAttr()));
    layout.add (std::make_unique<APB> (pid (ids::diveSync),   "Dive Sync",   false));
    layout.add (std::make_unique<APC> (pid (ids::diveDiv),    "Dive Division", syncNames(), 6));
    layout.add (std::make_unique<APC> (pid (ids::diveCurve),  "Dive Curve",  curves, 1));

    // --- wobble / shake / trill
    layout.add (std::make_unique<APF> (pid (ids::wobbleDepth), "Wobble Depth", NR (0.0f, 12.0f, 0.01f), 1.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("st")));
    layout.add (std::make_unique<APC> (pid (ids::wobbleDiv),   "Wobble Division", syncNames(), 3));
    layout.add (std::make_unique<APF> (pid (ids::shakeDepth),  "Shake Depth", NR (0.0f, 100.0f, 0.1f), 18.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("ct")));
    layout.add (std::make_unique<APF> (pid (ids::shakeRate),   "Shake Rate",  skewed (2.0f, 40.0f, 12.0f), 14.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<APC> (pid (ids::trillDiv),    "Trill Division", syncNames(), 9));

    // --- scale
    layout.add (std::make_unique<APB> (pid (ids::scaleLockOn),   "Scale Lock", false));
    layout.add (std::make_unique<APC> (pid (ids::scaleKey),      "Scale Key",  fromNullTerminated (ScaleLock::getKeyNames()), 0));
    layout.add (std::make_unique<APC> (pid (ids::scaleType),     "Scale Type", fromNullTerminated (ScaleLock::getScaleNames()), 1));
    layout.add (std::make_unique<APB> (pid (ids::quantizeInput), "Quantize Input", false));

    // --- pressure / timbre
    layout.add (std::make_unique<APB> (pid (ids::pressureOn),    "Pressure On",    true));
    layout.add (std::make_unique<APF> (pid (ids::pressure),      "Pressure",       NR (0.0f, 1.0f), 0.8f, pctAttr()));
    layout.add (std::make_unique<APF> (pid (ids::pressureTime),  "Pressure Time",  skewed (1.0f, 3000.0f, 300.0f), 250.0f, msAttr()));
    layout.add (std::make_unique<APC> (pid (ids::pressureShape), "Pressure Shape", shapes, 0));
    layout.add (std::make_unique<APF> (pid (ids::velToPressure), "Vel > Pressure", NR (0.0f, 1.0f), 0.5f, pctAttr()));
    layout.add (std::make_unique<APB> (pid (ids::timbreOn),      "Timbre On",      true));
    layout.add (std::make_unique<APF> (pid (ids::timbre),        "Timbre",         NR (0.0f, 1.0f), 0.5f, pctAttr()));
    layout.add (std::make_unique<APF> (pid (ids::timbreTime),    "Timbre Time",    skewed (1.0f, 3000.0f, 300.0f), 600.0f, msAttr()));
    layout.add (std::make_unique<APC> (pid (ids::timbreShape),   "Timbre Shape",   shapes, 1));
    layout.add (std::make_unique<APF> (pid (ids::velToTimbre),   "Vel > Timbre",   NR (0.0f, 1.0f), 0.3f, pctAttr()));

    // --- MPE / routing
    juce::StringArray ranges;
    for (auto r : bendRanges) ranges.add ("+/-" + juce::String (r));
    layout.add (std::make_unique<APC> (pid (ids::bendRange), "Bend Range", ranges, 3));
    layout.add (std::make_unique<API> (pid (ids::voices),    "Voices", 1, 15, 15));
    layout.add (std::make_unique<APB> (pid (ids::midiOutMode),    "MIDI Out Mode", false));
    layout.add (std::make_unique<APB> (pid (ids::bypassGestures), "Bypass Gestures", false));
    layout.add (std::make_unique<APF> (pid (ids::outputGain), "Output Gain", NR (-24.0f, 12.0f, 0.1f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel ("dB")
                                           .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));

    // --- 2.0: line sequencer + internal synth
    layout.add (std::make_unique<APC> (pid (ids::mode), "Mode", juce::StringArray { "Live", "Draw" }, 1));
    layout.add (std::make_unique<APB> (pid (ids::seqMono), "Mono Mode", false));
    {
        juce::StringArray sounds;
        for (int i = 0; i < InternalSynth::numSounds; ++i) sounds.add (InternalSynth::getSoundName (i));
        layout.add (std::make_unique<APC> (pid (ids::internalSound), "Internal Sound", sounds, 0));
    }

    return layout;
}

//==============================================================================
ParameterSnapshot::ParameterSnapshot (juce::AudioProcessorValueTreeState& s)
{
    auto get = [&s] (const char* id)
    {
        auto* r = s.getRawParameterValue (id);
        jassert (r != nullptr);
        return r;
    };

    p.glideOn = get (ids::glideOn);       p.chordGlideOn = get (ids::chordGlideOn);
    p.vibratoOn = get (ids::vibratoOn);   p.diveOn = get (ids::diveOn);   p.riseOn = get (ids::riseOn);
    p.wobbleOn = get (ids::wobbleOn);     p.shakeOn = get (ids::shakeOn); p.trillOn = get (ids::trillOn);
    p.glideTime = get (ids::glideTime);   p.glideWindow = get (ids::glideWindow);
    p.glideCurve = get (ids::glideCurve); p.chordWindow = get (ids::chordWindow);
    p.vibRate = get (ids::vibRate);       p.vibSync = get (ids::vibSync); p.vibDiv = get (ids::vibDiv);
    p.vibDepth = get (ids::vibDepth);     p.vibDelay = get (ids::vibDelay);
    p.diveAmount = get (ids::diveAmount); p.diveTime = get (ids::diveTime); p.diveSync = get (ids::diveSync);
    p.diveDiv = get (ids::diveDiv);       p.diveCurve = get (ids::diveCurve);
    p.wobbleDepth = get (ids::wobbleDepth); p.wobbleDiv = get (ids::wobbleDiv);
    p.shakeDepth = get (ids::shakeDepth); p.shakeRate = get (ids::shakeRate); p.trillDiv = get (ids::trillDiv);
    p.scaleLockOn = get (ids::scaleLockOn); p.scaleKey = get (ids::scaleKey);
    p.scaleType = get (ids::scaleType);   p.quantizeInput = get (ids::quantizeInput);
    p.pressureOn = get (ids::pressureOn); p.pressure = get (ids::pressure); p.pressureTime = get (ids::pressureTime);
    p.pressureShape = get (ids::pressureShape); p.velToPressure = get (ids::velToPressure);
    p.timbreOn = get (ids::timbreOn);     p.timbre = get (ids::timbre);   p.timbreTime = get (ids::timbreTime);
    p.timbreShape = get (ids::timbreShape); p.velToTimbre = get (ids::velToTimbre);
    p.bendRange = get (ids::bendRange);   p.voices = get (ids::voices);
    p.midiOut = get (ids::midiOutMode);   p.bypass = get (ids::bypassGestures); p.gain = get (ids::outputGain);
    p.mode = get (ids::mode);             p.seqMono = get (ids::seqMono);       p.sound = get (ids::internalSound);
}

EngineSettings ParameterSnapshot::read() const noexcept
{
    auto b = [] (std::atomic<float>* a) { return a->load() > 0.5f; };
    auto f = [] (std::atomic<float>* a) { return a->load(); };
    auto i = [] (std::atomic<float>* a) { return (int) std::lround (a->load()); };

    EngineSettings s;
    const bool bypass = b (p.bypass);

    s.numVoices      = i (p.voices);
    s.bendRange      = bendRanges[(size_t) juce::jlimit (0, numBendRanges - 1, i (p.bendRange))];

    s.glideOn        = ! bypass && b (p.glideOn);
    s.chordGlideOn   = ! bypass && b (p.chordGlideOn);
    s.glideTimeMs    = f (p.glideTime);
    s.glideWindowMs  = f (p.glideWindow);
    s.glideCurve     = (Curve) i (p.glideCurve);
    s.chordWindowMs  = f (p.chordWindow);

    s.vibratoOn      = ! bypass && b (p.vibratoOn);
    s.vibRateHz      = f (p.vibRate);
    s.vibSync        = b (p.vibSync);
    s.vibDiv         = i (p.vibDiv);
    s.vibDepthCents  = f (p.vibDepth);
    s.vibDelayMs     = f (p.vibDelay);

    s.diveOn         = ! bypass && b (p.diveOn);
    s.riseOn         = ! bypass && b (p.riseOn);
    s.diveSemis      = f (p.diveAmount);
    s.diveTimeMs     = f (p.diveTime);
    s.diveSync       = b (p.diveSync);
    s.diveDiv        = i (p.diveDiv);
    s.diveCurve      = (Curve) i (p.diveCurve);

    s.wobbleOn       = ! bypass && b (p.wobbleOn);
    s.wobbleSemis    = f (p.wobbleDepth);
    s.wobbleDiv      = i (p.wobbleDiv);
    s.shakeOn        = ! bypass && b (p.shakeOn);
    s.shakeCents     = f (p.shakeDepth);
    s.shakeRateHz    = f (p.shakeRate);
    s.trillOn        = ! bypass && b (p.trillOn);
    s.trillDiv       = i (p.trillDiv);

    s.scaleLockOn    = b (p.scaleLockOn);
    s.scaleKey       = i (p.scaleKey);
    s.scaleType      = i (p.scaleType);
    s.quantizeInput  = b (p.quantizeInput);

    s.pressureOn     = ! bypass && b (p.pressureOn);
    s.pressureAmount = f (p.pressure);
    s.pressureTimeMs = f (p.pressureTime);
    s.pressureShape  = (EnvShape) i (p.pressureShape);
    s.velToPressure  = f (p.velToPressure);
    s.timbreOn       = ! bypass && b (p.timbreOn);
    s.timbreAmount   = f (p.timbre);
    s.timbreTimeMs   = f (p.timbreTime);
    s.timbreShape    = (EnvShape) i (p.timbreShape);
    s.velToTimbre    = f (p.velToTimbre);
    return s;
}

} // namespace bd
