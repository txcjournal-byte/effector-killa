#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

GlowProcessor::GlowProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GLOW1", createLayout())
{
    pMode = apvts.getRawParameterValue (IDs::mode);
    pGlow = apvts.getRawParameterValue (IDs::glow);
    pContrast = apvts.getRawParameterValue (IDs::contrast);
    pColour = apvts.getRawParameterValue (IDs::colour);
    pOutput = apvts.getRawParameterValue (IDs::output);
    pAutoGain = apvts.getRawParameterValue (IDs::autoGain);
}

juce::AudioProcessorValueTreeState::ParameterLayout GlowProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                                              .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });
    auto bip = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                              {
                                                  const int i = roundToInt (v * 100.0f);
                                                  return (i > 0 ? "+" : "") + String (i);
                                              })
                                              .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });
    StringArray modes;
    for (auto* m : glowfx::modeNames) modes.add (m);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Mode", modes, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::glow, 1 }, "Glow", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::contrast, 1 }, "Contrast", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, bip));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::colour, 1 }, "Colour", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, bip));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::autoGain, 1 }, "Auto Gain", true));
    return l;
}

bool GlowProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void GlowProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

glowfx::Params GlowProcessor::currentParams() const
{
    glowfx::Params p;
    p.mode = (glowfx::Mode) juce::jlimit (0, glowfx::numModes - 1, (int) pMode->load());
    p.glow = pGlow->load();
    p.contrast = pContrast->load();
    p.colour = pColour->load();
    p.outputDb = pOutput->load();
    p.autoGain = pAutoGain->load() > 0.5f;
    return p;
}

void GlowProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    auto hold = [] (std::atomic<float>& a, float v) { a = std::max (v, a.load() * 0.85f); };
    hold (uiIn, engine.getInputPeak());
    hold (uiOut, engine.getOutputPeak());
    hold (uiGr, engine.getLimiterGrDb());
    uiAg = engine.getAutoGainDb();
    uiHarm = engine.getHarmonics();
    for (int b = 0; b < 3; ++b)
        uiLift[(size_t) b] = uiLift[(size_t) b].load() * 0.7f + 0.3f * engine.getBandLiftDb (b);
}

int GlowProcessor::getNumPrograms() { return (int) glowfx::factoryPresets().size(); }

const juce::String GlowProcessor::getProgramName (int index)
{
    const auto& list = glowfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void GlowProcessor::setCurrentProgram (int index)
{
    const auto& list = glowfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::mode, (float) (int) p.mode);
    set (IDs::glow, p.glow);
    set (IDs::contrast, p.contrast);
    set (IDs::colour, p.colour);
    set (IDs::output, p.outputDb);
    set (IDs::autoGain, p.autoGain ? 1.0f : 0.0f);
}

void GlowProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void GlowProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* GlowProcessor::createEditor() { return new GlowEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlowProcessor(); }
