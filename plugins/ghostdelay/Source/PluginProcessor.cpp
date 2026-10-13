#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

GhostDelayProcessor::GhostDelayProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GHOSTDELAY1", createLayout())
{
    pMode = apvts.getRawParameterValue (IDs::mode);
    pSync = apvts.getRawParameterValue (IDs::sync);
    pDivision = apvts.getRawParameterValue (IDs::division);
    pTime = apvts.getRawParameterValue (IDs::time);
    pLrRatio = apvts.getRawParameterValue (IDs::lrRatio);
    pFeedback = apvts.getRawParameterValue (IDs::feedback);
    pLowCut = apvts.getRawParameterValue (IDs::lowCut);
    pHighCut = apvts.getRawParameterValue (IDs::highCut);
    pModDepth = apvts.getRawParameterValue (IDs::modDepth);
    pModRate = apvts.getRawParameterValue (IDs::modRate);
    pAge = apvts.getRawParameterValue (IDs::age);
    pDuck = apvts.getRawParameterValue (IDs::duck);
    pFreeze = apvts.getRawParameterValue (IDs::freeze);
    pGhostPitch = apvts.getRawParameterValue (IDs::ghostPitch);
    pPattern = apvts.getRawParameterValue (IDs::pattern);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
    setCurrentProgram (0);
}

static juce::String ratioText (float v)
{
    double l, r;
    ghostdelay::ratioFactors (v, l, r);
    if (std::abs (v) < 0.005f) return "1 : 1";
    return juce::String (l, 2) + " : " + juce::String (r, 2);
}

juce::AudioProcessorValueTreeState::ParameterLayout GhostDelayProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto hz = [] (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; };

    StringArray modes, divs, pitches, pats;
    for (auto* m : ghostdelay::modeNames) modes.add (m);
    for (auto* d : ghostdelay::divisionNames) divs.add (d);
    for (auto* g : ghostdelay::ghostPitchNames) pitches.add (g);
    for (auto* p : ghostdelay::patternNames) pats.add (p);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Mode", modes, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::sync, 1 }, "Tempo Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::division, 1 }, "Note Value", divs, ghostdelay::defaultDivision));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::time, 1 }, "Time (free)",
                                                  NormalisableRange<float> (ghostdelay::minFreeMs, ghostdelay::maxFreeMs, 0.1f, 0.35f), 375.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v < 100.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::lrRatio, 1 }, "L/R Ratio", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return ratioText (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::feedback, 1 }, "Feedback",
                                                  NormalisableRange<float> (0.0f, ghostdelay::maxFeedback, 0.001f), 0.4f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::lowCut, 1 }, "Loop Low Cut",
                                                  NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.35f), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int)
                                                  { return v <= 20.5f ? String ("OFF") : hz (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::highCut, 1 }, "Loop High Cut",
                                                  NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.45f), 20000.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int)
                                                  { return v >= 19950.0f ? String ("OFF") : hz (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::modDepth, 1 }, "Mod Depth", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::modRate, 1 }, "Mod Rate",
                                                  NormalisableRange<float> (ghostdelay::minModRate, ghostdelay::maxModRate, 0.001f, 0.4f), 0.8f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return String (v, v < 1.0f ? 2 : 1) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::age, 1 }, "Age", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::duck, 1 }, "Duck", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::freeze, 1 }, "Freeze", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::ghostPitch, 1 }, "Ghost Pitch", pitches, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::pattern, 1 }, "Rhythm Pattern", pats, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

bool GhostDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void GhostDelayProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

ghostdelay::Params GhostDelayProcessor::currentParams() const
{
    ghostdelay::Params p;
    p.mode = (ghostdelay::Mode) juce::jlimit (0, ghostdelay::numModes - 1, (int) pMode->load());
    p.sync = pSync->load() > 0.5f;
    p.division = juce::jlimit (0, ghostdelay::numDivisions - 1, (int) pDivision->load());
    p.timeMs = pTime->load();
    p.bpm = uiBpm.load();
    p.lrRatio = pLrRatio->load();
    p.feedback = pFeedback->load();
    p.lowCutHz = pLowCut->load();
    p.highCutHz = pHighCut->load();
    p.modDepth = pModDepth->load();
    p.modRate = pModRate->load();
    p.age = pAge->load();
    p.duck = pDuck->load();
    p.freeze = pFreeze->load() > 0.5f;
    p.ghostPitch = juce::jlimit (0, ghostdelay::numGhostPitches - 1, (int) pGhostPitch->load());
    p.pattern = juce::jlimit (0, ghostdelay::numPatterns - 1, (int) pPattern->load());
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    return p;
}

void GhostDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    // host tempo + position (falls back to the last known tempo, 120 BPM by default)
    bool hostTempo = false;
    hostPlaying = false;
    hostPpq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0)
                {
                    uiBpm = *bpm;
                    hostTempo = true;
                }
            hostPlaying = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) hostPpq = *ppq;
        }
    uiHostTempo = hostTempo;

    float in = 0.0f;
    for (int c = 0; c < numCh; ++c)
        in = std::max (in, buffer.getMagnitude (c, 0, n));

    auto p = currentParams();
    p.playing = hostPlaying;
    p.ppq = hostPpq;
    engine.setParams (p);
    engine.process (buffer.getArrayOfWritePointers(), numCh, n);

    uiInput = std::max (in, uiInput.load() * 0.8f);
    uiWet = std::max (engine.getWetLevel(), uiWet.load() * 0.8f);
    uiDuck = engine.getDuckGain();
    uiDelayMs = engine.getCurrentDelayMs();
    uiRevPhase = engine.getReversePhase();
}

int GhostDelayProcessor::getNumPrograms() { return (int) ghostdelay::factoryPresets().size(); }

const juce::String GhostDelayProcessor::getProgramName (int index)
{
    const auto& list = ghostdelay::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void GhostDelayProcessor::setCurrentProgram (int index)
{
    const auto& list = ghostdelay::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::mode, (float) (int) p.mode);
    set (IDs::sync, p.sync ? 1.0f : 0.0f);
    set (IDs::division, (float) p.division);
    set (IDs::time, p.timeMs);
    set (IDs::lrRatio, p.lrRatio);
    set (IDs::feedback, p.feedback);
    set (IDs::lowCut, p.lowCutHz);
    set (IDs::highCut, p.highCutHz);
    set (IDs::modDepth, p.modDepth);
    set (IDs::modRate, p.modRate);
    set (IDs::age, p.age);
    set (IDs::duck, p.duck);
    set (IDs::freeze, 0.0f);
    set (IDs::ghostPitch, (float) p.ghostPitch);
    set (IDs::pattern, (float) p.pattern);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void GhostDelayProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void GhostDelayProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* GhostDelayProcessor::createEditor() { return new GhostDelayEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GhostDelayProcessor(); }
