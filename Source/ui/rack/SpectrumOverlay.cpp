#include "SpectrumOverlay.h"

namespace ek
{
// ============================================================================
// SpectrumAnalyzer
// ============================================================================
SpectrumAnalyzer::SpectrumAnalyzer()
{
    // periodic Hann window
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) fftSize);

    const float ratio = maxHz / minHz;
    for (int p = 0; p < numPoints; ++p)
        pointHz[(size_t) p] = minHz * std::pow (ratio, (float) p / (float) (numPoints - 1));

    scratchB.assign ((size_t) SpectrumFifo::kCapacity, 0.0f);
    scratchA.assign ((size_t) SpectrumFifo::kCapacity, 0.0f);
    reset();
}

void SpectrumAnalyzer::reset() noexcept
{
    for (auto& r : ring) r.fill (0.0f);
    for (auto& b : binDb) b.fill (floorDb);
    for (auto& p : pointDb) p.fill (floorDb);
    writePos = newSamples = frames = 0;
}

void SpectrumAnalyzer::push (const float* before, const float* after, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        ring[Before][(size_t) writePos] = std::isfinite (before[i]) ? before[i] : 0.0f;
        ring[After][(size_t) writePos] = std::isfinite (after[i]) ? after[i] : 0.0f;
        writePos = (writePos + 1) & (fftSize - 1);
    }
    newSamples = std::min (newSamples + n, 1 << 24);
}

int SpectrumAnalyzer::pull (SpectrumFifo& fifo) noexcept
{
    int total = 0;
    for (;;)
    {
        const int got = fifo.pop (scratchB.data(), scratchA.data(), (int) scratchB.size());
        if (got <= 0) break;
        push (scratchB.data(), scratchA.data(), got);
        total += got;
    }
    return total;
}

bool SpectrumAnalyzer::analyse (int hop) noexcept
{
    if (newSamples < std::max (1, hop))
        return false;
    newSamples = 0;

    // amplitude of a full-scale sine -> 0 dB (Hann coherent gain 0.5, one-sided spectrum)
    const float norm = 4.0f / (float) fftSize;
    const bool first = frames == 0;

    for (int s = 0; s < 2; ++s)
    {
        for (int i = 0; i < fftSize; ++i)
            work[(size_t) i] = ring[(size_t) s][(size_t) ((writePos + i) & (fftSize - 1))] * window[(size_t) i];
        std::fill (work.begin() + fftSize, work.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (work.data(), true);

        auto& bins = binDb[(size_t) s];
        for (int b = 0; b < numBins; ++b)
        {
            const float mag = work[(size_t) b] * norm;
            const float db = std::max (floorDb, 20.0f * std::log10 (mag + 1.0e-9f));
            float& v = bins[(size_t) b];
            if (first) v = db;
            else v += (db - v) * (1.0f - (db > v ? attack : release));
        }
    }
    ++frames;
    computePoints();
    return true;
}

int SpectrumAnalyzer::getPeakBin (int stream) const noexcept
{
    const auto& bins = binDb[(size_t) stream];
    int best = 1;
    for (int b = 1; b < numBins; ++b)
        if (bins[(size_t) b] > bins[(size_t) best]) best = b;
    return best;
}

void SpectrumAnalyzer::computePoints() noexcept
{
    const float binHz = (float) (sampleRate / fftSize);
    const float halfStep = std::pow (maxHz / minHz, 0.5f / (float) (numPoints - 1));
    for (int s = 0; s < 2; ++s)
    {
        const auto& bins = binDb[(size_t) s];
        for (int p = 0; p < numPoints; ++p)
        {
            const float f = pointHz[(size_t) p];
            const float lo = f / halfStep / binHz, hi = f * halfStep / binHz;
            float v = floorDb;
            if (f >= (float) sampleRate * 0.5f)
            {
                v = floorDb;
            }
            else if (hi - lo < 1.0f)
            {
                // narrower than one bin (low frequencies): interpolate
                const float pos = juce::jlimit (0.0f, (float) (numBins - 2), f / binHz);
                const int b = (int) pos;
                const float t = pos - (float) b;
                v = bins[(size_t) b] + (bins[(size_t) b + 1] - bins[(size_t) b]) * t;
            }
            else
            {
                const int b0 = juce::jlimit (0, numBins - 1, (int) std::ceil (lo));
                const int b1 = juce::jlimit (0, numBins - 1, (int) std::floor (hi));
                for (int b = b0; b <= b1; ++b) v = std::max (v, bins[(size_t) b]);
            }
            pointDb[(size_t) s][(size_t) p] = v;
        }
    }
}

// ============================================================================
// SpectrumOverlay
// ============================================================================
SpectrumOverlay::SpectrumOverlay (SlotTap& t) : tap (t)
{
    analyzer.setSampleRate (tap.getSampleRate());
}

SpectrumOverlay::~SpectrumOverlay()
{
    if (slot >= 0 && tap.getAnalyzedSlot() == slot)
        tap.setAnalyzedSlot (-1);
}

void SpectrumOverlay::setSlot (int newSlot)
{
    newSlot = (newSlot >= 0 && newSlot < kNumSlots) ? newSlot : -1;
    if (newSlot == slot) return;
    slot = newSlot;
    tap.setAnalyzedSlot (slot);
    tap.getFifo().discardAll(); // stale samples of the previous unit
    analyzer.reset();
}

bool SpectrumOverlay::update()
{
    analyzer.setSampleRate (tap.getSampleRate());
    if (slot < 0)
    {
        tap.getFifo().discardAll();
        return false;
    }
    analyzer.pull (tap.getFifo());
    return analyzer.analyse();
}

void SpectrumOverlay::paint (juce::Graphics& g, juce::Rectangle<float> area, float opacity) const
{
    if (slot < 0) return;
    paintSpectrum (g, area, analyzer, opacity);
}

void SpectrumOverlay::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> area, const SpectrumAnalyzer& a, float opacity)
{
    if (! a.hasData() || area.isEmpty() || opacity <= 0.0f) return;

    const float logRange = std::log (SpectrumAnalyzer::maxHz / SpectrumAnalyzer::minHz);
    const float nyquist = (float) a.getSampleRate() * 0.5f;
    auto xOf = [&] (float hz) { return area.getX() + area.getWidth() * std::log (hz / SpectrumAnalyzer::minHz) / logRange; };
    auto yOf = [&] (float db, float hz)
    {
        // +3 dB/octave display tilt around 1 kHz: typical music reads as roughly flat
        const float d = db + 3.0f * std::log2 (hz / 1000.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (d - bottomDb) / (topDb - bottomDb));
        return area.getBottom() - t * area.getHeight();
    };

    // faint decade ticks (100 Hz, 1 kHz, 10 kHz)
    g.setColour (juce::Colours::white.withAlpha (0.05f * opacity));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) std::round (xOf (hz)), area.getY() + area.getHeight() * 0.35f, area.getBottom());

    auto makePath = [&] (int stream, bool closed)
    {
        juce::Path p;
        bool started = false;
        float lastX = area.getX();
        for (int i = 0; i < SpectrumAnalyzer::numPoints; ++i)
        {
            const float hz = a.getPointHz (i);
            if (hz >= nyquist) break;
            const float x = xOf (hz), y = yOf (a.getPointDb (stream, i), hz);
            if (! started)
            {
                if (closed) { p.startNewSubPath (x, area.getBottom()); p.lineTo (x, y); }
                else p.startNewSubPath (x, y);
                started = true;
            }
            else p.lineTo (x, y);
            lastX = x;
        }
        if (closed && started) { p.lineTo (lastX, area.getBottom()); p.closeSubPath(); }
        return p;
    };

    const auto before = beforeColour(), after = afterColour();

    // BEFORE: faint grey fill + thin line
    {
        auto fill = makePath (SpectrumAnalyzer::Before, true);
        g.setGradientFill (juce::ColourGradient (before.withAlpha (0.10f * opacity), 0.0f, area.getY(),
                                                 before.withAlpha (0.0f), 0.0f, area.getBottom(), false));
        g.fillPath (fill);
        g.setColour (before.withAlpha (0.42f * opacity));
        g.strokePath (makePath (SpectrumAnalyzer::Before, false), juce::PathStrokeType (1.0f, juce::PathStrokeType::curved));
    }

    // AFTER: ice-blue glow + line
    {
        auto fill = makePath (SpectrumAnalyzer::After, true);
        g.setGradientFill (juce::ColourGradient (after.withAlpha (0.20f * opacity), 0.0f, area.getY(),
                                                 after.withAlpha (0.0f), 0.0f, area.getBottom(), false));
        g.fillPath (fill);
        auto line = makePath (SpectrumAnalyzer::After, false);
        g.setColour (after.withAlpha (0.16f * opacity));
        g.strokePath (line, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved));
        g.setColour (after.withAlpha (0.85f * opacity));
        g.strokePath (line, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));
    }
}

} // namespace ek
