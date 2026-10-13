#pragma once

#include "DSP/ChopperEngine.h"

#include <vector>

namespace chopper
{

struct Preset
{
    const char* category;
    const char* name;
    const char* pattern;   // 16 chars, see fxChars: . R V T G P H
    int rate;              // index into rateNames
    int size;              // index into sizeNames
    float swing;
    float mix;
    float outputDb;
};

const std::vector<Preset>& factoryPresets();

} // namespace chopper
