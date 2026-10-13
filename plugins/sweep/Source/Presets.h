#pragma once

#include "DSP/SweepEngine.h"

#include <vector>

namespace sweepfx
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace sweepfx
