#pragma once

// The complete audio engine behind the plugin:
//   ANTENNA IN -> [AV modulation + macros] -> Rack -> trim -> BLEND -> AUTO TRACKING -> RF OUT
//   -> OFF AIR (level-matched original) -> POCKET TV (monitor only)
// Everything runs lock-free; non-parameter program data arrives through SnapshotExchange.

#include "Rack.h"
#include "Macro.h"
#include "Modulation.h"
#include "LoudnessMatch.h"

namespace ek
{
// Single-producer (message thread) / single-consumer (audio thread) exchange of immutable snapshots.
template <typename T>
class SnapshotExchange
{
public:
    ~SnapshotExchange()
    {
        delete pending.exchange (nullptr);
        delete current;
        collect();
    }

    void publish (std::unique_ptr<T> s) // message thread
    {
        delete pending.exchange (s.release(), std::memory_order_acq_rel);
        collect();
    }

    const T* acquire() noexcept // audio thread
    {
        if (fifo.getFreeSpace() > 0)
        {
            if (T* p = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                if (current != nullptr)
                {
                    const auto scope = fifo.write (1);
                    if (scope.blockSize1 > 0) retired[(size_t) scope.startIndex1] = current;
                    else retired[(size_t) scope.startIndex2] = current;
                }
                current = p;
            }
        }
        return current;
    }

    void collect() // message thread
    {
        for (;;)
        {
            const auto scope = fifo.read (1);
            if (scope.blockSize1 > 0) delete retired[(size_t) scope.startIndex1];
            else if (scope.blockSize2 > 0) delete retired[(size_t) scope.startIndex2];
            else break;
        }
    }

private:
    std::atomic<T*> pending { nullptr };
    T* current = nullptr;
    juce::AbstractFifo fifo { 32 };
    std::array<T*, 32> retired {};
};

struct ProgramExtras
{
    Source source = Source::Melody;
    float trimDb = 0.0f;
    std::vector<MacroMapping> maps;
    ModState mod;
    juce::Point<float> pathMean { 0.5f, 0.5f };
};

struct GlobalParams
{
    float inGainDb = 0.0f, outGainDb = 0.0f, blend = 1.0f;
    bool autoTracking = false, pocketTv = false, offAir = false;
};

struct EngineInput
{
    RackParams base;
    std::array<float, kNumMacros> macros { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    GlobalParams global;
    std::array<bool, kNumMacros> macroModSuspended {}; // host is automating this macro
};

struct EngineMeters
{
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, outPeak { 0.0f };
    std::atomic<int> clipCount { 0 };
    std::atomic<float> dotX { 0.5f }, dotY { 0.5f }, glitch { 0.0f };
    std::atomic<bool> modActive { false };
    std::atomic<int> cornerHits { 0 };
    std::array<std::atomic<float>, kNumMacros> macroValue {}; // effective macro values (incl. modulation)
    std::atomic<float> autoGainDb { 0.0f }, offAirMatchDb { 0.0f };
    std::atomic<double> bpm { 140.0 }, ppq { 0.0 };
    std::atomic<bool> playing { false };
    std::atomic<float> transient { 0.0f }; // for KNOCK flashes on the TV
};

class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlock, int osFactorLog2); // audio stopped
    void setOversampling (int osFactorLog2);                          // message thread
    void setRackConfig (const RackConfig& cfg) { rack.setConfig (cfg); }
    void setExtras (const ProgramExtras& e);                          // message thread
    void collectGarbage() { rack.collectGarbage(); extras.collect(); }

    void process (juce::AudioBuffer<float>& buffer, const EngineInput& in, const ProcessContext& hostCtx) noexcept;

    int getLatency() const noexcept { return rack.getFixedLatency(); }
    Rack& getRack() noexcept { return rack; }
    const Rack& getRack() const noexcept { return rack; }
    EngineMeters meters;

    // Last effective rack parameters (for the UI, written on the audio thread).
    std::array<std::array<std::atomic<float>, kNumParams>, kNumSlots> effectiveParams {};

    static constexpr float offAirFadeMs = 20.0f;

private:
    void processChunk (float* L, float* R, int n, const EngineInput& in, const ProcessContext& ctx) noexcept;

    Rack rack;
    SnapshotExchange<ProgramExtras> extras;
    ProgramExtras defaultExtras;
    ModulationEngine modulation;
    LoudnessMeter inMeter, rackMeter, origMeter, finalMeter;
    AutoGain autoGain;
    double sampleRate = 44100.0;

    Smoothed inGain, outGain, blend, trim;
    LinearRamp offAirRamp, pocketRamp;
    std::array<DelayLine, 2> dryDelay, origDelay;
    std::vector<float> dryL, dryR, origL, origR, tmpR;

    // POCKET TV
    std::array<Biquad, 2> pocketHp, pocketLp;
    float pocketEnv = 0.0f, pocketAtt = 0.0f, pocketRel = 0.0f;
    float matchDb = 0.0f;
    EnvelopeFollower transientFast, transientSlow;
};
} // namespace ek
