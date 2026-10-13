#pragma once

#include "DSP/SnapEngine.h"

#include <vector>

namespace snapfx
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace snapfx
