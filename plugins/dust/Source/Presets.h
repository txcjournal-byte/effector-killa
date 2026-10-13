#pragma once

#include "DSP/DustEngine.h"

#include <vector>

namespace dust
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

// Factory presets (index 0 = Init).
const std::vector<Preset>& factoryPresets();

} // namespace dust
