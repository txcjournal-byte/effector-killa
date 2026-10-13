#pragma once

#include "DSP/SubEngine.h"

#include <vector>

namespace subkilla
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace subkilla
