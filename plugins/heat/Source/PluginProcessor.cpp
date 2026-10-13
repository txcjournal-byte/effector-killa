#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

HeatProcessor::HeatProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HEAT1", createLayout())
{
    for (auto& b : uiBand) b.store (0.0f);
    pBands = apvts.getRawParameterValue (IDs::bands);
    pXlo = apvts.getRawParameterValue (IDs::xoverLo);
    pXhi = apvts.getRawParameterValue (IDs::xoverHi);
    pOs = apvts.getRawParameterValue (IDs::os);
    pTiltPre = apvts.getRawParameterValue (IDs::tiltPre);
    pTiltPost = apvts.getRawParameterValue (IDs::tiltPost);
    pLowCut = apvts.getRawParameterValue (IDs::lowCut);
    pHighCut = apvts.getRawParameterValue (IDs::highCut);
    pAutoGain = apvts.getRawParameterValue (IDs::autoGain);
    pPunch = apvts.getRawParameterValue (IDs::punch);
    pDynamics = apvts.getRawParameterValue (IDs::dynamics);
    pLowKeep = apvts.getRawParameterValue (IDs::lowKeep);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        auto& bp = pBand[(size_t) b];
        bp.model = apvts.getRawParameterValue (IDs::band (b, "model"));
        bp.drive = apvts.getRawParameterValue (IDs::band (b, "drive"));
        bp.mix = apvts.getRawParameterValue (IDs::band (b, "mix"));
        bp.level = apvts.getRawParameterValue (IDs::band (b, "level"));
        bp.solo = apvts.getRawParameterValue (IDs::band (b, "solo"));
    }
    setLatencySamples (heatfx::latencyForChoice (heatfx::defaultOs));
}

juce::AudioProcessorValueTreeState::ParameterLayout HeatProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto bip = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                                           { const int k = roundToInt (v * 100.0f); return (k > 0 ? "+" : "") + String (k) + " %"; });
    auto hz = [] (float v) { return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto dbAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; });
    auto logRange = [] (float lo, float hi, float centre, float step)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };

    StringArray models, oss;
    for (auto* m : heatfx::modelNames) models.add (m);
    for (auto* o : heatfx::osNames) oss.add (o);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::bands, 1 }, "Bands", StringArray { "1 Band", "2 Bands", "3 Bands" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::xoverLo, 1 }, "Crossover Low", logRange (30.0f, 8000.0f, 400.0f, 1.0f), 180.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int) { return hz (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::xoverHi, 1 }, "Crossover High", logRange (300.0f, 16000.0f, 2500.0f, 1.0f), 2500.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int) { return hz (v); })));
    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::band (b, "model"), 1 }, n + "Model", models, 0));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::band (b, "drive"), 1 }, n + "Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f, pct));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::band (b, "mix"), 1 }, n + "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, pct));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::band (b, "level"), 1 }, n + "Level", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttr));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::band (b, "solo"), 1 }, n + "Solo", false));
    }
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::os, 1 }, "Oversampling", oss, heatfx::defaultOs));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::tiltPre, 1 }, "Tilt Pre", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::tiltPost, 1 }, "Tilt Post", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::lowCut, 1 }, "Low Cut", logRange (20.0f, 1000.0f, 120.0f, 1.0f), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int)
                                                  { return v <= 20.5f ? String ("OFF") : hz (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::highCut, 1 }, "High Cut", logRange (1000.0f, 20000.0f, 6000.0f, 1.0f), 20000.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([hz] (float v, int)
                                                  { return v >= 19950.0f ? String ("OFF") : hz (v); })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::autoGain, 1 }, "Auto Gain", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::punch, 1 }, "Punch", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::dynamics, 1 }, "Dynamics", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, bip));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::lowKeep, 1 }, "Low Keep", NormalisableRange<float> (20.0f, 300.0f, 1.0f), 20.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return v <= 20.5f ? String ("OFF") : String (roundToInt (v)) + " Hz"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    return l;
}

bool HeatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void HeatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateUi = sampleRate;
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
}

heatfx::Params HeatProcessor::currentParams() const
{
    heatfx::Params p;
    p.bands = juce::jlimit (1, 3, (int) pBands->load() + 1);
    p.xoverLoHz = pXlo->load();
    p.xoverHiHz = pXhi->load();
    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        auto& d = p.band[(size_t) b];
        const auto& s = pBand[(size_t) b];
        d.model = juce::jlimit (0, heatfx::numModels - 1, (int) s.model->load());
        d.drive = s.drive->load();
        d.mix = s.mix->load();
        d.levelDb = s.level->load();
        d.solo = s.solo->load() > 0.5f;
    }
    p.oversampling = juce::jlimit (0, heatfx::numOsChoices - 1, (int) pOs->load());
    p.tiltPreDb = pTiltPre->load();
    p.tiltPostDb = pTiltPost->load();
    p.lowCutHz = pLowCut->load();
    p.highCutHz = pHighCut->load();
    p.autoGain = pAutoGain->load() > 0.5f;
    p.punch = pPunch->load();
    p.dynamics = pDynamics->load();
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    p.lowKeepHz = pLowKeep->load();
    return p;
}

void HeatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numCh = std::min (getTotalNumInputChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0) return;

    // input feed for the spectrum display (mono sum)
    const int startPos = scopePos.load (std::memory_order_relaxed);
    float inPk = 0.0f;
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (numCh > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            scopeIn[(size_t) ((startPos + i) & (scopeSize - 1))] = m;
            inPk = std::max (inPk, std::max (std::abs (l[i]), std::abs (r[i])));
        }
    }

    engine.setParams (currentParams());
    engine.process (buffer.getArrayOfWritePointers(), numCh, n);

    float outPk = 0.0f;
    {
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (numCh > 1 ? 1 : 0);
        for (int i = 0; i < n; ++i)
        {
            scopeOut[(size_t) ((startPos + i) & (scopeSize - 1))] = 0.5f * (l[i] + r[i]);
            outPk = std::max (outPk, std::max (std::abs (l[i]), std::abs (r[i])));
        }
    }
    scopePos.store ((startPos + n) & (scopeSize - 1), std::memory_order_release);

    uiIn = std::max (inPk, uiIn.load() * 0.85f);
    uiOut = std::max (outPk, uiOut.load() * 0.85f);
    for (int b = 0; b < heatfx::maxBands; ++b)
        uiBand[(size_t) b] = std::max (engine.getBandInputPeak (b), uiBand[(size_t) b].load() * 0.85f);
    uiAutoGainDb = engine.getAutoGainDb();
    uiPunchGr = engine.getPunchGrDb();
    uiDriveMod = engine.getDriveModDb();

    if (engine.getLatencySamples() != getLatencySamples())
        triggerAsyncUpdate();
}

void HeatProcessor::handleAsyncUpdate()
{
    setLatencySamples (engine.getLatencySamples());
}

int HeatProcessor::getNumPrograms() { return (int) heatfx::factoryPresets().size(); }

const juce::String HeatProcessor::getProgramName (int index)
{
    const auto& list = heatfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void HeatProcessor::setCurrentProgram (int index)
{
    const auto& list = heatfx::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::bands, (float) (p.bands - 1));
    set (IDs::xoverLo, p.xoverLoHz);
    set (IDs::xoverHi, p.xoverHiHz);
    for (int b = 0; b < heatfx::maxBands; ++b)
    {
        const auto& bp = p.band[(size_t) b];
        set (IDs::band (b, "model"), (float) bp.model);
        set (IDs::band (b, "drive"), bp.drive);
        set (IDs::band (b, "mix"), bp.mix);
        set (IDs::band (b, "level"), bp.levelDb);
        set (IDs::band (b, "solo"), 0.0f);
    }
    set (IDs::os, (float) p.oversampling);
    set (IDs::tiltPre, p.tiltPreDb);
    set (IDs::tiltPost, p.tiltPostDb);
    set (IDs::lowCut, p.lowCutHz);
    set (IDs::highCut, p.highCutHz);
    set (IDs::autoGain, p.autoGain ? 1.0f : 0.0f);
    set (IDs::punch, p.punch);
    set (IDs::dynamics, p.dynamics);
    set (IDs::lowKeep, p.lowKeepHz);
    set (IDs::mix, p.mix);
    set (IDs::output, p.outputDb);
}

void HeatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void HeatProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* HeatProcessor::createEditor() { return new HeatEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HeatProcessor(); }
