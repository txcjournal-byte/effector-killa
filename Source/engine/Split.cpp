#include "Rack.h"

namespace ek
{
// ===========================================================================
// Names / serialisation
// ===========================================================================
const char* splitModeName (SplitMode m) noexcept
{
    switch (m)
    {
        case SplitMode::Parallel:         return "PARALLEL";
        case SplitMode::LeftRight:        return "L/R";
        case SplitMode::MidSide:          return "M/S";
        case SplitMode::LowHigh:          return "LOW/HIGH";
        case SplitMode::TransientSustain: return "TRANS/SUS";
        case SplitMode::Count:
        default:                          return "PARALLEL";
    }
}

const char* splitModeId (SplitMode m) noexcept
{
    switch (m)
    {
        case SplitMode::Parallel:         return "PARALLEL";
        case SplitMode::LeftRight:        return "LR";
        case SplitMode::MidSide:          return "MS";
        case SplitMode::LowHigh:          return "LOWHIGH";
        case SplitMode::TransientSustain: return "TRANSIENT";
        case SplitMode::Count:
        default:                          return "PARALLEL";
    }
}

SplitMode splitModeFromId (const juce::String& id) noexcept
{
    for (int i = 0; i < kNumSplitModes; ++i)
        if (id.equalsIgnoreCase (splitModeId ((SplitMode) i)) || id.equalsIgnoreCase (splitModeName ((SplitMode) i)))
            return (SplitMode) i;
    return SplitMode::Parallel;
}

const char* splitBranchName (SplitMode m, int branch) noexcept
{
    const bool b = branch != 0;
    switch (m)
    {
        case SplitMode::LeftRight:        return b ? "RIGHT" : "LEFT";
        case SplitMode::MidSide:          return b ? "SIDE" : "MID";
        case SplitMode::LowHigh:          return b ? "HIGH" : "LOW";
        case SplitMode::TransientSustain: return b ? "SUSTAIN" : "TRANSIENT";
        case SplitMode::Parallel:
        case SplitMode::Count:
        default:                          return b ? "B" : "A";
    }
}

SplitParams SplitParams::sanitised() const noexcept
{
    auto fin = [] (float v, float def) { return std::isfinite (v) ? v : def; };
    SplitParams p;
    p.freqHz = juce::jlimit (minFreq, maxFreq, fin (freqHz, 250.0f));
    p.balance = clamp01 (fin (balance, 0.5f));
    p.levelADb = juce::jlimit (minLevelDb, maxLevelDb, fin (levelADb, 0.0f));
    p.levelBDb = juce::jlimit (minLevelDb, maxLevelDb, fin (levelBDb, 0.0f));
    return p;
}

void splitMergeGains (SplitMode mode, float balance, float& gA, float& gB) noexcept
{
    const float b = clamp01 (balance);
    if (mode == SplitMode::Parallel)
    {
        // both branches carry the full signal: equal-gain crossfade, 0.5 / 0.5 sums to unity
        gA = 1.0f - b;
        gB = b;
    }
    else
    {
        // complementary parts: both at full level in the middle (sum = input), one fades out towards an end
        gA = std::min (1.0f, 2.0f * (1.0f - b));
        gB = std::min (1.0f, 2.0f * b);
    }
}

juce::var SplitState::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("at", at);
    o->setProperty ("mode", juce::String (splitModeId (mode)));
    o->setProperty ("freq", params.freqHz);
    o->setProperty ("balance", params.balance);
    o->setProperty ("levelA", params.levelADb);
    o->setProperty ("levelB", params.levelBDb);
    return juce::var (o);
}

SplitState SplitState::fromVar (const juce::var& v)
{
    SplitState s;
    if (! v.isObject()) return s;
    const int at = (int) v.getProperty ("at", -1);
    s.at = (at >= 0 && at <= kNumSlots) ? at : -1;
    s.mode = splitModeFromId (v.getProperty ("mode", "PARALLEL").toString());
    SplitParams p;
    p.freqHz = (float) (double) v.getProperty ("freq", 250.0);
    p.balance = (float) (double) v.getProperty ("balance", 0.5);
    p.levelADb = (float) (double) v.getProperty ("levelA", 0.0);
    p.levelBDb = (float) (double) v.getProperty ("levelB", 0.0);
    s.params = p.sanitised();
    return s;
}

bool SplitState::operator== (const SplitState& o) const noexcept
{
    if (isActive() != o.isActive()) return false;
    if (! isActive()) return true;
    auto near = [] (float a, float b, float tol) { return std::abs (a - b) <= tol; };
    return at == o.at && mode == o.mode
        && near (params.freqHz, o.params.freqHz, 0.01f) && near (params.balance, o.params.balance, 1.0e-4f)
        && near (params.levelADb, o.params.levelADb, 1.0e-3f) && near (params.levelBDb, o.params.levelBDb, 1.0e-3f);
}

// ===========================================================================
// Splitter
// ===========================================================================
namespace
{
constexpr int kCoeffInterval = 16;     // crossover coefficients follow the smoothed frequency every 16 samples
constexpr float kGainSmoothMs = 20.0f;
constexpr float kFreqSmoothMs = 40.0f;
}

void Splitter::prepare (double sampleRate, SplitMode m, const SplitParams& initial)
{
    sr = sampleRate;
    mode = m;
    last = initial.sanitised();
    xL.prepare (sr);
    xR.prepare (sr);
    freq.reset (sr, kFreqSmoothMs, last.freqHz);
    xL.setCutoff (last.freqHz, true);
    xR.setCutoff (last.freqHz, true);
    float a, b;
    splitMergeGains (mode, last.balance, a, b);
    gainA.reset (sr, kGainSmoothMs, a * dbToGain (last.levelADb));
    gainB.reset (sr, kGainSmoothMs, b * dbToGain (last.levelBDb));
    // transient detector: fast vs. slow envelope of the linked peak
    fast.set (0.3f, 40.0f, (float) sr);
    slow.set (15.0f, 150.0f, (float) sr);
    transientGain.reset (sr, 1.5f, 0.0f);
    coeffCountdown = 0;
    paramsSet = false;
    reset();
}

void Splitter::reset() noexcept
{
    xL.reset();
    xR.reset();
    fast.reset();
    slow.reset();
    transientGain.snap (0.0f);
}

void Splitter::setParams (const SplitParams* p) noexcept
{
    if (p != nullptr) last = p->sanitised();
    float a, b;
    splitMergeGains (mode, last.balance, a, b);
    a *= dbToGain (last.levelADb);
    b *= dbToGain (last.levelBDb);
    if (! paramsSet && p != nullptr)
    {
        // a freshly built chain starts at the current values instead of gliding from the defaults
        freq.snap (last.freqHz);
        xL.setCutoff (last.freqHz, true);
        xR.setCutoff (last.freqHz, true);
        gainA.snap (a);
        gainB.snap (b);
        paramsSet = true;
    }
    freq.setTarget (last.freqHz);
    gainA.setTarget (a);
    gainB.setTarget (b);
}

void Splitter::split (const float* L, const float* R, float* aL, float* aR, float* bL, float* bR, int n) noexcept
{
    switch (mode)
    {
        case SplitMode::Parallel:
            std::copy (L, L + n, aL); std::copy (R, R + n, aR);
            std::copy (L, L + n, bL); std::copy (R, R + n, bR);
            break;

        case SplitMode::LeftRight:
            // A = left side of the stereo image, B = right side; A + B = input exactly
            for (int i = 0; i < n; ++i)
            {
                aL[i] = L[i]; aR[i] = 0.0f;
                bL[i] = 0.0f; bR[i] = R[i];
            }
            break;

        case SplitMode::MidSide:
            // A = mid (M, M), B = side (S, -S); A + B = (M + S, M - S) = input
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                const float s = 0.5f * (L[i] - R[i]);
                aL[i] = aR[i] = m;
                bL[i] = s; bR[i] = -s;
            }
            break;

        case SplitMode::LowHigh:
            // Linkwitz-Riley 24 dB/oct: low + high = allpass (flat magnitude, phase-correct sum)
            for (int i = 0; i < n; ++i)
            {
                if (--coeffCountdown <= 0)
                {
                    const float f = freq.skip (kCoeffInterval);
                    xL.setCutoff (f);
                    xR.setCutoff (f);
                    coeffCountdown = kCoeffInterval;
                }
                xL.process (L[i], aL[i], bL[i]);
                xR.process (R[i], aR[i], bR[i]);
            }
            break;

        case SplitMode::TransientSustain:
            // A = attack part, B = the rest; a smoothed gain t moves the signal between them, A + B = input
            for (int i = 0; i < n; ++i)
            {
                const float x = std::max (std::abs (L[i]), std::abs (R[i]));
                const float f = fast.process (x);
                const float s = slow.process (x);
                const float raw = f > 1.0e-6f ? clamp01 (2.5f * (f - s) / f) : 0.0f;
                transientGain.setTarget (raw);
                const float t = transientGain.next();
                aL[i] = L[i] * t; aR[i] = R[i] * t;
                bL[i] = L[i] - aL[i]; bR[i] = R[i] - aR[i];
            }
            break;

        case SplitMode::Count:
        default:
            std::copy (L, L + n, aL); std::copy (R, R + n, aR);
            std::fill (bL, bL + n, 0.0f); std::fill (bR, bR + n, 0.0f);
            break;
    }
}

void Splitter::merge (float* L, float* R, const float* aL, const float* aR, const float* bL, const float* bR, int n, int focus) noexcept
{
    if (focus == 0 || focus == 1)
    {
        const float* sl = focus == 0 ? aL : bL;
        const float* sr2 = focus == 0 ? aR : bR;
        std::copy (sl, sl + n, L);
        std::copy (sr2, sr2 + n, R);
        gainA.skip (n);
        gainB.skip (n);
        return;
    }

    if (! gainA.isSmoothing() && ! gainB.isSmoothing() && gainA.get() == 1.0f && gainB.get() == 1.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            L[i] = aL[i] + bL[i];
            R[i] = aR[i] + bR[i];
        }
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        const float ga = gainA.next(), gb = gainB.next();
        L[i] = aL[i] * ga + bL[i] * gb;
        R[i] = aR[i] * ga + bR[i] * gb;
    }
}

// ===========================================================================
// Chain hooks (see Rack.h): routing, latency and processing of a split chain
// ===========================================================================
void Chain::initSplitRouting()
{
    const auto& sc = config.split;
    if (! sc.isActive()) return;

    const int at = sc.at;
    int focus = -1;
    if (soloSlot >= 0)
    {
        if (soloSlot < at) return;           // soloing a unit in front of the split: plain serial path
        focus = sc.branch[(size_t) soloSlot] != 0 ? 1 : 0;
    }
    else if (config.screening >= 0 && config.screening < kNumSlots)
    {
        if (config.screening < at) return;   // listening before the split: plain serial path
        focus = sc.branch[(size_t) config.screening] != 0 ? 1 : 0;
    }

    auto rt = std::make_unique<SplitRuntime>();
    rt->at = at;
    rt->focus = focus;
    for (int i = 0; i < kNumSlots; ++i)
    {
        rt->zone[(size_t) i] = i < at ? -1 : (sc.branch[(size_t) i] != 0 ? 1 : 0);
        if (focus >= 0 && rt->zone[(size_t) i] >= 0 && rt->zone[(size_t) i] != focus)
            heard[(size_t) i] = false; // the other branch is muted while listening inside one branch
    }
    splitRt = std::move (rt);
}

int Chain::prepareSplit (const PrepareSpec& spec)
{
    auto& rt = *splitRt;
    int pre = 0, latA = 0, latB = 0;
    for (int i = 0; i < kNumSlots; ++i)
    {
        if (! isHeard (i)) continue;
        const int lat = slots[(size_t) i].latency;
        const int z = rt.zone[(size_t) i];
        (z < 0 ? pre : (z == 0 ? latA : latB)) += lat;
    }
    const int branchLat = std::max (latA, latB);
    rt.padA.prepare (branchLat - latA);
    rt.padB.prepare (branchLat - latB);
    rt.splitter.prepare (spec.sampleRate, config.split.mode, SplitParams());
    for (auto* v : { &rt.aL, &rt.aR, &rt.bL, &rt.bR })
        v->assign ((size_t) kMaxChunk, 0.0f);
    return pre + branchLat;
}

void Chain::processSplit (float* L, float* R, int n, const RackParams* params, const ProcessContext& ctx) noexcept
{
    auto& rt = *splitRt;

    for (int i = 0; i < rt.at && i < kNumSlots; ++i)
        if (isHeard (i))
            processSlot (slots[(size_t) i], i, L, R, n, params, ctx);

    rt.splitter.setParams (params != nullptr ? &params->split : nullptr);
    rt.splitter.split (L, R, rt.aL.data(), rt.aR.data(), rt.bL.data(), rt.bR.data(), n);

    for (int i = rt.at; i < kNumSlots; ++i)
    {
        if (! isHeard (i)) continue;
        if (rt.zone[(size_t) i] == 0) processSlot (slots[(size_t) i], i, rt.aL.data(), rt.aR.data(), n, params, ctx);
        else                          processSlot (slots[(size_t) i], i, rt.bL.data(), rt.bR.data(), n, params, ctx);
    }

    rt.padA.processBlock (rt.aL.data(), rt.aR.data(), n);
    rt.padB.processBlock (rt.bL.data(), rt.bR.data(), n);
    rt.splitter.merge (L, R, rt.aL.data(), rt.aR.data(), rt.bL.data(), rt.bR.data(), n, rt.focus);
}

} // namespace ek
