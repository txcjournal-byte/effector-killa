#pragma once

#include <cmath>
#include <vector>
#include <juce_core/juce_core.h>

// Transposed direct form II biquad with RBJ cookbook designs
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setBypass() noexcept { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }

    void setLowPass (double fs, double f, double q)
    {
        const auto [cosw, alpha] = prep (fs, f, q);
        set ((1 - cosw) / 2, 1 - cosw, (1 - cosw) / 2, 1 + alpha, -2 * cosw, 1 - alpha);
    }

    void setHighPass (double fs, double f, double q)
    {
        const auto [cosw, alpha] = prep (fs, f, q);
        set ((1 + cosw) / 2, -(1 + cosw), (1 + cosw) / 2, 1 + alpha, -2 * cosw, 1 - alpha);
    }

    void setBandPass (double fs, double f, double q)
    {
        const auto [cosw, alpha] = prep (fs, f, q);
        set (alpha, 0, -alpha, 1 + alpha, -2 * cosw, 1 - alpha);
    }

    void setPeak (double fs, double f, double q, double gainDb)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto [cosw, alpha] = prep (fs, f, q);
        set (1 + alpha * A, -2 * cosw, 1 - alpha * A, 1 + alpha / A, -2 * cosw, 1 - alpha / A);
    }

    void setLowShelf (double fs, double f, double q, double gainDb)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto [cosw, alpha] = prep (fs, f, q);
        const auto s = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) - (A - 1) * cosw + s), 2 * A * ((A - 1) - (A + 1) * cosw), A * ((A + 1) - (A - 1) * cosw - s),
             (A + 1) + (A - 1) * cosw + s, -2 * ((A - 1) + (A + 1) * cosw), (A + 1) + (A - 1) * cosw - s);
    }

    void setHighShelf (double fs, double f, double q, double gainDb)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto [cosw, alpha] = prep (fs, f, q);
        const auto s = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * cosw + s), -2 * A * ((A - 1) + (A + 1) * cosw), A * ((A + 1) + (A - 1) * cosw - s),
             (A + 1) - (A - 1) * cosw + s, 2 * ((A - 1) - (A + 1) * cosw), (A + 1) - (A - 1) * cosw - s);
    }

private:
    static std::pair<double, double> prep (double fs, double f, double q)
    {
        const auto w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (1.0, fs * 0.45, f) / fs;
        return { std::cos (w0), std::sin (w0) / (2.0 * q) };
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
};

// Fixed-length delay line (length set in prepare, no allocation while processing)
struct DelayLine
{
    std::vector<float> data;
    int pos = 0;

    void prepare (int maxDelay) { data.assign ((size_t) juce::jmax (1, maxDelay + 1), 0.0f); pos = 0; }
    void reset() { std::fill (data.begin(), data.end(), 0.0f); pos = 0; }

    // push a sample and read the one written `delay` samples ago
    inline float process (float x, int delay) noexcept
    {
        const auto size = (int) data.size();
        data[(size_t) pos] = x;
        auto readPos = pos - juce::jlimit (0, size - 1, delay);
        if (readPos < 0) readPos += size;
        const auto y = data[(size_t) readPos];
        if (++pos >= size) pos = 0;
        return y;
    }
};

inline float onePole (double seconds, double fs)
{
    return (float) std::exp (-1.0 / (juce::jmax (1.0e-5, seconds) * fs));
}

inline float follow (float env, float in, float attack, float release) noexcept
{
    return in + (env - in) * (in > env ? attack : release);
}
