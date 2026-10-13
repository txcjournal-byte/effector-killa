#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

WideProcessor::WideProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WIDE1", createLayout())
{
    pLowX = apvts.getRawParameterValue (IDs::lowX);
    pHighX = apvts.getRawParameterValue (IDs::highX);
    pWLow = apvts.getRawParameterValue (IDs::widthLow);
    pWMid = apvts.getRawParameterValue (IDs::widthMid);
    pWHigh = apvts.getRawParameterValue (IDs::widthHigh);
    pMono = apvts.getRawParameterValue (IDs::monoBelow);
    pHaas = apvts.getRawParameterValue (IDs::haas);
    pHaasSide = apvts.getRawParameterValue (IDs::haasSide);
    pBalance = apvts.getRawParameterValue (IDs::balance);
    pStereoize = apvts.getRawParameterValue (IDs::stereoize);
    pOutput = apvts.getRawParameterValue (IDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout WideProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto hzText = [] (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([hzText] (float v, int) { return hzText (v); });
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                                              .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::lowX, 1 }, "Low Crossover", NormalisableRange<float> (40.0f, 1000.0f, 1.0f, 0.4f), 200.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::highX, 1 }, "High Crossover", NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.45f), 3000.0f, hz));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::widthLow, 1 }, "Low Width", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::widthMid, 1 }, "Mid Width", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::widthHigh, 1 }, "High Width", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::monoBelow, 1 }, "Mono Below", NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.5f), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hzText] (float v, int)
                                                  { return v <= widefx::monoOffHz ? String ("OFF") : hzText (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::haas, 1 }, "Haas", NormalisableRange<float> (0.0f, widefx::maxHaasMs, 0.01f, 0.6f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v <= 0.0f ? String ("OFF") : String (v, 1) + " ms"; })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::haasSide, 1 }, "Haas Side", StringArray { "Left", "Right" }, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::balance, 1 }, "M/S Balance", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      const int i = roundToInt (v * 100.0f);
                                                      return i == 0 ? String ("CENTER") : (i < 0 ? "M " + String (-i) : "S " + String (i));
                                                  })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::stereoize, 1 }, "Stereoize", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

bool WideProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void WideProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

widefx::Params WideProcessor::currentParams() const
{
    widefx::Params p;
    p.lowXHz = pLowX->load();
    p.highXHz = pHighX->load();
    p.widthLow = pWLow->load();
    p.widthMid = pWMid->load();
    p.widthHigh = pWHigh->load();
    p.monoBelowHz = pMono->load();
    p.haasMs = pHaas->load();
    p.haasSide = pHaasSide->load() > 0.5f ? widefx::HaasSide::right : widefx::HaasSide::left;
    p.balance = pBalance->load();
    p.stereoize = pStereoize->load();
    p.outputDb = pOutput->load();
    return p;
}

void WideProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numCh, n);

    uiCorr = engine.getCorrelation();
    for (int c = 0; c < 2; ++c) uiPeak[(size_t) c] = std::max (engine.getPeak (c), uiPeak[(size_t) c].load() * 0.85f);
    for (int b = 0; b < 3; ++b) uiBand[(size_t) b] = engine.getBandSideDb (b);
}

int WideProcessor::getNumPrograms() { return (int) widefx::factoryPresets().size(); }

const juce::String WideProcessor::getProgramName (int index)
{
    const auto& list = widefx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void WideProcessor::setCurrentProgram (int index)
{
    const auto& list = widefx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::lowX, p.lowXHz);
    set (IDs::highX, p.highXHz);
    set (IDs::widthLow, p.widthLow);
    set (IDs::widthMid, p.widthMid);
    set (IDs::widthHigh, p.widthHigh);
    set (IDs::monoBelow, std::max (widefx::monoOffHz, p.monoBelowHz));
    set (IDs::haas, p.haasMs);
    set (IDs::haasSide, p.haasSide == widefx::HaasSide::right ? 1.0f : 0.0f);
    set (IDs::balance, p.balance);
    set (IDs::stereoize, p.stereoize);
    set (IDs::output, p.outputDb);
}

void WideProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void WideProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* WideProcessor::createEditor() { return new WideEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WideProcessor(); }
