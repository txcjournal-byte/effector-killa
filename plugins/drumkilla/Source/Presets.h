#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct PresetInfo
{
    juce::String name;
    juce::String category;     // one of PresetManager::categories() or "USER"
    bool factory = true;
    juce::File file;           // user presets only
};

// Factory presets live in the binary, user presets as .drumkilla (JSON) files.
// All methods must be called on the message thread.
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);
    ~PresetManager() override;

    static constexpr int formatVersion = 1;
    static juce::File userFolder();
    static const juce::StringArray& categories();   // factory categories in display order

    bool isFavourite (const juce::String& name) const { return favourites.contains (name); }
    void toggleFavourite (const juce::String& name);

    const juce::Array<PresetInfo>& getPresets() const noexcept { return presets; }
    void rescan();

    void load (int index);
    void loadNext (int delta);
    void revert();
    void init();
    bool saveUser (const juce::String& name);

    juce::String getCurrentName() const;
    int getCurrentIndex() const;
    bool isModified() const noexcept { return modified.load(); }

    // restore name after the host loaded a project
    void restoreFromState();

    // A/B comparison: two independent snapshots of all settings
    void toggleAB();
    void copyToOther();
    bool isOnB() const noexcept { return abSlot == 1; }

private:
    void parameterChanged (const juce::String&, float) override;
    void applyValues (const juce::NamedValueSet& values);
    juce::var toJson (const juce::String& name) const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<PresetInfo> presets;
    std::atomic<bool> modified { false };
    std::atomic<bool> loading { false };
    juce::ValueTree abState[2];
    juce::StringArray favourites;
    int abSlot = 0;
};
