#pragma once

#include "DSP/AuraEngine.h"

#include <vector>

namespace auraverb
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace auraverb
