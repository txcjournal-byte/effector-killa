/*
    TrapWin BEND DEMON
    PresetManager.h - factory gesture presets + user presets (XML files).

    A gesture preset stores ONLY BEND DEMON's parameters (the gesture recipe), never the hosted
    synth: you can flip through presets while keeping your Serum patch. The host project state
    (getStateInformation) stores everything, including the hosted synth.

    User presets live in:
      Windows: %APPDATA%\TrapWin\BEND DEMON\Presets\<name>.bdpreset
      macOS:   ~/Library/Application Support/TrapWin/BEND DEMON/Presets/<name>.bdpreset
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bd
{

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;   // param id -> plain value
    };

    static const std::vector<FactoryPreset>& getFactoryPresets();

    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;

    void loadFactory (int index);
    bool loadUser (const juce::String& name);
    bool saveUser (const juce::String& name);
    bool deleteUser (const juce::String& name);

    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; }

    static juce::File getUserPresetDirectory();

private:
    void resetToDefaults();
    void setPlain (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState& state;
    juce::String currentName { "Init" };
};

} // namespace bd
