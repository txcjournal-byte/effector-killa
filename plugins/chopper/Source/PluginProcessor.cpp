#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

ChopperProcessor::ChopperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CHOPPER1", createLayout())
{
    pRate = apvts.getRawParameterValue (IDs::rate);
    pSize = apvts.getRawParameterValue (IDs::size);
    pSwing = apvts.getRawParameterValue (IDs::swing);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
    pHold = apvts.getRawParameterValue (IDs::hold);

    chopper::Pattern p {};
    chopper::patternFromString (chopper::factoryPresets().front().pattern, p);
    setPattern (p);
}

juce::AudioProcessorValueTreeState::ParameterLayout ChopperProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });

    StringArray rates, sizes;
    for (auto* r : chopper::rateNames) rates.add (r);
    for (auto* s : chopper::sizeNames) sizes.add (s);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::rate, 1 }, "Step Rate", rates, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::size, 1 }, "Repeat Size", sizes, 3));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::swing, 1 }, "Swing", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::hold, 1 }, "Hold", false));
    return l;
}

bool ChopperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ChopperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

chopper::Fx ChopperProcessor::getStep (int i) const
{
    if (i < 0 || i >= chopper::numSteps) return chopper::Fx::off;
    return (chopper::Fx) juce::jlimit (0, chopper::numFx - 1, (int) steps[(size_t) i].load (std::memory_order_relaxed));
}

void ChopperProcessor::setStep (int i, chopper::Fx fx)
{
    if (i < 0 || i >= chopper::numSteps || getStep (i) == fx) return;
    steps[(size_t) i].store ((uint8_t) fx, std::memory_order_relaxed);
    ++patternVersion;
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

chopper::Pattern ChopperProcessor::getPattern() const
{
    chopper::Pattern p {};
    for (int i = 0; i < chopper::numSteps; ++i) p[(size_t) i] = getStep (i);
    return p;
}

void ChopperProcessor::setPattern (const chopper::Pattern& p)
{
    for (int i = 0; i < chopper::numSteps; ++i)
        steps[(size_t) i].store ((uint8_t) p[(size_t) i], std::memory_order_relaxed);
    ++patternVersion;
}

void ChopperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    chopper::HostTime t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.hasPpq = true; }
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
        }

    chopper::Params p;
    p.rate = juce::jlimit (0, chopper::numRates - 1, (int) pRate->load());
    p.size = juce::jlimit (0, chopper::numSizes - 1, (int) pSize->load());
    p.swing = pSwing->load();
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    p.hold = pHold->load() > 0.5f;
    engine.setParams (p);
    engine.setPattern (getPattern());

    engine.process (buffer.getArrayOfWritePointers(), numCh, n, t);

    float lvl = 0.0f;
    for (int c = 0; c < numCh; ++c)
        lvl = std::max (lvl, buffer.getMagnitude (c, 0, n));
    uiLevel = std::max (lvl, uiLevel.load() * 0.85f);
    uiStep = engine.getCurrentStep();
    uiFx = (int) engine.getCurrentFx();
    uiPhase = engine.getStepPhase();
    uiBpm = engine.getBpm();
    uiHostPlaying = t.playing && t.hasPpq;
}

int ChopperProcessor::getNumPrograms() { return (int) chopper::factoryPresets().size(); }

const juce::String ChopperProcessor::getProgramName (int index)
{
    const auto& list = chopper::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void ChopperProcessor::setCurrentProgram (int index)
{
    const auto& list = chopper::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& pr = list[(size_t) index];
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::rate, (float) pr.rate);
    set (IDs::size, (float) pr.size);
    set (IDs::swing, pr.swing);
    set (IDs::mix, pr.mix);
    set (IDs::output, pr.outputDb);
    set (IDs::hold, 0.0f);
    chopper::Pattern p {};
    if (chopper::patternFromString (pr.pattern, p))
        setPattern (p);
}

void ChopperProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("pattern", juce::String (chopper::patternToString (getPattern())), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ChopperProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            const auto patText = tree.getProperty ("pattern").toString();
            tree.removeProperty ("pattern", nullptr);
            apvts.replaceState (tree);
            // APVTS skips parameters whose snapped value already matches (bool / choice): push every value exactly
            for (auto* param : getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    const auto child = tree.getChildWithProperty ("id", ranged->paramID);
                    if (child.isValid() && child.hasProperty ("value"))
                        ranged->setValueNotifyingHost (ranged->convertTo0to1 ((float) child.getProperty ("value")));
                }
            chopper::Pattern p {};
            if (chopper::patternFromString (patText.toStdString(), p))
                setPattern (p);
        }
}

juce::AudioProcessorEditor* ChopperProcessor::createEditor() { return new ChopperEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChopperProcessor(); }
