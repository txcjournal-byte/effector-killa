#include "PresetManager.h"
#include "../Parameters.h"
#include "../host/HostedPluginManager.h"
#include "../PatternStore.h"
#include "../engine/PatternTools.h"
#include "../engine/InternalSynth.h"

namespace bd
{

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, PatternStore& p) : state (s), patterns (p) {}

int PresetManager::numPatternPresets()
{
    int n = 0;
    for (auto& p : getFactoryPresets()) if (p.pattern >= 0) ++n;
    return n;
}

juce::File PresetManager::getUserPresetDirectory()
{
    auto d = HostedPluginManager::getAppDataDirectory().getChildFile ("Presets");
    d.createDirectory();
    return d;
}

const std::vector<PresetManager::FactoryPreset>& PresetManager::getFactoryPresets()
{
    using namespace ids;

    // Only values that differ from the parameter defaults are listed; everything else is reset.
    using S = InternalSynth;
    auto pat = [] (int index, int sound, std::vector<std::pair<juce::String, float>> v)
    {
        // C minor so the magnet / scale tools match the pattern; DRAW mode; the sound
        v.insert (v.begin(), { { mode, 1 }, { internalSound, (float) sound }, { scaleKey, 0 }, { scaleType, 1 } });
        return FactoryPreset { tools::factoryPatternName (index), std::move (v), index };
    };

    static const std::vector<FactoryPreset> presets {
        // --- 2.0 pattern presets (pattern + sound + gesture recipe used by APPLY GESTURE)
        pat (0, S::glideLead,    { { glideTime, 90 }, { vibDepth, 30 }, { diveAmount, 5 } }),
        pat (1, S::eightOhEight, { { glideTime, 80 }, { diveAmount, 12 }, { diveTime, 450 } }),
        pat (2, S::pad,          { { chordGlideOn, 1 }, { glideOn, 0 }, { glideTime, 400 }, { glideCurve, 2 } }),
        pat (3, S::glideLead,    { { vibDepth, 40 }, { vibRate, 5.5f } }),
        pat (4, S::bell,         { { riseOn, 0 }, { diveAmount, 2 } }),
        pat (5, S::pluck,        { { glideTime, 40 } }),
        pat (6, S::subBass,      { { wobbleDepth, 0.5f }, { wobbleDiv, 6 } }),
        pat (7, S::pad,          { { glideTime, 600 } }),
        pat (8, S::pad,          { { glideTime, 500 }, { glideCurve, 2 } }),

        // --- gesture presets (1.x): change the gestures only, never the pattern
        { "Init", {} },

        { "Dive Bass", {
            { glideOn, 1 }, { glideTime, 90 }, { glideWindow, 40 },
            { diveOn, 1 }, { diveAmount, 12 }, { diveTime, 450 }, { diveCurve, 1 },
            { pressure, 0.9f }, { pressureShape, 0 }, { timbre, 0.35f }, { timbreShape, 2 } } },

        { "Liquid Lead", {
            { glideOn, 1 }, { glideTime, 180 }, { glideWindow, 80 }, { glideCurve, 2 },
            { vibratoOn, 1 }, { vibRate, 5.2f }, { vibDepth, 40 }, { vibDelay, 300 },
            { pressure, 0.85f }, { pressureShape, 1 }, { pressureTime, 500 },
            { timbre, 0.7f }, { timbreShape, 1 }, { timbreTime, 900 } } },

        { "Chord Melt", {
            { glideOn, 0 }, { chordGlideOn, 1 }, { glideTime, 350 }, { glideWindow, 120 }, { glideCurve, 2 },
            { chordWindow, 15 }, { vibratoOn, 1 }, { vibDepth, 12 }, { vibRate, 4.0f }, { vibDelay, 600 },
            { pressureShape, 1 }, { pressureTime, 800 }, { timbre, 0.6f }, { timbreTime, 1500 } } },

        { "Vibrato Soul", {
            { glideOn, 1 }, { glideTime, 70 },
            { vibratoOn, 1 }, { vibRate, 6.0f }, { vibDepth, 55 }, { vibDelay, 220 },
            { pressureShape, 1 }, { pressureTime, 400 }, { velToPressure, 0.8f } } },

        { "Trill Hook", {
            { glideOn, 0 }, { trillOn, 1 }, { trillDiv, 10 },
            { scaleLockOn, 1 }, { scaleKey, 9 }, { scaleType, 1 },
            { pressure, 0.8f }, { pressureShape, 0 }, { timbre, 0.55f } } },

        { "Wobble Bass", {
            { glideOn, 1 }, { glideTime, 60 },
            { wobbleOn, 1 }, { wobbleDepth, 0.6f }, { wobbleDiv, 6 },
            { diveOn, 1 }, { diveAmount, 7 }, { diveSync, 1 }, { diveDiv, 6 },
            { timbre, 0.8f }, { timbreShape, 2 }, { timbreTime, 300 } } },

        { "Shaky Pluck", {
            { glideOn, 0 }, { shakeOn, 1 }, { shakeDepth, 22 }, { shakeRate, 16 },
            { pressureShape, 2 }, { pressureTime, 350 }, { timbreShape, 2 } } },

        { "Phrygian Rise", {
            { glideOn, 1 }, { riseOn, 1 }, { diveAmount, 5 }, { diveTime, 300 },
            { scaleLockOn, 1 }, { scaleKey, 4 }, { scaleType, 3 } } }
    };

    return presets;
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray s;
    for (auto& p : getFactoryPresets()) s.add (p.name);
    return s;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray s;
    for (auto& f : getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.bdpreset"))
        s.add (f.getFileNameWithoutExtension());
    s.sort (true);
    return s;
}

void PresetManager::setPlain (const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void PresetManager::resetToDefaults()
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            // Routing-related settings are not part of a gesture recipe (and gesture presets keep
            // the mode / sound; pattern presets set them explicitly).
            const auto id = rp->getParameterID();
            if (id == ids::midiOutMode || id == ids::voices || id == ids::bendRange || id == ids::outputGain
                || id == ids::mode || id == ids::seqMono || id == ids::internalSound)
                continue;

            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

void PresetManager::loadFactory (int index)
{
    const auto& all = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) all.size()))
        return;

    const auto& preset = all[(size_t) index];
    resetToDefaults();
    for (auto& [id, v] : preset.values)
        setPlain (id, v);

    if (preset.pattern >= 0)
    {
        patterns.perform (tools::makeFactoryPattern (preset.pattern), "Load pattern: " + preset.name, true);
        patterns.clearSelection();
    }

    currentName = preset.name;
}

bool PresetManager::saveUser (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    juce::XmlElement xml ("BENDDEMONPRESET");
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", JucePlugin_VersionString);

    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id == ids::midiOutMode || id == ids::outputGain)
                continue;

            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", id);
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }

    xml.addChildElement (patterns.createXml().release());

    const bool ok = xml.writeTo (getUserPresetDirectory().getChildFile (name + ".bdpreset"));
    if (ok) currentName = name;
    return ok;
}

bool PresetManager::loadUser (const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (getUserPresetDirectory().getChildFile (name + ".bdpreset"));
    if (xml == nullptr || ! xml->hasTagName ("BENDDEMONPRESET"))
        return false;

    resetToDefaults();
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        setPlain (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));

    if (auto* pe = xml->getChildByName (Pattern::xmlTag))
        patterns.perform (Pattern::fromXml (*pe), "Load preset: " + name, true);

    currentName = name;
    return true;
}

bool PresetManager::deleteUser (const juce::String& name)
{
    return getUserPresetDirectory().getChildFile (name + ".bdpreset").deleteFile();
}

} // namespace bd
