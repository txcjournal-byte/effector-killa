#include "Rack.h"

namespace ek
{
// ===========================================================================
// Chain
// ===========================================================================
Chain::Chain (const RackConfig& cfg, const PrepareSpec& spec, int fixedLatency) : config (cfg)
{
    lastSlot = (cfg.screening >= 0 && cfg.screening < kNumSlots) ? cfg.screening : kNumSlots - 1;

    // Only the last Red Line of the processed part of the chain may use lookahead,
    // so the fixed latency budget is never exceeded.
    int lookaheadSlot = -1;
    for (int i = 0; i <= lastSlot; ++i)
        if (cfg.slots[(size_t) i].type == EffectType::RedLine)
            lookaheadSlot = i;

    actualLatency = 0;
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& s = slots[(size_t) i];
        s.ms = cfg.slots[(size_t) i].ms;
        if (cfg.slots[(size_t) i].type == EffectType::None)
            continue;

        s.fx = createEffect (cfg.slots[(size_t) i].type);
        auto slotSpec = spec;
        slotSpec.maxBlock = kMaxChunk;
        slotSpec.allowLookahead = (i == lookaheadSlot);
        s.fx->prepare (slotSpec);
        s.latency = s.fx->getLatencySamples();
        s.dryDelay.prepare (s.latency);
        s.otherDelay.prepare (s.latency);
        s.wet.reset (spec.sampleRate, 12.0f, 1.0f);
        if (i <= lastSlot)
            actualLatency += s.latency;
    }

    jassert (actualLatency <= fixedLatency);
    pad.prepare (std::max (0, fixedLatency - actualLatency));

    for (auto* v : { &dryL, &dryR, &wL, &wR, &other })
        v->assign ((size_t) kMaxChunk, 0.0f);
}

void Chain::process (float* L, float* R, int n, const RackParams* params, const ProcessContext& ctx) noexcept
{
    for (int i = 0; i <= lastSlot; ++i)
        processSlot (slots[(size_t) i], i, L, R, n, params, ctx);

    pad.processBlock (L, R, n);
}

void Chain::processSlot (Slot& s, int index, float* L, float* R, int n, const RackParams* params, const ProcessContext& ctx) noexcept
{
    if (s.fx == nullptr)
        return;

    if (params != nullptr)
    {
        const auto& sp = params->slots[(size_t) index];
        s.fx->setParameters (sp.p.data());
        const float target = sp.pause ? 0.0f : clamp01 (sp.mix);
        if (! s.paramsSet) s.wet.snap (target);
        s.wet.setTarget (target);
        s.paramsSet = true;
    }

    const bool needFx = s.wet.getTarget() > 0.0f || s.wet.get() > 1.0e-5f;
    if (needFx && ! s.running)
    {
        s.fx->reset(); // restart cleanly, the wet ramp hides the restart
        s.running = true;
    }
    else if (! needFx && s.running)
    {
        s.running = false;
    }

    std::copy (L, L + n, dryL.data());
    std::copy (R, R + n, dryR.data());

    if (s.running)
    {
        switch (s.ms)
        {
            case MSMode::Stereo:
                std::copy (L, L + n, wL.data());
                std::copy (R, R + n, wR.data());
                s.fx->process (wL.data(), wR.data(), n, ctx);
                break;

            case MSMode::Mid:
            case MSMode::Side:
            {
                const bool mid = s.ms == MSMode::Mid;
                for (int i = 0; i < n; ++i)
                {
                    const float m = 0.5f * (L[i] + R[i]);
                    const float sd = 0.5f * (L[i] - R[i]);
                    wL[(size_t) i] = wR[(size_t) i] = mid ? m : sd;
                    other[(size_t) i] = mid ? sd : m;
                }
                s.fx->process (wL.data(), wR.data(), n, ctx);
                for (int i = 0; i < n; ++i)
                {
                    const float processed = 0.5f * (wL[(size_t) i] + wR[(size_t) i]);
                    const float o = s.otherDelay.process (0, other[(size_t) i]);
                    const float m = mid ? processed : o;
                    const float sd = mid ? o : processed;
                    wL[(size_t) i] = m + sd;
                    wR[(size_t) i] = m - sd;
                }
                break;
            }
        }
    }

    for (int i = 0; i < n; ++i)
    {
        const float dl = s.dryDelay.process (0, dryL[(size_t) i]);
        const float dr = s.dryDelay.process (1, dryR[(size_t) i]);
        const float w = s.wet.next();
        if (s.running)
        {
            L[i] = dl + (wL[(size_t) i] - dl) * w;
            R[i] = dr + (wR[(size_t) i] - dr) * w;
        }
        else
        {
            L[i] = dl;
            R[i] = dr;
        }
    }
}

// ===========================================================================
// Rack
// ===========================================================================
Rack::Rack()
{
    for (auto* v : { &aL, &aR, &bL, &bR })
        v->assign ((size_t) kMaxChunk, 0.0f);
    spec.maxBlock = kMaxChunk;
}

Rack::~Rack()
{
    delete current;
    delete next;
    delete pending.exchange (nullptr);
    collectGarbage();
}

int Rack::redLineLookaheadSamples (double sampleRate)
{
    return (int) std::lround (0.0015 * sampleRate); // 1.5 ms
}

int Rack::computeFixedLatency (double sampleRate, int osFactorLog2)
{
    return kNumSlots * Oversampler::latencyFor (osFactorLog2) + redLineLookaheadSamples (sampleRate);
}

Chain* Rack::buildChain (const RackConfig& cfg) const
{
    return new Chain (cfg, spec, fixedLatency);
}

void Rack::prepare (double sampleRate, int osFactorLog2)
{
    const juce::ScopedLock sl (configLock);

    delete current; current = nullptr;
    delete next; next = nullptr;
    delete pending.exchange (nullptr);
    fading = false;
    collectGarbage();

    spec.sampleRate = sampleRate;
    spec.osFactorLog2 = juce::jlimit (0, 2, osFactorLog2);
    spec.maxBlock = kMaxChunk;
    fixedLatency = computeFixedLatency (sampleRate, spec.osFactorLog2);
    fadeLen = std::max (1, (int) std::lround (crossfadeMs * 0.001 * sampleRate));

    current = buildChain (lastConfig);
}

void Rack::setOversampling (int osFactorLog2)
{
    const juce::ScopedLock sl (configLock);
    osFactorLog2 = juce::jlimit (0, 2, osFactorLog2);
    if (osFactorLog2 == spec.osFactorLog2) return;
    spec.osFactorLog2 = osFactorLog2;
    fixedLatency = computeFixedLatency (spec.sampleRate, spec.osFactorLog2);
    delete pending.exchange (buildChain (lastConfig));
    collectGarbage();
}

const RackConfig& Rack::getActiveConfig() const noexcept
{
    static const RackConfig empty;
    if (fading.load (std::memory_order_relaxed) && next != nullptr) return next->getConfig();
    return current != nullptr ? current->getConfig() : empty;
}

int Rack::getActiveLatency() const noexcept
{
    if (fading.load (std::memory_order_relaxed) && next != nullptr) return next->getTotalLatency();
    return current != nullptr ? current->getTotalLatency() : fixedLatency;
}

void Rack::setConfig (const RackConfig& cfg)
{
    const juce::ScopedLock sl (configLock);
    lastConfig = cfg;
    Chain* c = buildChain (cfg);
    delete pending.exchange (c); // an older, never-consumed chain can be deleted right here
    collectGarbage();
}

RackConfig Rack::getConfig() const
{
    const juce::ScopedLock sl (configLock);
    return lastConfig;
}

void Rack::retire (Chain* c) noexcept
{
    if (c == nullptr) return;
    const auto scope = graveyardFifo.write (1);
    if (scope.blockSize1 > 0) graveyard[(size_t) scope.startIndex1] = c;
    else if (scope.blockSize2 > 0) graveyard[(size_t) scope.startIndex2] = c;
    else jassertfalse; // cannot happen: fades only start when there is free space
}

void Rack::collectGarbage()
{
    for (;;)
    {
        const auto scope = graveyardFifo.read (1);
        Chain* c = nullptr;
        if (scope.blockSize1 > 0) c = graveyard[(size_t) scope.startIndex1];
        else if (scope.blockSize2 > 0) c = graveyard[(size_t) scope.startIndex2];
        else break;
        delete c;
    }
}

void Rack::process (float* L, float* R, int n, const RackParams& params, const ProcessContext& ctx) noexcept
{
    jassert (n <= kMaxChunk);

    if (! fading.load (std::memory_order_relaxed) && graveyardFifo.getFreeSpace() > 1)
    {
        if (Chain* p = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            if (current == nullptr)
            {
                current = p;
            }
            else
            {
                next = p;
                fadePos = 0;
                fading.store (true, std::memory_order_relaxed);
            }
        }
    }

    if (current == nullptr)
        return;

    if (! fading.load (std::memory_order_relaxed))
    {
        current->process (L, R, n, &params, ctx);
        return;
    }

    std::copy (L, L + n, aL.data()); std::copy (R, R + n, aR.data());
    std::copy (L, L + n, bL.data()); std::copy (R, R + n, bR.data());
    current->process (aL.data(), aR.data(), n, nullptr, ctx); // old chain: frozen parameters
    next->process (bL.data(), bR.data(), n, &params, ctx);

    // The new chain first fills its latency lines (pre-roll), then the 40 ms crossfade starts.
    const int warm = next->getTotalLatency();
    const float inv = 1.0f / (float) fadeLen;
    for (int i = 0; i < n; ++i)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (float) (fadePos + i + 1 - warm) * inv);
        L[i] = aL[(size_t) i] + (bL[(size_t) i] - aL[(size_t) i]) * t;
        R[i] = aR[(size_t) i] + (bR[(size_t) i] - aR[(size_t) i]) * t;
    }
    fadePos += n;

    if (fadePos >= fadeLen + warm)
    {
        retire (current);
        current = next;
        next = nullptr;
        fading.store (false, std::memory_order_relaxed);
    }
}

} // namespace ek
