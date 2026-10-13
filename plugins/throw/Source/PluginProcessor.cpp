#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

ThrowProcessor::ThrowProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "THROW1", createLayout())
{
    pSync = apvts.getRawParameterValue (IDs::sync);
    pDivision = apvts.getRawParameterValue (IDs::division);
    pTime = apvts.getRawParameterValue (IDs::time);
    pMode = apvts.getRawParameterValue (IDs::mode);
    pFeedback = apvts.getRawParameterValue (IDs::feedback);
    pDrive = apvts.getRawParameterValue (IDs::drive);
    pWobble = apvts.getRawParameterValue (IDs::wobble);
    pLowCut = apvts.getRawParameterValue (IDs::lowCut);
    pHighCut = apvts.getRawParameterValue (IDs::highCut);
    pDuck = apvts.getRawParameterValue (IDs::duck);
    pThrowOnly = apvts.getRawParameterValue (IDs::throwOnly);
    pThrow = apvts.getRawParameterValue (IDs::throwOn);
    pFreeze = apvts.getRawParameterValue (IDs::freeze);
    pWidth = apvts.getRawParameterValue (IDs::width);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout ThrowProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto hz = [] (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; };

    StringArray divs;
    for (auto* d : throwfx::divisionNames) divs.add (d);

    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::sync, 1 }, "Tempo Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::division, 1 }, "Note Value", divs, throwfx::defaultDivision));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::time, 1 }, "Time (free)",
                                                  NormalisableRange<float> (throwfx::minFreeMs, throwfx::maxFreeMs, 0.1f, 0.4f), 350.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v < 100.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Mode", StringArray { "Stereo", "Ping-Pong", "Mono" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::feedback, 1 }, "Feedback",
                                                  NormalisableRange<float> (0.0f, throwfx::maxFeedback, 0.001f), 0.35f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::drive, 1 }, "Tape Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::wobble, 1 }, "Wow & Flutter", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::lowCut, 1 }, "Low Cut",
                                                  NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.35f), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int)
                                                  { return v <= 20.5f ? String ("OFF") : hz (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::highCut, 1 }, "High Cut",
                                                  NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.45f), 20000.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int)
                                                  { return v >= 19950.0f ? String ("OFF") : hz (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::duck, 1 }, "Duck", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::throwOnly, 1 }, "Throw Only", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::throwOn, 1 }, "Throw", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::freeze, 1 }, "Freeze", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::width, 1 }, "Width", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    return l;
}

bool ThrowProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ThrowProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

throwfx::Params ThrowProcessor::currentParams() const
{
    throwfx::Params p;
    p.sync = pSync->load() > 0.5f;
    p.division = (int) pDivision->load();
    p.timeMs = pTime->load();
    p.bpm = uiBpm.load();
    p.mode = (throwfx::Mode) juce::jlimit (0, 2, (int) pMode->load());
    p.feedback = pFeedback->load();
    p.drive = pDrive->load();
    p.wobble = pWobble->load();
    p.lowCutHz = pLowCut->load();
    p.highCutHz = pHighCut->load();
    p.duck = pDuck->load();
    p.throwOnly = pThrowOnly->load() > 0.5f;
    p.throwOn = pThrow->load() > 0.5f;
    p.freeze = pFreeze->load() > 0.5f;
    p.width = pWidth->load();
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    return p;
}

void ThrowProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    // host tempo (falls back to the last known tempo, 120 BPM by default)
    bool hostTempo = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0)
                {
                    uiBpm = *bpm;
                    hostTempo = true;
                }
    uiHostTempo = hostTempo;

    float in = 0.0f;
    for (int c = 0; c < numCh; ++c)
        in = std::max (in, buffer.getMagnitude (c, 0, n));

    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numCh, n);

    uiInput = std::max (in, uiInput.load() * 0.8f);
    uiWet = std::max (engine.getWetLevel(), uiWet.load() * 0.8f);
    uiDuck = engine.getDuckGain();
    uiSend = engine.getSendGate();
    uiDelayMs = engine.getCurrentDelayMs();
}

int ThrowProcessor::getNumPrograms() { return (int) throwfx::factoryPresets().size(); }

const juce::String ThrowProcessor::getProgramName (int index)
{
    const auto& list = throwfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void ThrowProcessor::setCurrentProgram (int index)
{
    const auto& list = throwfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::sync, p.sync ? 1.0f : 0.0f);
    set (IDs::division, (float) p.division);
    set (IDs::time, p.timeMs);
    set (IDs::mode, (float) (int) p.mode);
    set (IDs::feedback, p.feedback);
    set (IDs::drive, p.drive);
    set (IDs::wobble, p.wobble);
    set (IDs::lowCut, p.lowCutHz);
    set (IDs::highCut, p.highCutHz);
    set (IDs::duck, p.duck);
    set (IDs::throwOnly, p.throwOnly ? 1.0f : 0.0f);
    set (IDs::throwOn, 0.0f);
    set (IDs::freeze, 0.0f);
    set (IDs::width, p.width);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void ThrowProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ThrowProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* ThrowProcessor::createEditor() { return new ThrowEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ThrowProcessor(); }
