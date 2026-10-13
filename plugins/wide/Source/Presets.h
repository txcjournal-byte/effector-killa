#pragma once

#include "DSP/WideEngine.h"

#include <vector>

namespace widefx
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace widefx
