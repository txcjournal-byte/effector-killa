#include "Effect.h"
#include "effects/AllEffects.h"

namespace ek
{
namespace
{
    struct Entry
    {
        EffectInfo info;
        std::unique_ptr<Effect> (*create)();
    };

    std::vector<Entry>& registry()
    {
        static std::vector<Entry> r = []
        {
            std::vector<Entry> v;
            v.push_back ({ EffectInfo{}, nullptr }); // EffectType::None
            v[0].info.id = "none";
            v[0].info.name = "Empty";
            for (auto& p : v[0].info.params) p = pUnused();

            auto add = [&v] (EffectInfo info, std::unique_ptr<Effect> (*fn)())
            {
                jassert ((int) info.type == (int) v.size()); // registration order == enum order
                v.push_back ({ std::move (info), fn });
            };
            add (makeCassettePlugInfo(),   createCassettePlug);
            add (makeMenaceInfo(),         createMenace);
            add (makeBrainrotInfo(),       createBrainrot);
            add (makeThroughTheWallInfo(), createThroughTheWall);
            add (makeToneUpInfo(),         createToneUp);
            add (makeSqueezeInfo(),        createSqueeze);
            add (makeOvercookedInfo(),     createOvercooked);
            add (makeSlapInfo(),           createSlap);
            add (makeDoublesInfo(),        createDoubles);
            add (makeSwirlInfo(),          createSwirl);
            add (makeAdLibThrowInfo(),     createAdLibThrow);
            add (makeAuraRoomInfo(),       createAuraRoom);
            add (makeWideBodyInfo(),       createWideBody);
            add (makeChoppedInfo(),        createChopped);
            add (makeRedLineInfo(),        createRedLine);
            return v;
        }();
        return r;
    }
} // namespace

const EffectInfo& effectInfo (EffectType t)
{
    auto& r = registry();
    const int i = (int) t;
    return r[(size_t) juce::jlimit (0, (int) r.size() - 1, i)].info;
}

std::unique_ptr<Effect> createEffect (EffectType t)
{
    auto& r = registry();
    const int i = (int) t;
    if (i <= 0 || i >= (int) r.size() || r[(size_t) i].create == nullptr)
        return nullptr;
    return r[(size_t) i].create();
}

EffectType effectTypeFromId (const juce::String& idOrName)
{
    const auto key = idOrName.trim();
    for (auto& e : registry())
        if (e.info.id.equalsIgnoreCase (key) || e.info.name.equalsIgnoreCase (key))
            return e.info.type;
    return EffectType::None;
}

juce::Array<EffectType> allEffectTypes()
{
    juce::Array<EffectType> a;
    for (int i = 1; i <= kNumEffectTypes; ++i)
        a.add ((EffectType) i);
    return a;
}

} // namespace ek
