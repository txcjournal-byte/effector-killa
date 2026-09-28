#pragma once

// AV modulation (audio thread): AV2 SCRIBBLE path follower and AV3 SCREENSAVER bouncing logo.
// AV1 REMOTE writes the target parameters directly from the UI and produces no audio-side offset.

#include "Preset.h"

namespace ek
{
struct ModOutput
{
    bool active = false;
    float offX = 0.0f, offY = 0.0f;  // offsets to add to targetX / targetY (normalised)
    float dotX = 0.5f, dotY = 0.5f;  // position of the moving dot / logo (0..1, y up)
    float glitch = 0.0f;             // AV3 corner-hit glitch envelope (0..1)
};

class ModulationEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    // Advances the modulation by n samples. mean = precomputed ModState::pathMean().
    ModOutput process (const ModState& st, juce::Point<float> mean, const ProcessContext& ctx, int n) noexcept;

    int getCornerHits() const noexcept { return cornerHits; }

    // Deterministic helpers shared with the UI and the tests.
    static juce::Point<float> pathPosition (const std::vector<juce::Point<float>>& path, double phase01) noexcept;
    static juce::Point<float> screensaverPosition (double beats, double crossingBeats) noexcept;
    static double beatsPerBar (const ProcessContext& ctx) noexcept;

    static constexpr float glitchAmount = 0.35f; // CRASH OUT boost at a corner hit

private:
    double sampleRate = 44100.0;
    double freeBeats = 0.0;      // internal clock when there is no transport
    double lastScribbleBeats = 0.0;
    float glitchEnv = 0.0f;
    int cornerHits = 0;
    juce::int64 lastCrossing = -1;
};
} // namespace ek
