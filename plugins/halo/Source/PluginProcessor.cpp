#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    juce::NormalisableRange<float> r (lo, hi,
        [] (float a, float b, float t) { return a * std::pow (b / a, t); },
        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
        [] (float a, float b, float v) { return juce::jlimit (a, b, v); });
    return r;
}

juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 9950.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

juce::String pctText (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
} // namespace

HaloProcessor::HaloProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HALO1", createLayout())
{
    pPredelay = apvts.getRawParameterValue (IDs::predelay);
    pSize = apvts.getRawParameterValue (IDs::size);
    pDecay = apvts.getRawParameterValue (IDs::decay);
    pDiffusion = apvts.getRawParameterValue (IDs::diffusion);
    pMod = apvts.getRawParameterValue (IDs::mod);
    pDamping = apvts.getRawParameterValue (IDs::damping);
    pLowcut = apvts.getRawParameterValue (IDs::lowcut);
    pWidth = apvts.getRawParameterValue (IDs::width);
    pDuck = apvts.getRawParameterValue (IDs::duck);
    pFreeze = apvts.getRawParameterValue (IDs::freeze);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout HaloProcessor::createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = A().withStringFromValueFunction ([] (float v, int) { return pctText (v); })
                  .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });
    auto hz = A().withStringFromValueFunction ([] (float v, int) { return hzText (v); })
                 .withValueFromStringFunction ([] (const String& s)
                 { const float v = s.getFloatValue(); return s.containsIgnoreCase ("k") ? v * 1000.0f : v; });

    NormalisableRange<float> pre (0.0f, 500.0f, 0.1f);
    pre.setSkewForCentre (80.0f);
    l.add (std::make_unique<F> (ParameterID { IDs::predelay, 1 }, "Pre-Delay", pre, 20.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return String (v < 10.0f ? v : std::round (v), v < 10.0f ? 1 : 0) + " ms"; })
                                   .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    l.add (std::make_unique<F> (ParameterID { IDs::size, 1 }, "Size", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::decay, 1 }, "Decay", logRange (halo::Engine::minDecay, halo::Engine::maxDecay), 3.5f,
                                A().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; })
                                   .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    l.add (std::make_unique<F> (ParameterID { IDs::diffusion, 1 }, "Diffusion", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::mod, 1 }, "Modulation", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::damping, 1 }, "Hi-Cut", logRange (1000.0f, 20000.0f), 9000.0f, hz));
    l.add (std::make_unique<F> (ParameterID { IDs::lowcut, 1 }, "Low-Cut", logRange (20.0f, 1000.0f), 150.0f, hz));
    l.add (std::make_unique<F> (ParameterID { IDs::width, 1 }, "Width", NormalisableRange<float> (0.0f, 1.5f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::duck, 1 }, "Duck", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::freeze, 1 }, "Freeze", false));
    l.add (std::make_unique<F> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })
                                   .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    return l;
}

bool HaloProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

halo::Params HaloProcessor::readParams() const
{
    halo::Params p;
    p.predelayMs = pPredelay->load();
    p.size = pSize->load();
    p.decaySec = pDecay->load();
    p.diffusion = pDiffusion->load();
    p.mod = pMod->load();
    p.hiCutHz = pDamping->load();
    p.lowCutHz = pLowcut->load();
    p.width = pWidth->load();
    p.duck = pDuck->load();
    p.freeze = pFreeze->load() > 0.5f;
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    return p;
}

void HaloProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

void HaloProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    // mono in -> stereo out: duplicate the input so the engine runs true stereo
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());
    else
        for (int c = numIn; c < numOut; ++c)
            buffer.clear (c, 0, buffer.getNumSamples());

    const int numCh = std::min (numOut, 2);
    engine.setParams (readParams());
    engine.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());

    uiIn = std::max (engine.getInputLevel(), uiIn.load() * 0.9f);
    uiWet = std::max (engine.getWetLevel(), uiWet.load() * 0.9f);
    uiDuck = engine.getDuckGain();
}

int HaloProcessor::getNumPrograms() { return (int) halo::factoryPresets().size(); }

const juce::String HaloProcessor::getProgramName (int index)
{
    const auto& list = halo::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void HaloProcessor::setCurrentProgram (int index)
{
    const auto& list = halo::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::predelay, p.predelayMs);
    set (IDs::size, p.size);
    set (IDs::decay, p.decaySec);
    set (IDs::diffusion, p.diffusion);
    set (IDs::mod, p.mod);
    set (IDs::damping, p.hiCutHz);
    set (IDs::lowcut, p.lowCutHz);
    set (IDs::width, p.width);
    set (IDs::duck, p.duck);
    set (IDs::freeze, p.freeze ? 1.0f : 0.0f);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void HaloProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void HaloProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, juce::jmax (0, getNumPrograms() - 1), (int) tree.getProperty ("program", 0));
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

juce::AudioProcessorEditor* HaloProcessor::createEditor() { return new HaloEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HaloProcessor(); }
