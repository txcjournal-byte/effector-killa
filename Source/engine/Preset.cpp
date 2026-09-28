#include "Preset.h"

namespace ek
{
// ============================================================================
// Source
// ============================================================================
juce::String sourceName (Source s)
{
    switch (s)
    {
        case Source::Melody:  return "MELODY";
        case Source::Vox:     return "VOX";
        case Source::Bass808: return "808";
        case Source::Drums:   return "DRUMS";
        case Source::FX:      return "FX";
        case Source::Bus:     return "BUS";
        case Source::Count:
        default:              return "MELODY";
    }
}

Source sourceFromName (const juce::String& s, Source fallback)
{
    for (int i = 0; i < (int) Source::Count; ++i)
        if (sourceName ((Source) i).equalsIgnoreCase (s.trim()))
            return (Source) i;
    return fallback;
}

static juce::String msName (MSMode m) { return m == MSMode::Mid ? "MID" : (m == MSMode::Side ? "SIDE" : "STEREO"); }
static MSMode msFromName (const juce::String& s)
{
    if (s.equalsIgnoreCase ("MID")) return MSMode::Mid;
    if (s.equalsIgnoreCase ("SIDE")) return MSMode::Side;
    return MSMode::Stereo;
}

// ============================================================================
// ModState
// ============================================================================
float screensaverBarsForIndex (int index)
{
    static constexpr float bars[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
    return bars[juce::jlimit (0, 4, index)];
}

void ModState::resamplePath()
{
    if (path.size() < 2) return;
    // the drawing is played as a closed loop: include the segment back to the start
    path.push_back (path.front());
    std::vector<float> cum (path.size(), 0.0f);
    for (size_t i = 1; i < path.size(); ++i)
        cum[i] = cum[i - 1] + path[i].getDistanceFrom (path[i - 1]);
    const float total = cum.back();
    if (total <= 1.0e-6f) { path.resize (1); return; }


    std::vector<juce::Point<float>> out;
    out.reserve (kPathPoints);
    size_t seg = 1;
    for (int k = 0; k < kPathPoints; ++k)
    {
        const float d = total * (float) k / (float) kPathPoints;
        while (seg < path.size() - 1 && cum[seg] < d) ++seg;
        const float segLen = cum[seg] - cum[seg - 1];
        const float t = segLen > 0.0f ? (d - cum[seg - 1]) / segLen : 0.0f;
        out.push_back (path[seg - 1] + (path[seg] - path[seg - 1]) * juce::jlimit (0.0f, 1.0f, t));
    }
    path = std::move (out);
}

juce::Point<float> ModState::pathMean() const
{
    if (path.empty()) return { 0.5f, 0.5f };
    juce::Point<float> m;
    for (auto& p : path) m += p;
    return m / (float) path.size();
}

// ============================================================================
// SlotState
// ============================================================================
void SlotState::setType (EffectType t)
{
    type = t;
    const auto& info = effectInfo (t);
    for (int i = 0; i < kNumParams; ++i)
        p[(size_t) i] = info.params[(size_t) i].defaultNorm();
}

juce::var SlotState::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("fx", effectInfo (type).id);
    juce::Array<juce::var> arr;
    for (auto v : p) arr.add (v);
    o->setProperty ("p", arr);
    o->setProperty ("mix", mix);
    o->setProperty ("pause", pause);
    o->setProperty ("ms", msName (ms));
    o->setProperty ("lock", writeProtect);
    return juce::var (o);
}

static float realToNormFromVar (const ParamSpec& spec, const juce::var& value)
{
    if (value.isString())
    {
        const auto label = value.toString();
        if (spec.unit == Unit::Toggle)
            return (label.equalsIgnoreCase ("ON") || label.equalsIgnoreCase ("TRUE")) ? 1.0f : 0.0f;
        const int idx = spec.choices.indexOf (label, true);
        if (idx >= 0) return spec.fromReal (spec.min + (float) idx);
        return spec.fromReal (label.getFloatValue());
    }
    if (value.isBool()) return (bool) value ? 1.0f : 0.0f;
    return spec.fromReal ((float) (double) value);
}

static int findParamByName (const EffectInfo& info, const juce::String& name)
{
    for (int i = 0; i < kNumParams; ++i)
        if (info.params[(size_t) i].used && info.params[(size_t) i].name.equalsIgnoreCase (name.trim()))
            return i;
    if (name.equalsIgnoreCase ("MIX") && info.mixParam >= 0) return info.mixParam;
    return -1;
}

SlotState slotFromPresetJson (const juce::var& v)
{
    SlotState s;
    if (! v.isObject()) return s;
    const auto type = effectTypeFromId (v.getProperty ("fx", "").toString());
    s.setType (type);
    if (type == EffectType::None) return s;
    const auto& info = effectInfo (type);

    if (auto* arr = v.getProperty ("p", {}).getArray())
        for (int i = 0; i < juce::jmin (kNumParams, arr->size()); ++i)
            s.p[(size_t) i] = clamp01 ((float) (double) arr->getReference (i));

    if (v.hasProperty ("amount"))
        s.p[(size_t) info.mainParam] = clamp01 ((float) (double) v.getProperty ("amount", 0.5));

    if (auto* params = v.getProperty ("params", {}).getDynamicObject())
    {
        for (auto& nv : params->getProperties())
        {
            const int idx = findParamByName (info, nv.name.toString());
            jassert (idx >= 0); // unknown parameter name in a preset
            if (idx >= 0)
                s.p[(size_t) idx] = realToNormFromVar (info.params[(size_t) idx], nv.value);
        }
    }

    s.mix = clamp01 ((float) (double) v.getProperty ("mix", 1.0));
    s.pause = (bool) v.getProperty ("pause", false);
    s.ms = msFromName (v.getProperty ("ms", "STEREO").toString());
    s.writeProtect = (bool) v.getProperty ("lock", false);
    return s;
}

SlotState SlotState::fromVar (const juce::var& v) { return slotFromPresetJson (v); }

// ============================================================================
// ProgramState
// ============================================================================
RackConfig ProgramState::rackConfig (int screening) const
{
    RackConfig c;
    for (int i = 0; i < kNumSlots; ++i)
    {
        c.slots[(size_t) i].type = slots[(size_t) i].type;
        c.slots[(size_t) i].ms = slots[(size_t) i].ms;
    }
    c.screening = screening;
    return c;
}

juce::var ProgramState::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("name", name);
    o->setProperty ("description", description);
    o->setProperty ("channel", channel);
    o->setProperty ("index", index);
    o->setProperty ("factory", factory);
    o->setProperty ("source", sourceName (source));
    o->setProperty ("focusMacro", focusMacro);

    juce::Array<juce::var> sl;
    for (auto& s : slots) sl.add (s.toVar());
    o->setProperty ("slots", sl);

    juce::Array<juce::var> mc;
    for (auto m : macros) mc.add (m);
    o->setProperty ("macros", mc);

    juce::Array<juce::var> mp;
    for (auto& m : maps)
    {
        auto* mo = new juce::DynamicObject();
        mo->setProperty ("macro", m.macro);
        mo->setProperty ("slot", m.slot);
        mo->setProperty ("param", m.param);
        mo->setProperty ("zero", m.atZero);
        mo->setProperty ("one", m.atOne);
        mp.add (juce::var (mo));
    }
    o->setProperty ("maps", mp);
    o->setProperty ("trim", trimDb);
    o->setProperty ("seed", juce::String ((juce::int64) seed));

    juce::Array<juce::var> tg;
    for (auto& t : tags) tg.add (t);
    o->setProperty ("tags", tg);

    auto* mod = new juce::DynamicObject();
    mod->setProperty ("mode", (int) this->mod.mode);
    mod->setProperty ("targetX", this->mod.targetX);
    mod->setProperty ("targetY", this->mod.targetY);
    mod->setProperty ("bars", this->mod.scribbleBars);
    mod->setProperty ("ssSpeed", this->mod.screensaverSpeed);
    mod->setProperty ("depth", this->mod.depth);
    juce::Array<juce::var> pts;
    for (auto& pt : this->mod.path) { juce::Array<juce::var> xy { pt.x, pt.y }; pts.add (xy); }
    mod->setProperty ("path", pts);
    o->setProperty ("mod", juce::var (mod));
    return juce::var (o);
}

static int macroFromVar (const juce::var& v)
{
    if (v.isString())
    {
        static const char* names[] = { "VILLAIN ARC", "CRASH OUT", "AURA", "DRIP", "KNOCK" };
        for (int i = 0; i < kNumMacros; ++i)
            if (v.toString().equalsIgnoreCase (names[i])) return i;
        return -1;
    }
    return juce::jlimit (-1, kNumMacros - 1, (int) v);
}

ProgramState ProgramState::fromVar (const juce::var& v)
{
    ProgramState s;
    if (! v.isObject()) return s;
    s.name = v.getProperty ("name", "Init").toString();
    s.description = v.getProperty ("description", "").toString();
    s.channel = (int) v.getProperty ("channel", -1);
    s.index = (int) v.getProperty ("index", -1);
    s.factory = (bool) v.getProperty ("factory", false);
    s.source = sourceFromName (v.getProperty ("source", "MELODY").toString());
    s.focusMacro = v.hasProperty ("focusMacro") ? macroFromVar (v.getProperty ("focusMacro", -1)) : -1;

    if (auto* sl = v.getProperty ("slots", {}).getArray())
        for (int i = 0; i < juce::jmin (kNumSlots, sl->size()); ++i)
            s.slots[(size_t) i] = SlotState::fromVar (sl->getReference (i));

    if (auto* mc = v.getProperty ("macros", {}).getArray())
        for (int i = 0; i < juce::jmin (kNumMacros, mc->size()); ++i)
            s.macros[(size_t) i] = clamp01 ((float) (double) mc->getReference (i));

    if (auto* mp = v.getProperty ("maps", {}).getArray())
    {
        for (auto& m : *mp)
        {
            MacroMapping mm;
            mm.macro = macroFromVar (m.getProperty ("macro", 0));
            mm.slot = juce::jlimit (0, kNumSlots - 1, (int) m.getProperty ("slot", 0));
            const auto paramVar = m.getProperty ("param", 0);
            const auto type = s.slots[(size_t) mm.slot].type;
            if (paramVar.isString())
            {
                // factory style: parameter name + real values
                const auto& info = effectInfo (type);
                mm.param = paramVar.toString().equalsIgnoreCase ("SLOT MIX") ? 8 : findParamByName (info, paramVar.toString());
                if (mm.param < 0 || mm.macro < 0) { jassertfalse; continue; }
                if (mm.param == 8)
                {
                    mm.atZero = clamp01 ((float) (double) m.getProperty ("zero", 0.0));
                    mm.atOne = clamp01 ((float) (double) m.getProperty ("one", 1.0));
                }
                else
                {
                    mm.atZero = realToNormFromVar (info.params[(size_t) mm.param], m.getProperty ("zero", 0.0));
                    mm.atOne = realToNormFromVar (info.params[(size_t) mm.param], m.getProperty ("one", 1.0));
                }
            }
            else
            {
                mm.param = juce::jlimit (0, 8, (int) paramVar);
                mm.atZero = clamp01 ((float) (double) m.getProperty ("zero", 0.0));
                mm.atOne = clamp01 ((float) (double) m.getProperty ("one", 1.0));
            }
            if (mm.macro >= 0) s.maps.push_back (mm);
        }
    }

    s.trimDb = juce::jlimit (-24.0f, 24.0f, (float) (double) v.getProperty ("trim", 0.0));
    s.seed = (uint64_t) v.getProperty ("seed", "0").toString().getLargeIntValue();
    if (auto* tg = v.getProperty ("tags", {}).getArray())
        for (auto& t : *tg) s.tags.add (t.toString());

    const auto mod = v.getProperty ("mod", {});
    if (mod.isObject())
    {
        s.mod.mode = (AvMode) juce::jlimit (0, 2, (int) mod.getProperty ("mode", 1));
        s.mod.targetX = (int) mod.getProperty ("targetX", ModTarget::macro (CrashOut));
        s.mod.targetY = (int) mod.getProperty ("targetY", ModTarget::macro (Aura));
        s.mod.scribbleBars = juce::jlimit (1, 4, (int) mod.getProperty ("bars", 1));
        s.mod.screensaverSpeed = juce::jlimit (0, 4, (int) mod.getProperty ("ssSpeed", 2));
        s.mod.depth = juce::jlimit (0.0f, 1.0f, (float) (double) mod.getProperty ("depth", 1.0));
        if (auto* pts = mod.getProperty ("path", {}).getArray())
            for (auto& pt : *pts)
                if (auto* xy = pt.getArray(); xy != nullptr && xy->size() == 2)
                    s.mod.path.push_back ({ clamp01 ((float) (double) xy->getReference (0)),
                                            clamp01 ((float) (double) xy->getReference (1)) });
    }
    return s;
}

juce::String ProgramState::toJson() const { return juce::JSON::toString (toVar(), false); }
ProgramState ProgramState::fromJson (const juce::String& json) { return fromVar (juce::JSON::parse (json)); }

bool ProgramState::sameSound (const ProgramState& o) const
{
    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto& a = slots[(size_t) i];
        const auto& b = o.slots[(size_t) i];
        if (a.type != b.type || a.ms != b.ms || a.pause != b.pause || a.writeProtect != b.writeProtect) return false;
        if (std::abs (a.mix - b.mix) > 1.0e-4f) return false;
        for (int k = 0; k < kNumParams; ++k)
            if (std::abs (a.p[(size_t) k] - b.p[(size_t) k]) > 1.0e-4f) return false;
    }
    for (int m = 0; m < kNumMacros; ++m)
        if (std::abs (macros[(size_t) m] - o.macros[(size_t) m]) > 1.0e-4f) return false;
    return std::abs (trimDb - o.trimDb) < 1.0e-3f && source == o.source;
}

// ============================================================================
// PresetBank
// ============================================================================
PresetBank::PresetBank() { loadFavourites(); }

void PresetBank::loadFactory (const juce::String& json)
{
    channels.clear();
    presets.clear();
    const auto root = juce::JSON::parse (json);
    auto* chans = root.getProperty ("channels", {}).getArray();
    if (chans == nullptr) { jassertfalse; return; }

    for (auto& c : *chans)
    {
        ChannelInfo ci;
        ci.number = (int) c.getProperty ("number", (int) channels.size() + 1);
        ci.name = c.getProperty ("name", "").toString();
        ci.description = c.getProperty ("description", "").toString();
        ci.colour = juce::Colour::fromString ("ff" + c.getProperty ("colour", "#ffffff").toString().removeCharacters ("#"));
        ci.source = sourceFromName (c.getProperty ("source", "MELODY").toString());

        std::vector<ProgramState> list;
        if (auto* ps = c.getProperty ("presets", {}).getArray())
        {
            for (auto& p : *ps)
            {
                auto s = ProgramState::fromVar (p);
                s.factory = true;
                s.channel = (int) channels.size();
                s.index = (int) list.size();
                if (! p.hasProperty ("source")) s.source = ci.source;
                list.push_back (std::move (s));
            }
        }
        channels.push_back (ci);
        presets.push_back (std::move (list));
    }
}

const std::vector<ProgramState>& PresetBank::getChannelPresets (int channel) const
{
    static const std::vector<ProgramState> empty;
    if (channel < 0 || channel >= (int) presets.size()) return empty;
    return presets[(size_t) channel];
}

const ProgramState* PresetBank::getFactory (int channel, int index) const
{
    const auto& list = getChannelPresets (channel);
    if (index < 0 || index >= (int) list.size()) return nullptr;
    return &list[(size_t) index];
}

int PresetBank::getNumFactoryPresets() const
{
    int n = 0;
    for (auto& l : presets) n += (int) l.size();
    return n;
}

juce::File PresetBank::getUserRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Killa").getChildFile ("Effector Killa");
}
juce::File PresetBank::root() const { return rootOverride != juce::File() ? rootOverride : getUserRoot(); }
juce::File PresetBank::getUserPresetDir() { return getUserRoot().getChildFile ("Presets"); }
juce::File PresetBank::getSlotPresetDir() { return getUserRoot().getChildFile ("Slot Presets"); }

static juce::String safeFileName (const juce::String& n)
{
    auto s = juce::File::createLegalFileName (n.trim());
    return s.isEmpty() ? juce::String ("Untitled") : s;
}

juce::Array<juce::File> PresetBank::getUserPresetFiles() const
{
    auto dir = root().getChildFile ("Presets");
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    return files;
}

bool PresetBank::saveUserPreset (const ProgramState& s, const juce::String& name) const
{
    auto dir = root().getChildFile ("Presets");
    if (! dir.createDirectory()) return false;
    auto copy = s;
    copy.name = name;
    copy.factory = false;
    return dir.getChildFile (safeFileName (name) + ".json").replaceWithText (juce::JSON::toString (copy.toVar()));
}

std::optional<ProgramState> PresetBank::loadUserPreset (const juce::File& f) const
{
    if (! f.existsAsFile()) return std::nullopt;
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject()) return std::nullopt;
    auto s = ProgramState::fromVar (v);
    s.factory = false;
    return s;
}

bool PresetBank::saveSlotPreset (const SlotState& s, const juce::String& name) const
{
    auto dir = root().getChildFile ("Slot Presets");
    if (! dir.createDirectory()) return false;
    auto v = s.toVar();
    v.getDynamicObject()->setProperty ("name", name);
    return dir.getChildFile (safeFileName (name) + ".json").replaceWithText (juce::JSON::toString (v));
}

juce::Array<juce::File> PresetBank::getSlotPresetFiles() const
{
    auto files = root().getChildFile ("Slot Presets").findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    return files;
}

std::optional<SlotState> PresetBank::loadSlotPreset (const juce::File& f) const
{
    if (! f.existsAsFile()) return std::nullopt;
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject()) return std::nullopt;
    return SlotState::fromVar (v);
}

juce::String PresetBank::keyFor (const ProgramState& s)
{
    if (s.factory) return "factory:" + juce::String (s.channel + 1) + "/" + s.name;
    return "user:" + s.name;
}

bool PresetBank::isFavourite (const juce::String& key) const { return favourites.contains (key); }

void PresetBank::setFavourite (const juce::String& key, bool fav)
{
    if (fav) favourites.addIfNotAlreadyThere (key);
    else favourites.removeString (key);
    saveFavourites();
}

void PresetBank::loadFavourites()
{
    favourites.clear();
    const auto f = root().getChildFile ("favourites.json");
    const auto parsed = juce::JSON::parse (f.loadFileAsString());
    if (auto* arr = parsed.getArray())
        for (auto& v : *arr) favourites.add (v.toString());
}

void PresetBank::saveFavourites() const
{
    const auto dir = root();
    if (! dir.createDirectory()) return;
    juce::Array<juce::var> arr;
    for (auto& s : favourites) arr.add (s);
    dir.getChildFile ("favourites.json").replaceWithText (juce::JSON::toString (arr));
}

} // namespace ek
