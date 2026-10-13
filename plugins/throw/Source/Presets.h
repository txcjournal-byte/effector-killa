#pragma once

#include "DSP/ThrowEngine.h"

#include <vector>

namespace throwfx
{

struct Preset
{
    const char* category;
    const char* name;
    Params params; // throwOn / freeze are never stored in presets
};

const std::vector<Preset>& factoryPresets();

} // namespace throwfx
