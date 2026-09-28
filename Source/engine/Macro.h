#pragma once

// Macro engine: turns the 5 macro knobs into effective slot parameters.
//
// Macros are bipolar around 0.5 (0.5 = the preset exactly as stored).
//  * Default mapping: every effect type defines an offset per macro per parameter (EffectInfo::macro).
//  * Preset mapping: a preset may define its own targets for a macro (atZero .. base .. atOne);
//    when it does, the default mapping for that macro is disabled.
//  * Source rules clamp what macros can do (e.g. 808: AURA never pushes reverb mix above 0.15).

#include "Preset.h"

namespace ek
{
struct MacroEngine
{
    // cfg: effect types in the running chain. base: host parameter values. macros: effective 0..1.
    // slotParamMod: additional per-slot normalised offsets (AV modulation), may be null.
    static void compute (const RackConfig& cfg,
                         const RackParams& base,
                         const std::array<float, kNumMacros>& macros,
                         const std::vector<MacroMapping>& maps,
                         Source source,
                         RackParams& out) noexcept;

    // Upper limits that the macros may not exceed for a given source (normalised, per param).
    // Returns 1.0 when unlimited.
    static float sourceLimit (Source s, EffectType t, int param) noexcept;

    static const char* macroName (int m) noexcept;
};
} // namespace ek
