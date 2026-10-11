#pragma once

// SPECTRAL VIEW (DESIGN-RACK.md 12.4): a subtle BEFORE / AFTER spectrum of one rack unit, drawn
// behind the unit's knobs while it is hovered.
//
//   audio thread:  Chain -> SlotTap -> SpectrumFifo (lock-free, see engine/SlotTap.h)
//   UI timer:      SpectrumOverlay::update() drains the FIFO and runs the FFT (2 x 2048 points, Hann,
//                  ~30 Hz – cheap enough for the message thread), smoothing, log-frequency curve.
//   paint:         SpectrumOverlay::paint (g, area) – grey = before the unit, ice blue = after.
//
// One SpectrumOverlay per processor (the SlotTap FIFO has a single reader). Usage from the rack UI:
//   overlay.setSlot (hoveredSlot);            // also calls processor.setAnalyzedSlot(); -1 = off
//   in the 30 Hz timer: if (overlay.update()) unit.repaint();
//   in the unit's paint, before the knobs: overlay.paint (g, knobArea.toFloat());

#include "../../engine/SlotTap.h"
#include <juce_graphics/juce_graphics.h>

namespace ek
{
// FFT analysis of the before / after streams. No GUI dependencies – unit-testable.
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr int numPoints = 192;          // log-spaced display points
    static constexpr float minHz = 20.0f, maxHz = 20000.0f;
    static constexpr float floorDb = -100.0f;

    enum Stream { Before = 0, After = 1 };

    SpectrumAnalyzer();

    void setSampleRate (double sr) noexcept { sampleRate = sr > 0.0 ? sr : 44100.0; }
    double getSampleRate() const noexcept { return sampleRate; }
    void reset() noexcept;

    // Appends mono samples of both streams to the analysis window.
    void push (const float* before, const float* after, int n) noexcept;
    // Drains the FIFO (all of it) into the analysis window. Returns the number of samples read.
    int pull (SpectrumFifo& fifo) noexcept;

    // Runs the FFT when at least `hop` new samples arrived since the last analysis.
    // Returns true when the spectra changed.
    bool analyse (int hop = fftSize / 2) noexcept;

    // Smoothed magnitude (dBFS of a full-scale sine) per FFT bin / per display point.
    float getBinDb (int stream, int bin) const noexcept { return binDb[(size_t) stream][(size_t) bin]; }
    float getPointDb (int stream, int point) const noexcept { return pointDb[(size_t) stream][(size_t) point]; }
    float getPointHz (int point) const noexcept { return pointHz[(size_t) point]; }
    int getPeakBin (int stream) const noexcept;
    float binToHz (int bin) const noexcept { return (float) (bin * sampleRate / fftSize); }
    bool hasData() const noexcept { return frames > 0; }

    // Smoothing: attack (rise) and release (fall) factors per analysis frame (0 = none, ->1 = slow).
    float attack = 0.35f, release = 0.82f;

private:
    void computePoints() noexcept;

    double sampleRate = 44100.0;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> window {};
    std::array<std::array<float, fftSize>, 2> ring {};
    int writePos = 0, newSamples = 0, frames = 0;
    std::array<float, fftSize * 2> work {};
    std::array<std::array<float, numBins>, 2> binDb {};
    std::array<std::array<float, numPoints>, 2> pointDb {};
    std::array<float, numPoints> pointHz {};
    std::vector<float> scratchB, scratchA;
};

// UI wrapper: owns an analyzer bound to the processor's SlotTap and paints the two curves.
class SpectrumOverlay
{
public:
    explicit SpectrumOverlay (SlotTap& tap);
    ~SpectrumOverlay();

    // Selects the analysed unit (-1 = off) and clears the old curves.
    void setSlot (int slot);
    int getSlot() const noexcept { return slot; }

    // Message thread (UI timer, ~30 Hz). Returns true when a repaint is worthwhile.
    bool update();

    // Paints the BEFORE (grey) / AFTER (ice blue #9fc3ff) curves into `area`. Nothing when off or
    // no data yet. `opacity` scales everything (fade in / out on hover).
    void paint (juce::Graphics& g, juce::Rectangle<float> area, float opacity = 1.0f) const;

    // Paints an analyzer's curves (also used by the snapshot tool and tests).
    static void paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area, const SpectrumAnalyzer& a, float opacity = 1.0f);

    SpectrumAnalyzer& getAnalyzer() noexcept { return analyzer; }

    static constexpr float topDb = -6.0f, bottomDb = -90.0f;
    static juce::Colour beforeColour() { return juce::Colour (0xffb4b8c0); }
    static juce::Colour afterColour() { return juce::Colour (0xff9fc3ff); }

private:
    SlotTap& tap;
    SpectrumAnalyzer analyzer;
    int slot = -1;
};

} // namespace ek
