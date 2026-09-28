#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Randomizer.h"

namespace ek
{
namespace ParamIDs
{
    juce::String slotParam (int slot, int param); // "s1_p1" .. "s8_p8"
    juce::String slotMix (int slot);              // "s1_mix"
    juce::String slotPause (int slot);            // "s1_pause"
    juce::String macro (int m);                   // "villain_arc" ...
    inline const juce::String inGain { "antenna_in" };
    inline const juce::String outGain { "rf_out" };
    inline const juce::String blend { "blend" };
    inline const juce::String autoTracking { "auto_tracking" };
    inline const juce::String pocketTv { "pocket_tv" };
    inline const juce::String offAir { "off_air" };
}
} // namespace ek

class EffectorKillaAudioProcessor : public juce::AudioProcessor,
                                    public juce::ChangeBroadcaster,
                                    private juce::Timer,
                                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    EffectorKillaAudioProcessor();
    ~EffectorKillaAudioProcessor() override;

    // ---- juce::AudioProcessor --------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Effector Killa"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return meta.name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Effector Killa API (message thread) --------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    ek::Engine engine;
    ek::PresetBank bank;

    ek::ProgramState captureProgram() const;          // current sound incl. host parameter values
    const ek::ProgramState& getMeta() const noexcept { return meta; }
    void applyProgram (const ek::ProgramState& p, bool undoable = true);

    // presets / channels
    void loadFactory (int channel, int index, bool undoable = true);
    void stepPreset (int delta);
    void stepChannel (int delta);
    int getCurrentChannel() const noexcept { return meta.channel >= 0 ? meta.channel : lastChannel; }
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& f);
    bool isCurrentFavourite() const;
    void toggleFavourite();

    // KILL / BOOTLEG
    void kill();
    void bootleg();
    ek::KillLevel killLevel = ek::KillLevel::R;
    std::atomic<int> killEvents { 0 }, programEvents { 0 };

    // history / A-B
    void pushUndo();
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    int getSide() const noexcept { return side; }
    void setSide (int newSide);
    static constexpr int maxUndo = 64;

    // slots
    void setSlotType (int slot, ek::EffectType t);
    void ejectSlot (int slot) { setSlotType (slot, ek::EffectType::None); }
    void moveSlot (int from, int to);
    void setSlotMs (int slot, ek::MSMode m);
    void setWriteProtect (int slot, bool on);
    void setScreening (int slot); // -1 = off
    int getScreening() const noexcept { return screening; }
    void copySlot (int slot);
    void pasteSlot (int slot);
    bool hasSlotClipboard() const noexcept { return slotClipboard.has_value(); }
    bool saveSlotPreset (int slot, const juce::String& name);
    bool loadSlotPreset (int slot, const juce::File& f);
    void setSlotParam (int slot, int param, float normValue);  // param 8 = MIX, 9 = PAUSE
    float getSlotParam (int slot, int param) const;
    ek::EffectType getSlotType (int slot) const noexcept { return meta.slots[(size_t) slot].type; }
    const ek::SlotState& getSlotMeta (int slot) const noexcept { return meta.slots[(size_t) slot]; }
    int selectedSlot = -1;

    // source / modulation / settings
    void setSource (ek::Source s);
    void setModState (const ek::ModState& m, bool undoable);
    void setOversampling (int osFactorLog2);
    int getOversampling() const noexcept { return oversampling; }
    bool premiumCable = true;

    // macro gestures from the UI (host automation of macros disconnects AV modulation)
    void beginUiGesture (int macro);
    void endUiGesture (int macro);

    juce::RangedAudioParameter* getParam (const juce::String& id) const { return apvts.getParameter (id); }
    void setParamNorm (const juce::String& id, float v);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float newValue) override;
    void pushConfigToEngine();
    void pushExtrasToEngine();
    void notifyUi() { sendChangeMessage(); }
    ek::ProgramState stateForSide() const { return captureProgram(); }

    ek::ProgramState meta;             // non-parameter program data (types, names, maps, mod ...)
    int lastChannel = 0;
    int screening = -1;
    int oversampling = 1;
    double currentSampleRate = 44100.0;
    bool applying = false;

    std::vector<ek::ProgramState> undoStack, redoStack;
    std::array<std::optional<ek::ProgramState>, 2> sides;
    int side = 0;
    std::optional<ek::SlotState> slotClipboard;

    // raw parameter pointers for the audio thread
    std::array<std::array<std::atomic<float>*, ek::kNumParams>, ek::kNumSlots> pSlot {};
    std::array<std::atomic<float>*, ek::kNumSlots> pMix {}, pPause {};
    std::array<std::atomic<float>*, ek::kNumMacros> pMacro {};
    std::atomic<float>* pIn = nullptr, *pOut = nullptr, *pBlend = nullptr, *pAuto = nullptr, *pPocket = nullptr, *pOffAir = nullptr;

    std::array<std::atomic<int>, ek::kNumMacros> uiGesture {};
    std::array<std::atomic<juce::uint32>, ek::kNumMacros> lastHostMacroChange {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectorKillaAudioProcessor)
};
