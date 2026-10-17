#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BendDemonProcessor::BendDemonProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BENDDEMON", bd::createParameterLayout()),
      snapshot (apvts),
      host (std::make_unique<bd::HostedPluginManager> (forwardingPlayHead)),
      presets (apvts, patternStore)
{
    engine.setVisualSink (&visualFifo);
    engine.setSettings (snapshot.read());

    host->onPluginChanged = [this]
    {
        // New synth: release everything (old notes) and send the MPE configuration again.
        // Loading a synth makes it the active one (the slot menu can switch back to INTERNAL).
        if (host->hasPlugin())
        {
            useInternal.store (false);

            // Synths without MPE can't bend notes one by one: switch to mono slides and say so.
            if (! suppressAutoMono)
            {
                const bool mpe = bd::HostedPluginManager::isKnownMpeSynth (host->getDescription());
                if (auto* p = apvts.getParameter (bd::ids::seqMono))
                    p->setValueNotifyingHost (mpe ? 0.0f : 1.0f);
                nonMpeNotice.store (! mpe);
            }
        }
        else
        {
            nonMpeNotice.store (false);
        }
        engine.requestReleaseAll();
        engine.requestZoneConfiguration();
        player.requestResync();
        updateLatency();
    };

    // First open: a demo pattern with the built-in sound, so pressing play makes music right away.
    // (A saved project replaces it in setStateInformation.)
    patternStore.replaceAll (bd::tools::makeFactoryPattern (0), true);
    presets.setCurrentName (bd::tools::factoryPatternName (0));

    startTimerHz (25);
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
    player.prepare (sampleRate);
    synth.prepare (sampleRate, maxBlockSize);
    lastSound = -1;
    host->prepare (sampleRate, maxBlockSize);

    // Preallocate MIDI buffers so the audio thread never allocates.
    // Room for the densest case: 15 sequencer voices x (bend + pressure + CC74) every 32 samples,
    // plus the live gestures (~10 bytes per event).
    const size_t midiBytes = (size_t) 65536 + (size_t) maxBlockSize * 64;
    mpeBuffer.ensureSize (midiBytes);
    chunkIn.ensureSize (4096);
    chunkOut.ensureSize (midiBytes);
    hostedMidi.ensureSize (midiBytes);
    monoScratch.ensureSize (midiBytes);

    gain.reset (sampleRate, 0.03);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (snapshot.outputGainDb()));

    wasPlaying = false;
    lastDestination = 0;
    previewWasOn = false;
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

    // --- parameters ---------------------------------------------------------------------
    auto settings = snapshot.read();
    const bool drawMode = snapshot.drawMode();
    const bool monoSeq  = snapshot.monoMode();
    const int  n        = juce::jlimit (1, 15, settings.numVoices);

    // Channel plan (one MPE lower zone, members 2 .. 1+n):
    //   LIVE:          gestures 2 .. 1+n
    //   DRAW (MPE):    gestures 2 .. 1+live, sequencer 2+live .. 1+n
    //   DRAW (mono):   gestures 2 .. 1+n,    sequencer channel 1
    int live = n;
    if (drawMode && ! monoSeq)
        live = n >= 8 ? 4 : n / 2;
    const bool liveEnabled = live > 0;
    settings.liveChannels    = juce::jmax (1, live);
    settings.masterBendRange = (drawMode && monoSeq) ? settings.bendRange : 2;

    engine.setSettings (settings);
    configuredVoices.store (n, std::memory_order_relaxed);
    bendRangeNow.store (settings.bendRange, std::memory_order_relaxed);
    engineLatency.store (engine.getLatencySamples(), std::memory_order_relaxed);

    bd::PatternPlayer::Config pc;
    pc.mono = monoSeq;
    pc.firstChannel = juce::jmin (16, 2 + live);
    pc.lastChannel  = 1 + n;
    pc.bendRange    = settings.bendRange;
    player.setConfig (pc);

    {
        bool changed = false;
        auto* snap = patternStore.getExchange().acquire (changed);
        if (changed)
            player.setPattern (snap != nullptr ? &snap->pattern : nullptr);
    }

    // --- transport ----------------------------------------------------------------------
    const auto ts = readTransport();
    const bool hostPlaying = ts.hasInfo && ts.isPlaying;
    const double bpm = (ts.hasInfo && ts.bpm > 1.0) ? ts.bpm : 140.0;
    tempoNow.store (bpm, std::memory_order_relaxed);
    const double beatsPerSample = bpm / 60.0 / currentSampleRate;

    // Transport stopped -> release every voice so nothing hangs (FL also sends note-offs, but
    // our delayed note-offs / dives must be cut too).
    if (ts.hasInfo && wasPlaying && ! ts.isPlaying)
        engine.requestReleaseAll();
    wasPlaying = hostPlaying;

    if (hostPlaying && previewRequested.load())
        previewRequested.store (false);          // the host took over

    const bool preview = previewRequested.load() && ! hostPlaying;
    if (preview && ! previewWasOn)
        previewPpq = 0.0;                        // preview always starts at the pattern start
    previewWasOn = preview;

    const bool seqPlaying = drawMode && (hostPlaying || preview);
    const double seqPpq = hostPlaying ? ts.ppq : previewPpq;
    seqRunning.store (seqPlaying, std::memory_order_relaxed);

    if (wasBypassed)
    {
        // coming back from bypass: start clean
        engine.requestZoneConfiguration();
        player.requestResync();
        wasBypassed = false;
    }

    // --- destination: MIDI out, hosted synth or the internal synth ----------------------
    const bool midiOutMode = snapshot.midiOutMode();
    bool hostedLive = false;
    if (auto* slot = host->beginAudio(); slot != nullptr && slot->instance != nullptr)
        hostedLive = true;
    host->endAudio();

    const int destination = midiOutMode ? destMidi : ((hostedLive && ! useInternal.load()) ? destHosted : destInternal);
    int sendMask = destination;
    if (destination != lastDestination)
    {
        // switching: release on both sides and reconfigure
        engine.requestReleaseAll();
        engine.requestZoneConfiguration();
        player.requestResync();
        sendMask |= lastDestination;
        lastDestination = destination;
    }

    const int sound = snapshot.internalSound();
    if (sound != lastSound)
    {
        synth.setSound (sound);
        lastSound = sound;
    }
    synth.setBendRanges ((float) settings.bendRange, (float) settings.masterBendRange, drawMode && monoSeq);

    gain.setTargetValue (juce::Decibels::decibelsToGain (snapshot.outputGainDb()));

    if (preview)
        visualFifo.publishTransport (engine.getSampleClock(), currentSampleRate, bpm, previewPpq,
                                     true, true, ts.timeSigNumerator, ts.timeSigDenominator);
    else
        visualFifo.publishTransport (engine.getSampleClock(), currentSampleRate, ts.bpm, ts.ppq,
                                     ts.hasInfo, ts.isPlaying, ts.timeSigNumerator, ts.timeSigDenominator);

    // --- process in chunks no longer than the prepared block size ----------------------
    audio.clear();
    mpeBuffer.clear();

    const double capturePpqBase = (hostPlaying || preview) ? seqPpq : freeBeats;

    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const int len = juce::jmin (maxBlockSize, numSamples - start);

        chunkIn.clear();
        if (liveEnabled)
            chunkIn.addEvents (midi, start, len, -start);
        else
            for (const auto m : midi)   // no live channel left: keep controllers, drop notes
                if (m.samplePosition >= start && m.samplePosition < start + len && ! m.getMessage().isNoteOnOrOff())
                    chunkIn.addEvent (m.getMessage(), m.samplePosition - start);

        auto chunkTs = ts;
        if (start > 0)
            chunkTs.ppq += (double) start * ts.bpm / 60.0 / currentSampleRate;

        // A pending release-all / zone configuration of the gesture engine goes out at the start
        // of this chunk (and covers our channels too): the sequencer re-chases after it.
        const bool engineReset = engine.isResetPending();
        if (engineReset)
            player.requestResync();

        chunkOut.clear();
        engine.process (chunkIn, chunkOut, len, chunkTs);

        // Mono slides: the live gestures go to channel 1 too (a non-MPE synth hears one channel)
        if (monoSeq)
        {
            monoScratch.clear();
            for (const auto m : chunkOut)
            {
                auto msg = m.getMessage();
                if (msg.getChannel() > 1) msg.setChannel (1);
                monoScratch.addEvent (msg, m.samplePosition);
            }
            chunkOut.swapWith (monoScratch);
        }

        // CAPTURE records what the live gestures played (not the sequencer)
        captureFifo.push (chunkOut, capturePpqBase + (double) start * beatsPerSample, beatsPerSample);

        bd::PatternPlayer::Clock clk { seqPlaying, seqPpq + (double) start * beatsPerSample, bpm };
        player.process (chunkOut, len, clk, engineReset ? bd::MpeOutput::zoneConfigurationSpan : 0);

        processChunk (audio, start, len, chunkOut, mpeBuffer, destination, sendMask);
        sendMask = destination;
    }

    if (preview)
        previewPpq += (double) numSamples * beatsPerSample;
    freeBeats += (double) numSamples * beatsPerSample;

    activeVoices.store (engine.getNumActiveVoices() + player.getNumActive(), std::memory_order_relaxed);

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
                                       int destination, int sendMask)
{
    if ((sendMask & destMidi) != 0)
        midiOut.addEvents (mpe, 0, numSamples, startSample);

    auto* slot = host->beginAudio();

    if (slot != nullptr && slot->instance != nullptr && (sendMask & destHosted) != 0)
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

            if (destination == destHosted)
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

    // Internal synth: plays when it is the destination; keeps rendering its release tails for a
    // moment after a switch to the hosted synth.
    if (destination == destInternal)
        synth.render (audio, startSample, numSamples, mpe);
    else if ((sendMask & destInternal) != 0 || (destination == destHosted && synth.getNumActiveVoices() > 0))
    {
        if ((sendMask & destInternal) != 0)
            synth.allNotesOff();
        synth.render (audio, startSample, numSamples, juce::MidiBuffer());
    }
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
        player.releaseAll (mpeBuffer, 0);
        synth.allNotesOff();

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

    // pattern snapshots the audio thread no longer uses
    patternStore.collectGarbage();

    // CAPTURE: rolling buffer + armed pass
    captureFifo.drain ([this] (const bd::tools::CapturedEvent& e) { captureRecorder.add (e); });

    if (captureRecorder.isArmed())
    {
        const auto tv = visualFifo.getTransportNow();
        std::vector<bd::PatternLine> lines;
        if (captureRecorder.poll (tv.ppq, tv.hasPpq && tv.playing, bendRangeNow.load(), lines))
        {
            const auto n = (int) lines.size();
            applyCapturedLines (std::move (lines), captureRecorder.getRecordLength(), "Capture");
            if (onCaptureComplete) onCaptureComplete (n);
        }
    }
}

//==============================================================================
void BendDemonProcessor::setDrawMode (bool draw)
{
    if (auto* p = apvts.getParameter (bd::ids::mode))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (draw ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

void BendDemonProcessor::setUseInternalSynth (bool b) noexcept
{
    useInternal.store (b);
}

void BendDemonProcessor::setInternalSound (int index)
{
    if (auto* p = apvts.getParameter (bd::ids::internalSound))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, bd::InternalSynth::numSounds - 1, index)));
        p->endChangeGesture();
    }
}

juce::String BendDemonProcessor::getSynthSlotText() const
{
    if (isUsingInternalSynth())
        return juce::String (juce::CharPointer_UTF8 ("Internal \xc2\xb7 ")) + bd::InternalSynth::getSoundName (getInternalSound());
    return host->getPluginName();
}

void BendDemonProcessor::armCapture()
{
    captureRecorder.arm (patternStore.get().lengthBeats());
}

int BendDemonProcessor::captureLastBars (int bars)
{
    std::vector<bd::PatternLine> lines;
    double len = 0.0;
    if (! captureRecorder.captureLast (bars, patternStore.get().beatsPerBar, bendRangeNow.load(), lines, len))
        return 0;

    const auto n = (int) lines.size();
    applyCapturedLines (std::move (lines), len, "Capture last " + juce::String (bars) + " bars");
    return n;
}

void BendDemonProcessor::applyCapturedLines (std::vector<bd::PatternLine> lines, double lengthBeats, const juce::String& name)
{
    auto p = patternStore.get();
    p.lines.clear();
    p.bars = juce::jlimit (bd::Pattern::minBars, bd::Pattern::maxBars,
                           (int) std::lround (lengthBeats / (double) juce::jmax (1, p.beatsPerBar)));
    std::vector<uint32_t> ids;
    for (auto& l : lines)
        ids.push_back (p.add (std::move (l)));

    patternStore.perform (std::move (p), name, true);
    patternStore.setSelection (ids);
    setDrawMode (true);
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
    state.setProperty ("useInternalSynth", useInternal.load(), nullptr);

    auto xml = state.createXml();
    xml->addChildElement (host->createStateXml().release());
    xml->addChildElement (patternStore.createXml().release());
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

    std::shared_ptr<bd::Pattern> pattern;
    if (auto* pe = xml->getChildByName (bd::Pattern::xmlTag))
    {
        pattern = std::make_shared<bd::Pattern> (bd::Pattern::fromXml (*pe));
        xml->removeChildElement (pe, true);
    }

    if (xml->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        presets.setCurrentName (tree.getProperty ("presetName", "Init").toString());
        useInternal.store ((bool) tree.getProperty ("useInternalSynth", false));

        // A 1.x project has no pattern and no mode: keep its 1.x behaviour (LIVE).
        const bool fromV1 = tree.getProperty ("version", "").toString().startsWith ("1.");
        apvts.replaceState (tree);
        if (fromV1)
            if (auto* p = apvts.getParameter (bd::ids::mode))
                p->setValueNotifyingHost (0.0f);
    }

    {
        auto restorePattern = [this, pattern]
        {
            patternStore.replaceAll (pattern != nullptr ? *pattern : bd::Pattern(), true);
        };

        if (juce::MessageManager::existsAndIsCurrentThread())
            restorePattern();
        else
            juce::MessageManager::callAsync (restorePattern);
    }

    if (hosted != nullptr)
    {
        auto restore = [this, h = std::shared_ptr<juce::XmlElement> (std::move (hosted))]
        {
            // a restored project keeps its own mono / MPE choice
            suppressAutoMono = true;
            host->restoreFromStateXml (*h, [this] (juce::String) { suppressAutoMono = false; });
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
