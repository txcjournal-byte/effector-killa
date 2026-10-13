#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

AutoKillaProcessor::AutoKillaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AUTOKILLA", createLayout())
{
    pMode = apvts.getRawParameterValue (IDs::mode);
    pKey = apvts.getRawParameterValue (IDs::key);
    pScale = apvts.getRawParameterValue (IDs::scale);
    pRetune = apvts.getRawParameterValue (IDs::retune);
    pHumanize = apvts.getRawParameterValue (IDs::humanize);
    pThreshold = apvts.getRawParameterValue (IDs::threshold);
    pVibrato = apvts.getRawParameterValue (IDs::vibrato);
    pVibRate = apvts.getRawParameterValue (IDs::vibRate);
    pTranspose = apvts.getRawParameterValue (IDs::transpose);
    pThroat = apvts.getRawParameterValue (IDs::throat);
    pFormant = apvts.getRawParameterValue (IDs::formant);
    pMidi = apvts.getRawParameterValue (IDs::midi);
    pAutoKey = apvts.getRawParameterValue (IDs::autoKey);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
    for (int i = 0; i < 12; ++i)
        pNotes[(size_t) i] = apvts.getRawParameterValue (IDs::note (i));
    engine.setParams (currentParams());
    setLatencySamples (akill::Engine::latencyFor (currentParams().mode, 48000.0));
}

AutoKillaProcessor::~AutoKillaProcessor() { cancelPendingUpdate(); }

juce::AudioProcessorValueTreeState::ParameterLayout AutoKillaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Mode", StringArray { "Live", "Mix" }, 1));
    StringArray keys, scales;
    for (auto* k : akill::keyNames) keys.add (k);
    for (auto* s : akill::scaleNames) scales.add (s);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::key, 1 }, "Key", keys, 9));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::scale, 1 }, "Scale", scales, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::retune, 1 }, "Retune Speed",
                                                  NormalisableRange<float> (0.0f, 400.0f, 0.1f, 0.4f), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v < 0.5f ? String ("HARD") : String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::humanize, 1 }, "Humanize", NormalisableRange<float> (0, 1, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::threshold, 1 }, "Threshold", NormalisableRange<float> (5.0f, 100.0f, 0.1f), 100.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v >= 99.5f ? String ("ALL") : String (roundToInt (v)) + " ct"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::vibrato, 1 }, "Vibrato", NormalisableRange<float> (0, 1, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::vibRate, 1 }, "Vibrato Rate", NormalisableRange<float> (3.0f, 8.0f, 0.01f), 5.5f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " Hz"; })));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { IDs::transpose, 1 }, "Transpose", -12, 12, 0,
                                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                { return (v > 0 ? "+" : "") + String (v) + " st"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::throat, 1 }, "Throat", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      const float r = std::round (v * 10.0f) / 10.0f;
                                                      return (r > 0.04f ? "+" : "") + String (r, 1) + " st";
                                                  })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::formant, 1 }, "Formant Preserve", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::midi, 1 }, "MIDI Target Notes", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::autoKey, 1 }, "Auto Key", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0, 1, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::note (i), 1 }, String ("Custom ") + akill::keyNames[(size_t) i], true));
    return l;
}

akill::Params AutoKillaProcessor::currentParams() const
{
    akill::Params p;
    p.mode = pMode->load() > 0.5f ? akill::Mode::mix : akill::Mode::live;
    p.key = juce::jlimit (0, 11, (int) pKey->load());
    p.scale = juce::jlimit (0, akill::numScales - 1, (int) pScale->load());
    int mask = 0;
    for (int i = 0; i < 12; ++i)
        if (pNotes[(size_t) i]->load() > 0.5f) mask |= 1 << i;
    p.customMask = mask;
    p.retuneMs = pRetune->load();
    p.humanize = pHumanize->load();
    p.thresholdCents = pThreshold->load();
    p.vibrato = pVibrato->load();
    p.vibratoRateHz = pVibRate->load();
    p.transpose = (float) juce::roundToInt (pTranspose->load());
    p.throatSemis = pThroat->load();
    p.preserveFormants = pFormant->load() > 0.5f;
    p.midiTarget = pMidi->load() > 0.5f;
    p.autoKey = pAutoKey->load() > 0.5f;
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    return p;
}

bool AutoKillaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void AutoKillaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    pendingLatency = engine.getLatencySamples();
    setLatencySamples (engine.getLatencySamples());
    held.fill (false);
}

void AutoKillaProcessor::handleAsyncUpdate()
{
    if (pendingLatency.load() != getLatencySamples())
        setLatencySamples (pendingLatency.load());
}

void AutoKillaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) { run (buffer, midi, false); }
void AutoKillaProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) { run (buffer, midi, true); }

void AutoKillaProcessor::run (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    if (numCh <= 0) return;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) held[(size_t) m.getNoteNumber()] = true;
        else if (m.isNoteOff()) held[(size_t) m.getNoteNumber()] = false;
        else if (m.isAllNotesOff() || m.isAllSoundOff()) held.fill (false);
    }
    int mask = 0;
    for (int n = 0; n < 128; ++n)
        if (held[(size_t) n]) mask |= 1 << (n % 12);
    engine.setMidiMask (mask);
    uiMidiMask = mask;

    auto p = currentParams();
    if (bypassed) p.mix = 0.0f;   // latency-compensated dry while bypassed
    engine.setParams (p);

    float lvl = 0.0f;
    for (int c = 0; c < numCh; ++c)
        lvl = std::max (lvl, buffer.getMagnitude (c, 0, buffer.getNumSamples()));
    uiLevel = std::max (lvl, uiLevel.load() * 0.85f);

    engine.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());

    if (engine.getLatencySamples() != pendingLatency.load())
    {
        pendingLatency = engine.getLatencySamples();
        triggerAsyncUpdate();
    }
}

bool AutoKillaProcessor::applyDetectedKey()
{
    const int k = engine.uiDetectedKey.load();
    if (k < 0) return false;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::key, (float) k);
    set (IDs::scale, engine.uiDetectedMinor.load() ? 1.0f : 0.0f);
    return true;
}

int AutoKillaProcessor::getNumPrograms() { return (int) akill::factoryPresets().size(); }

const juce::String AutoKillaProcessor::getProgramName (int index)
{
    const auto& list = akill::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void AutoKillaProcessor::setCurrentProgram (int index)
{
    const auto& list = akill::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& pr = list[(size_t) index];
    const auto& p = pr.params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::mode, p.mode == akill::Mode::mix ? 1.0f : 0.0f);
    if (pr.scale >= 0) set (IDs::scale, (float) pr.scale);
    set (IDs::retune, p.retuneMs);
    set (IDs::humanize, p.humanize);
    set (IDs::threshold, p.thresholdCents);
    set (IDs::vibrato, p.vibrato);
    set (IDs::vibRate, p.vibratoRateHz);
    set (IDs::transpose, p.transpose);
    set (IDs::throat, p.throatSemis);
    set (IDs::formant, p.preserveFormants ? 1.0f : 0.0f);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void AutoKillaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AutoKillaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
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

juce::AudioProcessorEditor* AutoKillaProcessor::createEditor() { return new AutoKillaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AutoKillaProcessor(); }
