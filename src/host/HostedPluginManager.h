/*
    TrapWin BEND DEMON
    HostedPluginManager.h - scans for, loads and owns the instrument plugin hosted inside BEND DEMON.

    Threading model
    ---------------
    * Scanning, loading, unloading and the hosted editor window live on the MESSAGE thread.
    * The audio thread calls beginAudio() / endAudio() around processBlock. Swapping in a new
      plugin is done with an atomic pointer exchange plus an "audio thread is inside" flag:

          audio:   inUse = true;  p = active.load();  ...process p...;  inUse = false;
          message: old = active.exchange (new);  while (inUse) yield;  delete old;

      With sequentially-consistent atomics the message thread can only delete `old` once the audio
      thread is guaranteed not to be using it. The audio thread never blocks or allocates.
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <functional>

namespace bd
{

/** Everything the audio thread needs about one hosted plugin. */
struct HostedSlot
{
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::PluginDescription description;
    juce::AudioBuffer<float> scratch;     // max(in, out) channels x maxBlockSize, preallocated
    int numOutputChannels = 2;
};

//==============================================================================
class HostedPluginManager final : private juce::Timer,
                                  public  juce::ChangeBroadcaster
{
public:
    /** `playHead` is forwarded to every hosted plugin (it reads BEND DEMON's own host playhead). */
    explicit HostedPluginManager (juce::AudioPlayHead& playHeadToForward);
    ~HostedPluginManager() override;

    //==============================================================================
    // Audio thread
    HostedSlot* beginAudio() noexcept
    {
        audioInside.store (true);
        return active.load();
    }

    void endAudio() noexcept { audioInside.store (false); }

    /** Called from prepareToPlay (never concurrently with processBlock). */
    void prepare (double sampleRate, int maxBlockSize);
    void release();

    //==============================================================================
    // Message thread: scanning
    juce::KnownPluginList& getKnownPlugins() noexcept { return knownPlugins; }
    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager; }

    /** All instrument plugins found so far (sorted by name). */
    juce::Array<juce::PluginDescription> getInstruments() const;

    void startScan (bool rescanEverything = false);
    void cancelScan();
    bool isScanning() const noexcept   { return ! scanners.empty() || scanIndex >= 0; }
    float getScanProgress() const noexcept { return scanProgress; }
    juce::String getScanStatus() const { return scanStatus; }

    //==============================================================================
    // Message thread: loading
    /** Loads the plugin (async if the format needs it). `onDone (errorMessage)` is called on the
        message thread; an empty error means success. If `stateToRestore` is non-empty it is
        applied to the new instance before it goes live. */
    void loadPlugin (const juce::PluginDescription&, juce::MemoryBlock stateToRestore = {},
                     std::function<void (juce::String)> onDone = {});
    void unloadPlugin();

    bool hasPlugin() const noexcept               { return current != nullptr; }
    juce::String getPluginName() const;
    juce::PluginDescription getDescription() const { return current != nullptr ? current->description : juce::PluginDescription(); }
    juce::String getLastError() const             { return lastError; }
    bool isLoading() const noexcept               { return loading; }

    /** Description of a plugin that should be loaded but could not be found/created. */
    juce::String getMissingPluginName() const     { return missingName; }

    int getHostedLatencySamples() const;

    //==============================================================================
    // State
    std::unique_ptr<juce::XmlElement> createStateXml() const;
    void restoreFromStateXml (const juce::XmlElement&, std::function<void (juce::String)> onDone = {});

    //==============================================================================
    // Hosted editor window
    void showEditorWindow();
    void closeEditorWindow();
    bool isEditorWindowOpen() const noexcept { return editorWindow != nullptr; }

    /** Called (message thread) right after a new plugin went live / was removed. */
    std::function<void()> onPluginChanged;

    static juce::File getAppDataDirectory();

    /** True for synths known to support MPE (per-note pitch bend). Others get mono slides. */
    static bool isKnownMpeSynth (const juce::PluginDescription&);

private:
    class EditorWindow;

    void timerCallback() override;
    void install (std::unique_ptr<juce::AudioPluginInstance>, const juce::PluginDescription&,
                  const juce::MemoryBlock& state);
    void configureBuses (juce::AudioPluginInstance&);
    void saveKnownList();
    void loadKnownList();
    juce::File getKnownListFile() const   { return getAppDataDirectory().getChildFile ("PluginList.xml"); }
    juce::File getDeadMansPedalFile() const { return getAppDataDirectory().getChildFile ("ScanInProgress.txt"); }

    juce::AudioPlayHead& forwardedPlayHead;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    // live slot (owned via `current`, read by the audio thread through `active`)
    std::unique_ptr<HostedSlot> current;
    std::atomic<HostedSlot*>    active { nullptr };
    std::atomic<bool>           audioInside { false };

    double sampleRate = 44100.0;
    int    blockSize  = 512;
    bool   prepared   = false;

    // scanning
    std::vector<std::unique_ptr<juce::PluginDirectoryScanner>> scanners;
    int scanIndex = -1;
    float scanProgress = 0.0f;
    juce::String scanStatus;

    bool loading = false;
    int  loadGeneration = 0;
    juce::String lastError, missingName;
    std::unique_ptr<juce::XmlElement> pendingMissingXml;   // description of a plugin that failed to load

    std::unique_ptr<EditorWindow> editorWindow;

    JUCE_DECLARE_WEAK_REFERENCEABLE (HostedPluginManager)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedPluginManager)
};

} // namespace bd
