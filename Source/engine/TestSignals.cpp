#include "TestSignals.h"

namespace ek
{
namespace TestSignals
{
Kind forSource (Source s)
{
    switch (s)
    {
        case Source::Vox:     return Kind::Vocal;
        case Source::Bass808: return Kind::Sine55;
        case Source::Drums:   return Kind::DrumLoop;
        case Source::FX:      return Kind::PinkNoise;
        case Source::Bus:     return Kind::Bus;
        case Source::Melody:
        case Source::Count:
        default:              return Kind::Melody;
    }
}

static void pink (double, float* out, int n, NoiseGen& g)
{
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (int i = 0; i < n; ++i)
    {
        const float w = g.next();
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        out[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.11f;
        b6 = w * 0.115926f;
    }
}

static void drums (double sr, float* L, float* R, int n, NoiseGen& g)
{
    const int beat = (int) (sr * 60.0 / 140.0); // 140 BPM
    const int eighth = beat / 2;
    float kickPh = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const int posInBar = i % (beat * 4);
        const int tk = posInBar % (beat * 2); // kick on 1 and 3
        const int ts = (posInBar + beat) % (beat * 2); // snare on 2 and 4
        const int th = posInBar % eighth;
        const float tkS = (float) tk / (float) sr, tsS = (float) ts / (float) sr, thS = (float) th / (float) sr;
        const float kf = 45.0f + 120.0f * std::exp (-tkS * 30.0f);
        kickPh += kf / (float) sr;
        const float kick = std::sin (kTwoPi * kickPh) * std::exp (-tkS * 7.0f);
        const float nz = g.next();
        const float snare = (nz * 0.6f + std::sin (kTwoPi * 190.0f * tsS) * 0.4f) * std::exp (-tsS * 18.0f);
        const float hat = g.next() * std::exp (-thS * 60.0f) * 0.25f;
        const float s = 0.7f * kick + 0.45f * snare;
        L[i] = s + hat * 0.8f;
        R[i] = s + hat;
    }
}

void generate (Kind k, double sr, float* L, float* R, int n, uint64_t seed)
{
    NoiseGen g; g.state = (uint32_t) (seed * 2654435761u + 12345u);
    switch (k)
    {
        case Kind::Sine55:
        {
            // 808-like: 55 Hz sine notes with a slow decay, one per beat
            const int beat = (int) (sr * 60.0 / 140.0);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) (i % (beat * 2)) / (float) sr;
                const float v = std::sin (kTwoPi * 55.0f * (float) i / (float) sr) * (0.25f + 0.55f * std::exp (-t * 2.0f));
                L[i] = R[i] = v;
            }
            break;
        }
        case Kind::PinkNoise:
            pink (sr, L, n, g);
            pink (sr, R, n, g);
            break;
        case Kind::DrumLoop:
            drums (sr, L, R, n, g);
            break;
        case Kind::Vocal:
        {
            // formant noise: glottal-ish pulse train + noise through vowel formants, syllable envelope
            Svf f1, f2, f3;
            float ph = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) sr;
                const float vowel = 0.5f + 0.5f * std::sin (kTwoPi * 0.7f * t);
                f1.set (lerp (300.0f, 750.0f, vowel), 8.0f, (float) sr);
                f2.set (lerp (2200.0f, 1100.0f, vowel), 10.0f, (float) sr);
                f3.set (2800.0f, 12.0f, (float) sr);
                ph += (180.0f + 20.0f * std::sin (kTwoPi * 5.0f * t)) / (float) sr;
                if (ph >= 1.0f) ph -= 1.0f;
                const float src = (ph < 0.08f ? 1.0f : 0.0f) - 0.08f + 0.15f * g.next();
                const float v = f1.bpNorm (src) + 0.7f * f2.bpNorm (src) + 0.4f * f3.bpNorm (src);
                const float env = std::pow (std::max (0.0f, std::sin (kPi * std::fmod (t * 3.0f, 1.0f))), 0.5f);
                L[i] = R[i] = v * env * 0.6f;
            }
            break;
        }
        case Kind::Melody:
        {
            // soft saw chord (A minor) with plucky envelope and a little stereo detune
            const float freqs[3] = { 220.0f, 261.63f, 329.63f };
            float phL[3] {}, phR[3] {};
            OnePole lpL, lpR; lpL.setCutoff (3000.0f, (float) sr); lpR.setCutoff (3000.0f, (float) sr);
            const int note = (int) (sr * 60.0 / 140.0);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) (i % note) / (float) sr;
                float l = 0.0f, r = 0.0f;
                for (int k2 = 0; k2 < 3; ++k2)
                {
                    phL[k2] += freqs[k2] / (float) sr; if (phL[k2] >= 1.0f) phL[k2] -= 1.0f;
                    phR[k2] += freqs[k2] * 1.003f / (float) sr; if (phR[k2] >= 1.0f) phR[k2] -= 1.0f;
                    l += 2.0f * phL[k2] - 1.0f;
                    r += 2.0f * phR[k2] - 1.0f;
                }
                const float env = 0.3f + 0.7f * std::exp (-t * 4.0f);
                L[i] = lpL.lp (l) * env * 0.22f;
                R[i] = lpR.lp (r) * env * 0.22f;
            }
            break;
        }
        case Kind::Bus:
        {
            std::vector<float> a ((size_t) n), b ((size_t) n);
            drums (sr, L, R, n, g);
            generate (Kind::Sine55, sr, a.data(), b.data(), n, seed + 1);
            for (int i = 0; i < n; ++i) { L[i] = 0.6f * L[i] + 0.5f * a[(size_t) i]; R[i] = 0.6f * R[i] + 0.5f * b[(size_t) i]; }
            generate (Kind::Melody, sr, a.data(), b.data(), n, seed + 2);
            for (int i = 0; i < n; ++i) { L[i] += 0.5f * a[(size_t) i]; R[i] += 0.5f * b[(size_t) i]; }
            break;
        }
    }
}
} // namespace TestSignals
} // namespace ek
