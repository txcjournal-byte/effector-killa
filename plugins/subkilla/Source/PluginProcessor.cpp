#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

SubKillaProcessor::SubKillaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SUBKILLA1", createLayout())
{
    pMode = apvts.getRawParameterValue (IDs::mode);
    pGlide = apvts.getRawParameterValue (IDs::glide);
    pTune = apvts.getRawParameterValue (IDs::tune);
    pFine = apvts.getRawParameterValue (IDs::fine);
    pPunch = apvts.getRawParameterValue (IDs::punch);
    pPunchTime = apvts.getRawParameterValue (IDs::punchTime);
    pClick = apvts.getRawParameterValue (IDs::click);
    pSub = apvts.getRawParameterValue (IDs::sub);
    pDrive = apvts.getRawParameterValue (IDs::drive);
    pSatType = apvts.getRawParameterValue (IDs::satType);
    pDistMix = apvts.getRawParameterValue (IDs::distMix);
    pCutoff = apvts.getRawParameterValue (IDs::cutoff);
    pReso = apvts.getRawParameterValue (IDs::reso);
    pFltEnv = apvts.getRawParameterValue (IDs::fltEnv);
    pAttack = apvts.getRawParameterValue (IDs::attack);
    pDecay = apvts.getRawParameterValue (IDs::decay);
    pSustain = apvts.getRawParameterValue (IDs::sustain);
    pRelease = apvts.getRawParameterValue (IDs::release);
    pWidth = apvts.getRawParameterValue (IDs::width);
    pOutput = apvts.getRawParameterValue (IDs::output);
    pLimiter = apvts.getRawParameterValue (IDs::limiter);
    for (auto& m : heldMask) m = 0;
    for (auto& s : scope) s = 0.0f;
}

juce::AudioProcessorValueTreeState::ParameterLayout SubKillaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto msOrS = [] (float v)
    {
        if (v >= 1000.0f) return String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " s";
        if (v < 10.0f) return String (v, 1) + " ms";
        return String (roundToInt (v)) + " ms";
    };
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction ([msOrS] (float v, int) { return msOrS (v); });
    auto signedText = [] (float v, const char* unit) { const int r = roundToInt (v); return (r > 0 ? "+" : "") + String (r) + unit; };

    StringArray modes, sats;
    for (auto* m : subkilla::voiceModeNames) modes.add (m);
    for (auto* s : subkilla::satTypeNames) sats.add (s);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::mode, 1 }, "Voice Mode", modes, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::glide, 1 }, "Glide", NormalisableRange<float> (0.0f, 2000.0f, 0.1f, 0.35f), 90.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([msOrS] (float v, int)
                                                  { return v < 0.5f ? String ("OFF") : msOrS (v); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::tune, 1 }, "Tune", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([signedText] (float v, int) { return signedText (v, " st"); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::fine, 1 }, "Fine", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([signedText] (float v, int) { return signedText (v, " ct"); })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::punch, 1 }, "Punch", NormalisableRange<float> (0.0f, 48.0f, 0.1f), 14.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " st"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::punchTime, 1 }, "Punch Time", NormalisableRange<float> (5.0f, 400.0f, 0.1f, 0.4f), 45.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::click, 1 }, "Click", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::sub, 1 }, "Sub Octave", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f, pct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { IDs::satType, 1 }, "Saturation", sats, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::distMix, 1 }, "Distortion Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::cutoff, 1 }, "Cutoff", NormalisableRange<float> (30.0f, 20000.0f, 1.0f, 0.25f), 20000.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  {
                                                      if (v >= 19950.0f) return String ("OPEN");
                                                      return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz";
                                                  })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::reso, 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.1f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::fltEnv, 1 }, "Filter Env", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::attack, 1 }, "Attack", NormalisableRange<float> (0.0f, 200.0f, 0.1f, 0.4f), 1.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::decay, 1 }, "Decay", NormalisableRange<float> (30.0f, 12000.0f, 1.0f, 0.35f), 2500.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::sustain, 1 }, "Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::release, 1 }, "Release", NormalisableRange<float> (5.0f, 5000.0f, 1.0f, 0.35f), 250.0f, ms));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::width, 1 }, "Width", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, pct));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { IDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { IDs::limiter, 1 }, "Limiter", true));
    return l;
}

bool SubKillaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() > 0 && ! layouts.getMainInputChannelSet().isDisabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SubKillaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, samplesPerBlock);
    scopeDecim = juce::jmax (1, (int) std::round (sampleRate / 48000.0));
    scopeRate = sampleRate / scopeDecim;
    scopeCount = 0;
    for (auto& m : heldMask) m = 0;
}

subkilla::Params SubKillaProcessor::currentParams() const
{
    subkilla::Params p;
    p.mode = (subkilla::VoiceMode) juce::jlimit (0, 2, (int) pMode->load());
    p.glideMs = pGlide->load();
    p.tuneSemis = pTune->load();
    p.fineCents = pFine->load();
    p.punchSemis = pPunch->load();
    p.punchMs = pPunchTime->load();
    p.click = pClick->load();
    p.sub = pSub->load();
    p.drive = pDrive->load();
    p.sat = (subkilla::SatType) juce::jlimit (0, 3, (int) pSatType->load());
    p.distMix = pDistMix->load();
    p.cutoffHz = pCutoff->load();
    p.reso = pReso->load();
    p.fltEnv = pFltEnv->load();
    p.attackMs = pAttack->load();
    p.decayMs = pDecay->load();
    p.sustain = pSustain->load();
    p.releaseMs = pRelease->load();
    p.width = pWidth->load();
    p.outputDb = pOutput->load();
    p.limiter = pLimiter->load() > 0.5f;
    return p;
}

void SubKillaProcessor::setHeld (int note, bool on)
{
    if (note < 0 || note > 127) return;
    const auto bit = (juce::uint32) 1 << (note & 31);
    if (on) heldMask[(size_t) (note >> 5)].fetch_or (bit);
    else heldMask[(size_t) (note >> 5)].fetch_and (~bit);
}

bool SubKillaProcessor::isNoteHeld (int note) const
{
    if (note < 0 || note > 127) return false;
    return (heldMask[(size_t) (note >> 5)].load() >> (note & 31)) & 1u;
}

void SubKillaProcessor::uiNoteOn (int note, float velocity)
{
    const auto w = uiFifo.write (1);
    if (w.blockSize1 > 0)
    {
        auto& e = uiEvents[(size_t) w.startIndex1];
        e.type = subkilla::Event::noteOn;
        e.note = note;
        e.value = velocity;
    }
}

void SubKillaProcessor::uiNoteOff (int note)
{
    const auto w = uiFifo.write (1);
    if (w.blockSize1 > 0)
    {
        auto& e = uiEvents[(size_t) w.startIndex1];
        e.type = subkilla::Event::noteOff;
        e.note = note;
        e.value = 0.0f;
    }
}

void SubKillaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    // ---- collect events: on-screen keyboard first (offset 0), then host MIDI (sample accurate)
    int numEvents = 0;
    {
        const auto r = uiFifo.read (uiFifo.getNumReady());
        auto take = [&] (int start, int size)
        {
            for (int i = 0; i < size && numEvents < (int) events.size(); ++i)
            {
                auto e = uiEvents[(size_t) (start + i)];
                e.offset = 0;
                setHeld (e.note, e.type == subkilla::Event::noteOn);
                events[(size_t) numEvents++] = e;
            }
        };
        take (r.startIndex1, r.blockSize1);
        take (r.startIndex2, r.blockSize2);
    }
    for (const auto meta : midi)
    {
        if (numEvents >= (int) events.size()) break;
        const auto m = meta.getMessage();
        subkilla::Event e;
        e.offset = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (m.isNoteOn())
        {
            e.type = subkilla::Event::noteOn; e.note = m.getNoteNumber(); e.value = m.getFloatVelocity();
            setHeld (e.note, true);
        }
        else if (m.isNoteOff())
        {
            e.type = subkilla::Event::noteOff; e.note = m.getNoteNumber();
            setHeld (e.note, false);
        }
        else if (m.isPitchWheel())
        {
            e.type = subkilla::Event::pitchBend; e.value = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        }
        else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        {
            e.type = subkilla::Event::sustainPedal; e.value = m.isSustainPedalOn() ? 1.0f : 0.0f;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            e.type = subkilla::Event::allNotesOff;
            for (auto& h : heldMask) h = 0;
        }
        else
            continue;
        events[(size_t) numEvents++] = e;
    }

    engine.setParams (currentParams());

    // ---- render in chunks into scratch, events split per chunk
    float peak = 0.0f, gr = 0.0f;
    int ev = 0;
    for (int pos = 0; pos < n;)
    {
        const int len = juce::jmin ((int) scratchL.size(), n - pos);
        int ne = 0;
        while (ev < numEvents && events[(size_t) ev].offset < pos + len)
        {
            auto e = events[(size_t) ev++];
            e.offset = juce::jmax (0, e.offset - pos);
            chunkEvents[(size_t) ne++] = e;
        }
        engine.process (scratchL.data(), scratchR.data(), len, chunkEvents.data(), ne);
        gr = juce::jmin (gr, engine.getGainReductionDb());

        if (numCh >= 2)
        {
            buffer.copyFrom (0, pos, scratchL.data(), len);
            buffer.copyFrom (1, pos, scratchR.data(), len);
            for (int c = 2; c < numCh; ++c) buffer.clear (c, pos, len);
        }
        else if (numCh == 1)
        {
            auto* d = buffer.getWritePointer (0, pos);
            for (int i = 0; i < len; ++i) d[i] = 0.5f * (scratchL[(size_t) i] + scratchR[(size_t) i]);
        }

        // scope history (mono)
        int w = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < len; ++i)
        {
            const float m = 0.5f * (scratchL[(size_t) i] + scratchR[(size_t) i]);
            peak = juce::jmax (peak, std::abs (scratchL[(size_t) i]), std::abs (scratchR[(size_t) i]));
            if (++scopeCount >= scopeDecim)
            {
                scopeCount = 0;
                scope[(size_t) (w & (scopeSize - 1))].store (m, std::memory_order_relaxed);
                ++w;
            }
        }
        scopeWrite.store (w & 0x3fffffff, std::memory_order_release);
        pos += len;
    }

    uiHz = engine.getCurrentHz();
    uiNote = engine.getCurrentNote();
    uiEnv = engine.getAmpEnv();
    uiGliding = engine.isGliding();
    uiVoices = engine.getActiveVoices();
    uiGrDb = juce::jmin (gr, uiGrDb.load() * 0.9f + 0.0f);
    uiPeak = juce::jmax (peak, uiPeak.load() * 0.9f);
}

int SubKillaProcessor::getNumPrograms() { return (int) subkilla::factoryPresets().size(); }

const juce::String SubKillaProcessor::getProgramName (int index)
{
    const auto& list = subkilla::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return {};
    return juce::String (list[(size_t) index].category) + " - " + list[(size_t) index].name;
}

void SubKillaProcessor::setCurrentProgram (int index)
{
    const auto& list = subkilla::factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;
    const auto& p = list[(size_t) index].params;
    auto set = [this] (const char* id, float plain)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    };
    set (IDs::mode, (float) (int) p.mode);
    set (IDs::glide, p.glideMs);
    set (IDs::tune, p.tuneSemis);
    set (IDs::fine, p.fineCents);
    set (IDs::punch, p.punchSemis);
    set (IDs::punchTime, p.punchMs);
    set (IDs::click, p.click);
    set (IDs::sub, p.sub);
    set (IDs::drive, p.drive);
    set (IDs::satType, (float) (int) p.sat);
    set (IDs::distMix, p.distMix);
    set (IDs::cutoff, p.cutoffHz);
    set (IDs::reso, p.reso);
    set (IDs::fltEnv, p.fltEnv);
    set (IDs::attack, p.attackMs);
    set (IDs::decay, p.decayMs);
    set (IDs::sustain, p.sustain);
    set (IDs::release, p.releaseMs);
    set (IDs::width, p.width);
    set (IDs::output, p.outputDb);
    set (IDs::limiter, p.limiter ? 1.0f : 0.0f);
}

void SubKillaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void SubKillaProcessor::setStateInformation (const void* data, int size)
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

juce::AudioProcessorEditor* SubKillaProcessor::createEditor() { return new SubKillaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SubKillaProcessor(); }
