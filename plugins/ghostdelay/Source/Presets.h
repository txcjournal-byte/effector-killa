#pragma once

#include "DSP/GhostEngine.h"

#include <vector>

namespace ghostdelay
{

struct Preset
{
    const char* category;
    const char* name;
    Params params; // freeze is never stored in presets
};

const std::vector<Preset>& factoryPresets();

} // namespace ghostdelay
