/*
    TrapWin BEND DEMON
    PresetManager.h - factory presets + user presets (XML files).

    Presets never store the hosted synth: you can flip through presets while keeping your Serum
    patch. The host project state (getStateInformation) stores everything, including the hosted
    synth.

    2.0: PATTERN presets (factory) also load a drawn pattern, the internal sound and DRAW mode.
    GESTURE presets (the 1.x ones) only change the gesture recipe and leave the pattern alone.
    User presets store the parameters AND the pattern (undoable when loaded).

    User presets live in:
      Windows: %APPDATA%\TrapWin\BEND DEMON\Presets\<name>.bdpreset
      macOS:   ~/Library/Application Support/TrapWin/BEND DEMON/Presets/<name>.bdpreset
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bd { class PatternStore; }

namespace bd
{

class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState&, PatternStore&);

    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;   // param id -> plain value
        int pattern = -1;                                     // tools::makeFactoryPattern index, -1 = gesture preset
    };

    /** Factory presets 0 .. numPatternPresets()-1 carry a pattern. */
    static int numPatternPresets();

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
    PatternStore& patterns;
    juce::String currentName { "Init" };
};

} // namespace bd
