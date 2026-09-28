#pragma once

// Program state (= everything a preset stores), JSON (de)serialisation, channels, factory bank,
// user presets, slot presets and favourites (STAFF PICK).

#include "Rack.h"
#include <optional>

namespace ek
{
enum class Source : int { Melody = 0, Vox, Bass808, Drums, FX, Bus, Count };
juce::String sourceName (Source s);
Source sourceFromName (const juce::String& s, Source fallback = Source::Melody);

// ----------------------------------------------------------------------------
// Modulation targets: macros 0..4, or a slot parameter (slot 0..7, param 0..7, 8 = slot MIX)
// ----------------------------------------------------------------------------
struct ModTarget
{
    static int macro (int m) noexcept { return m; }
    static int slotParam (int slot, int param) noexcept { return 100 + slot * 10 + param; }
    static bool isMacro (int t) noexcept { return t >= 0 && t < kNumMacros; }
    static bool isSlotParam (int t) noexcept { return t >= 100 && t < 100 + kNumSlots * 10; }
    static int slotOf (int t) noexcept { return (t - 100) / 10; }
    static int paramOf (int t) noexcept { return (t - 100) % 10; }
};

enum class AvMode : int { Remote = 0, Scribble, Screensaver };

struct ModState
{
    AvMode mode = AvMode::Scribble;
    int targetX = ModTarget::macro (CrashOut);
    int targetY = ModTarget::macro (Aura);
    std::vector<juce::Point<float>> path;  // AV2 drawing, normalised 0..1 (y up)
    int scribbleBars = 1;                  // 1, 2 or 4
    int screensaverSpeed = 2;              // index into {1/4, 1/2, 1, 2, 4} bars per crossing
    float depth = 1.0f;                    // modulation depth scale

    static constexpr int kPathPoints = 64;
    void resamplePath();                   // equal arc-length resampling to kPathPoints
    juce::Point<float> pathMean() const;
};

float screensaverBarsForIndex (int index);

struct MacroMapping
{
    int macro = 0;
    int slot = 0;
    int param = 0;       // 0..7, 8 = slot MIX
    float atZero = 0.0f; // normalised value when the macro is at 0 (base value at 0.5)
    float atOne = 1.0f;  // normalised value when the macro is at 1
};

struct SlotState
{
    EffectType type = EffectType::None;
    std::array<float, kNumParams> p {};
    float mix = 1.0f;
    bool pause = false;
    MSMode ms = MSMode::Stereo;
    bool writeProtect = false;

    void setType (EffectType t); // sets type and default parameter values
    bool isEmpty() const noexcept { return type == EffectType::None; }
    juce::var toVar() const;
    static SlotState fromVar (const juce::var& v);
};

struct ProgramState
{
    juce::String name { "Init" };
    juce::String description;
    int channel = -1;        // 0..11 for factory presets, -1 = user / generated
    int index = -1;          // position within the channel
    bool factory = false;
    Source source = Source::Melody;
    int focusMacro = -1;     // macro the preset is tuned for (-1 = default)
    std::array<SlotState, kNumSlots> slots {};
    std::array<float, kNumMacros> macros { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    std::vector<MacroMapping> maps;
    float trimDb = 0.0f;     // loudness trim (set by KILL / BOOTLEG)
    ModState mod;
    uint64_t seed = 0;
    juce::StringArray tags;

    RackConfig rackConfig (int screening = -1) const;
    juce::var toVar() const;
    static ProgramState fromVar (const juce::var& v);
    juce::String toJson() const;
    static ProgramState fromJson (const juce::String& json);
    bool sameSound (const ProgramState& o) const; // compares slots / macros (used by tests)
};

// Factory preset JSON can use real values by parameter name ({"CUTOFF": 2500}), choice labels
// ({"TYPE": "BP"}), or "amount" for the effect's main parameter (normalised 0..1).
SlotState slotFromPresetJson (const juce::var& v);

// ----------------------------------------------------------------------------
struct ChannelInfo
{
    int number = 1;
    juce::String name;
    juce::String description;
    juce::Colour colour;
    Source source = Source::Melody;
};

class PresetBank
{
public:
    PresetBank();

    // factory
    void loadFactory (const juce::String& json);
    const std::vector<ChannelInfo>& getChannels() const noexcept { return channels; }
    int getNumChannels() const noexcept { return (int) channels.size(); }
    const std::vector<ProgramState>& getChannelPresets (int channel) const;
    const ProgramState* getFactory (int channel, int index) const;
    int getNumFactoryPresets() const;

    // user presets (Documents/Killa/Effector Killa/Presets)
    static juce::File getUserRoot();
    static juce::File getUserPresetDir();
    static juce::File getSlotPresetDir();
    juce::Array<juce::File> getUserPresetFiles() const;
    bool saveUserPreset (const ProgramState& s, const juce::String& name) const;
    std::optional<ProgramState> loadUserPreset (const juce::File& f) const;

    // slot presets
    bool saveSlotPreset (const SlotState& s, const juce::String& name) const;
    juce::Array<juce::File> getSlotPresetFiles() const;
    std::optional<SlotState> loadSlotPreset (const juce::File& f) const;

    // favourites (STAFF PICK)
    static juce::String keyFor (const ProgramState& s);
    bool isFavourite (const juce::String& key) const;
    void setFavourite (const juce::String& key, bool fav);
    juce::StringArray getFavourites() const { return favourites; }
    void setRootOverride (const juce::File& f) { rootOverride = f; loadFavourites(); } // tests

private:
    void loadFavourites();
    void saveFavourites() const;
    juce::File root() const;

    std::vector<ChannelInfo> channels;
    std::vector<std::vector<ProgramState>> presets;
    juce::StringArray favourites;
    juce::File rootOverride;
};

} // namespace ek
