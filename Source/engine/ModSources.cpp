#include "ModSources.h"

namespace ek
{
// ============================================================================
// Names / tables
// ============================================================================
const char* modSourceName (int source) noexcept
{
    static const char* names[] = { "LFO 1", "LFO 2", "ENV", "SEQ" };
    return names[juce::jlimit (0, kNumModSources - 1, source)];
}

const char* lfoShapeName (LfoShape s) noexcept
{
    static const char* names[] = { "SINE", "TRI", "SAW", "SQUARE", "S&H" };
    return names[juce::jlimit (0, (int) LfoShape::Count - 1, (int) s)];
}

bool modSourceIsBipolar (int source) noexcept { return source == (int) ModSourceId::Lfo1 || source == (int) ModSourceId::Lfo2; }

static const ModDivision kDivisions[] = {
    { "4 BAR", 16.0 }, { "2 BAR", 8.0 }, { "1 BAR", 4.0 }, { "1/2", 2.0 }, { "1/4D", 1.5 }, { "1/4", 1.0 },
    { "1/4T", 2.0 / 3.0 }, { "1/8D", 0.75 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/32", 0.125 }
};

int numModDivisions() noexcept { return (int) (sizeof (kDivisions) / sizeof (kDivisions[0])); }
const ModDivision& modDivision (int index) noexcept { return kDivisions[juce::jlimit (0, numModDivisions() - 1, index)]; }

int findModDivision (const juce::String& label, int fallback) noexcept
{
    for (int i = 0; i < numModDivisions(); ++i)
        if (label.trim().equalsIgnoreCase (kDivisions[i].label)) return i;
    return fallback;
}

// ============================================================================
// ModAssignment / ModSourceState
// ============================================================================
bool ModAssignment::isValid() const noexcept
{
    if (source < 0 || source >= kNumModSources || ! std::isfinite (depth)) return false;
    if (slot < 0) return slot == -1 && param >= 0 && param < kNumMacros;
    return slot < kNumSlots && param >= 0 && param < kNumModSlotTargets;
}

int ModSourceState::add (const ModAssignment& a)
{
    if (! a.isValid()) return -1;
    auto c = a;
    c.depth = juce::jlimit (-1.0f, 1.0f, c.depth);
    for (int i = 0; i < numAssign; ++i)
    {
        auto& e = assign[(size_t) i];
        if (e.source == c.source && e.sameTarget (c)) { e.depth = c.depth; return i; }
    }
    if (numAssign >= kMaxModAssignments) return -1;
    assign[(size_t) numAssign] = c;
    return numAssign++;
}

void ModSourceState::remove (int index)
{
    if (index < 0 || index >= numAssign) return;
    for (int i = index; i < numAssign - 1; ++i) assign[(size_t) i] = assign[(size_t) i + 1];
    assign[(size_t) --numAssign] = {};
}

void ModSourceState::removeTarget (int slot, int param)
{
    for (int i = numAssign - 1; i >= 0; --i)
        if (assign[(size_t) i].slot == slot && assign[(size_t) i].param == param) remove (i);
}

void ModSourceState::removeSlot (int slot)
{
    if (slot < 0) return;
    for (int i = numAssign - 1; i >= 0; --i)
        if (assign[(size_t) i].slot == slot) remove (i);
}

void ModSourceState::remapSlots (const std::array<int, kNumSlots>& newIndexOfOld)
{
    for (int i = numAssign - 1; i >= 0; --i)
    {
        auto& a = assign[(size_t) i];
        if (a.isMacro()) continue;
        const int n = newIndexOfOld[(size_t) a.slot];
        if (n < 0) remove (i);
        else a.slot = n;
    }
}

bool ModSourceState::isModulating() const noexcept
{
    for (int i = 0; i < juce::jmin (numAssign, kMaxModAssignments); ++i)
        if (assign[(size_t) i].depth != 0.0f && assign[(size_t) i].isValid()) return true;
    return false;
}

bool ModSourceState::operator== (const ModSourceState& o) const noexcept
{
    if (! (lfo == o.lfo && env == o.env && seq == o.seq && numAssign == o.numAssign)) return false;
    for (int i = 0; i < numAssign; ++i)
        if (! (assign[(size_t) i] == o.assign[(size_t) i])) return false;
    return true;
}

juce::var ModSourceState::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("v", version);

    juce::Array<juce::var> lfos;
    for (auto& l : lfo)
    {
        auto* lo = new juce::DynamicObject();
        lo->setProperty ("shape", lfoShapeName (l.shape));
        lo->setProperty ("sync", l.sync);
        lo->setProperty ("hz", l.rateHz);
        lo->setProperty ("div", modDivision (l.division).label);
        lo->setProperty ("phase", l.phase);
        lfos.add (juce::var (lo));
    }
    o->setProperty ("lfo", lfos);

    auto* eo = new juce::DynamicObject();
    eo->setProperty ("att", env.attackMs);
    eo->setProperty ("rel", env.releaseMs);
    eo->setProperty ("sens", env.sensitivity);
    o->setProperty ("env", juce::var (eo));

    auto* so = new juce::DynamicObject();
    juce::Array<juce::var> steps;
    for (auto v : seq.steps) steps.add (v);
    so->setProperty ("steps", steps);
    so->setProperty ("div", modDivision (seq.division).label);
    so->setProperty ("smooth", seq.smooth);
    o->setProperty ("seq", juce::var (so));

    juce::Array<juce::var> as;
    for (int i = 0; i < numAssign; ++i)
    {
        const auto& a = assign[(size_t) i];
        auto* ao = new juce::DynamicObject();
        ao->setProperty ("src", modSourceName (a.source));
        if (a.isMacro()) ao->setProperty ("macro", a.param);
        else { ao->setProperty ("slot", a.slot); ao->setProperty ("param", a.param); }
        ao->setProperty ("depth", a.depth);
        as.add (juce::var (ao));
    }
    o->setProperty ("assign", as);
    return juce::var (o);
}

static float numProp (const juce::var& v, const char* name, float def, float lo, float hi)
{
    const auto p = v.getProperty (name, def);
    const float x = (float) (double) p;
    return std::isfinite (x) ? juce::jlimit (lo, hi, x) : def;
}

ModSourceState ModSourceState::fromVar (const juce::var& v)
{
    ModSourceState s;
    if (! v.isObject()) return s;

    if (auto* lfos = v.getProperty ("lfo", {}).getArray())
    {
        for (int i = 0; i < juce::jmin (2, lfos->size()); ++i)
        {
            const auto& lv = lfos->getReference (i);
            auto& l = s.lfo[(size_t) i];
            const auto shape = lv.getProperty ("shape", lfoShapeName (l.shape)).toString();
            for (int k = 0; k < (int) LfoShape::Count; ++k)
                if (shape.equalsIgnoreCase (lfoShapeName ((LfoShape) k))) l.shape = (LfoShape) k;
            l.sync = (bool) lv.getProperty ("sync", l.sync);
            l.rateHz = numProp (lv, "hz", l.rateHz, LfoSettings::minHz, LfoSettings::maxHz);
            l.division = findModDivision (lv.getProperty ("div", "").toString(), l.division);
            l.phase = numProp (lv, "phase", l.phase, 0.0f, 1.0f);
        }
    }

    const auto ev = v.getProperty ("env", {});
    if (ev.isObject())
    {
        s.env.attackMs = numProp (ev, "att", s.env.attackMs, 0.1f, 500.0f);
        s.env.releaseMs = numProp (ev, "rel", s.env.releaseMs, 5.0f, 5000.0f);
        s.env.sensitivity = numProp (ev, "sens", s.env.sensitivity, 0.0f, 1.0f);
    }

    const auto sv = v.getProperty ("seq", {});
    if (sv.isObject())
    {
        if (auto* steps = sv.getProperty ("steps", {}).getArray())
            for (int i = 0; i < juce::jmin (kSeqSteps, steps->size()); ++i)
            {
                const float x = (float) (double) steps->getReference (i);
                s.seq.steps[(size_t) i] = std::isfinite (x) ? clamp01 (x) : 0.0f;
            }
        s.seq.division = findModDivision (sv.getProperty ("div", "").toString(), s.seq.division);
        s.seq.smooth = numProp (sv, "smooth", s.seq.smooth, 0.0f, 1.0f);
    }

    if (auto* as = v.getProperty ("assign", {}).getArray())
    {
        for (auto& av : *as)
        {
            ModAssignment a;
            const auto src = av.getProperty ("src", 0);
            a.source = -1;
            if (src.isString())
            {
                for (int k = 0; k < kNumModSources; ++k)
                    if (src.toString().equalsIgnoreCase (modSourceName (k))) a.source = k;
            }
            else a.source = (int) src;
            if (av.hasProperty ("macro")) { a.slot = -1; a.param = (int) av.getProperty ("macro", 0); }
            else { a.slot = (int) av.getProperty ("slot", 0); a.param = (int) av.getProperty ("param", 0); }
            a.depth = numProp (av, "depth", 0.0f, -1.0f, 1.0f);
            s.add (a); // invalid entries are dropped
        }
    }
    return s;
}

// ============================================================================
// ModSourceEngine
// ============================================================================
void ModSourceEngine::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    reset();
}

void ModSourceEngine::reset()
{
    freeBeats = 0.0;
    lfoFreePhase = {};
    lfoPhaseOut = {};
    values = {};
    env = 0.0f;
    seqStep = 0;
    for (auto& s : slotOff) s.fill (0.0f);
    macroOff.fill (0.0f);
    anyOffset = false;
}

static inline uint64_t hash64 (uint64_t z) noexcept
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

float ModSourceEngine::lfoShape (LfoShape s, double phase01, juce::int64 cycle, int lfoIndex) noexcept
{
    const float p = (float) (phase01 - std::floor (phase01));
    switch (s)
    {
        case LfoShape::Sine:     return std::sin (kTwoPi * p);
        case LfoShape::Triangle: return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
        case LfoShape::Saw:      return 2.0f * p - 1.0f;
        case LfoShape::Square:   return p < 0.5f ? 1.0f : -1.0f;
        case LfoShape::SampleHold:
        {
            const auto h = hash64 ((uint64_t) cycle * 2654435761ull + (uint64_t) (lfoIndex + 1) * 0x51ED27ull);
            return (float) (h >> 40) * (2.0f / 16777216.0f) - 1.0f;
        }
        case LfoShape::Count:
        default: return 0.0f;
    }
}

float ModSourceEngine::seqValue (const SeqSettings& s, double stepPos) noexcept
{
    const double fl = std::floor (stepPos);
    const int k = (int) (((juce::int64) fl % kSeqSteps + kSeqSteps) % kSeqSteps);
    const float cur = clamp01 (s.steps[(size_t) k]);
    const float sm = clamp01 (s.smooth);
    if (sm <= 0.0f) return cur;
    const float f = (float) (stepPos - fl);
    if (f >= sm) return cur;
    const float prev = clamp01 (s.steps[(size_t) ((k + kSeqSteps - 1) % kSeqSteps)]);
    const float t = f / sm;
    return prev + (cur - prev) * (t * t * (3.0f - 2.0f * t)); // smoothstep glide
}

void ModSourceEngine::advance (const ModSourceState& st, const ProcessContext& ctx, const float* inL, const float* inR, int n) noexcept
{
    if (n <= 0) return;
    const double bps = ctx.beatsPerSample();
    const double dBeats = (double) n * (std::isfinite (bps) ? bps : 0.0);
    const bool transport = ctx.playing && ctx.hostHasPosition;
    const double b1 = (transport ? ctx.ppq : freeBeats) + dBeats; // position at the end of this block
    freeBeats = b1;

    // ---- LFOs (value at the end of the block) ----
    for (int i = 0; i < 2; ++i)
    {
        const auto& l = st.lfo[(size_t) i];
        double cycles;
        if (l.sync)
        {
            cycles = b1 / std::max (1.0e-3, modDivision (l.division).beats);
            lfoFreePhase[(size_t) i] = cycles - std::floor (cycles); // switching to free continues from here
        }
        else
        {
            const double hz = juce::jlimit ((double) LfoSettings::minHz, (double) LfoSettings::maxHz, (double) l.rateHz);
            double& ph = lfoFreePhase[(size_t) i];
            ph += hz * (double) n / sampleRate;
            if (ph >= 1.0e6) ph -= std::floor (ph); // keep precision, the S&H cycle index restarts
            cycles = ph;
        }
        const double shifted = cycles + (double) clamp01 (l.phase);
        const double fl = std::floor (shifted);
        lfoPhaseOut[(size_t) i] = shifted - fl;
        values[(size_t) i] = lfoShape (l.shape, shifted - fl, (juce::int64) fl, i);
    }

    // ---- envelope follower ----
    {
        const float att = (float) std::exp (-1.0 / (std::max (0.1f, st.env.attackMs) * 0.001 * sampleRate));
        const float rel = (float) std::exp (-1.0 / (std::max (5.0f, st.env.releaseMs) * 0.001 * sampleRate));
        const float gain = dbToGain (EnvSettings::sensitivityGainDb (st.env.sensitivity));
        float e = env;
        for (int i = 0; i < n; ++i)
        {
            float x = std::max (std::abs (inL[i]), std::abs (inR != nullptr ? inR[i] : inL[i])) * gain;
            if (! std::isfinite (x)) x = 0.0f;
            x = std::min (x, 4.0f);
            e = x > e ? x + (e - x) * att : x + (e - x) * rel;
        }
        env = e < 1.0e-9f ? 0.0f : e;
        values[(size_t) ModSourceId::Env] = clamp01 (env);
    }

    // ---- step sequencer ----
    {
        const double stepPos = b1 / std::max (1.0e-3, modDivision (st.seq.division).beats);
        seqStep = (int) (((juce::int64) std::floor (stepPos) % kSeqSteps + kSeqSteps) % kSeqSteps);
        values[(size_t) ModSourceId::Seq] = seqValue (st.seq, stepPos);
    }
}

bool ModSourceEngine::updateOffsets (const ModSourceState& st, int n) noexcept
{
    std::array<std::array<float, kNumModSlotTargets>, kNumSlots> rawSlot {};
    std::array<float, kNumMacros> rawMacro {};
    for (int i = 0; i < juce::jmin (st.numAssign, kMaxModAssignments); ++i)
    {
        const auto& a = st.assign[(size_t) i];
        if (! a.isValid()) continue;
        const float off = juce::jlimit (-1.0f, 1.0f, a.depth) * values[(size_t) a.source];
        if (a.isMacro()) rawMacro[(size_t) a.param] += off;
        else rawSlot[(size_t) a.slot][(size_t) a.param] += off;
    }

    const float k = 1.0f - (float) std::exp (-(double) std::max (1, n) / (smoothMs * 0.001 * sampleRate));
    bool any = false;
    auto step = [&] (float& cur, float raw)
    {
        raw = juce::jlimit (-2.0f, 2.0f, raw);
        cur += (raw - cur) * k;
        if (std::abs (cur - raw) < 1.0e-6f) cur = raw;
        if (! std::isfinite (cur)) cur = 0.0f;
        any = any || cur != 0.0f;
    };
    for (int s = 0; s < kNumSlots; ++s)
        for (int p = 0; p < kNumModSlotTargets; ++p)
            step (slotOff[(size_t) s][(size_t) p], rawSlot[(size_t) s][(size_t) p]);
    for (int m = 0; m < kNumMacros; ++m)
        step (macroOff[(size_t) m], rawMacro[(size_t) m]);
    anyOffset = any;
    return any;
}

void ModSourceEngine::publish (ModSourceMeters& m) const noexcept
{
    for (int i = 0; i < kNumModSources; ++i) m.value[(size_t) i].store (values[(size_t) i], std::memory_order_relaxed);
    for (int i = 0; i < 2; ++i) m.lfoPhase[(size_t) i].store ((float) lfoPhaseOut[(size_t) i], std::memory_order_relaxed);
    m.seqStep.store (seqStep, std::memory_order_relaxed);
    m.active.store (anyOffset, std::memory_order_relaxed);
    for (int s = 0; s < kNumSlots; ++s)
        for (int p = 0; p < kNumModSlotTargets; ++p)
            m.slotOffset[(size_t) s][(size_t) p].store (slotOff[(size_t) s][(size_t) p], std::memory_order_relaxed);
    for (int k = 0; k < kNumMacros; ++k) m.macroOffset[(size_t) k].store (macroOff[(size_t) k], std::memory_order_relaxed);
}
} // namespace ek
