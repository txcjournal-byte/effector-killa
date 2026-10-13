#include "Presets.h"
#include "Parameters.h"

using namespace juce;

namespace
{
    const Identifier presetProperty { "presetName" };

    struct Factory
    {
        const char* name;
        const char* category;
        int style;
        std::vector<std::pair<const char*, float>> values;
    };

    using namespace ParamIDs;

    const std::vector<Factory>& factoryPresets()
    {
        static const std::vector<Factory> list = {
            // ---------------- 808
            { "Phone Killer", "808", 0, { { satMode, 1.0f }, { driveAmount, 40.0f }, { midFocus, 10.0f }, { focusFreq, 650.0f }, { clipDrive, 6.0f }, { clipKnee, 0.85f }, { crossoverFreq, 120.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            { "Clean Sub", "808", 0, { { satMode, 0.0f }, { driveAmount, 6.0f }, { midFocus, 2.0f }, { focusFreq, 550.0f }, { clipDrive, 2.0f }, { clipKnee, 0.95f }, { crossoverFreq, 110.0f }, { hit, 60.0f }, { duckRelease, 45.0f } } },
            { "Distorted Slide", "808", 0, { { satMode, 2.0f }, { driveAmount, 70.0f }, { midFocus, 8.0f }, { focusFreq, 600.0f }, { clipDrive, 9.0f }, { clipKnee, 0.8f }, { crossoverFreq, 130.0f }, { hit, 80.0f }, { duckRelease, 45.0f } } },
            { "Tape Boom", "808", 0, { { satMode, 0.0f }, { driveAmount, 28.0f }, { midFocus, 4.0f }, { focusFreq, 500.0f }, { clipDrive, 5.0f }, { clipKnee, 0.7f }, { crossoverFreq, 120.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            { "Rage Grit", "808", 0, { { satMode, 1.0f }, { driveAmount, 85.0f }, { midFocus, 11.0f }, { focusFreq, 700.0f }, { clipDrive, 12.0f }, { clipKnee, 0.9f }, { crossoverFreq, 140.0f }, { hit, 85.0f }, { duckRelease, 45.0f } } },
            { "Drill Slide", "808", 0, { { satMode, 0.0f }, { driveAmount, 35.0f }, { midFocus, 6.0f }, { focusFreq, 620.0f }, { clipDrive, 6.0f }, { clipKnee, 0.8f }, { crossoverFreq, 120.0f }, { hit, 75.0f }, { duckRelease, 45.0f } } },
            // ---------------- KICK
            { "Knock", "KICK", 0, { { satMode, 1.0f }, { driveAmount, 25.0f }, { midFocus, 5.0f }, { focusFreq, 3200.0f }, { clipDrive, 6.0f }, { clipKnee, 0.85f }, { crossoverFreq, 100.0f }, { hit, 90.0f }, { duckRelease, 45.0f } } },
            { "Punch Through", "KICK", 0, { { satMode, 1.0f }, { driveAmount, 35.0f }, { midFocus, 7.0f }, { focusFreq, 2500.0f }, { clipDrive, 8.0f }, { clipKnee, 0.9f }, { crossoverFreq, 100.0f }, { hit, 95.0f }, { duckRelease, 45.0f } } },
            { "Thump", "KICK", 0, { { satMode, 0.0f }, { driveAmount, 20.0f }, { midFocus, 4.0f }, { focusFreq, 90.0f }, { clipDrive, 4.0f }, { clipKnee, 0.7f }, { crossoverFreq, 90.0f }, { hit, 80.0f }, { duckRelease, 45.0f } } },
            { "Clipped Kick", "KICK", 0, { { satMode, 1.0f }, { driveAmount, 30.0f }, { midFocus, 6.0f }, { focusFreq, 3000.0f }, { clipDrive, 12.0f }, { clipKnee, 0.95f }, { crossoverFreq, 100.0f }, { hit, 90.0f }, { duckRelease, 45.0f } } },
            { "Rage Kick", "KICK", 0, { { satMode, 2.0f }, { driveAmount, 60.0f }, { midFocus, 9.0f }, { focusFreq, 2800.0f }, { clipDrive, 10.0f }, { clipKnee, 0.9f }, { crossoverFreq, 110.0f }, { hit, 95.0f }, { duckRelease, 45.0f } } },
            { "Soft Pillow", "KICK", 0, { { satMode, 0.0f }, { driveAmount, 10.0f }, { midFocus, 2.0f }, { focusFreq, 1800.0f }, { clipDrive, 3.0f }, { clipKnee, 0.6f }, { crossoverFreq, 90.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            // ---------------- SNARE
            { "Crack", "SNARE", 0, { { satMode, 1.0f }, { driveAmount, 30.0f }, { midFocus, 8.0f }, { focusFreq, 4200.0f }, { clipDrive, 9.0f }, { clipKnee, 0.9f }, { crossoverFreq, 150.0f }, { hit, 90.0f }, { duckRelease, 45.0f } } },
            { "Fat Body", "SNARE", 0, { { satMode, 0.0f }, { driveAmount, 25.0f }, { midFocus, 6.0f }, { focusFreq, 200.0f }, { clipDrive, 6.0f }, { clipKnee, 0.75f }, { crossoverFreq, 150.0f }, { hit, 80.0f }, { duckRelease, 45.0f } } },
            { "Slam", "SNARE", 0, { { satMode, 1.0f }, { driveAmount, 45.0f }, { midFocus, 7.0f }, { focusFreq, 3500.0f }, { clipDrive, 12.0f }, { clipKnee, 0.95f }, { crossoverFreq, 160.0f }, { hit, 95.0f }, { duckRelease, 45.0f } } },
            { "Lo-Fi Snare", "SNARE", 0, { { satMode, 0.0f }, { driveAmount, 40.0f }, { midFocus, 5.0f }, { focusFreq, 1500.0f }, { clipDrive, 5.0f }, { clipKnee, 0.6f }, { crossoverFreq, 150.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            { "Drill Rim", "SNARE", 0, { { satMode, 1.0f }, { driveAmount, 35.0f }, { midFocus, 9.0f }, { focusFreq, 5000.0f }, { clipDrive, 8.0f }, { clipKnee, 0.9f }, { crossoverFreq, 180.0f }, { hit, 90.0f }, { duckRelease, 45.0f } } },
            { "Grit Snare", "SNARE", 0, { { satMode, 2.0f }, { driveAmount, 55.0f }, { midFocus, 6.0f }, { focusFreq, 2500.0f }, { clipDrive, 10.0f }, { clipKnee, 0.85f }, { crossoverFreq, 160.0f }, { hit, 85.0f }, { duckRelease, 45.0f } } },
            // ---------------- CLAP
            { "Wide Clap", "CLAP", 0, { { satMode, 0.0f }, { driveAmount, 20.0f }, { midFocus, 6.0f }, { focusFreq, 1500.0f }, { clipDrive, 6.0f }, { clipKnee, 0.8f }, { crossoverFreq, 200.0f }, { hit, 80.0f }, { duckRelease, 45.0f } } },
            { "Crushed Clap", "CLAP", 0, { { satMode, 1.0f }, { driveAmount, 45.0f }, { midFocus, 8.0f }, { focusFreq, 1800.0f }, { clipDrive, 11.0f }, { clipKnee, 0.9f }, { crossoverFreq, 200.0f }, { hit, 90.0f }, { duckRelease, 45.0f } } },
            { "Vintage Clap", "CLAP", 0, { { satMode, 0.0f }, { driveAmount, 30.0f }, { midFocus, 5.0f }, { focusFreq, 1200.0f }, { clipDrive, 5.0f }, { clipKnee, 0.65f }, { crossoverFreq, 200.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            { "Stack", "CLAP", 0, { { satMode, 1.0f }, { driveAmount, 30.0f }, { midFocus, 7.0f }, { focusFreq, 2200.0f }, { clipDrive, 8.0f }, { clipKnee, 0.85f }, { crossoverFreq, 200.0f }, { hit, 85.0f }, { duckRelease, 45.0f } } },
            { "Dirty Clap", "CLAP", 0, { { satMode, 2.0f }, { driveAmount, 50.0f }, { midFocus, 6.0f }, { focusFreq, 1600.0f }, { clipDrive, 9.0f }, { clipKnee, 0.85f }, { crossoverFreq, 200.0f }, { hit, 85.0f }, { duckRelease, 45.0f } } },
            { "Air Clap", "CLAP", 0, { { satMode, 0.0f }, { driveAmount, 15.0f }, { midFocus, 8.0f }, { focusFreq, 6000.0f }, { clipDrive, 4.0f }, { clipKnee, 0.8f }, { crossoverFreq, 200.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            // ---------------- HATS
            { "Crisp Hats", "HATS", 0, { { satMode, 0.0f }, { driveAmount, 10.0f }, { midFocus, 6.0f }, { focusFreq, 8000.0f }, { clipDrive, 3.0f }, { clipKnee, 0.85f }, { crossoverFreq, 200.0f }, { hit, 80.0f }, { duckRelease, 45.0f } } },
            { "Tape Hats", "HATS", 0, { { satMode, 0.0f }, { driveAmount, 25.0f }, { midFocus, 3.0f }, { focusFreq, 6000.0f }, { clipDrive, 4.0f }, { clipKnee, 0.6f }, { crossoverFreq, 200.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            { "Bitey Hats", "HATS", 0, { { satMode, 1.0f }, { driveAmount, 30.0f }, { midFocus, 8.0f }, { focusFreq, 5500.0f }, { clipDrive, 6.0f }, { clipKnee, 0.9f }, { crossoverFreq, 200.0f }, { hit, 90.0f }, { duckRelease, 45.0f } } },
            { "Smooth Hats", "HATS", 0, { { satMode, 0.0f }, { driveAmount, 8.0f }, { midFocus, 2.0f }, { focusFreq, 7000.0f }, { clipDrive, 2.0f }, { clipKnee, 0.55f }, { crossoverFreq, 200.0f }, { hit, 60.0f }, { duckRelease, 45.0f } } },
            { "Roll Hats", "HATS", 0, { { satMode, 1.0f }, { driveAmount, 20.0f }, { midFocus, 6.0f }, { focusFreq, 6500.0f }, { clipDrive, 5.0f }, { clipKnee, 0.85f }, { crossoverFreq, 200.0f }, { hit, 95.0f }, { duckRelease, 45.0f } } },
            { "Open Hat Sizzle", "HATS", 0, { { satMode, 0.0f }, { driveAmount, 18.0f }, { midFocus, 7.0f }, { focusFreq, 7500.0f }, { clipDrive, 4.0f }, { clipKnee, 0.75f }, { crossoverFreq, 200.0f }, { hit, 60.0f }, { duckRelease, 45.0f } } },
            // ---------------- PERC
            { "Pop", "PERC", 0, { { satMode, 1.0f }, { driveAmount, 25.0f }, { midFocus, 7.0f }, { focusFreq, 1100.0f }, { clipDrive, 6.0f }, { clipKnee, 0.85f }, { crossoverFreq, 150.0f }, { hit, 90.0f }, { duckRelease, 45.0f } } },
            { "Wood Knock", "PERC", 0, { { satMode, 0.0f }, { driveAmount, 20.0f }, { midFocus, 6.0f }, { focusFreq, 900.0f }, { clipDrive, 5.0f }, { clipKnee, 0.8f }, { crossoverFreq, 150.0f }, { hit, 85.0f }, { duckRelease, 45.0f } } },
            { "Metal Perc", "PERC", 0, { { satMode, 2.0f }, { driveAmount, 40.0f }, { midFocus, 8.0f }, { focusFreq, 2500.0f }, { clipDrive, 8.0f }, { clipKnee, 0.85f }, { crossoverFreq, 170.0f }, { hit, 85.0f }, { duckRelease, 45.0f } } },
            { "Shaker Air", "PERC", 0, { { satMode, 0.0f }, { driveAmount, 10.0f }, { midFocus, 6.0f }, { focusFreq, 7000.0f }, { clipDrive, 3.0f }, { clipKnee, 0.7f }, { crossoverFreq, 200.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            { "Bongo Thump", "PERC", 0, { { satMode, 0.0f }, { driveAmount, 25.0f }, { midFocus, 5.0f }, { focusFreq, 300.0f }, { clipDrive, 5.0f }, { clipKnee, 0.75f }, { crossoverFreq, 120.0f }, { hit, 80.0f }, { duckRelease, 45.0f } } },
            { "Glitch Perc", "PERC", 0, { { satMode, 2.0f }, { driveAmount, 60.0f }, { midFocus, 9.0f }, { focusFreq, 1800.0f }, { clipDrive, 10.0f }, { clipKnee, 0.9f }, { crossoverFreq, 180.0f }, { hit, 95.0f }, { duckRelease, 45.0f } } },
            // ---------------- DRUM BUS
            { "Glue", "DRUM BUS", 0, { { satMode, 1.0f }, { driveAmount, 15.0f }, { midFocus, 2.0f }, { focusFreq, 800.0f }, { clipDrive, 4.0f }, { clipKnee, 0.6f }, { crossoverFreq, 120.0f }, { hit, 40.0f }, { duckRelease, 45.0f } } },
            { "Punch Bus", "DRUM BUS", 0, { { satMode, 1.0f }, { driveAmount, 25.0f }, { midFocus, 4.0f }, { focusFreq, 2500.0f }, { clipDrive, 7.0f }, { clipKnee, 0.85f }, { crossoverFreq, 120.0f }, { hit, 70.0f }, { duckRelease, 45.0f } } },
            { "Loud Bus", "DRUM BUS", 0, { { satMode, 1.0f }, { driveAmount, 35.0f }, { midFocus, 5.0f }, { focusFreq, 1500.0f }, { clipDrive, 11.0f }, { clipKnee, 0.9f }, { crossoverFreq, 120.0f }, { hit, 60.0f }, { duckRelease, 45.0f } } },
            { "Tape Bus", "DRUM BUS", 0, { { satMode, 0.0f }, { driveAmount, 30.0f }, { midFocus, 3.0f }, { focusFreq, 1000.0f }, { clipDrive, 5.0f }, { clipKnee, 0.6f }, { crossoverFreq, 110.0f }, { hit, 40.0f }, { duckRelease, 45.0f } } },
            { "Crush Bus", "DRUM BUS", 0, { { satMode, 2.0f }, { driveAmount, 50.0f }, { midFocus, 6.0f }, { focusFreq, 1800.0f }, { clipDrive, 10.0f }, { clipKnee, 0.85f }, { crossoverFreq, 130.0f }, { hit, 60.0f }, { duckRelease, 45.0f } } },
            { "Phone Check Bus", "DRUM BUS", 0, { { satMode, 1.0f }, { driveAmount, 30.0f }, { midFocus, 8.0f }, { focusFreq, 1200.0f }, { clipDrive, 6.0f }, { clipKnee, 0.8f }, { crossoverFreq, 120.0f }, { hit, 50.0f }, { duckRelease, 45.0f } } },
        };
        return list;
    }

    File favouritesFile()
    {
        return PresetManager::userFolder().getChildFile ("favourites.txt");
    }

    File presetFileFor (const String& name)
    {
        return PresetManager::userFolder().getChildFile (File::createLegalFileName (name) + ".drumkilla");
    }
}

//==============================================================================
PresetManager::PresetManager (AudioProcessorValueTreeState& state) : apvts (state)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), this);
    rescan();
}

PresetManager::~PresetManager()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

const StringArray& PresetManager::categories()
{
    static const StringArray list { "808", "KICK", "SNARE", "CLAP", "HATS", "PERC", "DRUM BUS" };
    return list;
}

void PresetManager::toggleFavourite (const String& name)
{
    if (favourites.contains (name)) favourites.removeString (name);
    else                            favourites.add (name);

    userFolder().createDirectory();
    favouritesFile().replaceWithText (favourites.joinIntoString ("\n"));
}

File PresetManager::userFolder()
{
   #if JUCE_MAC
    return File::getSpecialLocation (File::userMusicDirectory).getChildFile ("TrapWin/DRUM KILLA/Presets");
   #else
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("TrapWin/DRUM KILLA/Presets");
   #endif
}

void PresetManager::rescan()
{
    presets.clear();

    for (auto& f : factoryPresets())
        presets.add ({ f.name, f.category, true, {} });

    for (auto& file : userFolder().findChildFiles (File::findFiles, false, "*.drumkilla"))
        presets.add ({ file.getFileNameWithoutExtension(), "USER", false, file });

    favourites = StringArray::fromLines (favouritesFile().loadFileAsString());
    favourites.removeEmptyStrings();
}

void PresetManager::parameterChanged (const String& id, float)
{
    // may be called from the audio thread (automation): only set a flag, the UI polls it
    if (! loading.load() && id != ParamIDs::bypass && id != ParamIDs::phonePreview)
        modified = true;
}

void PresetManager::applyValues (const NamedValueSet& values)
{
    if (apvts.undoManager != nullptr)
        apvts.undoManager->beginNewTransaction ("Load preset");

    loading = true;

    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<RangedAudioParameter*> (p);
        if (rp == nullptr || rp->getParameterID() == ParamIDs::bypass || rp->getParameterID() == ParamIDs::phonePreview)
            continue;

        const auto id = rp->getParameterID();
        const auto normalised = values.contains (id) ? rp->convertTo0to1 ((float) values[id]) : rp->getDefaultValue();
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (normalised);
        rp->endChangeGesture();
    }

    loading = false;
    modified = false;
}

void PresetManager::load (int index)
{
    if (! isPositiveAndBelow (index, presets.size()))
        return;

    const auto& info = presets.getReference (index);
    NamedValueSet values;

    if (info.factory)
    {
        const auto& f = factoryPresets()[(size_t) index];
        for (auto& [id, v] : f.values)
            values.set (id, v);
    }
    else
    {
        const auto json = JSON::parse (info.file);
        if (auto* params = json["params"].getDynamicObject())
            values = params->getProperties();
        // future format versions would be migrated here (json["version"])
    }

    applyValues (values);
    apvts.state.setProperty (presetProperty, info.name, nullptr);
}

void PresetManager::loadNext (int delta)
{
    const auto n = presets.size();
    if (n == 0) return;
    const auto current = getCurrentIndex();
    load (current < 0 ? 0 : ((current + delta) % n + n) % n);
}

void PresetManager::revert()
{
    if (const auto index = getCurrentIndex(); index >= 0)
        load (index);
}

void PresetManager::init()
{
    applyValues ({});
    apvts.state.setProperty (presetProperty, "Init", nullptr);
}

var PresetManager::toJson (const String& name) const
{
    auto* params = new DynamicObject();
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            if (rp->getParameterID() != ParamIDs::bypass && rp->getParameterID() != ParamIDs::phonePreview)
                params->setProperty (rp->getParameterID(), rp->convertFrom0to1 (rp->getValue()));

    auto* root = new DynamicObject();
    root->setProperty ("format", "808k");
    root->setProperty ("version", formatVersion);
    root->setProperty ("name", name);
    root->setProperty ("category", "User");
    root->setProperty ("params", var (params));
    return var (root);
}

bool PresetManager::saveUser (const String& name)
{
    const auto clean = name.trim();
    if (clean.isEmpty()) return false;

    userFolder().createDirectory();
    const auto file = presetFileFor (clean);
    if (! file.replaceWithText (JSON::toString (toJson (clean))))
        return false;

    rescan();
    apvts.state.setProperty (presetProperty, clean, nullptr);
    modified = false;
    return true;
}

String PresetManager::getCurrentName() const
{
    return apvts.state.getProperty (presetProperty, "Init").toString();
}

int PresetManager::getCurrentIndex() const
{
    const auto name = getCurrentName();
    for (int i = 0; i < presets.size(); ++i)
        if (presets.getReference (i).name == name)
            return i;
    return -1;
}

void PresetManager::restoreFromState()
{
    modified = false;
}

void PresetManager::toggleAB()
{
    abState[abSlot] = apvts.copyState();
    abSlot = 1 - abSlot;

    if (abState[abSlot].isValid())
    {
        loading = true;
        apvts.replaceState (abState[abSlot].createCopy());
        loading = false;
    }
    else
    {
        abState[abSlot] = apvts.copyState();   // first switch: B starts as a copy of A
    }
}

void PresetManager::copyToOther()
{
    abState[1 - abSlot] = apvts.copyState();
}
