#include "Presets.h"

namespace vox
{

namespace
{
Params P (float pitch, float formant, TuneMode tune = TuneMode::off, float retune = 40.0f, float mix = 1.0f, int key = 0, int scale = 1)
{
    Params p;
    p.pitchSemis = pitch; p.formantSemis = formant; p.tune = tune; p.retuneMs = retune; p.mix = mix; p.key = key; p.scale = scale;
    return p;
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    using T = TuneMode;
    static const std::vector<Preset> presets {
        { "Init",    "Init (clean)",       P (0, 0) },

        { "Tune",    "Hard Tune",          P (0, 0, T::chromatic, 0) },
        { "Tune",    "Melodic Tune (A minor)", P (0, 0, T::scale, 20, 1.0f, 9, 1) },
        { "Tune",    "Natural Tune",       P (0, 0, T::chromatic, 90) },
        { "Tune",    "Robot Hook",         P (0, 1, T::chromatic, 0) },

        { "Dark",    "Demon",              P (-7, -4) },
        { "Dark",    "Deep Voice",         P (-3, -3) },
        { "Dark",    "Possessed",          P (-12, -6) },
        { "Dark",    "Villain",            P (-5, -2, T::chromatic, 15) },

        { "Bright",  "Chipmunk",           P (7, 5) },
        { "Bright",  "Baby Voice",         P (5, 7) },
        { "Bright",  "Helium",             P (12, 8) },
        { "Bright",  "Alien",              P (-2, 8) },

        { "Ad-Libs", "Ad-Lib Up",          P (12, 3) },
        { "Ad-Libs", "Ad-Lib Down",        P (-12, -2) },
        { "Ad-Libs", "Ad-Lib Demon Tune",  P (-7, -3, T::chromatic, 0) },
        { "Ad-Libs", "Ad-Lib Chipmunk Tune", P (7, 4, T::chromatic, 0) },

        { "Layers",  "Octave Down Layer",  P (-12, 0, T::off, 40, 0.5f) },
        { "Layers",  "Harmony Fifth",      P (7, 0, T::off, 40, 0.5f) },
        { "Layers",  "Harmony Third",      P (4, 0, T::off, 40, 0.5f) },

        { "Character", "Male to Female",   P (4, 3) },
        { "Character", "Female to Male",   P (-4, -3) },
        { "Character", "Small Speaker",    P (0, 5) },
        { "Character", "Giant",            P (0, -6) },
    };
    return presets;
}

} // namespace vox
