#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

SnapProcessor::SnapProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SNAP1", createLayout())
{
    pAttack = apvts.getRawParameterValue (IDs::attack);
    pSustain = apvts.getRawParameterValue (IDs::sustain);
    pBand = apvts.getRawParameterValue (IDs::band);
    pSplit = apvts.getRawParameterValue (IDs::split);
    pSpeed = apvts.getRawParameterValue (IDs::speed);
    pLookahead = apvts.getRawParameterValue (IDs::lookahead);
    pClip = apvts.getRawParameterValue (IDs::clip);
    pCeiling = apvts.getRawParameterValue (IDs::ceiling);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout SnapProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto spct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const int p = roundToInt (v * 100.0f);
        return (p > 0 ? "+" : "") + String (p) + " %";
    });
    auto db = [] (float v) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; };

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::attack, 1 }, "Attack", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, spct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::sustain, 1 }, "Sustain", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, spct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::band, 1 }, "Shape Band", StringArray { "Full", "Above Split", "Below Split" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::split, 1 }, "Split Frequency",
                                                  NormalisableRange<float> (40.0f, 12000.0f, 1.0f, 0.3f), 200.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::speed, 1 }, "Detector Speed", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::lookahead, 1 }, "Lookahead", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::clip, 1 }, "Output Clipper", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::ceiling, 1 }, "Clip Ceiling", NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([db] (float v, int) { return db (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([db] (float v, int) { return db (v); })));
    return l;
}

bool SnapProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SnapProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParams (currentParams());
    updateLatency();
}

void SnapProcessor::updateLatency()
{
    lastLookahead = pLookahead->load() > 0.5f;
    const int lat = lastLookahead ? engine.getLookaheadSamples() : 0;
    uiLatency = lat;
    if (getLatencySamples() != lat)
        setLatencySamples (lat);
}

snapfx::Params SnapProcessor::currentParams() const
{
    snapfx::Params p;
    p.attack = pAttack->load();
    p.sustain = pSustain->load();
    p.band = (snapfx::Band) juce::jlimit (0, 2, (int) pBand->load());
    p.splitHz = pSplit->load();
    p.speed = pSpeed->load();
    p.lookahead = pLookahead->load() > 0.5f;
    p.clip = pClip->load() > 0.5f;
    p.ceilingDb = pCeiling->load();
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    return p;
}

void SnapProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    if ((pLookahead->load() > 0.5f) != lastLookahead)
        updateLatency();

    float in = 0.0f;
    for (int c = 0; c < numCh; ++c)
        in = std::max (in, buffer.getMagnitude (c, 0, n));

    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numCh, n);

    float out = 0.0f;
    for (int c = 0; c < numCh; ++c)
        out = std::max (out, buffer.getMagnitude (c, 0, n));

    uiInput = std::max (in, uiInput.load() * 0.85f);
    uiOutput = std::max (out, uiOutput.load() * 0.85f);
    uiGainDb = engine.getGainDb();
    uiClip = engine.getClipAmount();
}

int SnapProcessor::getNumPrograms() { return (int) snapfx::factoryPresets().size(); }

const juce::String SnapProcessor::getProgramName (int index)
{
    const auto& list = snapfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void SnapProcessor::setCurrentProgram (int index)
{
    const auto& list = snapfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::attack, p.attack);
    set (IDs::sustain, p.sustain);
    set (IDs::band, (float) (int) p.band);
    set (IDs::split, p.splitHz);
    set (IDs::speed, p.speed);
    set (IDs::lookahead, p.lookahead ? 1.0f : 0.0f);
    set (IDs::clip, p.clip ? 1.0f : 0.0f);
    set (IDs::ceiling, p.ceilingDb);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void SnapProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SnapProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* SnapProcessor::createEditor() { return new SnapEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SnapProcessor(); }
