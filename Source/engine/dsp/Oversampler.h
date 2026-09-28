#pragma once

#include "DspCommon.h"
#include <memory>

namespace ek
{
// Stereo oversampling wrapper around juce::dsp::Oversampling (linear-phase FIR halfbands,
// integer latency so dry/wet paths can be aligned exactly).
class Oversampler
{
public:
    void prepare (int factorLog2, int maxBlock)
    {
        order = juce::jlimit (0, 2, factorLog2);
        if (order == 0) { os.reset(); return; }
        os = std::make_unique<juce::dsp::Oversampling<float>> (
            2, (size_t) order, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, false, true);
        os->initProcessing ((size_t) maxBlock);
    }

    void reset() noexcept { if (os) os->reset(); }
    int getLatency() const noexcept { return os ? (int) std::lround (os->getLatencyInSamples()) : 0; }
    int getFactor() const noexcept { return 1 << order; }

    // fn (float* L, float* R, int numOversampledSamples, int factor)
    template <typename Fn>
    void process (float* L, float* R, int n, Fn&& fn)
    {
        if (! os) { fn (L, R, n, 1); return; }
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = os->processSamplesUp (block);
        fn (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), getFactor());
        os->processSamplesDown (block);
    }

    static int latencyFor (int factorLog2)
    {
        if (factorLog2 <= 0) return 0;
        Oversampler tmp;
        tmp.prepare (factorLog2, 64);
        return tmp.getLatency();
    }

private:
    int order = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
};
} // namespace ek
