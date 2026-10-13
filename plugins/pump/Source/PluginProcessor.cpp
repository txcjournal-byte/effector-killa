#include "PluginProcessor.h"
#include "PluginEditor.h"

PumpProcessor::PumpProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PUMP", createLayout())
{
    pRate = apvts.getRawParameterValue (IDs::rate);
    pTrigger = apvts.getRawParameterValue (IDs::trigger);
    pBand = apvts.getRawParameterValue (IDs::band);
    pCrossover = apvts.getRawParameterValue (IDs::crossover);
    pDepth = apvts.getRawParameterValue (IDs::depth);
    pSmooth = apvts.getRawParameterValue (IDs::smooth);
    pThreshold = apvts.getRawParameterValue (IDs::threshold);
    pMix = apvts.getRawParameterValue (IDs::mix);
    pOutput = apvts.getRawParameterValue (IDs::output);
    curvePoints = pump::Curve::preset (0).getPoints();
}

juce::AudioProcessorValueTreeState::ParameterLayout PumpProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray rates;
    for (auto* n : pump::rateNames) rates.add (n);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::rate, 1 }, "Rate", rates, 3));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::trigger, 1 }, "Trigger", StringArray { "Sync", "MIDI", "Audio" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::band, 1 }, "Band", StringArray { "Full", "Low", "High" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::crossover, 1 }, "Crossover",
                                                  NormalisableRange<float> (40.0f, 1000.0f, 1.0f, 0.4f), 120.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100)) + " %"; });
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::depth, 1 }, "Depth", NormalisableRange<float> (0, 1, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::smooth, 1 }, "Smooth",
                                                  NormalisableRange<float> (0.1f, 20.0f, 0.01f, 0.4f), 1.5f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::threshold, 1 }, "Threshold",
                                                  NormalisableRange<float> (-48.0f, 0.0f, 0.1f), -12.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::mix, 1 }, "Mix", NormalisableRange<float> (0, 1, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return l;
}

bool PumpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void PumpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    curveDirty = true;
    setLatencySamples (0);
}

void PumpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    const int numCh = std::min (mainIn.getNumChannels(), 2);

    const float* scPtrs[2] = { nullptr, nullptr };
    int scCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        scCh = std::min (sc.getNumChannels(), 2);
        for (int c = 0; c < scCh; ++c) scPtrs[c] = sc.getReadPointer (c);
    }

    if (curveDirty.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType lock (curveLock);
        if (lock.isLocked())
        {
            pump::Curve c;
            c.setPoints (curvePoints);
            engine.setCurve (c);
        }
        else
            curveDirty = true; // try again next block
    }

    pump::Params p;
    p.rate = (int) pRate->load();
    p.trigger = (pump::Trigger) (int) pTrigger->load();
    p.band = (pump::Band) (int) pBand->load();
    p.crossoverHz = pCrossover->load();
    p.depth = pDepth->load();
    p.smoothMs = pSmooth->load();
    p.threshold = juce::Decibels::decibelsToGain (pThreshold->load());
    p.mix = pMix->load();
    p.outputDb = pOutput->load();
    engine.setParams (p);

    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn())
            engine.addNoteOn (meta.samplePosition);

    pump::HostTime t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
        }

    engine.process (mainIn.getArrayOfWritePointers(), numCh, scCh > 0 ? scPtrs : nullptr, scCh, n, t);

    // mono in / stereo out never happens (layouts are matched), but keep extra outs clean
    for (int c = numCh; c < mainOut.getNumChannels(); ++c)
        mainOut.clear (c, 0, n);

    uiPhase = engine.getPhase();
    uiGain = engine.getCurrentGain();
    if (engine.hasTriggered()) ++uiTriggerCount;
}

std::vector<pump::Point> PumpProcessor::getCurvePoints() const
{
    const juce::SpinLock::ScopedLockType lock (curveLock);
    return curvePoints;
}

void PumpProcessor::setCurvePoints (std::vector<pump::Point> pts)
{
    pump::Curve c;
    c.setPoints (std::move (pts)); // sanitise
    {
        const juce::SpinLock::ScopedLockType lock (curveLock);
        curvePoints = c.getPoints();
    }
    curveDirty = true;
}

void PumpProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms()) return;
    currentProgram = index;
    setCurvePoints (pump::Curve::preset (index).getPoints());
}

void PumpProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    juce::ValueTree curve ("CURVE");
    for (const auto& pt : getCurvePoints())
    {
        juce::ValueTree v ("P");
        v.setProperty ("x", pt.x, nullptr);
        v.setProperty ("y", pt.y, nullptr);
        v.setProperty ("t", pt.tension, nullptr);
        curve.appendChild (v, nullptr);
    }
    state.removeChild (state.getChildWithName ("CURVE"), nullptr);
    state.appendChild (curve, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PumpProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            std::vector<pump::Point> pts;
            auto curve = tree.getChildWithName ("CURVE");
            for (int i = 0; i < curve.getNumChildren(); ++i)
            {
                auto v = curve.getChild (i);
                pts.push_back ({ (float) v.getProperty ("x"), (float) v.getProperty ("y"), (float) v.getProperty ("t") });
            }
            tree.removeChild (curve, nullptr);
            apvts.replaceState (tree);
            // APVTS skips parameters whose snapped value already matches (bool / choice), which can leave an
            // un-snapped raw value behind. Push every stored value explicitly so the restore is exact.
            for (auto* param : getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                {
                    const auto child = tree.getChildWithProperty ("id", ranged->paramID);
                    if (child.isValid() && child.hasProperty ("value"))
                        ranged->setValueNotifyingHost (ranged->convertTo0to1 ((float) child.getProperty ("value")));
                }
            if (! pts.empty())
                setCurvePoints (pts);
        }
}

juce::AudioProcessorEditor* PumpProcessor::createEditor() { return new PumpEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PumpProcessor(); }
