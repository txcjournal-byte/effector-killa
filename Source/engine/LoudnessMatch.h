#pragma once

// Loudness measurement (approximate K-weighting, ~3 s sliding window), AUTO TRACKING auto-gain,
// and the offline loudness estimator used by KILL / BOOTLEG.

#include "Preset.h"

namespace ek
{
class LoudnessMeter
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const float* L, const float* R, int n) noexcept;

    float getLoudnessDb() const noexcept { return loudnessDb; }  // short-term (window) loudness
    bool hasSignal() const noexcept { return loudnessDb > -60.0f; }
    float getBlockDb() const noexcept { return lastBlockDb; }

    static constexpr int kBlocks = 30;       // 30 x 100 ms = 3 s window
private:
    std::array<Biquad, 2> shelf, hp;
    std::array<double, kBlocks> blocks {};
    int blockLen = 4410, blockPos = 0, blockIndex = 0, filled = 0;
    double acc = 0.0;
    float loudnessDb = -120.0f, lastBlockDb = -120.0f;
};

// AUTO TRACKING: slowly matches the output loudness to the input loudness.
class AutoGain
{
public:
    void prepare (double sampleRate);
    void reset();
    // Updates the target from the meters (call once per chunk) and applies the gain.
    void process (float* L, float* R, int n, const LoudnessMeter& in, const LoudnessMeter& out, bool enabled) noexcept;
    float getGainDb() const noexcept { return currentDb; }

    static constexpr float maxDb = 12.0f;
private:
    double sampleRate = 44100.0;
    float targetDb = 0.0f, currentDb = 0.0f;
    float coeffPerSample = 0.0f;
    LinearRamp enableRamp;
};

// Offline estimate of the loudness change of a program on the source's test signal.
// Returns the trim (dB) that brings the output back to the input loudness.
float estimateProgramTrimDb (const ProgramState& program, double sampleRate, float seconds = 1.5f);
// Measures input and output loudness of a program (after its trim) – used by the tests.
void measureProgramLoudness (const ProgramState& program, double sampleRate, float seconds, float& inDb, float& outDb,
                             bool applyTrim, bool* sawNaN = nullptr, int osFactorLog2 = 0);

} // namespace ek
