#pragma once

#include "DSP/GlowEngine.h"

#include <vector>

namespace glowfx
{

struct Preset
{
    const char* category;
    const char* name;
    Params params;
};

const std::vector<Preset>& factoryPresets();

} // namespace glowfx
