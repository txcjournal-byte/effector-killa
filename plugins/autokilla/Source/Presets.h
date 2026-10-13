#pragma once

#include "DSP/TunerEngine.h"

#include <vector>

namespace akill
{

// Factory presets never touch the KEY (the song's key is the user's); a preset may force a scale
// (e.g. chromatic robot presets) with scale >= 0, otherwise the current scale is kept.
struct Preset
{
    const char* category;
    const char* name;
    Params params;
    int scale = -1;
};

const std::vector<Preset>& factoryPresets();

} // namespace akill
