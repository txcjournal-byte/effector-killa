// Wide Body – stereo width (0–200 %), Haas delay (0–30 ms) and mono-below (0–300 Hz).
#include "AllEffects.h"

namespace ek
{
EffectInfo makeWideBodyInfo()
{
    EffectInfo i;
    i.type = EffectType::WideBody;
    i.id = "wide_body";
    i.name = "Wide Body";
    i.colour = juce::Colour (0xff5ad18a);
    i.category = Category::Stereo;
    i.params = { pPercent ("WIDTH", 1.0f, 2.0f),
                 pMs ("HAAS", 0.0f, 30.0f, 0.0f, 8.0f),
                 pHz ("MONO BELOW", 0.0f, 300.0f, 0.0f, 100.0f),
                 pUnused(), pUnused(), pUnused(), pUnused(), pUnused() };
    i.mainParam = 0;
    i.macro[Aura] = { 0.2f, 0, 0, 0, 0, 0, 0, 0 };
    i.macro[Drip] = { 0.05f, 0, 0, 0, 0, 0, 0, 0 };
    return i;
}

namespace
{
class WideBody final : public Effect
{
public:
    WideBody() : Effect (EffectType::WideBody) {}

    void onPrepare() override
    {
        haasLine.prepare ((int) std::ceil (0.031f * sr()) + 8);
        for (auto& x : xo) x.prepare (spec.sampleRate);
        width.reset (spec.sampleRate, 40.0f, 1.0f);
        haas.reset (spec.sampleRate, 80.0f, 0.0f);
        monoMix.reset (spec.sampleRate, 40.0f, 0.0f);
    }

    void reset() override
    {
        haasLine.reset();
        for (auto& x : xo) x.reset();
        for (auto* s : { &width, &haas, &monoMix }) s->snap (s->getTarget());
    }

    void applyParameters() override
    {
        width.setTarget (getReal (0));
        haas.setTarget (getReal (1) * 0.001f * sr());
        const float mb = getReal (2);
        monoMix.setTarget (mb >= 20.0f ? 1.0f : 0.0f);
        monoHz = std::max (20.0f, mb);
    }

    void process (float* L, float* R, int n, const ProcessContext&) override
    {
        for (auto& x : xo) x.setCutoff (monoHz);
        for (int i = 0; i < n; ++i)
        {
            const float w = width.next(), h = haas.next(), mm = monoMix.next();
            float loL, hiL, loR, hiR;
            xo[0].process (L[i], loL, hiL);
            xo[1].process (R[i], loR, hiR);

            // low band: mono (crossfaded in/out so switching MONO BELOW never clicks)
            const float loMono = 0.5f * (loL + loR);
            loL = loL + (loMono - loL) * mm;
            loR = loR + (loMono - loR) * mm;

            // high band (or everything when mono-below is off): width + Haas
            const float mid = 0.5f * (hiL + hiR);
            const float side = 0.5f * (hiL - hiR) * w;
            float l = mid + side, r = mid - side;
            haasLine.push (r);
            if (h > 0.01f) r = haasLine.readLinear (std::max (1.0f, h));

            // keep the level roughly constant when widening a lot
            const float comp = w > 1.0f ? 1.0f / std::sqrt (1.0f + (w - 1.0f) * 0.35f) : 1.0f;
            L[i] = loL + l * comp;
            R[i] = loR + r * comp;
        }
    }

private:
    DelayLine haasLine;
    std::array<LR4Crossover, 2> xo;
    Smoothed width, haas, monoMix;
    float monoHz = 20.0f;
};
} // namespace

std::unique_ptr<Effect> createWideBody() { return std::make_unique<WideBody>(); }
} // namespace ek
