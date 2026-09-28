#include "Engine.h"

namespace ek
{
Engine::Engine()
{
    for (auto* v : { &dryL, &dryR, &origL, &origR, &tmpR })
        v->assign ((size_t) kMaxChunk, 0.0f);
    for (auto& m : meters.macroValue) m.store (0.5f);
    for (auto& s : effectiveParams) for (auto& p : s) p.store (0.0f);
}

void Engine::prepare (double sr, int maxBlock, int osFactorLog2)
{
    juce::ignoreUnused (maxBlock);
    sampleRate = sr;
    rack.prepare (sr, osFactorLog2);
    modulation.prepare (sr);
    for (auto* m : { &inMeter, &rackMeter, &origMeter, &finalMeter }) m->prepare (sr);
    autoGain.prepare (sr);

    inGain.reset (sr, 30.0f, 1.0f);
    outGain.reset (sr, 30.0f, 1.0f);
    blend.reset (sr, 30.0f, 1.0f);
    trim.reset (sr, 60.0f, 1.0f);
    offAirRamp.reset ((int) (offAirFadeMs * 0.001 * sr), 0.0f);
    pocketRamp.reset ((int) (0.02 * sr), 0.0f);

    // dry paths are sized for the largest possible latency (4x oversampling)
    const int maxLatency = Rack::computeFixedLatency (sr, 2) + 8;
    for (auto& d : dryDelay) d.prepare (maxLatency);
    for (auto& d : origDelay) d.prepare (maxLatency);

    for (auto& h : pocketHp) { h.setHighPass (300.0f, 0.707f, (float) sr); h.reset(); }
    for (auto& l : pocketLp) { l.setLowPass (8000.0f, 0.707f, (float) sr); l.reset(); }
    pocketAtt = std::exp (-1.0f / (0.005f * (float) sr));
    pocketRel = std::exp (-1.0f / (0.08f * (float) sr));
    pocketEnv = 0.0f;
    transientFast.set (0.5f, 30.0f, (float) sr);
    transientSlow.set (30.0f, 200.0f, (float) sr);
    matchDb = 0.0f;
}

void Engine::setOversampling (int osFactorLog2) { rack.setOversampling (osFactorLog2); }

void Engine::setExtras (const ProgramExtras& e)
{
    auto copy = std::make_unique<ProgramExtras> (e);
    copy->mod.resamplePath();
    copy->pathMean = copy->mod.pathMean();
    extras.publish (std::move (copy));
}

void Engine::process (juce::AudioBuffer<float>& buffer, const EngineInput& in, const ProcessContext& hostCtx) noexcept
{
    const int numCh = buffer.getNumChannels();
    const int total = buffer.getNumSamples();
    if (numCh == 0 || total == 0) return;

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    meters.bpm.store (hostCtx.bpm);
    meters.playing.store (hostCtx.playing);

    for (int pos = 0; pos < total; pos += kMaxChunk)
    {
        const int n = std::min (kMaxChunk, total - pos);
        ProcessContext ctx = hostCtx;
        ctx.ppq = hostCtx.ppq + (double) pos * hostCtx.beatsPerSample();

        float* r = R != nullptr ? R + pos : tmpR.data();
        if (R == nullptr) std::copy (L + pos, L + pos + n, tmpR.data());
        processChunk (L + pos, r, n, in, ctx);
        if (R == nullptr)
            for (int i = 0; i < n; ++i) L[pos + i] = 0.5f * (L[pos + i] + tmpR[(size_t) i]);
        meters.ppq.store (ctx.ppq);
    }

    for (int c = 2; c < numCh; ++c) buffer.clear (c, 0, total);
}

void Engine::processChunk (float* L, float* R, int n, const EngineInput& in, const ProcessContext& ctx) noexcept
{
    const ProgramExtras* ex = extras.acquire();
    if (ex == nullptr) ex = &defaultExtras;

    const int latency = rack.getActiveLatency();

    // ---- original input (for OFF AIR) --------------------------------------------------
    std::copy (L, L + n, origL.data());
    std::copy (R, R + n, origR.data());
    origMeter.process (origL.data(), origR.data(), n);

    // ---- ANTENNA IN ---------------------------------------------------------------------
    inGain.setTarget (dbToGain (in.global.inGainDb));
    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.next();
        L[i] *= g; R[i] *= g;
        inPeak = std::max (inPeak, std::max (std::abs (L[i]), std::abs (R[i])));
    }
    meters.inLevel.store (inPeak);
    std::copy (L, L + n, dryL.data());
    std::copy (R, R + n, dryR.data());
    inMeter.process (L, R, n);

    // ---- AV modulation + macros --------------------------------------------------------
    const ModOutput mo = modulation.process (ex->mod, ex->pathMean, ctx, n);
    std::array<float, kNumMacros> macros = in.macros;
    auto applyOffset = [&] (int target, float off, RackParams* params)
    {
        if (ModTarget::isMacro (target))
        {
            if (! in.macroModSuspended[(size_t) target] && params == nullptr)
                macros[(size_t) target] = clamp01 (macros[(size_t) target] + off);
        }
        else if (ModTarget::isSlotParam (target) && params != nullptr)
        {
            auto& sp = params->slots[(size_t) ModTarget::slotOf (target)];
            const int p = ModTarget::paramOf (target);
            if (p == 8) sp.mix = clamp01 (sp.mix + off);
            else if (p >= 0 && p < kNumParams) sp.p[(size_t) p] = clamp01 (sp.p[(size_t) p] + off);
        }
    };
    if (mo.active)
    {
        applyOffset (ex->mod.targetX, mo.offX, nullptr);
        applyOffset (ex->mod.targetY, mo.offY, nullptr);
    }
    if (mo.glitch > 0.0f && ! in.macroModSuspended[CrashOut])
        macros[CrashOut] = clamp01 (macros[CrashOut] + ModulationEngine::glitchAmount * mo.glitch);

    RackParams eff;
    MacroEngine::compute (rack.getActiveConfig(), in.base, macros, ex->maps, ex->source, eff);
    if (mo.active)
    {
        applyOffset (ex->mod.targetX, mo.offX, &eff);
        applyOffset (ex->mod.targetY, mo.offY, &eff);
    }

    meters.dotX.store (mo.dotX);
    meters.dotY.store (mo.dotY);
    meters.glitch.store (mo.glitch);
    meters.modActive.store (mo.active);
    meters.cornerHits.store (modulation.getCornerHits());
    for (int m = 0; m < kNumMacros; ++m) meters.macroValue[(size_t) m].store (macros[(size_t) m]);
    for (int s = 0; s < kNumSlots; ++s)
        for (int p = 0; p < kNumParams; ++p)
            effectiveParams[(size_t) s][(size_t) p].store (eff.slots[(size_t) s].p[(size_t) p], std::memory_order_relaxed);

    // ---- rack ---------------------------------------------------------------------------
    rack.process (L, R, n, eff, ctx);

    // ---- trim + BLEND ------------------------------------------------------------------
    trim.setTarget (dbToGain (ex->trimDb));
    blend.setTarget (clamp01 (in.global.blend));
    for (int i = 0; i < n; ++i)
    {
        const float t = trim.next(), b = blend.next();
        dryDelay[0].push (dryL[(size_t) i]);
        dryDelay[1].push (dryR[(size_t) i]);
        const float dl = latency > 0 ? dryDelay[0].readInt (latency) : dryL[(size_t) i];
        const float dr = latency > 0 ? dryDelay[1].readInt (latency) : dryR[(size_t) i];
        L[i] = dl + (L[i] * t - dl) * b;
        R[i] = dr + (R[i] * t - dr) * b;
    }

    // ---- AUTO TRACKING + RF OUT ------------------------------------------------------------
    rackMeter.process (L, R, n);
    autoGain.process (L, R, n, inMeter, rackMeter, in.global.autoTracking);
    meters.autoGainDb.store (in.global.autoTracking ? autoGain.getGainDb() : 0.0f);

    outGain.setTarget (dbToGain (in.global.outGainDb));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.next();
        L[i] *= g; R[i] *= g;
    }
    finalMeter.process (L, R, n);

    // ---- OFF AIR: level-matched original, 20 ms crossfade ---------------------------------
    if (origMeter.hasSignal() && finalMeter.hasSignal())
        matchDb = juce::jlimit (-12.0f, 12.0f, finalMeter.getLoudnessDb() - origMeter.getLoudnessDb());
    meters.offAirMatchDb.store (matchDb);
    offAirRamp.setTarget (in.global.offAir ? 1.0f : 0.0f);
    const float mg = dbToGain (matchDb);
    for (int i = 0; i < n; ++i)
    {
        origDelay[0].push (origL[(size_t) i]);
        origDelay[1].push (origR[(size_t) i]);
        const float t = offAirRamp.next();
        if (t > 0.0f)
        {
            const float ol = (latency > 0 ? origDelay[0].readInt (latency) : origL[(size_t) i]) * mg;
            const float orr = (latency > 0 ? origDelay[1].readInt (latency) : origR[(size_t) i]) * mg;
            L[i] = L[i] + (ol - L[i]) * t;
            R[i] = R[i] + (orr - R[i]) * t;
        }
    }

    // ---- POCKET TV (monitoring): mono, 300 Hz - 8 kHz, light compression ------------------
    pocketRamp.setTarget (in.global.pocketTv ? 1.0f : 0.0f);
    if (pocketRamp.isRamping() || pocketRamp.get() > 0.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float t = pocketRamp.next();
            float m = 0.5f * (L[i] + R[i]);
            m = pocketLp[1].process (pocketLp[0].process (pocketHp[1].process (pocketHp[0].process (m))));
            const float a = std::abs (m);
            pocketEnv = a > pocketEnv ? a + (pocketEnv - a) * pocketAtt : a + (pocketEnv - a) * pocketRel;
            const float envDb = gainToDb (pocketEnv + 1.0e-9f);
            const float gr = envDb > -20.0f ? (envDb + 20.0f) * (1.0f / 3.0f - 1.0f) : 0.0f;
            m *= dbToGain (gr + 4.0f);
            L[i] = L[i] + (m - L[i]) * t;
            R[i] = R[i] + (m - R[i]) * t;
        }
    }

    // ---- meters -------------------------------------------------------------------------
    float peak = 0.0f; double sum = 0.0;
    float tr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::max (std::abs (L[i]), std::abs (R[i]));
        peak = std::max (peak, a);
        sum += (double) (L[i] * L[i] + R[i] * R[i]);
        const float f = transientFast.process (a), s = transientSlow.process (a);
        tr = std::max (tr, f - s);
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) { L[i] = R[i] = 0.0f; } // never pass NaN to the host
    }
    if (peak >= 0.999f) meters.clipCount.fetch_add (1);
    meters.outPeak.store (peak);
    meters.outLevel.store ((float) std::sqrt (sum / (2.0 * n)));
    meters.transient.store (tr);
}

} // namespace ek
