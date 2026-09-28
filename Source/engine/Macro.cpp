#include "Macro.h"

namespace ek
{
const char* MacroEngine::macroName (int m) noexcept
{
    static const char* names[] = { "VILLAIN ARC", "CRASH OUT", "AURA", "DRIP", "KNOCK" };
    return names[juce::jlimit (0, kNumMacros - 1, m)];
}

float MacroEngine::sourceLimit (Source s, EffectType t, int param) noexcept
{
    switch (s)
    {
        case Source::Bass808:
            if (t == EffectType::AuraRoom && param == 7) return 0.15f;       // reverb mix
            if (t == EffectType::AdLibThrow && param == 7) return 0.15f;     // delay mix
            if (t == EffectType::WideBody && param == 0) return 0.75f;       // width <= 150 %
            break;
        case Source::Bus:
            if (t == EffectType::AuraRoom && param == 7) return 0.1f;
            if (t == EffectType::AdLibThrow && param == 7) return 0.12f;
            break;
        case Source::Drums:
            if (t == EffectType::AuraRoom && param == 1) return 0.39f;       // size < 0.4
            break;
        case Source::Melody: case Source::Vox: case Source::FX: case Source::Count:
        default: break;
    }
    return 1.0f;
}

void MacroEngine::compute (const RackConfig& cfg, const RackParams& base, const std::array<float, kNumMacros>& macros,
                           const std::vector<MacroMapping>& maps, Source source, RackParams& out) noexcept
{
    out = base;

    uint32_t customMask = 0;
    for (auto& m : maps)
        if (m.macro >= 0 && m.macro < kNumMacros) customMask |= 1u << m.macro;

    std::array<float, kNumMacros> bip {};
    for (int m = 0; m < kNumMacros; ++m)
        bip[(size_t) m] = (clamp01 (macros[(size_t) m]) - 0.5f) * 2.0f;

    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto type = cfg.slots[(size_t) s].type;
        if (type == EffectType::None) continue;
        const auto& info = effectInfo (type);
        auto& o = out.slots[(size_t) s];
        const auto& b = base.slots[(size_t) s];

        for (int m = 0; m < kNumMacros; ++m)
        {
            if (customMask & (1u << m)) continue;
            const float v = bip[(size_t) m];
            if (v == 0.0f) continue;
            const auto& amounts = info.macro[(size_t) m];
            for (int p = 0; p < kNumParams; ++p)
                if (amounts[(size_t) p] != 0.0f && info.params[(size_t) p].used)
                    o.p[(size_t) p] += amounts[(size_t) p] * v;
        }

        for (int p = 0; p < kNumParams; ++p)
        {
            float v = clamp01 (o.p[(size_t) p]);
            const float lim = sourceLimit (source, type, p);
            if (lim < 1.0f) v = std::min (v, std::max (b.p[(size_t) p], lim));
            o.p[(size_t) p] = v;
        }
    }

    // preset-defined mappings (piecewise: atZero -> base -> atOne)
    for (auto& mm : maps)
    {
        if (mm.macro < 0 || mm.macro >= kNumMacros || mm.slot < 0 || mm.slot >= kNumSlots) continue;
        if (cfg.slots[(size_t) mm.slot].type == EffectType::None) continue;
        const float mv = clamp01 (macros[(size_t) mm.macro]);
        auto& o = out.slots[(size_t) mm.slot];
        const auto& b = base.slots[(size_t) mm.slot];
        const float baseVal = mm.param == 8 ? b.mix : b.p[(size_t) juce::jlimit (0, 7, mm.param)];
        const float v = mv < 0.5f ? lerp (mm.atZero, baseVal, mv * 2.0f) : lerp (baseVal, mm.atOne, (mv - 0.5f) * 2.0f);
        if (mm.param == 8) o.mix = clamp01 (v);
        else
        {
            const auto type = cfg.slots[(size_t) mm.slot].type;
            const float lim = sourceLimit (source, type, mm.param);
            float c = clamp01 (v);
            if (lim < 1.0f) c = std::min (c, std::max (baseVal, lim));
            o.p[(size_t) mm.param] = c;
        }
    }
}

} // namespace ek
