#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
} // namespace

DustProcessor::DustProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DUST", createLayout())
{
    pAge = apvts.getRawParameterValue (IDs::age);
    pTape = apvts.getRawParameterValue (IDs::tape);
    pWobble = apvts.getRawParameterValue (IDs::wobble);
    pVinyl = apvts.getRawParameterValue (IDs::vinyl);
    pCrush = apvts.getRawParameterValue (IDs::crush);
    pTone = apvts.getRawParameterValue (IDs::tone);
    pWidth = apvts.getRawParameterValue (IDs::width);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
    pFollow = apvts.getRawParameterValue (IDs::follow);
}

juce::AudioProcessorValueTreeState::ParameterLayout DustProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (percent).withLabel ("");

    auto addPct = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                           NormalisableRange<float> (0.0f, 1.0f, 0.001f), def, pct));
    };

    const auto& init = dust::factoryPresets().front().params;
    addPct (IDs::age, "Age", init.age);
    addPct (IDs::tape, "Tape", init.tape);
    addPct (IDs::wobble, "Wobble", init.wobble);
    addPct (IDs::vinyl, "Vinyl", init.vinyl);
    addPct (IDs::crush, "Crush", init.crush);
    addPct (IDs::tone, "Tone", init.tone);
    addPct (IDs::width, "Width", init.width);
    addPct (IDs::mix, "Mix", init.mix);

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
                                                                                     { return String (v, 1) + " dB"; })));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::follow, 1 }, "Noise Follow", true));
    return layout;
}

bool DustProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void DustProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParams (readParams());
    setLatencySamples (engine.getLatencySamples());
}

dust::Params DustProcessor::readParams() const
{
    dust::Params p;
    p.age = pAge->load();
    p.tape = pTape->load();
    p.wobble = pWobble->load();
    p.vinyl = pVinyl->load();
    p.crush = pCrush->load();
    p.tone = pTone->load();
    p.width = pWidth->load();
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    p.noiseFollow = pFollow->load() > 0.5f;
    return p;
}

void DustProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    float inPeak = 0.0f;
    for (int c = 0; c < numCh; ++c)
        inPeak = std::max (inPeak, buffer.getMagnitude (c, 0, buffer.getNumSamples()));

    engine.setParams (readParams());
    engine.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());

    float outPeak = 0.0f;
    for (int c = 0; c < numCh; ++c)
        outPeak = std::max (outPeak, buffer.getMagnitude (c, 0, buffer.getNumSamples()));

    meterIn.store (std::max (inPeak, meterIn.load() * 0.9f));
    meterOut.store (std::max (outPeak, meterOut.load() * 0.9f));
}

int DustProcessor::getNumPrograms()
{
    return (int) dust::factoryPresets().size();
}

const juce::String DustProcessor::getProgramName (int index)
{
    const auto& list = dust::factoryPresets();
    if (index < 0 || index >= (int) list.size())
        return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void DustProcessor::setCurrentProgram (int index)
{
    const auto& list = dust::factoryPresets();
    if (index < 0 || index >= (int) list.size())
        return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;

    auto set = [this] (const char* id, float plainValue)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
    };
    set (IDs::age, p.age);
    set (IDs::tape, p.tape);
    set (IDs::wobble, p.wobble);
    set (IDs::vinyl, p.vinyl);
    set (IDs::crush, p.crush);
    set (IDs::tone, p.tone);
    set (IDs::width, p.width);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
    set (IDs::follow, p.noiseFollow ? 1.0f : 0.0f);
}

void DustProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DustProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* DustProcessor::createEditor()
{
    return new DustEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DustProcessor();
}
