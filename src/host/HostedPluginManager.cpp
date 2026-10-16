#include "HostedPluginManager.h"
#include <thread>

namespace bd
{

//==============================================================================
/** Floating window that shows the hosted plugin's own editor. */
class HostedPluginManager::EditorWindow final : public juce::DocumentWindow
{
public:
    EditorWindow (HostedPluginManager& o, juce::AudioProcessorEditor* editor, const juce::String& name)
        : juce::DocumentWindow (name + "  -  BEND DEMON", juce::Colour (0xff0a1030),
                                juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          owner (o)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
        setAlwaysOnTop (true);   // stays above the DAW like FL's own plugin windows
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        // Deleting ourselves from inside a callback is unsafe: defer to the owner.
        juce::MessageManager::callAsync ([w = juce::Component::SafePointer<EditorWindow> (this), &o = owner]
        {
            if (w != nullptr)
                o.closeEditorWindow();
        });
    }

private:
    HostedPluginManager& owner;
};

//==============================================================================
HostedPluginManager::HostedPluginManager (juce::AudioPlayHead& ph)
    : forwardedPlayHead (ph)
{
    formatManager.addDefaultFormats();   // VST3 everywhere, AudioUnit on macOS (see CMake defs)
    loadKnownList();
}

HostedPluginManager::~HostedPluginManager()
{
    stopTimer();
    closeEditorWindow();
    cancelScan();

    // No audio callbacks can be running any more (the processor is being destroyed).
    active.store (nullptr);
    current.reset();
}

juce::File HostedPluginManager::getAppDataDirectory()
{
   #if JUCE_MAC
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support/TrapWin/BEND DEMON");
   #else
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("TrapWin/BEND DEMON");
   #endif
    dir.createDirectory();
    return dir;
}

//==============================================================================
// Audio lifecycle

void HostedPluginManager::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    blockSize  = juce::jmax (1, maxBlock);
    prepared   = true;

    if (current != nullptr && current->instance != nullptr)
    {
        auto& inst = *current->instance;
        inst.setRateAndBufferSizeDetails (sampleRate, blockSize);
        inst.prepareToPlay (sampleRate, blockSize);
        current->scratch.setSize (juce::jmax (2, inst.getTotalNumInputChannels(), inst.getTotalNumOutputChannels()),
                                  blockSize, false, true, false);
    }
}

void HostedPluginManager::release()
{
    prepared = false;
    if (current != nullptr && current->instance != nullptr)
        current->instance->releaseResources();
}

int HostedPluginManager::getHostedLatencySamples() const
{
    return (current != nullptr && current->instance != nullptr) ? current->instance->getLatencySamples() : 0;
}

juce::String HostedPluginManager::getPluginName() const
{
    return current != nullptr ? current->description.name : juce::String();
}

//==============================================================================
// Scanning

juce::Array<juce::PluginDescription> HostedPluginManager::getInstruments() const
{
    juce::Array<juce::PluginDescription> result;

    for (auto& d : knownPlugins.getTypes())
    {
        // Never offer BEND DEMON (or other TrapWin dev tools) to itself: hosting itself recurses.
        const bool isSelf = d.manufacturerName.equalsIgnoreCase ("TrapWin") && d.name.containsIgnoreCase ("BEND DEMON");
        if (d.isInstrument && ! isSelf)
            result.add (d);
    }

    std::sort (result.begin(), result.end(), [] (const auto& a, const auto& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    return result;
}

void HostedPluginManager::startScan (bool rescanEverything)
{
    if (isScanning())
        return;

    if (rescanEverything)
        knownPlugins.clear();

    scanners.clear();

    for (auto* format : formatManager.getFormats())
    {
        if (! format->canScanForPlugins())
            continue;

        scanners.push_back (std::make_unique<juce::PluginDirectoryScanner> (
            knownPlugins, *format, format->getDefaultLocationsToSearch(),
            true, getDeadMansPedalFile(), false));
    }

    scanIndex = 0;
    scanProgress = 0.0f;
    scanStatus = "Scanning...";
    startTimer (15);
    sendChangeMessage();
}

void HostedPluginManager::cancelScan()
{
    scanners.clear();
    scanIndex = -1;
    scanStatus = {};
}

void HostedPluginManager::timerCallback()
{
    if (scanIndex < 0)
    {
        stopTimer();
        return;
    }

    if (scanIndex >= (int) scanners.size())
    {
        // finished
        scanners.clear();
        scanIndex = -1;
        scanProgress = 1.0f;
        scanStatus = "Found " + juce::String (getInstruments().size()) + " instruments";
        saveKnownList();
        stopTimer();
        sendChangeMessage();
        return;
    }

    // Scan one file per tick so the UI stays responsive.
    // NOTE: this runs in-process on the message thread (required for AudioUnits). A plugin that
    // crashes while being scanned takes the host down with it; the dead-man's-pedal file makes
    // sure it is skipped (blacklisted) on the next scan.
    auto& scanner = *scanners[(size_t) scanIndex];
    juce::String name;
    scanStatus = scanner.getNextPluginFileThatWillBeScanned();

    if (! scanner.scanNextFile (true, name))
        ++scanIndex;

    const float perFormat = 1.0f / (float) juce::jmax<size_t> (1, scanners.size());
    scanProgress = juce::jlimit (0.0f, 1.0f, (float) juce::jmin (scanIndex, (int) scanners.size()) * perFormat
                                     + (scanIndex < (int) scanners.size() ? scanner.getProgress() * perFormat : 0.0f));
    sendChangeMessage();
}

void HostedPluginManager::saveKnownList()
{
    if (auto xml = knownPlugins.createXml())
        xml->writeTo (getKnownListFile());
}

void HostedPluginManager::loadKnownList()
{
    if (auto xml = juce::XmlDocument::parse (getKnownListFile()))
        knownPlugins.recreateFromXml (*xml);
}

//==============================================================================
// Loading

void HostedPluginManager::loadPlugin (const juce::PluginDescription& desc, juce::MemoryBlock state,
                                      std::function<void (juce::String)> onDone)
{
    JUCE_ASSERT_MESSAGE_THREAD

    loading = true;
    const int generation = ++loadGeneration;
    juce::WeakReference<HostedPluginManager> weakThis (this);

    formatManager.createPluginInstanceAsync (desc, sampleRate, blockSize,
        [weakThis, generation, desc, state = std::move (state), onDone]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
        {
            auto* self = weakThis.get();
            if (self == nullptr || generation != self->loadGeneration)
                return;   // manager gone, or a newer load superseded this one

            self->loading = false;

            if (instance == nullptr)
            {
                self->lastError   = error.isNotEmpty() ? error : juce::String ("Could not create plugin");
                self->missingName = desc.name;
                self->sendChangeMessage();
                if (onDone) onDone (self->lastError);
                return;
            }

            self->lastError = {};
            self->missingName = {};
            self->pendingMissingXml.reset();
            self->install (std::move (instance), desc, state);
            if (onDone) onDone ({});
        });

    sendChangeMessage();
}

void HostedPluginManager::configureBuses (juce::AudioPluginInstance& inst)
{
    // Instruments: we feed MIDI only. Keep the main output bus (prefer stereo) and disable the
    // rest so the scratch buffer stays small and predictable.
    inst.disableNonMainBuses();

    if (inst.getBusCount (false) > 0)
    {
        auto layout = inst.getBusesLayout();
        layout.getMainOutputChannelSet() = juce::AudioChannelSet::stereo();

        if (inst.getBusCount (true) > 0)
            layout.getMainInputChannelSet() = juce::AudioChannelSet::disabled();

        if (! inst.setBusesLayout (layout))
        {
            // fall back: just keep whatever the plugin chose, but try without touching the input
            auto l2 = inst.getBusesLayout();
            l2.getMainOutputChannelSet() = juce::AudioChannelSet::stereo();
            inst.setBusesLayout (l2);
        }
    }
}

void HostedPluginManager::install (std::unique_ptr<juce::AudioPluginInstance> instance,
                                   const juce::PluginDescription& desc, const juce::MemoryBlock& state)
{
    JUCE_ASSERT_MESSAGE_THREAD

    auto slot = std::make_unique<HostedSlot>();
    slot->description = desc;

    auto& inst = *instance;
    configureBuses (inst);
    inst.setPlayHead (&forwardedPlayHead);
    inst.setNonRealtime (false);

    if (state.getSize() > 0)
        inst.setStateInformation (state.getData(), (int) state.getSize());

    inst.setRateAndBufferSizeDetails (sampleRate, blockSize);
    if (prepared)
        inst.prepareToPlay (sampleRate, blockSize);

    slot->numOutputChannels = inst.getTotalNumOutputChannels();
    slot->scratch.setSize (juce::jmax (2, inst.getTotalNumInputChannels(), inst.getTotalNumOutputChannels()),
                           blockSize, false, true, false);
    slot->instance = std::move (instance);

    closeEditorWindow();   // editor of the old plugin must go before the plugin itself

    // Atomically swap the new slot in and wait until the audio thread is done with the old one.
    HostedSlot* newSlot = slot.get();
    active.exchange (newSlot);
    while (audioInside.load())
        std::this_thread::yield();

    auto old = std::move (current);
    current = std::move (slot);

    if (old != nullptr && old->instance != nullptr)
        old->instance->releaseResources();
    old.reset();

    sendChangeMessage();
    if (onPluginChanged) onPluginChanged();
}

void HostedPluginManager::unloadPlugin()
{
    JUCE_ASSERT_MESSAGE_THREAD

    ++loadGeneration;   // cancel pending async loads
    loading = false;
    missingName = {};
    closeEditorWindow();

    active.exchange (nullptr);
    while (audioInside.load())
        std::this_thread::yield();

    current.reset();
    sendChangeMessage();
    if (onPluginChanged) onPluginChanged();
}

//==============================================================================
// State

std::unique_ptr<juce::XmlElement> HostedPluginManager::createStateXml() const
{
    // Can be called by the host from any thread; we only touch `current` which is changed on the
    // message thread. Hosts call getStateInformation from the message thread in practice.
    auto xml = std::make_unique<juce::XmlElement> ("HOSTED");

    if (current != nullptr && current->instance != nullptr)
    {
        if (auto d = current->description.createXml())
            xml->addChildElement (d.release());

        juce::MemoryBlock mb;
        current->instance->getStateInformation (mb);
        xml->createNewChildElement ("STATE")->addTextElement (mb.toBase64Encoding());
    }
    else if (missingName.isNotEmpty() && pendingMissingXml != nullptr)
    {
        // Keep the reference to a plugin that could not be loaded so the project is not lost.
        xml->addChildElement (new juce::XmlElement (*pendingMissingXml));
    }

    return xml;
}

void HostedPluginManager::restoreFromStateXml (const juce::XmlElement& xml, std::function<void (juce::String)> onDone)
{
    JUCE_ASSERT_MESSAGE_THREAD

    auto* descXml = xml.getChildByName ("PLUGIN");
    if (descXml == nullptr)
    {
        if (current != nullptr) unloadPlugin();
        if (onDone) onDone ({});
        return;
    }

    juce::PluginDescription desc;
    if (! desc.loadFromXml (*descXml))
    {
        if (onDone) onDone ("Invalid plugin description in saved state");
        return;
    }

    pendingMissingXml = std::make_unique<juce::XmlElement> (*descXml);

    juce::MemoryBlock state;
    if (auto* st = xml.getChildByName ("STATE"))
        state.fromBase64Encoding (st->getAllSubText());

    // Prefer the entry from our scan list (the file path may differ between machines).
    for (auto& known : knownPlugins.getTypes())
    {
        if (known.isDuplicateOf (desc))
        {
            desc = known;
            break;
        }
    }

    loadPlugin (desc, std::move (state), std::move (onDone));
}

//==============================================================================
// Editor window

void HostedPluginManager::showEditorWindow()
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (editorWindow != nullptr)
    {
        editorWindow->toFront (true);
        return;
    }

    if (current == nullptr || current->instance == nullptr)
        return;

    auto& inst = *current->instance;
    juce::AudioProcessorEditor* ed = inst.hasEditor() ? inst.createEditorIfNeeded() : nullptr;

    if (ed == nullptr)
        ed = new juce::GenericAudioProcessorEditor (inst);

    editorWindow = std::make_unique<EditorWindow> (*this, ed, current->description.name);
    sendChangeMessage();
}

void HostedPluginManager::closeEditorWindow()
{
    if (editorWindow == nullptr)
        return;

    // The window owns the editor; ~AudioProcessorEditor tells the plugin it is going away.
    editorWindow.reset();
    sendChangeMessage();
}

} // namespace bd
