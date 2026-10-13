#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

SweepProcessor::SweepProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SWEEP1", createLayout())
{
    auto bind = [this] (Raw& r, const juce::String& id) { r.p = apvts.getRawParameterValue (id); jassert (r.p != nullptr); };
    bind (aType, IDs::aType); bind (aCutoff, IDs::aCutoff); bind (aReso, IDs::aReso); bind (aDrive, IDs::aDrive); bind (aMod, IDs::aMod);
    bind (bOn, IDs::bOn); bind (bType, IDs::bType); bind (bCutoff, IDs::bCutoff); bind (bReso, IDs::bReso); bind (bDrive, IDs::bDrive); bind (bMod, IDs::bMod);
    bind (routing, IDs::routing); bind (sync, IDs::sync); bind (division, IDs::division); bind (rate, IDs::rate); bind (shape, IDs::shape);
    bind (lfoDepth, IDs::lfoDepth); bind (stereo, IDs::stereo); bind (glide, IDs::glide); bind (retrig, IDs::retrig);
    bind (envDepth, IDs::envDepth); bind (envSpeed, IDs::envSpeed); bind (trigger, IDs::trigger); bind (sens, IDs::sens);
    bind (attack, IDs::attack); bind (decay, IDs::decay); bind (sustain, IDs::sustain); bind (release, IDs::release); bind (adsrDepth, IDs::adsrDepth);
    bind (mix, IDs::mix); bind (output, IDs::output);
    for (int i = 0; i < sweepfx::numSteps; ++i) bind (steps[(size_t) i], IDs::step (i));
}

juce::AudioProcessorValueTreeState::ParameterLayout SweepProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto spct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const int p = roundToInt (v * 100.0f);
        return (p > 0 ? "+" : "") + String (p) + " %";
    });
    auto hzAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto msAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : (v < 10.0f ? String (v, 1) : String (roundToInt (v))) + " ms"; });
    auto cutRange = [] { return NormalisableRange<float> (sweepfx::minCutoff, sweepfx::maxCutoff, 0.1f, 0.25f); };

    StringArray types, shapes, divs;
    for (auto* t : sweepfx::filterTypeNames) types.add (t);
    for (auto* s : sweepfx::shapeNames) shapes.add (s);
    for (auto* d : sweepfx::divisionNames) divs.add (d);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::aType, 1 }, "A Type", types, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::aCutoff, 1 }, "A Cutoff", cutRange(), 800.0f, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::aReso, 1 }, "A Resonance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::aDrive, 1 }, "A Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::aMod, 1 }, "A Mod Amount", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 1.0f, spct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::bOn, 1 }, "B On", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::bType, 1 }, "B Type", types, 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::bCutoff, 1 }, "B Cutoff", cutRange(), 200.0f, hzAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::bReso, 1 }, "B Resonance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::bDrive, 1 }, "B Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::bMod, 1 }, "B Mod Amount", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 1.0f, spct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::routing, 1 }, "Routing", StringArray { "Serial", "Parallel" }, 0));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::sync, 1 }, "LFO Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::division, 1 }, "LFO Rate (sync)", divs, sweepfx::defaultDivision));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::rate, 1 }, "LFO Rate (free)", NormalisableRange<float> (0.05f, 20.0f, 0.001f, 0.35f), 2.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::shape, 1 }, "LFO Shape", shapes, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::lfoDepth, 1 }, "LFO Depth", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.5f, spct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::stereo, 1 }, "LFO Stereo", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 180.0f)) + " deg"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::glide, 1 }, "Step Glide", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::retrig, 1 }, "LFO Retrigger", false));
    for (int i = 0; i < sweepfx::numSteps; ++i)
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::step (i), 1 }, "Step " + String (i + 1), NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                      sweepfx::Params().pattern[(size_t) i], pct));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::envDepth, 1 }, "Env Depth", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, spct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::envSpeed, 1 }, "Env Speed", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::trigger, 1 }, "ADSR Trigger", StringArray { "MIDI", "Transient" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::sens, 1 }, "Transient Sensitivity", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::attack, 1 }, "ADSR Attack", NormalisableRange<float> (0.1f, 5000.0f, 0.1f, 0.25f), 5.0f, msAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::decay, 1 }, "ADSR Decay", NormalisableRange<float> (1.0f, 5000.0f, 0.1f, 0.3f), 250.0f, msAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::sustain, 1 }, "ADSR Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::release, 1 }, "ADSR Release", NormalisableRange<float> (1.0f, 5000.0f, 0.1f, 0.3f), 200.0f, msAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::adsrDepth, 1 }, "ADSR Depth", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, spct));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

bool SweepProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SweepProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (0);
}

sweepfx::Params SweepProcessor::currentParams() const
{
    sweepfx::Params p;
    auto ftype = [] (float v) { return (sweepfx::FilterType) juce::jlimit (0, sweepfx::numFilterTypes - 1, (int) v); };
    p.a.on = true;
    p.a.type = ftype (aType.get());
    p.a.cutoff = aCutoff.get();
    p.a.reso = aReso.get();
    p.a.drive = aDrive.get();
    p.a.mod = aMod.get();
    p.b.on = bOn.get() > 0.5f;
    p.b.type = ftype (bType.get());
    p.b.cutoff = bCutoff.get();
    p.b.reso = bReso.get();
    p.b.drive = bDrive.get();
    p.b.mod = bMod.get();
    p.routing = routing.get() > 0.5f ? sweepfx::Routing::parallel : sweepfx::Routing::serial;
    p.sync = sync.get() > 0.5f;
    p.division = juce::jlimit (0, sweepfx::numDivisions - 1, (int) division.get());
    p.rateHz = rate.get();
    p.shape = (sweepfx::Shape) juce::jlimit (0, sweepfx::numShapes - 1, (int) shape.get());
    p.lfoDepth = lfoDepth.get();
    p.stereo = stereo.get();
    p.glide = glide.get();
    p.retrig = retrig.get() > 0.5f;
    for (int i = 0; i < sweepfx::numSteps; ++i) p.pattern[(size_t) i] = steps[(size_t) i].get();
    p.envDepth = envDepth.get();
    p.envSpeed = envSpeed.get();
    p.trigger = trigger.get() > 0.5f ? sweepfx::Trigger::transient : sweepfx::Trigger::midi;
    p.sensitivity = sens.get();
    p.attackMs = attack.get();
    p.decayMs = decay.get();
    p.sustain = sustain.get();
    p.releaseMs = release.get();
    p.adsrDepth = adsrDepth.get();
    p.mix = mix.get();
    p.outputDb = output.get();
    return p;
}

void SweepProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) { midi.clear(); return; }

    sweepfx::HostTime t;
    t.bpm = uiBpm.load();
    bool hostTempo = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.hasPpq = true; }
            if (auto bpm = pos->getBpm()) if (*bpm > 1.0) { t.bpm = *bpm; hostTempo = true; }
        }
    uiBpm = t.bpm;
    uiHostTempo = hostTempo;

    engine.setParams (currentParams());

    // split the block at MIDI events so ADSR triggers are sample accurate
    auto* const* chans = buffer.getArrayOfWritePointers();
    float* seg[2] = { nullptr, nullptr };
    int pos = 0;
    auto runTo = [&] (int end)
    {
        if (end <= pos) return;
        for (int c = 0; c < numCh; ++c) seg[c] = chans[c] + pos;
        sweepfx::HostTime st = t;
        st.ppq = t.ppq + (double) pos / lastSampleRate * t.bpm / 60.0;
        engine.process (seg, numCh, end - pos, st);
        pos = end;
    };
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        runTo (juce::jlimit (0, n, meta.samplePosition));
        if (msg.isNoteOn()) engine.noteOn();
        else if (msg.isNoteOff()) engine.noteOff();
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }
    runTo (n);
    midi.clear();

    float lvl = 0.0f;
    for (int c = 0; c < numCh; ++c)
        lvl = std::max (lvl, buffer.getMagnitude (c, 0, n));
    uiLevel = std::max (lvl, uiLevel.load() * 0.85f);
}

int SweepProcessor::getNumPrograms() { return (int) sweepfx::factoryPresets().size(); }

const juce::String SweepProcessor::getProgramName (int index)
{
    const auto& list = sweepfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void SweepProcessor::setCurrentProgram (int index)
{
    const auto& list = sweepfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::aType, (float) (int) p.a.type);
    set (IDs::aCutoff, p.a.cutoff);
    set (IDs::aReso, p.a.reso);
    set (IDs::aDrive, p.a.drive);
    set (IDs::aMod, p.a.mod);
    set (IDs::bOn, p.b.on ? 1.0f : 0.0f);
    set (IDs::bType, (float) (int) p.b.type);
    set (IDs::bCutoff, p.b.cutoff);
    set (IDs::bReso, p.b.reso);
    set (IDs::bDrive, p.b.drive);
    set (IDs::bMod, p.b.mod);
    set (IDs::routing, p.routing == sweepfx::Routing::parallel ? 1.0f : 0.0f);
    set (IDs::sync, p.sync ? 1.0f : 0.0f);
    set (IDs::division, (float) p.division);
    set (IDs::rate, p.rateHz);
    set (IDs::shape, (float) (int) p.shape);
    set (IDs::lfoDepth, p.lfoDepth);
    set (IDs::stereo, p.stereo);
    set (IDs::glide, p.glide);
    set (IDs::retrig, p.retrig ? 1.0f : 0.0f);
    for (int i = 0; i < sweepfx::numSteps; ++i) set (IDs::step (i), p.pattern[(size_t) i]);
    set (IDs::envDepth, p.envDepth);
    set (IDs::envSpeed, p.envSpeed);
    set (IDs::trigger, p.trigger == sweepfx::Trigger::transient ? 1.0f : 0.0f);
    set (IDs::sens, p.sensitivity);
    set (IDs::attack, p.attackMs);
    set (IDs::decay, p.decayMs);
    set (IDs::sustain, p.sustain);
    set (IDs::release, p.releaseMs);
    set (IDs::adsrDepth, p.adsrDepth);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void SweepProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SweepProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            apvts.replaceState (tree);
            // APVTS skips parameters whose snapped value already matches (bool / choice): push every value exactly
            for (auto* param : getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    const auto child = tree.getChildWithProperty ("id", ranged->paramID);
                    if (child.isValid() && child.hasProperty ("value"))
                        ranged->setValueNotifyingHost (ranged->convertTo0to1 ((float) child.getProperty ("value")));
                }
        }
}

juce::AudioProcessorEditor* SweepProcessor::createEditor() { return new SweepEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SweepProcessor(); }
