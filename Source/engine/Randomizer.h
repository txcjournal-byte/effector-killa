#pragma once

// KILL (PG / R / UNRATED) and BOOTLEG – deterministic, rule-based chain generator.

#include "Preset.h"

namespace ek
{
enum class KillLevel : int { PG = 0, R, Unrated };

struct Randomizer
{
    // Generates a new program from `current`. Deterministic for a given seed.
    // WRITE PROTECT slots keep their effect, parameters and position.
    static ProgramState kill (const ProgramState& current, KillLevel level, Source source, uint64_t seed);

    // Subtle variation: +-10..20 % parameter changes, effect types never change.
    static ProgramState bootleg (const ProgramState& current, uint64_t seed);

    // Rule checker used by the tests (returns an empty string when the chain is valid).
    static juce::String checkRules (const ProgramState& p, Source source, KillLevel level);

    // Sorting rank used to enforce the chain order rules.
    static int orderRank (EffectType t);
};
} // namespace ek
