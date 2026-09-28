#pragma once

// 8-slot effect rack.
//
// The rack's structure (effect types, order, M/S modes, SCREENING) lives in an immutable Chain object.
// Structural changes build a new Chain off the audio thread, hand it over lock-free and crossfade
// old -> new over 40 ms. Retired chains are sent back through a FIFO and deleted on the message thread.
//
// Latency reported to the host is FIXED (8 x oversampling latency + Red Line lookahead) regardless of
// the rack content; every chain pads itself up to that value.

#include "Effect.h"
#include <atomic>

namespace ek
{
enum class MSMode : int { Stereo = 0, Mid, Side };

struct SlotConfig
{
    EffectType type = EffectType::None;
    MSMode ms = MSMode::Stereo;
    bool operator== (const SlotConfig& o) const noexcept { return type == o.type && ms == o.ms; }
};

struct RackConfig
{
    std::array<SlotConfig, kNumSlots> slots {};
    int screening = -1; // -1 = off, else slot index to listen to
    bool operator== (const RackConfig& o) const noexcept { return slots == o.slots && screening == o.screening; }
    bool operator!= (const RackConfig& o) const noexcept { return ! (*this == o); }
};

struct SlotParams
{
    std::array<float, kNumParams> p {};
    float mix = 1.0f;
    bool pause = false;
};

struct RackParams
{
    std::array<SlotParams, kNumSlots> slots {};
};

// Immutable processing structure (owned by exactly one thread at a time).
class Chain
{
public:
    Chain (const RackConfig& cfg, const PrepareSpec& spec, int fixedLatency);

    // params == nullptr -> keep last parameters (used while fading out)
    void process (float* L, float* R, int n, const RackParams* params, const ProcessContext& ctx) noexcept;

    const RackConfig& getConfig() const noexcept { return config; }
    int getActualLatency() const noexcept { return actualLatency; }
    int getTotalLatency() const noexcept { return actualLatency + pad.getDelay(); }
    int getSlotLatency (int slot) const noexcept { return slots[(size_t) slot].latency; }
    Effect* getEffect (int slot) const noexcept { return slots[(size_t) slot].fx.get(); }

private:
    struct Slot
    {
        std::unique_ptr<Effect> fx;
        MSMode ms = MSMode::Stereo;
        int latency = 0;
        LatencyDelay dryDelay, otherDelay;
        Smoothed wet;
        bool running = true;
        bool paramsSet = false;
    };

    void processSlot (Slot& s, int index, float* L, float* R, int n, const RackParams* params, const ProcessContext& ctx) noexcept;

    RackConfig config;
    std::array<Slot, kNumSlots> slots;
    int lastSlot = kNumSlots - 1;
    int actualLatency = 0;
    LatencyDelay pad;
    std::vector<float> dryL, dryR, wL, wR, other;
};

class Rack
{
public:
    Rack();
    ~Rack();

    // Message thread, audio stopped. Rebuilds the chain synchronously (no crossfade).
    void prepare (double sampleRate, int osFactorLog2);

    // Message thread. Builds a new chain and schedules a 40 ms crossfade to it.
    void setConfig (const RackConfig& cfg);
    RackConfig getConfig() const;

    // Audio thread. n <= kMaxChunk.
    void process (float* L, float* R, int n, const RackParams& params, const ProcessContext& ctx) noexcept;

    // Message thread, audio may be running: switch the oversampling factor (new fixed latency).
    void setOversampling (int osFactorLog2);

    // Audio thread: configuration / latency of the chain that is currently audible.
    const RackConfig& getActiveConfig() const noexcept;
    int getActiveLatency() const noexcept;

    // Message thread: delete retired chains.
    void collectGarbage();

    int getFixedLatency() const noexcept { return fixedLatency; }
    static int computeFixedLatency (double sampleRate, int osFactorLog2);
    static int redLineLookaheadSamples (double sampleRate);

    bool isCrossfading() const noexcept { return fading.load(); }
    double getSampleRate() const noexcept { return spec.sampleRate; }
    int getOversamplingLog2() const noexcept { return spec.osFactorLog2; }

    static constexpr float crossfadeMs = 40.0f;

private:
    Chain* buildChain (const RackConfig& cfg) const;
    void retire (Chain* c) noexcept;

    PrepareSpec spec;
    int fixedLatency = 0;
    RackConfig lastConfig;
    juce::CriticalSection configLock; // message side only – never taken by the audio thread

    Chain* current = nullptr; // audio-owned
    Chain* next = nullptr;    // audio-owned while crossfading
    std::atomic<Chain*> pending { nullptr };
    std::atomic<bool> fading { false };
    int fadePos = 0, fadeLen = 1;
    std::vector<float> aL, aR, bL, bR;

    // retired chains travel audio -> message thread through this SPSC FIFO
    juce::AbstractFifo graveyardFifo { 64 };
    std::array<Chain*, 64> graveyard {};
};

} // namespace ek
