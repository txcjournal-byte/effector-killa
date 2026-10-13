#pragma once

#include "DSP/HeatEngine.h"

#include <vector>

namespace heatfx
{

struct Preset
{
    const char* category;
    const char* name;
    Params params; // band solo is never stored in presets
};

const std::vector<Preset>& factoryPresets();

} // namespace heatfx
