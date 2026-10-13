#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

VoxProcessor::VoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOX1", createLayout())
{
    pPitch = apvts.getRawParameterValue (IDs::pitch);
    pFormant = apvts.getRawParameterValue (IDs::formant);
    pTune = apvts.getRawParameterValue (IDs::tune);
    pKey = apvts.getRawParameterValue (IDs::key);
    pScale = apvts.getRawParameterValue (IDs::scale);
    pRetune = apvts.getRawParameterValue (IDs::retune);
    pMidi = apvts.getRawParameterValue (IDs::midi);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
    heldNotes.reserve (128);
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto st = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const float r = std::round (v * 10.0f) / 10.0f;
        return (r > 0.04f ? "+" : "") + String (r, 1) + " st";
    });
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::pitch, 1 }, "Pitch", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, st));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::formant, 1 }, "Formant", NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, st));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::tune, 1 }, "Tune", StringArray { "Off", "Chromatic", "Scale" }, 0));
    StringArray keys, scales;
    for (auto* k : vox::keyNames) keys.add (k);
    for (auto* s : vox::scaleNames) scales.add (s);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::key, 1 }, "Key", keys, 9));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::scale, 1 }, "Scale", scales, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::retune, 1 }, "Retune Speed",
                                                  NormalisableRange<float> (0.0f, 400.0f, 0.1f, 0.4f), 40.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v < 0.5f ? String ("HARD") : String (roundToInt (v)) + " ms"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::midi, 1 }, "MIDI Control", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0, 1, 0.001f), 1.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)) + " %"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return l;
}

bool VoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
    heldNotes.clear();
}

void VoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            if (heldNotes.size() < 128) heldNotes.push_back (m.getNoteNumber()); // reserved: no allocation on the audio thread
        }
        else if (m.isNoteOff())
            heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), m.getNoteNumber()), heldNotes.end());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            heldNotes.clear();
    }
    engine.setMidiNote (heldNotes.empty() ? -1 : heldNotes.back());

    vox::Params p;
    p.pitchSemis = pPitch->load();
    p.formantSemis = pFormant->load();
    p.tune = (vox::TuneMode) (int) pTune->load();
    p.key = (int) pKey->load();
    p.scale = (int) pScale->load();
    p.retuneMs = pRetune->load();
    p.midiControl = pMidi->load() > 0.5f;
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    engine.setParams (p);

    float lvl = 0.0f;
    for (int c = 0; c < numCh; ++c)
        lvl = std::max (lvl, buffer.getMagnitude (c, 0, buffer.getNumSamples()));
    uiLevel = std::max (lvl, uiLevel.load() * 0.85f);

    engine.process (buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());
    uiDetected = engine.getDetectedHz();
    uiTarget = engine.getTargetHz();
}

int VoxProcessor::getNumPrograms() { return (int) vox::factoryPresets().size(); }

const juce::String VoxProcessor::getProgramName (int index)
{
    const auto& list = vox::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void VoxProcessor::setCurrentProgram (int index)
{
    const auto& list = vox::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::pitch, p.pitchSemis);
    set (IDs::formant, p.formantSemis);
    set (IDs::tune, (float) (int) p.tune);
    set (IDs::key, (float) p.key);
    set (IDs::scale, (float) p.scale);
    set (IDs::retune, p.retuneMs);
    set (IDs::midi, p.midiControl ? 1.0f : 0.0f);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void VoxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void VoxProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* VoxProcessor::createEditor() { return new VoxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxProcessor(); }
