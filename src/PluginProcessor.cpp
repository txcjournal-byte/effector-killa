#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BendDemonProcessor::BendDemonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BENDDEMON", bd::createParameterLayout()),
      snapshot (apvts),
      host (std::make_unique<bd::HostedPluginManager> (forwardingPlayHead)),
      presets (apvts)
{
    engine.setVisualSink (&visualFifo);
    engine.setSettings (snapshot.read());

    host->onPluginChanged = [this]
    {
        // New synth: release everything (old notes) and send the MPE configuration again.
        engine.requestReleaseAll();
        engine.requestZoneConfiguration();
        updateLatency();
    };

    startTimerHz (10);
}

BendDemonProcessor::~BendDemonProcessor()
{
    stopTimer();
    host.reset();
}

//==============================================================================
bool BendDemonProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BendDemonProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax (16, samplesPerBlock);

    engine.setSettings (snapshot.read());
    engine.prepare (sampleRate);           // also schedules the MPE zone configuration
    host->prepare (sampleRate, maxBlockSize);

    // Preallocate MIDI buffers so the audio thread never allocates.
    mpeBuffer.ensureSize (8192);
    chunkIn.ensureSize (2048);
    chunkOut.ensureSize (8192);
    hostedMidi.ensureSize (8192);

    gain.reset (sampleRate, 0.03);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (snapshot.outputGainDb()));

    wasPlaying = false;
    lastMidiOutMode = snapshot.midiOutMode();
    updateLatency();
}

void BendDemonProcessor::releaseResources()
{
    host->release();
}

//==============================================================================
bd::TransportState BendDemonProcessor::readTransport()
{
    bd::TransportState ts;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            ts.hasInfo   = true;
            ts.isPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm())         ts.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) ts.ppq = *ppq;
            if (auto sig = pos->getTimeSignature())
            {
                ts.timeSigNumerator   = sig->numerator;
                ts.timeSigDenominator = sig->denominator;
            }
        }
    }

    return ts;
}

void BendDemonProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = audio.getNumSamples();

    // --- parameters + transport -------------------------------------------------------
    const auto settings = snapshot.read();
    engine.setSettings (settings);
    configuredVoices.store (settings.numVoices, std::memory_order_relaxed);
    bendRangeNow.store (settings.bendRange, std::memory_order_relaxed);
    engineLatency.store (engine.getLatencySamples(), std::memory_order_relaxed);

    const auto ts = readTransport();

    // Transport stopped -> release every voice so nothing hangs (FL also sends note-offs, but
    // our delayed note-offs / dives must be cut too).
    if (ts.hasInfo && wasPlaying && ! ts.isPlaying)
        engine.requestReleaseAll();
    wasPlaying = ts.hasInfo && ts.isPlaying;

    if (wasBypassed)
    {
        // coming back from bypass: start clean
        engine.requestZoneConfiguration();
        wasBypassed = false;
    }

    const bool midiOutMode = snapshot.midiOutMode();
    bool sendToBoth = false;
    if (midiOutMode != lastMidiOutMode)
    {
        // switching destination: release on both sides and reconfigure
        engine.requestReleaseAll();
        engine.requestZoneConfiguration();
        sendToBoth = true;
        lastMidiOutMode = midiOutMode;
    }

    gain.setTargetValue (juce::Decibels::decibelsToGain (snapshot.outputGainDb()));

    visualFifo.publishTransport (engine.getSampleClock(), currentSampleRate, ts.bpm, ts.ppq,
                                 ts.hasInfo, ts.isPlaying, ts.timeSigNumerator, ts.timeSigDenominator);

    // --- process in chunks no longer than the prepared block size ----------------------
    audio.clear();
    mpeBuffer.clear();

    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const int len = juce::jmin (maxBlockSize, numSamples - start);

        chunkIn.clear();
        chunkIn.addEvents (midi, start, len, -start);

        auto chunkTs = ts;
        if (start > 0)
            chunkTs.ppq += (double) start / currentSampleRate * ts.bpm / 60.0;

        chunkOut.clear();
        engine.process (chunkIn, chunkOut, len, chunkTs);
        processChunk (audio, start, len, chunkOut, mpeBuffer, midiOutMode, sendToBoth);
        sendToBoth = false;
    }

    activeVoices.store (engine.getNumActiveVoices(), std::memory_order_relaxed);

    // MIDI output: in MIDI-out mode the generated MPE, otherwise nothing.
    midi.swapWith (mpeBuffer);

    // Output gain
    if (! midiOutMode)
        gain.applyGain (audio, numSamples);
    else
        gain.skip (numSamples);
}

void BendDemonProcessor::processChunk (juce::AudioBuffer<float>& audio, int startSample, int numSamples,
                                       const juce::MidiBuffer& mpe, juce::MidiBuffer& midiOut,
                                       bool midiOutMode, bool sendToBoth)
{
    if (midiOutMode || sendToBoth)
        midiOut.addEvents (mpe, 0, numSamples, startSample);

    auto* slot = host->beginAudio();

    if (slot != nullptr && slot->instance != nullptr && (! midiOutMode || sendToBoth))
    {
        auto& scratch = slot->scratch;
        const int chans = juce::jmin (scratch.getNumChannels(),
                                      juce::jmax (slot->instance->getTotalNumInputChannels(),
                                                  slot->instance->getTotalNumOutputChannels()));

        if (chans > 0 && numSamples <= scratch.getNumSamples())
        {
            juce::AudioBuffer<float> view (scratch.getArrayOfWritePointers(), chans, numSamples);
            view.clear();

            hostedMidi.clear();
            hostedMidi.addEvents (mpe, 0, numSamples, 0);

            slot->instance->processBlock (view, hostedMidi);

            if (! midiOutMode)
            {
                const int outCh = slot->instance->getTotalNumOutputChannels();
                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                {
                    const int src = outCh > 0 ? juce::jmin (ch, outCh - 1) : -1;   // mono synth -> both sides
                    if (src >= 0)
                        audio.copyFrom (ch, startSample, view, src, 0, numSamples);
                }
            }
        }
    }

    host->endAudio();
}

void BendDemonProcessor::processBlockBypassed (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    // Bypassed: silence, and make sure the hosted synth does not keep hanging notes.
    audio.clear();

    if (! wasBypassed)
    {
        wasBypassed = true;
        mpeBuffer.clear();
        engine.releaseAll (mpeBuffer, 0);

        if (auto* slot = host->beginAudio(); slot != nullptr && slot->instance != nullptr)
        {
            const int n = juce::jmin (audio.getNumSamples(), slot->scratch.getNumSamples());
            juce::AudioBuffer<float> view (slot->scratch.getArrayOfWritePointers(), slot->scratch.getNumChannels(), n);
            view.clear();
            hostedMidi.clear();
            hostedMidi.addEvents (mpeBuffer, 0, n, 0);
            slot->instance->processBlock (view, hostedMidi);
        }
        host->endAudio();

        if (snapshot.midiOutMode())
        {
            midi.swapWith (mpeBuffer);
            return;
        }
    }

    midi.clear();
}

//==============================================================================
void BendDemonProcessor::updateLatency()
{
    const int total = engineLatency.load() + host->getHostedLatencySamples();
    if (total != reportedLatency)
    {
        reportedLatency = total;
        setLatencySamples (total);
    }
}

void BendDemonProcessor::timerCallback()
{
    // Latency can change when CHORD GLIDE is toggled or the hosted synth changes its own latency.
    updateLatency();
}

//==============================================================================
int BendDemonProcessor::getNumPrograms()
{
    return (int) bd::PresetManager::getFactoryPresets().size();
}

void BendDemonProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    // Hosts may call this from any thread; parameter changes are fine, but do it on the
    // message thread when possible to keep listeners happy.
    if (juce::MessageManager::existsAndIsCurrentThread())
        presets.loadFactory (index);
    else
        juce::MessageManager::callAsync ([this, index] { presets.loadFactory (index); });
}

const juce::String BendDemonProcessor::getProgramName (int index)
{
    const auto& all = bd::PresetManager::getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) all.size()) ? all[(size_t) index].name : juce::String();
}

//==============================================================================
void BendDemonProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);

    auto xml = state.createXml();
    xml->addChildElement (host->createStateXml().release());
    copyXmlToBinary (*xml, dest);
}

void BendDemonProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;

    std::unique_ptr<juce::XmlElement> hosted;
    if (auto* h = xml->getChildByName ("HOSTED"))
    {
        hosted = std::make_unique<juce::XmlElement> (*h);
        xml->removeChildElement (h, true);
    }

    if (xml->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        presets.setCurrentName (tree.getProperty ("presetName", "Init").toString());
        apvts.replaceState (tree);
    }

    if (hosted != nullptr)
    {
        auto restore = [this, h = std::shared_ptr<juce::XmlElement> (std::move (hosted))]
        {
            host->restoreFromStateXml (*h);
        };

        if (juce::MessageManager::existsAndIsCurrentThread())
            restore();
        else
            juce::MessageManager::callAsync (restore);
    }
}

//==============================================================================
juce::AudioProcessorEditor* BendDemonProcessor::createEditor()
{
    return new BendDemonEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BendDemonProcessor();
}
