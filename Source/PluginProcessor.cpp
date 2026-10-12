#include "PluginProcessor.h"
#include "EKBinaryData.h"
#include <numeric>
#include <mutex>
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

namespace
{
// Process-wide cache of the factory preset loudness trims (an offline render each, ~20 ms). The result
// only depends on the preset, so every instance in the process shares it. Plain data only (no JUCE
// objects), so it is safe to destroy at library unload.
struct FactoryTrimCache
{
    std::mutex lock;
    std::map<int, float> trims;
};
FactoryTrimCache& factoryTrimCache()
{
    static FactoryTrimCache c;
    return c;
}

// Slot parameters keep fixed IDs (automation never jumps to another parameter), but their
// names and value texts follow the loaded effect: "S1: Cassette Plug: Drive" = "4.2 dB".
juce::String slotPrefix (const ek::HostInfo* info, int slot)
{
    const auto t = info != nullptr ? (ek::EffectType) info->type[(size_t) slot].load() : ek::EffectType::None;
    juce::String s = "S" + juce::String (slot + 1) + ": ";
    if (t != ek::EffectType::None) s << ek::effectInfo (t).name << ": ";
    return s;
}

class SlotFloatParam : public juce::AudioParameterFloat
{
public:
    SlotFloatParam (std::shared_ptr<ek::HostInfo> i, int s, int p, const juce::String& id, float def)
        : juce::AudioParameterFloat (juce::ParameterID { id, 1 },
                                     "Slot " + juce::String (s + 1) + (p < ek::kNumParams ? " P" + juce::String (p + 1) : juce::String (" Mix")),
                                     juce::NormalisableRange<float> (0.0f, 1.0f), def),
          info (std::move (i)), slot (s), param (p) {}

    juce::String getName (int maxLen) const override { return fullName().substring (0, maxLen); }

private:
    const ek::ParamSpec* spec() const
    {
        if (info == nullptr || param >= ek::kNumParams) return nullptr;
        const auto t = (ek::EffectType) info->type[(size_t) slot].load();
        if (t == ek::EffectType::None) return nullptr;
        return &ek::effectInfo (t).params[(size_t) param];
    }
    juce::String fullName() const
    {
        if (param >= ek::kNumParams) return slotPrefix (info.get(), slot) + "Mix";
        if (auto* sp = spec()) return slotPrefix (info.get(), slot) + (sp->used ? sp->name : "P" + juce::String (param + 1) + " (unused)");
        return slotPrefix (info.get(), slot) + "P" + juce::String (param + 1);
    }
    juce::String getText (float v, int maxLen) const override
    {
        if (param >= ek::kNumParams) return (juce::String (juce::roundToInt (v * 100.0f)) + " %").substring (0, maxLen);
        if (auto* sp = spec(); sp != nullptr && sp->used) return sp->format (sp->toReal (v)).substring (0, maxLen);
        return juce::String (v, 3).substring (0, maxLen);
    }
    std::shared_ptr<ek::HostInfo> info;
    int slot, param;
};

class SlotPowerParam : public juce::AudioParameterBool
{
public:
    SlotPowerParam (std::shared_ptr<ek::HostInfo> i, int s, const juce::String& id)
        : juce::AudioParameterBool (juce::ParameterID { id, 1 }, "Slot " + juce::String (s + 1) + " Pause", false),
          info (std::move (i)), slot (s) {}
    juce::String getName (int maxLen) const override { return (slotPrefix (info.get(), slot) + "Bypass").substring (0, maxLen); }
private:
    std::shared_ptr<ek::HostInfo> info;
    int slot;
};
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout EffectorKillaAudioProcessor::createLayout (std::shared_ptr<ek::HostInfo> info)
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
            group->addChild (std::make_unique<SlotFloatParam> (info, s, p, ParamIDs::slotParam (s, p), 0.0f));
        group->addChild (std::make_unique<SlotFloatParam> (info, s, kNumParams, ParamIDs::slotMix (s), 1.0f));
        group->addChild (std::make_unique<SlotPowerParam> (info, s, ParamIDs::slotPause (s)));
        layout.add (std::move (group));
    }

    for (int m = 0; m < kNumMacros; ++m)
        layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::macro (m), 1 },
                                                  juce::String (MacroEngine::macroName (m)),
                                                  juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    auto dbAttr = juce::AudioParameterFloatAttributes().withLabel ("dB");
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::inGain, 1 }, "Input",
                                              juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, dbAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::outGain, 1 }, "Output",
                                              juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, dbAttr));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::blend, 1 }, "Mix",
                                              juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { ParamIDs::autoTracking, 1 }, "Auto Gain", false));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { ParamIDs::pocketTv, 1 }, "Phone Check", false));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { ParamIDs::offAir, 1 }, "Compare", false));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::recycle, 1 }, "Recycle",
                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                              juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                  [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::tempoMode, 1 }, "Tempo Mode",
                                                              juce::StringArray { "Host", "Manual" }, 0));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { ParamIDs::tempoBpm, 1 }, "Tempo",
                                              juce::NormalisableRange<float> (40.0f, 300.0f, 0.01f), 120.0f,
                                              juce::AudioParameterFloatAttributes().withLabel ("BPM")));
    return layout;
}

// ============================================================================
EffectorKillaAudioProcessor::EffectorKillaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout (hostInfo))
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
    pRecycle = apvts.getRawParameterValue (ParamIDs::recycle);
    pTempoMode = apvts.getRawParameterValue (ParamIDs::tempoMode);
    pTempoBpm = apvts.getRawParameterValue (ParamIDs::tempoBpm);

    bank.loadFactory (juce::String::fromUTF8 (EKData::factory_json, EKData::factory_jsonSize));
    bank.loadTapes (juce::String::fromUTF8 (EKData::tapes_json, EKData::tapes_jsonSize));

    // start on CH 10 DRIFT – "Night Drive". Its loudness trim (an offline render) is resolved lazily:
    // a project load replaces this program right away, so the ctor stays fast (resolvePendingTrim()).
    if (bank.getFactory (9, 2) != nullptr) loadFactoryDeferred (9, 2);
    else if (bank.getFactory (0, 0) != nullptr) loadFactoryDeferred (0, 0);
    sides[0] = captureProgramUnresolved();
    pendingTrimSide0 = pendingTrimKey >= 0;

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
    resolvePendingTrim();   // the start-up preset's trim before any audio
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
    in.global.recycle = pRecycle->load();
    in.base.split.freqHz = splitFreq.load (std::memory_order_relaxed);
    in.base.split.balance = splitBalance.load (std::memory_order_relaxed);
    in.base.split.levelADb = splitLevelA.load (std::memory_order_relaxed);
    in.base.split.levelBDb = splitLevelB.load (std::memory_order_relaxed);

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

    // MANUAL tempo (or TAP): own BPM; follows the host transport position when there is one,
    // otherwise a free-running position keeps synced LFOs and delays moving.
    if (pTempoMode->load() > 0.5f)
    {
        ctx.bpm = juce::jlimit (40.0, 300.0, (double) pTempoBpm->load());
        if (! ctx.hostHasPosition || ! ctx.playing)
        {
            ctx.ppq = freePpq;
            freePpq += (double) buffer.getNumSamples() * ctx.beatsPerSample();
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
    resolvePendingTrim();
    return captureProgramUnresolved();
}

ProgramState EffectorKillaAudioProcessor::captureProgramUnresolved() const
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
    pendingTrimKey = -1;    // the program is replaced: a deferred trim is no longer needed
    pendingTrimSide0 = false;
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
    if (solo >= 0 && meta.slots[(size_t) solo].isEmpty()) solo = -1;
    updateHostNames();
    pushSplitParams();
    pushConfigToEngine();
    pushExtrasToEngine();
    ++programEvents;
    notifyUi();
}

void EffectorKillaAudioProcessor::updateHostNames()
{
    bool changed = false;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const int t = (int) meta.slots[(size_t) s].type;
        if (hostInfo->type[(size_t) s].exchange (t) != t) changed = true;
    }
    if (changed)
        updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
}

void EffectorKillaAudioProcessor::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0) taps.clear(); // a pause starts a new tap run
    taps.push_back (now);
    if (taps.size() > 6) taps.erase (taps.begin());
    if (taps.size() < 2) return;
    const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
    const double bpm = juce::jlimit (40.0, 300.0, 60000.0 / avg);
    if (auto* m = apvts.getParameter (ParamIDs::tempoMode)) m->setValueNotifyingHost (1.0f);
    if (auto* b = apvts.getParameter (ParamIDs::tempoBpm)) b->setValueNotifyingHost (b->convertTo0to1 ((float) bpm));
}

void EffectorKillaAudioProcessor::pushConfigToEngine() { engine.setRackConfig (meta.rackConfig (screening, solo)); }

void EffectorKillaAudioProcessor::pushSplitParams()
{
    meta.split.params = meta.split.params.sanitised();
    splitFreq.store (meta.split.params.freqHz);
    splitBalance.store (meta.split.params.balance);
    splitLevelA.store (meta.split.params.levelADb);
    splitLevelB.store (meta.split.params.levelBDb);
}

void EffectorKillaAudioProcessor::pushExtrasToEngine()
{
    resolvePendingTrim();
    ProgramExtras e;
    e.source = meta.source;
    e.trimDb = meta.trimDb;
    e.maps = meta.maps;
    e.mod = meta.mod;
    e.modSources = meta.modSources;
    // The rack UI has no AV screen any more: never run hidden XY / scribble / screensaver modulation.
    e.mod.mode = AvMode::Remote;
    e.mod.path.clear();
    engine.setExtras (e);
}

// ---- presets -----------------------------------------------------------------------------
void EffectorKillaAudioProcessor::loadFactory (int channel, int index, bool undoable)
{
    if (auto* p = bank.getFactory (channel, index))
    {
        auto copy = *p;
        copy.trimDb = factoryTrimDb (channel, index);
        applyProgram (copy, undoable);
    }
}

void EffectorKillaAudioProcessor::loadFactoryDeferred (int channel, int index)
{
    const auto* p = bank.getFactory (channel, index);
    if (p == nullptr) return;
    auto copy = *p;
    const auto cached = cachedFactoryTrimDb (channel, index);
    copy.trimDb = cached.value_or (0.0f);
    applyProgram (copy, false);
    if (! cached.has_value()) pendingTrimKey = channel * 100 + index;
}

void EffectorKillaAudioProcessor::resolvePendingTrim() const
{
    if (pendingTrimKey < 0) return;
    // message thread only; `meta` / `sides` are the processor's own (non-const) members
    auto& self = const_cast<EffectorKillaAudioProcessor&> (*this);
    const int key = pendingTrimKey;
    self.pendingTrimKey = -1;
    const float trim = self.factoryTrimDb (key / 100, key % 100);
    self.meta.trimDb = trim;
    if (pendingTrimSide0 && self.sides[0].has_value()) self.sides[0]->trimDb = trim;
    self.pendingTrimSide0 = false;
    self.pushExtrasToEngine();
}

std::optional<float> EffectorKillaAudioProcessor::cachedFactoryTrimDb (int channel, int index) const
{
    const auto* p = bank.getFactory (channel, index);
    if (p == nullptr || p->source == Source::Bus) return 0.0f;
    auto& cache = factoryTrimCache();
    const std::lock_guard<std::mutex> sl (cache.lock);
    if (auto it = cache.trims.find (channel * 100 + index); it != cache.trims.end()) return it->second;
    return std::nullopt;
}

float EffectorKillaAudioProcessor::factoryTrimDb (int channel, int index)
{
    // Factory presets are loudness matched to the input (like KILL) so switching presets compares sound,
    // not level. BUS / master presets keep their designed loudness (making things loud is their job).
    // The estimate only depends on the (read-only) factory preset: computed once per process.
    if (auto cached = cachedFactoryTrimDb (channel, index)) return *cached;
    const auto* p = bank.getFactory (channel, index);
    if (p == nullptr) return 0.0f;
    const float trim = juce::jlimit (-12.0f, 6.0f, estimateProgramTrimDb (*p, 44100.0));
    auto& cache = factoryTrimCache();
    const std::lock_guard<std::mutex> sl (cache.lock);
    cache.trims[channel * 100 + index] = trim;
    return trim;
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
    // MOD / MACRO LEARN survive KILL only on units that kept their effect type
    for (int sl = 0; sl < kNumSlots; ++sl)
    {
        if (np.slots[(size_t) sl].type == cur.slots[(size_t) sl].type && ! np.slots[(size_t) sl].isEmpty())
        {
            for (auto& m : cur.maps)
                if (m.learned && m.slot == sl) np.maps.push_back (m);
        }
        else np.modSources.removeSlot (sl);
    }
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
    const int branch = s.branch;
    s = SlotState();
    s.setType (t);
    s.writeProtect = lock && t != EffectType::None;
    s.branch = branch;
    p.maps.erase (std::remove_if (p.maps.begin(), p.maps.end(), [slot] (const MacroMapping& m) { return m.slot == slot; }), p.maps.end());
    p.modSources.removeSlot (slot);
    applyProgram (p, true);
}

void EffectorKillaAudioProcessor::playTape (int slot, EffectType t, int programme)
{
    const auto& list = bank.getTapes (t);
    if (slot < 0 || slot >= kNumSlots || programme < 0 || programme >= (int) list.size()) return;
    auto p = captureProgram();
    auto& s = p.slots[(size_t) slot];
    const auto ms = s.ms;
    const bool lock = s.writeProtect;
    const bool sameType = s.type == t;
    const int branch = s.branch;
    s = list[(size_t) programme].slot;
    s.ms = sameType ? ms : MSMode::Stereo;
    s.writeProtect = lock;
    s.branch = branch;
    if (! sameType)
    {
        p.maps.erase (std::remove_if (p.maps.begin(), p.maps.end(), [slot] (const MacroMapping& m) { return m.slot == slot; }), p.maps.end());
        p.modSources.removeSlot (slot);
    }
    applyProgram (p, true);
}

int EffectorKillaAudioProcessor::findTape (int slot) const
{
    if (slot < 0 || slot >= kNumSlots) return -1;
    const auto s = getSlotState (slot);
    const auto& list = bank.getTapes (s.type);
    for (int i = 0; i < (int) list.size(); ++i)
    {
        float d = std::abs (list[(size_t) i].slot.mix - s.mix);
        for (int k = 0; k < kNumParams; ++k) d += std::abs (list[(size_t) i].slot.p[(size_t) k] - s.p[(size_t) k]);
        if (d < 0.01f) return i;
    }
    return -1;
}

// Reorder slots inside a program; macro mappings and modulation targets follow their slot.
static void reorderSlots (ek::ProgramState& p, int from, int to)
{
    using namespace ek;
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
    // SPLIT: a unit crossing the split point changes zone, the split stays between the same neighbours
    if (p.split.isActive())
    {
        if (from < p.split.at && to >= p.split.at) --p.split.at;
        else if (from >= p.split.at && to < p.split.at) ++p.split.at;
    }
    // macro mappings follow their slot
    for (auto& m : p.maps)
        for (int i = 0; i < kNumSlots; ++i)
            if (order[(size_t) i] == m.slot) { m.slot = i; break; }
    std::array<int, kNumSlots> newIndexOfOld {};
    for (int i = 0; i < kNumSlots; ++i) newIndexOfOld[(size_t) order[(size_t) i]] = i;
    p.modSources.remapSlots (newIndexOfOld);
    for (auto* t : { &p.mod.targetX, &p.mod.targetY })
        if (ModTarget::isSlotParam (*t))
            for (int i = 0; i < kNumSlots; ++i)
                if (order[(size_t) i] == ModTarget::slotOf (*t)) { *t = ModTarget::slotParam (i, ModTarget::paramOf (*t)); break; }
}

void EffectorKillaAudioProcessor::moveSlot (int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= kNumSlots || to >= kNumSlots) return;
    auto p = captureProgram();
    reorderSlots (p, from, to);
    screening = -1;
    solo = -1;
    if (selectedSlot == from) selectedSlot = to;
    applyProgram (p, true);
}

int EffectorKillaAudioProcessor::insertEffect (int at, EffectType t) { return insertEffectImpl (at, t, -1); }

int EffectorKillaAudioProcessor::insertEffectImpl (int at, EffectType t, int branch)
{
    if (at < 0 || at > kNumSlots || t == EffectType::None) return -1; // at == kNumSlots: append at the end
    auto p = captureProgram();
    // free slot: the first empty one at/after the drop point, otherwise the last empty one before it
    int free = -1;
    for (int i = at; i < kNumSlots && free < 0; ++i) if (p.slots[(size_t) i].isEmpty()) free = i;
    for (int i = at - 1; i >= 0 && free < 0; --i) if (p.slots[(size_t) i].isEmpty()) free = i;
    if (free < 0) return -1; // rack full
    auto& s = p.slots[(size_t) free];
    s = SlotState();
    s.setType (t);
    const auto& tapes = bank.getTapes (t);
    if (! tapes.empty()) { s = tapes.front().slot; s.ms = MSMode::Stereo; s.writeProtect = false; }
    p.maps.erase (std::remove_if (p.maps.begin(), p.maps.end(), [free] (const MacroMapping& m) { return m.slot == free; }), p.maps.end());
    p.modSources.removeSlot (free);
    // a free slot before the drop point: moving it forward shifts the units in between back by one
    const int dest = free < at ? at - 1 : at;
    if (free != dest) reorderSlots (p, free, dest);
    if (branch >= 0 && p.split.isActive())
    {
        if (dest < p.split.at) p.split.at = dest; // the new unit is the last one in front: move the split above it
        p.slots[(size_t) dest].branch = branch;
    }
    screening = -1;
    solo = -1;
    selectedSlot = dest;
    applyProgram (p, true);
    return dest;
}

void EffectorKillaAudioProcessor::removeEffect (int slot)
{
    if (slot < 0 || slot >= kNumSlots || meta.slots[(size_t) slot].writeProtect) return;
    auto p = captureProgram();
    p.slots[(size_t) slot] = SlotState();
    p.maps.erase (std::remove_if (p.maps.begin(), p.maps.end(), [slot] (const MacroMapping& m) { return m.slot == slot; }), p.maps.end());
    p.modSources.removeSlot (slot);
    reorderSlots (p, slot, kNumSlots - 1);  // close the gap, empty slot goes to the end
    screening = -1;
    solo = -1;
    selectedSlot = -1;
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

void EffectorKillaAudioProcessor::setSolo (int slot)
{
    solo = (slot >= 0 && slot < kNumSlots && ! meta.slots[(size_t) slot].isEmpty()) ? slot : -1;
    pushConfigToEngine();
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
    if (p.slots[(size_t) slot].type != slotClipboard->type) p.modSources.removeSlot (slot);
    const int branch = p.slots[(size_t) slot].branch;
    p.slots[(size_t) slot] = *slotClipboard;
    p.slots[(size_t) slot].writeProtect = false;
    p.slots[(size_t) slot].branch = branch;
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
        if (p.slots[(size_t) slot].type != s->type) p.modSources.removeSlot (slot);
        const int branch = p.slots[(size_t) slot].branch;
        p.slots[(size_t) slot] = *s;
        p.slots[(size_t) slot].branch = branch;
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

SlotState EffectorKillaAudioProcessor::getSlotState (int slot) const
{
    auto s = meta.slots[(size_t) slot];
    for (int k = 0; k < kNumParams; ++k) s.p[(size_t) k] = pSlot[(size_t) slot][(size_t) k]->load();
    s.mix = pMix[(size_t) slot]->load();
    s.pause = pPause[(size_t) slot]->load() > 0.5f;
    return s;
}

float EffectorKillaAudioProcessor::getSlotParam (int slot, int param) const
{
    if (param < kNumParams) return pSlot[(size_t) slot][(size_t) param]->load();
    if (param == 8) return pMix[(size_t) slot]->load();
    return pPause[(size_t) slot]->load();
}

// ---- SPLIT / MERGE ---------------------------------------------------------------------------
void EffectorKillaAudioProcessor::insertSplit (int at)
{
    at = juce::jlimit (0, kNumSlots, at);
    if (meta.split.isActive() && meta.split.at == at) return;
    auto p = captureProgram();
    if (! p.split.isActive())
        for (auto& s : p.slots) s.branch = 0; // every unit behind a new split starts in branch A
    p.split.at = at;
    screening = -1;
    solo = -1;
    applyProgram (p, true);
}

void EffectorKillaAudioProcessor::removeSplit()
{
    if (! meta.split.isActive()) return;
    auto p = captureProgram();
    p.split = SplitState(); // inactive; the knob values return to their defaults
    for (auto& s : p.slots) s.branch = 0;
    screening = -1;
    solo = -1;
    applyProgram (p, true);
}

void EffectorKillaAudioProcessor::setSplitMode (SplitMode m)
{
    if (! meta.split.isActive() || meta.split.mode == m || (int) m < 0 || m >= SplitMode::Count) return;
    auto p = captureProgram();
    p.split.mode = m;
    applyProgram (p, true);
}

void EffectorKillaAudioProcessor::setSplitFreq (float hz)
{
    meta.split.params.freqHz = hz;
    pushSplitParams();
    notifyUi();
}

void EffectorKillaAudioProcessor::setSplitBalance (float balance)
{
    meta.split.params.balance = balance;
    pushSplitParams();
    notifyUi();
}

void EffectorKillaAudioProcessor::setSplitLevel (int branch, float db)
{
    (branch == 0 ? meta.split.params.levelADb : meta.split.params.levelBDb) = db;
    pushSplitParams();
    notifyUi();
}

int EffectorKillaAudioProcessor::getSlotBranch (int slot) const noexcept
{
    if (! meta.split.isActive() || slot < 0 || slot >= kNumSlots || slot < meta.split.at) return -1;
    return meta.slots[(size_t) slot].branch != 0 ? 1 : 0;
}

std::vector<int> EffectorKillaAudioProcessor::getBranchSlots (int branch) const
{
    std::vector<int> out;
    for (int i = 0; i < kNumSlots; ++i)
        if (! meta.slots[(size_t) i].isEmpty() && getSlotBranch (i) == branch)
            out.push_back (i);
    return out;
}

void EffectorKillaAudioProcessor::setSlotBranch (int slot, int branch)
{
    if (! meta.split.isActive() || slot < 0 || slot >= kNumSlots || meta.slots[(size_t) slot].isEmpty()) return;
    branch = branch != 0 ? 1 : 0;
    if (getSlotBranch (slot) == branch) return;
    auto p = captureProgram();
    int dest = slot;
    if (slot < p.split.at)
    {
        // a unit in front of the split moves behind it, after the last unit of the rack
        int lastUnit = -1;
        for (int i = 0; i < kNumSlots; ++i) if (! p.slots[(size_t) i].isEmpty()) lastUnit = i;
        dest = std::max (lastUnit, p.split.at - 1);
        if (dest != slot) reorderSlots (p, slot, dest);
        if (dest < p.split.at) p.split.at = dest;
    }
    p.slots[(size_t) dest].branch = branch;
    screening = -1;
    solo = -1;
    if (selectedSlot == slot) selectedSlot = dest;
    applyProgram (p, true);
}

int EffectorKillaAudioProcessor::insertEffectInBranch (int branch, EffectType t)
{
    if (! meta.split.isActive()) return -1;
    int lastUnit = -1;
    for (int i = 0; i < kNumSlots; ++i) if (! meta.slots[(size_t) i].isEmpty()) lastUnit = i;
    return insertEffectImpl (std::max (lastUnit + 1, meta.split.at), t, branch != 0 ? 1 : 0);
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

// ---- MOD SOURCES ---------------------------------------------------------------------------
void EffectorKillaAudioProcessor::modChanged()
{
    pushExtrasToEngine();
    notifyUi();
}

int EffectorKillaAudioProcessor::addModAssignment (int source, int slot, int param, float depth)
{
    ModAssignment a { source, slot, param, depth };
    if (slot < 0 || ! a.isValid()) return -1;
    auto copy = meta.modSources;
    const int idx = copy.add (a);
    if (idx < 0) return -1;
    pushUndo();
    meta.modSources = copy;
    modChanged();
    return idx;
}

int EffectorKillaAudioProcessor::addMacroModAssignment (int source, int macro, float depth)
{
    ModAssignment a { source, -1, macro, depth };
    if (! a.isValid()) return -1;
    auto copy = meta.modSources;
    const int idx = copy.add (a);
    if (idx < 0) return -1;
    pushUndo();
    meta.modSources = copy;
    modChanged();
    return idx;
}

void EffectorKillaAudioProcessor::removeModAssignment (int index)
{
    if (index < 0 || index >= meta.modSources.numAssign) return;
    pushUndo();
    meta.modSources.remove (index);
    modChanged();
}

void EffectorKillaAudioProcessor::removeModAssignmentsFor (int slot, int param)
{
    if (findModAssignments (slot, param).empty()) return;
    pushUndo();
    meta.modSources.removeTarget (slot, param);
    modChanged();
}

void EffectorKillaAudioProcessor::clearModAssignments()
{
    if (! meta.modSources.hasAssignments()) return;
    pushUndo();
    while (meta.modSources.numAssign > 0) meta.modSources.remove (meta.modSources.numAssign - 1);
    modChanged();
}

void EffectorKillaAudioProcessor::setModDepth (int index, float depth)
{
    if (index < 0 || index >= meta.modSources.numAssign || ! std::isfinite (depth)) return;
    auto& a = meta.modSources.assign[(size_t) index];
    depth = juce::jlimit (-1.0f, 1.0f, depth);
    if (a.depth == depth) return;
    a.depth = depth;
    modChanged();
}

std::vector<ModAssignment> EffectorKillaAudioProcessor::getModAssignments() const
{
    const auto& m = meta.modSources;
    return { m.assign.begin(), m.assign.begin() + m.numAssign };
}

std::vector<int> EffectorKillaAudioProcessor::findModAssignments (int slot, int param) const
{
    std::vector<int> out;
    for (int i = 0; i < meta.modSources.numAssign; ++i)
        if (meta.modSources.assign[(size_t) i].slot == slot && meta.modSources.assign[(size_t) i].param == param)
            out.push_back (i);
    return out;
}

void EffectorKillaAudioProcessor::setModSources (const ModSourceState& s, bool undoable)
{
    if (s == meta.modSources) return;
    if (undoable) pushUndo();
    meta.modSources = ModSourceState::fromVar (s.toVar()); // validated copy
    modChanged();
}

void EffectorKillaAudioProcessor::setLfo (int lfo, const LfoSettings& s)
{
    if (lfo < 0 || lfo > 1) return;
    auto copy = meta.modSources;
    copy.lfo[(size_t) lfo] = s;
    setModSources (copy);
}

void EffectorKillaAudioProcessor::setEnvFollower (const EnvSettings& s)
{
    auto copy = meta.modSources;
    copy.env = s;
    setModSources (copy);
}

void EffectorKillaAudioProcessor::setStepSeq (const SeqSettings& s)
{
    auto copy = meta.modSources;
    copy.seq = s;
    setModSources (copy);
}

void EffectorKillaAudioProcessor::setSeqStep (int step, float v)
{
    if (step < 0 || step >= kSeqSteps) return;
    auto copy = meta.modSources;
    copy.seq.steps[(size_t) step] = v;
    setModSources (copy);
}

float EffectorKillaAudioProcessor::getModSourceValue (int source) const noexcept
{
    if (source < 0 || source >= kNumModSources) return 0.0f;
    return engine.modMeters.value[(size_t) source].load (std::memory_order_relaxed);
}

float EffectorKillaAudioProcessor::getLfoPhase (int lfo) const noexcept
{
    return engine.modMeters.lfoPhase[(size_t) juce::jlimit (0, 1, lfo)].load (std::memory_order_relaxed);
}

float EffectorKillaAudioProcessor::getModOffset (int slot, int param) const noexcept
{
    if (slot < 0) return param >= 0 && param < kNumMacros ? engine.modMeters.macroOffset[(size_t) param].load (std::memory_order_relaxed) : 0.0f;
    if (slot >= kNumSlots || param < 0 || param >= kNumModSlotTargets) return 0.0f;
    return engine.modMeters.slotOffset[(size_t) slot][(size_t) param].load (std::memory_order_relaxed);
}

float EffectorKillaAudioProcessor::getEffectiveSlotParam (int slot, int param) const noexcept
{
    if (slot < 0 || slot >= kNumSlots) return 0.0f;
    if (param == kNumParams) return engine.effectiveMix[(size_t) slot].load (std::memory_order_relaxed);
    if (param < 0 || param > kNumParams) return 0.0f;
    return engine.effectiveParams[(size_t) slot][(size_t) param].load (std::memory_order_relaxed);
}

// ---- MACRO LEARN ---------------------------------------------------------------------------
bool EffectorKillaAudioProcessor::assignMacro (int macro, int slot, int param, float minNorm, float maxNorm)
{
    if (macro < 0 || macro >= kNumMacros || slot < 0 || slot >= kNumSlots || param < 0 || param > kNumParams) return false;
    if (meta.slots[(size_t) slot].isEmpty() || ! std::isfinite (minNorm) || ! std::isfinite (maxNorm)) return false;
    pushUndo();
    auto& maps = meta.maps;
    maps.erase (std::remove_if (maps.begin(), maps.end(), [&] (const MacroMapping& m)
                { return m.macro == macro && m.slot == slot && m.param == param; }), maps.end());
    MacroMapping mm;
    mm.macro = macro; mm.slot = slot; mm.param = param;
    mm.atZero = clamp01 (minNorm);
    mm.atOne = clamp01 (maxNorm);
    mm.learned = true;
    maps.push_back (mm);
    modChanged();
    return true;
}

void EffectorKillaAudioProcessor::removeMacroAssignment (int macro, int slot, int param)
{
    auto pred = [&] (const MacroMapping& m) { return m.macro == macro && m.slot == slot && m.param == param; };
    if (std::none_of (meta.maps.begin(), meta.maps.end(), pred)) return;
    pushUndo();
    meta.maps.erase (std::remove_if (meta.maps.begin(), meta.maps.end(), pred), meta.maps.end());
    modChanged();
}

std::vector<MacroMapping> EffectorKillaAudioProcessor::getMacroAssignments() const { return meta.maps; }

std::vector<MacroMapping> EffectorKillaAudioProcessor::getMacroAssignmentsFor (int slot, int param) const
{
    std::vector<MacroMapping> out;
    for (auto& m : meta.maps)
        if (m.slot == slot && m.param == param) out.push_back (m);
    return out;
}

// ---- state -------------------------------------------------------------------------------
void EffectorKillaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("EffectorKilla"); // state tag kept for compatibility
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("program", captureProgram().toJson(), nullptr);
    auto sideA = side == 0 ? captureProgram() : sides[0].value_or (captureProgram());
    auto sideB = side == 1 ? captureProgram() : sides[1].value_or (captureProgram());
    root.setProperty ("sideA", sideA.toJson(), nullptr);
    root.setProperty ("sideB", sideB.toJson(), nullptr);
    root.setProperty ("side", side, nullptr);
    root.setProperty ("oversampling", oversampling, nullptr);
    root.setProperty ("premium", premiumCable, nullptr);
    root.setProperty ("uiScale", uiScale, nullptr);
    root.setProperty ("showGear", showGear, nullptr);
    root.setProperty ("softwareTv", softwareTv, nullptr);
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

    pendingTrimKey = -1;    // the project's program replaces the start-up preset (its trim is never rendered)
    pendingTrimSide0 = false;
    meta = ProgramState::fromJson (root.getProperty ("program").toString());
    sides[0] = ProgramState::fromJson (root.getProperty ("sideA").toString());
    sides[1] = ProgramState::fromJson (root.getProperty ("sideB").toString());
    side = juce::jlimit (0, 1, (int) root.getProperty ("side", 0));
    premiumCable = (bool) root.getProperty ("premium", true);
    uiScale = juce::jlimit (0.0f, 2.0f, (float) (double) root.getProperty ("uiScale", 0.0)); // 0 = auto
    showGear = (bool) root.getProperty ("showGear", true);
    softwareTv = (bool) root.getProperty ("softwareTv", false);
    killLevel = (KillLevel) juce::jlimit (0, 2, (int) root.getProperty ("killLevel", 1));
    screening = juce::jlimit (-1, kNumSlots - 1, (int) root.getProperty ("screening", -1));
    lastChannel = juce::jlimit (0, 11, (int) root.getProperty ("lastChannel", 0));
    const int os = juce::jlimit (0, 2, (int) root.getProperty ("oversampling", 1));
    if (os != oversampling) setOversampling (os);

    undoStack.clear();
    redoStack.clear();
    pushSplitParams();
    pushConfigToEngine();
    pushExtrasToEngine();
    ++programEvents;
    notifyUi();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EffectorKillaAudioProcessor(); }
