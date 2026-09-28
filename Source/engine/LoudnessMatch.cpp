#include "LoudnessMatch.h"
#include "Macro.h"
#include "TestSignals.h"

namespace ek
{
// ============================================================================
void LoudnessMeter::prepare (double sr)
{
    for (auto& s : shelf) s.setHighShelf (1500.0f, 4.0f, (float) sr, 1.0f);
    for (auto& h : hp) h.setHighPass (60.0f, 0.5f, (float) sr);
    blockLen = std::max (1, (int) (sr * 0.1));
    reset();
}

void LoudnessMeter::reset()
{
    for (auto& s : shelf) s.reset();
    for (auto& h : hp) h.reset();
    blocks.fill (0.0);
    blockPos = blockIndex = filled = 0;
    acc = 0.0;
    loudnessDb = lastBlockDb = -120.0f;
}

void LoudnessMeter::process (const float* L, const float* R, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const float l = hp[0].process (shelf[0].process (L[i]));
        const float r = hp[1].process (shelf[1].process (R[i]));
        acc += (double) (l * l + r * r) * 0.5;
        if (++blockPos >= blockLen)
        {
            const double ms = acc / (double) blockLen;
            lastBlockDb = (float) (10.0 * std::log10 (ms + 1.0e-12));
            blocks[(size_t) blockIndex] = ms;
            blockIndex = (blockIndex + 1) % kBlocks;
            filled = std::min (kBlocks, filled + 1);
            acc = 0.0;
            blockPos = 0;

            // gated mean over the window: ignore blocks 20 dB below the loudest (silence gaps)
            double maxMs = 0.0;
            for (int k = 0; k < filled; ++k) maxMs = std::max (maxMs, blocks[(size_t) k]);
            double sum = 0.0; int cnt = 0;
            for (int k = 0; k < filled; ++k)
                if (blocks[(size_t) k] > maxMs * 0.01) { sum += blocks[(size_t) k]; ++cnt; }
            loudnessDb = cnt > 0 ? (float) (10.0 * std::log10 (sum / cnt + 1.0e-12)) : -120.0f;
        }
    }
}

// ============================================================================
void AutoGain::prepare (double sr)
{
    sampleRate = sr;
    coeffPerSample = (float) std::exp (-1.0 / (1.0 * sr)); // ~1 s time constant
    enableRamp.reset ((int) (0.05 * sr), 0.0f);
    reset();
}

void AutoGain::reset() { targetDb = currentDb = 0.0f; }

void AutoGain::process (float* L, float* R, int n, const LoudnessMeter& in, const LoudnessMeter& out, bool enabled) noexcept
{
    // freeze while either side is silent – never chase noise floors
    if (in.hasSignal() && out.hasSignal() && in.getBlockDb() > -50.0f)
        targetDb = juce::jlimit (-maxDb, maxDb, in.getLoudnessDb() - (out.getLoudnessDb() - currentDb));

    enableRamp.setTarget (enabled ? 1.0f : 0.0f);
    const float c = std::pow (coeffPerSample, (float) n);
    const float start = currentDb;
    currentDb = targetDb + (currentDb - targetDb) * c;
    const float g0 = dbToGain (start), g1 = dbToGain (currentDb);
    const float step = (g1 - g0) / (float) std::max (1, n);
    for (int i = 0; i < n; ++i)
    {
        const float e = enableRamp.next();
        const float g = 1.0f + (g0 + step * (float) i - 1.0f) * e;
        L[i] *= g;
        R[i] *= g;
    }
}

// ============================================================================
void measureProgramLoudness (const ProgramState& program, double sr, float seconds, float& inDb, float& outDb,
                             bool applyTrim, bool* sawNaN, int osFactorLog2)
{
    const int total = (int) (seconds * sr);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    TestSignals::generate (TestSignals::forSource (program.source), sr, L.data(), R.data(), total, 7);

    PrepareSpec spec;
    spec.sampleRate = sr;
    spec.maxBlock = kMaxChunk;
    spec.osFactorLog2 = osFactorLog2;
    const int fixed = Rack::computeFixedLatency (sr, osFactorLog2);
    Chain chain (program.rackConfig(), spec, fixed);

    RackParams base, eff;
    for (int s = 0; s < kNumSlots; ++s)
    {
        base.slots[(size_t) s].p = program.slots[(size_t) s].p;
        base.slots[(size_t) s].mix = program.slots[(size_t) s].mix;
        base.slots[(size_t) s].pause = program.slots[(size_t) s].pause;
    }
    MacroEngine::compute (program.rackConfig(), base, program.macros, program.maps, program.source, eff);

    LoudnessMeter inMeter, outMeter;
    inMeter.prepare (sr);
    outMeter.prepare (sr);
    const float trim = applyTrim ? dbToGain (program.trimDb) : 1.0f;
    const int warm = std::min (total / 4, (int) (0.25 * sr));

    ProcessContext ctx;
    ctx.sampleRate = sr;
    std::vector<float> oL ((size_t) kMaxChunk), oR ((size_t) kMaxChunk);
    bool nan = false;
    for (int pos = 0; pos < total; pos += kMaxChunk)
    {
        const int n = std::min (kMaxChunk, total - pos);
        std::copy (L.begin() + pos, L.begin() + pos + n, oL.begin());
        std::copy (R.begin() + pos, R.begin() + pos + n, oR.begin());
        chain.process (oL.data(), oR.data(), n, &eff, ctx);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (oL[(size_t) i]) || ! std::isfinite (oR[(size_t) i])) { nan = true; oL[(size_t) i] = oR[(size_t) i] = 0.0f; }
            oL[(size_t) i] *= trim;
            oR[(size_t) i] *= trim;
        }
        ctx.ppq += (double) n * ctx.beatsPerSample();
        if (pos >= warm)
        {
            inMeter.process (L.data() + pos, R.data() + pos, n);
            outMeter.process (oL.data(), oR.data(), n);
        }
    }
    inDb = inMeter.getLoudnessDb();
    outDb = outMeter.getLoudnessDb();
    if (sawNaN != nullptr) *sawNaN = nan;
}

float estimateProgramTrimDb (const ProgramState& program, double sampleRate, float seconds)
{
    float inDb = 0.0f, outDb = 0.0f;
    auto p = program;
    p.trimDb = 0.0f;
    measureProgramLoudness (p, sampleRate, seconds, inDb, outDb, false);
    if (outDb < -90.0f) return 0.0f;
    return juce::jlimit (-24.0f, 24.0f, inDb - outDb);
}

} // namespace ek
