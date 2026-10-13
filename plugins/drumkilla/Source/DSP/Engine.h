#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Filters.h"

// Snapshot of all parameter values for one block
struct EngineParams
{
    float inputGainDb = 0.0f;
    bool phaseInvert = false;
    float crossoverHz = 120.0f;
    bool subMono = true, subCut = true;
    float drive = 25.0f;            // 0..100 %
    int satMode = 1;                // 0 Tape, 1 Tube, 2 Foldback
    float focusDb = 3.0f;
    float focusHz = 550.0f;         // centre of the FOCUS bell (808 ~550 Hz, snare body ~200 Hz, crack ~3 kHz)
    float duckDepth = 50.0f;        // 0..100 %
    float duckReleaseMs = 45.0f;
    float clipDriveDb = 4.0f, clipKnee = 0.8f, ceilingDb = -0.2f;
    bool phone = false;
    float outputDb = 0.0f;
    bool bypass = false;
    float hit = 70.0f;              // 0..100 %: how much drive and clip follow the hit of each note
};

// Signal chain (everything between the input gain and the downsampler runs at 4x):
// input gain -> phase -> LR4 crossover -> sub: mono, 28 Hz HPF, sidechain ducking
//                                     -> mid/high: focus bell, drive, saturator (tape / tube / foldback)
// -> sum -> soft clipper with knee -> downsample -> phone preview -> output trim -> hard limit -0.1 dBFS
class Engine
{
public:
    static constexpr int oversamplingStages = 2;     // 4x

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, int numChannels, const juce::AudioBuffer<float>* sidechain,
                  const EngineParams& params);

    int getLatencySamples() const noexcept { return latency; }

    // the saturation curves (public for tests)
    static float saturate (int mode, float x) noexcept;
    static float softClip (float x, float knee) noexcept;

    // metering (audio thread writes, UI reads)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, clipDb { 0.0f }, duckDb { 0.0f };
    std::atomic<bool> sidechainActive { false };

    // waveform scope: peak of the (delayed) input and the output, one value every 5 ms
    static constexpr int scopeSize = 1024;
    std::array<std::atomic<float>, scopeSize> scopeIn {}, scopeOut {};
    std::atomic<int> scopeWrite { 0 };

private:
    struct Channel
    {
        DelayLine lookahead;
        Biquad subCut1, subCut2, focus, phoneHp1, phoneHp2, phoneLp1, phoneLp2;
        float dcX = 0.0f, dcY = 0.0f;
        DelayLine dryDelay;
    };

    void updateFilters (float crossover, float focusDb, float focusHz);

    double fs = 44100.0, osFs = 176400.0;
    int maxBlock = 512, latency = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::LinkwitzRileyFilter<float> crossover;
    std::array<Channel, 2> ch;
    juce::AudioBuffer<float> dryBuf;
    std::vector<float> duckGain, hitEnv;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGain, outGain, clipGain, ceiling;
    juce::SmoothedValue<float> driveSmoothed, focusSmoothed, focusHzSmoothed, crossoverSmoothed, bypassAmt;
    float cCrossover = -1.0f, cFocus = -999.0f, cFocusHz = -1.0f;

    float scEnv = 0.0f;
    float onFast = 0.0f, onSlow = 0.0f, hitLevel = 0.0f;
    int onHold = 0, lookaheadSamples = 0;
    bool firstBlock = true;
    int scSilentSamples = 1 << 30;
    int scopeCount = 0, scopeLength = 240;
    float scopeInMax = 0.0f, scopeOutMax = 0.0f;
};
