#include "Effect.h"

namespace ek
{
static float skewExponent (const ParamSpec& p) noexcept
{
    if (p.centre <= p.min || p.centre >= p.max) return 1.0f;
    return std::log (0.5f) / std::log ((p.centre - p.min) / (p.max - p.min));
}

float ParamSpec::toReal (float n) const noexcept
{
    n = clamp01 (n);
    if (isDiscrete())
        return std::round (min + n * (max - min));
    const float skew = skewExponent (*this);
    if (skew != 1.0f && n > 0.0f)
        n = std::exp (std::log (n) / skew);
    return min + (max - min) * n;
}

float ParamSpec::fromReal (float real) const noexcept
{
    if (max <= min) return 0.0f;
    real = juce::jlimit (min, max, real);
    float n = (real - min) / (max - min);
    if (isDiscrete()) return n;
    const float skew = skewExponent (*this);
    if (skew != 1.0f && n > 0.0f)
        n = std::pow (n, skew);
    return clamp01 (n);
}

juce::String ParamSpec::format (float v) const
{
    switch (unit)
    {
        case Unit::Hz:
            return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                : juce::String (juce::roundToInt (v)) + " Hz";
        case Unit::Db:      return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
        case Unit::Ms:      return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
        case Unit::Seconds: return juce::String (v, v < 10.0f ? 1 : 0) + " s";
        case Unit::Percent: return juce::String (juce::roundToInt (v * 100.0f)) + " %";
        case Unit::Ratio:   return juce::String (v, 1) + ":1";
        case Unit::Bits:    return juce::String (juce::roundToInt (v)) + " BIT";
        case Unit::Octaves: return juce::String (v, 1) + " OCT";
        case Unit::Toggle:  return v > 0.5f ? "ON" : "OFF";
        case Unit::Voices:  return juce::String (juce::roundToInt (v)) + " VOICES";
        case Unit::Stages:  return juce::String (juce::roundToInt (v)) + " STAGES";
        case Unit::Pattern: return "PATTERN " + juce::String (juce::roundToInt (v) + 1);
        case Unit::Choice:
        case Unit::Sync:
        {
            const int idx = juce::jlimit (0, juce::jmax (0, choices.size() - 1), juce::roundToInt (v - min));
            return choices[idx];
        }
        case Unit::None:
        default:            return juce::String (v, 2);
    }
}

Effect::Effect (EffectType t) : infoPtr (&effectInfo (t))
{
    for (int i = 0; i < kNumParams; ++i)
        norm[(size_t) i] = infoPtr->params[(size_t) i].defaultNorm();
}

// ---------------------------------------------------------------------------
void NonlinearHost::prepare (const PrepareSpec& s, bool useOversampling, bool useLowKeep)
{
    lowKeep = useLowKeep;
    os.prepare (useOversampling ? s.osFactorLog2 : 0, s.maxBlock);
    const int lat = os.getLatency();
    dryDelay.prepare (lat);
    lowDelay.prepare (lat);
    for (auto* v : { &dryL, &dryR, &lowL, &lowR })
        v->assign ((size_t) s.maxBlock, 0.0f);
    xL.prepare (s.sampleRate);
    xR.prepare (s.sampleRate);
    mix.reset (s.sampleRate, 20.0f, 1.0f);
}

void NonlinearHost::reset()
{
    os.reset();
    xL.reset(); xR.reset();
    dryDelay.reset(); lowDelay.reset();
    mix.snap (mix.getTarget());
}

} // namespace ek
