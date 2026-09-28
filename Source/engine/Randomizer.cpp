#include "Randomizer.h"

namespace ek
{
using T = EffectType;

namespace
{
    bool isSaturation (T t) { return t == T::CassettePlug || t == T::Menace || t == T::Brainrot; }
    bool isSpace (T t) { return t == T::AdLibThrow || t == T::AuraRoom; }
    bool isStereoOrMod (T t) { return t == T::Doubles || t == T::Swirl || t == T::AdLibThrow || t == T::AuraRoom || t == T::WideBody; }
    bool isFlexible (T t) { return t == T::ToneUp || t == T::ThroughTheWall || t == T::Slap || t == T::Chopped || t == T::Doubles || t == T::Swirl; }
    bool isSingleton (T t) { return t == T::Menace || t == T::CassettePlug || t == T::Brainrot || t == T::RedLine || t == T::WideBody; }

    float weightFor (Source s, T t)
    {
        // columns: Cassette Menace Brainrot TTW ToneUp Squeeze Overcooked Slap Doubles Swirl AdLib Aura Wide Chopped RedLine
        static const float w[6][15] = {
            /* MELODY */ { 3.0f, 1.5f, 1.2f, 2.0f, 2.0f, 1.0f, 1.2f, 0.5f, 2.0f, 1.2f, 2.0f, 2.5f, 1.5f, 0.7f, 0.8f },
            /* VOX    */ { 1.5f, 1.0f, 0.6f, 1.2f, 3.0f, 3.0f, 1.5f, 0.3f, 1.5f, 0.6f, 2.5f, 2.0f, 1.0f, 0.5f, 0.8f },
            /* 808    */ { 3.0f, 2.5f, 0.5f, 1.0f, 2.0f, 1.5f, 1.5f, 1.0f, 0.4f, 0.4f, 0.2f, 0.3f, 1.5f, 0.4f, 2.0f },
            /* DRUMS  */ { 1.5f, 1.2f, 1.0f, 1.0f, 1.5f, 2.0f, 1.5f, 3.5f, 0.3f, 0.3f, 0.5f, 1.0f, 1.0f, 1.0f, 3.0f },
            /* FX     */ { 1.5f, 1.5f, 2.0f, 2.0f, 1.0f, 0.8f, 1.2f, 0.5f, 1.5f, 2.0f, 2.0f, 2.0f, 1.5f, 1.5f, 0.6f },
            /* BUS    */ { 1.5f, 0.5f, 0.0f, 0.5f, 3.0f, 3.0f, 1.5f, 0.8f, 0.2f, 0.1f, 0.2f, 0.2f, 2.0f, 0.0f, 0.0f } };
        const int si = juce::jlimit (0, 5, (int) s);
        const int ti = (int) t - 1;
        return (ti >= 0 && ti < 15) ? w[si][ti] : 0.0f;
    }

    // ---------------------------------------------------------------------
    // parameter generation (real units, converted through the ParamSpec)
    // ---------------------------------------------------------------------
    struct ParamWriter
    {
        SlotState& s;
        const EffectInfo& info;
        void set (int i, float real) { s.p[(size_t) i] = info.params[(size_t) i].fromReal (real); }
        void setChoice (int i, int idx) { s.p[(size_t) i] = info.params[(size_t) i].fromReal (info.params[(size_t) i].min + (float) idx); }
        void setSync (int i, const char* label) { setChoice (i, juce::jmax (0, syncIndexOf (label))); }
    };

    const char* pickSync (Rng& r, std::initializer_list<const char*> labels)
    {
        const int idx = r.integer (0, (int) labels.size() - 1);
        return *(labels.begin() + idx);
    }

    void randomizeParams (SlotState& s, Rng& r, float in, Source src)
    {
        // in: intensity 0..1 (R ~ 0.5, UNRATED ~ 0.85)
        const auto& info = effectInfo (s.type);
        ParamWriter w { s, info };
        auto u = [&r] (float a, float b) { return r.uniform (a, b); };
        const bool bass = src == Source::Bass808;

        switch (s.type)
        {
            case T::CassettePlug:
                w.setChoice (0, r.chance (0.35f) ? 1 : 0);
                w.set (1, u (0.15f, 0.35f) + in * u (0.0f, 0.3f));
                w.set (2, u (0.0f, 0.35f));
                w.set (3, u (0.35f, 0.65f));
                w.set (4, r.chance (0.3f) ? u (0.05f, 0.35f) : 0.0f);
                w.set (5, r.chance (0.2f) ? u (0.03f, 0.2f) : 0.0f);
                w.set (6, bass ? u (60.0f, 110.0f) : 20.0f);
                w.set (7, u (0.6f, 1.0f));
                break;
            case T::Menace:
                w.setChoice (0, r.integer (0, 3));
                w.set (1, u (0.1f, 0.3f) + in * u (0.0f, 0.35f));
                w.set (2, u (0.35f, 0.65f));
                w.set (3, r.chance (0.4f) ? u (60.0f, 300.0f) : 20.0f);
                w.set (4, u (6000.0f, 20000.0f));
                w.set (5, bass ? u (60.0f, 120.0f) : (r.chance (0.3f) ? u (40.0f, 120.0f) : 20.0f));
                w.set (6, u (0.35f, 0.7f) + in * u (0.0f, 0.3f));
                break;
            case T::Brainrot:
                w.set (0, (float) r.integer (in > 0.7f ? 6 : 8, 14));
                w.set (1, r.chance (0.5f) ? u (8000.0f, 30000.0f) : 44100.0f);
                w.set (2, r.chance (0.3f) ? u (0.05f, 0.4f) : 0.0f);
                w.set (3, r.chance (0.3f) ? u (0.05f, 0.25f) : 0.0f);
                w.set (4, r.chance (0.25f) ? u (0.05f, 0.3f) : 0.0f);
                w.set (5, u (0.35f, 0.8f));
                w.set (6, u (0.3f, 0.6f) + in * u (0.0f, 0.3f));
                break;
            case T::ThroughTheWall:
            {
                const int type = r.chance (0.5f) ? 1 : r.integer (0, 5);
                w.setChoice (0, type);
                if (type == 2) w.set (1, u (150.0f, 800.0f));             // HP
                else if (type == 3) w.set (1, u (500.0f, 2500.0f));       // BP
                else if (type == 5) w.set (1, u (200.0f, 2000.0f));       // COMB
                else w.set (1, u (1500.0f, 9000.0f));                     // LP / NOTCH
                w.set (2, u (0.05f, 0.35f + 0.2f * in));
                w.set (3, r.chance (0.3f) ? u (0.0f, 0.3f) * in : 0.0f);
                w.setSync (4, pickSync (r, { "1 BAR", "1/2", "1/4", "1/8", "2 BAR" }));
                w.set (5, r.chance (0.4f) ? u (0.1f, 0.4f) : 0.0f);
                w.set (6, r.chance (0.2f) ? u (-0.3f, 0.4f) : 0.0f);
                w.set (7, u (1.0f, 3.0f));
                break;
            }
            case T::ToneUp:
                w.set (0, src == Source::Vox ? u (80.0f, 160.0f) : (bass ? 20.0f : (r.chance (0.5f) ? u (30.0f, 150.0f) : 10.0f)));
                w.set (1, u (-2.0f, 3.0f));
                w.set (2, u (-3.0f, 2.0f));
                w.set (3, u (150.0f, 600.0f));
                w.set (4, u (-2.0f, 4.0f));
                w.set (5, u (800.0f, 5000.0f));
                w.set (6, u (-2.0f, 4.0f));
                w.set (7, u (-2.0f, 2.0f));
                break;
            case T::Squeeze:
                w.set (0, u (-26.0f, -12.0f));
                w.set (1, u (2.0f, 4.0f) + in * u (0.0f, 4.0f));
                w.set (2, u (3.0f, 30.0f));
                w.set (3, u (60.0f, 300.0f));
                w.set (4, u (3.0f, 9.0f));
                w.set (5, u (0.0f, 3.0f));
                w.set (6, u (0.5f, 1.0f));
                break;
            case T::Overcooked:
                w.set (0, u (0.15f, 0.35f) + in * u (0.0f, 0.3f));
                w.set (1, u (0.3f, 0.7f));
                w.set (2, u (0.3f, 0.7f));
                w.set (3, u (0.3f, 0.7f));
                w.set (4, u (-2.0f, 2.0f));
                w.set (5, u (-2.0f, 2.0f));
                w.set (6, u (-2.0f, 2.0f));
                w.set (7, u (0.5f, 0.9f));
                break;
            case T::Slap:
                w.set (0, u (0.1f, 0.45f) + in * u (0.0f, 0.2f));
                w.set (1, u (-0.3f, 0.2f));
                w.set (2, r.chance (0.4f) ? u (0.1f, 0.4f) : 0.0f);
                w.set (3, u (0.7f, 1.0f));
                break;
            case T::Doubles:
                w.set (0, u (0.2f, 1.2f));
                w.set (1, u (0.2f, 0.6f));
                w.set (2, (float) r.integer (1, 4));
                w.set (3, u (0.5f, 1.0f));
                w.set (4, bass ? u (150.0f, 250.0f) : u (20.0f, 150.0f));
                w.set (5, u (0.2f, 0.4f) + in * u (0.0f, 0.2f));
                break;
            case T::Swirl:
                w.set (0, u (0.05f, 0.8f));
                w.setChoice (1, r.chance (0.3f) ? 1 : 0);
                w.set (2, (float) (2 * r.integer (1, 6)));
                w.set (3, u (0.1f, 0.5f));
                w.set (4, u (0.4f, 0.9f));
                w.set (5, u (0.15f, 0.35f) + in * u (0.0f, 0.2f));
                w.setSync (6, pickSync (r, { "1 BAR", "2 BAR", "1/2", "4 BAR" }));
                break;
            case T::AdLibThrow:
                w.setSync (0, pickSync (r, { "1/4", "1/8", "1/8D", "1/4D", "1/16", "1/2" }));
                w.setChoice (1, 1);
                w.set (2, u (0.2f, 0.45f) + in * u (0.0f, 0.15f));
                w.setChoice (3, r.chance (0.4f) ? 1 : 0);
                w.set (4, u (3000.0f, 10000.0f));
                w.set (5, u (100.0f, 400.0f));
                w.set (6, r.chance (0.4f) ? u (0.2f, 0.6f) : 0.0f);
                w.set (7, u (0.12f, 0.25f) + in * u (0.0f, 0.12f));
                break;
            case T::AuraRoom:
                w.setChoice (0, r.integer (0, 3));
                w.set (1, src == Source::Drums ? u (0.1f, 0.35f) : u (0.25f, 0.8f));
                w.set (2, u (0.8f, 3.0f) + in * u (0.0f, 2.0f));
                w.set (3, u (0.0f, 40.0f));
                w.set (4, u (0.2f, 0.6f));
                w.set (5, u (0.7f, 1.0f));
                w.set (6, r.chance (0.3f) ? u (0.1f, 0.5f) : 0.0f);
                w.set (7, u (0.12f, 0.28f) + in * u (0.0f, 0.12f));
                break;
            case T::WideBody:
                w.set (0, u (1.1f, 1.5f) + in * u (0.0f, 0.3f));
                w.set (1, r.chance (0.2f) ? u (2.0f, 12.0f) : 0.0f);
                w.set (2, bass ? u (120.0f, 180.0f) : (r.chance (0.5f) ? u (80.0f, 150.0f) : 0.0f));
                break;
            case T::Chopped:
                w.setChoice (0, r.chance (0.5f) ? 0 : 1);
                w.setChoice (1, r.integer (0, 15));
                w.setSync (2, pickSync (r, { "1/16", "1/8", "1/4", "1/32" }));
                w.set (3, u (0.2f, 0.45f) + in * u (0.0f, 0.3f));
                w.set (4, u (0.2f, 0.6f));
                w.set (5, -45.0f);
                w.set (6, 1.0f);
                break;
            case T::RedLine:
                w.set (0, u (-1.0f, -0.2f));
                w.set (1, u (0.0f, 3.0f) + in * u (0.0f, 4.0f));
                w.set (2, u (0.2f, 0.8f));
                w.setChoice (3, 1);
                break;
            case T::None:
            case T::Count:
            default: break;
        }
    }

    // ---------------------------------------------------------------------
    // source constraints on parameters
    // ---------------------------------------------------------------------
    void enforceParamRules (ProgramState& p, Source src)
    {
        auto clampReal = [] (SlotState& s, int i, float lo, float hi)
        {
            const auto& spec = effectInfo (s.type).params[(size_t) i];
            const float v = juce::jlimit (lo, hi, spec.toReal (s.p[(size_t) i]));
            s.p[(size_t) i] = spec.fromReal (v);
        };

        for (auto& s : p.slots)
        {
            if (s.isEmpty() || s.writeProtect) continue;
            switch (src)
            {
                case Source::Bass808:
                    if (s.type == T::AuraRoom) clampReal (s, 7, 0.0f, 0.15f);
                    if (s.type == T::AdLibThrow) clampReal (s, 7, 0.0f, 0.15f);
                    if (s.type == T::Doubles) clampReal (s, 4, 150.0f, 300.0f);
                    if (s.type == T::Menace) clampReal (s, 5, 60.0f, 250.0f);
                    if (s.type == T::CassettePlug) clampReal (s, 6, 60.0f, 250.0f);
                    if (s.type == T::WideBody) clampReal (s, 2, 120.0f, 300.0f);
                    break;
                case Source::Drums:
                    if (s.type == T::AuraRoom) clampReal (s, 1, 0.0f, 0.38f);
                    break;
                case Source::Vox:
                    if (s.type == T::ToneUp) clampReal (s, 0, 80.0f, 500.0f);
                    break;
                case Source::Bus:
                    if (s.type == T::AuraRoom) clampReal (s, 7, 0.0f, 0.1f);
                    break;
                case Source::Melody: case Source::FX: case Source::Count:
                default: break;
            }
        }
    }

    int countType (const std::vector<T>& v, T t) { return (int) std::count (v.begin(), v.end(), t); }

    T pickWeighted (Rng& r, Source src, const std::vector<T>& already, bool allowRedLine)
    {
        float total = 0.0f;
        std::array<float, 16> w {};
        for (int i = 1; i <= kNumEffectTypes; ++i)
        {
            const T t = (T) i;
            float wt = weightFor (src, t);
            if (isSingleton (t) && countType (already, t) > 0) wt = 0.0f;
            if (t == T::RedLine && ! allowRedLine) wt = 0.0f;
            if (countType (already, t) > 0) wt *= 0.25f; // prefer variety
            w[(size_t) i] = wt;
            total += wt;
        }
        if (total <= 0.0f) return T::None;
        float x = r.uniform() * total;
        for (int i = 1; i <= kNumEffectTypes; ++i)
        {
            x -= w[(size_t) i];
            if (x < 0.0f) return (T) i;
        }
        return T::None;
    }

    struct Pending { T type; int rank; };

    int rankFor (T t, Rng& r)
    {
        if (isFlexible (t))
        {
            static const int opts[3] = { 10, 25, 35 };
            return opts[r.integer (0, 2)];
        }
        return Randomizer::orderRank (t);
    }

    // Places sorted effects around locked slots. Returns false if nothing could be placed.
    void place (ProgramState& out, const ProgramState& cur, std::vector<Pending>& items, Rng& r, float intensity, Source src)
    {
        std::stable_sort (items.begin(), items.end(), [] (const Pending& a, const Pending& b) { return a.rank < b.rank; });

        std::array<int, kNumSlots> lockedRank {};
        for (int i = 0; i < kNumSlots; ++i)
        {
            const auto& s = cur.slots[(size_t) i];
            lockedRank[(size_t) i] = s.writeProtect ? (s.isEmpty() ? -1 : Randomizer::orderRank (s.type)) : -2;
        }

        size_t next = 0;
        int maxBefore = -1000;
        for (int pos = 0; pos < kNumSlots; ++pos)
        {
            if (lockedRank[(size_t) pos] != -2)
            {
                out.slots[(size_t) pos] = cur.slots[(size_t) pos];
                if (lockedRank[(size_t) pos] >= 0) maxBefore = std::max (maxBefore, lockedRank[(size_t) pos]);
                continue;
            }
            out.slots[(size_t) pos] = SlotState();
            int minAfter = 1000;
            for (int k = pos + 1; k < kNumSlots; ++k)
                if (lockedRank[(size_t) k] >= 0) minAfter = std::min (minAfter, lockedRank[(size_t) k]);

            while (next < items.size() && items[next].rank < maxBefore) ++next; // cannot be placed any more
            if (next < items.size() && items[next].rank <= minAfter)
            {
                auto& s = out.slots[(size_t) pos];
                s.setType (items[next].type);
                randomizeParams (s, r, intensity, src);
                ++next;
            }
        }
    }

    void tweak (SlotState& s, Rng& r, float lo, float hi, float discreteChance)
    {
        const auto& info = effectInfo (s.type);
        for (int i = 0; i < kNumParams; ++i)
        {
            const auto& spec = info.params[(size_t) i];
            if (! spec.used) continue;
            if (spec.isDiscrete())
            {
                if (discreteChance > 0.0f && spec.unit != Unit::Toggle && r.chance (discreteChance))
                {
                    const int steps = spec.numSteps();
                    const int cur = (int) std::lround (s.p[(size_t) i] * (float) (steps - 1));
                    const int nv = juce::jlimit (0, steps - 1, cur + (r.chance (0.5f) ? 1 : -1));
                    s.p[(size_t) i] = steps > 1 ? (float) nv / (float) (steps - 1) : 0.0f;
                }
                continue;
            }
            const float amount = r.uniform (lo, hi) * (r.chance (0.5f) ? 1.0f : -1.0f);
            const float v = s.p[(size_t) i];
            s.p[(size_t) i] = clamp01 (v + amount * std::max (0.15f, v));
        }
    }
} // namespace

int Randomizer::orderRank (EffectType t)
{
    switch (t)
    {
        case T::CassettePlug:   return 20;
        case T::Menace:         return 21;
        case T::Brainrot:       return 22;
        case T::Squeeze:        return 30;
        case T::Overcooked:     return 31;
        case T::ToneUp:
        case T::ThroughTheWall:
        case T::Slap:
        case T::Chopped:
        case T::Doubles:
        case T::Swirl:          return 25;
        case T::AdLibThrow:     return 50;
        case T::AuraRoom:       return 51;
        case T::WideBody:       return 60;
        case T::RedLine:        return 70;
        case T::None: case T::Count:
        default:                return 0;
    }
}

// ============================================================================
ProgramState Randomizer::kill (const ProgramState& current, KillLevel level, Source src, uint64_t seed)
{
    Rng r (seed ^ 0xC0FFEE1234ull);
    ProgramState out = current;
    out.seed = seed;
    out.source = src;
    out.factory = false;
    out.channel = current.channel;
    out.index = -1;
    out.maps.clear();
    out.trimDb = 0.0f;
    out.focusMacro = -1;
    out.macros = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

    int nonEmpty = 0;
    for (auto& s : current.slots) nonEmpty += s.isEmpty() ? 0 : 1;

    if (level == KillLevel::PG && nonEmpty > 0)
    {
        // same effects, parameters +-25 %, at most one slot swapped
        for (auto& s : out.slots)
            if (! s.isEmpty() && ! s.writeProtect)
                tweak (s, r, 0.05f, 0.25f, 0.1f);

        if (r.chance (0.5f))
        {
            std::vector<int> candidates;
            for (int i = 0; i < kNumSlots; ++i)
                if (! out.slots[(size_t) i].isEmpty() && ! out.slots[(size_t) i].writeProtect) candidates.push_back (i);
            if (! candidates.empty())
            {
                const int slot = candidates[(size_t) r.integer (0, (int) candidates.size() - 1)];
                for (int attempt = 0; attempt < 12; ++attempt)
                {
                    std::vector<T> others;
                    for (int i = 0; i < kNumSlots; ++i)
                        if (i != slot && ! out.slots[(size_t) i].isEmpty()) others.push_back (out.slots[(size_t) i].type);
                    const T t = pickWeighted (r, src, others, false);
                    if (t == T::None || t == out.slots[(size_t) slot].type) continue;
                    auto trial = out;
                    trial.slots[(size_t) slot].setType (t);
                    randomizeParams (trial.slots[(size_t) slot], r, 0.5f, src);
                    trial.slots[(size_t) slot].mix = 1.0f;
                    enforceParamRules (trial, src);
                    if (checkRules (trial, src, KillLevel::PG).isEmpty()) { out = trial; break; }
                }
            }
        }
        enforceParamRules (out, src);
        return out;
    }

    // ---- R / UNRATED: new chain -------------------------------------------------
    const float intensity = level == KillLevel::Unrated ? 0.85f : 0.5f;
    int lo = level == KillLevel::Unrated ? 4 : 3;
    int hi = level == KillLevel::Unrated ? 7 : 5;
    if (src == Source::Bus) { hi = 4; lo = std::min (lo, 3); }

    std::vector<T> lockedTypes;
    int lockedCount = 0;
    for (auto& s : current.slots)
        if (s.writeProtect) { ++lockedCount; if (! s.isEmpty()) lockedTypes.push_back (s.type); }

    const int target = r.integer (lo, hi);
    const int freeSlots = kNumSlots - lockedCount;
    const int toAdd = juce::jlimit (0, freeSlots, target - (int) lockedTypes.size());

    std::vector<T> chosen = lockedTypes;
    std::vector<Pending> items;
    auto add = [&] (T t) { chosen.push_back (t); items.push_back ({ t, rankFor (t, r) }); };

    // mandatory / preferred effects
    if (src == Source::Bus && countType (chosen, T::RedLine) == 0 && (int) items.size() < toAdd) add (T::RedLine);
    if (src == Source::Drums)
    {
        if (countType (chosen, T::Slap) == 0 && (int) items.size() < toAdd && r.chance (0.7f)) add (T::Slap);
        if (countType (chosen, T::RedLine) == 0 && (int) items.size() < toAdd && r.chance (0.6f)) add (T::RedLine);
    }
    if (src == Source::Vox)
    {
        if (countType (chosen, T::ToneUp) == 0 && (int) items.size() < toAdd && r.chance (0.75f)) add (T::ToneUp);
        if (countType (chosen, T::Squeeze) == 0 && (int) items.size() < toAdd && r.chance (0.7f)) add (T::Squeeze);
    }

    while ((int) items.size() < toAdd)
    {
        const T t = pickWeighted (r, src, chosen, src != Source::Bus);
        if (t == T::None) break;
        add (t);
    }

    // 808: any stereo / modulation effect requires Wide Body with mono-below >= 120 Hz
    if (src == Source::Bass808)
    {
        bool needsWide = false;
        for (auto t : chosen) needsWide |= isStereoOrMod (t);
        if (needsWide && countType (chosen, T::WideBody) == 0)
        {
            const int total = (int) (lockedTypes.size() + items.size());
            if (total < hi && (int) items.size() < freeSlots)
            {
                items.push_back ({ T::WideBody, orderRank (T::WideBody) });
            }
            else
            {
                // replace the last item that is not the Red Line
                for (int i = (int) items.size() - 1; i >= 0; --i)
                    if (items[(size_t) i].type != T::RedLine)
                    { items[(size_t) i] = { T::WideBody, orderRank (T::WideBody) }; break; }
            }
        }
    }

    place (out, current, items, r, intensity, src);
    for (auto& s : out.slots) if (! s.writeProtect) { s.mix = 1.0f; s.pause = false; s.ms = MSMode::Stereo; }
    enforceParamRules (out, src);

    // BUS: Red Line must be last even if the placement dropped it
    return out;
}

ProgramState Randomizer::bootleg (const ProgramState& current, uint64_t seed)
{
    Rng r (seed ^ 0xB0071E6ull);
    ProgramState out = current;
    out.seed = seed;
    for (auto& s : out.slots)
        if (! s.isEmpty() && ! s.writeProtect)
            tweak (s, r, 0.10f, 0.20f, 0.0f);
    return out; // KILL rules are not applied: BOOTLEG only varies what is there

}

// ============================================================================
juce::String Randomizer::checkRules (const ProgramState& p, Source src, KillLevel level)
{
    std::vector<T> seq;
    std::vector<int> pos;
    for (int i = 0; i < kNumSlots; ++i)
        if (! p.slots[(size_t) i].isEmpty()) { seq.push_back (p.slots[(size_t) i].type); pos.push_back (i); }

    auto count = [&] (T t) { return countType (seq, t); };
    if (count (T::Menace) > 1) return "more than one Menace";
    if (count (T::CassettePlug) > 1) return "more than one Cassette Plug";
    if (count (T::Brainrot) > 1) return "more than one Brainrot";

    const int n = (int) seq.size();
    if (level == KillLevel::R && (n < 3 || n > 5)) return "R must have 3-5 slots, has " + juce::String (n);
    if (level == KillLevel::Unrated && src != Source::Bus && (n < 4 || n > 7)) return "UNRATED must have 4-7 slots, has " + juce::String (n);

    int firstSpace = -1, lastSat = -1, firstDyn = -1, cassettePos = -1, menacePos = -1;
    for (int i = 0; i < n; ++i)
    {
        const T t = seq[(size_t) i];
        if (isSpace (t) && firstSpace < 0) firstSpace = i;
        if (isSaturation (t)) lastSat = i;
        if ((t == T::Squeeze || t == T::Overcooked) && firstDyn < 0) firstDyn = i;
        if (t == T::CassettePlug) cassettePos = i;
        if (t == T::Menace) menacePos = i;
        if (t == T::RedLine && i != n - 1) return "Red Line is not last";
    }
    if (cassettePos >= 0 && menacePos >= 0 && cassettePos > menacePos) return "Cassette Plug after Menace";
    if (firstSpace >= 0)
        for (int i = firstSpace; i < n; ++i)
        {
            const T t = seq[(size_t) i];
            if (! isSpace (t) && t != T::WideBody && t != T::RedLine) return effectInfo (t).name + " after space";
        }
    if (firstDyn >= 0 && lastSat > firstDyn) return "saturation after Squeeze/Overcooked";

    auto realOf = [&] (T t, int param, bool wantMax) -> float
    {
        float v = wantMax ? -1.0e9f : 1.0e9f;
        for (auto& s : p.slots)
            if (s.type == t)
            {
                const float x = effectInfo (t).params[(size_t) param].toReal (s.p[(size_t) param]);
                v = wantMax ? std::max (v, x) : std::min (v, x);
            }
        return v;
    };
    constexpr float eps = 0.02f;

    switch (src)
    {
        case Source::Bass808:
        {
            if (count (T::AuraRoom) && realOf (T::AuraRoom, 7, true) > 0.15f + eps) return "808: Aura Room mix > 0.15";
            bool stereo = false;
            for (auto t : seq) stereo |= isStereoOrMod (t);
            if (stereo)
            {
                if (count (T::WideBody) == 0) return "808: stereo/mod effect without Wide Body";
                if (realOf (T::WideBody, 2, false) < 120.0f - 0.5f) return "808: Wide Body mono-below < 120 Hz";
            }
            if (count (T::Doubles) && realOf (T::Doubles, 4, false) < 150.0f - 0.5f) return "808: Doubles low keep < 150";
            if (count (T::Menace) && realOf (T::Menace, 5, false) < 60.0f - 0.5f) return "808: Menace low keep < 60";
            if (count (T::CassettePlug) && realOf (T::CassettePlug, 6, false) < 60.0f - 0.5f) return "808: Cassette low keep < 60";
            break;
        }
        case Source::Drums:
            if (count (T::AuraRoom) && realOf (T::AuraRoom, 1, true) >= 0.4f) return "DRUMS: Aura Room size >= 0.4";
            break;
        case Source::Vox:
            if (count (T::ToneUp) && realOf (T::ToneUp, 0, false) < 80.0f - 0.5f) return "VOX: Tone Up low-cut < 80";
            break;
        case Source::Bus:
            if (n > 4) return "BUS: more than 4 slots";
            if (count (T::Brainrot) || count (T::Chopped)) return "BUS: Brainrot / Chopped not allowed";
            if (count (T::AuraRoom) && realOf (T::AuraRoom, 7, true) > 0.1f + eps) return "BUS: Aura Room mix > 0.1";
            if (level != KillLevel::PG && (n == 0 || seq.back() != T::RedLine)) return "BUS: Red Line must be last";
            break;
        case Source::Melody: case Source::FX: case Source::Count:
        default: break;
    }
    return {};
}

} // namespace ek
