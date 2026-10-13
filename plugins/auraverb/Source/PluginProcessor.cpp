#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return juce::NormalisableRange<float> (lo, hi,
        [] (float a, float b, float t) { return a * std::pow (b / a, t); },
        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
        [] (float a, float b, float v) { return juce::jlimit (a, b, v); });
}

juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 9950.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

juce::String pctText (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

float hzFromText (const juce::String& s)
{
    const float v = s.getFloatValue();
    return s.containsIgnoreCase ("k") ? v * 1000.0f : v;
}
} // namespace

AuraVerbProcessor::AuraVerbProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AURAVERB1", createLayout())
{
    pMode = apvts.getRawParameterValue (IDs::mode);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pPredelay = apvts.getRawParameterValue (IDs::predelay);
    pPreSync = apvts.getRawParameterValue (IDs::preSync);
    pPreDiv = apvts.getRawParameterValue (IDs::preDiv);
    pDecay = apvts.getRawParameterValue (IDs::decay);
    pSize = apvts.getRawParameterValue (IDs::size);
    pEarlyLate = apvts.getRawParameterValue (IDs::earlyLate);
    pAttack = apvts.getRawParameterValue (IDs::attack);
    pDiffusion = apvts.getRawParameterValue (IDs::diffusion);
    pModDepth = apvts.getRawParameterValue (IDs::modDepth);
    pModRate = apvts.getRawParameterValue (IDs::modRate);
    pDamping = apvts.getRawParameterValue (IDs::damping);
    pLowCut = apvts.getRawParameterValue (IDs::lowCut);
    pBassMult = apvts.getRawParameterValue (IDs::bassMult);
    pWidth = apvts.getRawParameterValue (IDs::width);
    pDuck = apvts.getRawParameterValue (IDs::duck);
    pFreeze = apvts.getRawParameterValue (IDs::freeze);
    pColour = apvts.getRawParameterValue (IDs::colour);
    pOutput = apvts.getRawParameterValue (IDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout AuraVerbProcessor::createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = A().withStringFromValueFunction ([] (float v, int) { return pctText (v); })
                  .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });
    auto hz = A().withStringFromValueFunction ([] (float v, int) { return hzText (v); })
                 .withValueFromStringFunction (hzFromText);
    auto lin01 = NormalisableRange<float> (0.0f, 1.0f, 0.001f);

    StringArray modes, divs;
    for (auto* m : auraverb::modeNames) modes.add (m);
    for (auto* d : auraverb::divisionNames) divs.add (d);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Mode", modes, (int) auraverb::Mode::hall));
    l.add (std::make_unique<F> (ParameterID { IDs::mix, 1 }, "Mix", lin01, 0.30f, pct));

    NormalisableRange<float> pre (0.0f, 500.0f, 0.1f);
    pre.setSkewForCentre (80.0f);
    l.add (std::make_unique<F> (ParameterID { IDs::predelay, 1 }, "Pre-Delay", pre, 20.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return String (v < 10.0f ? v : std::round (v), v < 10.0f ? 1 : 0) + " ms"; })
                                   .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::preSync, 1 }, "Pre-Delay Sync", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::preDiv, 1 }, "Pre-Delay Note", divs, auraverb::defaultDivision));

    l.add (std::make_unique<F> (ParameterID { IDs::decay, 1 }, "Decay", logRange (auraverb::Engine::minDecay, auraverb::Engine::maxDecay), 2.5f,
                                A().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " s"; })
                                   .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    l.add (std::make_unique<F> (ParameterID { IDs::size, 1 }, "Size", lin01, 0.6f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::earlyLate, 1 }, "Early / Late", lin01, 0.65f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    if (v <= 0.005f) return String ("EARLY");
                                    if (v >= 0.995f) return String ("LATE");
                                    return "E" + String (roundToInt (std::min (1.0f, 2.0f * (1.0f - v)) * 100.0f)) + " L" + String (roundToInt (std::min (1.0f, 2.0f * v) * 100.0f));
                                })));
    l.add (std::make_unique<F> (ParameterID { IDs::attack, 1 }, "Attack / Build", lin01, 0.0f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::diffusion, 1 }, "Diffusion", lin01, 0.8f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::modDepth, 1 }, "Mod Depth", lin01, 0.35f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::modRate, 1 }, "Mod Rate", logRange (0.05f, 5.0f), 0.6f,
                                A().withStringFromValueFunction ([] (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; })
                                   .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    l.add (std::make_unique<F> (ParameterID { IDs::damping, 1 }, "Damping", logRange (1000.0f, 20000.0f), 8000.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return v >= 19500.0f ? String ("OFF") : hzText (v); })
                                   .withValueFromStringFunction ([] (const String& s) { return s.containsIgnoreCase ("off") ? 20000.0f : hzFromText (s); })));
    l.add (std::make_unique<F> (ParameterID { IDs::lowCut, 1 }, "Low Cut", logRange (20.0f, 1000.0f), 60.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return v <= 20.5f ? String ("OFF") : hzText (v); })
                                   .withValueFromStringFunction ([] (const String& s) { return s.containsIgnoreCase ("off") ? 20.0f : hzFromText (s); })));
    NormalisableRange<float> bass (0.25f, 2.5f, 0.01f);
    bass.setSkewForCentre (1.0f);
    l.add (std::make_unique<F> (ParameterID { IDs::bassMult, 1 }, "Bass Multiplier", bass, 1.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return "x" + String (v, 2); })
                                   .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("0123456789.").getFloatValue(); })));
    l.add (std::make_unique<F> (ParameterID { IDs::width, 1 }, "Width", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<F> (ParameterID { IDs::duck, 1 }, "Duck", lin01, 0.0f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::freeze, 1 }, "Freeze", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::colour, 1 }, "Colour", StringArray { "Modern", "Vintage" }, 0));
    l.add (std::make_unique<F> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                A().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })
                                   .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); })));
    return l;
}

bool AuraVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

auraverb::Params AuraVerbProcessor::currentParams() const
{
    auraverb::Params p;
    p.mode = (auraverb::Mode) juce::jlimit (0, auraverb::numModes - 1, (int) pMode->load());
    p.mix = pMix->load();
    p.predelayMs = pPredelay->load();
    p.predelaySync = pPreSync->load() > 0.5f;
    p.predelayDiv = juce::jlimit (0, auraverb::numDivisions - 1, (int) pPreDiv->load());
    p.bpm = uiBpm.load();
    p.decaySec = pDecay->load();
    p.size = pSize->load();
    p.earlyLate = pEarlyLate->load();
    p.attack = pAttack->load();
    p.diffusion = pDiffusion->load();
    p.modDepth = pModDepth->load();
    p.modRateHz = pModRate->load();
    p.dampingHz = pDamping->load();
    p.lowCutHz = pLowCut->load();
    p.bassMult = pBassMult->load();
    p.width = pWidth->load();
    p.duck = pDuck->load();
    p.freeze = pFreeze->load() > 0.5f;
    p.vintage = pColour->load() > 0.5f;
    p.outputDb = pOutput->load();
    return p;
}

void AuraVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

void AuraVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (n == 0 || numOut == 0) return;
    // mono in -> stereo out: duplicate the input so the engine runs true stereo
    if (numIn == 1 && numOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else
        for (int c = numIn; c < numOut; ++c)
            buffer.clear (c, 0, n);

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

    const int numCh = std::min (numOut, 2);
    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numCh, n);

    uiIn = std::max (engine.getInputLevel(), uiIn.load() * 0.9f);
    uiWet = std::max (engine.getWetLevel(), uiWet.load() * 0.9f);
    uiDuck = engine.getDuckGain();
}

int AuraVerbProcessor::getNumPrograms() { return (int) auraverb::factoryPresets().size(); }

const juce::String AuraVerbProcessor::getProgramName (int index)
{
    const auto& list = auraverb::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void AuraVerbProcessor::setCurrentProgram (int index)
{
    const auto& list = auraverb::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::mode, (float) (int) p.mode);
    set (IDs::mix, p.mix);
    set (IDs::predelay, p.predelayMs);
    set (IDs::preSync, p.predelaySync ? 1.0f : 0.0f);
    set (IDs::preDiv, (float) p.predelayDiv);
    set (IDs::decay, p.decaySec);
    set (IDs::size, p.size);
    set (IDs::earlyLate, p.earlyLate);
    set (IDs::attack, p.attack);
    set (IDs::diffusion, p.diffusion);
    set (IDs::modDepth, p.modDepth);
    set (IDs::modRate, p.modRateHz);
    set (IDs::damping, p.dampingHz);
    set (IDs::lowCut, p.lowCutHz);
    set (IDs::bassMult, p.bassMult);
    set (IDs::width, p.width);
    set (IDs::duck, p.duck);
    set (IDs::freeze, p.freeze ? 1.0f : 0.0f);
    set (IDs::colour, p.vintage ? 1.0f : 0.0f);
    set (IDs::output, p.outputDb);
}

void AuraVerbProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AuraVerbProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* AuraVerbProcessor::createEditor() { return new AuraVerbEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraVerbProcessor(); }
