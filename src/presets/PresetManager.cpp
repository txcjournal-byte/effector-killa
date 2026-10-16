#include "PresetManager.h"
#include "../Parameters.h"
#include "../host/HostedPluginManager.h"

namespace bd
{

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

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
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        { "808 Dive", {
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
            // Routing-related settings are not part of a gesture recipe.
            const auto id = rp->getParameterID();
            if (id == ids::midiOutMode || id == ids::voices || id == ids::bendRange || id == ids::outputGain)
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

    resetToDefaults();
    for (auto& [id, v] : all[(size_t) index].values)
        setPlain (id, v);

    currentName = all[(size_t) index].name;
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

    currentName = name;
    return true;
}

bool PresetManager::deleteUser (const juce::String& name)
{
    return getUserPresetDirectory().getChildFile (name + ".bdpreset").deleteFile();
}

} // namespace bd
