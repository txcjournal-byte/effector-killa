#pragma once

#include "DSP/VoxEngine.h"

#include <vector>

namespace vox
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace vox
