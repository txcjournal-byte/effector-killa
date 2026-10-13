#pragma once

#include "DSP/HaloEngine.h"

#include <vector>

namespace halo
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace halo
