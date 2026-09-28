#pragma once

#include "../Effect.h"

namespace ek
{
// ---- parameter spec helpers ------------------------------------------------
inline ParamSpec pPercent (const char* name, float def, float max = 1.0f)
{
    ParamSpec p; p.name = name; p.min = 0.0f; p.max = max; p.def = def; p.unit = Unit::Percent; return p;
}
inline ParamSpec pBipolar (const char* name, float def = 0.0f)
{
    ParamSpec p; p.name = name; p.min = -1.0f; p.max = 1.0f; p.def = def; p.unit = Unit::Percent; return p;
}
inline ParamSpec pHz (const char* name, float min, float max, float def, float centre)
{
    ParamSpec p; p.name = name; p.min = min; p.max = max; p.def = def; p.centre = centre; p.unit = Unit::Hz; return p;
}
inline ParamSpec pDb (const char* name, float min, float max, float def, float centre = 0.0f)
{
    ParamSpec p; p.name = name; p.min = min; p.max = max; p.def = def; p.centre = centre; p.unit = Unit::Db; return p;
}
inline ParamSpec pMs (const char* name, float min, float max, float def, float centre = 0.0f)
{
    ParamSpec p; p.name = name; p.min = min; p.max = max; p.def = def; p.centre = centre; p.unit = Unit::Ms; return p;
}
inline ParamSpec pUnit (const char* name, Unit u, float min, float max, float def, float centre = 0.0f)
{
    ParamSpec p; p.name = name; p.min = min; p.max = max; p.def = def; p.centre = centre; p.unit = u; return p;
}
inline ParamSpec pChoice (const char* name, juce::StringArray choices, int def)
{
    ParamSpec p; p.name = name; p.min = 0.0f; p.max = (float) (choices.size() - 1); p.def = (float) def;
    p.unit = Unit::Choice; p.choices = std::move (choices); return p;
}
inline ParamSpec pToggle (const char* name, bool def)
{
    ParamSpec p; p.name = name; p.min = 0.0f; p.max = 1.0f; p.def = def ? 1.0f : 0.0f; p.unit = Unit::Toggle; return p;
}
inline ParamSpec pSync (const char* name, const char* defLabel)
{
    ParamSpec p; p.name = name; p.choices = syncDivisionLabels(); p.min = 0.0f; p.max = (float) (p.choices.size() - 1);
    p.def = (float) juce::jmax (0, syncIndexOf (defLabel)); p.unit = Unit::Sync; return p;
}
inline ParamSpec pUnused()
{
    ParamSpec p; p.name = "-"; p.used = false; return p;
}

inline float syncBeats (float realIndex)
{
    const auto& d = syncDivisions();
    return d[(size_t) juce::jlimit (0, (int) d.size() - 1, (int) std::lround (realIndex))].beats;
}

// Tempo-synced / free-running LFO phase helper.
struct SyncPhase
{
    double phase = 0.0;
    // Returns phase in [0,1). When the host is playing we lock to the song position.
    void advanceBlock (const ProcessContext& ctx, double periodBeats, int n) noexcept
    {
        if (ctx.playing && ctx.hostHasPosition)
            phase = std::fmod (ctx.ppq / periodBeats, 1.0);
        else
            phase = std::fmod (phase + 0.0, 1.0);
        blockInc = ctx.beatsPerSample() / periodBeats;
        juce::ignoreUnused (n);
    }
    double tick() noexcept
    {
        const double p = phase;
        phase += blockInc;
        if (phase >= 1.0) phase -= 1.0;
        return p;
    }
    double blockInc = 0.0;
};

// ---- effect factories --------------------------------------------------------
#define EK_DECLARE_EFFECT(Name) \
    EffectInfo make##Name##Info(); \
    std::unique_ptr<Effect> create##Name();

EK_DECLARE_EFFECT (CassettePlug)
EK_DECLARE_EFFECT (Menace)
EK_DECLARE_EFFECT (Brainrot)
EK_DECLARE_EFFECT (ThroughTheWall)
EK_DECLARE_EFFECT (ToneUp)
EK_DECLARE_EFFECT (Squeeze)
EK_DECLARE_EFFECT (Overcooked)
EK_DECLARE_EFFECT (Slap)
EK_DECLARE_EFFECT (Doubles)
EK_DECLARE_EFFECT (Swirl)
EK_DECLARE_EFFECT (AdLibThrow)
EK_DECLARE_EFFECT (AuraRoom)
EK_DECLARE_EFFECT (WideBody)
EK_DECLARE_EFFECT (Chopped)
EK_DECLARE_EFFECT (RedLine)

#undef EK_DECLARE_EFFECT

uint16_t choppedPattern (int index); // 16-step pattern bitmask, bit 15 = step 1
} // namespace ek
