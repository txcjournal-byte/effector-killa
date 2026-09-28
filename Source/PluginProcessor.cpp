#include "PluginProcessor.h"
#include "EKBinaryData.h"
#include <numeric>
#if ! EK_TESTS
 #include "PluginEditor.h"
#endif

using namespace ek;

// ============================================================================
namespace ek::ParamIDs
{
juce::String slotParam (int slot, int param) { return "s" + juce::String (slot + 1) + "_p" + juce::String (param + 1); }
juce::String slotMix (int slot) { return "s" + juce::String (slot + 1) + "_mix"; }
juce::String slotPause (int slot) { return "s" + juce::String (slot + 1) + "_pause"; }
juce::String macro (int m)
{
    static const char* ids[] = { "villain_arc", "crash_out", "aura", "drip", "knock" };
    return ids[juce::jlimit (0, kNumMacros - 1, m)];
}
} // namespace ek::ParamIDs

juce::AudioProcessorValueTreeState::ParameterLayout EffectorKillaAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using FloatParam = juce::AudioParameterFloat;
    using BoolParam = juce::AudioParameterBool;

    // Fixed parameter set: automation survives effect changes. IDs are reserved forever.
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            "slot" + juce::String (s + 1), "Slot " + juce::String (s + 1), " | ");
        for (int p = 0; p < kNumParams; ++p)
            group->addChild (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::slotParam (s, p), 1 },
                                                           "Slot " + juce::String (s + 1) + " P" + juce::String (p + 1),
                                                           juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        group->addChild (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::slotMix (s), 1 },
                                                       "Slot " + juce::String (s + 1) + " Mix",
                                                       juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
        group->addChild (std::make_unique<BoolParam> (juce::ParameterID { ParamIDs::slotPause (s), 1 },
                                                      "Slot " + juce::String (s + 1) + " Pause", false));
        layout.add (std::move (group));
    }

    for (int m = 0; m < kNumMacros; ++m)
        layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::macro (m), 1 },
                                                  juce::String (MacroEngine::macroName (m)),
                                                  juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    auto dbAttr = juce::AudioParameterFloatAttributes().withLabel ("dB");
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::inGain, 1 }, "Antenna In",
                                              juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, dbAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::outGain, 1 }, "RF Out",
                                              juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, dbAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::blend, 1 }, "Blend",
                                              juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { ParamIDs::autoTracking, 1 }, "Auto Tracking", false));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { ParamIDs::pocketTv, 1 }, "Pocket TV", false));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { ParamIDs::offAir, 1 }, "Off Air", false));
    return layout;
}

// ============================================================================
EffectorKillaAudioProcessor::EffectorKillaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int s = 0; s < kNumSlots; ++s)
    {
        for (int p = 0; p < kNumParams; ++p)
            pSlot[(size_t) s][(size_t) p] = apvts.getRawParameterValue (ParamIDs::slotParam (s, p));
        pMix[(size_t) s] = apvts.getRawParameterValue (ParamIDs::slotMix (s));
        pPause[(size_t) s] = apvts.getRawParameterValue (ParamIDs::slotPause (s));
    }
    for (int m = 0; m < kNumMacros; ++m)
    {
        pMacro[(size_t) m] = apvts.getRawParameterValue (ParamIDs::macro (m));
        apvts.addParameterListener (ParamIDs::macro (m), this);
        uiGesture[(size_t) m] = 0;
        lastHostMacroChange[(size_t) m] = 0;
    }
    pIn = apvts.getRawParameterValue (ParamIDs::inGain);
    pOut = apvts.getRawParameterValue (ParamIDs::outGain);
    pBlend = apvts.getRawParameterValue (ParamIDs::blend);
    pAuto = apvts.getRawParameterValue (ParamIDs::autoTracking);
    pPocket = apvts.getRawParameterValue (ParamIDs::pocketTv);
    pOffAir = apvts.getRawParameterValue (ParamIDs::offAir);

    bank.loadFactory (juce::String::fromUTF8 (EKData::factory_json, EKData::factory_jsonSize));

    // start on CH 10 DRIFT – "Night Drive"
    if (bank.getFactory (9, 2) != nullptr) loadFactory (9, 2, false);
    else if (bank.getFactory (0, 0) != nullptr) loadFactory (0, 0, false);
    sides[0] = captureProgram();

    startTimerHz (20);
}

EffectorKillaAudioProcessor::~EffectorKillaAudioProcessor()
{
    stopTimer();
    for (int m = 0; m < kNumMacros; ++m)
        apvts.removeParameterListener (ParamIDs::macro (m), this);
}

// ============================================================================
void EffectorKillaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock, oversampling);
    pushExtrasToEngine();
    setLatencySamples (engine.getLatency());
}

void EffectorKillaAudioProcessor::releaseResources() {}

bool EffectorKillaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void EffectorKillaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    EngineInput in;
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& sp = in.base.slots[(size_t) s];
        for (int p = 0; p < kNumParams; ++p) sp.p[(size_t) p] = pSlot[(size_t) s][(size_t) p]->load (std::memory_order_relaxed);
        sp.mix = pMix[(size_t) s]->load (std::memory_order_relaxed);
        sp.pause = pPause[(size_t) s]->load (std::memory_order_relaxed) > 0.5f;
    }
    const auto now = juce::Time::getMillisecondCounter();
    for (int m = 0; m < kNumMacros; ++m)
    {
        in.macros[(size_t) m] = pMacro[(size_t) m]->load (std::memory_order_relaxed);
        const auto last = lastHostMacroChange[(size_t) m].load();
        in.macroModSuspended[(size_t) m] = last != 0 && now - last < 1500;
    }
    in.global.inGainDb = pIn->load();
    in.global.outGainDb = pOut->load();
    in.global.blend = pBlend->load();
    in.global.autoTracking = pAuto->load() > 0.5f;
    in.global.pocketTv = pPocket->load() > 0.5f;
    in.global.offAir = pOffAir->load() > 0.5f;

    ProcessContext ctx;
    ctx.sampleRate = getSampleRate() > 0 ? getSampleRate() : currentSampleRate;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) ctx.bpm = juce::jlimit (20.0, 999.0, *bpm);
            if (auto ppq = pos->getPpqPosition()) { ctx.ppq = *ppq; ctx.hostHasPosition = true; }
            ctx.playing = pos->getIsPlaying();
            if (auto ts = pos->getTimeSignature()) { ctx.timeSigNumerator = ts->numerator; ctx.timeSigDenominator = ts->denominator; }
        }
    }

    engine.process (buffer, in, ctx);
}

// ============================================================================
juce::AudioProcessorEditor* EffectorKillaAudioProcessor::createEditor()
{
#if EK_TESTS
    return nullptr;
#else
    return new EffectorKillaAudioProcessorEditor (*this);
#endif
}

// ============================================================================
void EffectorKillaAudioProcessor::setParamNorm (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        if (std::abs (p->getValue() - v) > 1.0e-7f)
            p->setValueNotifyingHost (v);
    }
}

ProgramState EffectorKillaAudioProcessor::captureProgram() const
{
    ProgramState p = meta;
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto& sl = p.slots[(size_t) s];
        for (int k = 0; k < kNumParams; ++k) sl.p[(size_t) k] = pSlot[(size_t) s][(size_t) k]->load();
        sl.mix = pMix[(size_t) s]->load();
        sl.pause = pPause[(size_t) s]->load() > 0.5f;
    }
    for (int m = 0; m < kNumMacros; ++m) p.macros[(size_t) m] = pMacro[(size_t) m]->load();
    return p;
}

void EffectorKillaAudioProcessor::applyProgram (const ProgramState& p, bool undoable)
{
    if (undoable) pushUndo();
    applying = true;
    meta = p;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto& sl = p.slots[(size_t) s];
        for (int k = 0; k < kNumParams; ++k) setParamNorm (ParamIDs::slotParam (s, k), sl.p[(size_t) k]);
        setParamNorm (ParamIDs::slotMix (s), sl.mix);
        setParamNorm (ParamIDs::slotPause (s), sl.pause ? 1.0f : 0.0f);
    }
    for (int m = 0; m < kNumMacros; ++m) setParamNorm (ParamIDs::macro (m), p.macros[(size_t) m]);
    applying = false;
    if (meta.channel >= 0) lastChannel = meta.channel;
    if (screening >= 0 && meta.slots[(size_t) screening].isEmpty()) screening = -1;
    pushConfigToEngine();
    pushExtrasToEngine();
    ++programEvents;
    notifyUi();
}

void EffectorKillaAudioProcessor::pushConfigToEngine() { engine.setRackConfig (meta.rackConfig (screening)); }

void EffectorKillaAudioProcessor::pushExtrasToEngine()
{
    ProgramExtras e;
    e.source = meta.source;
    e.trimDb = meta.trimDb;
    e.maps = meta.maps;
    e.mod = meta.mod;
    engine.setExtras (e);
}

// ---- presets -----------------------------------------------------------------------------
void EffectorKillaAudioProcessor::loadFactory (int channel, int index, bool undoable)
{
    if (auto* p = bank.getFactory (channel, index))
    {
        auto copy = *p;
        // write-protected slots survive preset changes only for KILL; a preset replaces everything
        applyProgram (copy, undoable);
    }
}

void EffectorKillaAudioProcessor::stepPreset (int delta)
{
    const int ch = getCurrentChannel();
    const int count = (int) bank.getChannelPresets (ch).size();
    if (count == 0) return;
    int idx = meta.channel == ch && meta.index >= 0 ? meta.index + delta : (delta > 0 ? 0 : count - 1);
    idx = (idx % count + count) % count;
    loadFactory (ch, idx);
}

void EffectorKillaAudioProcessor::stepChannel (int delta)
{
    const int n = bank.getNumChannels();
    if (n == 0) return;
    const int ch = ((getCurrentChannel() + delta) % n + n) % n;
    loadFactory (ch, 0);
}

bool EffectorKillaAudioProcessor::saveUserPreset (const juce::String& name)
{
    auto p = captureProgram();
    const bool ok = bank.saveUserPreset (p, name);
    if (ok)
    {
        meta.name = name;
        meta.factory = false;
        meta.index = -1;
        notifyUi();
    }
    return ok;
}

bool EffectorKillaAudioProcessor::loadUserPreset (const juce::File& f)
{
    if (auto p = bank.loadUserPreset (f))
    {
        applyProgram (*p);
        return true;
    }
    return false;
}

bool EffectorKillaAudioProcessor::isCurrentFavourite() const { return bank.isFavourite (PresetBank::keyFor (meta)); }

void EffectorKillaAudioProcessor::toggleFavourite()
{
    const auto key = PresetBank::keyFor (meta);
    bank.setFavourite (key, ! bank.isFavourite (key));
    notifyUi();
}

// ---- KILL / BOOTLEG ----------------------------------------------------------------------
void EffectorKillaAudioProcessor::kill()
{
    const auto cur = captureProgram();
    const uint64_t seed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
    auto np = Randomizer::kill (cur, killLevel, cur.source, seed);
    np.name = "KILL " + juce::String::toHexString ((juce::int64) (seed & 0xffffff)).toUpperCase().paddedLeft ('0', 6);
    np.trimDb = estimateProgramTrimDb (np, currentSampleRate);
    applyProgram (np, true);
    ++killEvents;
}

void EffectorKillaAudioProcessor::bootleg()
{
    const auto cur = captureProgram();
    const uint64_t seed = (uint64_t) juce::Random::getSystemRandom().nextInt64();
    auto np = Randomizer::bootleg (cur, seed);
    if (! np.name.endsWith ("*")) np.name += "*";
    np.trimDb = estimateProgramTrimDb (np, currentSampleRate);
    applyProgram (np, true);
}

// ---- history / A-B -------------------------------------------------------------------------
void EffectorKillaAudioProcessor::pushUndo()
{
    undoStack.push_back (captureProgram());
    if ((int) undoStack.size() > maxUndo) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void EffectorKillaAudioProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (captureProgram());
    auto p = undoStack.back();
    undoStack.pop_back();
    applyProgram (p, false);
}

void EffectorKillaAudioProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (captureProgram());
    auto p = redoStack.back();
    redoStack.pop_back();
    applyProgram (p, false);
}

void EffectorKillaAudioProcessor::setSide (int newSide)
{
    newSide = juce::jlimit (0, 1, newSide);
    if (newSide == side) return;
    sides[(size_t) side] = captureProgram();
    side = newSide;
    if (sides[(size_t) side].has_value()) applyProgram (*sides[(size_t) side], true);
    else sides[(size_t) side] = captureProgram();
    notifyUi();
}

// ---- slots ---------------------------------------------------------------------------------
void EffectorKillaAudioProcessor::setSlotType (int slot, EffectType t)
{
    if (slot < 0 || slot >= kNumSlots) return;
    auto p = captureProgram();
    auto& s = p.slots[(size_t) slot];
    const bool lock = s.writeProtect;
    s = SlotState();
    s.setType (t);
    s.writeProtect = lock && t != EffectType::None;
    p.maps.erase (std::remove_if (p.maps.begin(), p.maps.end(), [slot] (const MacroMapping& m) { return m.slot == slot; }), p.maps.end());
    applyProgram (p, true);
}

void EffectorKillaAudioProcessor::moveSlot (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= kNumSlots || to >= kNumSlots) return;
    auto p = captureProgram();
    std::vector<SlotState> v (p.slots.begin(), p.slots.end());
    std::vector<int> order (kNumSlots);
    std::iota (order.begin(), order.end(), 0);
    const auto moved = v[(size_t) from];
    v.erase (v.begin() + from);
    v.insert (v.begin() + to, moved);
    const int movedIdx = order[(size_t) from];
    order.erase (order.begin() + from);
    order.insert (order.begin() + to, movedIdx);
    for (int i = 0; i < kNumSlots; ++i) p.slots[(size_t) i] = v[(size_t) i];
    // macro mappings follow their slot
    for (auto& m : p.maps)
        for (int i = 0; i < kNumSlots; ++i)
            if (order[(size_t) i] == m.slot) { m.slot = i; break; }
    for (auto* t : { &p.mod.targetX, &p.mod.targetY })
        if (ModTarget::isSlotParam (*t))
            for (int i = 0; i < kNumSlots; ++i)
                if (order[(size_t) i] == ModTarget::slotOf (*t)) { *t = ModTarget::slotParam (i, ModTarget::paramOf (*t)); break; }
    screening = -1;
    if (selectedSlot == from) selectedSlot = to;
    applyProgram (p, true);
}

void EffectorKillaAudioProcessor::setSlotMs (int slot, MSMode m)
{
    auto p = captureProgram();
    p.slots[(size_t) slot].ms = m;
    applyProgram (p, true);
}

void EffectorKillaAudioProcessor::setWriteProtect (int slot, bool on)
{
    meta.slots[(size_t) slot].writeProtect = on;
    notifyUi();
}

void EffectorKillaAudioProcessor::setScreening (int slot)
{
    screening = (slot >= 0 && slot < kNumSlots && ! meta.slots[(size_t) slot].isEmpty()) ? slot : -1;
    pushConfigToEngine();
    notifyUi();
}

void EffectorKillaAudioProcessor::copySlot (int slot) { slotClipboard = captureProgram().slots[(size_t) slot]; }

void EffectorKillaAudioProcessor::pasteSlot (int slot)
{
    if (! slotClipboard) return;
    auto p = captureProgram();
    p.slots[(size_t) slot] = *slotClipboard;
    p.slots[(size_t) slot].writeProtect = false;
    applyProgram (p, true);
}

bool EffectorKillaAudioProcessor::saveSlotPreset (int slot, const juce::String& name)
{
    return bank.saveSlotPreset (captureProgram().slots[(size_t) slot], name);
}

bool EffectorKillaAudioProcessor::loadSlotPreset (int slot, const juce::File& f)
{
    if (auto s = bank.loadSlotPreset (f))
    {
        auto p = captureProgram();
        p.slots[(size_t) slot] = *s;
        applyProgram (p, true);
        return true;
    }
    return false;
}

void EffectorKillaAudioProcessor::setSlotParam (int slot, int param, float v)
{
    if (param < kNumParams) setParamNorm (ParamIDs::slotParam (slot, param), v);
    else if (param == 8) setParamNorm (ParamIDs::slotMix (slot), v);
    else setParamNorm (ParamIDs::slotPause (slot), v);
}

float EffectorKillaAudioProcessor::getSlotParam (int slot, int param) const
{
    if (param < kNumParams) return pSlot[(size_t) slot][(size_t) param]->load();
    if (param == 8) return pMix[(size_t) slot]->load();
    return pPause[(size_t) slot]->load();
}

// ---- source / modulation / settings ------------------------------------------------------
void EffectorKillaAudioProcessor::setSource (Source s)
{
    meta.source = s;
    pushExtrasToEngine();
    notifyUi();
}

void EffectorKillaAudioProcessor::setModState (const ModState& m, bool undoable)
{
    if (undoable) pushUndo();
    meta.mod = m;
    meta.mod.resamplePath();
    for (int k = 0; k < kNumMacros; ++k) lastHostMacroChange[(size_t) k] = 0; // user re-engaged AV
    pushExtrasToEngine();
    notifyUi();
}

void EffectorKillaAudioProcessor::setOversampling (int osFactorLog2)
{
    osFactorLog2 = juce::jlimit (0, 2, osFactorLog2);
    if (osFactorLog2 == oversampling) return;
    oversampling = osFactorLog2;
    engine.setOversampling (oversampling);
    setLatencySamples (engine.getLatency());
    notifyUi();
}

void EffectorKillaAudioProcessor::beginUiGesture (int m) { if (m >= 0 && m < kNumMacros) ++uiGesture[(size_t) m]; }
void EffectorKillaAudioProcessor::endUiGesture (int m) { if (m >= 0 && m < kNumMacros) uiGesture[(size_t) m] = std::max (0, uiGesture[(size_t) m] - 1); }

void EffectorKillaAudioProcessor::parameterChanged (const juce::String& id, float)
{
    if (applying) return;
    for (int m = 0; m < kNumMacros; ++m)
        if (id == ParamIDs::macro (m) && uiGesture[(size_t) m].load() == 0)
            lastHostMacroChange[(size_t) m] = juce::jmax ((juce::uint32) 1, juce::Time::getMillisecondCounter());
}

void EffectorKillaAudioProcessor::timerCallback() { engine.collectGarbage(); }

// ---- state -------------------------------------------------------------------------------
void EffectorKillaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("EffectorKilla");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("program", captureProgram().toJson(), nullptr);
    auto sideA = side == 0 ? captureProgram() : sides[0].value_or (captureProgram());
    auto sideB = side == 1 ? captureProgram() : sides[1].value_or (captureProgram());
    root.setProperty ("sideA", sideA.toJson(), nullptr);
    root.setProperty ("sideB", sideB.toJson(), nullptr);
    root.setProperty ("side", side, nullptr);
    root.setProperty ("oversampling", oversampling, nullptr);
    root.setProperty ("premium", premiumCable, nullptr);
    root.setProperty ("killLevel", (int) killLevel, nullptr);
    root.setProperty ("screening", screening, nullptr);
    root.setProperty ("lastChannel", lastChannel, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void EffectorKillaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("EffectorKilla")) return;

    const auto params = root.getChildWithName (apvts.state.getType());
    applying = true;
    if (params.isValid()) apvts.replaceState (params);
    applying = false;

    meta = ProgramState::fromJson (root.getProperty ("program").toString());
    sides[0] = ProgramState::fromJson (root.getProperty ("sideA").toString());
    sides[1] = ProgramState::fromJson (root.getProperty ("sideB").toString());
    side = juce::jlimit (0, 1, (int) root.getProperty ("side", 0));
    premiumCable = (bool) root.getProperty ("premium", true);
    killLevel = (KillLevel) juce::jlimit (0, 2, (int) root.getProperty ("killLevel", 1));
    screening = juce::jlimit (-1, kNumSlots - 1, (int) root.getProperty ("screening", -1));
    lastChannel = juce::jlimit (0, 11, (int) root.getProperty ("lastChannel", 0));
    const int os = juce::jlimit (0, 2, (int) root.getProperty ("oversampling", 1));
    if (os != oversampling) setOversampling (os);

    undoStack.clear();
    redoStack.clear();
    pushConfigToEngine();
    pushExtrasToEngine();
    ++programEvents;
    notifyUi();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EffectorKillaAudioProcessor(); }
